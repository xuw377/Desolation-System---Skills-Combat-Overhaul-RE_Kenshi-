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
// 流法系统 (Ryu, 2026-09-07): 近战攻击>=60 解锁, 三种流派显式切换
//   疾风流(0): 攻速+20% 伤害-20% (下限: 伤害最多-90%)   虐菜
//   中庸流(1): 攻速+10% 伤害+20%                        均衡
//   重击流(2): 攻速-20%(下限最多-45%) 伤害+40% 穿甲+20%  破甲
// 60→100 按 GetMasteryScale 平滑插值(60级半效); >100 线性外推。
// 只作用于近战 (弓弩/炮塔不受影响)。NPC 近战>=60 随机分配(40/40/20)。
// 安全性: 全部运行时乘数 (getStat/攻速/伤害/穿甲四个已验证管线), 零内存写入。
// ============================================================

// 角色流法登记表 (显式切换/NPC随机后生效)
#include <map>

// 角色标识读取 (2026-09-07 安全重构):
// 通过 RootObjectBase::getGameData() 获取 GameData，安全读取 stringID 或 name。
// 仅使用指针解引用与标准 C 字符串，完全绕过跨模块 std::string 析构与 ABI 堆冲突。
static std::string RyuCharName(void* ch);

struct RyuEntry { Character* c; int style; };
static RyuEntry g_ryuTable[64] = {};
static int      g_ryuCount = 0;

static const char* RyuName(int style) {
    switch (style) {
        case 0:  return "疾风流";
        case 1:  return "中庸流";
        case 2:  return "重击流";
        default: return "无";
    }
}

static std::string GetRyuIniPath() {
    char e[MAX_PATH] = { 0 }; GetModuleFileNameA(NULL, e, MAX_PATH);
    char* sl = strrchr(e, '\\'); if (sl) *sl = 0;
    return std::string(e) + "\\mods\\DesolationSystem\\DesolationSystem.ini";
}

static void RyuPersist(void* ch, int style) {
    std::string nm = RyuCharName(ch);
    if (nm.empty() || nm.size() > 60) return;
    WritePrivateProfileStringA("Ryu", nm.c_str(), std::to_string(style).c_str(), GetRyuIniPath().c_str());
}

static void RyuSet(Character* c, int style, bool persist) {
    if (!c || style < 0 || style > 2) return;
    for (int i = 0; i < g_ryuCount; ++i) {
        if (g_ryuTable[i].c == c) { g_ryuTable[i].style = style; if (persist) RyuPersist(c, style); return; }
    }
    if (g_ryuCount < 64) { g_ryuTable[g_ryuCount].c = c; g_ryuTable[g_ryuCount].style = style; ++g_ryuCount; }
    else g_ryuTable[rand() % 64] = { c, style };
    if (persist) RyuPersist(c, style);
}

static void RyuForget(Character* c) {
    for (int i = 0; i < g_ryuCount; ++i) {
        if (g_ryuTable[i].c == c) { g_ryuTable[i] = g_ryuTable[g_ryuCount - 1]; --g_ryuCount; return; }
    }
}

static std::string RyuCharName(void* ch) {
    if (!ch) return "";
    __try {
        // 从 RootObjectBase -> GameData 安全读取角色名字/stringID
        GameData* gd = nullptr;
        if (rootGetGameData_orig) {
            gd = rootGetGameData_orig((RootObjectBase*)ch);
        } else {
            // 虚表备用: RootObjectBase::getGameData vtable offset = 0x18
            void** vtbl = *(void***)ch;
            if (vtbl && !IsBadReadPtr(vtbl, 0x20)) {
                typedef GameData* (*GetGDFn)(void*);
                GetGDFn fn = (GetGDFn)vtbl[3]; // 0x18 / 8 = 3
                if (fn) gd = fn(ch);
            }
        }
        if (!gd || IsBadReadPtr(gd, sizeof(void*) * 8)) return "";
        
        // GameData 中的 name 位于 offset 0x28 (MSVC std::string)
        // 布局 (x64): _Bx[16]@0, _Mysize@0x10, _Myres@0x18
        char* strObj = (char*)gd + 0x28;
        if (IsBadReadPtr(strObj, 0x20)) return "";
        unsigned long long size = *(unsigned long long*)(strObj + 0x10);
        unsigned long long res  = *(unsigned long long*)(strObj + 0x18);
        if (size == 0 || size > 100) return "";
        if (res <= 15) {
            return std::string(strObj, (size_t)size);
        } else {
            char* ptr = *(char**)strObj;
            if (ptr && !IsBadReadPtr(ptr, (size_t)size)) {
                return std::string(ptr, (size_t)size);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return "";
}

// ini 里的名字->流派 (启动时加载, 角色指针就绪后懒解析)
static std::map<std::string, int> g_ryuSaved;

static void RyuLoadIni() {
    static char buf[4096] = {};
    GetPrivateProfileSectionA("Ryu", buf, sizeof(buf), GetRyuIniPath().c_str());
    const char* p = buf;
    while (*p) {
        std::string kv(p);
        size_t eq = kv.find('=');
        if (eq != std::string::npos) {
            std::string name = kv.substr(0, eq);
            int st = atoi(kv.substr(eq + 1).c_str());
            if (!name.empty() && st >= 0 && st <= 2) g_ryuSaved[name] = st;
        }
        p += kv.size() + 1;
    }
}

// 未匹配到 ini 且未设置的角色, 避免每帧重复调用 rootGetGameData
static void* g_ryuNotFound[128] = {};
static int   g_ryuNotFoundCount = 0;

static int RyuGet(Character* c) {
    if (!c) return -1;
    for (int i = 0; i < g_ryuCount; ++i)
        if (g_ryuTable[i].c == c) return g_ryuTable[i].style;
    if (g_ryuSaved.empty()) return -1;

    // 查未命中缓存
    for (int i = 0; i < g_ryuNotFoundCount; ++i)
        if (g_ryuNotFound[i] == c) return -1;

    // 懒解析: 名字匹配 ini 持久值 → 提升为表项
    std::string nm = RyuCharName(c);
    if (!nm.empty()) {
        auto it = g_ryuSaved.find(nm);
        if (it != g_ryuSaved.end()) { RyuSet(c, it->second, false); return it->second; }
    }
    if (g_ryuNotFoundCount < 128) {
        g_ryuNotFound[g_ryuNotFoundCount++] = c;
    }
    return -1;
}

// 前向声明 (定义于 CombatHooks: 近战攻击等级有效值)
static float RyuMeleeAtkLevel(CharStats* stats);

// 流法乘数查询 (level=当前近战攻击等级; 返回后三个可空指针)
// unlocked=false 时全部返回 0 乘数
static void RyuMults(CharStats* stats, float* atkSpdMult, float* dmgMult, float* pierceAdd) {
    if (atkSpdMult) *atkSpdMult = 1.0f;
    if (dmgMult)    *dmgMult    = 1.0f;
    if (pierceAdd)  *pierceAdd  = 0.0f;
    if (!stats) return;
    int style = RyuGet(GetOwnerCharacter(stats));
    if (style < 0) return;
    float lv = RyuMeleeAtkLevel(stats);
    if (lv < 60.0f) return;                       // 未解锁
    float t;                                      // 60→100 归一 0..1
    if (lv <= 100.0f) t = (lv - 60.0f) / 40.0f;
    else t = 1.0f + (lv - 100.0f) / 60.0f;        // >100 线性外推 (用户下限闸保护)
    if (style == 0) {                             // 疾风流
        float spdB = 0.20f * t, dmgB = -0.20f * t;   // 100级: +20%/-20% (v2 减半)
        if (dmgB < -0.90f) dmgB = -0.90f;         // 用户定: 伤害至多-90% (不变)
        if (atkSpdMult) *atkSpdMult = 1.0f + spdB;
        if (dmgMult)    *dmgMult    = 1.0f + dmgB;
    } else if (style == 1) {                      // 中庸流
        if (atkSpdMult) *atkSpdMult = 1.0f + 0.10f * t;   // 100级: +10% (v2)
        if (dmgMult)    *dmgMult    = 1.0f + 0.20f * t;
    } else {                                      // 重击流
        float spdB = -0.20f * t, dmgB = 0.40f * t, pierce = 0.20f * t;
        if (spdB < -0.45f) spdB = -0.45f;         // 用户定: 攻速至多-45% (v2)
        if (atkSpdMult) *atkSpdMult = 1.0f + spdB;
        if (dmgMult)    *dmgMult    = 1.0f + dmgB;
        if (pierceAdd)  *pierceAdd  = pierce;
    }
}

// 近战攻击等级 (有效值; 独立小函数避免与主钩子递归保护互相干扰)
static float RyuMeleeAtkLevel(CharStats* stats) {
    if (!stats) return 0.0f;
    __try { return SafeGetStat(stats, STAT_MELEE_ATTACK, false); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
}
