#pragma once

namespace waw {

enum class AimMode : int {
  Silent = 0,
  Camera = 1,
};

enum class AimBone : int {
  Head = 0,
  Neck = 1,
  Chest = 2,
  Pelvis = 3,
};

enum class EspBoxStyle : int {
  Corner = 0,
  Full = 1,
};

enum class SnaplineOrigin : int {
  Bottom = 0,
  Center = 1,
  Top = 2,
};

struct ModConfig {
  // Menu / UI
  bool menuOpen = true;
  bool keybindsVisible = true;
  bool watermarkVisible = true;
  int activeTab = 0;
  bool unloading = false;

  // Combat / Aimbot
  bool aimEnabled = true;
  int aimMode = static_cast<int>(AimMode::Silent);
  bool aimVisCheck = true;
  bool aimAlwaysOn = false;
  int aimKey = 0x02; // VK_RBUTTON
  int aimBone = static_cast<int>(AimBone::Head);
  float aimFov = 180.0f;
  float aimSmooth = 3.5f;
  float aimMaxDist = 250.0f;
  bool drawFovCircle = true;
  bool drawTargetLine = true;
  bool triggerbot = false;
  float triggerFov = 18.0f;

  // Weapon Mods
  bool noRecoil = true;
  bool noSpread = true;
  bool fastReload = true;
  float reloadSpeedScale = 0.15f; // multiplier (lower = faster)
  bool rapidFire = false;
  float rapidFireMult = 4.0f;     // divisor on fireTime (higher = faster)
  bool fullAuto = false;
  bool instantSwap = false;
  bool infAmmo = true;
  bool infAmmoReserve = true;
  bool oneHitKill = false;
  int customWeaponDmg = 50000;

  // Enemy ESP
  bool espEnabled = true;
  bool espEnemiesOnly = true;
  bool espBox = true;
  int espBoxStyle = static_cast<int>(EspBoxStyle::Corner);
  bool espHealthBar = true;
  bool espHealthText = true;
  bool espSnapline = true;
  int espSnaplineOrigin = static_cast<int>(SnaplineOrigin::Bottom);
  bool espName = true;
  bool espWeapon = true;
  bool espDistance = true;
  bool espSkeleton = true;
  bool espOffscreenAlert = true;
  float offscreenRadius = 190.0f;
  float espMaxDist = 250.0f;

  // World / Mission / Items ESP
  bool espObjectives = true;
  bool espWeaponsItems = true;
  bool espGrenades = true;
  bool espVehicles = true;
  bool espScriptInteract = false;

  // Crosshair
  bool customCrosshair = false;
  float crosshairSize = 7.0f;

  // ESP Colors (RGBA 0..1)
  float colVisible[4] = {0.22f, 0.94f, 0.45f, 1.0f};
  float colHidden[4] = {0.96f, 0.25f, 0.25f, 1.0f};
  float colAlly[4] = {0.25f, 0.65f, 1.0f, 0.85f};
  float colSkeleton[4] = {1.0f, 1.0f, 1.0f, 0.88f};
  float colObjective[4] = {1.0f, 0.82f, 0.15f, 1.0f};
  float colItem[4] = {0.35f, 0.90f, 0.98f, 0.95f};
  float colGrenade[4] = {1.0f, 0.35f, 0.12f, 1.0f};
  float colVehicle[4] = {0.82f, 0.48f, 1.0f, 0.95f};
  float colFovCircle[4] = {0.72f, 0.68f, 0.98f, 0.55f};

  // Player & World Mods
  bool godMode = false;
  bool demiGod = false;
  bool noTarget = false;
  bool noclip = false;
  bool ufo = false;
  bool noclipKeyToggle = true; // 'V' or 'N'? We'll support V quick toggle if enabled
  bool infSprint = true;
  bool noFallDmg = true;
  bool thirdPerson = false;

  bool overrideFov = false;
  float customFov = 90.0f;

  bool overrideSpeed = false;
  int customSpeed = 280;

  bool overrideMaxHealth = false;
  int customMaxHealth = 500;

  bool overrideTimescale = false;
  float customTimescale = 1.0f;

  // Zombies / SP Points & Utilities
  bool lockPoints = false;
  int lockedPoints = 100000;
  int givePointsAmount = 50000;
  char customCmd[128] = "give ammo";
};

inline ModConfig& Config() {
  static ModConfig cfg;
  return cfg;
}

} // namespace waw
