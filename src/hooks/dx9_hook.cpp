#include "dx9_hook.hpp"
#include "../features/config.hpp"
#include "../features/esp.hpp"
#include "../game/offsets.hpp"
#include "../game/sdk.hpp"
#include "../ui/ui.hpp"

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <atomic>
#include <d3d9.h>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace waw::hooks {

namespace {

using FnEndScene = HRESULT(__stdcall*)(IDirect3DDevice9* device);
using FnReset = HRESULT(__stdcall*)(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params);

FnEndScene g_origEndScene = nullptr;
FnReset g_origReset = nullptr;
void* g_endSceneTarget = nullptr;
void* g_resetTarget = nullptr;

HWND g_hwnd = nullptr;
WNDPROC g_origWndProc = nullptr;
bool g_hwndIsUnicode = false;
bool g_imguiInitialized = false;
std::atomic<bool> g_shutdownDone{false};

LRESULT CALLBACK Hook_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto& cfg = Config();

  if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
    if (wParam == VK_INSERT || wParam == VK_F1) {
      cfg.menuOpen = !cfg.menuOpen;
      return 0;
    }
    if (wParam == VK_F7) {
      cfg.keybindsVisible = !cfg.keybindsVisible;
      return 0;
    }
    if (wParam == VK_END) {
      cfg.menuOpen = false;
      cfg.unloading = true;
      return 0;
    }
  }

  if (cfg.menuOpen && !cfg.unloading) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
      return TRUE;
    }
    switch (msg) {
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_LBUTTONDBLCLK:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_RBUTTONDBLCLK:
      case WM_MBUTTONDOWN:
      case WM_MBUTTONUP:
      case WM_XBUTTONDOWN:
      case WM_XBUTTONUP:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_MOUSEMOVE:
      case WM_CHAR:
      case WM_KEYDOWN:
      case WM_KEYUP:
        return TRUE;
      default:
        break;
    }
  }

  return g_hwndIsUnicode ? CallWindowProcW(g_origWndProc, hwnd, msg, wParam, lParam)
                         : CallWindowProcA(g_origWndProc, hwnd, msg, wParam, lParam);
}

void CleanupImGuiAndWndProc() {
  if (g_origWndProc && g_hwnd) {
    if (g_hwndIsUnicode) {
      SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
    } else {
      SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
    }
    g_origWndProc = nullptr;
  }
  if (g_imguiInitialized) {
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_imguiInitialized = false;
  }
}

HRESULT __stdcall Hook_Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
  if (g_imguiInitialized) {
    ImGui_ImplDX9_InvalidateDeviceObjects();
  }
  const HRESULT hr = g_origReset ? g_origReset(device, params) : D3D_OK;
  if (SUCCEEDED(hr) && g_imguiInitialized) {
    ImGui_ImplDX9_CreateDeviceObjects();
  }
  return hr;
}

HRESULT __stdcall Hook_EndScene(IDirect3DDevice9* device) {
  static bool s_inHook = false;
  if (s_inHook || !device) {
    return g_origEndScene ? g_origEndScene(device) : D3D_OK;
  }
  s_inHook = true;

  auto& cfg = Config();
  if (cfg.unloading) {
    CleanupImGuiAndWndProc();
    g_shutdownDone.store(true);
    s_inHook = false;
    return g_origEndScene ? g_origEndScene(device) : D3D_OK;
  }

  if (!g_imguiInitialized) {
    D3DDEVICE_CREATION_PARAMETERS cp{};
    if (SUCCEEDED(device->GetCreationParameters(&cp)) && cp.hFocusWindow) {
      g_hwnd = cp.hFocusWindow;
    } else {
      g_hwnd = GetForegroundWindow();
    }

    if (g_hwnd && IsWindow(g_hwnd)) {
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO& io = ImGui::GetIO();
      io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
      io.IniFilename = nullptr;

      ui::ApplyStyle();
      ui::InitFonts();

      ImGui_ImplWin32_Init(g_hwnd);
      ImGui_ImplDX9_Init(device);

      g_hwndIsUnicode = (IsWindowUnicode(g_hwnd) != FALSE);
      g_origWndProc = reinterpret_cast<WNDPROC>(
          g_hwndIsUnicode ? SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Hook_WndProc))
                          : SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Hook_WndProc)));
      g_imguiInitialized = true;
    }
  }

  if (g_imguiInitialized) {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = cfg.menuOpen;

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    esp::Render();
    ui::RenderMenu();

    ImGui::EndFrame();
    ImGui::Render();

    DWORD prevSrgb = 0;
    device->GetRenderState(D3DRS_SRGBWRITEENABLE, &prevSrgb);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, prevSrgb);
  }

  s_inHook = false;
  return g_origEndScene ? g_origEndScene(device) : D3D_OK;
}

} // namespace

bool InstallDx9Hook() {
  IDirect3DDevice9* dev = nullptr;
  for (int i = 0; i < 200; ++i) {
    if (Config().unloading) return false;
    dev = *reinterpret_cast<IDirect3DDevice9**>(offsets::kDx9Device);
    if (dev) break;
    Sleep(100);
  }
  if (!dev) return false;

  void** vtbl = *reinterpret_cast<void***>(dev);
  if (!vtbl) return false;

  g_resetTarget = vtbl[16];
  g_endSceneTarget = vtbl[42];

  if (MH_CreateHook(g_resetTarget, reinterpret_cast<LPVOID>(&Hook_Reset), reinterpret_cast<LPVOID*>(&g_origReset)) != MH_OK) {
    return false;
  }
  if (MH_CreateHook(g_endSceneTarget, reinterpret_cast<LPVOID>(&Hook_EndScene), reinterpret_cast<LPVOID*>(&g_origEndScene)) != MH_OK) {
    return false;
  }

  if (MH_EnableHook(g_resetTarget) != MH_OK) return false;
  if (MH_EnableHook(g_endSceneTarget) != MH_OK) return false;
  return true;
}

void ShutdownDx9Hook() {
  if (g_endSceneTarget) {
    MH_DisableHook(g_endSceneTarget);
    MH_RemoveHook(g_endSceneTarget);
    g_endSceneTarget = nullptr;
  }
  if (g_resetTarget) {
    MH_DisableHook(g_resetTarget);
    MH_RemoveHook(g_resetTarget);
    g_resetTarget = nullptr;
  }
  // Fallback cleanup in case EndScene was not running (e.g. minimized)
  CleanupImGuiAndWndProc();
}

bool IsDx9ReadyToUnload() {
  return !g_imguiInitialized || g_shutdownDone.load();
}

} // namespace waw::hooks
