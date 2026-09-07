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

// =============================================================================
// DesolationSystem.cpp
// 废土绝境系统 (Desolation System) - 整合核心总入口
// 模块化架构：所有机制已解耦拆分至独立子头文件
// =============================================================================

#include "DesolationCommon.h"
#include "DesolationHooksABI.h"
#include "DesolationSynergy.h"
#include "DesolationRyu.h"

static void* GetKenshiLibExport(const char* name);   // fwd: 定义在 HookExport 区
#include "DesolationPerkRegistry.h"   // 专长元数据单一事实源 (M1: 建表; M2/M3 逐步接入 UI/门槛)
#include "DesolationCombatHooks.h"
#include "DesolationMedicalHooks.h"
#include "DesolationInteraction.h"

// ============================================================
// 构建版本指纹 (P0 治理 2026-09-04)
// build_desolation_system.ps1 以 /DDESO_GIT_HASH=g<短哈希> 注入；
// 未注入时回落为 unknown。产物日志可溯源到源码状态。
// ============================================================
#ifndef DESO_GIT_HASH
#define DESO_GIT_HASH unknown
#endif
#define DESO_STR_IMPL(x) #x
#define DESO_STR(x) DESO_STR_IMPL(x)
static const char* const kDesoVersionString =
    "DesolationSystem git:" DESO_STR(DESO_GIT_HASH) " built " __DATE__ " " __TIME__;

// ============================================================
// 专长门槛查表 (M3, 2026-09-06 维护降本①): 注册表 GateDef → 运行时阈值 → 判定。
// UI(DivineGraceUI) 经导出函数调用, 消灭 UI 与机制的门槛双写漂移。
// ============================================================
static float ResolveGateThreshold(const char* sec, const char* key, float def) {
    if (sec && key) {
        if (!strcmp(sec, "Synergy")) {
            if (!strcmp(key, "MasterKeyLock"))        return SynergyPerks::MASTER_KEY_LOCK;
            if (!strcmp(key, "MasterKeyEng"))         return SynergyPerks::MASTER_KEY_ENG;
            if (!strcmp(key, "MasterKeySci"))         return SynergyPerks::MASTER_KEY_SCI;
            if (!strcmp(key, "SilentStealth"))        return SynergyPerks::SILENT_STEALTH;
            if (!strcmp(key, "SilentAssassin"))       return SynergyPerks::SILENT_ASSASSIN;
            if (!strcmp(key, "PickpocketStealth"))    return SynergyPerks::PICKPOCKET_STEALTH;
            if (!strcmp(key, "PickpocketThieving"))   return SynergyPerks::PICKPOCKET_THIEVING;
            if (!strcmp(key, "TitanLabour"))          return SynergyPerks::TITAN_LABOUR;
            if (!strcmp(key, "TitanEng"))             return SynergyPerks::TITAN_ENG;
            if (!strcmp(key, "ChefFarm"))             return SynergyPerks::CHEF_FARM;
            if (!strcmp(key, "ChefCook"))             return SynergyPerks::CHEF_COOK;
            if (!strcmp(key, "CyberMedic"))           return SynergyPerks::CYBER_MEDIC;
            if (!strcmp(key, "CyberSci"))             return SynergyPerks::CYBER_SCI;
        } else if (!strcmp(sec, "Thresholds")) {
            if (!strcmp(key, "MasteryThreshold"))     return MASTERY_THRESHOLD;
            if (!strcmp(key, "DefenseMastery"))       return DEFENSE_MASTERY;
            if (!strcmp(key, "CounterThreshold"))     return COUNTER_THRESHOLD;
            if (!strcmp(key, "HyperArmorStrReq"))     return HYPER_ARMOR_STR_REQ;
            if (!strcmp(key, "ShockwaveStrReq"))      return SHOCKWAVE_STR_REQ;
            if (!strcmp(key, "StrengthGapThreshold")) return STRENGTH_GAP_THRESHOLD;
        }
    }
    return def;
}

static bool PerkGatesMet(const PerkRegistry::PerkDef& p, CharStats* s) {
    if (!s) return false;
    for (int i = 0; i < p.gateCount; ++i) {
        const PerkRegistry::GateDef& g = p.gates[i];
        float thr = ResolveGateThreshold(g.iniSection, g.iniKey, g.defThreshold);
        if (SafeGetStat(s, (StatsEnumerated)g.stat, true) < thr) return false;
    }
    return true;
}

extern "C" __declspec(dllexport) bool IsDesolationPerkGateMet(int perkId, Character* ch) {
    const PerkRegistry::PerkDef* p = PerkRegistry::Get(perkId);
    if (!p || !ch || !charGetStats_orig) return false;
    __try {
        // 体检修正 (2026-09-06): ch 由 UI 跨 DLL 传入, 可能为悬垂指针;
        // charGetStats_orig 是虚调用(读 vtable), 必须纳入 SEH, 不能只护 PerkGatesMet
        CharStats* s = charGetStats_orig(ch);
        if (!s) return false;
        return PerkGatesMet(*p, s);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ============================================================
// Hook ㉔: 主渲染循环 (GameWorld::_NV_mainLoop_GPUSensitiveStuff)
// ============================================================
// 荒原主宰·负向判畜 (用户方案 2026-09-06): 非玩家 && 非人 && 非骨人/机械种族
// && 无机械部件 && 身上无任何物品(不穿装备) => 视为动物。识别难度高的"动物正判"
// 由四条高置信负判替代 (isHuman/RaceIsRobot/hasRobotics/背包全空)。
static bool WildLooksAnimal(Character* c) {
    return DesoIsAnimal(c, true);   // 统一动物判定 (HooksABI.h): 收编档=查装备+排除玩家方
}

// 荒原主宰持久缓存 + 自动登记 (periodicUpdate 全角色上报, 不依赖选中)
static Character* s_knownWildSovereign = nullptr;
static void WildNoteCharacter(Character* ch) {
    if (!ch || s_knownWildSovereign) return;
    __try {
        if (!IsBadReadPtr(ch, 64) && HasWildernessAscension(ch)) {
            s_knownWildSovereign = ch;
            DiagnosticLog("[DesolationSystem] ★【荒原主宰】已自动识别主宰角色 (无需选中)");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void ReportShieldHits();

static void gwMainLoop_hook(GameWorld* thisptr, float time) {
    ReportShieldHits();
    // 2026-09-06 回退: 曾将 orig 包入 SEH — 引擎在区域流送期可能以内部 SEH 异常做流程
    // 控制, 我们的 __except 会抢先吞掉本该由引擎上层处理的异常, 导致状态机错乱
    // (10:46 流送期崩溃嫌疑回归点, 见 crash_reports/20260906_104702)。orig 保持原样放行。
    if (gwMainLoop_orig) {
        gwMainLoop_orig(thisptr, time);
    }
    if (!thisptr) return;

    __try {
        ForgottenGUI* gui = (g_guiPtrPtr && *g_guiPtrPtr) ? *g_guiPtrPtr : nullptr;

        HWND fgWnd = GetForegroundWindow();
        DWORD fgPid = 0;
        if (fgWnd) GetWindowThreadProcessId(fgWnd, &fgPid);
        bool isFocused = (fgPid == GetCurrentProcessId());

        // 【方案: 快捷键改由可点击UI承担, 此处禁用 F6 模式切换】
        // 改为点击控制条"模式"卡切换 (DivineGraceUI)
#if 0
        bool f6Now = isFocused && ((GetAsyncKeyState(VK_F6) & 0x8000) != 0);
        if (f6Now && !g_f6LastDown) {
            g_targetScope = (g_targetScope + 1) % 3;
            SaveDesolationConfig();
            DiagnosticLog(std::string("[DesolationSystem] 作用域已切换至: ") + 
                          (g_targetScope == 0 ? "全局模式" : (g_targetScope == 1 ? "仅玩家生效" : "仅NPC生效")) + " 并持久化至配置！");
        }
        g_f6LastDown = f6Now;
#endif

        // 【方案: F8 快捷键改由可点击UI承担(攻速/模式/极境), 此处禁用, 释放 F8 给 Divine UI 唤出键】
#if 0
        // F8 + 1..5 快捷键调节攻速上限
        bool f8Holding = isFocused && ((GetAsyncKeyState(VK_F8) & 0x8000) != 0);
        if (f8Holding) {
            bool k1 = ((GetAsyncKeyState('1') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0);
            bool k2 = ((GetAsyncKeyState('2') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD2) & 0x8000) != 0);
            bool k3 = ((GetAsyncKeyState('3') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD3) & 0x8000) != 0);
            bool k4 = ((GetAsyncKeyState('4') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD4) & 0x8000) != 0);
            bool k5 = ((GetAsyncKeyState('5') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD5) & 0x8000) != 0);

            if (k1 && !g_num1LastDown) {
                g_maxAttackSpeedCap = 1.2f;
                SaveDesolationConfig();
                DiagnosticLog("[DesolationSystem] 攻速上限切换为: 1.2 (原版攻速) 并已保存");
            }
            if (k2 && !g_num2LastDown) {
                g_maxAttackSpeedCap = 2.4f;
                SaveDesolationConfig();
                DiagnosticLog("[DesolationSystem] 攻速上限切换为: 2.4 (平稳高速) 并已保存");
            }
            if (k3 && !g_num3LastDown) {
                g_maxAttackSpeedCap = 3.6f;
                SaveDesolationConfig();
                DiagnosticLog("[DesolationSystem] 攻速上限切换为: 3.6 (疾风迅雷) 并已保存");
            }
            if (k4 && !g_num4LastDown) {
                g_maxAttackSpeedCap = 4.8f;
                SaveDesolationConfig();
                DiagnosticLog("[DesolationSystem] 攻速上限切换为: 4.8 (极境超速) 并已保存");
            }
            if (k5 && !g_num5LastDown) {
                g_maxAttackSpeedCap = 9.6f;
                SaveDesolationConfig();
                DiagnosticLog("[DesolationSystem] 攻速上限切换为: 9.6 (神魔化境) 并已保存");
            }

            g_num1LastDown = k1;
            g_num2LastDown = k2;
            g_num3LastDown = k3;
            g_num4LastDown = k4;
            g_num5LastDown = k5;
        } else {
            g_num1LastDown = false;
            g_num2LastDown = false;
            g_num3LastDown = false;
            g_num4LastDown = false;
            g_num5LastDown = false;
        }
#endif

        // 【方案: 9+0 极境切换改由可点击UI承担, 此处禁用】
#if 0
        bool key9 = ((GetAsyncKeyState('9') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD9) & 0x8000) != 0);
        bool key0 = ((GetAsyncKeyState('0') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD0) & 0x8000) != 0);
        bool ascNow = (key9 && key0);
        if (ascNow && !g_ascensionLastDown) {
            g_enableAscensionPerks = !g_enableAscensionPerks;
            DiagnosticLog(std::string("[DesolationSystem] 四大极境天道") + 
                          (g_enableAscensionPerks ? "已激活！" : "已临时关闭！"));
        }
        g_ascensionLastDown = ascNow;
#endif

        // F10 热重载所有配置与数值 (无需重启游戏)
        bool f10Now = isFocused && ((GetAsyncKeyState(VK_F10) & 0x8000) != 0);
        if (f10Now && !g_f10LastDown) {
            LoadDesolationConfig();
            DiagnosticLog("[DesolationSystem] >>> [F10] 配置与数值热重载完成！已重新从 INI 同步最新门槛与倍率 <<<");
        }
        g_f10LastDown = f10Now;

        // ★【机械至尊·背负收编与彻底重构修复】：
        // 玩家背负骨人或含机械部位角色时，自动重构修复并直接收编为玩家小队成员
        // P3 瘦身: 先做廉价检查(选中玩家+确实背着非玩家目标), 再跑昂贵的机械至尊判定
        static DWORD s_lastCarryDominationTick = 0;
        DWORD nowTick = GetTickCount();
        if (nowTick - s_lastCarryDominationTick > 500) {
            s_lastCarryDominationTick = nowTick;

            // 【荒原主宰】缓存/计数/属性同步: 独立于选中 (2026-09-07) —
            // 主宰由 periodicUpdate 全角色自动登记 (WildNoteCharacter),
            // 未选中主宰时旧逻辑缓存为空 → 计数0/buff全链失效 (本轮实测根因)
            if (s_knownWildSovereign && IsBadReadPtr(s_knownWildSovereign, 64)) s_knownWildSovereign = nullptr;
            g_wildernessChar = s_knownWildSovereign;
            g_wildPackCount = 0;
            if (g_wildernessChar && charGetPlatoon_orig) {
                ActivePlatoon* pl = charGetPlatoon_orig(g_wildernessChar);
                // characterHandles @0x80 (ActivePlatoon); HandleListBase: objects@0x8, maxSize@0x10
                void* hl = pl ? *(void**)((char*)pl + 0x80) : nullptr;
                if (hl) {
                    RootObjectBase** objs = *(RootObjectBase***)((char*)hl + 0x8);
                    unsigned maxSize = *(unsigned*)((char*)hl + 0x10);
                    if (objs && maxSize > 0) {
                        if (maxSize > 256) maxSize = 256;
                        int n = 0;
                        __try {
                            for (unsigned i = 0; i < maxSize; ++i) {
                                RootObjectBase* o = objs[i];
                                if (!o) continue;
                                Character* c = (Character*)o;
                                if (c == g_wildernessChar) continue;
                                if (DesoIsAnimal(c, false)) { if (++n >= 15) break; }
                            }
                        } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        g_wildPackCount = n;
                    }
                }
            }

            if (gui && getSelectedPlayerChar_orig && handGetChar_orig) {
                const void* pHand = getSelectedPlayerChar_orig(gui);
                Character* playerCh = handGetChar_orig(pHand);
                if (playerCh && IsApplicable(playerCh)) {
                    void* carriedHand = (void*)((char*)playerCh + 0x380);
                    Character* carriedTarget = handGetChar_orig(carriedHand);
                    if (carriedTarget && carriedTarget != playerCh && isPlayerChar_orig &&
                        !isPlayerChar_orig(carriedTarget)) {
                        // 2026-09-07 骨犬复盘: 分支内引擎调用(疑似 hasRobotics/charSetFaction)
                        // 对骨犬类目标抛异常 → 被主循环大 __try 吞掉 → 该帧之后所有逻辑
                        // (含诊断/缓存/收编) 每帧被同一异常跳过, 表现为"整体静默失效"。
                        // 各分支改独立 SEH + 异常留痕, 互不拖累。
                        __try {
                        if (HasCyberAscension(playerCh)) {
                        MedicalSystem* tMed = charGetMedical_orig ? charGetMedical_orig(carriedTarget) : nullptr;
                        bool hasRoboticElement = false;
                        if (tMed && hasRobotics_orig && hasRobotics_orig(tMed)) hasRoboticElement = true;
                        if (!hasRoboticElement && getRace_orig) {
                            void* race = getRace_orig(carriedTarget);
                            if (race && RaceIsRobot(race)) hasRoboticElement = true;
                        }
                        if (!hasRoboticElement && tMed && getPartCount_orig && getPartByIndex_orig) {
                            int pCount = getPartCount_orig(tMed);
                            if (pCount > 64) pCount = 64;   // 保护: mod 动物异常巨量部位上限
                            for (int pi = 0; pi < pCount; ++pi) {
                                void* pt = getPartByIndex_orig(tMed, pi);
                                if (pt && IsUsableObject(pt, 0x20) && PART_IS_ROBOTIC(pt)) {
                                    hasRoboticElement = true;
                                    break;
                                }
                            }
                        }

                        if (hasRoboticElement && rootGetFaction_orig && charGetPlatoon_orig && charSetFaction_orig) {
                            Faction* pFaction = rootGetFaction_orig(playerCh);
                            ActivePlatoon* pPlatoon = charGetPlatoon_orig(playerCh);
                            if (pFaction && pPlatoon) {
                                charSetFaction_orig(carriedTarget, pFaction, pPlatoon);
                                if (platoonAddChar_orig) {
                                    platoonAddChar_orig(pPlatoon, (RootObject*)carriedTarget, -1);
                                }
                                if (charHealCompletely_orig) {
                                    charHealCompletely_orig(carriedTarget);
                                }
                                if (addPortraitUpdate_orig) {
                                    addPortraitUpdate_orig(thisptr, carriedHand);
                                }
                                DiagnosticLog("[DesolationSystem] ★【机械至尊·协议重写】背负机械目标成功收编并彻底修复唤醒！");
                            }
                        }
                        }  // end cyber
                        } __except (EXCEPTION_EXECUTE_HANDLER) {
                            DiagnosticLog("[机械至尊·诊断] 收编路径抛异常(已拦截, 不影响荒原分支)");
                        }
                        // ★【荒原主宰·野性支配】：负向判畜 (非人非骨非机械无物品) 即收编入队
                        // 独立 if (非 else-if): 玩家可能同时持有机械至尊+荒原主宰, 背动物时
                        // 机械分支因目标非机械而空转, else 会短路掉荒原分支 (实测招募失效根因)
                        __try {
                        // 逐项隔离判定 (2026-09-07): 每个引擎调用各自 SEH + 全量判定值日志,
                        // 异常无论落在哪个判定都可见, 且不再阻断后续判定/收编
                        bool wildGate = false;
                        __try { wildGate = HasWildernessAscension(playerCh); }
                        __except (EXCEPTION_EXECUTE_HANDLER) { DiagnosticLog("[荒原·判定] 门判定抛异常"); }
                        // 招募判定 v2 (2026-09-07, 用户定稿): 非人 && 非"机械至尊可统御对象"
                        // => 野生血肉动物。机械判定与机械至尊同款三重标准:
                        //   hasRobotics(医疗结构) / RaceIsRobot(种族) / 逐部位 PART_IS_ROBOTIC
                        // 各步独立 SEH, 单步抛异常按"非机械"中性值继续 (mod 机械生物若因此
                        // 误入荒原, 由机械至尊分支先行收编兜底 — 两分支并行评估)。
                        bool isHumanCh = false;
                        __try { isHumanCh = deso_isHuman_fn ? (deso_isHuman_fn(carriedTarget) != nullptr) : false; }
                        __except (EXCEPTION_EXECUTE_HANDLER) {}
                        bool mechanical = false;
                        MedicalSystem* wMed = nullptr;
                        __try {
                            wMed = charGetMedical_orig ? charGetMedical_orig(carriedTarget) : nullptr;
                            mechanical = (wMed && hasRobotics_orig) ? (hasRobotics_orig(wMed) != false) : false;
                        } __except (EXCEPTION_EXECUTE_HANDLER) {
                            DiagnosticLogThrottled("WildMech", 10000, "[荒原·判定] hasRobotics 抛异常(按非机械继续)");
                        }
                        if (!mechanical && getRace_orig) {
                            __try {
                                void* rc = getRace_orig(carriedTarget);
                                mechanical = (rc && RaceIsRobot(rc));
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                        if (!mechanical && wMed && getPartCount_orig && getPartByIndex_orig) {
                            __try {
                                int pc = getPartCount_orig(wMed);
                                if (pc > 64) pc = 64;
                                for (int pi = 0; pi < pc && !mechanical; ++pi) {
                                    void* pt = getPartByIndex_orig(wMed, pi);
                                    if (pt && IsUsableObject(pt, 0x20) && PART_IS_ROBOTIC(pt)) mechanical = true;
                                }
                            } __except (EXCEPTION_EXECUTE_HANDLER) {}
                        }
                        bool negAnimal = !isHumanCh && !mechanical;
                        DiagnosticLogThrottled("WildJudge", 10000,
                            "[荒原·判定] 门=" + std::string(wildGate ? "1" : "0") +
                            " 非人=" + std::string(!isHumanCh ? "1" : "0") +
                            " 机械=" + std::string(mechanical ? "1" : "0") +
                            " 血肉动物=" + std::string(negAnimal ? "1" : "0"));
                        if (wildGate &&
                            rootGetFaction_orig && charGetPlatoon_orig && charSetFaction_orig) {
                            if (!negAnimal) {
                                DiagnosticLogThrottled("WildSkip", 10000,
                                    "[荒原主宰·诊断] 跳过支配: 非动物目标");
                            } else {
                            // 防重复闸: 同一目标 5s 内不重复执行收编 (单步抛异常时状态可能
                            // 未完全生效, 否则每帧重复 platoonAddChar 会造成小队重复成员)
                            static Character* s_lastWildTarget = nullptr;
                            static DWORD s_lastWildTick = 0;
                            DWORD wnow = GetTickCount();
                            bool wildRepeat = (carriedTarget == s_lastWildTarget && (wnow - s_lastWildTick) < 5000);
                            s_lastWildTarget = carriedTarget; s_lastWildTick = wnow;
                            // 逐步独立保护 + 步骤标记: 单步抛异常不拖累其余步骤, 日志点名元凶
                            if (!wildRepeat) {
                            Faction* pFaction = nullptr;
                            ActivePlatoon* pPlatoon = nullptr;
                            __try { pFaction = rootGetFaction_orig(playerCh); }
                            __except (EXCEPTION_EXECUTE_HANDLER) { DiagnosticLog("[荒原·步骤] rootGetFaction 抛异常"); }
                            __try { pPlatoon = charGetPlatoon_orig(playerCh); }
                            __except (EXCEPTION_EXECUTE_HANDLER) { DiagnosticLog("[荒原·步骤] charGetPlatoon 抛异常"); }
                            if (!pFaction || !pPlatoon) {
                                DiagnosticLog("[荒原主宰·诊断] 收编中止: pFaction或pPlatoon为空");
                            } else {
                                __try { charSetFaction_orig(carriedTarget, pFaction, pPlatoon); }
                                __except (EXCEPTION_EXECUTE_HANDLER) { DiagnosticLog("[荒原·步骤] charSetFaction 抛异常"); }
                                if (platoonAddChar_orig) {
                                    __try { platoonAddChar_orig(pPlatoon, (RootObject*)carriedTarget, -1); }
                                    __except (EXCEPTION_EXECUTE_HANDLER) { DiagnosticLog("[荒原·步骤] platoonAddChar 抛异常"); }
                                }
                                if (charHealCompletely_orig) {
                                    __try { charHealCompletely_orig(carriedTarget); }
                                    __except (EXCEPTION_EXECUTE_HANDLER) { DiagnosticLog("[荒原·步骤] charHealCompletely 抛异常"); }
                                }
                                if (addPortraitUpdate_orig) {
                                    __try { addPortraitUpdate_orig(thisptr, carriedHand); }
                                    __except (EXCEPTION_EXECUTE_HANDLER) { DiagnosticLog("[荒原·步骤] addPortraitUpdate 抛异常"); }
                                }
                                DiagnosticLog("[DesolationSystem] ★【荒原主宰·野性支配】背负动物收编流程执行完毕(单步异常见上方步骤日志)！");
                            }
                            }   // !wildRepeat
                            }
                        }  // end wild if
                        } __except (EXCEPTION_EXECUTE_HANDLER) {
                            DiagnosticLog("[荒原主宰·诊断] 收编路径抛异常(已拦截, 见下一帧重试)");
                        }
                    }

                    if (carriedTarget && g_enableAscensionPerks) {
                        static DWORD s_carryDbgTick = 0;
                        DWORD ct = GetTickCount();
                        if (ct - s_carryDbgTick > 10000) {
                            s_carryDbgTick = ct;
                            DiagnosticLog("[荒原主宰·诊断] 背负中: IsAnimal=" +
                                std::to_string(DesoIsAnimal(carriedTarget, false) ? 1 : 0) +
                                " 荒原门=" + std::to_string(HasWildernessAscension(playerCh) ? 1 : 0));
                        }
                    }
                    // 【荒原主宰】选中即登记 (自动探测另由 periodicUpdate→WildNoteCharacter)
                    if (playerCh && HasWildernessAscension(playerCh)) {
                        s_knownWildSovereign = playerCh;
                    } else if (s_knownWildSovereign && IsBadReadPtr(s_knownWildSovereign, 64)) {
                        s_knownWildSovereign = nullptr;   // 悬垂自愈 (角色卸载/死亡回收)
                    }
                    if (!HasWildernessAscension(playerCh) && g_enableAscensionPerks) {
                        // 门诊断 (30s 节流): 六项基哪项一目了然
                        CharStats* wst = charGetStats_orig ? charGetStats_orig(playerCh) : nullptr;
                        if (wst) DiagnosticLogThrottled("WildGate", 30000,
                            "[荒原主宰] 门未过: 医疗=" + std::to_string((int)SafeGetStat(wst, STAT_MEDIC, true)) +
                            " 感知=" + std::to_string((int)SafeGetStat(wst, STAT_PERCEPTION, true)) +
                            " 烹饪=" + std::to_string((int)SafeGetStat(wst, STAT_COOKING, true)) +
                            " 农业=" + std::to_string((int)SafeGetStat(wst, STAT_FARMING, true)) +
                            " 科学=" + std::to_string((int)SafeGetStat(wst, STAT_SCIENCE, true)) +
                            " 力量=" + std::to_string((int)SafeGetStat(wst, STAT_STRENGTH, true)));
                    }
                    DiagnosticLogThrottled("WildCache", 30000,
                        "[荒原主宰·诊断] 主宰已缓存, 队内动物计数=" + std::to_string(g_wildPackCount));
                }
            }
        }

        bool vNow = (GetAsyncKeyState('V') & 0x8000) != 0;
        if (vNow && !g_vKeyLastState) {
            DWORD now = GetTickCount();
            if (now - g_lastPickpocketTick > 200) {
                g_lastPickpocketTick = now;
                ExecutePhantomPickpocket(thisptr, gui);
            }
        }
        g_vKeyLastState = vNow;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

extern "C" __declspec(dllexport) void RyuSetStyle(void* ch, int style) {
    RyuSet((Character*)ch, style, true);   // UI 显式切换 → 持久化
}
extern "C" __declspec(dllexport) int RyuGetStyle(void* ch) {
    return RyuGet((Character*)ch);
}

static int g_hooksRegistered = 0;   // P2 治理: 实际注册成功的 Hook 数 (日志动态计数)

// ============================================================
// 寻路崩溃盾 (2026-09-06): dump 伪栈证实反复同址崩溃 kenshi+0x3B984C 发生在引擎
// NavMesh 工作线程 (resolvePoint/_NV_threadProc, Havok 向量数学), 栈上无任何插件帧 —
// 纯引擎寻路竞态。VEH 第一处理器精确匹配该读违例签名, RIP 转到 ret 桩:
// 该次寻路计算作废(路径略劣化), 进程存活。INI [Settings] NavMeshCrashShield=0 关闭。
// ============================================================
static void* g_shieldStub = nullptr;      // xor eax,eax; ret
static uintptr_t g_gameBase = 0;

static LONG CALLBACK NavMeshCrashShield_VEH(PEXCEPTION_POINTERS ep) {
    if (!g_navShieldOn || !g_shieldStub || !g_gameBase) return EXCEPTION_CONTINUE_SEARCH;
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode != 0xC0000005) return EXCEPTION_CONTINUE_SEARCH;
    if (er->NumberParameters < 2 || er->ExceptionInformation[0] != 0) return EXCEPTION_CONTINUE_SEARCH; // 仅读违例
    uintptr_t rip = (uintptr_t)ep->ContextRecord->Rip;
    if (rip - g_gameBase != 0x3B984C) return EXCEPTION_CONTINUE_SEARCH;
    ep->ContextRecord->Rip = (DWORD64)g_shieldStub;
    InterlockedIncrement(&g_shieldHits);
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void InstallNavMeshCrashShield() {
    g_gameBase = (uintptr_t)GetModuleHandleA(nullptr);
    if (!g_gameBase) return;
    BYTE stub[] = { 0x31, 0xC0, 0xC3 };   // xor eax,eax; ret
    g_shieldStub = VirtualAlloc(nullptr, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_shieldStub) return;
    memcpy(g_shieldStub, stub, sizeof(stub));
    if (AddVectoredExceptionHandler(1 /*first*/, NavMeshCrashShield_VEH)) {
        DiagnosticLog("[DesolationSystem] 寻路崩溃盾已布防 (签名: kenshi+0x3B984C 读违例, INI NavMeshCrashShield 可关)");
    } else {
        DiagnosticLog("[DesolationSystem] 寻路崩溃盾布防失败 (AddVectoredExceptionHandler)");
    }
}

static void ReportShieldHits() {
    static LONG s_lastReported = 0;
    LONG cur = g_shieldHits;
    if (cur != s_lastReported) {
        DiagnosticLogThrottled("NavShield", 30000,
            "[寻路崩溃盾] 已拦截 " + std::to_string(cur) + " 次引擎寻路线程AV (进程存活, 路径质量略降)");
        s_lastReported = cur;
    }
}

#define HOOK_EXPORT(symName, detourFn, origPtr) HookExport(symName, (void*)&detourFn, (void**)&origPtr, #detourFn)

static bool HookExport(const char* symName, void* detour, void** orig, const char* key) {
    // P2 治理: 逐 Hook 灰度开关 (INI [Hooks] 段 key=0 禁用)
    if (key && IsHookDisabled(key)) {
        DiagnosticLog(std::string("[DesolationSystem] [Hooks] INI 中禁用, 跳过注册: ") + key);
        return false;
    }
    void* pTarget = GetKenshiLibExport(symName);
    if (!pTarget) {
        DiagnosticLog(std::string("[DesolationSystem] Missing export: ") + symName);
        return false;
    }
    intptr_t realAddr = KenshiLib::GetRealAddress(pTarget);
    void* pReal = realAddr ? (void*)realAddr : pTarget;
    KenshiLib::HookStatus st = KenshiLib::AddHook(pReal, detour, orig);
    bool ok = (st == KenshiLib::HookStatus::SUCCESS);
    if (ok) ++g_hooksRegistered;
    return ok;
}

__declspec(dllexport) void startPlugin() {
    DiagnosticLog(std::string("[DesolationSystem] === ") + kDesoVersionString + " ===");
    DiagnosticLog("[DesolationSystem] Initializing Unified Desolation System (27 Perks + Synergy + 4 Ascensions)...");
    LoadDesolationConfig();
    DiagnosticLog(std::string("[DesolationSystem] 已加载配置: TargetScope = ") + std::to_string(g_targetScope) + ", AttackSpeedCap = " + std::to_string(g_maxAttackSpeedCap));

    g_guiPtrPtr           = (ForgottenGUI**)GetKenshiLibExport("?gui@@3PEAVForgottenGUI@@EA");
    g_gameWorldPtrPtr     = (GameWorld**)GetKenshiLibExport("?ou@@3PEAVGameWorld@@EA");

    charGetStats_orig     = (CharGetStatsFn)GetKenshiLibExport("?getStats@Character@@QEAAPEAVCharStats@@XZ");
    charGetMedical_orig   = (CharGetMedicalFn)GetKenshiLibExport("?getMedical@Character@@QEAAPEAVMedicalSystem@@XZ");
    getPart_orig          = (GetPartFn)GetKenshiLibExport("?getPart@MedicalSystem@@QEAAPEAVHealthPartStatus@1@W4PartType@21@W4LeftRight@@@Z");
    getPartByIndex_orig   = (GetPartByIndexFn)GetKenshiLibExport("?getPart@MedicalSystem@@QEAAPEAVHealthPartStatus@1@_K@Z");
    getPartCount_orig     = (GetPartCountFn)GetKenshiLibExport("?getPartCount@MedicalSystem@@QEBAHXZ");
    getMaxBlood_orig      = (GetMaxBloodFn)GetKenshiLibExport("?getMaxBlood@MedicalSystem@@QEBAMXZ");
    isRobotic_orig        = (IsRoboticFn)GetKenshiLibExport("?isRobotic@HealthPartStatus@MedicalSystem@@QEAA_NXZ");
    hasRobotics_orig      = (HasRoboticsFn)GetKenshiLibExport("?hasRobotics@MedicalSystem@@QEBA_NXZ");
    getRace_orig          = (GetRaceFn)GetKenshiLibExport("?getRace@Character@@UEBAPEAVRaceData@@XZ");
    isAnimal_orig         = (IsAnimalFn)GetKenshiLibExport("?isAnimal@Character@@UEAAPEAVCharacterAnimal@@XZ");
    isPlayerChar_orig     = (IsPlayerCharacterFn)GetKenshiLibExport("?isPlayerCharacter@Character@@QEBA_NXZ");
    isStealthMode_orig    = (IsStealthModeFn)GetKenshiLibExport("?isStealthMode@Character@@QEBA_NXZ");
    isInCombatMode_orig   = (IsInCombatModeFn)GetKenshiLibExport("?isInCombatMode@Character@@QEBA_N_N0@Z");
    attackTarget_orig     = (AttackTargetFn)GetKenshiLibExport("?attackTarget@Character@@QEAAXPEAV1@@Z");
    charGetInventory_orig = (CharGetInventoryFn)GetKenshiLibExport("?getInventory@Character@@UEBAPEAVInventory@@XZ");
    invGetAllSections_orig= (InvGetAllSectionsFn)GetKenshiLibExport("?getAllSections@Inventory@@QEAAAEAV?$lektor@PEAVInventorySection@@@@XZ");
    invAddItem_orig       = (InvAddItemFn)GetKenshiLibExport("?addItem@Inventory@@UEAA_NPEAVItem@@H_N1@Z");
    sectionRemoveItem_orig= (SectionRemoveItemFn)GetKenshiLibExport("?removeItem@InventorySection@@QEAA_NPEAVItem@@@Z");
    sectionGetItemAt_orig = (SectionGetItemAtFn)GetKenshiLibExport("?getItemAt@InventorySection@@QEAAPEAVItem@@HH@Z");
    getCharsWithinSphere_orig = (GetCharsWithinSphereFn)GetKenshiLibExport("?getCharactersWithinSphere@GameWorld@@QEAAXAEAV?$lektor@PEAVRootObject@@@@AEBVVector3@Ogre@@MMMHHPEAVRootObject@@@Z");
    charGetPosition_orig = (GetCharPositionFn)GetKenshiLibExport("?getPosition@Character@@UEAA?AVVector3@Ogre@@XZ");
    sayALine_orig = (SayALineFn)GetKenshiLibExport("?sayALine@Character@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z");
    rootGetFaction_orig = (RootGetFactionFn)GetKenshiLibExport("?getFaction@RootObjectBase@@UEBAPEAVFaction@@XZ");
    invRefreshGui_orig = (InvRefreshGuiFn)GetKenshiLibExport("?refreshGui@Inventory@@UEAAXXZ");
    sectionIsEmpty_orig   = (SectionIsEmptyFn)GetKenshiLibExport("?isEmpty@InventorySection@@QEBA_NXZ");
    getSelectedPlayerChar_orig = (GetSelectedPlayerCharFn)GetKenshiLibExport("?getSelectedPlayerCharacter@ForgottenGUI@@QEBAAEBVhand@@XZ");
    getSelectedObject_orig= (GetSelectedObjectFn)GetKenshiLibExport("?getSelectedObject@ForgottenGUI@@QEBAAEBVhand@@XZ");
    handGetChar_orig      = (HandGetCharFn)GetKenshiLibExport("?getCharacter@hand@@QEBAPEAVCharacter@@XZ");
    healthPartApplyDmg_orig=(HealthPartApplyDmgFn)GetKenshiLibExport("?applyDamage@HealthPartStatus@MedicalSystem@@QEAAXAEBVDamages@@@Z");
    charGetPlatoon_orig   = (CharGetPlatoonFn)GetKenshiLibExport("?getPlatoon@Character@@QEBAPEAVActivePlatoon@@XZ");
    charSetFaction_orig   = (CharSetFactionFn)GetKenshiLibExport("?_NV_setFaction@Character@@QEAAXPEAVFaction@@PEAVActivePlatoon@@@Z");
    platoonAddChar_orig   = (PlatoonAddCharFn)GetKenshiLibExport("?addCharacterAt@ActivePlatoon@@QEAAXPEAVRootObject@@H@Z");
    charHealCompletely_orig=(CharHealCompletelyFn)GetKenshiLibExport("?healCompletely@Character@@QEAAXXZ");
    addPortraitUpdate_orig= (AddPortraitUpdateFn)GetKenshiLibExport("?addPortraitUpdate@GameWorld@@QEAAXAEBVhand@@@Z");
    rootGetGameData_orig  = (RootGetGameDataFn)GetKenshiLibExport("?getGameData@RootObjectBase@@UEBAPEAVGameData@@XZ");

    HOOK_EXPORT("?getStat@CharStats@@QEBAMW4StatsEnumerated@@_N@Z", getStat_hook, getStat_orig);
    HOOK_EXPORT("?isEnemy@Character@@UEAA_NPEAV1@_N@Z", isEnemy_hook, isEnemy_orig);
    HOOK_EXPORT("?stealthUpdate@Character@@QEAAXM@Z", stealthUpdate_hook, stealthUpdate_orig);
    HOOK_EXPORT("?notifyICanSeeYouSneaking@Character@@QEAAXPEAV1@VYesNoMaybe@@M@Z", notifyICanSeeYouSneaking_hook, notifyICanSeeYouSneaking_orig);
    HOOK_EXPORT("?_NV_getMovementSpeed@Character@@QEBAMXZ", charGetMovementSpeed_hook, charMoveSpeed_orig);
    HOOK_EXPORT("?spot@SpottingPeopleMgr@SensoryData@@QEAAXAEBVhand@@M@Z", spot_hook, spot_orig);
    HOOK_EXPORT("?has@SpottingPeopleMgr@SensoryData@@QEBA_NAEBVhand@@@Z", spotHas_hook, spotHas_orig);
    HOOK_EXPORT("?getMeleeAttack@CharStats@@QEBAMXZ", getMeleeAttack_hook, getMeleeAttack_orig);
    HOOK_EXPORT("?getMeleeDefence@CharStats@@QEBAM_N@Z", getMeleeDefence_hook, getMeleeDefence_orig);
    HOOK_EXPORT("?getDodge@CharStats@@QEBAM_N@Z", getDodge_hook, getDodge_orig);
    HOOK_EXPORT("?getAttackPierceDamage@CharStats@@QEAAMXZ", getAttackPierceDamage_hook, getPierceDamage_orig);
    HOOK_EXPORT("?calculateHungerMult@CharStats@@QEAAMXZ", calculateHungerMult_hook, calcHungerMult_orig);
    HOOK_EXPORT("?calculateAttackOrBlockSpeed@CharStats@@QEAAMMM_N@Z", calculateAttackOrBlockSpeed_hook, calcAttackBlockSpeed_orig);
    HOOK_EXPORT("?getCurrentWeaponLength@CharStats@@QEBAMXZ", getCurrentWeaponLength_hook, getCurrentWeaponLength_orig);
    HOOK_EXPORT("?blockState@CombatClass@@UEAA_N_N@Z", blockState_hook, blockState_orig);
    HOOK_EXPORT("?_blockHit@CombatClass@@QEAAXW4CutDirection@@AEBVDamages@@PEAVRootObject@@@Z", blockHit_hook, blockHit_orig);
    HOOK_EXPORT("?_getHit@CombatClass@@QEAAXW4CutDirection@@AEBVDamages@@PEAVRootObject@@_N@Z", getHit_hook, getHit_orig);
    HOOK_EXPORT("?attackImpactCheck@CombatClass@@QEAAXXZ", attackImpactCheck_hook, attackImpactCheck_orig);
    HOOK_EXPORT("?applyDamage@MedicalSystem@@QEAAXPEAVHealthPartStatus@1@AEBVDamages@@_N2AEBVVector3@Ogre@@@Z", applyDamage_hook, applyDamage_orig);
    HOOK_EXPORT("?_NV_periodicUpdate@MedicalSystem@@QEAAXXZ", periodicUpdate_hook, medPeriodicUpdate_orig);
    HOOK_EXPORT("?bloodlossUpdate@MedicalSystem@@QEAAXM@Z", bloodlossUpdate_hook, bloodlossUpdate_orig);
    HOOK_EXPORT("?_getRoboticsStatMult@MedicalSystem@@QEBAMW4StatsEnumerated@@@Z", getRoboticsStatMult_hook, getRoboticsStatMult_orig);
    HOOK_EXPORT("?setEquipmentStatBonuses@CharStats@@QEAAXMMHHMHMMHMMMM@Z", setEquip_hook, setEquipStatBonuses_orig);
    HOOK_EXPORT("?getLockpickChance@Character@@QEAAMPEAVDoorLock@@@Z", getLockpickChance_hook, getLockpickChance_orig);
    HOOK_EXPORT("?setCrime@BountyManager@@QEAA_NW4CrimeEnum@@PEAVFaction@@AEBVhand@@@Z", setCrime_hook, setCrime_orig);
    HOOK_EXPORT("?notifyCrimeWitnessed@BountyManager@@QEAAXPEAVFaction@@AEBVhand@@HW4CrimeEnum@@@Z", notifyCrimeWitnessed_hook, notifyCrimeWitnessed_orig);
    HOOK_EXPORT("?notifyPossibleCrimeWitnessed@BountyManager@@QEAAXM@Z", notifyPossibleCrimeWitnessed_hook, notifyPossibleCrimeWitnessed_orig);
    HOOK_EXPORT("?knockout@MedicalSystem@@QEAAXM@Z", knockout_hook, knockout_orig);
    HOOK_EXPORT("?startKnockoutTimer@MedicalSystem@@QEAAXXZ", startKnockoutTimer_hook, startKnockoutTimer_orig);
    HOOK_EXPORT("?knockoutForceTimer@MedicalSystem@@QEAAXM@Z", knockoutForceTimer_hook, knockoutForceTimer_orig);
    HOOK_EXPORT("?_NV_mainLoop_GPUSensitiveStuff@GameWorld@@QEAAXM@Z", gwMainLoop_hook, gwMainLoop_orig);
    HOOK_EXPORT("?_NV_updateStats@MedicalSystem@@QEAAXXZ", medUpdateStats_hook, medUpdateStats_orig);
    HOOK_EXPORT("?maxHealth@HealthPartStatus@MedicalSystem@@QEBAMXZ", healthPartMaxHealth_hook, healthPartMaxHealth_orig);
    HOOK_EXPORT("?getStealthSkill01@CharStats@@QEBAM_N@Z", getStealthSkill01_hook, getStealthSkill01_orig);
    HOOK_EXPORT("?getStealthKOChance@Character@@QEAAMPEAV1@_N@Z", getStealthKOChance_hook, getStealthKOChance_orig);
    HOOK_EXPORT("?setStealthMode@Character@@QEAAX_N@Z", setStealthMode_hook, setStealthMode_orig);
    HOOK_EXPORT("?calculateReloadTime@GunClass@@QEAAMM@Z", gunReload_hook, gunReloadTime_orig); // 弩装填(模块㉕)
    HOOK_EXPORT("?shoot@GunClass@@QEAAXPEAVCharacter@@PEAVRootObject@@W4StatsEnumerated@@AEBVVector3@Ogre@@@Z",
                gunShoot_hook, gunShoot_orig); // 弩伤害三段(模块㉗)

    // —— 本次新增三件弩机制的就位诊断 (用于启动后判断是"没挂载"还是"运行期崩") ——
    {
        std::string g;
        g += "[弩模块] reload="; g += (gunReloadTime_orig ? "OK" : "MISSING");
        g += " shoot=";   g += (gunShoot_orig   ? "OK" : "MISSING");
        g += " pierce(并入破甲hook)="; g += "enabled";
        g += " | 总注册钩子="; g += std::to_string(g_hooksRegistered);
        DiagnosticLog(g);
    }

    deso_isHuman_fn = (DesoIsHumanFn)GetKenshiLibExport("?isHuman@Character@@UEAAPEAVCharacterHuman@@XZ");
    RyuLoadIni();
    InstallNavMeshCrashShield();
    DiagnosticLog(std::string("[DesolationSystem] All ") + std::to_string(g_hooksRegistered) + " Hooks registered; Synergy System initialized successfully!");
}

