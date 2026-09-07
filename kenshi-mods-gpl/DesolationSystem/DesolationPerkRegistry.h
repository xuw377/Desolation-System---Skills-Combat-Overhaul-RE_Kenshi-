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
// 专长注册表 (Perk Registry) — 2026-09-06 维护降本方案①
// ------------------------------------------------------------
// 定位: 全部专长/能力的**元数据单一事实源**。
//   - 表行粒度 = "能力"(UI 可显示的独立行), 与 REFERENCE 规范条目经 ref 关联;
//     REFERENCE 一个条目可派生多个能力行(如 ㉓纳米自修 → 纳米自愈 + 义体超频)。
//   - UI(DivineGraceUI) 文案名查表获取 → 文案漂移结构性消失;
//   - 门槛: gates 为属性型门槛(属性/默认阈值/INI键位); 复合门槛 gateCount=0, note 说明。
// 迁移路线图: M2 UI名称查表(已接) / M3 门槛判定查表 / M4 REFERENCE清单导出。
// 规则: 新增/修改能力先改本表, 再改机制; 文案与 REFERENCE 严格一致。
// ============================================================

#include <kenshi/Enums.h>

namespace PerkRegistry {

struct GateDef {
    int         stat;            // StatsEnumerated
    float       defThreshold;    // 默认阈值 (与 INI 缺省一致)
    const char* iniSection;      // 运行时阈值所在 INI 段 (nullptr = 固定值)
    const char* iniKey;          // 运行时阈值所在 INI 键
};

struct PerkDef {
    int         id;              // 能力唯一 id (1..99 = REFERENCE 规范条目号; 200+ = UI派生能力行)
    int         ref;             // REFERENCE 规范条目号 (0 = 无独立条目)
    const char* zh;              // 规范中文名 (与 REFERENCE 一致)
    const char* en;              // 规范英文名
    const char* uiZh;            // UI 短名 (nullptr = 用 zh)
    const char* uiEn;            // UI 短名英文 (nullptr = 用 en)
    const GateDef* gates;
    int         gateCount;
    const char* status;          // "implemented" | "revoked" | "partial"
    const char* note;            // 特殊门槛/机制说明
};

#define G1(stat_, thr_, sec_, key_) { stat_, thr_, sec_, key_ }
#define GFIX(stat_, thr_)           { stat_, thr_, nullptr, nullptr }

// ---------- 门槛组 (默认值与 INI 缺省一致) ----------
static const GateDef G_MASTERKEY[]  = { G1(STAT_LOCKPICKING,80,"Synergy","MasterKeyLock"), G1(STAT_ENGINEERING,80,"Synergy","MasterKeyEng"), G1(STAT_SCIENCE,80,"Synergy","MasterKeySci") };
static const GateDef G_SILENT[]     = { G1(STAT_STEALTH,80,"Synergy","SilentStealth"),     G1(STAT_ASSASSINATION,80,"Synergy","SilentAssassin") };
static const GateDef G_PICKPOCKET[] = { G1(STAT_STEALTH,80,"Synergy","PickpocketStealth"), G1(STAT_THIEVING,80,"Synergy","PickpocketThieving") };
static const GateDef G_TITAN[]      = { G1(STAT_LABOURING,70,"Synergy","TitanLabour"),     G1(STAT_ENGINEERING,70,"Synergy","TitanEng") };
static const GateDef G_CHEF[]       = { G1(STAT_FARMING,70,"Synergy","ChefFarm"),          G1(STAT_COOKING,70,"Synergy","ChefCook") };
static const GateDef G_CYBERTUNE[]  = { G1(STAT_MEDIC,80,"Synergy","CyberMedic"),          G1(STAT_SCIENCE,80,"Synergy","CyberSci") };

static const GateDef G_ASC_PHYS[]    = { GFIX(STAT_STRENGTH,90), GFIX(STAT_TOUGHNESS,90), GFIX(STAT_DEXTERITY,90), GFIX(STAT_ATHLETICS,90), GFIX(STAT_SWIMMING,90), GFIX(STAT_LABOURING,90) };
static const GateDef G_ASC_MARTIAL[] = { GFIX(STAT_MELEE_ATTACK,90), GFIX(STAT_MELEE_DEFENCE,90), GFIX(STAT_MARTIALARTS,90), GFIX(STAT_DODGE,90) };  // 任意3武器90 为复合门(见hook)
static const GateDef G_ASC_SHADOW[]  = { GFIX(STAT_STEALTH,90), GFIX(STAT_ASSASSINATION,90), GFIX(STAT_THIEVING,90), GFIX(STAT_LOCKPICKING,90), GFIX(STAT_PERCEPTION,90) };
static const GateDef G_ASC_CYBER[]   = { GFIX(STAT_SCIENCE,90), GFIX(STAT_ROBOTICS,90), GFIX(STAT_SMITHING_BOW,90) };   // +武器/护甲锻造90 复合门(见hook)
static const GateDef G_ASC_WILD[]    = { GFIX(STAT_MEDIC,90), GFIX(STAT_PERCEPTION,90), GFIX(STAT_COOKING,90), GFIX(STAT_FARMING,90), GFIX(STAT_SCIENCE,90), GFIX(STAT_STRENGTH,90) };

static const GateDef G_DEFMASTERY[]  = { G1(STAT_MELEE_DEFENCE,105,"Thresholds","DefenseMastery") };
static const GateDef G_COUNTER[]     = { G1(STAT_MELEE_DEFENCE,100,"Thresholds","CounterThreshold") };
static const GateDef G_HYPERARMOR[]  = { G1(STAT_STRENGTH,85,"Thresholds","HyperArmorStrReq") };
static const GateDef G_SHOCKWAVE[]   = { G1(STAT_STRENGTH,120,"Thresholds","ShockwaveStrReq") };
static const GateDef G_M_KATANA[]    = { G1(STAT_KATANAS,60,"Thresholds","MasteryThreshold") };
static const GateDef G_M_SABRE[]     = { G1(STAT_SABRES,60,"Thresholds","MasteryThreshold") };
static const GateDef G_M_HACKER[]    = { G1(STAT_HACKERS,60,"Thresholds","MasteryThreshold") };
static const GateDef G_M_BLUNT[]     = { G1(STAT_BLUNT,60,"Thresholds","MasteryThreshold") };
static const GateDef G_M_HEAVY[]     = { G1(STAT_HEAVYWEAPONS,60,"Thresholds","MasteryThreshold") };
static const GateDef G_M_POLEARM[]   = { G1(STAT_POLEARMS,60,"Thresholds","MasteryThreshold") };
static const GateDef G_M_MARTIAL[]   = { G1(STAT_MARTIALARTS,60,"Thresholds","MasteryThreshold") };
static const GateDef G_M_CROSSBOW[]  = { G1(STAT_CROSSBOWS,60,"Thresholds","MasteryThreshold") };
static const GateDef G_PERCEPTION[]  = { GFIX(STAT_PERCEPTION,20) };
static const GateDef G_MEDIC_M[]     = { G1(STAT_MEDIC,60,"Thresholds","MasteryThreshold") };
static const GateDef G_SHADOWSTEP[]  = { GFIX(STAT_DEXTERITY,60), GFIX(STAT_DODGE,60) };
static const GateDef G_FIELDNUTRITION[] = { GFIX(STAT_SCIENCE,75), GFIX(STAT_COOKING,75) };
static const GateDef G_CYBEROVER[]   = { GFIX(STAT_ROBOTICS,80) };
static const GateDef G_FIELDHEAL[]   = { GFIX(STAT_MEDIC,40) };
static const GateDef G_EVASION[]     = { G1(STAT_DODGE,60,"Thresholds","MasteryThreshold") };
static const GateDef G_TOUGHRED[]    = { GFIX(STAT_TOUGHNESS,50) };
static const GateDef G_XBOWMAST[]    = { GFIX(STAT_CROSSBOWS,40) };
static const GateDef G_PHANTOM[]     = { GFIX(STAT_DEXTERITY,100) };
static const GateDef G_LIGHTCNT[]    = { GFIX(STAT_DEXTERITY,75) };
static const GateDef G_ATHMOM[]      = { G1(STAT_ATHLETICS,60,"Thresholds","MasteryThreshold") };
static const GateDef G_SWIMDRAGON[]  = { G1(STAT_SWIMMING,60,"Thresholds","MasteryThreshold") };
static const GateDef G_QUIVER[]      = { G1(STAT_SMITHING_BOW,60,"Thresholds","MasteryThreshold") };
static const GateDef G_SMITHW[]      = { G1(STAT_SMITHING_WEAPON,60,"Thresholds","MasteryThreshold") };
static const GateDef G_SMITHA[]      = { G1(STAT_SMITHING_ARMOUR,60,"Thresholds","MasteryThreshold") };
static const GateDef G_TURRET[]      = { G1(STAT_TURRETS,60,"Thresholds","MasteryThreshold") };

// ---------- 能力表 ----------
// id: 1..99 = REFERENCE 规范条目; 200+ = UI 能力行 (ref 指向规范条目, 0 = 无独立条目)
static const PerkDef PERKS[] = {
    // —— 1. 力量与霸体对抗 (REFERENCE 1-6) ——
    { 1, 1,  "力量真实装甲穿透", "Strength Armor Penetration",       "力量穿透",   "Str Pierce",      nullptr, 0, "implemented", "力量>0 即生效, 强度随力量线性" },
    { 2, 2,  "力量差距反震",     "Strength Gap Recoil",              nullptr, nullptr,              nullptr, 0, "implemented", "攻防力量差>30 且防方未达防御大师(105); 阈值[Thresholds]StrengthGapThreshold/DefenseMastery" },
    { 3, 3,  "防御大师反震免疫", "Defense Mastery Immunity",         "防御大师",   "Defense Master",  G_DEFMASTERY, 1, "implemented", "近防>105; 阈值[Thresholds]DefenseMastery" },
    { 4, 4,  "绝境防御反伤",     "Counter Reflect Damage",           nullptr, nullptr,              G_COUNTER, 1, "implemented", "近防>100; 阈值[Thresholds]CounterThreshold" },
    { 5, 5,  "重装挥刀霸体",     "Hyper Armor",                      nullptr, nullptr,              G_HYPERARMOR, 1, "implemented", "力量>=85 且(重武器/钝器>=20 或 力量>=100); [Thresholds]HyperArmorStrReq" },
    { 6, 6,  "力拔千钧·震荡波",  "Titan Shockwave",                  "震荡波",     "Shockwave",       G_SHOCKWAVE, 1, "implemented", "力量>=120; [Thresholds]ShockwaveStrReq" },
    // —— 2. 兵刃武器谱质变 (REFERENCE 7-14) ——
    { 7, 7,  "太刀·撕裂割伤与大出血",       "Katana Hemorrhage",                    "太刀撕裂",   "Katana Tear",     G_M_KATANA, 1, "implemented", "太刀>60" },
    { 8, 8,  "军刀·御守格挡与精准反震",     "Sabre Parry & Precision Riposte",      "军刀御守",   "Saber Guard",     G_M_SABRE, 1, "implemented", "军刀>60; 命中降敌攻速5s" },
    { 9, 9,  "劈刀·重型装甲粉碎与机械特攻", "Hacker Mech Breaker",                  "劈刀专精",   "Hackers Mastery", G_M_HACKER, 1, "implemented", "劈刀>60" },
    { 10, 10, "钝器·重型碎甲与安全脑震荡",   "Blunt Armor Crusher & Concussion",     "钝器重击",   "Blunt Impact",    G_M_BLUNT, 1, "implemented", "钝器>60" },
    { 11, 11, "重武器·横扫攻击范围扩增",     "Heavy Weapon Cleave Extension",        "重武压阵",   "Heavy Onslaught", G_M_HEAVY, 1, "implemented", "重武器>60" },
    { 12, 12, "长柄武器·枪术正向距离扩增",   "Polearm Thrust Extension",             "长柄压制",   "Polearm Supremacy", G_M_POLEARM, 1, "implemented", "长柄>60; 范围放大+命中降敌攻速5s" },
    { 13, 13, "武术/空手·疾风骤雨",          "Martial Arts Flurry",                  "武术爆发",   "Martial Burst",   G_M_MARTIAL, 1, "implemented", "武术>60; 纯徒手攻速(无连击)" },
    { 14, 14, "弩手·要害贯穿与巨兽要害暴击", "Crossbow Penetration & Colossus Crit", nullptr, nullptr,               G_M_CROSSBOW, 1, "implemented", "弩系>60; 配套制箭成长化" },
    // —— 3. 身法、感知与境界威压 (REFERENCE 16-19; 15=排枪齐射, 煞气领域已撤销不入表) ——
    { 15, 15, "走火规避/精准射击·小队排枪齐射", "Squad Volley Reload",             "小队齐射",   "Squad Volley",    nullptr, 0, "implemented", "友情射击>60; 每队友装填减免" },
    { 16, 16, "铁骨要害物理御守",   "Iron Bone Vital Protection",       "御守铁骨",   "Iron Guard",      nullptr, 0, "implemented", "近防>60; 要害减伤随近防成长" },
    { 17, 17, "感知反哺战术体系",   "Perception Synergy",               "感知洞察",   "Perceptive Insight", G_PERCEPTION, 1, "implemented", "感知>=20; 近战攻防+感知/10" },
    { 18, 18, "濒死回光返照",       "Near-Death Surge",                 "回光返照",   "Last Light",      nullptr, 0, "implemented", "躯干<=20%触发, 全属性+50%, 无资格门" },
    { 19, 19, "残影瞬步·身法极速",  "Shadow Step",                      "残影瞬步",   "Afterimage Step", G_SHADOWSTEP, 2, "implemented", "敏捷>=60 && 闪避>=60" },
    // —— 4. 医疗、骨人纳米自愈与生机 (REFERENCE 20-23) ——
    { 20, 20, "止血奇迹",           "Bleed Suppression Miracle",        "止血奇迹",   "Hemostasis Miracle", nullptr, 0, "implemented", "医疗>=80; 流血削减, 100级自动造血" },
    { 21, 21, "大师造血自愈",       "Master Hemopoiesis",               nullptr, nullptr,               G_MEDIC_M, 1, "implemented", "医疗>60" },
    { 22, 22, "战地灵粮与主厨常驻", "Field Chef Auto Feed",             "战地灵粮",   "Field Rations",   G_CHEF,  2, "implemented", "" },
    { 23, 23, "纳米自修与义肢超频", "Nanite Self-Repair & Cyber Tuning", "纳米自愈",  "Nano-Repair",     nullptr, 0, "implemented", "机器人学分级: 40自愈/80磨损修复; 义肢调校走联合门(301)" },
    // —— 5. 废土特勤与神偷 (REFERENCE 24-27) ——
    { 24, 24, "妙手空空·探囊取物",         "Phantom Pickpocket",   "妙手空空", "Sleight of Hand", G_PICKPOCKET, 2, "implemented", "实机交互另需双技能>=80" },
    { 25, 25, "静谧之触·无痕暗杀与零通缉",  "Silent Touch",         "静谧之触", "Silent Touch",    G_SILENT,     2, "implemented", "" },
    { 26, 26, "神级万能钥·瞬间破锁",        "Master Decryptor",     "万能钥匙", "Master Key",      G_MASTERKEY,  3, "implemented", "" },
    { 27, 27, "搬运宗师",                   "Titan Carrier",        "搬运宗师", "Master Porter",   G_TITAN,      2, "implemented", "派生_MaxCarryWeight +100kg & 移速x1.25" },
    // —— 六、四大极境天道 (REFERENCE 六①-④; 100+ 编号) ——
    { 101, 101, "肉身成圣", "Physical Sanctification",  "肉身成圣", "Physical Sanctification", G_ASC_PHYS,    6, "implemented", "仅玩家; 锁血只保四肢(伤害时点+周期双保险); 要害负血免昏迷(KO三入口拦截); 血液锁50%; 10%免伤; 血肉回血+1.5/周期" },
    { 102, 102, "万法皆通", "Universal Weapon Mastery", "万法皆通", "Universal Weapon Mastery", G_ASC_MARTIAL, 4, "implemented", "仅玩家; 近战攻/防90+武术/闪避90+任意3武器90(复合门见hook)" },
    { 103, 103, "幽冥主宰", "Shadow Sovereign",         "幽冥主宰", "Shadow Sovereign",        G_ASC_SHADOW,  5, "implemented", "仅玩家" },
    { 104, 104, "机械至尊", "Cybernetic Overlord",      "机械至尊", "Cybernetic Overlord",     G_ASC_CYBER,   3, "implemented", "仅玩家; 科学/机器人学/弩箭锻造90+武器/护甲锻造90(复合门见hook)" },
    { 105, 105, "荒原主宰", "Wilderness Sovereign",     "荒原主宰", "Wilderness Sovereign",    G_ASC_WILD,    6, "implemented", "仅玩家; 医疗/感知/烹饪/农业/科学/力量全90; 背负动物即入队+兽群同调+兽群庇护+野性威压" },
    // —— UI 派生能力行 (200+, REFERENCE 无独立条目; ref 指向关联规范条目) ——
    { 201, 6,   "仿生义肢调校", "Prosthetic Tuning",   "义肢调校", "Prosthetic Tuning", G_CYBERTUNE, 2, "implemented", "联合专长: 义肢属性增益+25%" },
    { 202, 0,   "战地营养",     "Field Nutrition",     nullptr, nullptr, G_FIELDNUTRITION, 2, "implemented", "科学>=75 && 烹饪>=75; 饥饿-30%·自愈加速" },
    { 203, 23,  "义体超频",     "Cyber Overclock",     nullptr, nullptr, G_CYBEROVER, 1, "implemented", "机器人学>=80; 力敏增幅随技能成长(上限35%)" },
    { 204, 0,   "战地自愈",     "Battlefield Healing", nullptr, nullptr, G_FIELDHEAL, 1, "implemented", "医疗(含蜂巢医疗)>=40; 周期调理" },
    { 205, 19,  "身法宗师",     "Evasion Master",      nullptr, nullptr, G_EVASION, 1, "implemented", "闪避>60; 闪避攻速加成" },
    { 206, 0,   "韧性减免",     "Toughness Reduction", nullptr, nullptr, G_TOUGHRED, 1, "implemented", "韧性>50; 伤病惩罚减免(上限85%)" },
    { 207, 14,  "弩术精通",     "Crossbow Mastery",    nullptr, nullptr, G_XBOWMAST, 1, "implemented", "弩>=40; 远程伤害倍率" },
    { 208, 0,   "幻影身法",     "Phantom Movement",    nullptr, nullptr, G_PHANTOM, 1, "implemented", "敏捷>=100; 幻影连斩·神速多重格挡" },
    { 209, 0,   "极速反击",     "Lightning Counter",   nullptr, nullptr, G_LIGHTCNT, 1, "implemented", "敏捷>=75; 招架瞬间必定高速反击" },
    { 210, 12,  "田径动能",     "Athletics Momentum",  nullptr, nullptr, G_ATHMOM, 1, "implemented", "田径>60; 出招攻速(文案已对齐真机制)" },
    { 211, 0,   "水上蛟龙",     "Aquatic Dragon",      nullptr, nullptr, G_SWIMDRAGON, 1, "implemented", "游泳>60; 直线公式 60级起每级+0.2(100级+8, 同斜率无上限)" },
    { 212, 14,  "箭术行囊",     "Quiver Pack",         nullptr, nullptr, G_QUIVER, 1, "implemented", "弩箭锻造>=60; 等级驱动周期自动补箭" },
    { 213, 0,   "人剑合一",     "Blade Unity",         nullptr, nullptr, G_SMITHW, 1, "implemented", "武器锻造>=60; 手中武器切/钝伤+0.5%每级(100级+20%)" },
    { 214, 0,   "钢铁之躯",     "Steel Body",          nullptr, nullptr, G_SMITHA, 1, "implemented", "盔甲锻造>=60; 受击切/钝-0.5%每级+鱼叉-0.75每级(100级20%/30)" },
    { 215, 0,   "炮塔轰击",     "Turret Barrage",      nullptr, nullptr, G_TURRET, 1, "implemented", "炮塔>60; 重型击退倍率" },
    { 216, 0,   "宗师流法",     "Master Ryu",          "宗师流法", "Master Ryu", nullptr, 0, "implemented", "近战>=60; UI显示当前流法与效果; 显式切换" },
};

static const int PERK_COUNT = (int)(sizeof(PERKS) / sizeof(PERKS[0]));

static inline const PerkDef* Get(int id) {
    for (int i = 0; i < PERK_COUNT; ++i) if (PERKS[i].id == id) return &PERKS[i];
    return nullptr;
}

// UI 显示名 (编译期语言开关: DESO_ENGLISH=英)
static inline const char* UiName(int id) {
    const PerkDef* p = Get(id);
    if (!p) return "";
#ifdef DESO_ENGLISH
    return (p->uiEn && *p->uiEn) ? p->uiEn : p->en;
#else
    return (p->uiZh && *p->uiZh) ? p->uiZh : p->zh;
#endif
}

} // namespace PerkRegistry
