#include "features/config.hpp"
#include "game/sdk.hpp"
#include "hooks/dx9_hook.hpp"
#include "hooks/game_hooks.hpp"

#include "MinHook.h"
#include <cstring>
#include <windows.h>

namespace {

HMODULE g_hModule = nullptr;

bool IsTargetProcessValid() {
  char path[MAX_PATH]{};
  if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) return false;
  const char* name = std::strrchr(path, '\\');
  name = name ? (name + 1) : path;
  // Strictly allow CoDWaW.exe (SP / Zombies Solo), block multiplayer binary CoDWaWmp.exe
  return _stricmp(name, "CoDWaW.exe") == 0;
}

DWORD WINAPI ModThread(LPVOID) {
  if (MH_Initialize() != MH_OK) {
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
  }

  if (!waw::hooks::InstallDx9Hook() || !waw::hooks::InstallGameHooks()) {
    waw::hooks::RemoveGameHooks();
    waw::hooks::ShutdownDx9Hook();
    MH_Uninitialize();
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
  }

  while (!waw::Config().unloading) {
    Sleep(50);
  }

  // Wait briefly for render thread EndScene to clean up DX9 ImGui objects cleanly
  for (int i = 0; i < 40 && !waw::hooks::IsDx9ReadyToUnload(); ++i) {
    Sleep(25);
  }

  waw::sdk::OnEndSceneCleanup();
  waw::hooks::RemoveGameHooks();
  waw::hooks::ShutdownDx9Hook();
  Sleep(100);
  MH_Uninitialize();

  FreeLibraryAndExitThread(g_hModule, 0);
  return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    if (!IsTargetProcessValid()) {
      return FALSE;
    }
    g_hModule = hModule;
    HANDLE hThread = CreateThread(nullptr, 0, ModThread, nullptr, 0, nullptr);
    if (hThread) CloseHandle(hThread);
  }
  return TRUE;
}
