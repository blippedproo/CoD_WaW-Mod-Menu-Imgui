#include "sdk.hpp"
#include "offsets.hpp"
#include "../features/config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <windows.h>

namespace waw::sdk {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;
constexpr float kRad2Deg = 180.0f / kPi;

std::mutex g_snapMutex;
FrameSnapshot g_frameSnapshot;

std::mutex g_silentMutex;
SilentTargetState g_silentTarget;

std::mutex g_actionMutex;
std::deque<QueuedAction> g_actions;

struct WeaponDefBackup {
  int fireDelay = 0;
  int fireTime = 0;
  int rechamberTime = 0;
  int rechamberBoltTime = 0;
  int holdFireTime = 0;
  int reloadTime = 0;
  int reloadEmptyTime = 0;
  int reloadAddTime = 0;
  int reloadEmptyAddTime = 0;
  int reloadStartTime = 0;
  int dropTime = 0;
  int raiseTime = 0;
  int quickDropTime = 0;
  int quickRaiseTime = 0;
  int sprintInTime = 0;
  int sprintOutTime = 0;
  int damage = 0;
  int minDamage = 0;
  int explosionInnerDamage = 0;
  int explosionOuterDamage = 0;
  int fireType = 0;
  float hipSpread[6] = {}; // +0x53C..+0x550
  float adsSpread = 0.0f;  // +0x830
};

std::unordered_map<uintptr_t, WeaponDefBackup> g_weaponBackups;

struct CachedTags {
  bool resolved = false;
  uint16_t j_head = 0;
  uint16_t j_neck = 0;
  uint16_t j_spine4 = 0;
  uint16_t j_spineupper = 0;
  uint16_t j_spinelower = 0;
  uint16_t pelvis = 0;
  uint16_t j_shoulder_le = 0;
  uint16_t j_elbow_le = 0;
  uint16_t j_wrist_le = 0;
  uint16_t j_shoulder_ri = 0;
  uint16_t j_elbow_ri = 0;
  uint16_t j_wrist_ri = 0;
  uint16_t j_hip_le = 0;
  uint16_t j_knee_le = 0;
  uint16_t j_ankle_le = 0;
  uint16_t j_hip_ri = 0;
  uint16_t j_knee_ri = 0;
  uint16_t j_ankle_ri = 0;
  uint16_t tag_eye = 0;
} g_tags;

bool IsReadableMemory(uintptr_t addr, size_t size = 4) {
  if (addr < 0x00010000 || addr > 0x7FFF0000) return false;
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) return false;
  if (mbi.State != MEM_COMMIT) return false;
  const DWORD prot = mbi.Protect & 0xFF;
  if (prot == PAGE_NOACCESS || prot == PAGE_EXECUTE) return false;
  const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
  return (addr + size) <= regionEnd;
}

template <typename T>
inline T ReadMem(uintptr_t addr) {
  return *reinterpret_cast<const volatile T*>(addr);
}

template <typename T>
inline void WriteMem(uintptr_t addr, T val) {
  *reinterpret_cast<volatile T*>(addr) = val;
}

template <typename T>
inline void SetDvarValue(uintptr_t ppDvarVa, T val) {
  uintptr_t dvar = *reinterpret_cast<const uintptr_t*>(ppDvarVa);
  if (dvar > 0x00100000 && dvar < 0x7FFF0000) {
    *reinterpret_cast<T*>(dvar + 0x0C) = val;
  }
}

void EnsureBoneTagsResolved() {
  if (g_tags.resolved) return;
  g_tags.j_head = SLGetString("j_head");
  g_tags.j_neck = SLGetString("j_neck");
  g_tags.j_spine4 = SLGetString("j_spine4");
  g_tags.j_spineupper = SLGetString("j_spineupper");
  g_tags.j_spinelower = SLGetString("j_spinelower");
  g_tags.pelvis = SLGetString("pelvis");
  g_tags.j_shoulder_le = SLGetString("j_shoulder_le");
  g_tags.j_elbow_le = SLGetString("j_elbow_le");
  g_tags.j_wrist_le = SLGetString("j_wrist_le");
  g_tags.j_shoulder_ri = SLGetString("j_shoulder_ri");
  g_tags.j_elbow_ri = SLGetString("j_elbow_ri");
  g_tags.j_wrist_ri = SLGetString("j_wrist_ri");
  g_tags.j_hip_le = SLGetString("j_hip_le");
  g_tags.j_knee_le = SLGetString("j_knee_le");
  g_tags.j_ankle_le = SLGetString("j_ankle_le");
  g_tags.j_hip_ri = SLGetString("j_hip_ri");
  g_tags.j_knee_ri = SLGetString("j_knee_ri");
  g_tags.j_ankle_ri = SLGetString("j_ankle_ri");
  g_tags.tag_eye = SLGetString("tag_eye");
  g_tags.resolved = (g_tags.j_head != 0);
}

std::string StripColorCodes(const char* raw) {
  if (!raw) return {};
  std::string out;
  for (size_t i = 0; raw[i] != '\0' && i < 256; ++i) {
    if (raw[i] == '^' && raw[i + 1] >= '0' && raw[i + 1] <= '9') {
      ++i;
      continue;
    }
    if (static_cast<unsigned char>(raw[i]) >= 0x20) out.push_back(raw[i]);
  }
  return out;
}

std::string CleanActorLabel(const char* classname, const char* targetname, int eType) {
  if (targetname && targetname[0] != '\0' && std::strncmp(targetname, "auto", 4) != 0) {
    std::string t = targetname;
    for (char& c : t) if (c == '_') c = ' ';
    return t;
  }
  if (!classname || classname[0] == '\0') {
    return (eType == offsets::eType::Player) ? "Player" : "Enemy";
  }
  std::string s = classname;
  if (s.find("zombie") != std::string::npos) {
    if (s.find("dog") != std::string::npos) return "Hellhound";
    if (s.find("boss") != std::string::npos) return "Boss Zombie";
    return "Zombie";
  }
  if (s.find("dog") != std::string::npos || s.find("wolf") != std::string::npos) return "Attack Dog";
  if (s.find("axis_jap") != std::string::npos || s.find("japanese") != std::string::npos) return "IJA Soldier";
  if (s.find("axis_ger") != std::string::npos || s.find("german") != std::string::npos ||
      s.find("wehrmacht") != std::string::npos || s.find("waffen") != std::string::npos) return "Wehrmacht";
  if (s.find("ally") != std::string::npos || s.find("marine") != std::string::npos ||
      s.find("russian") != std::string::npos || s.find("soviet") != std::string::npos) return "Ally";
  if (s.rfind("actor_", 0) == 0) s = s.substr(6);
  for (char& c : s) if (c == '_') c = ' ';
  if (s.size() > 26) s.resize(26);
  return s.empty() ? "Enemy" : s;
}

std::string GetWeaponFriendlyName(uint32_t weapId) {
  if (weapId == 0) return {};
  uint32_t maxWeaps = *reinterpret_cast<const uint32_t*>(offsets::kBgNumWeapons);
  if (maxWeaps == 0 || maxWeaps > 512) maxWeaps = 256;
  if (weapId >= maxWeaps) return {};
  uintptr_t def = *reinterpret_cast<const uintptr_t*>(offsets::kBgWeaponDefs + 4 * weapId);
  if (!def) return {};
  const char* disp = *reinterpret_cast<const char* const*>(def + 0x04);
  const char* intl = *reinterpret_cast<const char* const*>(def + 0x00);
  if (disp && disp[0] != '\0' && std::strncmp(disp, "WEAPON_", 7) != 0) {
    std::string clean = StripColorCodes(disp);
    if (!clean.empty()) return clean;
  }
  if (intl && intl[0] != '\0') {
    std::string clean = intl;
    const size_t slash = clean.find('/');
    if (slash != std::string::npos) clean = clean.substr(slash + 1);
    const size_t us = clean.rfind("_sp");
    if (us != std::string::npos && us + 3 == clean.size()) clean.resize(us);
    for (char& c : clean) if (c == '_') c = ' ';
    return clean;
  }
  return {};
}

uintptr_t GetWeaponDefPtr(uint32_t weapId) {
  if (weapId == 0) return 0;
  uint32_t maxWeaps = *reinterpret_cast<const uint32_t*>(offsets::kBgNumWeapons);
  if (maxWeaps == 0 || maxWeaps > 512) maxWeaps = 256;
  if (weapId >= maxWeaps) return 0;
  return *reinterpret_cast<const uintptr_t*>(offsets::kBgWeaponDefs + 4 * weapId);
}

void UpdateWeaponDefsAndPlayerStats(const ModConfig& cfg, uintptr_t ps, uintptr_t gclient, uintptr_t gent0) {
  // 1. Dvar-backed mods
  SetDvarValue<uint8_t>(offsets::kDvarPlayerSustainAmmo, cfg.infAmmo ? 1 : 0);
  SetDvarValue<uint8_t>(offsets::kDvarPlayerSprintUnlimited, cfg.infSprint ? 1 : 0);
  if (cfg.noFallDmg) {
    SetDvarValue<float>(offsets::kDvarBgFallDmgMin, 99999.0f);
    SetDvarValue<float>(offsets::kDvarBgFallDmgMax, 99999.0f);
  } else {
    SetDvarValue<float>(offsets::kDvarBgFallDmgMin, 128.0f);
    SetDvarValue<float>(offsets::kDvarBgFallDmgMax, 300.0f);
  }

  if (cfg.overrideFov) {
    SetDvarValue<float>(offsets::kDvarCgFov, cfg.customFov);
  }
  SetDvarValue<uint8_t>(offsets::kDvarCgThirdPerson, cfg.thirdPerson ? 1 : 0);

  if (cfg.overrideSpeed) {
    SetDvarValue<int>(offsets::kDvarGSpeed, cfg.customSpeed);
  }
  if (cfg.overrideMaxHealth) {
    SetDvarValue<int>(offsets::kDvarGPlayerMaxHealth, cfg.customMaxHealth);
  }
  if (cfg.overrideTimescale) {
    SetDvarValue<float>(offsets::kDvarComTimescale, cfg.customTimescale);
  }

  // Incoming damage multiplier (demigod mitigation on top of FL_DEMI_GODMODE)
  if (cfg.demiGod) {
    SetDvarValue<float>(offsets::kDvarPlayerDamageMult, 0.0f);
    SetDvarValue<float>(offsets::kDvarPlayerMeleeDamageMult, 0.0f);
  }

  // 2. Player Entity flags (God / Demigod / Notarget) & client state (Noclip / UFO / Points)
  if (gent0) {
    int flags = *reinterpret_cast<int*>(gent0 + 0x1B4);
    if (cfg.godMode) flags |= offsets::entFlags::GodMode;
    else flags &= ~offsets::entFlags::GodMode;

    if (cfg.demiGod) flags |= offsets::entFlags::DemiGodMode;
    else flags &= ~offsets::entFlags::DemiGodMode;

    if (cfg.noTarget) flags |= offsets::entFlags::NoTarget;
    else flags &= ~offsets::entFlags::NoTarget;

    *reinterpret_cast<int*>(gent0 + 0x1B4) = flags;
  }

  if (gclient) {
    // Handle quick Noclip hotkey ('V') edge trigger
    static bool prevV = false;
    bool curV = (GetAsyncKeyState('V') & 0x8000) != 0;
    if (cfg.noclipKeyToggle && !cfg.menuOpen && curV && !prevV) {
      Config().noclip = !Config().noclip;
    }
    prevV = curV;

    *reinterpret_cast<int*>(gclient + 0x2234) = cfg.noclip ? 1 : 0;
    *reinterpret_cast<int*>(gclient + 0x2238) = cfg.ufo ? 1 : 0;

    if (cfg.lockPoints) {
      *reinterpret_cast<int*>(gclient + 0x20B8) = cfg.lockedPoints;
    }
  }

  // 3. WeaponDef live patching (backed up cleanly for full restoration)
  const uint32_t curWeapId = *reinterpret_cast<const uint32_t*>(ps + 0x104);
  const uintptr_t weapDef = GetWeaponDefPtr(curWeapId);
  if (weapDef) {
    if (g_weaponBackups.find(weapDef) == g_weaponBackups.end()) {
      WeaponDefBackup bk{};
      bk.fireDelay = *reinterpret_cast<const int*>(weapDef + 0x438);
      bk.fireTime = *reinterpret_cast<const int*>(weapDef + 0x448);
      bk.rechamberTime = *reinterpret_cast<const int*>(weapDef + 0x44C);
      bk.rechamberBoltTime = *reinterpret_cast<const int*>(weapDef + 0x450);
      bk.holdFireTime = *reinterpret_cast<const int*>(weapDef + 0x454);
      bk.reloadTime = *reinterpret_cast<const int*>(weapDef + 0x464);
      bk.reloadEmptyTime = *reinterpret_cast<const int*>(weapDef + 0x46C);
      bk.reloadAddTime = *reinterpret_cast<const int*>(weapDef + 0x470);
      bk.reloadEmptyAddTime = *reinterpret_cast<const int*>(weapDef + 0x474);
      bk.reloadStartTime = *reinterpret_cast<const int*>(weapDef + 0x478);
      bk.dropTime = *reinterpret_cast<const int*>(weapDef + 0x484);
      bk.raiseTime = *reinterpret_cast<const int*>(weapDef + 0x488);
      bk.quickDropTime = *reinterpret_cast<const int*>(weapDef + 0x494);
      bk.quickRaiseTime = *reinterpret_cast<const int*>(weapDef + 0x498);
      bk.sprintInTime = *reinterpret_cast<const int*>(weapDef + 0x4A8);
      bk.sprintOutTime = *reinterpret_cast<const int*>(weapDef + 0x4B0);
      bk.damage = *reinterpret_cast<const int*>(weapDef + 0x420);
      bk.minDamage = *reinterpret_cast<const int*>(weapDef + 0x914);
      bk.explosionInnerDamage = *reinterpret_cast<const int*>(weapDef + 0x658);
      bk.explosionOuterDamage = *reinterpret_cast<const int*>(weapDef + 0x65C);
      bk.fireType = *reinterpret_cast<const int*>(weapDef + 0x158);
      for (int i = 0; i < 6; ++i) {
        bk.hipSpread[i] = *reinterpret_cast<const float*>(weapDef + 0x53C + i * 4);
      }
      bk.adsSpread = *reinterpret_cast<const float*>(weapDef + 0x830);
      g_weaponBackups.emplace(weapDef, bk);
    }

    const WeaponDefBackup& bk = g_weaponBackups[weapDef];

    // No Spread
    if (cfg.noSpread) {
      for (int i = 0; i < 6; ++i) {
        *reinterpret_cast<float*>(weapDef + 0x53C + i * 4) = 0.0f;
      }
      *reinterpret_cast<float*>(weapDef + 0x830) = 0.0f;
    } else {
      for (int i = 0; i < 6; ++i) {
        *reinterpret_cast<float*>(weapDef + 0x53C + i * 4) = bk.hipSpread[i];
      }
      *reinterpret_cast<float*>(weapDef + 0x830) = bk.adsSpread;
    }

    // Fast Reload
    if (cfg.fastReload) {
      auto scaleTime = [&](int orig) {
        if (orig <= 0) return orig;
        return std::max(30, static_cast<int>(std::round(static_cast<float>(orig) * cfg.reloadSpeedScale)));
      };
      *reinterpret_cast<int*>(weapDef + 0x464) = scaleTime(bk.reloadTime);
      *reinterpret_cast<int*>(weapDef + 0x46C) = scaleTime(bk.reloadEmptyTime);
      *reinterpret_cast<int*>(weapDef + 0x470) = scaleTime(bk.reloadAddTime);
      *reinterpret_cast<int*>(weapDef + 0x474) = scaleTime(bk.reloadEmptyAddTime);
      *reinterpret_cast<int*>(weapDef + 0x478) = scaleTime(bk.reloadStartTime);
    } else {
      *reinterpret_cast<int*>(weapDef + 0x464) = bk.reloadTime;
      *reinterpret_cast<int*>(weapDef + 0x46C) = bk.reloadEmptyTime;
      *reinterpret_cast<int*>(weapDef + 0x470) = bk.reloadAddTime;
      *reinterpret_cast<int*>(weapDef + 0x474) = bk.reloadEmptyAddTime;
      *reinterpret_cast<int*>(weapDef + 0x478) = bk.reloadStartTime;
    }

    // Rapid Fire
    if (cfg.rapidFire) {
      const float div = std::max(1.0f, cfg.rapidFireMult);
      auto fastT = [&](int orig) {
        if (orig <= 0) return orig;
        return std::max(16, static_cast<int>(std::round(static_cast<float>(orig) / div)));
      };
      *reinterpret_cast<int*>(weapDef + 0x438) = fastT(bk.fireDelay);
      *reinterpret_cast<int*>(weapDef + 0x448) = fastT(bk.fireTime);
      *reinterpret_cast<int*>(weapDef + 0x44C) = fastT(bk.rechamberTime);
      *reinterpret_cast<int*>(weapDef + 0x450) = fastT(bk.rechamberBoltTime);
      *reinterpret_cast<int*>(weapDef + 0x454) = fastT(bk.holdFireTime);
    } else {
      *reinterpret_cast<int*>(weapDef + 0x438) = bk.fireDelay;
      *reinterpret_cast<int*>(weapDef + 0x448) = bk.fireTime;
      *reinterpret_cast<int*>(weapDef + 0x44C) = bk.rechamberTime;
      *reinterpret_cast<int*>(weapDef + 0x450) = bk.rechamberBoltTime;
      *reinterpret_cast<int*>(weapDef + 0x454) = bk.holdFireTime;
    }

    // Full Auto
    *reinterpret_cast<int*>(weapDef + 0x158) = cfg.fullAuto ? 0 : bk.fireType;

    // Instant Swap / Sprint-to-fire
    if (cfg.instantSwap) {
      *reinterpret_cast<int*>(weapDef + 0x484) = 15;
      *reinterpret_cast<int*>(weapDef + 0x488) = 15;
      *reinterpret_cast<int*>(weapDef + 0x494) = 15;
      *reinterpret_cast<int*>(weapDef + 0x498) = 15;
      *reinterpret_cast<int*>(weapDef + 0x4A8) = 10;
      *reinterpret_cast<int*>(weapDef + 0x4B0) = 10;
    } else {
      *reinterpret_cast<int*>(weapDef + 0x484) = bk.dropTime;
      *reinterpret_cast<int*>(weapDef + 0x488) = bk.raiseTime;
      *reinterpret_cast<int*>(weapDef + 0x494) = bk.quickDropTime;
      *reinterpret_cast<int*>(weapDef + 0x498) = bk.quickRaiseTime;
      *reinterpret_cast<int*>(weapDef + 0x4A8) = bk.sprintInTime;
      *reinterpret_cast<int*>(weapDef + 0x4B0) = bk.sprintOutTime;
    }

    // One-Hit Kill (Enemy Weapon Damage override - NOT player_damageMultiplier!)
    if (cfg.oneHitKill) {
      *reinterpret_cast<int*>(weapDef + 0x420) = cfg.customWeaponDmg;
      *reinterpret_cast<int*>(weapDef + 0x914) = cfg.customWeaponDmg;
      if (bk.explosionInnerDamage > 0) *reinterpret_cast<int*>(weapDef + 0x658) = cfg.customWeaponDmg;
      if (bk.explosionOuterDamage > 0) *reinterpret_cast<int*>(weapDef + 0x65C) = cfg.customWeaponDmg;
    } else {
      *reinterpret_cast<int*>(weapDef + 0x420) = bk.damage;
      *reinterpret_cast<int*>(weapDef + 0x914) = bk.minDamage;
      *reinterpret_cast<int*>(weapDef + 0x658) = bk.explosionInnerDamage;
      *reinterpret_cast<int*>(weapDef + 0x65C) = bk.explosionOuterDamage;
    }

    // Live clip & reserve top-off + instant reload state acceleration
    const int ammoIdx = *reinterpret_cast<const int*>(weapDef + 0x3F4);
    const int clipIdx = *reinterpret_cast<const int*>(weapDef + 0x3FC);
    const int clipCap = std::max(1, *reinterpret_cast<const int*>(weapDef + 0x408));
    const int maxReserve = std::max(clipCap, *reinterpret_cast<const int*>(weapDef + 0x404));

    if (cfg.infAmmo) {
      if (clipIdx >= 0 && clipIdx < 128) {
        *reinterpret_cast<int*>(ps + 0x5FC + 4 * clipIdx) = clipCap;
        if (gclient) *reinterpret_cast<int*>(gclient + 0x5FC + 4 * clipIdx) = clipCap;
      }
      if (cfg.infAmmoReserve && ammoIdx >= 0 && ammoIdx < 128) {
        *reinterpret_cast<int*>(ps + 0x17C + 4 * ammoIdx) = maxReserve;
        if (gclient) *reinterpret_cast<int*>(gclient + 0x17C + 4 * ammoIdx) = maxReserve;
      }
    }

    // Accelerate active reload state or active fire delay on ps & gclient
    const int wState = *reinterpret_cast<const int*>(ps + 0x108);
    if (cfg.fastReload && wState >= 7 && wState <= 11) {
      int* pWt = reinterpret_cast<int*>(ps + 0x40);
      if (*pWt > 50) *pWt = 50;
      if (gclient) {
        int* sWt = reinterpret_cast<int*>(gclient + 0x40);
        if (*sWt > 50) *sWt = 50;
      }
    }
    if (cfg.rapidFire && (wState == 5 || wState == 6)) {
      const int maxFireMs = std::max(15, *reinterpret_cast<const int*>(weapDef + 0x448));
      int* pWt = reinterpret_cast<int*>(ps + 0x40);
      if (*pWt > maxFireMs) *pWt = maxFireMs;
      if (gclient) {
        int* sWt = reinterpret_cast<int*>(gclient + 0x40);
        if (*sWt > maxFireMs) *sWt = maxFireMs;
      }
    }
  }

  // Zero active visual/server kick angles when No Recoil is active
  if (cfg.noRecoil) {
    float* kickAVel = reinterpret_cast<float*>(offsets::kCgKickAVel);
    float* kickAng = reinterpret_cast<float*>(offsets::kCgKickAngles);
    kickAVel[0] = kickAVel[1] = kickAVel[2] = 0.0f;
    kickAng[0] = kickAng[1] = kickAng[2] = 0.0f;
    if (gclient) {
      float* srvKick = reinterpret_cast<float*>(gclient + 0x232C);
      srvKick[0] = srvKick[1] = srvKick[2] = 0.0f;
    }
  }
}

void ProcessActionQueue(uintptr_t gclient, bool hasServerHost) {
  std::deque<QueuedAction> local;
  {
    std::lock_guard<std::mutex> lock(g_actionMutex);
    local.swap(g_actions);
  }
  while (!local.empty()) {
    QueuedAction act = std::move(local.front());
    local.pop_front();
    switch (act.type) {
      case ActionType::ExecCmd:
        if (!act.strArg.empty()) {
          std::string cmd = act.strArg;
          if (cmd.back() != '\n') cmd.push_back('\n');
          CbufAddText(cmd.c_str());
        }
        break;
      case ActionType::AddPoints:
        if (hasServerHost && gclient) {
          int* pScore = reinterpret_cast<int*>(gclient + 0x20B8);
          *pScore += act.intArg;
        }
        break;
      case ActionType::RefillAmmo:
        SetDvarValue<uint8_t>(offsets::kDvarSvCheats, 1);
        CbufAddText("give ammo\n");
        break;
      case ActionType::WeakAllEnemies:
        if (hasServerHost) {
          for (int i = 1; i < offsets::kMaxEntities; ++i) {
            uintptr_t g = offsets::kGEntities + static_cast<uintptr_t>(i) * offsets::kGentityStride;
            int eType = *reinterpret_cast<const int*>(g + 0x04);
            if (eType != offsets::eType::Actor) continue;
            uintptr_t sent = *reinterpret_cast<const uintptr_t*>(g + 0x188);
            int team = sent ? *reinterpret_cast<const int*>(sent + 0x04) : offsets::team::Axis;
            if (team == offsets::team::Axis || team == offsets::team::Free) {
              int* hp = reinterpret_cast<int*>(g + 0x1C8);
              if (*hp > 1) *hp = 1;
            }
          }
        }
        break;
      case ActionType::ToggleCheats:
        SetDvarValue<uint8_t>(offsets::kDvarSvCheats, 1);
        break;
      default:
        break;
    }
  }
}

bool GetEntityBonePosValidated(int entNum, uint16_t tag, const Vec3& entOrigin, Vec3& outPos) {
  if (!GetEntityBonePos(entNum, tag, outPos)) return false;
  if (!std::isfinite(outPos.x) || !std::isfinite(outPos.y) || !std::isfinite(outPos.z)) return false;
  const float dx = outPos.x - entOrigin.x;
  const float dy = outPos.y - entOrigin.y;
  const float dz = outPos.z - entOrigin.z;
  return (dx * dx + dy * dy + dz * dz) <= (115.0f * 115.0f);
}

Vec3 GetTargetAimBonePos(int entNum, int boneIdx, const Vec3& origin3D) {
  Vec3 out{};
  uint16_t tag = g_tags.j_head;
  if (boneIdx == static_cast<int>(AimBone::Neck)) tag = g_tags.j_neck;
  else if (boneIdx == static_cast<int>(AimBone::Chest)) tag = g_tags.j_spineupper;
  else if (boneIdx == static_cast<int>(AimBone::Pelvis)) tag = g_tags.pelvis;

  if (tag && GetEntityBonePosValidated(entNum, tag, origin3D, out)) return out;
  if (g_tags.j_spineupper && GetEntityBonePosValidated(entNum, g_tags.j_spineupper, origin3D, out)) return out;
  if (g_tags.pelvis && GetEntityBonePosValidated(entNum, g_tags.pelvis, origin3D, out)) return out;

  out = origin3D;
  if (boneIdx == static_cast<int>(AimBone::Head)) out.z += 58.0f;
  else if (boneIdx == static_cast<int>(AimBone::Neck)) out.z += 50.0f;
  else if (boneIdx == static_cast<int>(AimBone::Chest)) out.z += 40.0f;
  else out.z += 28.0f;
  return out;
}

void CollectSkeletonBones(int entNum, const Vec3& origin3D, std::vector<BonePair3D>& outBones) {
  Vec3 head, neck, spineUp, spineLow, pelvis;
  if (!GetEntityBonePosValidated(entNum, g_tags.j_head, origin3D, head)) return;
  if (!GetEntityBonePosValidated(entNum, g_tags.j_neck, origin3D, neck)) neck = head;
  if (!GetEntityBonePosValidated(entNum, g_tags.j_spineupper, origin3D, spineUp)) spineUp = neck;
  if (!GetEntityBonePosValidated(entNum, g_tags.j_spinelower, origin3D, spineLow)) spineLow = spineUp;
  if (!GetEntityBonePosValidated(entNum, g_tags.pelvis, origin3D, pelvis)) pelvis = spineLow;

  outBones.push_back({head, neck});
  outBones.push_back({neck, spineUp});
  outBones.push_back({spineUp, spineLow});
  outBones.push_back({spineLow, pelvis});

  Vec3 sL, eL, wL, sR, eR, wR;
  if (GetEntityBonePosValidated(entNum, g_tags.j_shoulder_le, origin3D, sL)) {
    outBones.push_back({neck, sL});
    if (GetEntityBonePosValidated(entNum, g_tags.j_elbow_le, origin3D, eL)) {
      outBones.push_back({sL, eL});
      if (GetEntityBonePosValidated(entNum, g_tags.j_wrist_le, origin3D, wL)) outBones.push_back({eL, wL});
    }
  }
  if (GetEntityBonePosValidated(entNum, g_tags.j_shoulder_ri, origin3D, sR)) {
    outBones.push_back({neck, sR});
    if (GetEntityBonePosValidated(entNum, g_tags.j_elbow_ri, origin3D, eR)) {
      outBones.push_back({sR, eR});
      if (GetEntityBonePosValidated(entNum, g_tags.j_wrist_ri, origin3D, wR)) outBones.push_back({eR, wR});
    }
  }

  Vec3 hL, kL, aL, hR, kR, aR;
  if (GetEntityBonePosValidated(entNum, g_tags.j_hip_le, origin3D, hL)) {
    outBones.push_back({pelvis, hL});
    if (GetEntityBonePosValidated(entNum, g_tags.j_knee_le, origin3D, kL)) {
      outBones.push_back({hL, kL});
      if (GetEntityBonePosValidated(entNum, g_tags.j_ankle_le, origin3D, aL)) outBones.push_back({kL, aL});
    }
  }
  if (GetEntityBonePosValidated(entNum, g_tags.j_hip_ri, origin3D, hR)) {
    outBones.push_back({pelvis, hR});
    if (GetEntityBonePosValidated(entNum, g_tags.j_knee_ri, origin3D, kR)) {
      outBones.push_back({hR, kR});
      if (GetEntityBonePosValidated(entNum, g_tags.j_ankle_ri, origin3D, aR)) outBones.push_back({kR, aR});
    }
  }
}

void RunCameraAimbotAndTrigger(const ModConfig& cfg, const FrameSnapshot& snap, const Vec3& vieworg, uintptr_t ps) {
  if (!cfg.aimEnabled || cfg.menuOpen) return;

  if (cfg.aimMode == static_cast<int>(AimMode::Camera) && snap.aimKeyDown && snap.hasAimTarget) {
    Vec3 targetAng = CalcAimAngles(vieworg, snap.aimTargetPos3D);
    float* viewAngles = reinterpret_cast<float*>(offsets::kClViewAngles);
    Vec3 delta{
        targetAng.x - viewAngles[0],
        targetAng.y - viewAngles[1],
        0.0f,
    };
    NormalizeAngles(delta);

    const float smooth = std::max(1.0f, cfg.aimSmooth);
    viewAngles[0] += delta.x / smooth;
    viewAngles[1] += delta.y / smooth;
    NormalizeAngles(*reinterpret_cast<Vec3*>(viewAngles));
  }

  // Triggerbot / Auto-Fire when locked onto target within triggerFov (and ready to fire)
  if (cfg.triggerbot && snap.hasAimTarget && snap.aimTargetVisible) {
    Vec2 target2D{}, scrSz{};
    if (WorldToScreen(snap.aimTargetPos3D, target2D, &scrSz)) {
      const float dx = target2D.x - scrSz.x * 0.5f;
      const float dy = target2D.y - scrSz.y * 0.5f;
      const float pxDist = std::sqrt(dx * dx + dy * dy);
      if (pxDist <= cfg.triggerFov) {
        const int wTime = *reinterpret_cast<const int*>(ps + 0x40);
        const int wState = *reinterpret_cast<const int*>(ps + 0x108);
        if (wTime <= 0 && wState == 0) {
          INPUT inp[2]{};
          inp[0].type = INPUT_MOUSE;
          inp[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
          inp[1].type = INPUT_MOUSE;
          inp[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
          SendInput(2, inp, sizeof(INPUT));
        }
      }
    }
  }
}

} // namespace

void QueueAction(ActionType type, int intArg, const char* strArg) {
  std::lock_guard<std::mutex> lock(g_actionMutex);
  QueuedAction act;
  act.type = type;
  act.intArg = intArg;
  if (strArg) act.strArg = strArg;
  g_actions.push_back(std::move(act));
}

FrameSnapshot CopyFrameSnapshot() {
  std::lock_guard<std::mutex> lock(g_snapMutex);
  return g_frameSnapshot;
}

SilentTargetState GetSilentTargetState() {
  std::lock_guard<std::mutex> lock(g_silentMutex);
  return g_silentTarget;
}

const char* SLConvertToString(uint16_t id) {
  if (id == 0) return nullptr;
  const uintptr_t base = *reinterpret_cast<const uintptr_t*>(offsets::kSlStringTableBasePtr);
  if (!base) return nullptr;
  const uintptr_t addr = base + static_cast<uintptr_t>(id) * 12u + 4u;
  if (!IsReadableMemory(addr, 4)) return nullptr;
  return reinterpret_cast<const char*>(addr);
}

uint16_t SLGetString(const char* str) {
  if (!str) return 0;
  using FnSLGetString = uint16_t(__cdecl*)(const char*, int);
  auto fn = reinterpret_cast<FnSLGetString>(offsets::kFnSLGetString);
  return fn(str, 0);
}

bool IsEntityAliveServer(int entNum) {
  if (entNum < 0 || entNum >= offsets::kMaxEntities) return false;
  const uintptr_t gclientBase = *reinterpret_cast<const uintptr_t*>(offsets::kGClientsPtr);
  if (!gclientBase) return true;
  const uintptr_t g = offsets::kGEntities + static_cast<uintptr_t>(entNum) * offsets::kGentityStride;
  return *reinterpret_cast<const int*>(g + 0x1C8) > 0;
}

__attribute__((noinline)) void CbufAddText(const char* cmd) {
  if (!cmd || !cmd[0]) return;
  struct CbufArgs {
    const char* text;
    uintptr_t fn;
  } args{cmd, offsets::kFnCbufAddText};
  __asm__ __volatile__(
      "pushl 0(%[a])\n\t"
      "movl 4(%[a]), %%ecx\n\t"
      "xorl %%eax, %%eax\n\t"
      "calll *%%ecx\n\t"
      "addl $4, %%esp\n\t"
      :
      : [a]"S"(&args)
      : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "cc", "memory"
  );
}

__attribute__((noinline)) bool CGTraceVisible(const Vec3& start, const Vec3& end, int passEntNum, int targetEntNum) {
  uint8_t traceBuf[0x4C]{};
  struct TraceArgs {
    const float* start;
    uintptr_t mins;
    uintptr_t maxs;
    const float* end;
    int passEnt;
    int mask;
    void* tracePtr;
    uintptr_t fn;
  } args{
      &start.x,
      offsets::kTraceVecZero,
      offsets::kTraceVecZero,
      &end.x,
      passEntNum,
      0x280E833,
      traceBuf,
      offsets::kFnCGTrace,
  };

  __asm__ __volatile__(
      "pushl %%edi\n\t"
      "pushl $0\n\t"
      "pushl $0\n\t"
      "pushl $0\n\t"
      "pushl $1\n\t"
      "pushl $1\n\t"
      "pushl 20(%[a])\n\t" /* mask */
      "pushl 16(%[a])\n\t" /* passEnt */
      "pushl 12(%[a])\n\t" /* end */
      "pushl 8(%[a])\n\t"  /* maxs */
      "pushl 0(%[a])\n\t"  /* start */
      "movl 24(%[a]), %%edi\n\t" /* trace_t* */
      "movl 4(%[a]), %%eax\n\t"  /* mins */
      "movl 28(%[a]), %%ecx\n\t" /* fn */
      "calll *%%ecx\n\t"
      "addl $40, %%esp\n\t"
      "popl %%edi\n\t"
      :
      : [a]"S"(&args)
      : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "cc", "memory"
  );

  const float frac = *reinterpret_cast<const float*>(traceBuf + 0x10);
  const int hitType = *reinterpret_cast<const int*>(traceBuf + 0x20);
  const uint16_t hitId = *reinterpret_cast<const uint16_t*>(traceBuf + 0x24);
  if (frac >= 0.96f) return true;
  if (hitType == 1 && static_cast<int>(hitId) == targetEntNum) return true;
  return false;
}

__attribute__((noinline)) bool GetEntityBonePos(int entNum, uint16_t tag, Vec3& outPos) {
  if (entNum < 0 || entNum >= offsets::kMaxEntities || tag == 0) return false;
  const uint16_t dobjIdx = *reinterpret_cast<const uint16_t*>(offsets::kObjIndexTable + 2 * static_cast<uintptr_t>(entNum));
  if (dobjIdx == 0) return false;
  void* dobj = reinterpret_cast<void*>(offsets::kObjPoolBase + static_cast<int32_t>(static_cast<int16_t>(dobjIdx)) * 0x68);
  const void* centity = reinterpret_cast<const void*>(offsets::kCgEntities + static_cast<uintptr_t>(entNum) * offsets::kCentityStride);

  struct BoneCallArgs {
    void* dobj;
    uint32_t tag;
    const void* cent;
    float* outp;
    uintptr_t fn;
  } args{dobj, static_cast<uint32_t>(tag), centity, &outPos.x, offsets::kFnCGDObjGetWorldTagPosInternal};

  int ok = 0;
  __asm__ __volatile__(
      "pushl %%edi\n\t"
      "pushl 12(%%eax)\n\t" /* outPos */
      "pushl 8(%%eax)\n\t"  /* centity */
      "movl 4(%%eax), %%ecx\n\t" /* tag */
      "movl 0(%%eax), %%edi\n\t" /* dobj */
      "movl 16(%%eax), %%edx\n\t" /* fn */
      "calll *%%edx\n\t"
      "addl $8, %%esp\n\t"
      "popl %%edi\n\t"
      : "=a"(ok)
      : "a"(&args)
      : "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "cc", "memory"
  );

  return ok != 0;
}

void NormalizeAngles(Vec3& a) {
  while (a.y > 180.0f) a.y -= 360.0f;
  while (a.y < -180.0f) a.y += 360.0f;
  if (a.x > 89.0f) a.x = 89.0f;
  if (a.x < -89.0f) a.x = -89.0f;
  a.z = 0.0f;
}

Vec3 CalcAimAngles(const Vec3& src, const Vec3& dst) {
  const float dx = dst.x - src.x;
  const float dy = dst.y - src.y;
  const float dz = dst.z - src.z;
  const float hyp = std::sqrt(dx * dx + dy * dy);
  Vec3 out{};
  out.x = -std::atan2(dz, hyp) * kRad2Deg;
  out.y = std::atan2(dy, dx) * kRad2Deg;
  out.z = 0.0f;
  NormalizeAngles(out);
  return out;
}

void AngleVectors(const Vec3& angles, Vec3* forward, Vec3* right, Vec3* up) {
  const float sy = std::sin(angles.y * kDeg2Rad);
  const float cy = std::cos(angles.y * kDeg2Rad);
  const float sp = std::sin(angles.x * kDeg2Rad);
  const float cp = std::cos(angles.x * kDeg2Rad);
  const float sr = std::sin(angles.z * kDeg2Rad);
  const float cr = std::cos(angles.z * kDeg2Rad);

  if (forward) {
    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
  }
  if (right) {
    right->x = (-1.0f * sr * sp * cy + -1.0f * cr * -sy);
    right->y = (-1.0f * sr * sp * sy + -1.0f * cr * cy);
    right->z = -1.0f * sr * cp;
  }
  if (up) {
    up->x = (cr * sp * cy + -sr * -sy);
    up->y = (cr * sp * sy + -sr * cy);
    up->z = cr * cp;
  }
}

bool WorldToScreen(const Vec3& world, Vec2& outScreen, Vec2* outScreenSize) {
  const uintptr_t rd = offsets::kRefdef;
  const int w = *reinterpret_cast<const int*>(rd + 0x08);
  const int h = *reinterpret_cast<const int*>(rd + 0x0C);
  const float tanX = *reinterpret_cast<const float*>(rd + 0x10);
  const float tanY = *reinterpret_cast<const float*>(rd + 0x14);
  if (w <= 0 || h <= 0 || tanX <= 0.0001f || tanY <= 0.0001f) return false;
  if (outScreenSize) {
    outScreenSize->x = static_cast<float>(w);
    outScreenSize->y = static_cast<float>(h);
  }

  const Vec3* vieworg = reinterpret_cast<const Vec3*>(rd + 0x1C);
  const Vec3* fwd = reinterpret_cast<const Vec3*>(rd + 0x2C);
  const Vec3* left = reinterpret_cast<const Vec3*>(rd + 0x38);
  const Vec3* up = reinterpret_cast<const Vec3*>(rd + 0x44);

  const float dx = world.x - vieworg->x;
  const float dy = world.y - vieworg->y;
  const float dz = world.z - vieworg->z;

  const float z = dx * fwd->x + dy * fwd->y + dz * fwd->z;
  if (z <= 0.1f) return false;

  const float xLeft = dx * left->x + dy * left->y + dz * left->z;
  const float yUp = dx * up->x + dy * up->y + dz * up->z;

  const float cx = static_cast<float>(w) * 0.5f;
  const float cy = static_cast<float>(h) * 0.5f;
  // CoD viewaxis[1] points LEFT, so screen X subtracts right/left offset
  outScreen.x = cx * (1.0f - (xLeft / tanX / z));
  outScreen.y = cy * (1.0f - (yUp / tanY / z));
  return true;
}

bool CalcOffscreenDir(const Vec3& world, Vec2& outDir, float& outAngleRad) {
  const uintptr_t rd = offsets::kRefdef;
  const Vec3* vieworg = reinterpret_cast<const Vec3*>(rd + 0x1C);
  const Vec3* fwd = reinterpret_cast<const Vec3*>(rd + 0x2C);
  const Vec3* left = reinterpret_cast<const Vec3*>(rd + 0x38);

  const float dx = world.x - vieworg->x;
  const float dy = world.y - vieworg->y;
  // 2D planar direction in view plane (top = forward, right = -left)
  const float f = dx * fwd->x + dy * fwd->y;
  const float r = -(dx * left->x + dy * left->y);
  const float len = std::sqrt(f * f + r * r);
  if (len < 0.001f) return false;

  // Screen coordinates: +X right, +Y down (so forward is -Y)
  outDir.x = r / len;
  outDir.y = -f / len;
  outAngleRad = std::atan2(outDir.y, outDir.x);
  return true;
}

void RestoreAllModifiedWeaponDefs() {
  for (const auto& kv : g_weaponBackups) {
    const uintptr_t weapDef = kv.first;
    if (!IsReadableMemory(weapDef, 0x920)) continue;
    const WeaponDefBackup& bk = kv.second;
    *reinterpret_cast<int*>(weapDef + 0x438) = bk.fireDelay;
    *reinterpret_cast<int*>(weapDef + 0x448) = bk.fireTime;
    *reinterpret_cast<int*>(weapDef + 0x44C) = bk.rechamberTime;
    *reinterpret_cast<int*>(weapDef + 0x450) = bk.rechamberBoltTime;
    *reinterpret_cast<int*>(weapDef + 0x454) = bk.holdFireTime;
    *reinterpret_cast<int*>(weapDef + 0x464) = bk.reloadTime;
    *reinterpret_cast<int*>(weapDef + 0x46C) = bk.reloadEmptyTime;
    *reinterpret_cast<int*>(weapDef + 0x470) = bk.reloadAddTime;
    *reinterpret_cast<int*>(weapDef + 0x474) = bk.reloadEmptyAddTime;
    *reinterpret_cast<int*>(weapDef + 0x478) = bk.reloadStartTime;
    *reinterpret_cast<int*>(weapDef + 0x484) = bk.dropTime;
    *reinterpret_cast<int*>(weapDef + 0x488) = bk.raiseTime;
    *reinterpret_cast<int*>(weapDef + 0x494) = bk.quickDropTime;
    *reinterpret_cast<int*>(weapDef + 0x498) = bk.quickRaiseTime;
    *reinterpret_cast<int*>(weapDef + 0x4A8) = bk.sprintInTime;
    *reinterpret_cast<int*>(weapDef + 0x4B0) = bk.sprintOutTime;
    *reinterpret_cast<int*>(weapDef + 0x420) = bk.damage;
    *reinterpret_cast<int*>(weapDef + 0x914) = bk.minDamage;
    *reinterpret_cast<int*>(weapDef + 0x658) = bk.explosionInnerDamage;
    *reinterpret_cast<int*>(weapDef + 0x65C) = bk.explosionOuterDamage;
    *reinterpret_cast<int*>(weapDef + 0x158) = bk.fireType;
    for (int i = 0; i < 6; ++i) {
      *reinterpret_cast<float*>(weapDef + 0x53C + i * 4) = bk.hipSpread[i];
    }
    *reinterpret_cast<float*>(weapDef + 0x830) = bk.adsSpread;
  }
  g_weaponBackups.clear();
}

void OnEndSceneCleanup() {
  RestoreAllModifiedWeaponDefs();
  g_tags.resolved = false;
  {
    std::lock_guard<std::mutex> lock(g_snapMutex);
    g_frameSnapshot = FrameSnapshot{};
  }
  {
    std::lock_guard<std::mutex> lock(g_silentMutex);
    g_silentTarget = SilentTargetState{};
  }
}

void OnMainGameTick() {
  const ModConfig& cfg = Config();
  if (cfg.unloading) return;

  const int connState = *reinterpret_cast<const int*>(offsets::kClsState);
  const uintptr_t cgSnap = *reinterpret_cast<const uintptr_t*>(offsets::kCgSnapPtr);
  if (connState < 9 || !cgSnap) {
    if (!g_weaponBackups.empty()) RestoreAllModifiedWeaponDefs();
    g_tags.resolved = false;
    {
      std::lock_guard<std::mutex> lock(g_snapMutex);
      g_frameSnapshot = FrameSnapshot{};
    }
    {
      std::lock_guard<std::mutex> lock(g_silentMutex);
      g_silentTarget = SilentTargetState{};
    }
    return;
  }

  EnsureBoneTagsResolved();

  FrameSnapshot snap;
  snap.inGame = true;

  const uintptr_t ps = offsets::kCgPredictedPS;
  const int pmType = *reinterpret_cast<const int*>(ps + 0x04);
  const int localClientNum = *reinterpret_cast<const int*>(ps + 0xF8);
  snap.localClientNum = localClientNum;

  const uintptr_t gclientBase = *reinterpret_cast<const uintptr_t*>(offsets::kGClientsPtr);
  const bool hasServer = (gclientBase != 0 && IsReadableMemory(gclientBase, 0x2400));
  snap.hasServerHost = hasServer;

  uintptr_t gclient0 = 0;
  uintptr_t gent0 = 0;
  if (hasServer && localClientNum >= 0 && localClientNum < 4) {
    gent0 = offsets::kGEntities + static_cast<uintptr_t>(localClientNum) * offsets::kGentityStride;
    gclient0 = *reinterpret_cast<const uintptr_t*>(gent0 + 0x180);
    if (!gclient0) gclient0 = gclientBase + static_cast<uintptr_t>(localClientNum) * offsets::kGclientStride;
    if (gclient0) {
      snap.playerHealth = *reinterpret_cast<const int*>(gclient0 + 0x164);
      snap.playerMaxHealth = *reinterpret_cast<const int*>(gent0 + 0x1CC);
      snap.playerPoints = *reinterpret_cast<const int*>(gclient0 + 0x20B8);
      snap.playerKills = *reinterpret_cast<const int*>(gclient0 + 0x20BC);
    }
  } else {
    snap.playerHealth = *reinterpret_cast<const int*>(ps + 0x164);
  }

  const uint32_t curWeapId = *reinterpret_cast<const uint32_t*>(ps + 0x104);
  snap.currentWeaponName = GetWeaponFriendlyName(curWeapId);
  if (uintptr_t wd = GetWeaponDefPtr(curWeapId)) {
    const int ammoIdx = *reinterpret_cast<const int*>(wd + 0x3F4);
    const int clipIdx = *reinterpret_cast<const int*>(wd + 0x3FC);
    if (clipIdx >= 0 && clipIdx < 128) snap.weaponClip = *reinterpret_cast<const int*>(ps + 0x5FC + 4 * clipIdx);
    if (ammoIdx >= 0 && ammoIdx < 128) snap.weaponReserve = *reinterpret_cast<const int*>(ps + 0x17C + 4 * ammoIdx);
  }

  const Vec3 playerOrig = *reinterpret_cast<const Vec3*>(ps + 0x20);
  snap.playerOrigin = playerOrig;

  // Execute queued Cbuf / Point / Cheat actions cleanly on main thread
  ProcessActionQueue(gclient0, hasServer);

  // Apply continuous weapon & player mods while player alive
  if (pmType < 7) {
    UpdateWeaponDefsAndPlayerStats(cfg, ps, gclient0, gent0);
  }

  const Vec3 vieworg = *reinterpret_cast<const Vec3*>(offsets::kRefdef + 0x1C);
  const float screenW = static_cast<float>(*reinterpret_cast<const int*>(offsets::kRefdef + 0x08));
  const float screenH = static_cast<float>(*reinterpret_cast<const int*>(offsets::kRefdef + 0x0C));
  const Vec2 screenCenter{screenW * 0.5f, screenH * 0.5f};

  snap.aimKeyDown = cfg.aimAlwaysOn || ((GetAsyncKeyState(cfg.aimKey) & 0x8000) != 0);

  SilentTargetState bestSilent;
  float bestScore = 1e9f;

  // Iterate all entities
  for (int i = 0; i < offsets::kMaxEntities; ++i) {
    if (i == localClientNum) continue;

    const uintptr_t cent = offsets::kCgEntities + static_cast<uintptr_t>(i) * offsets::kCentityStride;
    const int cValid = *reinterpret_cast<const int*>(cent + 0x2D0);
    const bool centValid = (cValid & 2) != 0;
    const int ceType = *reinterpret_cast<const int*>(cent + 0xD4);

    int eType = ceType;
    Vec3 entOrigin = *reinterpret_cast<const Vec3*>(cent + 0x24);

    if (hasServer) {
      const uintptr_t gent = offsets::kGEntities + static_cast<uintptr_t>(i) * offsets::kGentityStride;
      const int geType = *reinterpret_cast<const int*>(gent + 0x04);
      const int gHp = *reinterpret_cast<const int*>(gent + 0x1C8);
      const uintptr_t gActor = *reinterpret_cast<const uintptr_t*>(gent + 0x184);

      if (geType == offsets::eType::Actor && gActor != 0 && gHp > 0) {
        eType = offsets::eType::Actor;
        const Vec3 gOrg = *reinterpret_cast<const Vec3*>(gent + 0x160);
        const float cdx = entOrigin.x - gOrg.x;
        const float cdy = entOrigin.y - gOrg.y;
        const float cdz = entOrigin.z - gOrg.z;
        const bool cOrgMatches = centValid && (ceType == geType) && ((cdx * cdx + cdy * cdy + cdz * cdz) < 96.0f * 96.0f);
        if (!cOrgMatches) entOrigin = gOrg;
      } else {
        if (!centValid) continue;
        eType = ceType;
      }
    } else {
      if (!centValid) continue;
    }

    if (std::fabs(entOrigin.x) < 0.01f && std::fabs(entOrigin.y) < 0.01f && std::fabs(entOrigin.z) < 0.01f) continue;

    const float dx = entOrigin.x - playerOrig.x;
    const float dy = entOrigin.y - playerOrig.y;
    const float dz = entOrigin.z - playerOrig.z;
    const float distMeters = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.0254f; // WaW units -> meters (~1m = 39.37u)

    // Actors & Players (ESP + Aimbot)
    if (eType == offsets::eType::Actor || eType == offsets::eType::Player) {
      bool alive = true;
      bool isHostile = true;
      int hp = 100;
      int maxHp = 100;
      const char* clsStr = nullptr;
      const char* tgtStr = nullptr;

      if (hasServer) {
        const uintptr_t gent = offsets::kGEntities + static_cast<uintptr_t>(i) * offsets::kGentityStride;
        const int sType = *reinterpret_cast<const int*>(gent + 0x04);
        if (sType != eType) continue;
        hp = *reinterpret_cast<const int*>(gent + 0x1C8);
        maxHp = *reinterpret_cast<const int*>(gent + 0x1CC);
        if (hp <= 0) alive = false;
        uintptr_t sentient = *reinterpret_cast<const uintptr_t*>(gent + 0x188);
        if (sentient) {
          int eTeam = *reinterpret_cast<const int*>(sentient + 0x04);
          if (eTeam == offsets::team::Dead) alive = false;
          isHostile = (eTeam != offsets::team::Allies);
        }
        clsStr = SLConvertToString(*reinterpret_cast<const uint16_t*>(gent + 0x1A0));
        tgtStr = SLConvertToString(*reinterpret_cast<const uint16_t*>(gent + 0x1A8));
      } else {
        if (eType == offsets::eType::Actor) {
          const uintptr_t actInfo = offsets::kCgActorInfo + static_cast<uintptr_t>(i) * 0x458;
          const int aTeam = *reinterpret_cast<const int*>(actInfo + 0x00);
          isHostile = (aTeam != offsets::team::Allies);
        } else if (eType == offsets::eType::Player && i < 4) {
          const uintptr_t cInfo = offsets::kCgClientInfo + static_cast<uintptr_t>(i) * 0x58C;
          if (*reinterpret_cast<const int*>(cInfo + 0x00) == 0) alive = false;
          const int pTeam = *reinterpret_cast<const int*>(cInfo + 0x2C);
          isHostile = (pTeam != offsets::team::Allies);
        }
      }

      if (!alive) continue;
      if (eType == offsets::eType::Player) isHostile = false; // In SP/Coop, other human players are allies

      const Vec3 aimPos = GetTargetAimBonePos(i, cfg.aimBone, entOrigin);
      Vec3 headPos = aimPos;
      if (cfg.aimBone != static_cast<int>(AimBone::Head)) {
        if (!GetEntityBonePosValidated(i, g_tags.j_head, entOrigin, headPos)) {
          headPos = entOrigin;
          headPos.z += 58.0f;
        }
      }

      // Visibility trace needed for ESP colors or VisCheck Aimbot
      bool visible = false;
      const bool withinEsp = (distMeters <= cfg.espMaxDist);
      const bool withinAim = (distMeters <= cfg.aimMaxDist);
      if ((withinEsp && cfg.espEnabled) || (withinAim && cfg.aimEnabled)) {
        if (!cfg.aimVisCheck && (!cfg.espEnabled)) {
          visible = true;
        } else {
          visible = CGTraceVisible(vieworg, aimPos, localClientNum, i);
        }
      }

      // Aimbot candidate evaluation (strictly hostile)
      if (cfg.aimEnabled && isHostile && withinAim && (!cfg.aimVisCheck || visible)) {
        Vec2 sPos{};
        if (WorldToScreen(aimPos, sPos)) {
          const float sx = sPos.x - screenCenter.x;
          const float sy = sPos.y - screenCenter.y;
          const float fovDist = std::sqrt(sx * sx + sy * sy);
          if (fovDist <= cfg.aimFov && fovDist < bestScore) {
            bestScore = fovDist;
            bestSilent.valid = true;
            bestSilent.entNum = i;
            bestSilent.visible = visible;
            bestSilent.aimPos3D = aimPos;
            bestSilent.dist = distMeters;
            bestSilent.fovDist = fovDist;
          }
        }
      }

      // ESP actor snapshot
      if (cfg.espEnabled && withinEsp && (!cfg.espEnemiesOnly || isHostile)) {
        ActorSnapshot act;
        act.entNum = i;
        act.eType = eType;
        act.isHostile = isHostile;
        act.visible = visible;
        act.health = hp;
        act.maxHealth = (maxHp > 0) ? maxHp : std::max(100, hp);
        act.distance = distMeters;
        act.origin3D = entOrigin;
        act.head3D = headPos;
        act.aimPos3D = aimPos;
        act.name = CleanActorLabel(clsStr, tgtStr, eType);

        const uint32_t entWeap = *reinterpret_cast<const uint32_t*>(cent + 0x1B0);
        if (cfg.espWeapon && entWeap > 0) act.weaponName = GetWeaponFriendlyName(entWeap);

        if ((cfg.espSkeleton || cfg.espBox) && distMeters <= 220.0f) {
          CollectSkeletonBones(i, entOrigin, act.bones);
        }
        snap.actors.push_back(std::move(act));
      }
      continue;
    }

    if (distMeters > cfg.espMaxDist) continue;

    // Dropped Weapons & Items (ET_ITEM = 3)
    if (eType == offsets::eType::Item && cfg.espWeaponsItems) {
      WorldItemSnapshot wi;
      wi.entNum = i;
      wi.eType = eType;
      wi.category = 0;
      wi.distance = distMeters;
      wi.origin3D = entOrigin;
      const uint32_t itemWeap = *reinterpret_cast<const uint32_t*>(cent + 0x1B0);
      wi.label = GetWeaponFriendlyName(itemWeap);
      if (wi.label.empty() && hasServer) {
        const uintptr_t gent = offsets::kGEntities + static_cast<uintptr_t>(i) * offsets::kGentityStride;
        const char* cls = SLConvertToString(*reinterpret_cast<const uint16_t*>(gent + 0x1A0));
        if (cls) wi.label = cls;
      }
      if (wi.label.empty()) wi.label = "Weapon / Item";
      snap.worldItems.push_back(std::move(wi));
      continue;
    }

    // Incoming Grenades / Missiles (ET_MISSILE = 4)
    if (eType == offsets::eType::Missile && cfg.espGrenades) {
      WorldItemSnapshot wi;
      wi.entNum = i;
      wi.eType = eType;
      wi.category = 1;
      wi.distance = distMeters;
      wi.origin3D = entOrigin;
      const uint32_t mWeap = *reinterpret_cast<const uint32_t*>(cent + 0x1B0);
      std::string wn = GetWeaponFriendlyName(mWeap);
      wi.label = wn.empty() ? "! GRENADE !" : ("! " + wn + " !");
      snap.worldItems.push_back(std::move(wi));
      continue;
    }

    // Vehicles & Mounted MG42 Turrets (ET_VEHICLE = 13, ET_MG42 = 11)
    if ((eType == offsets::eType::Vehicle || eType == offsets::eType::Mg42) && cfg.espVehicles) {
      WorldItemSnapshot wi;
      wi.entNum = i;
      wi.eType = eType;
      wi.category = 2;
      wi.distance = distMeters;
      wi.origin3D = entOrigin;
      wi.label = (eType == offsets::eType::Mg42) ? "MG Turret" : "Vehicle";
      if (hasServer) {
        const uintptr_t gent = offsets::kGEntities + static_cast<uintptr_t>(i) * offsets::kGentityStride;
        const char* tgt = SLConvertToString(*reinterpret_cast<const uint16_t*>(gent + 0x1A8));
        if (tgt && tgt[0] != '\0') wi.label = tgt;
      }
      snap.worldItems.push_back(std::move(wi));
      continue;
    }

    // Script Movers / Interactive Mission Objects (ET_SCRIPTMOVER = 6)
    if (eType == offsets::eType::ScriptMover && cfg.espScriptInteract && hasServer && distMeters <= 80.0f) {
      const uintptr_t gent = offsets::kGEntities + static_cast<uintptr_t>(i) * offsets::kGentityStride;
      const char* tgt = SLConvertToString(*reinterpret_cast<const uint16_t*>(gent + 0x1A8));
      const char* nw = SLConvertToString(*reinterpret_cast<const uint16_t*>(gent + 0x1A4));
      const char* lbl = (tgt && tgt[0]) ? tgt : ((nw && nw[0]) ? nw : nullptr);
      if (lbl) {
        WorldItemSnapshot wi;
        wi.entNum = i;
        wi.eType = eType;
        wi.category = 3;
        wi.distance = distMeters;
        wi.origin3D = entOrigin;
        wi.label = lbl;
        snap.worldItems.push_back(std::move(wi));
      }
    }
  }

  // Mission Objectives (cg.objectives[16], stride 0x490)
  if (cfg.espObjectives) {
    for (int o = 0; o < 16; ++o) {
      const uintptr_t objAddr = offsets::kCgObjectives + static_cast<uintptr_t>(o) * 0x490;
      const int state = *reinterpret_cast<const int*>(objAddr + 0x000);
      if (state == 0 || state == 3) continue; // 0=empty, 3=done/inactive usually (keep active/current!)

      const char* objText = reinterpret_cast<const char*>(objAddr + 0x084);
      std::string text = StripColorCodes(objText);
      if (text.empty()) text = "Objective #" + std::to_string(o + 1);

      for (int p = 0; p < 8; ++p) {
        Vec3 oPos = *reinterpret_cast<const Vec3*>(objAddr + 0x004 + p * 12);
        const int trackEnt = *reinterpret_cast<const int*>(objAddr + 0x064 + p * 4);
        if (trackEnt >= 0 && trackEnt < offsets::kMaxEntities) {
          const uintptr_t cent = offsets::kCgEntities + static_cast<uintptr_t>(trackEnt) * offsets::kCentityStride;
          if ((*reinterpret_cast<const int*>(cent + 0x2D0) & 2) != 0) {
            oPos = *reinterpret_cast<const Vec3*>(cent + 0x24);
          }
        }
        if (std::fabs(oPos.x) < 0.1f && std::fabs(oPos.y) < 0.1f && std::fabs(oPos.z) < 0.1f) continue;

        const float dx = oPos.x - playerOrig.x;
        const float dy = oPos.y - playerOrig.y;
        const float dz = oPos.z - playerOrig.z;
        ObjectiveSnapshot os;
        os.index = o;
        os.state = state;
        os.distance = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.0254f;
        os.origin3D = oPos;
        os.text = text;
        snap.objectives.push_back(std::move(os));
      }
    }
  }

  snap.hasAimTarget = bestSilent.valid;
  snap.aimTargetEnt = bestSilent.entNum;
  snap.aimTargetVisible = bestSilent.visible;
  snap.aimTargetPos3D = bestSilent.aimPos3D;

  {
    std::lock_guard<std::mutex> lock(g_silentMutex);
    g_silentTarget = bestSilent;
  }
  {
    std::lock_guard<std::mutex> lock(g_snapMutex);
    g_frameSnapshot = snap;
  }

  // Run Camera Aimbot & Triggerbot right on main client tick
  RunCameraAimbotAndTrigger(cfg, snap, vieworg, ps);
}

} // namespace waw::sdk
