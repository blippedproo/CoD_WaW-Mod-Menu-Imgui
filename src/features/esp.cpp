#include "esp.hpp"
#include "config.hpp"
#include "../game/sdk.hpp"

#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace waw::esp {

namespace {

ImU32 ToImCol(const float c[4], float alphaScale = 1.0f) {
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(c[0], c[1], c[2], std::clamp(c[3] * alphaScale, 0.0f, 1.0f)));
}

void DrawOutlinedText(ImDrawList* draw, float x, float y, ImU32 col, const char* text, bool centered = true) {
  if (!text || !text[0]) return;
  ImVec2 sz = ImGui::CalcTextSize(text);
  const float px = centered ? std::floor(x - sz.x * 0.5f) : std::floor(x);
  const float py = std::floor(y);
  const ImU32 shadow = IM_COL32(0, 0, 0, 215);
  draw->AddText(ImVec2(px + 1.0f, py + 1.0f), shadow, text);
  draw->AddText(ImVec2(px, py), col, text);
}

void DrawCornerBox(ImDrawList* draw, float x, float y, float w, float h, ImU32 col, float thickness = 1.6f) {
  const float lw = std::max(4.0f, w * 0.25f);
  const float lh = std::max(4.0f, h * 0.25f);
  const ImU32 outline = IM_COL32(0, 0, 0, 200);

  auto line2 = [&](float x1, float y1, float x2, float y2) {
    draw->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), outline, thickness + 1.5f);
    draw->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, thickness);
  };

  // Top-Left
  line2(x, y, x + lw, y);
  line2(x, y, x, y + lh);
  // Top-Right
  line2(x + w - lw, y, x + w, y);
  line2(x + w, y, x + w, y + lh);
  // Bottom-Left
  line2(x, y + h, x + lw, y + h);
  line2(x, y + h - lh, x, y + h);
  // Bottom-Right
  line2(x + w - lw, y + h, x + w, y + h);
  line2(x + w, y + h - lh, x + w, y + h);
}

void DrawFullBox(ImDrawList* draw, float x, float y, float w, float h, ImU32 col, float thickness = 1.4f) {
  draw->AddRect(ImVec2(x - 1.0f, y - 1.0f), ImVec2(x + w + 1.0f, y + h + 1.0f), IM_COL32(0, 0, 0, 200), 0.0f, 0, thickness + 1.2f);
  draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col, 0.0f, 0, thickness);
}

ImU32 HealthGradientColor(float frac) {
  frac = std::clamp(frac, 0.0f, 1.0f);
  float r = (frac < 0.5f) ? 1.0f : (1.0f - (frac - 0.5f) * 2.0f);
  float g = (frac < 0.5f) ? (frac * 2.0f) : 1.0f;
  return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.15f, 1.0f));
}

void DrawDiamondMarker(ImDrawList* draw, float cx, float cy, float r, ImU32 fillCol, ImU32 borderCol) {
  ImVec2 pts[4] = {
      ImVec2(cx, cy - r),
      ImVec2(cx + r, cy),
      ImVec2(cx, cy + r),
      ImVec2(cx - r, cy),
  };
  draw->AddConvexPolyFilled(pts, 4, fillCol);
  draw->AddPolyline(pts, 4, borderCol, 1, 1.4f);
}

} // namespace

void Render() {
  const ModConfig& cfg = Config();
  if (cfg.unloading) return;

  const sdk::FrameSnapshot snap = sdk::CopyFrameSnapshot();
  if (!snap.inGame) return;

  ImDrawList* draw = ImGui::GetBackgroundDrawList();
  if (!draw) return;

  const ImVec2 disp = ImGui::GetIO().DisplaySize;
  const float sw = disp.x;
  const float sh = disp.y;
  const float cx = sw * 0.5f;
  const float cy = sh * 0.5f;

  // 1. Custom Crosshair
  if (cfg.customCrosshair) {
    const float sz = cfg.crosshairSize;
    const float gap = 2.5f;
    const ImU32 bg = IM_COL32(0, 0, 0, 210);
    const ImU32 fg = IM_COL32(120, 255, 160, 245);
    draw->AddLine(ImVec2(cx - sz, cy), ImVec2(cx - gap, cy), bg, 2.8f);
    draw->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + sz, cy), bg, 2.8f);
    draw->AddLine(ImVec2(cx, cy - sz), ImVec2(cx, cy - gap), bg, 2.8f);
    draw->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + sz), bg, 2.8f);
    draw->AddLine(ImVec2(cx - sz, cy), ImVec2(cx - gap, cy), fg, 1.3f);
    draw->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + sz, cy), fg, 1.3f);
    draw->AddLine(ImVec2(cx, cy - sz), ImVec2(cx, cy - gap), fg, 1.3f);
    draw->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + sz), fg, 1.3f);
  }

  // 2. Aimbot FOV Circle & Active Target Lock Line
  if (cfg.aimEnabled && cfg.drawFovCircle) {
    draw->AddCircle(ImVec2(cx, cy), cfg.aimFov, ToImCol(cfg.colFovCircle), 64, 1.3f);
  }

  if (cfg.aimEnabled && cfg.drawTargetLine && snap.hasAimTarget) {
    sdk::Vec2 t2d{};
    if (sdk::WorldToScreen(snap.aimTargetPos3D, t2d)) {
      const ImU32 lineCol = snap.aimTargetVisible ? IM_COL32(255, 225, 70, 220) : IM_COL32(255, 100, 60, 180);
      draw->AddLine(ImVec2(cx, cy), ImVec2(t2d.x, t2d.y), lineCol, 1.25f);
      draw->AddCircleFilled(ImVec2(t2d.x, t2d.y), 3.2f, lineCol);
    }
  }

  // 3. Mission Objectives ESP (with offscreen compass indicator if behind camera)
  if (cfg.espObjectives) {
    const ImU32 objCol = ToImCol(cfg.colObjective);
    for (const auto& obj : snap.objectives) {
      sdk::Vec2 sPos{};
      if (sdk::WorldToScreen(obj.origin3D, sPos) && sPos.x > -40.0f && sPos.x < sw + 40.0f && sPos.y > -40.0f && sPos.y < sh + 40.0f) {
        DrawDiamondMarker(draw, sPos.x, sPos.y, 6.5f, IM_COL32(255, 200, 35, 170), objCol);
        char buf[160]{};
        std::snprintf(buf, sizeof(buf), "[OBJ] %s (%.0fm)", obj.text.c_str(), obj.distance);
        DrawOutlinedText(draw, sPos.x, sPos.y + 9.0f, objCol, buf);
      } else {
        sdk::Vec2 dir{};
        float ang = 0.0f;
        if (sdk::CalcOffscreenDir(obj.origin3D, dir, ang)) {
          const float rad = cfg.offscreenRadius + 24.0f;
          const float ax = std::clamp(cx + dir.x * rad, 30.0f, sw - 30.0f);
          const float ay = std::clamp(cy + dir.y * rad, 30.0f, sh - 30.0f);
          DrawDiamondMarker(draw, ax, ay, 6.0f, IM_COL32(255, 200, 35, 150), objCol);
          char dbuf[48]{};
          std::snprintf(dbuf, sizeof(dbuf), "OBJ %.0fm", obj.distance);
          DrawOutlinedText(draw, ax, ay + 8.0f, objCol, dbuf);
        }
      }
    }
  }

  // 4. Dropped Weapons / Grenades / Vehicles / Turrets / Script Interactors ESP
  for (const auto& wi : snap.worldItems) {
    sdk::Vec2 sPos{};
    if (!sdk::WorldToScreen(wi.origin3D, sPos)) continue;
    if (sPos.x < -50.0f || sPos.x > sw + 50.0f || sPos.y < -50.0f || sPos.y > sh + 50.0f) continue;

    ImU32 col = ToImCol(cfg.colItem);
    const char* tag = "ITEM";
    if (wi.category == 1) {
      col = ToImCol(cfg.colGrenade);
      tag = "DANGER";
      draw->AddCircle(ImVec2(sPos.x, sPos.y), 11.0f, col, 18, 1.8f);
    } else if (wi.category == 2) {
      col = ToImCol(cfg.colVehicle);
      tag = "VEH";
      draw->AddRect(ImVec2(sPos.x - 5.0f, sPos.y - 5.0f), ImVec2(sPos.x + 5.0f, sPos.y + 5.0f), col, 0.0f, 0, 1.5f);
    } else if (wi.category == 3) {
      col = IM_COL32(210, 210, 255, 220);
      tag = "TASK";
      draw->AddCircleFilled(ImVec2(sPos.x, sPos.y), 3.0f, col);
    } else {
      draw->AddRectFilled(ImVec2(sPos.x - 2.5f, sPos.y - 2.5f), ImVec2(sPos.x + 2.5f, sPos.y + 2.5f), col);
    }

    char buf[128]{};
    std::snprintf(buf, sizeof(buf), "[%s] %s (%.0fm)", tag, wi.label.c_str(), wi.distance);
    DrawOutlinedText(draw, sPos.x, sPos.y + 6.0f, col, buf);
  }

  // 5. Actors / Enemies ESP
  if (!cfg.espEnabled) return;

  for (const auto& act : snap.actors) {
    const ImU32 boxCol = !act.isHostile
                             ? ToImCol(cfg.colAlly)
                             : (act.visible ? ToImCol(cfg.colVisible) : ToImCol(cfg.colHidden));

    // Project skeleton bones once; derive tight 2D bounding box directly from projected skeleton joints
    float bMinX = FLT_MAX, bMinY = FLT_MAX, bMaxX = -FLT_MAX, bMaxY = -FLT_MAX;
    int bonePts = 0;
    const ImU32 skelCol = act.isHostile ? ToImCol(cfg.colSkeleton) : ToImCol(cfg.colAlly, 0.85f);

    for (const auto& bp : act.bones) {
      sdk::Vec2 a2d{}, b2d{};
      const bool aOk = sdk::WorldToScreen(bp.a, a2d);
      const bool bOk = sdk::WorldToScreen(bp.b, b2d);
      if (aOk) {
        bMinX = std::min(bMinX, a2d.x);
        bMaxX = std::max(bMaxX, a2d.x);
        bMinY = std::min(bMinY, a2d.y);
        bMaxY = std::max(bMaxY, a2d.y);
        ++bonePts;
      }
      if (bOk) {
        bMinX = std::min(bMinX, b2d.x);
        bMaxX = std::max(bMaxX, b2d.x);
        bMinY = std::min(bMinY, b2d.y);
        bMaxY = std::max(bMaxY, b2d.y);
        ++bonePts;
      }
      if (cfg.espSkeleton && aOk && bOk) {
        draw->AddLine(ImVec2(a2d.x, a2d.y), ImVec2(b2d.x, b2d.y), IM_COL32(0, 0, 0, 165), 2.4f);
        draw->AddLine(ImVec2(a2d.x, a2d.y), ImVec2(b2d.x, b2d.y), skelCol, 1.25f);
      }
    }

    sdk::Vec2 feet2D{}, head2D{};
    const bool feetOn = sdk::WorldToScreen(act.origin3D, feet2D);
    sdk::Vec3 top3D = act.head3D;
    top3D.z += 9.5f;
    const bool headOn = sdk::WorldToScreen(top3D, head2D);

    float bx = 0.0f, by = 0.0f, w = 0.0f, h = 0.0f;
    float anchorX = 0.0f, anchorBotY = 0.0f;
    bool haveBox = false;

    if (bonePts >= 2) {
      const float rawH = std::max(8.0f, bMaxY - bMinY);
      const float padTop = rawH * 0.14f;
      const float padBot = rawH * 0.06f;
      const float topY = headOn ? std::min(head2D.y, bMinY - padTop) : (bMinY - padTop);
      float botY = bMaxY + padBot;
      const float boneCx = (bMinX + bMaxX) * 0.5f;
      if (feetOn && feet2D.y > botY && (feet2D.y - botY) < rawH * 0.45f &&
          std::fabs(feet2D.x - boneCx) < rawH * 0.65f) {
        botY = feet2D.y;
      }
      h = std::max(12.0f, botY - topY);
      w = std::max((bMaxX - bMinX) + 10.0f, h * 0.46f);
      bx = boneCx - w * 0.5f;
      by = topY;
      anchorX = boneCx;
      anchorBotY = topY + h;
      haveBox = true;
    } else if (headOn && feetOn) {
      const float topY = std::min(head2D.y, feet2D.y);
      const float botY = std::max(head2D.y, feet2D.y);
      h = std::max(12.0f, botY - topY);
      w = h * 0.52f;
      const float midX = (head2D.x + feet2D.x) * 0.5f;
      bx = midX - w * 0.5f;
      by = topY;
      anchorX = midX;
      anchorBotY = botY;
      haveBox = true;
    } else if (headOn) {
      h = std::clamp(1100.0f / std::max(act.distance, 1.5f), 20.0f, 160.0f);
      w = h * 0.52f;
      bx = head2D.x - w * 0.5f;
      by = head2D.y;
      anchorX = head2D.x;
      anchorBotY = by + h;
      haveBox = true;
    } else if (feetOn) {
      h = std::clamp(1100.0f / std::max(act.distance, 1.5f), 20.0f, 160.0f);
      w = h * 0.52f;
      bx = feet2D.x - w * 0.5f;
      by = feet2D.y - h;
      anchorX = feet2D.x;
      anchorBotY = feet2D.y;
      haveBox = true;
    }

    const bool inViewport = haveBox &&
                            (bx + w) > -40.0f && bx < sw + 40.0f &&
                            (by + h) > -40.0f && by < sh + 40.0f;

    // 360° Offscreen Threat Arrow
    if (!inViewport && act.isHostile && cfg.espOffscreenAlert) {
      sdk::Vec2 dir{};
      float ang = 0.0f;
      if (sdk::CalcOffscreenDir(act.origin3D, dir, ang)) {
        const float r = cfg.offscreenRadius;
        const float px = std::clamp(cx + dir.x * r, 24.0f, sw - 24.0f);
        const float py = std::clamp(cy + dir.y * r, 24.0f, sh - 24.0f);

        const float tipLen = 13.0f;
        const float sideLen = 8.0f;
        const float cosA = std::cos(ang);
        const float sinA = std::sin(ang);
        const ImVec2 pTip(px + cosA * tipLen, py + sinA * tipLen);
        const ImVec2 pL(px - cosA * 4.0f - sinA * sideLen, py - sinA * 4.0f + cosA * sideLen);
        const ImVec2 pR(px - cosA * 4.0f + sinA * sideLen, py - sinA * 4.0f - cosA * sideLen);

        const ImU32 arrCol = (act.distance < 15.0f) ? IM_COL32(255, 55, 55, 245) : boxCol;
        draw->AddTriangleFilled(pTip, pL, pR, arrCol);
        draw->AddTriangle(pTip, pL, pR, IM_COL32(0, 0, 0, 220), 1.3f);

        if (cfg.espDistance) {
          char dbuf[24]{};
          std::snprintf(dbuf, sizeof(dbuf), "%.0fm", act.distance);
          DrawOutlinedText(draw, px - dir.x * 16.0f, py - dir.y * 16.0f - 6.0f, arrCol, dbuf);
        }
      }
      continue;
    }

    if (!inViewport) continue;

    // 2D / Corner Box
    if (cfg.espBox) {
      if (cfg.espBoxStyle == static_cast<int>(EspBoxStyle::Corner)) {
        DrawCornerBox(draw, bx, by, w, h, boxCol);
      } else {
        DrawFullBox(draw, bx, by, w, h, boxCol);
      }
    }

    // Health Bar + numeric HP
    if (cfg.espHealthBar) {
      const float frac = (act.maxHealth > 0)
                             ? std::clamp(static_cast<float>(act.health) / static_cast<float>(act.maxHealth), 0.0f, 1.0f)
                             : 1.0f;
      const float barX = bx - 6.0f;
      const float fillH = h * frac;
      draw->AddRectFilled(ImVec2(barX - 1.0f, by - 1.0f), ImVec2(barX + 3.0f, by + h + 1.0f), IM_COL32(0, 0, 0, 210));
      draw->AddRectFilled(ImVec2(barX, by + h - fillH), ImVec2(barX + 2.0f, by + h), HealthGradientColor(frac));

      if (cfg.espHealthText) {
        char hpBuf[24]{};
        std::snprintf(hpBuf, sizeof(hpBuf), "%d", act.health);
        DrawOutlinedText(draw, barX - 4.0f, by + h - fillH - 6.0f, IM_COL32(255, 255, 255, 240), hpBuf);
      }
    } else if (cfg.espHealthText) {
      char hpBuf[24]{};
      std::snprintf(hpBuf, sizeof(hpBuf), "%d HP", act.health);
      DrawOutlinedText(draw, anchorX, by - 26.0f, HealthGradientColor(static_cast<float>(act.health) / std::max(1, act.maxHealth)), hpBuf);
    }

    // Snaplines
    if (cfg.espSnapline) {
      float startY = sh;
      if (cfg.espSnaplineOrigin == static_cast<int>(SnaplineOrigin::Center)) startY = cy;
      else if (cfg.espSnaplineOrigin == static_cast<int>(SnaplineOrigin::Top)) startY = 0.0f;
      draw->AddLine(ImVec2(cx, startY), ImVec2(anchorX, anchorBotY), boxCol, 1.1f);
    }

    // Top Label: Name + Distance
    float topCursorY = by - 15.0f;
    if (cfg.espName || cfg.espDistance) {
      char labelBuf[128]{};
      if (cfg.espName && cfg.espDistance) {
        std::snprintf(labelBuf, sizeof(labelBuf), "%s [%.0fm]", act.name.c_str(), act.distance);
      } else if (cfg.espName) {
        std::snprintf(labelBuf, sizeof(labelBuf), "%s", act.name.c_str());
      } else {
        std::snprintf(labelBuf, sizeof(labelBuf), "%.0fm", act.distance);
      }
      DrawOutlinedText(draw, anchorX, topCursorY, boxCol, labelBuf);
    }

    // Bottom Label: Held Weapon
    if (cfg.espWeapon && !act.weaponName.empty()) {
      DrawOutlinedText(draw, anchorX, by + h + 3.0f, IM_COL32(225, 232, 245, 230), act.weaponName.c_str());
    }
  }
}

} // namespace waw::esp
