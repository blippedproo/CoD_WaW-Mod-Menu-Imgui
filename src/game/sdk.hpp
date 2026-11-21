#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace waw::sdk {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct BonePair3D {
  Vec3 a;
  Vec3 b;
};

struct ActorSnapshot {
  int entNum = -1;
  int eType = 0;
  bool isHostile = true;
  bool visible = false;
  int health = 100;
  int maxHealth = 100;
  float distance = 0.0f;
  Vec3 origin3D;
  Vec3 head3D;
  Vec3 aimPos3D;
  std::string name;
  std::string weaponName;
  std::vector<BonePair3D> bones;
};

struct WorldItemSnapshot {
  int entNum = -1;
  int eType = 0;
  int category = 0; // 0=Weapon/Item, 1=Grenade/Missile, 2=Vehicle/Turret, 3=ScriptObj
  float distance = 0.0f;
  Vec3 origin3D;
  std::string label;
};

struct ObjectiveSnapshot {
  int index = 0;
  int state = 0;
  float distance = 0.0f;
  Vec3 origin3D;
  std::string text;
};

struct SilentTargetState {
  bool valid = false;
  int entNum = -1;
  bool visible = false;
  Vec3 aimPos3D;
  float dist = 0.0f;
  float fovDist = 0.0f;
};

struct FrameSnapshot {
  bool inGame = false;
  bool hasServerHost = false;
  int localClientNum = 0;
  int playerHealth = 0;
  int playerMaxHealth = 100;
  int playerPoints = 0;
  int playerKills = 0;
  int weaponClip = 0;
  int weaponReserve = 0;
  std::string currentWeaponName;
  Vec3 playerOrigin;

  bool aimKeyDown = false;
  bool hasAimTarget = false;
  int aimTargetEnt = -1;
  bool aimTargetVisible = false;
  Vec3 aimTargetPos3D;

  std::vector<ActorSnapshot> actors;
  std::vector<WorldItemSnapshot> worldItems;
  std::vector<ObjectiveSnapshot> objectives;
};

enum class ActionType : int {
  None = 0,
  ExecCmd,
  AddPoints,
  RefillAmmo,
  WeakAllEnemies,
  ToggleCheats,
};

struct QueuedAction {
  ActionType type = ActionType::None;
  int intArg = 0;
  std::string strArg;
};

// Thread-safe action queue & frame snapshot exchange
void QueueAction(ActionType type, int intArg = 0, const char* strArg = nullptr);
FrameSnapshot CopyFrameSnapshot();
SilentTargetState GetSilentTargetState();

// Math & Projection (usable from render thread too - reads refdef floats safely)
bool WorldToScreen(const Vec3& world, Vec2& outScreen, Vec2* outScreenSize = nullptr);
bool CalcOffscreenDir(const Vec3& world, Vec2& outDir, float& outAngleRad);
Vec3 CalcAimAngles(const Vec3& src, const Vec3& dst);
void AngleVectors(const Vec3& angles, Vec3* forward, Vec3* right, Vec3* up);
void NormalizeAngles(Vec3& angles);

// Engine & Main-Thread Tick
void OnMainGameTick();
void OnEndSceneCleanup();
void RestoreAllModifiedWeaponDefs();
const char* SLConvertToString(uint16_t id);
uint16_t SLGetString(const char* str);
bool IsEntityAliveServer(int entNum);

// Raw engine helpers
void CbufAddText(const char* cmd);
bool CGTraceVisible(const Vec3& start, const Vec3& end, int passEntNum, int targetEntNum);
bool GetEntityBonePos(int entNum, uint16_t tag, Vec3& outPos);

} // namespace waw::sdk
