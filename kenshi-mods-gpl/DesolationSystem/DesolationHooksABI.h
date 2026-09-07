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
// SEH 包裹的引擎 orig 调用 (2026-09-06 崩溃复发加固):
// 本轮同址 (+0x3B984C) 流送期崩溃第 3 次 — 审计发现 27 处钩子在 __try 之外
// 裸调引擎 orig, 引擎在流送期以半初始化对象回调任一处都会在引擎内部 AV 崩进程。
// 统一收进 SEH (返回 fallback)。注意: 仅限逐对象钩子, gwMainLoop orig 保持不包裹
// (docs/05 5.29 教训4: 引擎主循环可能以内部 SEH 做流程控制)。
// ============================================================
template <typename R, typename Fn, typename... A>
static inline R CallOrigSeh(Fn fn, R fallback, A... args) {
    if (!fn) return fallback;
    __try { return fn(args...); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return fallback; }
}
template <typename Fn, typename... A>
static inline void CallOrigSehV(Fn fn, A... args) {
    if (!fn) return;
    __try { fn(args...); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// 函数指针原型定义
// ============================================================
typedef float (*GetStatFn)(CharStats*, StatsEnumerated, bool);
typedef float (*GetMeleeAttackFn)(CharStats*);
typedef float (*GetMeleeDefenceFn)(CharStats*, bool);
typedef float (*GetDodgeFn)(CharStats*, bool);
typedef float (*GetAttackPierceDamageFn)(CharStats*);
typedef float (*GetMovementSpeedFn)(CharStats*);
typedef float (*CalculateHungerMultFn)(CharStats*);
typedef float (*CalculateAttackBlockSpeedFn)(CharStats*, float, float, bool);
typedef bool  (*BlockStateFn)(void*, bool);
typedef float (*GetCurrentWeaponLengthFn)(CharStats*);
typedef void  (*ApplyDamageFn)(void*, void*, const Damages*, bool, bool, const void*);
typedef void  (*HealthPartApplyDmgFn)(void*, const Damages*);
typedef void  (*BlockHitFn)(void*, int, const Damages*, void*);
typedef void  (*GetHitFn)(void*, int, const Damages*, void*, bool);
typedef void  (*AttackImpactCheckFn)(void*);
typedef void  (*BloodlossUpdateFn)(void*, float);
typedef void  (*PeriodicUpdateFn)(void*);
typedef float (*GetRoboticsStatMultFn)(void*, StatsEnumerated);
typedef void  (*SetEquipStatBonusesFn)(CharStats*, float, float, int, int, float, int, float, float, int, float, float, float, float);
typedef float (*GetLockpickChanceFn)(Character*, void*);
typedef bool  (*SetCrimeFn)(void*, int, Faction*, const void*);
typedef void  (*NotifyCrimeWitnessedFn)(void*, Faction*, const void*, int, int);
typedef void  (*NotifyPossibleCrimeWitnessedFn)(void*, float);
typedef void  (*KnockoutFn)(void*, float);
typedef void  (*GwMainLoopFn)(GameWorld*, float);
typedef void  (*MedUpdateStatsFn)(MedicalSystem*);
typedef float (*HealthPartMaxHealthFn)(void*);
typedef float (*GetStealthSkill01Fn)(CharStats*, bool);
typedef float (*GetStealthKOChanceFn)(Character*, Character*, bool);
typedef void  (*SetStealthModeFn)(Character*, bool);
typedef float (*GunReloadTimeFn)(void*, float);      // GunClass::calculateReloadTime(void*, float stat01)
typedef void  (*GunShootFn)(void*, Character*, void*, int, const void*); // GunClass::shoot(this, me, target, stat, aimpos)

typedef CharStats*     (*CharGetStatsFn)(Character*);
typedef MedicalSystem* (*CharGetMedicalFn)(Character*);
typedef void*          (*GetPartFn)(MedicalSystem*, int, int);
typedef void*          (*GetPartByIndexFn)(MedicalSystem*, size_t);
typedef int            (*GetPartCountFn)(MedicalSystem*);
typedef float          (*GetMaxBloodFn)(MedicalSystem*);
typedef bool           (*IsRoboticFn)(void*);
typedef bool           (*HasRoboticsFn)(MedicalSystem*);
typedef void*          (*GetRaceFn)(Character*);
typedef Character*     (*IsAnimalFn)(Character*);
typedef bool           (*IsPlayerCharacterFn)(Character*);
typedef bool           (*IsStealthModeFn)(Character*);
typedef bool           (*IsInCombatModeFn)(Character*, bool, bool);
typedef void           (*AttackTargetFn)(Character*, Character*);
typedef Inventory*     (*CharGetInventoryFn)(Character*);
typedef void*          (*InvGetAllSectionsFn)(Inventory*);
typedef bool           (*InvAddItemFn)(Inventory*, Item*, int, bool, bool);
typedef bool           (*SectionRemoveItemFn)(InventorySection*, Item*);
typedef Item*          (*SectionGetItemAtFn)(InventorySection*, int, int);
typedef void           (*GetCharsWithinSphereFn)(void*, lektor<RootObject*>&, const SimpleVec3&, float, float, float, int, int, RootObject*);
typedef void           (*GetCharPositionFn)(Character*, SimpleVec3*);
typedef void           (*SayALineFn)(Character*, const std::string&, bool);
typedef Faction*       (*RootGetFactionFn)(void*);
typedef ActivePlatoon* (*CharGetPlatoonFn)(Character*);
typedef void           (*CharSetFactionFn)(Character*, Faction*, ActivePlatoon*);
typedef void           (*PlatoonAddCharFn)(ActivePlatoon*, RootObject*, int);
typedef void           (*CharHealCompletelyFn)(Character*);
typedef void           (*AddPortraitUpdateFn)(GameWorld*, const void*);
typedef void           (*InvRefreshGuiFn)(Inventory*);
typedef bool           (*SectionIsEmptyFn)(InventorySection*);
typedef const void*    (*GetSelectedPlayerCharFn)(ForgottenGUI*);
typedef const void*    (*GetSelectedObjectFn)(ForgottenGUI*);
typedef Character*     (*HandGetCharFn)(const void*);
typedef RootObjectBase* (*HandGetRootObjBaseFn)(const void*);
typedef GameData*      (*RootGetGameDataFn)(RootObjectBase*);

static GetStatFn                      getStat_orig                      = nullptr;
static GetMeleeAttackFn               getMeleeAttack_orig               = nullptr;
static GetMeleeDefenceFn              getMeleeDefence_orig              = nullptr;
static GetDodgeFn                     getDodge_orig                     = nullptr;
static GetAttackPierceDamageFn        getPierceDamage_orig              = nullptr;
static GetMovementSpeedFn             getMovementSpeed_orig             = nullptr;
static CalculateHungerMultFn          calcHungerMult_orig               = nullptr;
static CalculateAttackBlockSpeedFn    calcAttackBlockSpeed_orig         = nullptr;
static BlockStateFn                   blockState_orig                   = nullptr;
static GetCurrentWeaponLengthFn       getCurrentWeaponLength_orig       = nullptr;
static ApplyDamageFn                  applyDamage_orig                  = nullptr;
static HealthPartApplyDmgFn           healthPartApplyDmg_orig           = nullptr;
static BlockHitFn                     blockHit_orig                     = nullptr;
static GetHitFn                       getHit_orig                       = nullptr;
static AttackImpactCheckFn            attackImpactCheck_orig            = nullptr;
static BloodlossUpdateFn              bloodlossUpdate_orig              = nullptr;
static PeriodicUpdateFn               medPeriodicUpdate_orig            = nullptr;
static GetRoboticsStatMultFn          getRoboticsStatMult_orig          = nullptr;
static SetEquipStatBonusesFn          setEquipStatBonuses_orig          = nullptr;
static GetLockpickChanceFn            getLockpickChance_orig            = nullptr;
static SetCrimeFn                     setCrime_orig                     = nullptr;
static NotifyCrimeWitnessedFn         notifyCrimeWitnessed_orig         = nullptr;
static NotifyPossibleCrimeWitnessedFn notifyPossibleCrimeWitnessed_orig = nullptr;
static KnockoutFn                     knockout_orig                     = nullptr;
typedef void  (*KnockoutNoArgFn)(void*);
static KnockoutNoArgFn                startKnockoutTimer_orig           = nullptr;  // MedicalSystem::startKnockoutTimer (KO入口2)
static KnockoutFn                     knockoutForceTimer_orig           = nullptr;  // MedicalSystem::knockoutForceTimer (KO入口3)
static GwMainLoopFn                   gwMainLoop_orig                   = nullptr;
static MedUpdateStatsFn               medUpdateStats_orig               = nullptr;
static HealthPartMaxHealthFn          healthPartMaxHealth_orig          = nullptr;
static GetStealthSkill01Fn            getStealthSkill01_orig            = nullptr;
static GetStealthKOChanceFn           getStealthKOChance_orig           = nullptr;
static SetStealthModeFn               setStealthMode_orig               = nullptr;
static GunReloadTimeFn                gunReloadTime_orig                = nullptr;
static GunShootFn                     gunShoot_orig                     = nullptr;

static CharGetStatsFn                 charGetStats_orig                 = nullptr;
static CharGetMedicalFn               charGetMedical_orig               = nullptr;
static GetPartFn                      getPart_orig                      = nullptr;
static GetPartByIndexFn               getPartByIndex_orig               = nullptr;
static GetPartCountFn                 getPartCount_orig                 = nullptr;
static GetMaxBloodFn                  getMaxBlood_orig                  = nullptr;
static IsRoboticFn                    isRobotic_orig                    = nullptr;
static HasRoboticsFn                  hasRobotics_orig                  = nullptr;
static GetRaceFn                      getRace_orig                      = nullptr;
static IsAnimalFn                     isAnimal_orig                     = nullptr;
static IsPlayerCharacterFn            isPlayerChar_orig                 = nullptr;
static IsStealthModeFn                isStealthMode_orig                = nullptr;
static IsInCombatModeFn               isInCombatMode_orig               = nullptr;
static AttackTargetFn                 attackTarget_orig                 = nullptr;
static CharGetInventoryFn             charGetInventory_orig             = nullptr;
static InvGetAllSectionsFn            invGetAllSections_orig            = nullptr;
static InvAddItemFn                   invAddItem_orig                   = nullptr;
static SectionRemoveItemFn            sectionRemoveItem_orig            = nullptr;
static SectionGetItemAtFn             sectionGetItemAt_orig             = nullptr;
static GetCharsWithinSphereFn         getCharsWithinSphere_orig         = nullptr;
static GetCharPositionFn              charGetPosition_orig              = nullptr;
static SayALineFn                     sayALine_orig                     = nullptr;
static RootGetFactionFn               rootGetFaction_orig               = nullptr;
static CharGetPlatoonFn               charGetPlatoon_orig               = nullptr;
static CharSetFactionFn               charSetFaction_orig               = nullptr;
static PlatoonAddCharFn               platoonAddChar_orig               = nullptr;
static CharHealCompletelyFn           charHealCompletely_orig           = nullptr;
static AddPortraitUpdateFn            addPortraitUpdate_orig            = nullptr;
static InvRefreshGuiFn                invRefreshGui_orig                = nullptr;
static SectionIsEmptyFn               sectionIsEmpty_orig               = nullptr;
static GetSelectedPlayerCharFn        getSelectedPlayerChar_orig        = nullptr;
static GetSelectedObjectFn            getSelectedObject_orig            = nullptr;
static HandGetCharFn                  handGetChar_orig                  = nullptr;
static HandGetRootObjBaseFn           handGetRootObjBase_orig           = nullptr;
static RootGetGameDataFn              rootGetGameData_orig              = nullptr;

static ForgottenGUI**                 g_guiPtrPtr                       = nullptr;
static GameWorld**                    g_gameWorldPtrPtr                 = nullptr;

static void DiagnosticLog(const std::string& msg) {
    static std::string logPath = "";
    if (logPath.empty()) {
        char exePath[MAX_PATH] = { 0 };
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) *lastSlash = '\0';
        logPath = std::string(exePath) + "\\Desolation_Diagnostic.log";
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeBuf[64];
    snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    // P2 治理: 日志轮转 - 超过 2MB 时归档为 .old (保留最近两代, 防无限增长)
    {
        WIN32_FILE_ATTRIBUTE_DATA fad = { 0 };
        if (GetFileAttributesExA(logPath.c_str(), GetFileExInfoStandard, &fad)) {
            ULONGLONG fileSz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
            if (fileSz >= (2ULL << 20)) {
                std::string oldPath = logPath + ".old";
                DeleteFileA(oldPath.c_str());
                MoveFileA(logPath.c_str(), oldPath.c_str());
                FILE* fpNew = nullptr;
                fopen_s(&fpNew, logPath.c_str(), "a");
                if (fpNew) {
                    fprintf(fpNew, "[DesolationSystem] === 诊断日志已轮转 (2MB 上限), 旧档: Desolation_Diagnostic.log.old ===\n");
                    fflush(fpNew);
                    fclose(fpNew);
                }
            }
        }
    }

    FILE* fp = nullptr;
    fopen_s(&fp, logPath.c_str(), "a+");
    if (fp) {
        fprintf(fp, "%s%s\n", timeBuf, msg.c_str());
        fflush(fp);
        fclose(fp);
    }
    ErrorLog(msg);
}

// 日志限频标准封装 (2026-09-06 固化): 机制触发类日志(战斗/医疗高频路径)必须走此函数,
// key=调用点唯一标识, intervalMs=最小间隔。同 key 在窗口内静默, 防日志风暴。
// 注意: 非线程安全(单锁预期), 仅限主循环/医疗 hook 单线程上下文使用。
static void DiagnosticLogThrottled(const char* key, DWORD intervalMs, const std::string& msg) {
    struct Slot { std::string k; DWORD t; };
    static Slot s_slots[8] = {};
    static int  s_next = 0;
    DWORD now = GetTickCount();
    for (auto& s : s_slots) {
        if (s.k == key) {
            if (now - s.t < intervalMs) return;
            s.t = now;
            DiagnosticLog(msg);
            return;
        }
    }
    s_slots[s_next].k = key;
    s_slots[s_next].t = now;
    s_next = (s_next + 1) % 8;
    DiagnosticLog(msg);
}

static inline void* GetKenshiLibExport(const char* name) {
    HMODULE h = GetModuleHandleA("KenshiLib.dll");
    if (!h) return nullptr;
    return (void*)GetProcAddress(h, name);
}

static inline bool IsUsableObject(const void* p, size_t minBytes) {
    // P3 修复: 调用引擎方法前的基础可达性校验, 拦截野/悬垂指针 (如 0x319930522 崩溃特征)
    if (!p) return false;
    return IsBadReadPtr(p, minBytes) == FALSE;
}

// P3 审计: RaceData 官方字段 robot@0x7C (RaceData 无 vtable, 0x00 起为 specialFoods 容器,
// 并非 std::string); 旧代码把 race+0x08 当 std::string 做名字 find = 伪读取 (SEH 兜底但判据错),
// 骨人/机械判定统一走官方 robot 标志 + hasRobotics/isRobotic。
static inline bool RaceIsRobot(const void* race) {
    if (!race || !IsUsableObject(race, 0x80)) return false;
    return *(const bool*)((const char*)race + 0x7C);
}

// ============================================================
// 绝境系统统一动物判定 (用户方案 2026-09-06, 第五天道引入):
// 非人(isHuman导出) && 非骨人/机械种族(RaceIsRobot) && 无机械部件(hasRobotics)
// && [可选]身上无任何物品 => 视为动物。四条高置信负判对 mod 生物天然兼容,
// 取代在 mod 环境下不可靠的正向 IsAnimal 识别。
// checkEquipment: 收编场景开(玩家/具装生物不误收); 队内场景(威压/兽群同调/计数)关
// (已招募动物属玩家方且可能被 mod 塞物品, 关掉才不误排)。
// ============================================================
typedef void* (*DesoIsHumanFn)(Character*);
static DesoIsHumanFn deso_isHuman_fn = nullptr;

static inline bool DesoHasWeaponOrClothing(Character* c) {
    if (!charGetInventory_orig || !invGetAllSections_orig) return true;
    __try {
        Inventory* inv = charGetInventory_orig(c);
        const void* ls = inv ? invGetAllSections_orig(inv) : nullptr;
        if (!ls) return false;
        const uint32_t* hdr = (const uint32_t*)ls;
        uint32_t cnt = hdr[0]; if (cnt > 24) cnt = 24;
        InventorySection** arr = *(InventorySection***)((const char*)ls + 8);
        if (!arr) return false;
        for (uint32_t i = 0; i < cnt; ++i) {
            InventorySection* sec = arr[i];
            if (!sec) continue;
            char* b = *(char**)((char*)sec + 0x40);
            char* e = *(char**)((char*)sec + 0x48);
            if (!b || !e || e <= b) continue;
            size_t n = (size_t)(e - b) / 0x10;
            if (n > 512) n = 512;
            for (size_t k = 0; k < n; ++k) {
                void* item = *(void**)(b + k * 0x10);
                if (!item) continue;
                __try {
                    int fn = *(int*)((char*)item + 0x124);   // ItemFunction @0x124
                    if (fn == 5 /*ITEM_WEAPON*/ || fn == 6 /*ITEM_CLOTHING*/) return true;
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DiagnosticLogThrottled("DesoAnimalStep", 5000, "[判畜·步骤] 背包武器/装备扫描抛异常(按有装备处理)");
        return true;
    }
}

// 统一动物判定 — 每步引擎调用各自 SEH + 点名日志 (2026-09-07: 骨犬场景单一大保护块
// 出现异常逃逸, 逐步隔离后既免疫雪崩又可定位元凶; 单步判不了时按"继续判"的中性值)
// 荒原主宰角色自动登记 (定义在 System.cpp, periodicUpdate 全角色上报)
static void WildNoteCharacter(Character* ch);

static inline bool DesoIsAnimal(Character* c, bool checkEquipment) {
    if (!c) return false;
    bool human = false;
    __try { human = deso_isHuman_fn ? (deso_isHuman_fn(c) != nullptr) : false; }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DiagnosticLogThrottled("DesoAnimalStep", 5000, "[判畜·步骤] isHuman 抛异常(按非人处理)");
        human = false;
    }
    if (human) return false;
    bool robotRace = false;
    __try { robotRace = getRace_orig ? RaceIsRobot(getRace_orig(c)) : false; }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DiagnosticLogThrottled("DesoAnimalStep", 5000, "[判畜·步骤] 种族机械判定抛异常(按非机械处理)");
        robotRace = false;
    }
    if (robotRace) return false;
    bool roboticParts = false;
    __try {
        MedicalSystem* tMed = charGetMedical_orig ? charGetMedical_orig(c) : nullptr;
        roboticParts = (tMed && hasRobotics_orig) ? (hasRobotics_orig(tMed) != false) : false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DiagnosticLogThrottled("DesoAnimalStep", 5000, "[判畜·步骤] hasRobotics 抛异常(按无机械件处理)");
        roboticParts = false;
    }
    if (roboticParts) return false;
    if (checkEquipment) {
        bool isPlayer = false;
        __try { isPlayer = isPlayerChar_orig ? (isPlayerChar_orig(c) != false) : false; }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            DiagnosticLogThrottled("DesoAnimalStep", 5000, "[判畜·步骤] isPlayerChar 抛异常(按玩家方处理)");
            isPlayer = true;
        }
        if (isPlayer) return false;
        if (DesoHasWeaponOrClothing(c)) return false;
    }
    return true;
}

// P3 动物判据增强: 除了引擎 isAnimal, 再用 RaceData 特征标志兜底
// (mod 动物/特殊种族可能不被 isAnimal 识别, 但不满足人类正常穿戴 flags -> 仍按动物处理)
static inline bool IsAnimal(Character* ch) {
    if (!ch) return false;
    // P3: 内部 SEH - 本函数可能在调用方 __try 之外被执行 (如属性 hook 开头的
    // IsApplicableStats 检查), 引擎方法对半初始化/异常角色调用可能 AV, 必须就地拦截
    __try {
        if (isAnimal_orig && IsUsableObject(ch, 32)) {
            if (isAnimal_orig(ch) != nullptr) return true;
        }
        // 兜底: 引擎 isAnimal 对 mod 动物/特殊种族可能识别失败返回 0,
        // 用 RaceData 特征标志再判 (动物通常 noShirts+noHats+noShoes 同真 或 gigantic).
        if (getRace_orig) {
            void* race = getRace_orig(ch);
            if (race && IsUsableObject(race, 0x80)) {
                bool gigantic = *(bool*)((char*)race + 0x78);
                if (gigantic) return true;
                bool noShirts = *(bool*)((char*)race + 0x7F);
                bool noHats   = *(bool*)((char*)race + 0x7E);
                bool noShoes  = *(bool*)((char*)race + 0x80);
                if (noShirts && noHats && noShoes) return true;   // 人类/骨人/蜂人总有至少一种穿戴, 动物三者皆无
                bool isRobot = *(bool*)((char*)race + 0x7C);
                if (isRobot) return false;   // 骨人/机器种族明确非动物
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }  // 无法判定时按动物处理(跳过加成)
    return false;
}

// P3 审计修正: 官方 CharStats.h me@0x10 (CharStats 有 vtable@0x00);
// 旧代码读 +0x08 (非 me 字段) 导致 owner 判定对象错误
static inline Character* GetOwnerCharacter(CharStats* s) {
    if (!s || !IsUsableObject(s, 0x18)) return nullptr;
    Character* ch = *(Character**)((char*)s + 0x10);
    if (ch && !IsUsableObject(ch, 32)) return nullptr;   // P3: 野 owner 拦截
    return ch;
}

// ============================================================
// P3 审计修复: MedicalSystem -> Character 权威反查
// 官方 KenshiLib MedicalSystem.h: MedicalSystem 无 Character 反指字段
// (0x08 起为 status map); 正确链路 = 肢体裸指针成员
// leftLeg@0x80 rightLeg@0x88 leftArm@0x90 rightArm@0x98
// -> HealthPartStatus::me @0x18 (Character*)
// 旧 MED_ME(m)=*(Character**)(m+0x08) 读 map 内部数据 = 伪指针,
// 导致 6 个医疗 hook 的角色判定全错 (机制静默失效 + 潜在写坏路径)。
// ============================================================
static inline Character* MedGetOwnerCharacter(void* m) {
    if (!m || !IsUsableObject(m, 0xA0)) return nullptr;
    const size_t limbOffsets[4] = { 0x80, 0x88, 0x90, 0x98 };
    for (int i = 0; i < 4; ++i) {
        void* part = *(void**)((char*)m + limbOffsets[i]);
        if (!part || !IsUsableObject(part, 0x20)) continue;
        Character* ch = *(Character**)((char*)part + 0x18);
        if (ch && IsUsableObject(ch, 32)) return ch;
    }
    return nullptr;
}

static inline bool IsApplicable(Character* ch) {
    if (!ch) return false;
    if (g_targetScope == 0) return true;
    // P3: 内部 SEH - scope=1/2 时需调 isPlayerChar_orig 判定; 载入存档期引擎对
    // 半初始化角色调用属性 getter -> 本函数在 hook 开头 __try 外执行 -> 引擎方法
    // AV 会逃逸成进程闪退 (scope=1 载入必崩根因, 0x319930522 特征)
    __try {
        bool isPlayer = (isPlayerChar_orig && isPlayerChar_orig(ch));
        if (g_targetScope == 1) return isPlayer;
        if (g_targetScope == 2) return !isPlayer;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static inline bool IsApplicableStats(CharStats* s) {
    Character* ch = GetOwnerCharacter(s);
    return IsApplicable(ch);
}

static inline float SafeGetStat(CharStats* s, StatsEnumerated what, bool unmodified = true) {
    if (!s || !getStat_orig) return 0.0f;
    return getStat_orig(s, what, unmodified);
}

// P3 修复: 输出消毒 - 任何派生属性若产生 NaN/Inf (存档字段被旧版写坏等),
// 一律回退原版值, 杜绝 NaN 传播进引擎动画/战斗结算导致延迟崩溃。
static inline float SanitizeStat(float modified, float original) {
    if (!std::isfinite(modified)) return original;
    return modified;
}

static inline float GetMasteryScale(float skillVal) {
    if (skillVal <= 60.0f) return 0.0f;
    if (skillVal <= 100.0f) {
        float f = (skillVal - 60.0f) / 40.0f;
        return f * f;
    }
    return 1.0f + (skillVal - 100.0f) / 25.0f;
}

// 部位机械判定 (2026-09-06 崩溃排查加固): 官方 isRobotic() 比裸读 robotLimb@0x28 更权威,
// 但它是引擎方法调用 — 流送期半初始化部位上调用可能内部解引用悬垂指针
// (crash_reports/20260906 两次 kenshi_x64.exe+0x3B984C 流送期读违例)。
// 故先用官方关联字段 (medical@0x10 / me@0x18) 双重校验部位一致性, 不一致直接回落纯读判据。
static inline bool PartIsRoboticVerified(void* med, Character* me, void* part) {
    if (!part || !IsUsableObject(part, 0x60)) return false;
    if (isRobotic_orig) {
        __try {
            void* pm = *(void**)((char*)part + 0x10);
            void* po = *(void**)((char*)part + 0x18);
            if (pm == med && po == me) return isRobotic_orig(part) != 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    return PART_IS_ROBOTIC(part);
}

