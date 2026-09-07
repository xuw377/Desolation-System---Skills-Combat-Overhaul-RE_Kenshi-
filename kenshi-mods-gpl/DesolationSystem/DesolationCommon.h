// ============================================================================
// 本文件是 绝境系统 (Desolation System) 的一部分。
// Copyright (C) 2026 kenshi-mod-dev
//
// 本程序是自由软件：你可以根据自由软件基金会发布的 GNU 通用公共许可证
// (第 3 版或更高版本) 再分发和/或修改它。
// 本程序的发布希望它有用，但【不提供任何保证】。
// 详见 GNU 通用公共许可证 (LICENSE 文件)。
//
// This file is part of 绝境系统 (Desolation System).
// Copyright (C) 2026 kenshi-mod-dev
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License (LICENSE) for details.
// ============================================================================

#pragma once

// =============================================================================
// DesolationSystem.cpp
// 废土绝境系统 (Desolation System) - 全量大一统底层 C++ 核心引擎
// 
// 整合机制: 
//   - 27 项全量战斗/兵刃/体魄专长
//   - 7 大联合专长 (Synergy Perks，含建筑+科学+开锁>=80万能钥匙等)
//   - 4 大极境天道 (究极专长)
// =============================================================================

#include <Debug.h>
#include <Defines.h>
#include <core/Functions.h>

#include <ogre/OgrePrerequisites.h>
#include <kenshi/Enums.h>
#include <kenshi/CharStats.h>
#include <kenshi/util/lektor.h>

#include <windows.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "user32.lib")

// 前向声明
class Character;
class MedicalSystem;
class GameWorld;
class BountyManager;
class Faction;
class Inventory;
class InventorySection;
class Item;
class ForgottenGUI;
struct Damages;
struct SimpleVec3 {
    float x;
    float y;
    float z;
    SimpleVec3(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}
};

// ============================================================
// 全局动态可调门槛与数值配置 (支持热重载)
// ============================================================
static float MASTERY_THRESHOLD         = 60.0f;
static float STRENGTH_GAP_THRESHOLD    = 30.0f;
static float RECOIL_DAMAGE_PER_10      = 5.0f;
static float DEFENSE_MASTERY           = 105.0f;
static float COUNTER_THRESHOLD         = 100.0f;
static float HYPER_ARMOR_STR_REQ       = 85.0f;
static float SHOCKWAVE_STR_REQ         = 120.0f;
static float PICKPOCKET_MAX_DIST       = 35.0f;

// 联合专长 (Synergy Perks) 门槛变量
namespace SynergyPerks {
    static float MASTER_KEY_LOCK       = 80.0f;
    static float MASTER_KEY_ENG        = 80.0f;
    static float MASTER_KEY_SCI        = 80.0f;

    static float SILENT_STEALTH        = 80.0f;
    static float SILENT_ASSASSIN       = 80.0f;

    static float PICKPOCKET_STEALTH    = 80.0f;
    static float PICKPOCKET_THIEVING   = 80.0f;

    static float TITAN_LABOUR          = 70.0f;
    static float TITAN_ENG             = 70.0f;

    static float CHEF_FARM             = 70.0f;
    static float CHEF_COOK             = 70.0f;

    static float CYBER_MEDIC           = 80.0f;
    static float CYBER_SCI             = 80.0f;
}

#define STATS_WEAPON_TYPE(s)  (*(int*)((char*)(s) + 0x1F0))

// MedicalSystem 偏移 (P3 修正: 依据 KenshiLib 官方头文件 MedicalSystem.h)
// 官方: leftLeg@0x80 rightLeg@0x88 leftArm@0x90 rightArm@0x98 knockoutTimer@0xA0
// 获取所属角色请用 MedGetOwnerCharacter() (DesolationHooksABI.h);
// 旧 MED_ME(+0x08) 读 status map 内部数据为伪指针, 已废弃。
#define MED_HUNGER(m)         (*(float*)((char*)(m) + 0x60))
#define MED_BLOOD(m)          (*(float*)((char*)(m) + 0x70))
#define MED_BLEED_RATE(m)     (*(float*)((char*)(m) + 0x78))
#define MED_LEFT_LEG(m)       (*(void**)((char*)(m) + 0x80))
#define MED_RIGHT_LEG(m)      (*(void**)((char*)(m) + 0x88))
#define MED_LEFT_ARM(m)       (*(void**)((char*)(m) + 0x90))
#define MED_RIGHT_ARM(m)      (*(void**)((char*)(m) + 0x98))
#define MED_KNOCKOUT_TIMER(m) (*(float*)((char*)(m) + 0xA0))

// CombatClass 偏移
#define COMBAT_OWNER(c)       (*(Character**)((char*)(c) + 0x188))
#define COMBAT_TARGET(c)      (*(void**)((char*)(c) + 0x290))
#define COMBAT_STUMBLE_X(c)   (*(float*)((char*)(c) + 0x124))
#define COMBAT_STUMBLE_Y(c)   (*(float*)((char*)(c) + 0x128))
#define COMBAT_STUMBLE_Z(c)   (*(float*)((char*)(c) + 0x12C))

// HealthPartStatus 偏移 (官方: selfHealing@0x30 ... robotLimb@0x28 指针, flesh@0x40, wearDamage@0x50, _maxHealth@0x54)
// P3 审计修正: PART_IS_ROBOTIC 旧读 +0x30 (实为 selfHealing bool) -> 改判 robotLimb@0x28 非空 = 机械部位
#define PART_IS_ROBOTIC(p)    (*(void**)((char*)(p) + 0x28) != nullptr)
#define PART_ROBOT_LIMB(p)    (*(void**)((char*)(p) + 0x28))
#define PART_FLESH_HP(p)      (*(float*)((char*)(p) + 0x40))
#define PART_WEAR(p)          (*(float*)((char*)(p) + 0x50))
#define PART_MAX_HP(p)        (*(float*)((char*)(p) + 0x54))

static int   g_targetScope          = 0; // 0 = 全局, 1 = 仅玩家, 2 = 仅NPC
static float g_maxAttackSpeedCap    = 4.8f; // 攻速上限: 1.2, 2.4, 3.6, 4.8, 9.6
static bool  g_enableAscensionPerks = true; // 四大极境总开关
static bool  g_navShieldOn          = true; // 寻路崩溃盾 (引擎NavMesh工作线程已知AV签名拦截)
static volatile LONG g_shieldHits   = 0;    // 命中计数 (主循环汇报, VEH内不碰文件/锁)
static bool  g_ascensionLastDown    = false;
static bool  g_f6LastDown           = false;
static bool  g_f10LastDown          = false;
static bool  g_num1LastDown         = false;
static bool  g_num2LastDown         = false;
static bool  g_num3LastDown         = false;
static bool  g_num4LastDown         = false;
static bool  g_num5LastDown         = false;

static std::string GetDesolationIniPath() {
    HMODULE hDll = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&GetDesolationIniPath, &hDll) && hDll) {
        char dllPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hDll, dllPath, MAX_PATH) > 0) {
            char* lastSlash = strrchr(dllPath, '\\');
            if (lastSlash) {
                *lastSlash = '\0';
                return std::string(dllPath) + "\\DesolationSystem.ini";
            }
        }
    }
    return "mods\\DesolationSystem\\DesolationSystem.ini";
}

static float ReadIniFloat(const char* section, const char* key, float defVal, const char* path) {
    char buf[64] = { 0 };
    char defBuf[32] = { 0 };
    snprintf(defBuf, sizeof(defBuf), "%.2f", defVal);
    GetPrivateProfileStringA(section, key, defBuf, buf, sizeof(buf), path);
    float val = (float)atof(buf);
    return (val != 0.0f || buf[0] == '0') ? val : defVal;
}

// 灰度开关前置声明 (定义在文件尾部, 见 LoadHookSwitches)
static void LoadHookSwitches();
static bool IsHookDisabled(const char* key);

static void LoadDesolationConfig() {
    std::string iniPath = GetDesolationIniPath();
    g_targetScope = (int)GetPrivateProfileIntA("Settings", "TargetScope", 0, iniPath.c_str());
    if (g_targetScope < 0 || g_targetScope > 2) g_targetScope = 0;
    
    g_maxAttackSpeedCap    = ReadIniFloat("Settings", "AttackSpeedCap", 4.8f, iniPath.c_str());
    g_enableAscensionPerks = GetPrivateProfileIntA("Settings", "EnableAscension", 1, iniPath.c_str()) != 0;
    g_navShieldOn          = GetPrivateProfileIntA("Settings", "NavMeshCrashShield", 0, iniPath.c_str()) != 0;   // 2026-09-07 默认关闭: ret桩提前返回会破坏寻路状态→延迟野跳崩溃(实测)

    // 数值与门槛热加载
    MASTERY_THRESHOLD      = ReadIniFloat("Thresholds", "MasteryThreshold", 60.0f, iniPath.c_str());
    STRENGTH_GAP_THRESHOLD = ReadIniFloat("Thresholds", "StrengthGapThreshold", 30.0f, iniPath.c_str());
    RECOIL_DAMAGE_PER_10   = ReadIniFloat("Thresholds", "RecoilDamagePer10", 5.0f, iniPath.c_str());
    DEFENSE_MASTERY        = ReadIniFloat("Thresholds", "DefenseMastery", 105.0f, iniPath.c_str());
    COUNTER_THRESHOLD      = ReadIniFloat("Thresholds", "CounterThreshold", 100.0f, iniPath.c_str());
    HYPER_ARMOR_STR_REQ    = ReadIniFloat("Thresholds", "HyperArmorStrReq", 85.0f, iniPath.c_str());
    SHOCKWAVE_STR_REQ      = ReadIniFloat("Thresholds", "ShockwaveStrReq", 120.0f, iniPath.c_str());
    PICKPOCKET_MAX_DIST    = ReadIniFloat("Thresholds", "PickpocketMaxDist", 35.0f, iniPath.c_str());

    SynergyPerks::MASTER_KEY_LOCK     = ReadIniFloat("Synergy", "MasterKeyLock", 80.0f, iniPath.c_str());
    SynergyPerks::MASTER_KEY_ENG      = ReadIniFloat("Synergy", "MasterKeyEng", 80.0f, iniPath.c_str());
    SynergyPerks::MASTER_KEY_SCI      = ReadIniFloat("Synergy", "MasterKeySci", 80.0f, iniPath.c_str());
    SynergyPerks::SILENT_STEALTH      = ReadIniFloat("Synergy", "SilentStealth", 80.0f, iniPath.c_str());
    SynergyPerks::SILENT_ASSASSIN     = ReadIniFloat("Synergy", "SilentAssassin", 80.0f, iniPath.c_str());
    SynergyPerks::PICKPOCKET_STEALTH  = ReadIniFloat("Synergy", "PickpocketStealth", 80.0f, iniPath.c_str());
    SynergyPerks::PICKPOCKET_THIEVING = ReadIniFloat("Synergy", "PickpocketThieving", 80.0f, iniPath.c_str());
    SynergyPerks::TITAN_LABOUR        = ReadIniFloat("Synergy", "TitanLabour", 70.0f, iniPath.c_str());
    SynergyPerks::TITAN_ENG           = ReadIniFloat("Synergy", "TitanEng", 70.0f, iniPath.c_str());
    SynergyPerks::CHEF_FARM           = ReadIniFloat("Synergy", "ChefFarm", 70.0f, iniPath.c_str());
    SynergyPerks::CHEF_COOK           = ReadIniFloat("Synergy", "ChefCook", 70.0f, iniPath.c_str());
    SynergyPerks::CYBER_MEDIC         = ReadIniFloat("Synergy", "CyberMedic", 80.0f, iniPath.c_str());
    SynergyPerks::CYBER_SCI           = ReadIniFloat("Synergy", "CyberSci", 80.0f, iniPath.c_str());

    LoadHookSwitches(); // 灰度开关随配置一同加载 (注册前调用)
}

static void SaveDesolationConfig() {
    std::string iniPath = GetDesolationIniPath();
    WritePrivateProfileStringA("Settings", "TargetScope", std::to_string(g_targetScope).c_str(), iniPath.c_str());
    char spdBuf[32];
    snprintf(spdBuf, sizeof(spdBuf), "%.1f", g_maxAttackSpeedCap);
    WritePrivateProfileStringA("Settings", "AttackSpeedCap", spdBuf, iniPath.c_str());
    WritePrivateProfileStringA("Settings", "EnableAscension", g_enableAscensionPerks ? "1" : "0", iniPath.c_str());
}

static DWORD g_lastPickpocketTick   = 0;
static bool  g_vKeyLastState        = false;

// ============================================================
// 万法皆通·真伤 pending (2026-09-06): _getHit 知攻击者但不知命中部位,
// MedicalSystem::applyDamage 知部位但不知攻击者 → getHit 侧标记 250ms 窗口,
// applyDamage 侧对本次实际受击部位追加 10% 最大生命真伤。
// ============================================================
struct MartialTruePending { Character* victim; DWORD until; };
static MartialTruePending g_martialTrue[4] = {};
static int g_martialTrueRobin = 0;

static inline void MartialTrueMark(Character* victim) {
    if (!victim) return;
    g_martialTrue[g_martialTrueRobin++ % 4] = { victim, GetTickCount() + 250 };
}
static inline bool MartialTrueConsume(Character* victim) {
    if (!victim) return false;
    DWORD now = GetTickCount();
    for (int i = 0; i < 4; ++i) {
        if (g_martialTrue[i].victim == victim && (int)(g_martialTrue[i].until - now) > 0) {
            g_martialTrue[i].until = 0;
            return true;
        }
    }
    return false;
}

// ============================================================
// 逐 Hook 灰度开关 (P2 治理, INI [Hooks] 段)
// INI: [Hooks]  key=0 表示禁用该 Hook (key = hook 函数名, 如 getStat_hook)
// 说明: 开关在插件启动注册时生效, 需重启游戏; F10 热重载不影响已注册 Hook
// ============================================================
static char g_disabledHookKeys[64][48] = { {0} };
static int  g_disabledHookCount = 0;

static void LoadHookSwitches() {
    g_disabledHookCount = 0;
    std::string iniPath = GetDesolationIniPath();
    char sectionBuf[4096] = { 0 };
    GetPrivateProfileSectionA("Hooks", sectionBuf, sizeof(sectionBuf), iniPath.c_str());
    const char* p = sectionBuf;
    while (p && *p && g_disabledHookCount < 64) {
        const char* eq = strchr(p, '=');
        if (eq) {
            int nameLen = (int)(eq - p);
            int val = atoi(eq + 1);
            if (val == 0 && nameLen > 0 && nameLen < 48) {
                memcpy(g_disabledHookKeys[g_disabledHookCount], p, nameLen);
                g_disabledHookKeys[g_disabledHookCount][nameLen] = '\0';
                g_disabledHookCount++;
            }
        }
        p += strlen(p) + 1;
    }
}

static bool IsHookDisabled(const char* key) {
    if (!key || !*key) return false;
    for (int i = 0; i < g_disabledHookCount; ++i) {
        if (strcmp(g_disabledHookKeys[i], key) == 0) return true;
    }
    return false;
}

static thread_local bool t_recursing_getStat = false;

