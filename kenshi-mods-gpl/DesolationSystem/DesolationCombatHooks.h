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
// 弩 (Gun / Crossbow) 特有 —— 用户 2026-09 规格, 仅"持弩/弓(WeaponCategory=BOW)"生效
//   · 装填 -30% : 敏捷§及感知 ≥80 → GunClass::calculateReloadTime ×0.70
//   · 感知破甲  : 感知>60 起; 100感知=+30%; >100 每级 +0.008 (无上限)
// Kenshi无独立 CROSSBOW WeaponCategory, 弓/弩同 SKILL_BOW(6); 故用当前武器==SKILL_BOW 判"持弩/弓",
// 炮塔(SKILL_TURRET) 与近战不享受。
// ============================================================
namespace CrossbowBoost {
    static const float DEX_SENSE_REQ   = 80.0f;
    static const float SENSE_START     = 60.0f;
    static const float SENSE_FULL      = 100.0f;
    static const float PIERCE_AT_100   = 0.30f;
    static const float PIERCE_OVER_LVL = 0.008f;
    static const float RELOAD_MULT     = 0.70f;
}
static __forceinline bool CrossbowHeldType(CharStats* s) {
    if (!s) return false;
    return STATS_WEAPON_TYPE(s) == SKILL_BOW;   // 弓/弩 category
}

// ============================================================
// 长柄压制 (Pole Suppress) —— 用户 2026-09 规格:
//   持长柄者(其攻击命中对象) 及 攻击持长柄者的敌方 其一受"攻速压低 5s(不可叠加)"。
//   减压制幅度按 UI/既有口径: 攻速- = (0.04 + scale*0.11)%, 即命中方攻速 ×(1 − 该值)。
//   做法: 不球扫/不枚举, 采用"计时表"——被压 target 命中时刻打到它自己身上,在其
//   calculateAttackOrBlockSpeed 里乘；同 target 重复只续 5s(并取更强者)不叠加。
// ============================================================
namespace PoleSuppress {
    static const int   MAX      = 16;
    static const DWORD DURATION = 5000;        // 5 秒
    struct Entry { Character* c; float mult; DWORD until; };
}
static PoleSuppress::Entry g_poleSupp[PoleSuppress::MAX] = {};
static int g_poleSuppN = 0;

// 给目标 target 打一个"攻速 debuff"标记 (由长柄命中/被打时写) —— mult 为要被乘的攻速衰减(<1)
static inline void PoleMark(Character* target, float mult) {
    if (!target) return;
    DWORD t0 = GetTickCount();
    DWORD until = t0 + PoleSuppress::DURATION;
    // 已存在 → 续期 + 取更强(更小 mult)
    for (int i = 0; i < PoleSuppress::MAX && i <= g_poleSuppN; ++i) {
        PoleSuppress::Entry& e = g_poleSupp[i];
        if (e.c == target) {
            if (mult < e.mult || !e.until) { e.mult = mult; }
            e.until = until;
            return;
        }
    }
    // 新槽
    if (g_poleSuppN < PoleSuppress::MAX) {
        int i = g_poleSuppN++;
        g_poleSupp[i].c     = target;
        g_poleSupp[i].mult  = mult;
        g_poleSupp[i].until = until;
    }
}

// 若 target 正在被 5s 压制窗内, 返回其攻速乘子 (否则返回 1; 惰性清过期)
static inline float PoleSuppressedMult(Character* target) {
    if (!target || g_poleSuppN <= 0) return 1.0f;
    DWORD now = GetTickCount();
    float found = 1.0f;
    int out = 0;
    for (int i = 0; i < g_poleSuppN; ++i) {
        PoleSuppress::Entry& e = g_poleSupp[i];
        bool live = e.c && (int)(e.until - now) > 0;
        if (live && e.c == target) { found = e.mult; }
        if (live) { g_poleSupp[out++] = e; }      // 紧凑保活
    }
    g_poleSuppN = out;
    return found;
}

// 由持长柄者 skill 推导压制幅度 mult (长柄长者 → 更弱 target 攻速) → target攻速 × mult
static inline float PoleMultForSkill(float poleSkill) {
    float scale = GetMasteryScale(poleSkill);            // 60→0,100→1,150→3(>100=1+(v-100)/25)
    float supp  = 0.04f + scale * 0.11f;
    if (supp > 0.60f) supp = 0.60f;                       // 上限防御硬顶(避免极端低速)
    float m = 1.0f - supp;
    if (m < 0.20f) m = 0.20f;
    return m;
}
// ============================================================
// Hook ①: 属性与状态 (CharStats::getStat)
// ============================================================
// ============================================================
// Hook: 野性威压 (荒原主宰) — Character::isEnemy 热路径: 动物永不与主宰敌对
// (即使主宰先动手)。首查 who==g_wildernessChar 指针相等(0成本, 99.9%直接放行),
// 命中才做动物判定; 幽冥"潜行脱战清仇恨"将来可复用本钩子模式。
// ============================================================
typedef bool (*IsEnemyFn)(Character*, Character*, bool);
static IsEnemyFn isEnemy_orig = nullptr;
static bool isEnemy_hook(Character* thisptr, Character* who, bool factorInDisguises) {
    if (!isEnemy_orig) return false;
    if (who && who == g_wildernessChar && thisptr != who) {
        static DWORD s_enDbgTick = 0;
        DWORD nt = GetTickCount();
        if (nt - s_enDbgTick > 10000) {
            s_enDbgTick = nt;
            bool animal = false;
            __try { animal = DesoIsAnimal(thisptr, false); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            DiagnosticLog("[荒原主宰·诊断] isEnemy被调, 对象是动物=" + std::string(animal ? "1" : "0"));
        }
        if (DesoIsAnimal(thisptr, false)) return false;   // 野性威压 (统一动物判定)
    }
    return CallOrigSeh(isEnemy_orig, false, thisptr, who, factorInDisguises);
}

static float getStat_hook(CharStats* thisptr, StatsEnumerated what, bool unmodified) {
    if (!thisptr || !getStat_orig) return 0.0f;
    float baseVal = CallOrigSeh(getStat_orig, 0.0f, thisptr, what, unmodified);
    const float origVal = baseVal;   // P3: 消毒回退基准

    if (t_recursing_getStat) return baseVal;
    t_recursing_getStat = true;

    __try {
        Character* ch = GetOwnerCharacter(thisptr);

        // 【兽群同调·强化】(荒原主宰): 同队动物全属性 +20% (运行时乘数, 与水中蛟龙同机制;
        // 面板数字保持原版显示 — 动物面板是引擎独立数据管线, 写存储方案已因崩溃风险废弃)
        // 性能 (体检 P2): 廉价指针检查前置, 无主宰时零判畜开销
        if (ch && g_wildernessChar && charGetPlatoon_orig &&
            charGetPlatoon_orig(ch) == charGetPlatoon_orig(g_wildernessChar) &&
            DesoIsAnimal(ch, false) && !unmodified) {
            baseVal *= 1.20f;
            return SanitizeStat(baseVal, origVal);
        }

        if (ch && !IsAnimal(ch) && IsApplicable(ch)) {
            // 【韧性·伤病惩罚减免】（移植 ToughnessSynergy; 仅在派生读+仍未满时生效, 不污染原始基准）
            // 仅战斗系属性; 若 val(已含伤病惩罚) < 未伤病 raw → 按"韧性>50 每+10→免12%, 上限85%"
            // 把该损失往回补一部分 => 韧性高=受伤后属性惩罚更轻。
            if (!unmodified) {
                bool isWoundable = (what == STAT_STRENGTH || what == STAT_DEXTERITY ||
                                    what == STAT_MELEE_ATTACK || what == STAT_MELEE_DEFENCE ||
                                    what == STAT_KATANAS || what == STAT_SABRES || what == STAT_HACKERS ||
                                    what == STAT_HEAVYWEAPONS || what == STAT_BLUNT || what == STAT_MARTIALARTS ||
                                    what == STAT_POLEARMS || what == STAT_DODGE);
                if (isWoundable && getStat_orig) {
                    float raw    = getStat_orig(thisptr, what, true);
                    float rawTgh = getStat_orig(thisptr, STAT_TOUGHNESS, true);
                    if (baseVal < raw && rawTgh > 50.0f) {
                        float loss     = raw - baseVal;
                        float factor   = ((rawTgh - 50.0f) / 10.0f) * 0.12f;   // 每10韧性免12%
                        if (factor > 0.85f) factor = 0.85f;                    // 上限85%
                        baseVal += loss * factor;
                    }
                }
            }

            // 【搬运宗师 Titan Carrier】劳作>=70 && 工程>=70 -> getStat(_MaxCarryWeight) 派生 +100kg
            if (!unmodified && what == _MaxCarryWeight && HasTitanCarrierSynergy(thisptr))
                baseVal += 100.0f;

            // 【兽群庇护】(荒原主宰): 每只同队动物 +1 攻/防, 上限 15 (g_wildPackCount 500ms 缓存)
            if (!unmodified && ch == g_wildernessChar &&
                (what == STAT_MELEE_ATTACK || what == STAT_MELEE_DEFENCE))
                baseVal += (float)g_wildPackCount;

            // 【回光返照】：濒死状态 (躯干血量 <= 20%) 全属性 +50%
            // P3 瘦身: 250ms 节流 (getStat 是引擎最高频调用之一, 避免每次调用都走引擎医疗接口)
            // 体检修正 (2026-09-06): 节流槽按角色独立 — 旧实现单一全局槽, 多角色查询时
            // 互相挤占 250ms 窗口 → 回光返照加成随机丢失 (效果闪烁)。
            {
                struct ND { Character* c; DWORD t; };
                static ND s_nd[8] = {};
                static int s_ndRobin = 0;
                DWORD nowTick = GetTickCount();
                ND* slot = nullptr;
                for (int k = 0; k < 8; ++k) if (s_nd[k].c == ch) { slot = &s_nd[k]; break; }
                if (!slot) { slot = &s_nd[s_ndRobin++ % 8]; slot->c = ch; slot->t = 0; }
                if (charGetMedical_orig && getPart_orig && (nowTick - slot->t) >= 250) {
                    slot->t = nowTick;
                    MedicalSystem* med = charGetMedical_orig(ch);
                    if (med) {
                        void* chest = getPart_orig(med, 0, 0);
                        if (chest) {
                            float curHp = PART_FLESH_HP(chest);
                            float maxHp = PART_MAX_HP(chest);
                            if (maxHp > 0.0f && curHp > 0.0f && (curHp / maxHp) <= 0.20f) {
                                baseVal *= 1.50f;
                            }
                        }
                    }
                }
            }

            // 【水上蛟龙】：游泳 > 60 级反哺四大基础属性 (力/敏/韧/感)
            // 直线公式: 60级起每级+0.2, 100级+8, 之后同斜率延续(无上限)
            // 修复: UI 面板长期显示该专长但机制从未实现 (整合时遗漏)
            if (what == STAT_STRENGTH || what == STAT_DEXTERITY ||
                what == STAT_TOUGHNESS || what == STAT_PERCEPTION)
            {
                float swim = SafeGetStat(thisptr, STAT_SWIMMING, true);
                if (swim > MASTERY_THRESHOLD) {
                    float bonus = (swim - MASTERY_THRESHOLD) * 0.2f;
                    if (bonus > 0.01f) baseVal += bonus;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    t_recursing_getStat = false;
    return SanitizeStat(baseVal, origVal);
}

// ============================================================
// Hook ②: 近战攻击与防御 (CharStats::getMeleeAttack / getMeleeDefence)
// ============================================================
static float getMeleeAttack_hook(CharStats* thisptr) {
    if (!thisptr || !getMeleeAttack_orig) return 0.0f;
    float atk = CallOrigSeh(getMeleeAttack_orig, 0.0f, thisptr);
    const float origAtk = atk;
    if (!IsApplicableStats(thisptr)) return atk;

    __try {
        float per = SafeGetStat(thisptr, STAT_PERCEPTION, true);
        if (per > 0.0f) {
            float bonus = (per / 10.0f);
            if (bonus > 10.0f) bonus = 10.0f;
            atk += bonus;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return SanitizeStat(atk, origAtk);
}

static float getMeleeDefence_hook(CharStats* thisptr, bool includeDefensiveMode) {
    if (!thisptr || !getMeleeDefence_orig) return 0.0f;
    float def = CallOrigSeh(getMeleeDefence_orig, 0.0f, thisptr, includeDefensiveMode);
    const float origDef = def;
    if (!IsApplicableStats(thisptr)) return def;

    __try {
        float per = SafeGetStat(thisptr, STAT_PERCEPTION, true);
        if (per > 0.0f) {
            float bonus = (per / 10.0f);
            if (bonus > 10.0f) bonus = 10.0f;
            def += bonus;
        }

        if (STATS_WEAPON_TYPE(thisptr) == SKILL_SABRES && !IsAnimal(GetOwnerCharacter(thisptr))) {
            float sabreSkill = SafeGetStat(thisptr, STAT_SABRES, true);
            if (sabreSkill > MASTERY_THRESHOLD) {
                float scale = GetMasteryScale(sabreSkill);
                def += (1.0f + scale * 7.0f);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return SanitizeStat(def, origDef);
}

// ============================================================
// Hook ③: 闪避挂钩 (CharStats::getDodge)
// ============================================================
static float getDodge_hook(CharStats* thisptr, bool bonuses) {
    if (!thisptr || !getDodge_orig) return 0.0f;
    float d = CallOrigSeh(getDodge_orig, 0.0f, thisptr, bonuses);
    const float origD = d;
    if (!IsApplicableStats(thisptr)) return d;

    __try {
        // 【敏捷身赐】敏捷→闪避: 每10点敏捷 +1 闪避(上限+10) [移植 DodgeSynergy]
        if (SafeGetStat(thisptr, STAT_DEXTERITY, false) > 0.0f) {
            float dexB = SafeGetStat(thisptr, STAT_DEXTERITY, false) / 10.0f;
            if (dexB > 10.0f) dexB = 10.0f;   // cap +10
            d += dexB;
        }

        float per = SafeGetStat(thisptr, STAT_PERCEPTION, true);
        if (per > 0.0f) {
            float bonus = (per / 12.0f);
            if (bonus > 8.0f) bonus = 8.0f;
            d += bonus;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return SanitizeStat(d, origD);
}

// ============================================================
// Hook ④: 破甲计算 (CharStats::getAttackPierceDamage)
// ============================================================
static float getAttackPierceDamage_hook(CharStats* thisptr) {
    if (!thisptr || !getPierceDamage_orig) return 0.0f;
    float pen = CallOrigSeh(getPierceDamage_orig, 0.0f, thisptr);
    // ★【流法·重击流】: 穿甲加成 (+0..20%+, 随等级外推)
    __try {
        float rSpd = 1.0f, rDmg = 1.0f, rPierce = 0.0f;
        RyuMults(thisptr, &rSpd, &rDmg, &rPierce);
        if (rPierce > 0.0f) {
            pen += rPierce;
            DiagnosticLogThrottled("RyuPierce", 5000,
                "[流法] 穿甲 +" + std::to_string((int)(rPierce * 100)) + "%");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    const float origPen = pen;
    if (!IsApplicableStats(thisptr)) return pen;

    __try {
        float str = SafeGetStat(thisptr, STAT_STRENGTH, true);
        pen += (str / 10.0f) * 0.01f;

        if (!IsAnimal(GetOwnerCharacter(thisptr))) {
            int wType = STATS_WEAPON_TYPE(thisptr);
            if (wType == SKILL_HACKERS) {
                float skill = SafeGetStat(thisptr, STAT_HACKERS, true);
                if (skill > MASTERY_THRESHOLD) {
                    pen += (0.03f + GetMasteryScale(skill) * 0.12f);
                }
            } else if (wType == SKILL_BLUNT) {
                float skill = SafeGetStat(thisptr, STAT_BLUNT, true);
                if (skill > MASTERY_THRESHOLD) {
                    pen += (0.05f + GetMasteryScale(skill) * 0.20f);
                }
            } else if (CrossbowHeldType(thisptr)) {
                // 弩感知破甲 (仅持弩/弓): 感知>60 起, 100=+30%, >100 每级 +0.008 无上限
                float perc = SafeGetStat(thisptr, STAT_PERCEPTION, true);
                if (perc > CrossbowBoost::SENSE_START) {
                    if (perc <= CrossbowBoost::SENSE_FULL) {
                        float k = (perc - CrossbowBoost::SENSE_START) / (CrossbowBoost::SENSE_FULL - CrossbowBoost::SENSE_START);
                        pen += CrossbowBoost::PIERCE_AT_100 * k;
                    } else {
                        pen += CrossbowBoost::PIERCE_AT_100 +
                               (perc - CrossbowBoost::SENSE_FULL) * CrossbowBoost::PIERCE_OVER_LVL;
                    }
                }
            }
        }
        if (pen > 1.0f) pen = 1.0f;
        if (pen < 0.0f) pen = 0.0f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return SanitizeStat(pen, origPen);
}

// ============================================================
// Hook ⑤: 移速挂钩 (CharStats::getMovementSpeed)
// 包含: 【搬运宗师 (Titan Carrier)】联合专长 (劳作>=70 && 建筑>=70)
// ============================================================
static float getMovementSpeed_hook(CharStats* thisptr) {
    if (!thisptr || !getMovementSpeed_orig) return 0.0f;
    float spd = CallOrigSeh(getMovementSpeed_orig, 0.0f, thisptr);
    // 【兽群同调·强化】(荒原主宰): 同队动物移速 +20%
    __try {
        Character* oc = GetOwnerCharacter(thisptr);
        if (oc && DesoIsAnimal(oc, false) && g_wildernessChar && charGetPlatoon_orig &&
            charGetPlatoon_orig(oc) == charGetPlatoon_orig(g_wildernessChar)) {
            spd *= 1.20f;
            DiagnosticLogThrottled("WildMoveBuff", 5000,
                "[兽群buff] 移速放大触发: -> " + std::to_string((int)(spd * 100)));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!IsApplicableStats(thisptr)) return spd;

    __try {
        // 搬运宗师联合专长生效：负重与背人不减速，并提供移速加成
        if (HasTitanCarrierSynergy(thisptr)) {
            spd *= 1.25f;
        } else {
            float str = SafeGetStat(thisptr, STAT_STRENGTH, true);
            if (str >= 60.0f) {
                float bonus = 1.0f + ((str - 60.0f) / 40.0f) * 0.15f;
                if (bonus > 1.30f) bonus = 1.30f;
                spd *= bonus;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return spd;
}

// ============================================================
// Hook ⑥: 饥饿代谢 (CharStats::calculateHungerMult)
// 包含: 【战地灵粮 (Field Chef)】联合专长 (农业>=70 && 烹饪>=70)
// ============================================================
static float calculateHungerMult_hook(CharStats* thisptr) {
    if (!thisptr || !calcHungerMult_orig) return 1.0f;
    float mult = CallOrigSeh(calcHungerMult_orig, 1.0f, thisptr);
    const float origMult = mult;
    if (!IsApplicableStats(thisptr)) return mult;

    __try {
        if (HasFieldChefSynergy(thisptr) || SafeGetStat(thisptr, STAT_COOKING, true) >= 60.0f) {
            mult *= 0.70f; // 饥饿速度减少 30%
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return SanitizeStat(mult, origMult);
}

// ============================================================
// Hook ⑦: 攻速与招架速度 (CharStats::calculateAttackOrBlockSpeed)
// 包含: 原版 1.2 攻速截断重算突破至 4.8 + 【万法皆通·神速】+ 田径/武术攻速加成
// ============================================================
static float calculateAttackOrBlockSpeed_hook(CharStats* thisptr, float wWeight, float skill, bool isBlock) {
    if (!thisptr || !calcAttackBlockSpeed_orig) return 1.0f;
    float spd = CallOrigSeh(calcAttackBlockSpeed_orig, 1.0f, thisptr, wWeight, skill, isBlock);
    // ★【流法】: 近战>=60 三流派攻速乘数 (疾风/中庸/重击; 格挡动作不吃流法攻速)
    if (!isBlock) {
        float rSpd = 1.0f, rDmg = 1.0f, rPierce = 0.0f;
        RyuMults(thisptr, &rSpd, &rDmg, &rPierce);
        if (rSpd != 1.0f) {
            spd *= rSpd;
            DiagnosticLogThrottled("RyuSpd", 5000,
                "[流法] 攻速乘数 " + std::to_string(rSpd).substr(0, 5) +
                " -> " + std::to_string((int)(spd * 100)));
        }
    }

    // 【兽群同调·强化】(荒原主宰): 同队动物攻速 +20% (动物攻击若走此路径)
    __try {
        Character* oc = GetOwnerCharacter(thisptr);
        if (oc && DesoIsAnimal(oc, false) && !isBlock && g_wildernessChar && charGetPlatoon_orig &&
            charGetPlatoon_orig(oc) == charGetPlatoon_orig(g_wildernessChar))
            spd *= 1.20f;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    const float origSpd = spd;
    if (!IsApplicableStats(thisptr)) return spd;

    __try {
        // ★ 攻速上限解锁的"重算"部分（中间不动, 只重算不 clamp; 上限在末尾统一应用）：
        // 原版引擎内部死锁 clamp 到 1.2f。当触顶且上限放宽时重算真实速度, 供末尾 clamp
        if (!isBlock && spd >= 1.199f) {
            if (g_maxAttackSpeedCap > 1.201f) {
                float dex = SafeGetStat(thisptr, STAT_DEXTERITY, false);
                float combatMult = *(float*)((char*)thisptr + 0x1C); // combatSpeedMultiplier @ 0x1C
                if (combatMult <= 0.001f) combatMult = 1.0f;

                float raw = (0.765f + skill * 0.00152f + dex * 0.00228f) * wWeight * combatMult;
                if (raw > spd) spd = raw;
            }
        }

        Character* ch = GetOwnerCharacter(thisptr);

        // ★【万法皆通·神速】：攻速与招架速度 +50%
        if (ch && HasMartialAscension(ch)) {
            spd *= 1.50f;
        }

        // 田径宗师攻速加成
        float ath = SafeGetStat(thisptr, STAT_ATHLETICS, true);
        if (ath > MASTERY_THRESHOLD) {
            float athBonus = (ath <= 100.0f) ? ((ath - 60.0f) / 40.0f) * 0.15f : 0.15f + (ath - 100.0f) * 0.005f;
            spd *= (1.0f + athBonus);
        }

        // 武术宗师攻速加成
        if (STATS_WEAPON_TYPE(thisptr) == SKILL_UNARMED) {
            float maSkill = SafeGetStat(thisptr, STAT_MARTIALARTS, true);
            if (maSkill > MASTERY_THRESHOLD) {
                float scale = GetMasteryScale(maSkill);
                spd *= (1.0f + 0.08f + scale * 0.22f);
            }
        }

        // 长柄压制 debuff: 该角色处于"被打/被长柄压制"窗时攻速×mult
        if (ch) {
            float pm = PoleSuppressedMult(ch);
            if (pm < 1.0f) spd *= pm;
        }

        // ★ 速度上限收尾: 区分攻速 vs 格挡 (格挡独立上限, 不随攻速上限降到1.2而降到1.2)
        // 格挡上限: 恒定 4; 仅当攻速上限提到 9.6(神魔化境) 时才放宽到 8.
        float capLimit = isBlock
            ? (g_maxAttackSpeedCap >= 9.59f ? 8.0f : 4.0f)
            : g_maxAttackSpeedCap;
        if (spd > capLimit) spd = capLimit;
        if (spd < 0.0f) spd = 0.0f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return SanitizeStat(spd, origSpd);
}

// ============================================================
// Hook ⑧: 格挡姿态与【残影瞬步】(CombatClass::blockState)
// ============================================================
static bool blockState_hook(void* thisptr, bool stumbleBlocking) {
    __try {
        Character* me = COMBAT_OWNER(thisptr);
        if (me && charGetStats_orig && !IsAnimal(me) && IsApplicable(me)) {
            CharStats* stats = charGetStats_orig(me);
            if (stats) {
                // ★【万法皆通·道】：90% 绝对格挡
                if (HasMartialAscension(me)) {
                    if (((float)rand() / (float)RAND_MAX) < 0.90f) {
                        return true;
                    }
                }

                float dex = SafeGetStat(stats, STAT_DEXTERITY, true);
                float dodge = SafeGetStat(stats, STAT_DODGE, true);

                // 【残影瞬步】：敏捷>=60 && 闪避>=60 削减硬直后摇
                if (dodge >= 60.0f && dex >= 60.0f) {
                    float lowerStat = dex < dodge ? dex : dodge;
                    float scale = GetMasteryScale(lowerStat);

                    float* stumbleTimer = (float*)((char*)thisptr + 0x124);
                    float* whenCanStop  = (float*)((char*)thisptr + 0x128);

                    float recoveryBoost = 0.30f + scale * 0.30f;
                    if (recoveryBoost > 0.80f) recoveryBoost = 0.80f;

                    if (stumbleTimer && *stumbleTimer > 0.0f) *stumbleTimer *= (1.0f - recoveryBoost);
                    if (whenCanStop  && *whenCanStop  > 0.0f) *whenCanStop  *= (1.0f - recoveryBoost);
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return CallOrigSeh(blockState_orig, false, thisptr, stumbleBlocking);
}

// ============================================================
// Hook ⑨: 攻击判定距离 (CharStats::getCurrentWeaponLength)
// ============================================================
static float getCurrentWeaponLength_hook(CharStats* thisptr) {
    if (!thisptr || !getCurrentWeaponLength_orig) return 0.0f;
    float baseLen = CallOrigSeh(getCurrentWeaponLength_orig, 0.0f, thisptr);
    const float origLen = baseLen;
    if (!IsApplicableStats(thisptr)) return baseLen;

    __try {
        Character* ch = GetOwnerCharacter(thisptr);
        if (ch && !IsAnimal(ch)) {
            // ★【万法皆通·剑罡】：有效判定距离 +50%
            if (HasMartialAscension(ch)) {
                baseLen *= 1.50f;
            }

            int wType = STATS_WEAPON_TYPE(thisptr);
            if (wType == SKILL_HEAVY || wType == ATTACK_POLEARMS) {
                float skill = (wType == SKILL_HEAVY) ? SafeGetStat(thisptr, STAT_HEAVYWEAPONS, true) : SafeGetStat(thisptr, STAT_POLEARMS, true);
                if (skill > MASTERY_THRESHOLD) {
                    float scale = GetMasteryScale(skill);
                    baseLen *= (1.0f + (0.05f + scale * 0.15f));
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return SanitizeStat(baseLen, origLen);
}

// ============================================================
// Hook ⑩: 格挡受击反震 (CombatClass::_blockHit)
// ============================================================
static void blockHit_hook(void* thisptr, int cutDir, const Damages* damage, void* who) {
    if (blockHit_orig) {
        CallOrigSehV(blockHit_orig, thisptr, cutDir, damage, who);
    }
    __try {
        Character* defender = COMBAT_OWNER(thisptr);
        if (!defender || !who || IsAnimal(defender) || !charGetStats_orig || !charGetMedical_orig || !healthPartApplyDmg_orig)
            return;

        CharStats* defStats = charGetStats_orig(defender);
        CharStats* atkStats = charGetStats_orig((Character*)who);
        if (!defStats || !atkStats) return;

        float defStr = SafeGetStat(defStats, STAT_STRENGTH, true);
        float defDef = SafeGetStat(defStats, STAT_MELEE_DEFENCE, true);
        float atkStr = SafeGetStat(atkStats, STAT_STRENGTH, true);
        bool masteryImmune = (defDef > DEFENSE_MASTERY);

        // 反震(高力攻方压制低力守方): 触发取决于攻方(who)是否适用
        // (只玩家模式下, NPC高力攻方打玩家不应触发反震惩罚玩家; 只有玩家高力攻方惩罚NPC守方)
        if (IsApplicable((Character*)who) && !masteryImmune && (atkStr - defStr) > STRENGTH_GAP_THRESHOLD) {
            float excess = (atkStr - defStr) - STRENGTH_GAP_THRESHOLD;
            float recoilDamage = (excess / 10.0f) * RECOIL_DAMAGE_PER_10;
            MedicalSystem* med = charGetMedical_orig(defender);
            if (med) {
                char dmgBuf[0x18] = { 0 };
                float* d = (float*)dmgBuf;
                d[1] = recoilDamage;
                void* leftArm = MED_LEFT_ARM(med);
                void* rightArm = MED_RIGHT_ARM(med);
                if (leftArm) healthPartApplyDmg_orig(leftArm, (Damages*)dmgBuf);
                if (rightArm) healthPartApplyDmg_orig(rightArm, (Damages*)dmgBuf);
            }
        }

        if (IsApplicable(defender) && defDef > COUNTER_THRESHOLD) {
            float counterChance = (defDef - COUNTER_THRESHOLD) * 0.03f;
            if (((float)rand() / (float)RAND_MAX) < counterChance) {
                MedicalSystem* atkMed = charGetMedical_orig((Character*)who);
                if (atkMed && getPart_orig) {
                    char dmgBuf[0x18] = { 0 };
                    float* d = (float*)dmgBuf;
                    d[1] = 15.0f;
                    void* chest = getPart_orig(atkMed, 0, 0);
                    if (chest) healthPartApplyDmg_orig(chest, (Damages*)dmgBuf);
                }
            }
        }

        // 【幻影多人格挡 / 多线程身法】(敏捷≥100): 成功架开一击后立即清空自身受创/格挡冷却,
        // 使能连续格挡"同时劈下的多把刀"。见 DodgeSynergy 相同实现 (stumbleTimer@0x124,
        // deadTimer@0x148, stateTimer@0x14C, inDeadTime@0x144)
        if (IsApplicable(defender)) {
            CharStats* dvStats = charGetStats_orig? charGetStats_orig(defender) : nullptr;
            if (dvStats) {
                float dex = SafeGetStat(dvStats, STAT_DEXTERITY, false);
                if (dex >= 100.0f) {
                    *(float*)((char*)thisptr + 0x124) = 0.0f;          // stumbleTimer = 0
                    *(float*)((char*)thisptr + 0x148) = 0.0f;          // deadTimer = 0
                    *(float*)((char*)thisptr + 0x14C) = 0.0f;          // stateTimer = 0
                    *(bool*)((char*)thisptr + 0x144) = false;          // inDeadTime = false
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// Hook ⑪: 受击与伤害结算 (CombatClass::_getHit)
// ============================================================
static void getHit_hook(void* thisptr, int cutDir, const Damages* damage, void* who, bool stumble) {
    bool suppressStumble = false;
    char modDmgBuf[0x20] = { 0 };
    const Damages* finalDamage = damage;

    __try {
        Character* victim = COMBAT_OWNER(thisptr);
        if (victim && charGetStats_orig) {
            CharStats* victimStats = charGetStats_orig(victim);
            CharStats* atkStats = who ? charGetStats_orig((Character*)who) : nullptr;

            float victimStr = victimStats ? SafeGetStat(victimStats, STAT_STRENGTH, true) : 0.0f;
            float attackerStr = atkStats ? SafeGetStat(atkStats, STAT_STRENGTH, true) : 0.0f;

            if (victimStats && !IsAnimal(victim) && IsApplicable(victim)) {
                float heavySkill = SafeGetStat(victimStats, STAT_HEAVYWEAPONS, true);
                float bluntSkill = SafeGetStat(victimStats, STAT_BLUNT, true);
                bool hasHyperArmor = (victimStr >= HYPER_ARMOR_STR_REQ) &&
                                     (heavySkill >= 20.0f || bluntSkill >= 20.0f || victimStr >= 100.0f);
                bool brokenByGap = (attackerStr - victimStr) >= STRENGTH_GAP_THRESHOLD;

                if (hasHyperArmor && !brokenByGap) {
                    suppressStumble = true;
                    COMBAT_STUMBLE_X(thisptr) = 0.0f;
                    COMBAT_STUMBLE_Y(thisptr) = 0.0f;
                    COMBAT_STUMBLE_Z(thisptr) = 0.0f;
                }
            }

            if (atkStats && damage && !IsAnimal((Character*)who) && IsApplicable((Character*)who)) {
                memcpy(modDmgBuf, damage, 0x18);
                float* d = (float*)modDmgBuf;
                finalDamage = (const Damages*)modDmgBuf;

                // ★【万法皆通·真意】：全武器物理伤害 +50% + 目标"本次受击部位"10% 最大生命真实伤害
                if (HasMartialAscension((Character*)who)) {
                    if (d[0] > 0.0f) d[0] *= 1.50f;
                    if (d[1] > 0.0f) d[1] *= 1.50f;

                    // 规格修正 (2026-09-06): 真伤打"所攻击部位"而非固定躯干。
                    // _getHit 不知命中部位 → 此处仅标记 250ms 窗口,
                    // 由 MedicalSystem::applyDamage hook 对本次实际受击部位追加真伤。
                    MartialTrueMark(victim);
                }

                // ★【流法】: 伤害乘数 (疾风-40%.. / 中庸+20% / 重击+40%..; 只对近战兵器类)
                {
                    float rSpd = 1.0f, rDmg = 1.0f, rPierce = 0.0f;
                    RyuMults(atkStats, &rSpd, &rDmg, &rPierce);
                    if (rDmg != 1.0f) {
                        if (d[0] > 0.0f) d[0] *= rDmg;
                        if (d[1] > 0.0f) d[1] *= rDmg;
                        DiagnosticLogThrottled("RyuDmg", 5000,
                            "[流法] 伤害乘数 " + std::to_string(rDmg).substr(0, 5));
                    }
                }

                // ★【人剑合一】(武器工匠60+, 2026-09-07): 手中武器切/钝伤 +0.5%/级 (100级=+20%)
                if (atkStats) {
                    float wLv = SafeGetStat(atkStats, STAT_SMITHING_WEAPON, false);
                    if (wLv >= 60.0f) {
                        float mult = 1.0f + (wLv - 60.0f) * 0.005f;
                        if (d[0] > 0.0f) d[0] *= mult;
                        if (d[1] > 0.0f) d[1] *= mult;
                        DiagnosticLogThrottled("RyuSword", 5000,
                            "[人剑合一] 武器伤害×" + std::to_string(mult).substr(0, 5));
                    }
                }

                int wtype = STATS_WEAPON_TYPE(atkStats);

                // ★【长柄压制】：攻击者为长柄 → 被其打中的对象(victim)攻速降至 PoleMultForSkill, 5s, 不叠加
                if (wtype == ATTACK_POLEARMS) {
                    float pskill = SafeGetStat(atkStats, STAT_POLEARMS, true);
                    if (pskill > MASTERY_THRESHOLD) PoleMark(victim, PoleMultForSkill(pskill));
                }

                if (wtype == SKILL_KATANAS) {
                    float katanaSkill = SafeGetStat(atkStats, STAT_KATANAS, true);
                    if (katanaSkill > MASTERY_THRESHOLD) {
                        MedicalSystem* targetMed = charGetMedical_orig ? charGetMedical_orig(victim) : nullptr;
                        bool targetRobotic = (targetMed && hasRobotics_orig && hasRobotics_orig(targetMed));
                        if (!targetRobotic && targetMed) {
                            float scale = GetMasteryScale(katanaSkill);
                            MED_BLOOD(targetMed) -= (1.0f + scale * 3.0f);
                            MED_BLEED_RATE(targetMed) += (0.1f + scale * 0.4f);
                            float extraCut = 0.04f + scale * 0.12f;
                            if (d[0] > 0.0f) d[0] *= (1.0f + extraCut);
                        }
                    }
                }
                else if (wtype == SKILL_HACKERS) {
                    float hackerSkill = SafeGetStat(atkStats, STAT_HACKERS, true);
                    if (hackerSkill > MASTERY_THRESHOLD) {
                        MedicalSystem* targetMed = charGetMedical_orig ? charGetMedical_orig(victim) : nullptr;
                        bool targetRobotic = (targetMed && hasRobotics_orig && hasRobotics_orig(targetMed));
                        if (targetRobotic) {
                            float scale = GetMasteryScale(hackerSkill);
                            float robotBonus = 0.05f + scale * 0.15f;
                            if (d[0] > 0.0f) d[0] *= (1.0f + robotBonus);
                            if (d[1] > 0.0f) d[1] *= (1.0f + robotBonus);
                        }
                    }
                }
                else if (wtype == SKILL_HEAVY) {
                    float heavySkill = SafeGetStat(atkStats, STAT_HEAVYWEAPONS, true);
                    if (heavySkill > MASTERY_THRESHOLD) {
                        float scale = GetMasteryScale(heavySkill);
                        *(float*)((char*)thisptr + 0x124) *= (1.1f + scale * 0.4f);
                        *(float*)((char*)thisptr + 0x12C) *= (1.1f + scale * 0.4f);
                    }
                }
                else if (wtype == SKILL_TURRET) {  // 官方 WeaponCategory: SKILL_TURRET=7
                    float turretSkill = SafeGetStat(atkStats, STAT_TURRETS, true);
                    if (turretSkill > MASTERY_THRESHOLD) {
                        float mult = (turretSkill <= 100.0f)
                            ? 1.0f + GetMasteryScale(turretSkill) * 1.5f
                            : 2.5f + (turretSkill - 100.0f) * 0.02f;
                        *(float*)((char*)thisptr + 0x124) *= mult;
                        *(float*)((char*)thisptr + 0x12C) *= mult;
                    }
                }
            }

            // ★【长柄压制 · 反】: 被打对象正持长柄(守方长柄) → 其攻击方(who)也被降攻速,5s,不叠加
            // (victim 需是人且持 ATTACK_POLEARMS; 以 victim 的长柄技能推导幅度)
            if (victimStats && STATS_WEAPON_TYPE(victimStats) == ATTACK_POLEARMS) {
                float pskillV = SafeGetStat(victimStats, STAT_POLEARMS, true);
                if (pskillV > MASTERY_THRESHOLD && who && !IsAnimal((Character*)who))
                    PoleMark((Character*)who, PoleMultForSkill(pskillV));
            }

            if (atkStats && attackerStr >= SHOCKWAVE_STR_REQ && IsApplicable((Character*)who) &&
                charGetMedical_orig && healthPartApplyDmg_orig && getPart_orig) {
                float victimTough = victimStats ? SafeGetStat(victimStats, STAT_TOUGHNESS, true) : 0.0f;
                float resistMult = (victimTough > 90.0f) ? (1.0f - (victimTough - 90.0f) * 0.10f) : 1.0f;
                if (resistMult > 0.0f) {
                    MedicalSystem* victimMed = charGetMedical_orig(victim);
                    if (victimMed) {
                        float shockDamage = (30.0f + ((attackerStr - SHOCKWAVE_STR_REQ) / 10.0f) * 8.0f) * resistMult;
                        if (shockDamage > 75.0f) shockDamage = 75.0f;
                        void* chestPart = getPart_orig(victimMed, 0, 0);
                        if (chestPart) {
                            char dmgBuf[0x18] = { 0 };
                            float* d = (float*)dmgBuf;
                            d[1] = shockDamage;
                            healthPartApplyDmg_orig(chestPart, (Damages*)dmgBuf);
                        }
                    }
                }
            }

            // ★【机械至尊·命中致瘫】：只要目标为机械或含有机械肢体/部位，直接电路致瘫短路 15 秒
            if (atkStats && HasCyberAscension((Character*)who) && charGetMedical_orig) {
                MedicalSystem* targetMed = charGetMedical_orig(victim);
                if (targetMed) {
                    bool hasRoboticElement = false;
                    // 2026-09-06: 删除未验证的 Character::isRobotic(victim) 裸调 (5.29 同族
                    // 反模式: 流送期引擎方法沿悬垂链解引用); 其语义由下方 hasRobotics
                    // (医疗级) + RaceIsRobot (种族级) + 逐部位 PartIsRoboticVerified 覆盖
                    if (!hasRoboticElement && hasRobotics_orig && hasRobotics_orig(targetMed)) hasRoboticElement = true;

                    if (!hasRoboticElement && getRace_orig) {
                        void* race = getRace_orig(victim);
                        if (race && RaceIsRobot(race)) hasRoboticElement = true;
                    }

                    // 进一步遍历其所有具体身体部位，只要任意一部位是机械（含义肢），即触发短路
                    if (!hasRoboticElement && getPartCount_orig && getPartByIndex_orig) {
                        int pCount = getPartCount_orig(targetMed);
                        for (int pi = 0; pi < pCount; ++pi) {
                            void* pt = getPartByIndex_orig(targetMed, pi);
                            if (pt && PartIsRoboticVerified(targetMed, victim, pt)) {
                                hasRoboticElement = true;
                                break;
                            }
                        }
                    }

                    if (hasRoboticElement) {
                        // 直接安全写入 15 秒昏迷倒计时 (MedicalSystem::knockoutTimer @ 0xA0)
                        // 绝不调用原版 knockout_orig，避免骨人状态机解引用空指针崩溃
                        *(float*)((char*)targetMed + 0xA0) = 15.0f;
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    CallOrigSehV(getHit_orig, thisptr, cutDir, finalDamage, who, suppressStumble ? false : stumble);
}

// ============================================================
// Hook ⑫: 钝器脑震荡 (CombatClass::attackImpactCheck)
// ============================================================
static void attackImpactCheck_hook(void* thisptr) {
    if (attackImpactCheck_orig) {
        CallOrigSehV(attackImpactCheck_orig, thisptr);
    }
    if (!thisptr) return;

    __try {
        Character* attacker = COMBAT_OWNER(thisptr);
        if (!attacker || !charGetStats_orig || IsAnimal(attacker) || !IsApplicable(attacker))
            return;

        CharStats* stats = charGetStats_orig(attacker);
        if (!stats) return;

        int wType = STATS_WEAPON_TYPE(stats);
        if (wType != SKILL_BLUNT) return;

        float bluntSkill = SafeGetStat(stats, STAT_BLUNT, true);
        if (bluntSkill <= MASTERY_THRESHOLD) return;

        void* targetObj = COMBAT_TARGET(thisptr);
        if (!targetObj) return;

        Character* targetChar = nullptr;
        if (charGetStats_orig) {
            CharStats* targetStats = charGetStats_orig((Character*)targetObj);
            if (targetStats) targetChar = (Character*)targetObj;
        }
        if (!targetChar) return;

        float scale = GetMasteryScale(bluntSkill);
        float stunChance = 0.04f + scale * 0.10f;

        if (((float)rand() / (float)RAND_MAX) < stunChance) {
            if (charGetMedical_orig && getPart_orig && healthPartApplyDmg_orig) {
                MedicalSystem* targetMed = charGetMedical_orig(targetChar);
                if (targetMed) {
                    void* headPart = getPart_orig(targetMed, 0, 0);
                    if (headPart) {
                        char dmgBuf[0x18] = { 0 };
                        float* d = (float*)dmgBuf;
                        d[1] = 12.0f + scale * 18.0f;
                        healthPartApplyDmg_orig(headPart, (Damages*)dmgBuf);
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// 弩 (Gun / Crossbow) 特有 —— 用户 2026-09 规格, 仅"持弩(弓/弩 WeaponCategory=BOW)"生效
//   · 装填 -30%   : 敏捷 ≥80 且 感知 ≥80 时 (GunClass::calculateReloadTime 装填 ×0.70)
//   · 感知破甲    : 感知>60 起; 100感知=+30% (>60→100 每级推进); >100 每级 +0.008 (无上限)
// Kenshi 无独立 CROSSBOW WeaponCategory —— 弓/弩同属 SKILL_BOW(6); 故“持弩/弓”判定用
// 当前武器类型==SKILL_BOW; 炮塔(SKILL_TURRET)与近战武器不享受。
// ============================================================
// (CrossbowBoost namespace 与 CrossbowHeldType 已在文件顶部定义, 勿重复)
// 由 Gun 实例(+0x30 user hand) 解析其持有角色
static inline Character* GunOwnerChar(void* gun) {
    if (!gun || !handGetChar_orig) return nullptr;
    const void* h = (const void*)((char*)gun + 0x30);   // GunClass.user @0x30 (RangedSynergy 实证)
    return handGetChar_orig(h);
}

// ============================================================
// Hook ㉕: 弩装填 (GunClass::calculateReloadTime) —— 装填 -30%
// ============================================================
static float gunReload_hook(void* thisptr, float stat01) {
    if (!gunReloadTime_orig) return stat01;
    float t = CallOrigSeh(gunReloadTime_orig, stat01, thisptr, stat01);
    __try {
        Character* owner = GunOwnerChar(thisptr);
        if (!owner || IsAnimal(owner) || !IsApplicable(owner)) return t;
        CharStats* st = charGetStats_orig ? charGetStats_orig(owner) : nullptr;
        if (!st || !CrossbowHeldType(st)) return t;     // 仅持弩/弓(不含炮塔/近战)
        static bool __log_ok = false;
        if (!__log_ok) { __log_ok = true; DiagnosticLog("[弩模块] reload_hook 首次触发 OK (持弩/弓)"); }
        float dex  = SafeGetStat(st, STAT_DEXTERITY,  true);
        float perc = SafeGetStat(st, STAT_PERCEPTION, true);
        if (dex >= CrossbowBoost::DEX_SENSE_REQ && perc >= CrossbowBoost::DEX_SENSE_REQ)
            t *= CrossbowBoost::RELOAD_MULT;            // 装填 -30%
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return t;
}

// ============================================================
// Hook ㉖: 弩感知破甲 —— 并入 CharStats::getAttackPierceDamage (核心已有 hook)
//   仅此扩展: 持弩/弓 + 感知>60 → 加穿(见公式); 与力量/黑客/钝器分支并存。
// ============================================================

// ============================================================
// 弩伤害三段 (移植 CrossbowSynergy v4, 用户确认照搬 — 用"对人形压极低、对非人/动物不压
// 不减伤"间接达成"打人以外伤害超高", 不写动物x2字样):
//   · ① 弩技能→伤害: skill≤40 ×1.0; 40→100 线性→×3.0; >100 每级 +8%
//   · ② 打人形(race.noShirts==false 且非 gigantic)=可穿衣人/巨兽除外: ×0.40 减值, 再按
//       目标近战防御 每10防御 -6% 减伤, 上限 -60% (打在护甲十足的目标非常刮痧)
//   · 非人/动物/建筑等: 不检查防御、不加减值 → 伤害保持基础超高
//   仅 stat==STAT_CROSSBOWS 的手持弩; 炮塔(STAT_TURRETS)不受影响。
//   实现 = shoot 瞬间按倍率改 GunClassPersonal damage低限 @0x118/高限 @0x11C,
//   调用原 shoot(引擎取 rnd(min,max)) 后恢复原值 → 命中伤害自然放大,零持续污染。
// ============================================================
namespace XbowDamage {
    static const float SKILL_MIN_BASE       = 40.0f;
    static const float SKILL_MAX_BASE       = 100.0f;
    static const float SKILL_MAX_MULT       = 3.0f;
    static const float SKILL_EXTRA_PER_LVL  = 0.08f;   // 100+ 每级 +8%
    static const float HUMAN_PENALTY        = 0.40f;   // 打人形 ×0.40 (-60%)
    static const float DEF_RED_PER_10       = 0.06f;   // 每10近战防御 -6%
    static const float DEF_RED_CAP          = 0.60f;   // 减伤上限 60%
    static const int   RACE_GIGANTIC_OFF    = 0x78;
    static const int   RACE_NOSHIRTS_OFF    = 0x7F;
}
static float XbowSkillMult(float skill);
static float XbowDefMult(float def);

// 目标是否"人形"(可穿衣 RaceData.noShirts==0 且 非巨型 gigantic==0); 动物/巨兽 = 非人
static inline bool TargetIsHumanoid(void* target) {
    if (!target || !getRace_orig) return false;
    __try {
        void* race = getRace_orig((Character*)target);
        if (!race) return false;
        unsigned char noShirts = *(unsigned char*)((char*)race + XbowDamage::RACE_NOSHIRTS_OFF);
        unsigned char gigantic = *(unsigned char*)((char*)race + XbowDamage::RACE_GIGANTIC_OFF);
        return (noShirts == 0) && (gigantic == 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static float XbowSkillMult(float skill) {
    if (skill <= XbowDamage::SKILL_MIN_BASE) return 1.0f;
    if (skill <= XbowDamage::SKILL_MAX_BASE)
        return 1.0f + (skill - XbowDamage::SKILL_MIN_BASE) *
               (XbowDamage::SKILL_MAX_MULT - 1.0f) / (XbowDamage::SKILL_MAX_BASE - XbowDamage::SKILL_MIN_BASE);
    return XbowDamage::SKILL_MAX_MULT + (skill - XbowDamage::SKILL_MAX_BASE) * XbowDamage::SKILL_EXTRA_PER_LVL;
}
static float XbowDefMult(float def) {
    if (def <= 0.0f) return 1.0f;
    float r = 1.0f - def * (XbowDamage::DEF_RED_PER_10 / 10.0f);
    if (r < (1.0f - XbowDamage::DEF_RED_CAP)) r = 1.0f - XbowDamage::DEF_RED_CAP;
    return r;
}

// ============================================================
// Hook ㉗: GunClass::shoot 弩伤害三段
// ============================================================
static void gunShoot_hook(void* thisptr, Character* me, void* target, int stat, const void* aimpos) {
    if (!gunShoot_orig) return;

    // 仅手持弩; 炮塔不干预
    if (stat == STAT_CROSSBOWS && me && target) {
        static bool __shot_seen = false;
        if (!__shot_seen) { __shot_seen = true; DiagnosticLog("[弩模块] shoot_hook 首次进入 (stat=CROSSBOWS) OK"); }
        float mult = 1.0f;
        __try {
            // ① 弩技能→伤害
            if (charGetStats_orig && getStat_orig) {
                CharStats* ms = charGetStats_orig(me);
                if (ms) mult *= XbowSkillMult(SafeGetStat(ms, STAT_CROSSBOWS, false));
            }
            // ② 打人形 → 减值 + 近战防御减伤; 非人形 → 不扣
            if (TargetIsHumanoid(target)) {
                mult *= XbowDamage::HUMAN_PENALTY;
                __try {
                    CharStats* ts = charGetStats_orig((Character*)target);
                    if (ts) mult *= XbowDefMult(SafeGetStat(ts, STAT_MELEE_DEFENCE, false));
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        if (mult != 1.0f) {
            char* base = (char*)thisptr;
            int oldMin = 0, oldMax = 0;
            __try {
                oldMin = *(int*)(base + 0x118);
                oldMax = *(int*)(base + 0x11C);
                *(int*)(base + 0x118) = (int)(oldMin * mult);
                *(int*)(base + 0x11C) = (int)(oldMax * mult);
                gunShoot_orig(thisptr, me, target, stat, aimpos);
                *(int*)(base + 0x118) = oldMin;
                *(int*)(base + 0x11C) = oldMax;
                return;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // 体检修正 (2026-09-06): shoot 抛异常也必须恢复 damage 上下限,
                // 否则改写后的伤害区间永久污染该 GunClass (写路径泄漏)
                __try {
                    *(int*)(base + 0x118) = oldMin;
                    *(int*)(base + 0x11C) = oldMax;
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
    }
    CallOrigSehV(gunShoot_orig, thisptr, me, target, stat, aimpos);
}

