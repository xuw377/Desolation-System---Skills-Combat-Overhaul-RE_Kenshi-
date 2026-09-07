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

// 双语字符串宏 (与 DivineGraceUI 相同的编译开关; ifndef 防重复定义)
#ifndef L_
#  ifdef DESO_ENGLISH
#    define L_(zh, en) en
#  else
#    define L_(zh, en) zh
#  endif
#endif

// ============================================================
// 【妙手空空】联合专长执行体 (潜行>=80 && 偷窃>=80)
// ============================================================
static void ExecutePhantomPickpocket(GameWorld* gw, ForgottenGUI* gui) {
    __try {
        if (!gui || !gw || !getSelectedPlayerChar_orig || !handGetChar_orig || !charGetStats_orig) {
            DiagnosticLog("[妙手空空] 执行失败：引擎关键接口指针未初始化！");
            return;
        }

        const void* playerHand = getSelectedPlayerChar_orig(gui);
        Character* playerChar = handGetChar_orig(playerHand);
        if (!playerChar || IsAnimal(playerChar) || !IsApplicable(playerChar)) {
            DiagnosticLog("[妙手空空] 执行失败：当前未选定有效的玩家可控角色！");
            return;
        }

        CharStats* pStats = charGetStats_orig(playerChar);
        if (!pStats) {
            DiagnosticLog("[妙手空空] 执行失败：无法获取玩家角色属性！");
            return;
        }

        float pStealth = SafeGetStat(pStats, STAT_STEALTH, true);
        float pThieving = SafeGetStat(pStats, STAT_THIEVING, true);

        // ★【妙手空空】：潜行 >= 80 && 偷窃 >= 80
        if (!HasPickpocketSynergy(pStats)) {
            DiagnosticLog(std::string("[妙手空空] 门槛未达成：当前角色 潜行=") + std::to_string((int)pStealth) + 
                          ", 偷窃=" + std::to_string((int)pThieving) + " (需要两者均 >= 80)");
            return;
        }

        if (isStealthMode_orig && !isStealthMode_orig(playerChar)) {
            DiagnosticLog("[妙手空空] 失败：角色未处于潜行姿态！(请先按潜行键进入潜行模式)");
            return;
        }

        if (isInCombatMode_orig && isInCombatMode_orig(playerChar, false, false)) {
            DiagnosticLog("[妙手空空] 失败：角色正处于交战状态，无法在刀光剑影中探囊取物！");
            return;
        }

        SimpleVec3 playerPos(0.0f, 0.0f, 0.0f);
        if (charGetPosition_orig) {
            charGetPosition_orig(playerChar, &playerPos);
        } else {
            playerPos = *(SimpleVec3*)((char*)playerChar + 0x48);
        }

        Character* targetChar = nullptr;
        // 1. 优先捕获玩家鼠标左键选中的对象
        if (getSelectedObject_orig) {
            const void* objHand = getSelectedObject_orig(gui);
            Character* sel = handGetChar_orig(objHand);
            if (sel && sel != playerChar) {
                targetChar = sel;
            }
        }

        // 2. 若未手动选中目标，则在身前范围内自动寻找最近的 NPC
        if (!targetChar && getCharsWithinSphere_orig) {
            lektor<RootObject*> nearby;
            getCharsWithinSphere_orig(gw, nearby, playerPos, PICKPOCKET_MAX_DIST, PICKPOCKET_MAX_DIST, 0.0f, 20, 20, (RootObject*)playerChar);
            float bestDistSq = PICKPOCKET_MAX_DIST * PICKPOCKET_MAX_DIST;
            for (uint32_t i = 0; i < nearby.size(); ++i) {
                Character* cand = (Character*)nearby[i];
                if (!cand || cand == playerChar || IsAnimal(cand)) continue;
                if (isPlayerChar_orig && isPlayerChar_orig(cand)) continue;

                SimpleVec3 candPos(0.0f, 0.0f, 0.0f);
                if (charGetPosition_orig) {
                    charGetPosition_orig(cand, &candPos);
                } else {
                    candPos = *(SimpleVec3*)((char*)cand + 0x48);
                }
                float dx = candPos.x - playerPos.x;
                float dy = candPos.y - playerPos.y;
                float dz = candPos.z - playerPos.z;
                float d2 = dx*dx + dy*dy + dz*dz;
                if (d2 < bestDistSq) {
                    bestDistSq = d2;
                    targetChar = cand;
                }
            }
        }

        if (!targetChar) {
            DiagnosticLog("[妙手空空] 贴身范围内未找到有效目标！(请贴近目标并鼠标左键选中敌人，距离需 <= " + std::to_string((int)PICKPOCKET_MAX_DIST) + ")");
            return;
        }

        // 校验目标距离
        SimpleVec3 targetPos(0.0f, 0.0f, 0.0f);
        if (charGetPosition_orig) {
            charGetPosition_orig(targetChar, &targetPos);
        } else {
            targetPos = *(SimpleVec3*)((char*)targetChar + 0x48);
        }
        float dx = targetPos.x - playerPos.x;
        float dy = targetPos.y - playerPos.y;
        float dz = targetPos.z - playerPos.z;
        float distSq = dx*dx + dy*dy + dz*dz;
        if (distSq > (PICKPOCKET_MAX_DIST * PICKPOCKET_MAX_DIST)) {
            DiagnosticLog(std::string("[妙手空空] 距离过远！当前距离约 ") + std::to_string((int)std::sqrt(distSq)) + 
                          " (贴身判定最大 " + std::to_string((int)PICKPOCKET_MAX_DIST) + ")");
            return;
        }

        if (isPlayerChar_orig && isPlayerChar_orig(targetChar)) {
            DiagnosticLog("[妙手空空] 不能盗窃己方小队成员！");
            return;
        }

        if (!charGetInventory_orig || !invGetAllSections_orig || !invAddItem_orig || !sectionRemoveItem_orig || !sectionGetItemAt_orig) {
            DiagnosticLog("[妙手空空] 错误：背包操作 API 符号缺失！");
            return;
        }

        Inventory* targetInv = charGetInventory_orig(targetChar);
        Inventory* playerInv = charGetInventory_orig(playerChar);
        if (!targetInv || !playerInv) {
            DiagnosticLog("[妙手空空] 错误：目标或自身背包指针为空！");
            return;
        }

        CharStats* tStats = charGetStats_orig ? charGetStats_orig(targetChar) : nullptr;
        float tPerception = tStats ? SafeGetStat(tStats, STAT_PERCEPTION, true) : 30.0f;

        // 博弈判定: 幽冥主宰锁定 99%，宗师基于公式动态博弈
        float successChance = 80.0f + (pThieving + pStealth) * 0.25f - (tPerception * 0.5f);
        if (HasShadowAscension(playerChar)) {
            successChance = 99.0f;
        } else {
            if (successChance < 15.0f) successChance = 15.0f;
            if (successChance > 95.0f) successChance = 95.0f;
        }

        float roll = (float)(rand() % 10000) / 100.0f;
        bool isSuccess = (roll <= successChance);

        if (!isSuccess) {
            DiagnosticLog(std::string("[妙手空空] 糟糕！盗窃被目标当场察觉并抓包！(骰子: ") + std::to_string((int)roll) + 
                          "%, 成功率: " + std::to_string((int)successChance) + "%)");
            // 抓包惩罚
            if (setStealthMode_orig) setStealthMode_orig(playerChar, false);
            if (sayALine_orig) sayALine_orig(targetChar, L_("抓小偷！有人偷东西！", "Thief! Someone's stealing!"), true);
            if (attackTarget_orig) attackTarget_orig(targetChar, playerChar);
            if (setCrime_orig && rootGetFaction_orig) {
                Faction* tFaction = rootGetFaction_orig(targetChar);
                const void* pHnd = (const void*)((char*)playerChar + 0x58);
                void* bountyMgr = (void*)((char*)playerChar + 0xF0);
                setCrime_orig(bountyMgr, 3 /* CRIME_STEALING */, tFaction, pHnd);
            }
            return;
        }

        DiagnosticLog("[妙手空空] 开始执行探囊取物流程...");

        // 从目标背包中顺走 1 件物品
        lektor<InventorySection*>& sections = *(lektor<InventorySection*>*)invGetAllSections_orig(targetInv);
        Item* stolenItem = nullptr;
        InventorySection* stolenSection = nullptr;

        for (uint32_t sIdx = 0; sIdx < sections.size(); ++sIdx) {
            InventorySection* sec = sections[sIdx];
            if (!sec) continue;
            if (sectionIsEmpty_orig && sectionIsEmpty_orig(sec)) continue;

            for (int y = 0; y < 12 && !stolenItem; ++y) {
                for (int x = 0; x < 12; ++x) {
                    Item* it = sectionGetItemAt_orig(sec, x, y);
                    if (it) {
                        stolenItem = it;
                        stolenSection = sec;
                        break;
                    }
                }
            }
            if (stolenItem) break;
        }

        if (!stolenItem || !stolenSection) {
            DiagnosticLog("[妙手空空] 目标身上没有任何可偷取的随身物品！");
            return;
        }

        bool removed = sectionRemoveItem_orig(stolenSection, stolenItem);
        if (removed) {
            bool added = invAddItem_orig(playerInv, stolenItem, 1, false, false);
            if (!added) {
                // 背包无剩余空间，掉落地上
                invAddItem_orig(playerInv, stolenItem, 1, true, false);
                DiagnosticLog("[妙手空空] ★ 神不知鬼不觉窃得 1 件战利品！(自身背包已满，物品已安全掉落在脚边)");
            } else {
                DiagnosticLog("[妙手空空] ★ 神不知鬼不觉！探囊取物成功窃得 1 件战利品！");
            }
            if (invRefreshGui_orig) {
                invRefreshGui_orig(playerInv);
                invRefreshGui_orig(targetInv);
            }
        } else {
            DiagnosticLog("[妙手空空] 物品取出失败，目标紧紧护住了随身财物！");
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DiagnosticLog("[妙手空空] 执行异常已安全隔离。");
    }
}

