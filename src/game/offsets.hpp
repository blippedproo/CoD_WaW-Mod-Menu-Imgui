#pragma once

#include <cstdint>

namespace waw::offsets {

// DirectX 9
constexpr uintptr_t kDx9Device = 0x03BABC88; // IDirect3DDevice9**
constexpr uintptr_t kDx9Object = 0x03BABC84; // IDirect3D9**

// Client connection & input state
constexpr uintptr_t kClsState = 0x0300B18C;     // connstate_t (>= 9 means in-game active)
constexpr uintptr_t kClViewAngles = 0x03030430; // float[3] (pitch, yaw, roll)
constexpr uintptr_t kClCmds = 0x030B0460;       // usercmd_s[128], stride 0x38
constexpr uintptr_t kClCmdNumber = 0x030B2060;  // int

// Client game (cg_t) & camera (refdef_t)
constexpr uintptr_t kCgBase = 0x03426A68;
constexpr uintptr_t kCgSnapPtr = 0x03426A8C;
constexpr uintptr_t kCgNextSnapPtr = 0x03426A90;
constexpr uintptr_t kCgPredictedPS = 0x034D16D4; // playerState_s
constexpr uintptr_t kRefdef = 0x034D3AB8;        // refdef_t
constexpr uintptr_t kCgKickAVel = 0x034DED0C;    // float[3]
constexpr uintptr_t kCgKickAngles = 0x034DED54;  // float[3]
constexpr uintptr_t kCgObjectives = 0x034DEE00;  // objective_t[16], stride 0x490
constexpr uintptr_t kCgClientInfo = 0x0356B520;  // clientInfo_t[4], stride 0x58C
constexpr uintptr_t kCgActorInfo = 0x0356CB80;   // actorInfo_t[], stride 0x458

// Client entities (centity_s[1024], stride 0x2D4)
constexpr uintptr_t kCgEntities = 0x03586BA8;
constexpr uint32_t kCentityStride = 0x2D4;
constexpr int kMaxEntities = 1024;

// Server entities & clients (SP / Co-op Host runs in the same process)
constexpr uintptr_t kGEntities = 0x0173CF00; // gentity_s[1024], stride 0x378
constexpr uint32_t kGentityStride = 0x378;
constexpr uintptr_t kGClientsPtr = 0x018C6540; // gclient_s**
constexpr uint32_t kGclientStride = 0x233C;

// Script strings (SL) & DObj bone pool
constexpr uintptr_t kSlStringTableBasePtr = 0x036B5540; // uint8_t**
constexpr uintptr_t kObjIndexTable = 0x01FB5E18;        // uint16_t[]
constexpr uintptr_t kObjPoolBase = 0x01F67E10;          // DObj pool, stride 0x68

// Weapons table
constexpr uintptr_t kBgWeaponDefs = 0x008E6FF0; // WeaponDef*[]
constexpr uintptr_t kBgNumWeapons = 0x0468EA40; // uint32_t

// Engine functions
constexpr uintptr_t kFnCbufAddText = 0x0058E050;
constexpr uintptr_t kFnCGTrace = 0x0046AA50;
constexpr uintptr_t kTraceVecZero = 0x0080BD38;
constexpr uintptr_t kFnCGDObjGetWorldTagPosInternal = 0x00441000;
constexpr uintptr_t kFnSLGetString = 0x00684740;
constexpr uintptr_t kFnCLCreateNewCommands = 0x006355F0;
constexpr uintptr_t kFnGGetWeaponParam = 0x0054D530;
constexpr uintptr_t kFnBGWeaponFireRecoil = 0x00420E90;

// Dvar pointers (dvar_s* at VA, current value at +0x0C, latched value at +0x1C? we set +0x0C)
constexpr uintptr_t kDvarPlayerSustainAmmo = 0x008E4D50;     // bool
constexpr uintptr_t kDvarPlayerClipMult = 0x008E2164;        // float
constexpr uintptr_t kDvarPlayerSprintUnlimited = 0x008E4CD4; // bool
constexpr uintptr_t kDvarBgFallDmgMin = 0x008E4D08;          // float
constexpr uintptr_t kDvarBgFallDmgMax = 0x008E4C48;          // float
constexpr uintptr_t kDvarGSpeed = 0x018B8F3C;                // int (default 190)
constexpr uintptr_t kDvarGPlayerMaxHealth = 0x01826028;      // int (default 100)
constexpr uintptr_t kDvarPlayerDamageMult = 0x0170CEEC;      // float (incoming dmg to player)
constexpr uintptr_t kDvarPlayerMeleeDamageMult = 0x018CB9E8; // float (incoming melee dmg)
constexpr uintptr_t kDvarComTimescale = 0x01F45820;          // float (default 1.0)
constexpr uintptr_t kDvarCgFov = 0x03641D28;                 // float (default 65.0)
constexpr uintptr_t kDvarCgThirdPerson = 0x03350400;         // bool
constexpr uintptr_t kDvarSvCheats = 0x018B8EFC;              // bool

// Entity type constants (verified from CoDWaW.exe .rdata ET_* table)
namespace eType {
constexpr int General = 0;
constexpr int Player = 1;
constexpr int PlayerCorpse = 2;
constexpr int Item = 3;
constexpr int Missile = 4;
constexpr int Invisible = 5;
constexpr int ScriptMover = 6;
constexpr int Mg42 = 11;
constexpr int Plane = 12;
constexpr int Vehicle = 13;
constexpr int VehicleCorpse = 15;
constexpr int Actor = 16;
constexpr int ActorSpawner = 17;
constexpr int ActorCorpse = 18;
} // namespace eType

// gentity_s::flags bits (verified from 0x004F0F80..0x004F1160)
namespace entFlags {
constexpr int GodMode = 0x1;
constexpr int DemiGodMode = 0x2;
constexpr int NoTarget = 0x4;
} // namespace entFlags

// sentient_s::eTeam (verified 2 = TEAM_ALLIES)
namespace team {
constexpr int Free = 0;
constexpr int Axis = 1;
constexpr int Allies = 2;
constexpr int Neutral = 3;
constexpr int Dead = 4;
} // namespace team

} // namespace waw::offsets
