#include "ui.hpp"
#include "museo_font.hpp"
#include "../features/config.hpp"
#include "../game/sdk.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>
#include <windows.h>

namespace waw::ui {

namespace {

ImFont* g_fontMain = nullptr;
ImFont* g_fontTitle = nullptr;

const char* AimKeyLabel(int vk) {
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
  static const struct { int vk; const char* name; } kKeys[] = {
      {VK_RBUTTON,  "Right Mouse"},
      {VK_LBUTTON,  "Left Mouse"},
      {VK_LSHIFT,   "Left Shift"},
      {VK_LMENU,    "Left Alt"},
      {VK_XBUTTON1, "Mouse 4"},
      {VK_XBUTTON2, "Mouse 5"},
  };
  if (ImGui::BeginCombo("Aim Key", AimKeyLabel(cfg.aimKey))) {
    for (const auto& k : kKeys) {
      bool sel = (cfg.aimKey == k.vk);
      if (ImGui::Selectable(k.name, sel)) cfg.aimKey = k.vk;
      if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
}

void RenderKeybindsPanel(const ModConfig& cfg, const sdk::FrameSnapshot& snap) {
  if (!g_fontMain) return;

  static float openAlpha = 1.0f;
  openAlpha = ImClamp(openAlpha + (2.0f * ImGui::GetIO().DeltaTime * (cfg.keybindsVisible ? 0.8f : -1.5f)), 0.0f, 1.0f);
  if (openAlpha <= 0.01f) return;

  std::vector<std::pair<const char*, const char*>> binds;
  if (cfg.aimEnabled) {
    const char* lbl = (cfg.aimMode == static_cast<int>(AimMode::Silent)) ? "Silent Aim" : "Aimbot";
    binds.emplace_back(lbl, snap.aimKeyDown ? (snap.hasAimTarget ? "[LOCK]" : "[WAIT]") : "[ON]");
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

  const float spacing = g_fontMain->FontSize + 3.0f;
  const float h = 26.0f + spacing * (binds.empty() ? 1.0f : static_cast<float>(binds.size()));

  ImGui::SetNextWindowSize(ImVec2(175.0f, h));
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 190.0f, ImGui::GetIO().DisplaySize.y * 0.40f), ImGuiCond_FirstUseEver);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, openAlpha);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("##OldUI_Keybinds", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
  {
    auto* d = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos();
    const ImVec2 s = ImGui::GetWindowSize();

    d->AddRectFilled(ImVec2(p.x, p.y + 18.0f), ImVec2(p.x + s.x, p.y + s.y), ImColor(25, 25, 25, static_cast<int>(240 * openAlpha)));
    d->AddRectFilled(p, ImVec2(p.x + s.x, p.y + 18.0f), ImColor(33, 33, 33, static_cast<int>(245 * openAlpha)));
    d->AddLine(ImVec2(p.x, p.y + 18.0f), ImVec2(p.x + s.x, p.y + 18.0f), ImColor(89, 113, 162, static_cast<int>(255 * openAlpha)));

    const ImVec2 hdrSz = g_fontMain->CalcTextSizeA(g_fontMain->FontSize, FLT_MAX, 0.0f, "Keybinds [F7]");
    d->AddText(g_fontMain, g_fontMain->FontSize, ImVec2(p.x + (s.x - hdrSz.x) * 0.5f, p.y + (17.0f - hdrSz.y) * 0.5f),
               ImColor(235, 238, 245, static_cast<int>(220 * openAlpha)), "Keybinds [F7]");

    if (binds.empty()) {
      d->AddText(g_fontMain, g_fontMain->FontSize, ImVec2(p.x + 8.0f, p.y + 22.0f),
                 ImColor(150, 155, 165, static_cast<int>(170 * openAlpha)), "No active cheats");
    } else {
      for (size_t i = 0; i < binds.size(); ++i) {
        const float y = p.y + 22.0f + spacing * static_cast<float>(i);
        d->AddText(g_fontMain, g_fontMain->FontSize, ImVec2(p.x + 8.0f, y),
                   ImColor(235, 238, 245, static_cast<int>(200 * openAlpha)), binds[i].first);
        const ImVec2 rSz = g_fontMain->CalcTextSizeA(g_fontMain->FontSize, FLT_MAX, 0.0f, binds[i].second);
        d->AddText(g_fontMain, g_fontMain->FontSize, ImVec2(p.x + s.x - 8.0f - rSz.x, y),
                   ImColor(112, 146, 190, static_cast<int>(235 * openAlpha)), binds[i].second);
      }
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

} // namespace

void InitFonts() {
  ImGuiIO& io = ImGui::GetIO();
  ImFontConfig cfg{};
  cfg.FontDataOwnedByAtlas = false;
  cfg.OversampleH = 2;
  cfg.OversampleV = 1;
  cfg.PixelSnapH = true;

  g_fontMain = io.Fonts->AddFontFromMemoryTTF(
      const_cast<unsigned char*>(g_MuseoSansData),
      static_cast<int>(sizeof(g_MuseoSansData)),
      13.5f,
      &cfg,
      io.Fonts->GetGlyphRangesDefault());
  g_fontTitle = io.Fonts->AddFontFromMemoryTTF(
      const_cast<unsigned char*>(g_MuseoSansData),
      static_cast<int>(sizeof(g_MuseoSansData)),
      16.0f,
      &cfg,
      io.Fonts->GetGlyphRangesDefault());

  if (!g_fontMain) {
    g_fontMain = io.Fonts->AddFontDefault();
    g_fontTitle = g_fontMain;
  }
}

void ApplyStyle() {
  ImGui::StyleColorsDark();
  ImGuiStyle& st = ImGui::GetStyle();
  st.WindowRounding = 0.0f;
  st.ChildRounding = 0.0f;
  st.FrameRounding = 2.0f;
  st.WindowBorderSize = 0.0f;
  st.Colors[ImGuiCol_Button] = ImColor(38, 42, 48);
  st.Colors[ImGuiCol_ButtonHovered] = ImColor(89, 113, 162);
  st.Colors[ImGuiCol_ButtonActive] = ImColor(112, 146, 190);
  st.Colors[ImGuiCol_FrameBg] = ImColor(24, 26, 28);
  st.Colors[ImGuiCol_FrameBgHovered] = ImColor(35, 38, 42);
  st.Colors[ImGuiCol_PopupBg] = ImColor(24, 26, 28);
  st.Colors[ImGuiCol_Header] = ImColor(60, 82, 120);
  st.Colors[ImGuiCol_HeaderHovered] = ImColor(89, 113, 162);
}

void RenderMenu() {
  ModConfig& cfg = Config();
  const sdk::FrameSnapshot snap = sdk::CopyFrameSnapshot();

  RenderKeybindsPanel(cfg, snap);
  if (!cfg.menuOpen) return;

  const ImVec2 winSize(650.0f, 475.0f);
  ImGui::SetNextWindowSize(winSize, ImGuiCond_Once);
  ImGui::Begin("##WaW_OldUI_GUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
  {
    const ImVec2 pad = ImGui::GetStyle().WindowPadding;
    const ImVec2 s(ImGui::GetWindowSize().x - pad.x * 2.0f, ImGui::GetWindowSize().y - pad.y * 2.0f);
    const ImVec2 p(ImGui::GetWindowPos().x + pad.x, ImGui::GetWindowPos().y + pad.y);
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), ImColor(33, 33, 33));
    draw->AddRectFilled(ImVec2(p.x, p.y + 28.0f), ImVec2(p.x + s.x, p.y + s.y - 25.0f), ImColor(25, 25, 25));
    draw->AddLine(ImVec2(p.x, p.y + 28.0f), ImVec2(p.x + s.x, p.y + 28.0f), ImColor(89, 113, 162));
    draw->AddLine(ImVec2(p.x, p.y + s.y - 25.0f), ImVec2(p.x + s.x, p.y + s.y - 25.0f), ImColor(89, 113, 162));
    draw->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), ImColor(48, 52, 60));

    if (g_fontMain) ImGui::PushFont(g_fontMain);
    ImGui::SetCursorPos(ImVec2(20.0f, 11.0f));
    ImGui::BeginGroup();
    if (ImGui::tab("Combat", cfg.activeTab == 0)) cfg.activeTab = 0;
    ImGui::SameLine(0.0f, 13.0f);
    if (ImGui::tab("Weapons", cfg.activeTab == 1)) cfg.activeTab = 1;
    ImGui::SameLine(0.0f, 13.0f);
    if (ImGui::tab("Visuals", cfg.activeTab == 2)) cfg.activeTab = 2;
    ImGui::SameLine(0.0f, 13.0f);
    if (ImGui::tab("Player", cfg.activeTab == 3)) cfg.activeTab = 3;
    ImGui::SameLine(0.0f, 13.0f);
    if (ImGui::tab("World", cfg.activeTab == 4)) cfg.activeTab = 4;
    ImGui::SameLine(0.0f, 13.0f);
    if (ImGui::tab("Info", cfg.activeTab == 5)) cfg.activeTab = 5;
    ImGui::EndGroup();

    const char* titleTxt = "CoD:WaW SP [OldUI]";
    const ImVec2 tsz = g_fontMain ? g_fontMain->CalcTextSizeA(g_fontMain->FontSize, FLT_MAX, 0.0f, titleTxt) : ImVec2(110.0f, 13.5f);
    draw->AddText(g_fontMain, g_fontMain ? g_fontMain->FontSize : 13.5f,
                  ImVec2(p.x + s.x - tsz.x - 12.0f, p.y + 7.0f), ImColor(135, 168, 232, 240), titleTxt);

    if (cfg.activeTab == 0) {
      ImGui::SetCursorPos(ImVec2(15.0f, 40.0f));
      ImGui::MenuChild("Aimbot & Silent Aim", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Enable Aimbot", &cfg.aimEnabled);
        static const char* kModes[] = {"Silent Aim (No Snap)", "Camera Aimbot"};
        ImGui::Combo("Aim Mode", &cfg.aimMode, kModes, 2);
        ImGui::Checkbox("Visibility Check (CG_Trace)", &cfg.aimVisCheck);
        ImGui::Checkbox("Always Active (Auto Silent)", &cfg.aimAlwaysOn);
        if (!cfg.aimAlwaysOn) DrawAimKeyCombo(cfg);
        static const char* kBones[] = {"Head", "Neck", "Chest", "Pelvis"};
        ImGui::Combo("Target Bone", &cfg.aimBone, kBones, 4);
        ImGui::SliderFloat("Aim FOV", &cfg.aimFov, 20.0f, 550.0f, "%.0f px");
        if (cfg.aimMode == static_cast<int>(AimMode::Camera)) {
          ImGui::SliderFloat("Smooth", &cfg.aimSmooth, 1.0f, 15.0f, "%.1f");
        }
        ImGui::SliderFloat("Max Distance", &cfg.aimMaxDist, 25.0f, 400.0f, "%.0f m");
      }
      ImGui::EndChild();

      ImGui::SetCursorPos(ImVec2(330.0f, 40.0f));
      ImGui::MenuChild("Targeting & Crosshair", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Draw FOV Circle", &cfg.drawFovCircle);
        ImGui::Checkbox("Draw Target Line", &cfg.drawTargetLine);
        ImGui::Checkbox("Triggerbot (Auto Fire)", &cfg.triggerbot);
        if (cfg.triggerbot) {
          ImGui::SliderFloat("Trigger FOV", &cfg.triggerFov, 4.0f, 60.0f, "%.0f px");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Checkbox("Custom Crosshair", &cfg.customCrosshair);
        if (cfg.customCrosshair) {
          ImGui::SliderFloat("Crosshair Size", &cfg.crosshairSize, 3.0f, 18.0f, "%.1f");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::ColorEdit4("FOV Color", cfg.colFovCircle, ImGuiColorEditFlags_NoInputs);
      }
      ImGui::EndChild();
    } else if (cfg.activeTab == 1) {
      ImGui::SetCursorPos(ImVec2(15.0f, 40.0f));
      ImGui::MenuChild("Weapon Mods", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("No Recoil", &cfg.noRecoil);
        ImGui::Checkbox("Zero Spread (Hip + ADS)", &cfg.noSpread);
        ImGui::Checkbox("Fast Reload", &cfg.fastReload);
        if (cfg.fastReload) {
          ImGui::SliderFloat("Reload Scale", &cfg.reloadSpeedScale, 0.05f, 1.0f, "%.2fx");
        }
        ImGui::Checkbox("Rapid Fire", &cfg.rapidFire);
        if (cfg.rapidFire) {
          ImGui::SliderFloat("Fire Rate", &cfg.rapidFireMult, 1.0f, 12.0f, "%.1fx");
        }
        ImGui::Checkbox("Full Auto Any Gun", &cfg.fullAuto);
        ImGui::Checkbox("Instant Weapon Swap", &cfg.instantSwap);
        ImGui::Checkbox("Infinite Ammo (Clip)", &cfg.infAmmo);
        ImGui::Checkbox("Infinite Reserve Ammo", &cfg.infAmmoReserve);
        ImGui::Checkbox("One-Hit Kill", &cfg.oneHitKill);
        if (cfg.oneHitKill) {
          ImGui::SliderInt("Damage", &cfg.customWeaponDmg, 500, 100000);
        }
      }
      ImGui::EndChild();

      ImGui::SetCursorPos(ImVec2(330.0f, 40.0f));
      ImGui::MenuChild("Give Weapons & Ammo", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (ImGui::Button("Refill Ammo", ImVec2(280.0f, 24.0f))) {
          sdk::QueueAction(sdk::ActionType::RefillAmmo);
        }
        if (ImGui::Button("Give All Weapons", ImVec2(280.0f, 24.0f))) {
          sdk::QueueAction(sdk::ActionType::ToggleCheats);
          sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "give all");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        static const struct { const char* name; const char* cmd; } kWeapons[] = {
            {"Ray Gun", "give ray_gun"},
            {"Wunderwaffe DG-2", "give tesla_gun"},
            {"PPSh-41", "give ppsh"},
            {"MG42", "give mg42"},
            {"PTRS-41", "give ptrs41"},
            {"Thompson", "give thompson"},
            {"StG-44", "give stg44"},
            {"Flamethrower", "give m2_flamethrower"},
            {"Panzerschreck", "give panzerschrek"},
        };
        for (const auto& w : kWeapons) {
          if (ImGui::Button(w.name, ImVec2(280.0f, 22.0f))) {
            sdk::QueueAction(sdk::ActionType::ToggleCheats);
            sdk::QueueAction(sdk::ActionType::ExecCmd, 0, w.cmd);
          }
        }
      }
      ImGui::EndChild();
    } else if (cfg.activeTab == 2) {
      ImGui::SetCursorPos(ImVec2(15.0f, 40.0f));
      ImGui::MenuChild("Enemy ESP", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Enable Enemy ESP", &cfg.espEnabled);
        ImGui::Checkbox("Enemies Only", &cfg.espEnemiesOnly);
        ImGui::Checkbox("2D Box", &cfg.espBox);
        if (cfg.espBox) {
          static const char* kBox[] = {"Corner", "Full Box"};
          ImGui::Combo("Box Type", &cfg.espBoxStyle, kBox, 2);
        }
        ImGui::Checkbox("Health Bar", &cfg.espHealthBar);
        ImGui::Checkbox("HP Value Text", &cfg.espHealthText);
        ImGui::Checkbox("Snaplines", &cfg.espSnapline);
        if (cfg.espSnapline) {
          static const char* kSnaps[] = {"Bottom", "Center", "Top"};
          ImGui::Combo("Snap Origin", &cfg.espSnaplineOrigin, kSnaps, 3);
        }
        ImGui::Checkbox("Name", &cfg.espName);
        ImGui::Checkbox("Held Weapon", &cfg.espWeapon);
        ImGui::Checkbox("Distance (m)", &cfg.espDistance);
        ImGui::Checkbox("Skeleton Bones", &cfg.espSkeleton);
        ImGui::Checkbox("360 Offscreen Alert", &cfg.espOffscreenAlert);
        if (cfg.espOffscreenAlert) ImGui::SliderFloat("Alert Radius", &cfg.offscreenRadius, 90.0f, 340.0f, "%.0f px");
        ImGui::SliderFloat("Max Distance", &cfg.espMaxDist, 30.0f, 500.0f, "%.0f m");
      }
      ImGui::EndChild();

      ImGui::SetCursorPos(ImVec2(330.0f, 40.0f));
      ImGui::MenuChild("World ESP & Colors", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Mission Objectives ESP", &cfg.espObjectives);
        ImGui::Checkbox("Weapons & Pickups ESP", &cfg.espWeaponsItems);
        ImGui::Checkbox("Grenade Warning ESP", &cfg.espGrenades);
        ImGui::Checkbox("Vehicles & MG Turrets", &cfg.espVehicles);
        ImGui::Checkbox("Mission Script Objects", &cfg.espScriptInteract);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::ColorEdit4("Visible Enemy", cfg.colVisible, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Hidden Enemy", cfg.colHidden, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Ally", cfg.colAlly, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Skeleton", cfg.colSkeleton, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Objective", cfg.colObjective, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Item / Weapon", cfg.colItem, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Grenade", cfg.colGrenade, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Vehicle", cfg.colVehicle, ImGuiColorEditFlags_NoInputs);
      }
      ImGui::EndChild();
    } else if (cfg.activeTab == 3) {
      ImGui::SetCursorPos(ImVec2(15.0f, 40.0f));
      ImGui::MenuChild("Player Protection", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("God Mode", &cfg.godMode);
        ImGui::Checkbox("Demi-God", &cfg.demiGod);
        ImGui::Checkbox("No Target", &cfg.noTarget);
        ImGui::Checkbox("Noclip", &cfg.noclip);
        ImGui::Checkbox("Noclip Hotkey [V]", &cfg.noclipKeyToggle);
        ImGui::Checkbox("UFO Mode", &cfg.ufo);
        ImGui::Checkbox("Infinite Sprint", &cfg.infSprint);
        ImGui::Checkbox("No Fall Damage", &cfg.noFallDmg);
        ImGui::Checkbox("Third Person", &cfg.thirdPerson);
      }
      ImGui::EndChild();

      ImGui::SetCursorPos(ImVec2(330.0f, 40.0f));
      ImGui::MenuChild("Movement & View", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Override FOV", &cfg.overrideFov);
        if (cfg.overrideFov) ImGui::SliderFloat("FOV", &cfg.customFov, 50.0f, 125.0f, "%.0f");
        ImGui::Checkbox("Override Speed", &cfg.overrideSpeed);
        if (cfg.overrideSpeed) ImGui::SliderInt("g_speed", &cfg.customSpeed, 80, 700);
        ImGui::Checkbox("Override Max HP", &cfg.overrideMaxHealth);
        if (cfg.overrideMaxHealth) ImGui::SliderInt("Max Health", &cfg.customMaxHealth, 100, 5000);
        ImGui::Checkbox("Override Timescale", &cfg.overrideTimescale);
        if (cfg.overrideTimescale) ImGui::SliderFloat("Timescale", &cfg.customTimescale, 0.1f, 4.0f, "%.2fx");
      }
      ImGui::EndChild();
    } else if (cfg.activeTab == 4) {
      ImGui::SetCursorPos(ImVec2(15.0f, 40.0f));
      ImGui::MenuChild("Zombies Points & Enemies", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Text("Current Points: %d  |  Kills: %d", snap.playerPoints, snap.playerKills);
        ImGui::SliderInt("Amount", &cfg.givePointsAmount, 1000, 250000);
        if (ImGui::Button("Add Points", ImVec2(280.0f, 24.0f))) {
          sdk::QueueAction(sdk::ActionType::AddPoints, cfg.givePointsAmount);
        }
        ImGui::Checkbox("Lock Points", &cfg.lockPoints);
        if (cfg.lockPoints) ImGui::SliderInt("Locked Pts", &cfg.lockedPoints, 10000, 999999);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (ImGui::Button("Set All Hostiles 1 HP", ImVec2(280.0f, 24.0f))) {
          sdk::QueueAction(sdk::ActionType::WeakAllEnemies);
        }
        if (ImGui::Button("Enable sv_cheats 1", ImVec2(280.0f, 24.0f))) {
          sdk::QueueAction(sdk::ActionType::ToggleCheats);
        }
      }
      ImGui::EndChild();

      ImGui::SetCursorPos(ImVec2(330.0f, 40.0f));
      ImGui::MenuChild("Console Runner", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::InputText("##CmdOld", cfg.customCmd, sizeof(cfg.customCmd));
        if (ImGui::Button("Execute Command", ImVec2(280.0f, 24.0f))) {
          sdk::QueueAction(sdk::ActionType::ExecCmd, 0, cfg.customCmd);
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::TextDisabled("Presets:");
        if (ImGui::Button("Toggle ai_disableSpawn", ImVec2(280.0f, 22.0f))) {
          sdk::QueueAction(sdk::ActionType::ToggleCheats);
          sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "toggle ai_disableSpawn");
        }
        if (ImGui::Button("Low Gravity (300)", ImVec2(280.0f, 22.0f))) {
          sdk::QueueAction(sdk::ActionType::ToggleCheats);
          sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "g_gravity 300");
        }
        if (ImGui::Button("Normal Gravity (800)", ImVec2(280.0f, 22.0f))) {
          sdk::QueueAction(sdk::ActionType::ExecCmd, 0, "g_gravity 800");
        }
      }
      ImGui::EndChild();
    } else if (cfg.activeTab == 5) {
      ImGui::SetCursorPos(ImVec2(15.0f, 40.0f));
      ImGui::MenuChild("Info & Status", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (g_fontTitle) ImGui::PushFont(g_fontTitle);
        ImGui::TextColored(ImVec4(0.54f, 0.71f, 0.96f, 1.0f), "CoD: World at War SP Suite");
        if (g_fontTitle) ImGui::PopFont();
        ImGui::TextDisabled("Target: CoDWaW.exe (DX9 x86)");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Checkbox("Show Keybinds HUD [F7]", &cfg.keybindsVisible);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::BulletText("In Game: %s", snap.inGame ? "Yes" : "No");
        ImGui::BulletText("Health: %d / %d", snap.playerHealth, snap.playerMaxHealth);
        ImGui::BulletText("Weapon: %s", snap.currentWeaponName.empty() ? "-" : snap.currentWeaponName.c_str());
        ImGui::BulletText("Ammo: %d / %d", snap.weaponClip, snap.weaponReserve);
        ImGui::BulletText("Actors: %d | Objectives: %d",
                          static_cast<int>(snap.actors.size()),
                          static_cast<int>(snap.objectives.size()));
      }
      ImGui::EndChild();

      ImGui::SetCursorPos(ImVec2(330.0f, 40.0f));
      ImGui::MenuChild("Hotkeys & Actions", ImVec2(304.0f, 392.0f), false);
      {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::BulletText("INSERT / F1 : Toggle Menu");
        ImGui::BulletText("F7          : Toggle Keybinds HUD");
        ImGui::BulletText("V           : Quick Noclip");
        ImGui::BulletText("END         : Unload Mod Cleanly");
        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        if (ImGui::Button("Unload Mod DLL", ImVec2(280.0f, 28.0f))) {
          cfg.menuOpen = false;
          cfg.unloading = true;
        }
      }
      ImGui::EndChild();
    }

    if (g_fontMain) ImGui::PopFont();

    draw->AddText(g_fontMain, g_fontMain ? g_fontMain->FontSize : 13.5f,
                  ImVec2(p.x + 12.0f, p.y + s.y - 19.0f), ImColor(170, 175, 185, 210),
                  "[INS] Menu  |  [F7] Keybinds  |  [V] Noclip  |  [END] Unload");
    if (snap.inGame) {
      char botStat[96]{};
      std::snprintf(botStat, sizeof(botStat), "HP %d  |  %d Pts  |  %d/%d",
                    snap.playerHealth, snap.playerPoints, snap.weaponClip, snap.weaponReserve);
      const ImVec2 bsz = g_fontMain ? g_fontMain->CalcTextSizeA(g_fontMain->FontSize, FLT_MAX, 0.0f, botStat)
                                    : ImGui::CalcTextSize(botStat);
      draw->AddText(g_fontMain, g_fontMain ? g_fontMain->FontSize : 13.5f,
                    ImVec2(p.x + s.x - bsz.x - 12.0f, p.y + s.y - 19.0f),
                    ImColor(135, 210, 170, 235), botStat);
    }
  }
  ImGui::End();
}

} // namespace waw::ui
