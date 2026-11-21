#include "ui.hpp"
#include "modern_icons.h"
#include "modern_regular.h"
#include "../features/config.hpp"
#include "../game/sdk.hpp"

#include "menu.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <utility>
#include <vector>
#include <windows.h>

Menu menu;

namespace waw::ui {

namespace {

ImFont* g_fontBrand1 = nullptr;
ImFont* g_fontBrand2 = nullptr;
ImFont* g_fontTitle = nullptr;
ImFont* g_fontBody = nullptr;
ImFont* g_fontTab = nullptr;
ImFont* g_fontSmall = nullptr;
ImFont* g_fontIcons = nullptr;

const char* AimKeyName(int vk) {
  switch (vk) {
    case VK_RBUTTON:  return "Right Mouse";
    case VK_LBUTTON:  return "Left Mouse";
    case VK_LSHIFT:   return "Left Shift";
    case VK_LMENU:    return "Left Alt";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    default:          return "Right Mouse";
  }
}

void DrawAimKeyCombo(ModConfig& cfg) {
  static const struct { int vk; const char* label; } kKeys[] = {
      {VK_RBUTTON,  "Right Mouse"},
      {VK_LBUTTON,  "Left Mouse"},
      {VK_LSHIFT,   "Left Shift"},
      {VK_LMENU,    "Left Alt"},
      {VK_XBUTTON1, "Mouse 4"},
      {VK_XBUTTON2, "Mouse 5"},
  };
  if (ImGui::BeginCombo("Aim Key", AimKeyName(cfg.aimKey))) {
    for (const auto& k : kKeys) {
      bool sel = (cfg.aimKey == k.vk);
      if (ImGui::Selectable(k.label, sel)) cfg.aimKey = k.vk;
      if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
}

void DrawModernTab(const char* icon, const char* label, int tabId, int& activeTab) {
  const ImVec2 pos = ImGui::GetCursorPos();
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImGuiID id = window->GetID(label);

  static std::map<ImGuiID, int> s_alphaBg;
  static std::map<ImGuiID, int> s_alphaBar;
  int& aBg = s_alphaBg[id];
  int& aBar = s_alphaBar[id];

  ImGui::SetCursorPos(pos);
  if (ImGui::InvisibleButton(label, ImVec2(132.0f, 30.0f))) {
    activeTab = tabId;
  }

  const bool selected = (activeTab == tabId);
  const bool hovered = ImGui::IsItemHovered();

  if (selected) {
    aBg = std::min(180, aBg + 12);
    aBar = std::min(255, aBar + 16);
  } else if (hovered) {
    aBg = std::min(120, aBg + 10);
    aBar = std::min(140, aBar + 10);
  } else {
    aBg = std::max(0, aBg - 10);
    aBar = std::max(0, aBar - 12);
  }

  ImDrawList* dl = window->DrawList;
  const ImVec2 absPos(window->Pos.x + pos.x, window->Pos.y + pos.y);
  if (aBar > 0) {
    dl->AddRectFilled(absPos, ImVec2(absPos.x + 3.0f, absPos.y + 28.0f),
                      ImColor(139, 130, 159, aBar), 5.0f, ImDrawFlags_RoundCornersRight);
  }
  if (aBg > 0) {
    dl->AddRectFilled(ImVec2(absPos.x + 7.0f, absPos.y), ImVec2(absPos.x + 134.0f, absPos.y + 28.0f),
                      ImColor(20, 23, 22, aBg), 5.0f);
  }

  const ImColor txtCol = (selected || hovered) ? ImColor(255, 255, 255) : ImColor(132, 136, 139);
  if (g_fontIcons) {
    ImGui::PushFont(g_fontIcons);
    ImGui::SetCursorPos(ImVec2(pos.x + 12.0f, pos.y + 4.0f));
    ImGui::TextColored(txtCol, "%s", icon);
    ImGui::PopFont();
  }
  if (g_fontTab) ImGui::PushFont(g_fontTab);
  ImGui::SetCursorPos(ImVec2(pos.x + 38.0f, pos.y + 6.0f));
  ImGui::TextColored(txtCol, "%s", label);
  if (g_fontTab) ImGui::PopFont();
  ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 34.0f));
}

bool BeginCard(const char* title, const ImVec2& pos, const ImVec2& size) {
  ImGui::SetCursorPos(pos);
  const bool open = ImGui::BeginChild(title, size);
  if (open) {
    ImGui::SetCursorPos(ImVec2(8.0f, 44.0f));
    ImGui::PushItemWidth(172.0f);
  }
  return open;
}

void EndCard() {
  ImGui::PopItemWidth();
  ImGui::EndChild();
}

void RenderModernWidgets(const ModConfig& cfg, const sdk::FrameSnapshot& snap) {
  if (cfg.watermarkVisible) {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(325.0f, 30.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##Modern_Watermark", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    {
      auto* bg = ImGui::GetBackgroundDrawList();
      auto* dl = ImGui::GetWindowDrawList();
      const ImVec2 xy = ImGui::GetWindowPos();
      bg->AddShadowRect(xy, ImVec2(xy.x + 325.0f, xy.y + 30.0f), ImColor(26, 28, 96), 80.0f, ImVec2(0, 0));
      dl->AddRectFilled(xy, ImVec2(xy.x + 325.0f, xy.y + 30.0f), ImColor(12, 15, 14, 242), 5.0f);
      dl->AddRectFilled(xy, ImVec2(xy.x + 3.0f, xy.y + 30.0f), ImColor(139, 130, 159, 255), 4.0f, ImDrawFlags_RoundCornersLeft);

      char buf[128]{};
      if (snap.inGame) {
        std::snprintf(buf, sizeof(buf), "WAW SP  |  %d FPS  |  HP %d  |  %d Pts",
                      static_cast<int>(ImGui::GetIO().Framerate), snap.playerHealth, snap.playerPoints);
      } else {
        std::snprintf(buf, sizeof(buf), "WAW SP  |  %d FPS  |  Main Menu",
                      static_cast<int>(ImGui::GetIO().Framerate));
      }
      if (g_fontTitle) {
        dl->AddText(g_fontTitle, 14.0f, ImVec2(xy.x + 12.0f, xy.y + 8.0f), ImColor(215, 215, 225, 255), buf);
      } else {
        dl->AddText(ImVec2(xy.x + 12.0f, xy.y + 8.0f), ImColor(215, 215, 225, 255), buf);
      }
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }

  if (cfg.keybindsVisible) {
    std::vector<std::pair<const char*, const char*>> binds;
    if (cfg.aimEnabled) {
      const char* modeLbl = (cfg.aimMode == static_cast<int>(AimMode::Silent)) ? "Silent Aim" : "Camera Aim";
      binds.emplace_back(modeLbl, snap.aimKeyDown ? (snap.hasAimTarget ? "[LOCK]" : "[WAIT]") : "[ON]");
    }
    if (cfg.noRecoil) binds.emplace_back("No Recoil", "[ON]");
    if (cfg.noSpread) binds.emplace_back("Zero Spread", "[ON]");
    if (cfg.fastReload) binds.emplace_back("Fast Reload", "[ON]");
    if (cfg.rapidFire) binds.emplace_back("Rapid Fire", "[ON]");
    if (cfg.infAmmo) binds.emplace_back("Inf Ammo", "[ON]");
    if (cfg.oneHitKill) binds.emplace_back("1-Hit Kill", "[ON]");
    if (cfg.espEnabled) binds.emplace_back("Enemy ESP", "[ON]");
    if (cfg.espObjectives) binds.emplace_back("Obj ESP", "[ON]");
    if (cfg.godMode) binds.emplace_back("God Mode", "[ON]");
    if (cfg.noclip) binds.emplace_back("Noclip [V]", "[ON]");

    const float h = 34.0f + 17.0f * std::max<float>(1.0f, static_cast<float>(binds.size()));
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 196.0f, ImGui::GetIO().DisplaySize.y * 0.35f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(180.0f, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##Modern_Keybinds", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    {
      auto* bg = ImGui::GetBackgroundDrawList();
      auto* dl = ImGui::GetWindowDrawList();
      const ImVec2 xy = ImGui::GetWindowPos();
      bg->AddShadowRect(xy, ImVec2(xy.x + 180.0f, xy.y + h), ImColor(26, 28, 96), 70.0f, ImVec2(0, 0));
      dl->AddRectFilled(xy, ImVec2(xy.x + 180.0f, xy.y + h), ImColor(12, 15, 14, 240), 5.0f);
      dl->AddLine(ImVec2(xy.x, xy.y + 24.0f), ImVec2(xy.x + 180.0f, xy.y + 24.0f), ImColor(139, 130, 159, 180), 1.0f);
      ImFont* f = g_fontSmall ? g_fontSmall : ImGui::GetFont();
      const float fs = f ? f->FontSize : 12.0f;
      dl->AddText(f, fs, ImVec2(xy.x + 10.0f, xy.y + 6.0f), ImColor(235, 235, 245, 255), "KEYBINDS [F7]");

      if (binds.empty()) {
        dl->AddText(f, fs, ImVec2(xy.x + 10.0f, xy.y + 30.0f), ImColor(130, 134, 140, 220), "No active mods");
      } else {
        for (size_t i = 0; i < binds.size(); ++i) {
          const float y = xy.y + 30.0f + 17.0f * static_cast<float>(i);
          dl->AddText(f, fs, ImVec2(xy.x + 10.0f, y), ImColor(220, 222, 230, 240), binds[i].first);
          const ImVec2 rsz = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, binds[i].second);
          dl->AddText(f, fs, ImVec2(xy.x + 170.0f - rsz.x, y), ImColor(139, 130, 159, 255), binds[i].second);
        }
      }
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }
}

void DrawCombatTab(ModConfig& cfg) {
  if (BeginCard("AIMBOT & SILENT AIM", ImVec2(148.0f, 48.0f), ImVec2(276.0f, 440.0f))) {
    ImGui::Checkbox("Enable Aimbot", &cfg.aimEnabled);
    static const char* kModes[] = {"Silent Aim (No Camera Snap)", "Camera Aimbot (Smooth)"};
    ImGui::Combo("Mode", &cfg.aimMode, kModes, 2);
    ImGui::Checkbox("Visibility Check (CG_Trace)", &cfg.aimVisCheck);
    ImGui::Checkbox("Always Active (No Key Hold)", &cfg.aimAlwaysOn);
    if (!cfg.aimAlwaysOn) DrawAimKeyCombo(cfg);

    static const char* kBones[] = {"Head (j_head)", "Neck (j_neck)", "Chest (j_spineupper)", "Pelvis"};
    ImGui::Combo("Target Bone", &cfg.aimBone, kBones, 4);
    ImGui::SliderFloat("Aim FOV", &cfg.aimFov, 20.0f, 550.0f, "%.0f px");
    if (cfg.aimMode == static_cast<int>(AimMode::Camera)) {
      ImGui::SliderFloat("Smoothness", &cfg.aimSmooth, 1.0f, 15.0f, "%.1f");
    }
    ImGui::SliderFloat("Max Range", &cfg.aimMaxDist, 25.0f, 400.0f, "%.0f m");
  }
  EndCard();

  if (BeginCard("TARGETING & TRIGGERBOT", ImVec2(434.0f, 48.0f), ImVec2(274.0f, 440.0f))) {
    ImGui::Checkbox("Draw FOV Circle", &cfg.drawFovCircle);
    ImGui::Checkbox("Target Lock Line", &cfg.drawTargetLine);
    ImGui::Separator();
    ImGui::Checkbox("Triggerbot (Auto Fire)", &cfg.triggerbot);
    if (cfg.triggerbot) {
      ImGui::SliderFloat("Trigger Radius", &cfg.triggerFov, 4.0f, 60.0f, "%.0f px");
    }
    ImGui::Separator();
    ImGui::Checkbox("Custom Crosshair", &cfg.customCrosshair);
    if (cfg.customCrosshair) {
      ImGui::SliderFloat("Crosshair Size", &cfg.crosshairSize, 3.0f, 18.0f, "%.1f");
    }
    ImGui::Separator();
    ImGui::ColorEdit4("FOV Circle Color", cfg.colFovCircle, ImGuiColorEditFlags_NoInputs);
    ImGui::Spacing();
    ImGui::TextDisabled("Silent Aim hooks G_GetWeaponParam\nso shots track visible targets\nwithout camera shake.");
  }
  EndCard();
}

void DrawWeaponTab(ModConfig& cfg) {
  if (BeginCard("WEAPON MODS", ImVec2(148.0f, 48.0f), ImVec2(276.0f, 440.0f))) {
    ImGui::Checkbox("No Recoil", &cfg.noRecoil);
    ImGui::Checkbox("Zero Spread (Hip + ADS)", &cfg.noSpread);
    ImGui::Checkbox("Fast Reload", &cfg.fastReload);
    if (cfg.fastReload) {
      ImGui::SliderFloat("Reload Time Scale", &cfg.reloadSpeedScale, 0.05f, 1.0f, "%.2fx");
    }
    ImGui::Checkbox("Rapid Fire", &cfg.rapidFire);
    if (cfg.rapidFire) {
      ImGui::SliderFloat("Fire Rate Mult", &cfg.rapidFireMult, 1.0f, 12.0f, "%.1fx");
    }
    ImGui::Checkbox("Full Auto (Any Gun)", &cfg.fullAuto);
    ImGui::Checkbox("Instant Weapon Swap", &cfg.instantSwap);
    ImGui::Separator();
    ImGui::Checkbox("Infinite Ammo (Clip)", &cfg.infAmmo);
    ImGui::Checkbox("Infinite Reserve Ammo", &cfg.infAmmoReserve);
    ImGui::Checkbox("One-Hit Kill (Damage Override)", &cfg.oneHitKill);
    if (cfg.oneHitKill) {
      ImGui::SliderInt("Bullet Damage", &cfg.customWeaponDmg, 500, 100000);
    }
  }
  EndCard();

  if (BeginCard("ARSENAL & AMMO ACTIONS", ImVec2(434.0f, 48.0f), ImVec2(274.0f, 440.0f))) {
    if (ImGui::Button("Refill Current Ammo", ImVec2(248.0f, 26.0f))) {
      sdk::QueueAction(sdk::ActionType::RefillAmmo);
    }
    if (ImGui::Button("Give All Weapons", ImVec2(248.0f, 26.0f))) {
      sdk::QueueAction(sdk::ActionType::ToggleCheats);
      sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "give all");
    }
    ImGui::Separator();
    static const struct { const char* name; const char* cmd; } kWeapons[] = {
        {"Ray Gun (Zombies)", "give ray_gun"},
        {"Wunderwaffe DG-2", "give tesla_gun"},
        {"PPSh-41", "give ppsh"},
        {"MG42", "give mg42"},
        {"PTRS-41 Anti-Tank", "give ptrs41"},
        {"Thompson SMG", "give thompson"},
        {"StG-44", "give stg44"},
        {"M2 Flamethrower", "give m2_flamethrower"},
        {"Panzerschreck", "give panzerschrek"},
    };
    for (const auto& w : kWeapons) {
      if (ImGui::Button(w.name, ImVec2(248.0f, 23.0f))) {
        sdk::QueueAction(sdk::ActionType::ToggleCheats);
        sdk::QueueAction(sdk::ActionType::ExecCmd, 0, w.cmd);
      }
    }
  }
  EndCard();
}

void DrawVisualsTab(ModConfig& cfg) {
  if (BeginCard("ENEMY ESP", ImVec2(148.0f, 48.0f), ImVec2(276.0f, 440.0f))) {
    ImGui::Checkbox("Enable Enemy ESP", &cfg.espEnabled);
    ImGui::Checkbox("Enemies Only", &cfg.espEnemiesOnly);
    ImGui::Checkbox("2D Box", &cfg.espBox);
    if (cfg.espBox) {
      static const char* kBoxStyles[] = {"Corner Box", "Full 2D Box"};
      ImGui::Combo("Box Style", &cfg.espBoxStyle, kBoxStyles, 2);
    }
    ImGui::Checkbox("Health Bar", &cfg.espHealthBar);
    ImGui::Checkbox("HP Numeric Text", &cfg.espHealthText);
    ImGui::Checkbox("Snaplines", &cfg.espSnapline);
    if (cfg.espSnapline) {
      static const char* kSnaps[] = {"Bottom", "Center", "Top"};
      ImGui::Combo("Snap Origin", &cfg.espSnaplineOrigin, kSnaps, 3);
    }
    ImGui::Checkbox("Enemy Name", &cfg.espName);
    ImGui::Checkbox("Held Weapon", &cfg.espWeapon);
    ImGui::Checkbox("Distance (m)", &cfg.espDistance);
    ImGui::Checkbox("Bone Skeleton", &cfg.espSkeleton);
    ImGui::Checkbox("360 Offscreen Alert", &cfg.espOffscreenAlert);
    if (cfg.espOffscreenAlert) {
      ImGui::SliderFloat("Alert Radius", &cfg.offscreenRadius, 90.0f, 340.0f, "%.0f px");
    }
    ImGui::SliderFloat("Max ESP Range", &cfg.espMaxDist, 30.0f, 500.0f, "%.0f m");
  }
  EndCard();

  if (BeginCard("WORLD, ITEMS & COLORS", ImVec2(434.0f, 48.0f), ImVec2(274.0f, 440.0f))) {
    ImGui::Checkbox("Mission Objectives ESP", &cfg.espObjectives);
    ImGui::Checkbox("Weapons & Pickups ESP", &cfg.espWeaponsItems);
    ImGui::Checkbox("Grenade / Missile Warning", &cfg.espGrenades);
    ImGui::Checkbox("Vehicles & MG Turrets", &cfg.espVehicles);
    ImGui::Checkbox("Mission Script Objects", &cfg.espScriptInteract);
    ImGui::Separator();
    ImGui::ColorEdit4("Visible Enemy", cfg.colVisible, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit4("Hidden Enemy", cfg.colHidden, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit4("Ally / Friendly", cfg.colAlly, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit4("Skeleton Bones", cfg.colSkeleton, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit4("Mission Objective", cfg.colObjective, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit4("Weapon / Pickup", cfg.colItem, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit4("Grenade Alert", cfg.colGrenade, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit4("Vehicle / Turret", cfg.colVehicle, ImGuiColorEditFlags_NoInputs);
  }
  EndCard();
}

void DrawPlayerTab(ModConfig& cfg, const sdk::FrameSnapshot& snap) {
  if (BeginCard("SURVIVAL & MOVEMENT", ImVec2(148.0f, 48.0f), ImVec2(276.0f, 440.0f))) {
    ImGui::Checkbox("God Mode", &cfg.godMode);
    ImGui::Checkbox("Demi-God (No Dmg Taken)", &cfg.demiGod);
    ImGui::Checkbox("No Target (AI Ignores You)", &cfg.noTarget);
    ImGui::Checkbox("Noclip", &cfg.noclip);
    ImGui::Checkbox("Noclip Hotkey [V]", &cfg.noclipKeyToggle);
    ImGui::Checkbox("UFO Mode", &cfg.ufo);
    ImGui::Checkbox("Infinite Sprint", &cfg.infSprint);
    ImGui::Checkbox("No Fall Damage", &cfg.noFallDmg);
    ImGui::Checkbox("Third Person", &cfg.thirdPerson);
    ImGui::Separator();
    ImGui::Checkbox("Custom FOV", &cfg.overrideFov);
    if (cfg.overrideFov) ImGui::SliderFloat("FOV", &cfg.customFov, 50.0f, 125.0f, "%.0f");
    ImGui::Checkbox("Custom Move Speed", &cfg.overrideSpeed);
    if (cfg.overrideSpeed) ImGui::SliderInt("g_speed", &cfg.customSpeed, 80, 700);
    ImGui::Checkbox("Custom Max Health", &cfg.overrideMaxHealth);
    if (cfg.overrideMaxHealth) ImGui::SliderInt("Max HP", &cfg.customMaxHealth, 100, 5000);
    ImGui::Checkbox("Custom Timescale", &cfg.overrideTimescale);
    if (cfg.overrideTimescale) ImGui::SliderFloat("Timescale", &cfg.customTimescale, 0.1f, 4.0f, "%.2fx");
  }
  EndCard();

  if (BeginCard("ZOMBIES / SP & CONSOLE", ImVec2(434.0f, 48.0f), ImVec2(274.0f, 440.0f))) {
    ImGui::Text("Score/Points: %d | Kills: %d", snap.playerPoints, snap.playerKills);
    ImGui::SliderInt("Points Amount", &cfg.givePointsAmount, 1000, 250000);
    if (ImGui::Button("Add Points", ImVec2(248.0f, 25.0f))) {
      sdk::QueueAction(sdk::ActionType::AddPoints, cfg.givePointsAmount);
    }
    ImGui::Checkbox("Lock Points Value", &cfg.lockPoints);
    if (cfg.lockPoints) {
      ImGui::SliderInt("Locked Pts", &cfg.lockedPoints, 10000, 999999);
    }
    ImGui::Separator();
    if (ImGui::Button("Weaken All Hostiles (1 HP)", ImVec2(248.0f, 25.0f))) {
      sdk::QueueAction(sdk::ActionType::WeakAllEnemies);
    }
    if (ImGui::Button("Enable sv_cheats 1", ImVec2(248.0f, 25.0f))) {
      sdk::QueueAction(sdk::ActionType::ToggleCheats);
    }
    ImGui::Separator();
    ImGui::InputText("##Cmd", cfg.customCmd, sizeof(cfg.customCmd));
    if (ImGui::Button("Run Console Command", ImVec2(248.0f, 25.0f))) {
      sdk::QueueAction(sdk::ActionType::ExecCmd, 0, cfg.customCmd);
    }
    ImGui::Spacing();
    if (ImGui::Button("Low Gravity (300)", ImVec2(120.0f, 23.0f))) {
      sdk::QueueAction(sdk::ActionType::ToggleCheats);
      sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "g_gravity 300");
    }
    ImGui::SameLine();
    if (ImGui::Button("Normal Gravity", ImVec2(120.0f, 23.0f))) {
      sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "g_gravity 800");
    }
    if (ImGui::Button("Toggle ai_disableSpawn", ImVec2(248.0f, 23.0f))) {
      sdk::QueueAction(sdk::ActionType::ToggleCheats);
      sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "toggle ai_disableSpawn");
    }
  }
  EndCard();
}

void DrawSettingsTab(ModConfig& cfg, const sdk::FrameSnapshot& snap) {
  if (BeginCard("HUD & INTERFACE", ImVec2(148.0f, 48.0f), ImVec2(276.0f, 440.0f))) {
    ImGui::Checkbox("Show Watermark", &cfg.watermarkVisible);
    ImGui::Checkbox("Show Keybinds HUD [F7]", &cfg.keybindsVisible);
    ImGui::Separator();
    ImGui::TextDisabled("LIVE GAME STATE");
    ImGui::BulletText("In Game: %s", snap.inGame ? "YES" : "NO (Menu)");
    ImGui::BulletText("Host Access: %s", snap.hasServerHost ? "SP/Host Full" : "Client Only");
    ImGui::BulletText("Health: %d / %d", snap.playerHealth, snap.playerMaxHealth);
    ImGui::BulletText("Weapon: %s", snap.currentWeaponName.empty() ? "-" : snap.currentWeaponName.c_str());
    ImGui::BulletText("Ammo: %d / %d", snap.weaponClip, snap.weaponReserve);
    ImGui::BulletText("Active Actors: %d", static_cast<int>(snap.actors.size()));
    ImGui::BulletText("Objectives Tracked: %d", static_cast<int>(snap.objectives.size()));
  }
  EndCard();

  if (BeginCard("CONTROLS & UNLOAD", ImVec2(434.0f, 48.0f), ImVec2(274.0f, 440.0f))) {
    ImGui::TextColored(ImVec4(0.545f, 0.510f, 0.624f, 1.0f), "CoD: World at War SP Suite");
    ImGui::TextDisabled("ModernUI Theme (DX9 Adapted)");
    ImGui::Separator();
    ImGui::BulletText("INSERT / F1 : Toggle Menu");
    ImGui::BulletText("F7          : Toggle Keybinds");
    ImGui::BulletText("V           : Quick Noclip");
    ImGui::BulletText("END         : Clean Unload");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Unload Mod DLL Safely", ImVec2(248.0f, 30.0f))) {
      cfg.menuOpen = false;
      cfg.unloading = true;
    }
  }
  EndCard();
}

} // namespace

void InitFonts() {
  ImGuiIO& io = ImGui::GetIO();
  ImFontConfig cfg{};
  cfg.FontDataOwnedByAtlas = false;

  g_fontBody = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 14.0f, &cfg);
  g_fontTitle = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 15.0f, &cfg);
  g_fontTab = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 16.0f, &cfg);
  g_fontBrand1 = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 18.0f, &cfg);
  g_fontBrand2 = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 19.0f, &cfg);
  g_fontSmall = io.Fonts->AddFontFromMemoryTTF(lexend_bold, sizeof(lexend_bold), 12.0f, &cfg);
  g_fontIcons = io.Fonts->AddFontFromMemoryTTF(IconFont, sizeof(IconFont), 19.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

  if (!g_fontBody) {
    g_fontBody = io.Fonts->AddFontDefault();
    g_fontTitle = g_fontTab = g_fontBrand1 = g_fontBrand2 = g_fontSmall = g_fontBody;
  }
  menu.nameTable = g_fontTitle;
}

void ApplyStyle() {
  ImGui::StyleColorsDark();
  ImGuiStyle& st = ImGui::GetStyle();
  st.WindowRounding = 5.0f;
  st.ChildRounding = 6.0f;
  st.FrameRounding = 4.0f;
  st.GrabRounding = 4.0f;
  st.PopupRounding = 4.0f;
  st.WindowPadding = ImVec2(0.0f, 0.0f);
  st.FramePadding = ImVec2(6.0f, 4.0f);
  st.ItemSpacing = ImVec2(8.0f, 6.0f);
  st.Colors[ImGuiCol_WindowBg] = ImVec4(0.067f, 0.075f, 0.075f, 1.0f);
  st.Colors[ImGuiCol_FrameBg] = ImVec4(0.085f, 0.098f, 0.094f, 1.0f);
  st.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.125f, 0.140f, 0.138f, 1.0f);
  st.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.23f, 1.0f);
  st.Colors[ImGuiCol_CheckMark] = ImVec4(0.545f, 0.510f, 0.624f, 1.0f);
  st.Colors[ImGuiCol_SliderGrab] = ImVec4(0.545f, 0.510f, 0.624f, 1.0f);
  st.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.68f, 0.64f, 0.78f, 1.0f);
  st.Colors[ImGuiCol_Button] = ImVec4(0.098f, 0.114f, 0.110f, 1.0f);
  st.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.400f, 0.396f, 0.525f, 0.80f);
  st.Colors[ImGuiCol_ButtonActive] = ImVec4(0.545f, 0.510f, 0.624f, 1.0f);
  st.Colors[ImGuiCol_Header] = ImVec4(0.32f, 0.31f, 0.44f, 0.85f);
  st.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.44f, 0.60f, 0.90f);
  st.Colors[ImGuiCol_PopupBg] = ImVec4(0.055f, 0.065f, 0.063f, 0.98f);
}

void RenderMenu() {
  ModConfig& cfg = Config();
  const sdk::FrameSnapshot snap = sdk::CopyFrameSnapshot();

  RenderModernWidgets(cfg, snap);
  if (!cfg.menuOpen) return;

  const ImVec2 winSz(720.0f, 500.0f);
  ImGui::SetNextWindowSize(winSz, ImGuiCond_Once);
  ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_FirstUseEver);
  ImGui::Begin("##WaW_ModernUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
  {
    auto* bgList = ImGui::GetBackgroundDrawList();
    auto* dl = ImGui::GetWindowDrawList();
    const ImVec2 xy = ImGui::GetWindowPos();

    bgList->AddShadowRect(xy, ImVec2(xy.x + winSz.x, xy.y + winSz.y), ImColor(26, 28, 96), 100.0f, ImVec2(0, 0));
    dl->AddRectFilled(xy, ImVec2(xy.x + winSz.x, xy.y + winSz.y), ImColor(17, 19, 19, 255), 5.0f, ImDrawFlags_RoundCornersAll);
    dl->AddRectFilled(xy, ImVec2(xy.x + 140.0f, xy.y + winSz.y), ImColor(12, 15, 14, 255), 5.0f, ImDrawFlags_RoundCornersLeft);
    dl->AddLine(ImVec2(xy.x + 140.0f, xy.y), ImVec2(xy.x + 140.0f, xy.y + winSz.y), ImColor(26, 29, 28, 255), 1.5f);
    dl->AddLine(ImVec2(xy.x + 140.0f, xy.y + 38.0f), ImVec2(xy.x + winSz.x, xy.y + 38.0f), ImColor(26, 29, 28, 255), 1.5f);

    // Header brand text
    dl->AddText(g_fontBrand1, 18.0f, ImVec2(xy.x + 154.0f, xy.y + 11.0f), ImColor(255, 255, 255, 255), "COD WAW");
    dl->AddText(g_fontBrand2, 19.0f, ImVec2(xy.x + 244.0f, xy.y + 10.0f), ImColor(102, 101, 134, 255), "MODERN");

    // Sidebar section headers + tabs
    ImFont* sm = g_fontSmall ? g_fontSmall : ImGui::GetFont();
    dl->AddLine(ImVec2(xy.x, xy.y + 15.0f), ImVec2(xy.x + 10.0f, xy.y + 15.0f), ImColor(26, 29, 28, 255), 1.5f);
    dl->AddText(sm, 12.0f, ImVec2(xy.x + 14.0f, xy.y + 9.0f), ImColor(133, 133, 133, 255), "COMBAT");
    dl->AddLine(ImVec2(xy.x + 68.0f, xy.y + 15.0f), ImVec2(xy.x + 136.0f, xy.y + 15.0f), ImColor(26, 29, 28, 255), 1.5f);

    ImGui::SetCursorPos(ImVec2(0.0f, 30.0f));
    ImGui::BeginGroup();
    DrawModernTab("1", "Aimbot", 0, cfg.activeTab);
    DrawModernTab("3", "Weapons", 1, cfg.activeTab);
    ImGui::EndGroup();

    dl->AddLine(ImVec2(xy.x, xy.y + 110.0f), ImVec2(xy.x + 10.0f, xy.y + 110.0f), ImColor(26, 29, 28, 255), 1.5f);
    dl->AddText(sm, 12.0f, ImVec2(xy.x + 14.0f, xy.y + 104.0f), ImColor(133, 133, 133, 255), "VISUALS");
    dl->AddLine(ImVec2(xy.x + 66.0f, xy.y + 110.0f), ImVec2(xy.x + 136.0f, xy.y + 110.0f), ImColor(26, 29, 28, 255), 1.5f);

    ImGui::SetCursorPos(ImVec2(0.0f, 125.0f));
    ImGui::BeginGroup();
    DrawModernTab("2", "ESP", 2, cfg.activeTab);
    DrawModernTab("4", "Player", 3, cfg.activeTab);
    ImGui::EndGroup();

    dl->AddLine(ImVec2(xy.x, xy.y + 205.0f), ImVec2(xy.x + 10.0f, xy.y + 205.0f), ImColor(26, 29, 28, 255), 1.5f);
    dl->AddText(sm, 12.0f, ImVec2(xy.x + 14.0f, xy.y + 199.0f), ImColor(133, 133, 133, 255), "SYSTEM");
    dl->AddLine(ImVec2(xy.x + 64.0f, xy.y + 205.0f), ImVec2(xy.x + 136.0f, xy.y + 205.0f), ImColor(26, 29, 28, 255), 1.5f);

    ImGui::SetCursorPos(ImVec2(0.0f, 220.0f));
    ImGui::BeginGroup();
    DrawModernTab("6", "Settings", 4, cfg.activeTab);
    ImGui::EndGroup();

    if (g_fontBody) ImGui::PushFont(g_fontBody);
    switch (cfg.activeTab) {
      case 0: DrawCombatTab(cfg); break;
      case 1: DrawWeaponTab(cfg); break;
      case 2: DrawVisualsTab(cfg); break;
      case 3: DrawPlayerTab(cfg, snap); break;
      case 4: DrawSettingsTab(cfg, snap); break;
      default: DrawCombatTab(cfg); break;
    }
    if (g_fontBody) ImGui::PopFont();
  }
  ImGui::End();
}

} // namespace waw::ui
