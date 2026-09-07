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

// ============================================================
// 联合专长 (Synergy Perks) 判定函数
// ============================================================

// 1. 【万能钥匙 / 逻辑万能钥】：开锁 >= 80 && 建筑 >= 80 && 科学 >= 80
static inline bool HasMasterKeySynergy(CharStats* s) {
    if (!s) return false;
    return (SafeGetStat(s, STAT_LOCKPICKING, true) >= SynergyPerks::MASTER_KEY_LOCK &&
            SafeGetStat(s, STAT_ENGINEERING, true) >= SynergyPerks::MASTER_KEY_ENG &&
            SafeGetStat(s, STAT_SCIENCE, true)     >= SynergyPerks::MASTER_KEY_SCI);
}

// 2. 【静谧之触】：潜行 >= 80 && 暗杀 >= 80
static inline bool HasSilentTouchSynergy(CharStats* s) {
    if (!s) return false;
    return (SafeGetStat(s, STAT_STEALTH, true)      >= SynergyPerks::SILENT_STEALTH &&
            SafeGetStat(s, STAT_ASSASSINATION, true) >= SynergyPerks::SILENT_ASSASSIN);
}

// 3. 【妙手空空】：潜行 >= 80 && 偷窃 >= 80
static inline bool HasPickpocketSynergy(CharStats* s) {
    if (!s) return false;
    return (SafeGetStat(s, STAT_STEALTH, true)  >= SynergyPerks::PICKPOCKET_STEALTH &&
            SafeGetStat(s, STAT_THIEVING, true) >= SynergyPerks::PICKPOCKET_THIEVING);
}

// 4. 【搬运宗师】：劳作 >= 70 && 建筑 >= 70
static inline bool HasTitanCarrierSynergy(CharStats* s) {
    if (!s) return false;
    return (SafeGetStat(s, STAT_LABOURING, true)   >= SynergyPerks::TITAN_LABOUR &&
            SafeGetStat(s, STAT_ENGINEERING, true) >= SynergyPerks::TITAN_ENG);
}

// 5. 【战地灵粮 / 战地主厨】：农业 >= 70 && 烹饪 >= 70
static inline bool HasFieldChefSynergy(CharStats* s) {
    if (!s) return false;
    return (SafeGetStat(s, STAT_FARMING, true) >= SynergyPerks::CHEF_FARM &&
            SafeGetStat(s, STAT_COOKING, true) >= SynergyPerks::CHEF_COOK);
}

// 6. 【仿生义肢调校】：医疗 >= 80 && 科学 >= 80
static inline bool HasCyberTuningSynergy(CharStats* s) {
    if (!s) return false;
    return (SafeGetStat(s, STAT_MEDIC, true)   >= SynergyPerks::CYBER_MEDIC &&
            SafeGetStat(s, STAT_SCIENCE, true) >= SynergyPerks::CYBER_SCI);
}

// ============================================================
// 四大极境天道 准入判定 (门槛 >= 90.0f, AND 门)
// ============================================================
// 玩家判定安全封装: 载入存档期半初始化角色上调用 isPlayerChar 可能 AV, 独立 SEH 包裹
static inline bool IsPlayerCharacterSafely(Character* ch) {
    if (!isPlayerChar_orig) return false;
    __try {
        return isPlayerChar_orig(ch) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static inline bool CanApplyAscension(Character* ch) {
    if (!g_enableAscensionPerks || !ch || IsAnimal(ch) || !IsApplicable(ch)) return false;
    // 规格准则 1: 四大极境仅对玩家角色生效 —— 与 TargetScope 无关, scope=0 时也不放行 NPC
    if (!IsPlayerCharacterSafely(ch)) return false;
    return true;
}

static inline bool HasPhysicalAscension(Character* ch) {
    if (!CanApplyAscension(ch)) return false;
    if (!charGetStats_orig) return false;
    CharStats* s = charGetStats_orig(ch);
    if (!s) return false;
    return (SafeGetStat(s, STAT_STRENGTH, true) >= 90.0f &&
            SafeGetStat(s, STAT_TOUGHNESS, true) >= 90.0f &&
            SafeGetStat(s, STAT_DEXTERITY, true) >= 90.0f &&
            SafeGetStat(s, STAT_ATHLETICS, true) >= 90.0f &&
            SafeGetStat(s, STAT_SWIMMING, true) >= 90.0f &&
            SafeGetStat(s, STAT_LABOURING, true) >= 90.0f);
}

static inline bool HasMartialAscension(Character* ch) {
    if (!CanApplyAscension(ch)) return false;
    if (!charGetStats_orig) return false;
    CharStats* s = charGetStats_orig(ch);
    if (!s) return false;
    if (SafeGetStat(s, STAT_MELEE_ATTACK, true) < 90.0f ||
        SafeGetStat(s, STAT_MELEE_DEFENCE, true) < 90.0f ||
        SafeGetStat(s, STAT_MARTIALARTS, true) < 90.0f ||
        SafeGetStat(s, STAT_DODGE, true) < 90.0f) return false;

    int wep90Count = 0;
    if (SafeGetStat(s, STAT_KATANAS, true) >= 90.0f) wep90Count++;
    if (SafeGetStat(s, STAT_SABRES, true) >= 90.0f) wep90Count++;
    if (SafeGetStat(s, STAT_HACKERS, true) >= 90.0f) wep90Count++;
    if (SafeGetStat(s, STAT_HEAVYWEAPONS, true) >= 90.0f) wep90Count++;
    if (SafeGetStat(s, STAT_BLUNT, true) >= 90.0f) wep90Count++;
    if (SafeGetStat(s, STAT_POLEARMS, true) >= 90.0f) wep90Count++;
    if (SafeGetStat(s, STAT_CROSSBOWS, true) >= 90.0f) wep90Count++;
    if (SafeGetStat(s, STAT_TURRETS, true) >= 90.0f) wep90Count++;
    return wep90Count >= 3;
}

// ============================================================
// 荒原主宰 (第五天道, 2026-09-06): 野性支配/兽群同调/兽群庇护/野性威压
// ============================================================
// 缓存的单例主宰 (gwMainLoop 每 500ms 刷新): 供 isEnemy 热路径指针快查与兽群计数
static Character* g_wildernessChar = nullptr;
static int        g_wildPackCount  = 0;      // 队内动物数 (上限15, 500ms 缓存)

static inline bool HasWildernessAscension(Character* ch) {
    if (!CanApplyAscension(ch)) return false;
    if (!charGetStats_orig) return false;
    CharStats* s = charGetStats_orig(ch);
    if (!s) return false;
    return (SafeGetStat(s, STAT_MEDIC, true)      >= 90.0f &&
            SafeGetStat(s, STAT_PERCEPTION, true) >= 90.0f &&
            SafeGetStat(s, STAT_COOKING, true)    >= 90.0f &&
            SafeGetStat(s, STAT_FARMING, true)    >= 90.0f &&
            SafeGetStat(s, STAT_SCIENCE, true)    >= 90.0f &&
            SafeGetStat(s, STAT_STRENGTH, true)   >= 90.0f);
}

static inline bool HasShadowAscension(Character* ch) {
    if (!CanApplyAscension(ch)) return false;
    if (!charGetStats_orig) return false;
    CharStats* s = charGetStats_orig(ch);
    if (!s) return false;
    return (SafeGetStat(s, STAT_STEALTH, true) >= 90.0f &&
            SafeGetStat(s, STAT_ASSASSINATION, true) >= 90.0f &&
            SafeGetStat(s, STAT_THIEVING, true) >= 90.0f &&
            SafeGetStat(s, STAT_LOCKPICKING, true) >= 90.0f &&
            SafeGetStat(s, STAT_PERCEPTION, true) >= 90.0f);
}

static inline bool HasCyberAscension(Character* ch) {
    if (!CanApplyAscension(ch)) return false;
    if (!charGetStats_orig) return false;
    CharStats* s = charGetStats_orig(ch);
    if (!s) return false;
    return (SafeGetStat(s, STAT_SCIENCE, true) >= 90.0f &&
            SafeGetStat(s, STAT_ROBOTICS, true) >= 90.0f &&
            SafeGetStat(s, STAT_SMITHING_WEAPON, true) >= 90.0f &&
            SafeGetStat(s, STAT_SMITHING_ARMOUR, true) >= 90.0f &&
            SafeGetStat(s, STAT_SMITHING_BOW, true) >= 90.0f);
}

extern "C" __declspec(dllexport) int GetDesolationPackCount() {
    return g_wildPackCount;
}
extern "C" __declspec(dllexport) bool GetAscensionPerksEnabled() {
    return g_enableAscensionPerks;
}
extern "C" __declspec(dllexport) void SetAscensionPerksEnabled(bool enabled) {
    g_enableAscensionPerks = enabled;
    SaveDesolationConfig();   // 2026-09-07: 落盘 — 此前只改内存, 重启后极境总是回到 INI 旧值
}
extern "C" __declspec(dllexport) int GetDesolationScopeMode() {
    return g_targetScope;
}
extern "C" __declspec(dllexport) void SetDesolationScopeMode(int mode) {
    g_targetScope = mode % 3;
    SaveDesolationConfig();
}
extern "C" __declspec(dllexport) float GetDesolationAttackSpeedCap() {
    return g_maxAttackSpeedCap;
}
extern "C" __declspec(dllexport) void SetDesolationAttackSpeedCap(float cap) {
    g_maxAttackSpeedCap = cap;
    SaveDesolationConfig();
}

