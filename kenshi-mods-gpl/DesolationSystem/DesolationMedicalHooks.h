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

#include "kenshi/Damages.h"

// ============================================================
// Hook ⑬: 伤害减免 (MedicalSystem::applyDamage)
// ============================================================

// 【万法皆通·真意】受击部位真伤追加 (2026-09-06 规格修正):
// _getHit 侧已标记 250ms 窗口 (攻击者持万法皆通); 本 hook 知道本次实际受击部位,
// 在原版伤害结算完成后, 对该部位追加其 10% 最大生命的纯钝击真伤。
static void ApplyMartialTrueIfPending(void* part, Character* me) {
    if (!part || !me || !healthPartApplyDmg_orig) return;
    if (!MartialTrueConsume(me)) return;
    __try {
        if (!IsUsableObject(part, 0x60)) return;
        float maxHp = PART_MAX_HP(part);
        if (maxHp > 0.0f) {
            char trueDmgBuf[0x18] = { 0 };
            float* td = (float*)trueDmgBuf;
            td[1] = maxHp * 0.10f;
            healthPartApplyDmg_orig(part, (Damages*)trueDmgBuf);
            DiagnosticLogThrottled("MartialTrueDmg", 2000,
                "[万法皆通·真意] 受击部位真伤追加: " + std::to_string((int)(maxHp * 0.10f)));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ★【钢铁之躯】(盔甲工匠60+, 2026-09-07): 受击时 切/钝伤 -0.5%/级 (100级=+20%抗), 鱼(穿)刺伤 -0.75/级
// 挂 applyDamage 入口: 受击者=MedicalSystem 归属角色, 读其盔甲工匠等级后改写本地伤害副本
static void SteelBodyApply(Character* victim, Damages* dLocal) {
    __try {
        if (!victim) return;
        CharStats* st = charGetStats_orig ? charGetStats_orig(victim) : nullptr;
        if (!st) return;
        float aLv = SafeGetStat(st, STAT_SMITHING_ARMOUR, false);
        if (aLv < 60.0f) return;
        float cut = (aLv - 60.0f) * 0.005f;      // 100级=0.20
        if (dLocal->cut > 0.0f)  dLocal->cut  *= (1.0f - cut);
        if (dLocal->blunt > 0.0f) dLocal->blunt *= (1.0f - cut);
        float pierceCut = (aLv - 60.0f) * 0.75f; // 100级=30点
        if (dLocal->pierce > 0.0f) dLocal->pierce -= pierceCut;
        if (dLocal->pierce < 0.0f) dLocal->pierce = 0.0f;
        DiagnosticLogThrottled("SteelBody", 5000,
            "[钢铁之躯] 受击减免生效: 切钝×" + std::to_string(1.0f - cut).substr(0, 5));
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void applyDamage_hook(void* thisptr, void* part, const Damages* damage, bool a, bool b, const void* vec) {
    if (!applyDamage_orig) return;
    if (!thisptr || !part || !damage) {
        CallOrigSehV(applyDamage_orig, thisptr, part, damage, a, b, vec);
        return;
    }

    __try {
        Character* me = MedGetOwnerCharacter(thisptr);
        if (me && IsApplicable(me)) {
            bool hasPhys = HasPhysicalAscension(me);

            char localBuf[0x20] = { 0 };
            memcpy(localBuf, damage, 0x18);
            float* localDmg = (float*)localBuf;
            // ★【钢铁之躯】(盔甲工匠60+): 受击减免 (在缩放前改写本地副本)
            SteelBodyApply(me, (Damages*)localBuf);
            // 体检修正 (2026-09-06): NaN/Inf 伤害值不放大不传播, 回退原版伤害
            if (hasPhys && !(std::isfinite(localDmg[0]) && std::isfinite(localDmg[1]) && std::isfinite(localDmg[2]))) {
                CallOrigSehV(applyDamage_orig, thisptr, part, damage, a, b, vec);
                ApplyMartialTrueIfPending(part, me);
                return;
            }
            if (hasPhys) {
                localDmg[0] *= 0.90f;
                localDmg[1] *= 0.90f;
                localDmg[2] *= 0.90f;
            }

            bool isVital = (getPart_orig && (part == getPart_orig((MedicalSystem*)thisptr, 0, 0) ||
                                            part == getPart_orig((MedicalSystem*)thisptr, 0, 1) ||
                                            part == getPart_orig((MedicalSystem*)thisptr, 3, 0)));
            // 体检修正 (2026-09-06): PartType 官方枚举 TORSO=0/LEG=1/ARM=2/HEAD=3,
            // 旧写法 (0,0)(1,0)(2,0) = 躯干/左腿/左臂, 头(3,0)从未被排除 → 头部被锁血不死。
            // 要害 = 躯干胸(0,0) + 躯干腹(0,1) + 头(3,0); 四肢 = LEG/ARM × 左右。
            if (isVital && charGetStats_orig) {
                CharStats* stats = charGetStats_orig(me);
                if (stats) {
                    float defSkill = SafeGetStat(stats, STAT_MELEE_DEFENCE, true);
                    if (defSkill > MASTERY_THRESHOLD) {
                        float reduction = 1.0f + GetMasteryScale(defSkill) * 4.0f;
                        if (localDmg[0] > reduction) localDmg[0] -= reduction; else localDmg[0] = 0.0f;
                        if (localDmg[1] > reduction) localDmg[1] -= reduction; else localDmg[1] = 0.0f;
                    }
                }
            }

            // 【四肢锁血·伤害时点加固】(2026-09-06): 断肢判定发生在本次结算瞬间, 周期钳制追不上
            // 单次重击; 在伤害进引擎前按"当前HP-1.0保底"缩放四肢(非头/胸/腹)总伤, 保证结算后
            // 四肢 HP >= 1.0, 与周期兜底钳制构成双保险。总量钳制为保守近似 (实际扣血 <= 三项之和)。
            if (hasPhys && !isVital && IsUsableObject(part, 0x60)) {
                float cur = PART_FLESH_HP(part);
                float allowed = cur - 1.0f; if (allowed < 0.0f) allowed = 0.0f;
                float total = localDmg[0] + localDmg[1] + localDmg[2];
                if (total > allowed) {
                    float scale = (total > 0.0001f) ? (allowed / total) : 0.0f;
                    localDmg[0] *= scale;
                    localDmg[1] *= scale;
                    localDmg[2] *= scale;
                    DiagnosticLogThrottled("LimbClamp", 1000,
                        "[肉身成圣] 四肢锁血·伤害时点钳制: curHP=" + std::to_string(cur) +
                        " 原始总伤=" + std::to_string(total) + " → 放行=" + std::to_string(allowed));
                }
            }

            applyDamage_orig(thisptr, part, (const Damages*)localBuf, a, b, vec);
            ApplyMartialTrueIfPending(part, me);
            return;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    CallOrigSehV(applyDamage_orig, thisptr, part, damage, a, b, vec);
    ApplyMartialTrueIfPending(part, MedGetOwnerCharacter(thisptr));
}

// ============================================================
// Hook ⑮: 造血恢复 (MedicalSystem::bloodlossUpdate)
// ============================================================
static void bloodlossUpdate_hook(void* thisptr, float frameTime) {
    __try {
        Character* me = MedGetOwnerCharacter(thisptr);
        if (me && g_wildernessChar && charGetPlatoon_orig &&
            charGetPlatoon_orig(me) == charGetPlatoon_orig(g_wildernessChar) && DesoIsAnimal(me, false)) {
            // 【兽群同调】(荒原主宰): 同队动物流血速度 -50% — 临时半衰 rate 调 orig 后还原,
            // 不持久污染 MED_BLEED_RATE 字段 (每帧乘法会指数衰减到 0); 动物不走人形造血分支
            if (g_wildernessChar && charGetPlatoon_orig && bloodlossUpdate_orig &&
                charGetPlatoon_orig(me) == charGetPlatoon_orig(g_wildernessChar)) {
                float savedRate = MED_BLEED_RATE(thisptr);
                if (savedRate > 0.0f) {
                    MED_BLEED_RATE(thisptr) = savedRate * 0.5f;
                    CallOrigSehV(bloodlossUpdate_orig, thisptr, frameTime);
                    MED_BLEED_RATE(thisptr) = savedRate;
                }
            } else if (bloodlossUpdate_orig) {
                CallOrigSehV(bloodlossUpdate_orig, thisptr, frameTime);
            }
            return;
        }
        if (bloodlossUpdate_orig) {
            CallOrigSehV(bloodlossUpdate_orig, thisptr, frameTime);
        }
        if (!me || !charGetStats_orig || !IsApplicable(me)) return;
        CharStats* stats = charGetStats_orig(me);
        if (stats) {
            float medSkill = SafeGetStat(stats, STAT_MEDIC, true);
            if (medSkill > MASTERY_THRESHOLD) {
                float scale = GetMasteryScale(medSkill);
                float maxB = getMaxBlood_orig ? getMaxBlood_orig((MedicalSystem*)thisptr) : 100.0f;
                if (MED_BLOOD(thisptr) < maxB) {
                    MED_BLOOD(thisptr) += (0.05f + scale * 0.15f) * frameTime;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// Hook ⑯: 周期更新与骨人自愈 (MedicalSystem::_NV_periodicUpdate)
// ============================================================

// 【制箭·箭术行囊】(弩箭锻造 STAT_SMITHING_BOW >= 60):
// 照搬 CraftingMasterySynergy 已上架(1.0.65)的泛型实现 —— 每 ~60s 把该角色背包内任一
// ItemFunction==ITEM_AMMO(16) 堆叠数量 +2 (单叠上限200)。仅++ 已有数量、不新建 Item、不分 mod
// 箭种 → 对任何弩/弓通用且无"补 novel item/存档污染"。布局按 kenshi lektor + InventorySection:
//   lektor{count,maxSize,stuff}; InventorySection.items@0x40 = Ogre 向量(begin/end 指针),
//   element=SectionItem(内部 Item* 在 +0, 整元素 0x10) → 每 0x10 读一个 Item*。
//   ItemField: ItemFunction@0x124, quantity@0x12C (Crafting 实机常数)。全程 SEH+计数上限。
static void ArrowRestock(Character* me, CharStats* stats) {
    if (!me || !stats || !charGetInventory_orig || !invGetAllSections_orig) return;
    if (SafeGetStat(stats, STAT_SMITHING_BOW, true) < 60.0f) return;

    // 弩箭锻造等级驱动成长 (≥60):
    //   周期(每批) = max(10s, 120 - skill)秒  → 60级60s / 80级40s / 100级20s / ≥110级10s(最低)
    //   每批补量   = 2 + floor((skill-60)/10) → 60:+2 / 80:+4 / 100:+6 / 130:+9 / 200:+16 (堆叠上限 200 不动)
    float bowSkill = SafeGetStat(stats, STAT_SMITHING_BOW, true);
    if (bowSkill < 60.0f) return;
    int level = (int)bowSkill;
    int periodMs = 120 - level; if (periodMs < 10) periodMs = 10;   // 秒→最低10s
    int batch = 2 + ((level - 60) / 10); if (batch < 1) batch = 1;
    periodMs *= 1000;

    // 每角色独立计时(固定槽), 用 periodMs
    struct Arest { Character* c; DWORD t; };
    static Arest s_me[8] = {};
    static int   s_robin = 0;
    DWORD now = GetTickCount();
    bool okTime = false;
    int  hit = -1;
    for (int k = 0; k < 8; ++k) {
        if (s_me[k].c == me) { hit = k; okTime = s_me[k].t == 0 || (int)(now - s_me[k].t) >= periodMs; break; }
    }
    // 体检修正 (2026-09-06): 8 槽固定且不回收 → 第 9 个角色补箭永久失效 + 悬垂指针残留。
    // 未命中时按轮替覆写最旧槽 (地址复用致计时继承属良性), 槽位始终可服务。
    if (hit < 0) { hit = s_robin++ % 8; s_me[hit].c = me; s_me[hit].t = now; okTime = true; }
    else if (okTime) s_me[hit].t = now;
    if (!okTime) return;

    __try {
        Inventory* inv = charGetInventory_orig(me);
        if (!inv) return;

        // 1) 所有 section (lektor<InventorySection*>&)
        uint32_t allCount = 0; InventorySection** stuffArr = nullptr;
        __try {
            const void* ls = invGetAllSections_orig(inv);
            if (ls) {
                const uint32_t* hdr = (const uint32_t*)ls;
                allCount = hdr[0];                                  // count
                stuffArr = *(InventorySection***)((const char*)ls + 8); // stuff
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { allCount = 0; stuffArr = nullptr; }
        if (!stuffArr || allCount == 0) return;
        if (allCount > 24) allCount = 24;

        for (uint32_t s = 0; s < allCount; ++s) {
            InventorySection* sec = stuffArr[s];
            if (!sec) continue;
            // 2) section.items 向量: @0x40 beginPtr / @0x48 endPtr; element 0x10
            char* beginPtr = nullptr, *endPtr = nullptr;
            __try {
                beginPtr = *(char**)((char*)sec + 0x40);
                endPtr   = *(char**)((char*)sec + 0x48);
            } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
            if (!beginPtr || !endPtr || endPtr <= beginPtr) continue;
            size_t n = (size_t)(endPtr - beginPtr) / 0x10;
            if (n > 512) n = 512;
            for (size_t i = 0; i < n; ++i) {
                void* item = *(void**)((char*)beginPtr + i * 0x10);
                if (!item) continue;
                __try {
                    int func = *(int*)((char*)item + 0x124);   // ItemFunction @0x124
                    if (func != 16 /*ITEM_AMMO*/) continue;    // 非弩箭/弓弹不补
                    int* qty = (int*)((char*)item + 0x12C);    // quantity @0x12C
                    if (*qty < 200) { *qty += batch; if (*qty > 200) *qty = 200; }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
                break;   // 一节补一次即可
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void periodicUpdate_hook(void* thisptr) {
    if (medPeriodicUpdate_orig) {
        CallOrigSehV(medPeriodicUpdate_orig, thisptr);
    }
    // P3 瘦身: 内部 50% 节流 (引擎每周期都调, 我们每两次执行一次完整检查;
    // 保底/回血延迟最多 1 个引擎周期, 无感知差异, 稳态 CPU 减半)
    static int s_periodicGate = 0;
    if ((++s_periodicGate & 1) != 0) return;
    __try {
        Character* me = MedGetOwnerCharacter(thisptr);
        if (!me) return;
        WildNoteCharacter(me);   // 荒原主宰自动登记 (未选中也能发现主宰, 2026-09-07)
        // 【流法】NPC 近战>=60 自动随机分配 (40/40/20); 玩家/动物不参与。
        // 速率: 每角色自己 250ms 检查一次(配固定槽节流), 分配是一次查表+rand, 无性能压力
        {
            static DWORD s_ryuNpcTick = 0;
            DWORD rt = GetTickCount();
            if (rt - s_ryuNpcTick > 250) {
                s_ryuNpcTick = rt;
                __try {
                    if (!IsAnimal(me) && !isPlayerChar_orig(me) && IsApplicable(me)) {
                        CharStats* st = charGetStats_orig ? charGetStats_orig(me) : nullptr;
                        if (st && RyuGet(me) < 0) {
                            float lv = SafeGetStat(st, STAT_MELEE_ATTACK, false);
                            if (lv >= 60.0f) {
                                int r = rand() % 100;
                                int style = (r < 40) ? 0 : (r < 80 ? 1 : 2);
                                RyuSet(me, style, false);   // NPC 随机 → 不持久化
                                DiagnosticLogThrottled("RyuNpc", 2000,
                                    std::string("[流法] NPC 随机流派: ") + RyuName(style));
                            }
                        }
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        // 【兽群同调】(荒原主宰): 同队动物饥饿冻结 — 实测语义 (2026-09-06 截图证实):
        // hunger 250=饱 0=饿死 (字段从满往 0 衰减), 旧文档"0=饱"为颠倒误记。
        // 冻结 = 不足 250 即补满 (利维坦 3.5x 种族饥饿速率也扛不住衰减, 必须补满)。
        if (DesoIsAnimal(me, false)) {
            if (g_wildernessChar && charGetPlatoon_orig &&
                charGetPlatoon_orig(me) == charGetPlatoon_orig(g_wildernessChar)) {
                if (MED_HUNGER(thisptr) < 300.0f)
                    MED_HUNGER(thisptr) = 300.0f;   // 实测上限 300 (用户 mod 环境)
                // 【兽群同调·强化】(荒原主宰): 回复速率 +100% — 在引擎自然回复之上
                // 追加同量级再生 (每周期每血肉部位 +0.05, 血液 +1% 上限)
                if (getPartCount_orig && getPartByIndex_orig && getMaxBlood_orig) {
                    int pc = getPartCount_orig((MedicalSystem*)thisptr);
                    if (pc > 64) pc = 64;
                    for (int pi = 0; pi < pc; ++pi) {
                        void* part = getPartByIndex_orig((MedicalSystem*)thisptr, pi);
                        if (!part || !IsUsableObject(part, 0x60)) continue;
                        if (PartIsRoboticVerified(thisptr, me, part)) continue;
                        float* f = &PART_FLESH_HP(part);
                        float mx = PART_MAX_HP(part);
                        if (mx > 0.0f && *f > 0.0f && *f < mx) {
                            float step = mx * 0.0025f; if (step < 0.05f) step = 0.05f;
                            *f += step; if (*f > mx) *f = mx;
                        }
                    }
                    float mB = getMaxBlood_orig((MedicalSystem*)thisptr);
                    if (mB > 0.0f && MED_BLOOD(thisptr) > 0.0f && MED_BLOOD(thisptr) < mB) {
                        MED_BLOOD(thisptr) += mB * 0.01f;
                        if (MED_BLOOD(thisptr) > mB) MED_BLOOD(thisptr) = mB;
                    }
                }
            }
            return;
        }
        if (!charGetStats_orig || !IsApplicable(me))
            return;

        CharStats* stats = charGetStats_orig(me);
        if (!stats) return;

        bool hasPhys = HasPhysicalAscension(me);
        bool hasCyber = HasCyberAscension(me);

        // 检查角色是否为骨人种族 (P3 审计: 官方 RaceData::robot@0x7C, 替代伪 string 读取)
        bool isSkeletonRace = false;
        if (getRace_orig) {
            void* race = getRace_orig(me);
            if (race && RaceIsRobot(race)) isSkeletonRace = true;
        }

        // 肉身成圣仅对非纯骨人血肉生命体生效，骨人无血肉肢体
        if (!isSkeletonRace && hasPhys && getPartByIndex_orig && getPartCount_orig) {
            // 规格对齐 (2026-09-06): 锁血只保四肢, 要害允许负血至 -2xMaxHP 死亡线
            // 体检修正: 要害 = 头(3,0) + 躯干胸(0,0) + 躯干腹(0,1) (PartType: TORSO=0/LEG=1/ARM=2/HEAD=3)
            void* vitalHead    = getPart_orig ? getPart_orig((MedicalSystem*)thisptr, 3, 0) : nullptr;
            void* vitalChest   = getPart_orig ? getPart_orig((MedicalSystem*)thisptr, 0, 0) : nullptr;
            void* vitalStomach = getPart_orig ? getPart_orig((MedicalSystem*)thisptr, 0, 1) : nullptr;
            int count = getPartCount_orig((MedicalSystem*)thisptr);
            if (count > 64) count = 64;   // 保护: mod 动物可能 report 异常巨量部位, 限上限
            for (int i = 0; i < count; i++) {
                void* part = getPartByIndex_orig((MedicalSystem*)thisptr, i);
                if (!part) continue;
                if (part == vitalHead || part == vitalChest || part == vitalStomach) continue;   // 要害不锁血
                if (IsUsableObject(part, 0x60)) {   // 校验部位对象可达且够大, 防 mod 动物部位结构不同越界
                    float* flesh = &PART_FLESH_HP(part);
                    if (flesh && *flesh < 1.0f) *flesh = 1.0f;
                }
            }
            float maxB = getMaxBlood_orig ? getMaxBlood_orig((MedicalSystem*)thisptr) : 100.0f;
            if (MED_BLOOD(thisptr) < maxB * 0.50f) {
                MED_BLOOD(thisptr) = maxB * 0.50f;
            }
        }

        // 【肉身成圣 · 血肉回血】(用户 2026批准): 仅对血肉部位(非纯骨人/非机械义肢)每引擎周期 +1.5 血，
        // 未达到上限才补, 永不把断肢部位造出生(仅升到 health满)。含生物骨骼 +义肢除外。
        if (hasPhys && !isSkeletonRace && getPartByIndex_orig && getPartCount_orig) {
            int count = getPartCount_orig((MedicalSystem*)thisptr);
            if (count > 64) count = 64;
            for (int i = 0; i < count && i < 64; ++i) {
                void* part = getPartByIndex_orig((MedicalSystem*)thisptr, i);
                if (!part || !IsUsableObject(part, 0x60)) continue;
                if (PartIsRoboticVerified(thisptr, me, part)) continue;   // 跳过机械部位 (官方方法+校验)
                float* f = &PART_FLESH_HP(part);
                float mx = PART_MAX_HP(part);
                if (mx > 0.0f && *f < mx) { *f += 1.5f; if (*f > mx) *f = mx; }
            }
        }

        float roboSkill = SafeGetStat(stats, STAT_ROBOTICS, true);
        if (roboSkill >= 60.0f && getPartByIndex_orig && getPartCount_orig) {
            int count = getPartCount_orig((MedicalSystem*)thisptr);
            if (count > 64) count = 64;   // 保护: mod 动物异常巨量部位上限
            for (int i = 0; i < count; i++) {
                void* part = getPartByIndex_orig((MedicalSystem*)thisptr, i);
                if (!part || !IsUsableObject(part, 0x60)) continue;   // 校验部位可达且够大
                bool isRobo = isSkeletonRace ? true : PartIsRoboticVerified(thisptr, me, part);
                if (!isRobo) continue;

                if (roboSkill >= 80.0f || hasCyber) {
                    float* pWear = &PART_WEAR(part);
                    if (pWear && *pWear > 0.0f) {
                        float wearRecover = hasCyber ? 2.0f : (0.2f + (roboSkill - 80.0f) * 0.05f);
                        *pWear = (*pWear > wearRecover) ? (*pWear - wearRecover) : 0.0f;
                        DiagnosticLogThrottled("WearRepair", 2000,
                            "[纳米自愈] 磨损修复生效: 剩余磨损=" + std::to_string(*pWear) +
                            " 机器人学=" + std::to_string((int)roboSkill));
                    }
                }

                float* pFlesh = &PART_FLESH_HP(part);
                float maxHp = PART_MAX_HP(part);
                if (pFlesh && *pFlesh < maxHp) {
                    float healRate = hasCyber ? 3.0f : (0.5f + (roboSkill - 60.0f) * 0.05f);
                    *pFlesh = min(maxHp, *pFlesh + healRate);
                }
            }
        }

        // 【制箭 · Arrow Restock】: 弩箭锻造≥60 → 周期补满背包箭种数量 (60s 节流见函数内)
        ArrowRestock(me, stats);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// Hook ⑰: 义肢属性加成 (MedicalSystem::_getRoboticsStatMult)
// 包含: 【仿生义肢调校 (Cyber-Tuning)】联合专长 (医疗>=80 && 科学>=80)
// ============================================================
static float getRoboticsStatMult_hook(void* thisptr, StatsEnumerated stat) {
    if (!thisptr || !getRoboticsStatMult_orig) return 1.0f;
    float mult = CallOrigSeh(getRoboticsStatMult_orig, 1.0f, thisptr, stat);

    __try {
        Character* me = MedGetOwnerCharacter(thisptr);
        if (me && charGetStats_orig && !IsAnimal(me) && IsApplicable(me)) {
            CharStats* stats = charGetStats_orig(me);
            if (stats) {
                if (HasCyberTuningSynergy(stats)) {
                    mult *= 1.25f; // 联合专长额外 +25%
                } else {
                    float roboSkill = SafeGetStat(stats, STAT_ROBOTICS, true);
                    if (roboSkill > MASTERY_THRESHOLD) {
                        float bonus = 0.05f + GetMasteryScale(roboSkill) * 0.20f;
                        mult *= (1.0f + bonus);
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return mult;
}

// ============================================================
// Hook ⑱: 装备属性更新 (CharStats::setEquipmentStatBonuses)
// ============================================================
static void setEquip_hook(CharStats* thisptr, float a, float b, int c, int d, float e, int f, float g, float h, int i, float j, float k, float l, float m) {
    if (setEquipStatBonuses_orig) {
        CallOrigSehV(setEquipStatBonuses_orig, thisptr, a, b, c, d, e, f, g, h, i, j, k, l, m);
    }
}

// ============================================================
// Hook ⑲: 开锁概率 (Character::getLockpickChance)
// 包含: 【万能钥匙 / 逻辑万能钥】联合专长 (建筑>=80 && 科学>=80 && 开锁>=80)
// ============================================================
static float getLockpickChance_hook(Character* thisptr, void* lock) {
    if (!thisptr || !getLockpickChance_orig) return 0.0f;
    float ch = CallOrigSeh(getLockpickChance_orig, 0.0f, thisptr, lock);
    if (!IsApplicable(thisptr)) return ch;

    __try {
        if (charGetStats_orig) {
            CharStats* stats = charGetStats_orig(thisptr);
            // ★【万能钥匙】：建筑 >= 80 && 科学 >= 80 && 开锁 >= 80 瞬间秒开
            // 刻度自适应 (2026-09-06): 与 getStealthKOChance 同类 — ...Chance 原版
            // 疑为 0~100 百分比, 硬返 1.0f 可能=1%。按原版返回值判刻度。
            if (stats && HasMasterKeySynergy(stats)) {
                return (ch > 1.0f) ? 100.0f : 1.0f;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return ch;
}

// ============================================================
// Hook ⑳: 犯罪与通缉 (BountyManager::setCrime)
// 包含: 【静谧之触】联合专长 (潜行>=80 && 暗杀>=80)
// ============================================================
static bool setCrime_hook(void* thisptr, int crime, Faction* against, const void* agnst) {
    __try {
        if (g_guiPtrPtr && *g_guiPtrPtr && getSelectedPlayerChar_orig && handGetChar_orig && charGetStats_orig) {
            const void* h = getSelectedPlayerChar_orig(*g_guiPtrPtr);
            Character* playerChar = handGetChar_orig(h);
            if (playerChar && IsApplicable(playerChar)) {
                CharStats* stats = charGetStats_orig(playerChar);
                if (stats) {
                    if (HasShadowAscension(playerChar)) return false;
                    // ★【静谧之触】：潜行 >= 80 && 暗杀 >= 80 零通缉
                    if (HasSilentTouchSynergy(stats)) {
                        return false;
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return CallOrigSeh(setCrime_orig, false, thisptr, crime, against, agnst);
}

static void notifyCrimeWitnessed_hook(void* thisptr, Faction* f, const void* agnst, int crime, int bounty) {
    __try {
        if (g_guiPtrPtr && *g_guiPtrPtr && getSelectedPlayerChar_orig && handGetChar_orig && charGetStats_orig) {
            const void* h = getSelectedPlayerChar_orig(*g_guiPtrPtr);
            Character* playerChar = handGetChar_orig(h);
            if (playerChar && IsApplicable(playerChar)) {
                CharStats* stats = charGetStats_orig(playerChar);
                if (stats) {
                    if (HasShadowAscension(playerChar) || HasSilentTouchSynergy(stats)) {
                        return; // 抹除目击警报广播
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (notifyCrimeWitnessed_orig) {
        CallOrigSehV(notifyCrimeWitnessed_orig, thisptr, f, agnst, crime, bounty);
    }
}

static void notifyPossibleCrimeWitnessed_hook(void* thisptr, float time) {
    if (notifyPossibleCrimeWitnessed_orig) {
        CallOrigSehV(notifyPossibleCrimeWitnessed_orig, thisptr, time);
    }
}

// 【肉身成圣·意志死战】KO 多入口统一拦截 (2026-09-06 加固): 引擎有 3 个昏迷入口
// (knockout / startKnockoutTimer / knockoutForceTimer), 只挂其一拦截不全。命中返回 true,
// 调用方直接 return 不进 orig (引擎不进入昏迷状态机); 带限频诊断日志便于实机回归定位。
static bool AscensionSuppressKO(void* thisptr) {
    __try {
        Character* me = MedGetOwnerCharacter(thisptr);
        if (me && IsApplicable(me) && HasPhysicalAscension(me)) {
            *(float*)((char*)thisptr + 0xA0) = 0.0f;
            DiagnosticLogThrottled("KOSuppress", 1000,
                "[肉身成圣] 意志死战拦截昏迷入口 (timer→0)");
            return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

static void knockout_hook(void* thisptr, float time) {
    if (AscensionSuppressKO(thisptr)) return;
    if (knockout_orig) {
        CallOrigSehV(knockout_orig, thisptr, time);
    }
}

static void startKnockoutTimer_hook(void* thisptr) {
    if (AscensionSuppressKO(thisptr)) return;
    if (startKnockoutTimer_orig) {
        CallOrigSehV(startKnockoutTimer_orig, thisptr);
    }
}

static void knockoutForceTimer_hook(void* thisptr, float seconds) {
    if (AscensionSuppressKO(thisptr)) return;
    if (knockoutForceTimer_orig) {
        CallOrigSehV(knockoutForceTimer_orig, thisptr, seconds);
    }
}

// ============================================================
// Hook ㉑: 医疗更新与【战地灵粮】自动喂食 (MedicalSystem::_NV_updateStats)
// ============================================================
static void medUpdateStats_hook(MedicalSystem* thisptr) {
    if (medUpdateStats_orig) {
        CallOrigSehV(medUpdateStats_orig, thisptr);
    }
    __try {
        Character* me = MedGetOwnerCharacter(thisptr);
        if (me && charGetStats_orig && !IsAnimal(me) && IsApplicable(me)) {
            CharStats* stats = charGetStats_orig(me);
            // ★【战地灵粮】：农业 >= 70 && 烹饪 >= 70 自动喂食
            // 体检修正 (2026-09-06): hunger 官方语义 0=饱腹 → 250=饿死(isHungerKO/pointOfNoReturn_Hunger01),
            // 旧代码写 300 是把角色推向饿死! 现改为压回饱腹端。
            if (stats && HasFieldChefSynergy(stats)) {
                // 撤销 2026-09-06 体检批误修: 当时按"0=饱"颠倒语义压到20 (=压到濒死)。
                // 实测 250=饱/0=饿死 (见荒原兽群同调注), 喂饱 = 不足 250 补满。
                if (MED_HUNGER(thisptr) < 300.0f) {
                    MED_HUNGER(thisptr) = 300.0f;   // 实测上限 300
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static float healthPartMaxHealth_hook(void* thisptr) {
    if (!thisptr || !healthPartMaxHealth_orig) return 100.0f;
    return CallOrigSeh(healthPartMaxHealth_orig, 100.0f, thisptr);
}

static float getStealthSkill01_hook(CharStats* thisptr, bool b) {
    if (!thisptr || !getStealthSkill01_orig) return 0.0f;
    float s = CallOrigSeh(getStealthSkill01_orig, 0.0f, thisptr, b);
    __try {
        Character* ch = GetOwnerCharacter(thisptr);
        if (ch && HasShadowAscension(ch)) return 1.0f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return s;
}

static float getStealthKOChance_hook(Character* thisptr, Character* victim, bool b) {
    if (!thisptr || !getStealthKOChance_orig) return 0.0f;
    float ch = CallOrigSeh(getStealthKOChance_orig, 0.0f, thisptr, victim, b);
    __try {
        // 刻度自适应 (2026-09-06, 玩家反馈"幽冥主宰反而 1% 暗杀"): 该函数原版返回
        // 0~100 百分比而非 0~1 概率 — 硬返 1.0f 恰好 = 1% 成功率。以原版返回值
        // 判定刻度: >1.0 视为百分比刻度返 100, 否则视为 0~1 概率返 1.0。
        if (HasShadowAscension(thisptr)) return 100.0f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return ch;
}

static void setStealthMode_hook(Character* thisptr, bool stealth) {
    if (setStealthMode_orig) {
        CallOrigSehV(setStealthMode_orig, thisptr, stealth);
    }
}

// ============================================================
// Hook: 潜行状态更新 (Character::stealthUpdate) — 幽冥主宰"时刻蓝眼"
// 引擎每帧由 stealthUpdate 依 whoSeesMeSneaking 表计算 stealthUnseen@0xE8
// (YesNoMaybe: NO=蓝眼/YES=红眼/MAYBE=黄眼)。幽冥主宰潜行时每帧强制写回 NO。
// ============================================================
// ============================================================
// 兽群强化辅助: 判定"荒原主宰同队动物" (供 Character 级钩子用, thisptr 即角色)
// ============================================================
static bool IsWildPackAnimal(Character* ch) {
    __try {
        return ch && DesoIsAnimal(ch, false) && g_wildernessChar && charGetPlatoon_orig &&
               charGetPlatoon_orig(ch) == charGetPlatoon_orig(g_wildernessChar);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Character::_NV_getMovementSpeed — 动物移速的真实管线 (CharStats::getMovementSpeed
// 对动物零触发, 引擎不走)。同队动物移速 +20%。
typedef float (*CharMoveSpeedFn)(Character*);
static CharMoveSpeedFn charMoveSpeed_orig = nullptr;
static float charGetMovementSpeed_hook(Character* thisptr) {
    float spd = CallOrigSeh(charMoveSpeed_orig, 0.0f, thisptr);
    __try {
        if (IsWildPackAnimal(thisptr)) {
            spd *= 1.20f;
            DiagnosticLogThrottled("WildMoveBuff", 5000,
                "[兽群buff] Character移速放大触发: -> " + std::to_string((int)(spd * 100)));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return spd;
}

typedef void (*StealthUpdateFn)(Character*, float);
static StealthUpdateFn stealthUpdate_orig = nullptr;
static void stealthUpdate_hook(Character* thisptr, float time) {
    CallOrigSehV(stealthUpdate_orig, thisptr, time);
    __try {
        // 性能 (体检 P2): 1s 缓存幽冥判定, 免每帧每角色多次 SafeGetStat
        static DWORD s_shadowCkTick = 0;
        static Character* s_shadowCkChar = nullptr;
        static bool s_shadowCkRes = false;
        if (thisptr && (thisptr != s_shadowCkChar || GetTickCount() - s_shadowCkTick >= 1000)) {
            s_shadowCkChar = thisptr; s_shadowCkTick = GetTickCount();
            s_shadowCkRes = HasShadowAscension(thisptr);
        }
        if (thisptr && s_shadowCkRes) {
            bool sneaking = *(bool*)((char*)thisptr + 0xD4);   // stealthMode
            if (sneaking) {
                *(int*)((char*)thisptr + 0xE8) = 0;  // YesNoMaybe::NO = 蓝眼
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// Hook: 潜行发现上报 (Character::notifyICanSeeYouSneaking) — 幽冥主宰潜行时
// 无人能"看见你潜行": 丢弃上报 → 不入 whoSeesMeSneaking 表, 从源头杜绝
// "被看见→暗杀强制归零"与黄眼/红眼跃迁。(这也是幽冥"潜行脱战清仇恨"的地基)
// ============================================================
// 观察者侧发现系统 (SensoryData::SpottingPeopleMgr): NPC 用它记录"我正盯着谁"。
// 暗杀交互门槛即查目标的 has(潜行者)。幽冥主宰: spot 丢弃 + has 恒 false
// → 无论多少人盯着, 潜行暗杀交互恒开放。
typedef bool (*SpotHasFn)(void*, const void*);
static SpotHasFn spotHas_orig = nullptr;
typedef void (*SpotFn)(void*, const void*, float);
static SpotFn spot_orig = nullptr;

// hand -> Character*, 再判幽冥主宰 (带缓存短路)
static bool HandIsShadowAscended(const void* h) {
    __try {
        if (!h || !handGetChar_orig) return false;
        Character* ch = handGetChar_orig(h);
        return ch && HasShadowAscension(ch);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// 性能 (体检 P2): 按 hand 值缓存幽冥判定 1s (spot/has 热路径)
static bool HandIsShadowAscendedCached(const void* h) {
    static const void* s_h = nullptr; static DWORD s_t = 0; static bool s_r = false;
    DWORD now = GetTickCount();
    if (h != s_h || now - s_t >= 1000) {
        s_h = h; s_t = now; s_r = HandIsShadowAscended(h);
    }
    return s_r;
}

static void spot_hook(void* thisptr, const void* h, float timelimit) {
    __try {
        if (HandIsShadowAscendedCached(h)) return;   // 遁影极境: 无法被"盯上" (1s缓存)
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    CallOrigSehV(spot_orig, thisptr, h, timelimit);
}

static bool spotHas_hook(void* thisptr, const void* h) {
    __try {
        if (HandIsShadowAscendedCached(h)) return false;   // 幽冥主宰: 谁也没盯着你 (1s缓存)
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return CallOrigSeh(spotHas_orig, false, thisptr, h);
}

typedef void (*NotifySeeSneakingFn)(Character*, Character*, int, float);
static NotifySeeSneakingFn notifyICanSeeYouSneaking_orig = nullptr;
static void notifyICanSeeYouSneaking_hook(Character* thisptr, Character* who, int seeing, float progress) {
    __try {
        if (thisptr && HasShadowAscension(thisptr)) {
            bool sneaking = *(bool*)((char*)thisptr + 0xD4);
            if (sneaking) return;   // 遁影极境: 上报直接丢弃
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    CallOrigSehV(notifyICanSeeYouSneaking_orig, thisptr, who, seeing, progress);
}

