#include "game_hooks.hpp"
#include "../features/config.hpp"
#include "../game/offsets.hpp"
#include "../game/sdk.hpp"

#include "MinHook.h"
#include <windows.h>

namespace {

using FnCLCreateNewCommands = void(__fastcall*)(int localClientNum, void* edxUnused);
FnCLCreateNewCommands g_origCLCreateNewCommands = nullptr;

// Referenced from top-level x86 assembly detours (cdecl underscores handled via asm name)
extern "C" {
void* g_origGetWeaponParam asm("_g_origGetWeaponParam") = nullptr;
void* g_origWeaponFireRecoil asm("_g_origWeaponFireRecoil") = nullptr;
volatile uint8_t g_noRecoilActive asm("_g_noRecoilActive") = 0;

void Detour_G_GetWeaponParam() asm("_Detour_G_GetWeaponParam");
void Detour_BG_WeaponFireRecoil() asm("_Detour_BG_WeaponFireRecoil");

struct WeaponParms {
  waw::sdk::Vec3 forward;
  waw::sdk::Vec3 right;
  waw::sdk::Vec3 up;
  waw::sdk::Vec3 muzzleTrace;
  waw::sdk::Vec3 gunForward;
  void* weapDef;
};

void __cdecl OnPostGetWeaponParam(WeaponParms* wp, void* ent) asm("_OnPostGetWeaponParam");

void __cdecl OnPostGetWeaponParam(WeaponParms* wp, void* ent) {
  if (!wp || !ent) return;
  const auto& cfg = waw::Config();
  if (!cfg.aimEnabled || cfg.aimMode != static_cast<int>(waw::AimMode::Silent) || cfg.menuOpen) return;

  // Verify firing entity is local player entity #0
  const uintptr_t entAddr = reinterpret_cast<uintptr_t>(ent);
  if (entAddr != waw::offsets::kGEntities) return;

  const bool keyHeld = cfg.aimAlwaysOn || ((GetAsyncKeyState(cfg.aimKey) & 0x8000) != 0);
  if (!keyHeld) return;

  const waw::sdk::SilentTargetState tgt = waw::sdk::GetSilentTargetState();
  if (!tgt.valid || tgt.entNum <= 0) return;
  if (!waw::sdk::IsEntityAliveServer(tgt.entNum)) return;

  const waw::sdk::Vec3 silentAngles = waw::sdk::CalcAimAngles(wp->muzzleTrace, tgt.aimPos3D);
  waw::sdk::AngleVectors(silentAngles, &wp->forward, &wp->right, &wp->up);
}
} // extern "C"

__asm__(
    ".text\n\t"
    ".globl _Detour_G_GetWeaponParam\n"
    "_Detour_G_GetWeaponParam:\n\t"
    "pushl %ebp\n\t"
    "movl %esp, %ebp\n\t"
    "pushl %eax\n\t"
    "pushl 8(%ebp)\n\t"
    "calll *_g_origGetWeaponParam\n\t"
    "addl $4, %esp\n\t"
    "pushl 8(%ebp)\n\t"
    "pushl -4(%ebp)\n\t"
    "calll _OnPostGetWeaponParam\n\t"
    "addl $8, %esp\n\t"
    "movl %ebp, %esp\n\t"
    "popl %ebp\n\t"
    "ret\n\t"

    ".globl _Detour_BG_WeaponFireRecoil\n"
    "_Detour_BG_WeaponFireRecoil:\n\t"
    "cmpb $0, _g_noRecoilActive\n\t"
    "je 1f\n\t"
    "ret\n"
    "1:\n\t"
    "jmpl *_g_origWeaponFireRecoil\n\t"
);

void __fastcall Hook_CL_CreateNewCommands(int localClientNum, void* edxUnused) {
  if (g_origCLCreateNewCommands) {
    g_origCLCreateNewCommands(localClientNum, edxUnused);
  }
  const auto& cfg = waw::Config();
  g_noRecoilActive = (!cfg.unloading && cfg.noRecoil) ? 1 : 0;
  waw::sdk::OnMainGameTick();
}

} // namespace

namespace waw::hooks {

bool InstallGameHooks() {
  if (MH_CreateHook(
          reinterpret_cast<LPVOID>(offsets::kFnCLCreateNewCommands),
          reinterpret_cast<LPVOID>(&Hook_CL_CreateNewCommands),
          reinterpret_cast<LPVOID*>(&g_origCLCreateNewCommands)) != MH_OK) {
    return false;
  }

  if (MH_CreateHook(
          reinterpret_cast<LPVOID>(offsets::kFnGGetWeaponParam),
          reinterpret_cast<LPVOID>(&Detour_G_GetWeaponParam),
          &g_origGetWeaponParam) != MH_OK) {
    return false;
  }

  if (MH_CreateHook(
          reinterpret_cast<LPVOID>(offsets::kFnBGWeaponFireRecoil),
          reinterpret_cast<LPVOID>(&Detour_BG_WeaponFireRecoil),
          &g_origWeaponFireRecoil) != MH_OK) {
    return false;
  }

  if (MH_EnableHook(reinterpret_cast<LPVOID>(offsets::kFnCLCreateNewCommands)) != MH_OK) return false;
  if (MH_EnableHook(reinterpret_cast<LPVOID>(offsets::kFnGGetWeaponParam)) != MH_OK) return false;
  if (MH_EnableHook(reinterpret_cast<LPVOID>(offsets::kFnBGWeaponFireRecoil)) != MH_OK) return false;
  return true;
}

void RemoveGameHooks() {
  g_noRecoilActive = 0;
  MH_DisableHook(reinterpret_cast<LPVOID>(offsets::kFnBGWeaponFireRecoil));
  MH_DisableHook(reinterpret_cast<LPVOID>(offsets::kFnGGetWeaponParam));
  MH_DisableHook(reinterpret_cast<LPVOID>(offsets::kFnCLCreateNewCommands));
  MH_RemoveHook(reinterpret_cast<LPVOID>(offsets::kFnBGWeaponFireRecoil));
  MH_RemoveHook(reinterpret_cast<LPVOID>(offsets::kFnGGetWeaponParam));
  MH_RemoveHook(reinterpret_cast<LPVOID>(offsets::kFnCLCreateNewCommands));
}

} // namespace waw::hooks
