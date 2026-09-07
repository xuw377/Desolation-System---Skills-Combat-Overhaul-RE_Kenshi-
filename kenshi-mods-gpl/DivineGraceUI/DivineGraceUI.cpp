// ============================================================================
// 本文件是 绝境系统 UI (Divine Grace UI) 的一部分。
// Copyright (C) 2026 kenshi-mod-dev
//
// 本程序是自由软件：你可以根据自由软件基金会发布的 GNU 通用公共许可证
// (第 3 版或更高版本) 再分发和/或修改它。
// 本程序的发布希望它有用，但【不提供任何保证】。
// 详见 GNU 通用公共许可证 (LICENSE 文件)。
//
// This file is part of 绝境系统 UI (Divine Grace UI).
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

//
// KenshiLib plugin: 绝境系统 UI 专属悬浮面板 (主副双面板架构)
//
// 架构设计：
//   1. 主面板（上方）：展示 27 项常规战斗、特勤、体魄与生活专长
//   2. 副面板（紧贴下方）：专属展示四大极境天道（底部按钮切换）
//   3. [F8] 快捷键：一键同时显隐主副双面板
//   4. 模式/攻速/极境切换：底部可点击控制条 (F8/F6/9+0 快捷键已移除)
//
// 部署: Kenshi/mods/DesolationSystem/

#include <Debug.h>
#include <Defines.h>
#include <core/Functions.h>

#include <ogre/OgrePrerequisites.h>
#include <kenshi/Enums.h>
#include <kenshi/CharStats.h>
#include <kenshi/gui/DataPanelLine.h>

#include <windows.h>
#include <string>
#include <vector>
#include <map>

// 双语字符串宏: 编译期选语言 (一份源码, 两种语言 DLL)
//   编译英文版: -DDESO_ENGLISH ; 否则中文版
#ifdef DESO_ENGLISH
#  define L_(zh, en) en
#else
#  define L_(zh, en) zh
#endif

// 前向声明
class Character;

// ============================================================
// 函数类型
// ============================================================
typedef float (*GetStatFn)(CharStats* thisptr, StatsEnumerated what, bool unmodified);
typedef void (*GetGUIDataFn)(CharStats* thisptr, void* datapanel, int category, bool combatMode);
typedef void (*FguiUpdateFn)(void* fgui);
typedef void* (*CreateDatapanelFn)(void* fgui, float x, float y, float w, float h, bool visible,
                                   const std::string& name, const std::string& skin);
typedef void (*SetCaptionFn)(void* datapanel, const std::string& caption);
typedef void (*SetNameFn)(void* datapanel, const std::string& name);
typedef void (*SetPosRealFn)(void* datapanel, float x, float y);
typedef void (*ResizeFn)(void* datapanel, int w, int h);
typedef void (*ShowFn)(void* datapanel, bool on);
typedef bool (*IsVisibleFn)(void* datapanel);
typedef DataPanelLine* (*SetLineFn)(void* datapanel, const std::string& s1, const std::string& s2, int category);
typedef void (*ClearPageFn)(void* datapanel, int category);
typedef void* (*GetSelectedFn)(void* fgui);
typedef Character* (*HandGetCharFn)(void* hand);
typedef CharStats* (*CharGetStatsFn)(Character* ch);
typedef void* (*IsAnimalFn)(Character* thisptr);
typedef void* (*GetRaceFn)(Character* thisptr);

typedef int (*GetScopeModeFn)();
typedef bool (*GetAscensionFn)();
typedef float (*GetAttackSpeedCapFn)();
typedef void (*SetScopeModeFn)(int mode);
typedef void (*SetAscensionFn)(bool enabled);
typedef void (*SetAttackSpeedCapFn)(float cap);

static GetStatFn         getStat_orig        = 0;
static GetGUIDataFn      getGUIData_orig     = 0;
static FguiUpdateFn      fguiUpdate_orig     = 0;
static CreateDatapanelFn createDatapanel_fn  = 0;
static SetCaptionFn      setCaption_fn       = 0;
static SetNameFn         setPanelName_fn     = 0;
static SetPosRealFn      setPositionReal_fn  = 0;
static ResizeFn          resize_fn           = 0;
static ShowFn            show_fn             = 0;
static IsVisibleFn       isVisible_fn        = 0;
static SetLineFn         setLineStatInfo_fn  = 0;
static ClearPageFn       clearPage_fn        = 0;
static void             (*clear_fn)(void*)  = 0;   // 全清所有行 (?clear@DatapanelGUI@@UEAAXXZ, 无参)
static void             (*updatePanel_fn)(void*) = 0; // 强制刷新面板行布局 (?update@DatapanelGUI@@UEAAXXZ)
static GetSelectedFn     getSelected_fn      = 0;
static HandGetCharFn     handGetChar_fn      = 0;
static CharGetStatsFn    charGetStats_fn     = 0;
static IsAnimalFn        isAnimal_fn         = 0;
static GetRaceFn         getRace_fn          = 0;
static GetScopeModeFn        getScopeMode_fn        = 0;
static GetAscensionFn        getAscension_fn        = 0;
static GetAttackSpeedCapFn   getAttackSpeedCap_fn   = 0;
static SetScopeModeFn        setScopeMode_fn        = 0;
static SetAscensionFn        setAscension_fn        = 0;
static SetAttackSpeedCapFn   setAttackSpeedCap_fn   = 0;

// ============================================================
// 统一 UI 布局: 以窗口实际尺寸按比例换算 (自适应分辨率/缩放)
// 设计基准: 1920x1080. 位置用归一化 setPositionReal, 尺寸用随窗口缩放的像素 resize.
// 关键: 所有面板(主/副/控制)共用同一套比例 -> 任意分辨率下相对布局恒定, 无缝连体.
// ============================================================
static float g_uiWinW = 1920.0f, g_uiWinH = 1080.0f;   // 实际窗口 client 像素 (DPI-aware 为物理像素)

// 游戏主窗口定位 (2026-09-06 修正): 旧读 GetForegroundWindow — 载入/切窗时前台可能
// 不是游戏, 导致读错分辨率 (低分辨率布局挤压实测)。改为按本进程可见主窗口定位。
static HWND GetGameWindowHwnd()
{
    static HWND s_hwnd = 0;
    if (s_hwnd && IsWindow(s_hwnd)) return s_hwnd;
    struct Ctx { HWND found; DWORD pid; } ctx = { 0, GetCurrentProcessId() };
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        Ctx* c = (Ctx*)lp;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == c->pid && IsWindowVisible(hwnd)) {
            wchar_t title[128] = { 0 };
            GetWindowTextW(hwnd, title, 128);
            if (title[0]) { c->found = hwnd; return FALSE; }
        }
        return TRUE;
    }, (LPARAM)&ctx);
    s_hwnd = ctx.found;
    return s_hwnd;
}

static void UiReadWindowSize()
{
    HWND fg = GetGameWindowHwnd();
    if (!fg) return;
    RECT rc;
    if (GetClientRect(fg, &rc)) {
        if (rc.right > 0) g_uiWinW = (float)rc.right;
        if (rc.bottom > 0) g_uiWinH = (float)rc.bottom;
    }
    // 探针: 记录窗口客户区尺寸 + 屏幕逻辑/物理尺寸 + DPI 缩放, 用于诊断布局错位
    static bool s_probed = false;
    if (!s_probed) {
        s_probed = true;
        UINT dpi = GetDpiForWindow(fg);
        HDC hdc = GetDC(fg);
        int logW = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(fg, hdc);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoA(MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST), &mi);
        char b[256];
        sprintf_s(b, "UiReadWindowSize: client=%dx%d dpi=%u logpx=%d monitor=%dx%d",
            rc.right, rc.bottom, dpi, logW, mi.rcMonitor.right-mi.rcMonitor.left, mi.rcMonitor.bottom-mi.rcMonitor.top);
        ErrorLog(b);
    }
}

// 基准比例 -> 实际像素 (跟着窗口缩放)
static int UiScaleX(float frac) { return (int)(g_uiWinW * frac); }
static int UiScaleY(float frac) { return (int)(g_uiWinH * frac); }
static float UiNormX(float frac) { return frac; }   // 归一化位置已经是 0~1, 直接等同窗口比例
static float UiNormY(float frac) { return frac; }

// 面板布局常量 (以设计基准 1920x1080 的比例定义, 运行时按窗口缩放)
#define PANEL_POSX 0.66f       // 面板左边缘 (归一化, 给小UI留左侧空间)
#define PANEL_POSY 0.052f      // 主面板顶部 (归一化; 顶部 0.015 留给翻页钮一行)
#define PANEL_FW   0.25f       // 面板宽 (占屏宽比例)
#define PANEL_FH   0.37f       // 主面板高 (占屏高比例)
// (主副连体布局已废弃: 单面板三页翻页制, 2026-09-06)

// ============================================================
// 面板指针与状态管理
static void*      g_gracePanel         = 0;  // 单面板 (三页翻页制: 常规上/常规下/极境)
static int        g_currentPage        = 0;  // 当前页 0/1/2 (翻页钮点循环)
static bool       g_created            = false;
static bool       g_needCreate         = false;
static Character* g_displayedChar      = 0;
static bool       g_panelVisible       = true;
static bool       g_uiHiddenByUser     = false;  // F8 用户隐藏意图 (持久): 跨面板丢弃/重建/引擎re-show保持, 仅 F8 可翻转
static bool       g_f8LastDown         = false;
static int        g_lastRenderedScope  = -1;
static bool       g_lastAscState       = true;
static int        g_refreshTimer       = 0;

// ============================================================
// 控制条 (可点击面板): 生效模式 / 攻速上限 / 极境天道 切换
// ============================================================
enum {
    CTRL_SCOPE = 0,     // 生效模式 (0全局/1玩家/2NPC, 点循环)
    CTRL_SPEED,         // 攻速上限 (1.2/2.4/3.6/4.8/9.6, 点循环)
    CTRL_ASC,           // 极境天道 (toggle)
    CTRL_RYU,           // 流法切换 (近战>=60; 点击循环 疾风/中庸/重击)
    CTRL_COUNT
};
// 控制条: 3 个独立小卡 (每个 setCaption 一行, 无行距错位).
// 卡片用固定像素小尺寸 (同断肢 ~36px 高), 避免比例缩放出高卡触发皮肤两层渲染(重影).
struct CtrlCard {
    void* handle;
    float x, y, w, h;
    int   type;         // CTRL_*
};
static CtrlCard g_ctrl[CTRL_COUNT];
static CtrlCard g_pageBtn;   // 翻页钮 (单面板顶部横排, 点循环 3 页)
static bool   g_ctrlCreated  = false;
static bool   g_mousePrevDown = false;
static DWORD  g_lastClickTick = 0;
static float  g_ctrlTop      = 0.70f;  // 控制条顶y
// 固定像素卡片尺寸 (同断肢: 300x36) + 右对齐副UI (x=0.814 使右边缘=0.97=副UI右缘)
static const int CTRL_WPX = 300;   // 每卡像素宽 (同断肢)
static const int CTRL_HPX = 36;    // 每卡像素高 (同断肢)
static const int CTRL_GAPPX = -2;  // 卡间间隙 (同断肢 -2px 负重叠贴合)
static const float CTRL_POSX = 0.504f;   // 控制条左x (右缘贴主UI左缘=0.66)
static const int   PAGEBTN_WPX = 300;    // 翻页钮像素宽 (同控制卡)
static const int   PAGEBTN_HPX = 36;     // 翻页钮像素高
static const float PAGEBTN_Y = 0.015f;   // 翻页钮顶y (贴屏幕顶, 主面板在其下方)

// P3 修复: GUI 生命周期防护 (Kenshi 会在载入/切场景时重建 GUI 并释放注入者创建的
// DatapanelGUI, 却不通知注入者 - 参见 RE_Kenshi 源码 ReHookTimeButtons 注释)。
// 策略: 绝不缓存跨代面板; 检测到"长时间无选中角色"(界面切换/载入) 即静默丢弃引用,
// 待角色重新选中时基于当前 GUI 重建 (旧面板交给引擎自己清理, 绝不手动 destroy 防双释)。
static int         g_noCharFrames      = 0;

// ============================================================
// 生效模式获取与映射
// ============================================================
static const char* GetScopeName(int scope)
{
    switch (scope)
    {
        case 1:  return L_("只玩家", "Player Only");
        case 2:  return L_("只NPC", "NPC Only");
        case 0:
        default: return L_("全局模式", "Global Mode");
    }
}

static int GetCurrentScopeMode()
{
    if (getScopeMode_fn == 0)
    {
        HMODULE hDeso = GetModuleHandleA("DesolationSystem.dll");
        if (hDeso)
        {
            getScopeMode_fn = (GetScopeModeFn)GetProcAddress(hDeso, "GetDesolationScopeMode");
        }
    }
    if (getScopeMode_fn)
    {
        __try { return getScopeMode_fn(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // 回退机制：读取 ini
    HMODULE hDll = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&GetCurrentScopeMode, &hDll) && hDll)
    {
        char dllPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hDll, dllPath, MAX_PATH) > 0)
        {
            char* lastSlash = strrchr(dllPath, '\\');
            if (lastSlash)
            {
                *lastSlash = '\0';
                std::string iniPath = std::string(dllPath) + "\\DesolationSystem.ini";
                // P3 修复: 回退段名错误 ("DesolationSystem" -> "Settings", 与游戏内一致)
                return (int)GetPrivateProfileIntA("Settings", "TargetScope", 0, iniPath.c_str());
            }
        }
    }
    return 0;
}

static bool GetAscensionEnabled()
{
    if (getAscension_fn == 0)
    {
        HMODULE hDeso = GetModuleHandleA("DesolationSystem.dll");
        if (hDeso)
        {
            getAscension_fn = (GetAscensionFn)GetProcAddress(hDeso, "GetAscensionPerksEnabled");
        }
    }
    if (getAscension_fn)
    {
        __try { return getAscension_fn(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return true;
}

static float GetCurrentAttackSpeedCap()
{
    if (getAttackSpeedCap_fn == 0)
    {
        HMODULE hDeso = GetModuleHandleA("DesolationSystem.dll");
        if (hDeso)
        {
            getAttackSpeedCap_fn = (GetAttackSpeedCapFn)GetProcAddress(hDeso, "GetDesolationAttackSpeedCap");
        }
    }
    if (getAttackSpeedCap_fn)
    {
        __try { return getAttackSpeedCap_fn(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return 4.8f;
}

static void UpdatePanelTitles(int scope, bool ascEnabled)
{
    __try
    {
        if (g_gracePanel && setCaption_fn)
        {
            std::string title = std::string(L_("绝境系统", "Desolation System")) + " " + GetScopeName(scope) + L_(" [F8 显隐]", " [F8 toggle]");
            setCaption_fn(g_gracePanel, title);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static inline bool IsGameWindowFocused() {
    HWND fgWnd = GetForegroundWindow();
    if (!fgWnd) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fgWnd, &pid);
    return (pid == GetCurrentProcessId());
}

// ============================================================
// 辅助判定
// ============================================================
static bool IsAnimal(Character* ch)
{
    if (ch == 0) return false;
    __try {
        if (isAnimal_fn && isAnimal_fn(ch) != 0) return true;
        if (getRace_fn) {
            void* race = getRace_fn(ch);
            if (race) {
                bool gigantic = *(bool*)((char*)race + 0x78);
                if (gigantic) return true;
                bool isRobot = *(bool*)((char*)race + 0x7C);
                if (isRobot) return false;
                bool noShirts = *(bool*)((char*)race + 0x7F);
                bool noHats   = *(bool*)((char*)race + 0x7E);
                bool noShoes  = *(bool*)((char*)race + 0x80);
                if (noShirts && noHats && noShoes) return true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return false;
}

static const float MASTERY_THRESHOLD = 60.0f;

static float GetMasteryScale(float stat)
{
    if (stat <= MASTERY_THRESHOLD) return 0.0f;
    if (stat <= 100.0f)
    {
        float ratio = (stat - MASTERY_THRESHOLD) / (100.0f - MASTERY_THRESHOLD);
        return ratio * ratio;
    }
    else
    {
        float beyond100 = stat - 100.0f;
        return 1.0f + (beyond100 / 25.0f);
    }
}

static float SafeStat(CharStats* stats, StatsEnumerated what)
{
    if (stats == 0 || getStat_orig == 0) return 0.0f;
    __try { return getStat_orig(stats, what, true); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
}

#include "DesolationPerkRegistry.h"   // 专长/能力元数据单一事实源 (M2: UI 文案名查表)

// 能力短名查表 (注册表唯一来源; 消灭 UI 文案与 REFERENCE 漂移)
static std::string PName(int id) { return PerkRegistry::UiName(id); }

// 门槛查表 (M3): 实际门槛判定由 DesolationSystem.dll 导出 (注册表+运行时INI阈值)。
// 导出缺失(旧版 Deso)时回退为"视为达标"——文案行仍会显示, 属可接受的降级。
typedef bool (*PerkGateFn)(int, void*);
static PerkGateFn g_gateFn = 0;
static bool GateOK(int id, Character* ch) {
    if (!g_gateFn) return true;
    __try { return g_gateFn(id, ch) != 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// 行收集槽 (单面板翻页制): 置非空时 SetGraceLine 只入缓冲不写面板,
// 收集器代码零改动即可复用为"取数"模式 (渲染器再按页切片写面板)
struct UILine { std::string key; std::string text; };
static std::vector<UILine>* g_lineSink = 0;
// 定期刷新瘦身: 上次全量重建的行 key 签名与面板身份 (原地更新判定用, 见 RenderCharacterPanels)
static std::string g_lastPageKeysSig;
static void*       g_lastPageKeysPanel = 0;

// 视图坐标系标定 (2026-09-06, 布局挤压根因修复):
// 我们假设 resize(px) 的单位 = 窗口客户区像素, 但各分辨率实测布局比例偏移,
// 说明引擎 MyGUI 视图空间 ≠ 客户区像素。方案: 建面板后用官方导出的
// GUIWindow::getWidth/getHeight 读回控件真实尺寸, 与 resize 传入值相比
// 得出换算系数 g_viewScale, 所有像素尺寸统一乘它 — 不再假设坐标系。
// ============================================================
typedef int (*GetWFn)(const void*);
typedef int (*GetPackCountFn)();
static GetPackCountFn packCount_fn = 0;
typedef void (*RyuSetStyleFn)(void*, int);
typedef int  (*RyuGetStyleFn)(void*);
static RyuSetStyleFn ryuSet_fn = 0;
static RyuGetStyleFn ryuGet_fn = 0;
static GetWFn getWidth_fn = 0, getHeight_fn = 0;
static float g_viewScale = 1.0f;   // MyGUI视图单位 / 客户区像素

// 行值 diff 缓存 (key → 上次写入的最终文本): 定期刷新时数值未变的行 0 引擎调用
static std::map<std::string, std::string> g_lineVals;

// UI 缩放系数: 1080p=1.0, 4K=2.0 — 用于纯像素尺寸的控件 (控制卡/翻页钮)
static float UiScaleFactor() {
    float s = g_uiWinH / 1080.0f;
    if (s < 1.0f) s = 1.0f;
    if (s > 3.0f) s = 3.0f;
    return s;
}

static int PX(int base) { return (int)(base * UiScaleFactor() * g_viewScale + 0.5f); }

// 比例制定位 (2026-09-06 最终修复): 卡片此前用固定像素(按1920宽设计), 而 Position 是
// 归一化坐标 → 任何 ≠1920 宽的分辨率必然与主面板错位 (1280 挤压重叠 / 4K 过小)。
// 现统一为主面板同款比例制: 宽/横向 = 屏宽比例 (基准1920), 高/纵向 = 屏高比例 (基准1080)。
static int SXpx(int base) { return (int)(base * g_uiWinW / 1920.0f + 0.5f); }
static int SYpx(int base) { return (int)(base * g_uiWinH / 1080.0f + 0.5f); }

static void SetGraceLine(void* datapanel, int category, const std::string& key,
                         const std::string& value)
{
    if (datapanel == 0 && g_lineSink == 0) return;
    if (g_lineSink) {
        g_lineSink->push_back({key, value});
        return;
    }
    if (datapanel == 0 || setLineStatInfo_fn == 0) return;
    __try
    {
        // 深度加固 (2026-09-06): 行值 diff — 内容未变的行直接返回, 0 引擎调用。
        // 定期刷新的绝大多数行是静态专长门文案, 命中率极高。
        std::string cacheKey = std::to_string(category) + '\x1F' + key;
        std::string paddedVal = value + "   ";
        auto it = g_lineVals.find(cacheKey);
        if (it != g_lineVals.end() && it->second == paddedVal) return;
        DataPanelLine* line = setLineStatInfo_fn(datapanel, key, paddedVal, category);
        if (line) g_lineVals[cacheKey] = paddedVal;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// 重建分帧摊销 (2026-09-06): 换角色/翻页的全量重建 (clear + ~27 行 widget 创建)
// 从单帧尖峰改为每帧最多 8 行, 消除"点高专长角色瞬间卡死"
// ============================================================
static std::vector<UILine> g_pendingLines;
static size_t              g_pendingPos = 0;
static void FlushPendingLines()
{
    if (g_pendingPos >= g_pendingLines.size()) { g_pendingLines.clear(); g_pendingPos = 0; return; }
    int budget = 8;
    while (budget-- > 0 && g_pendingPos < g_pendingLines.size()) {
        UILine& l = g_pendingLines[g_pendingPos++];
        SetGraceLine(g_gracePanel, 0, l.key, l.text);
    }
    if (g_pendingPos >= g_pendingLines.size()) { g_pendingLines.clear(); g_pendingPos = 0; }
}
static void CancelPendingLines() { g_pendingLines.clear(); g_pendingPos = 0; }

// ============================================================
// ① 主面板：常规 27 项绝境专长收集
// ============================================================
static void CollectGraceLines(Character* ch, CharStats* stats, void* datapanel, int category)
{
    if (ch == 0 || stats == 0) return;   // datapanel 允许为 0 (行收集槽模式)

    bool isAnimalChar = false;
    if (ch) isAnimalChar = IsAnimal(ch);

    // 1. 七大废土联合专长
    if (!isAnimalChar)
    {
        float stealth = SafeStat(stats, STAT_STEALTH);
        float assassin = SafeStat(stats, STAT_ASSASSINATION);
        if (GateOK(25, ch))
            SetGraceLine(datapanel, category, PName(25),
                L_("无痕暗杀 · 仇恨抹除 · 零通缉", "Traceless assassinate · wipe resentment · zero bounty"));

        float stealing = SafeStat(stats, STAT_THIEVING);
        if (GateOK(24, ch))
            SetGraceLine(datapanel, category, PName(24),
                L_("潜行贴近选中目标按V探囊取物", "Sneak close, press V to pilfer target"));

        float labour = SafeStat(stats, STAT_LABOURING);
        float eng = SafeStat(stats, STAT_ENGINEERING);
        if (GateOK(27, ch))
            SetGraceLine(datapanel, category, PName(27),
                L_("负重+100kg · 背人无减速", "+100kg carry · no slowdown carrying allies"));

        float lockpick = SafeStat(stats, STAT_LOCKPICKING);
        float sci = SafeStat(stats, STAT_SCIENCE);
        if (GateOK(26, ch))
            SetGraceLine(datapanel, category, PName(26),
                L_("机械神偷 · 瞬间破锁", "Robotic thief · instant lockpick"));

        float farm = SafeStat(stats, STAT_FARMING);
        float cook = SafeStat(stats, STAT_COOKING);
        if (GateOK(22, ch))
            SetGraceLine(datapanel, category, PName(22),
                L_("随身主厨 · 自动喂食", "Portable cook · auto-feed"));

        float medic = SafeStat(stats, STAT_MEDIC);
        if (GateOK(201, ch))
            SetGraceLine(datapanel, category, PName(201),
                L_("义肢属性增益 +25%", "+25% prosthetic stat bonus"));

        float dex = SafeStat(stats, STAT_DEXTERITY);
        float dodge = SafeStat(stats, STAT_DODGE);
        if (GateOK(19, ch))
        {
            float lower = dex < dodge ? dex : dodge;
            float scale = (lower <= 100.0f) ? ((lower - 60.0f) / 40.0f) * ((lower - 60.0f) / 40.0f)
                                            : 1.0f + (lower - 100.0f) / 25.0f;
            int pct = (int)((0.30f + scale * 0.30f) * 100.0f);
            if (pct > 80) pct = 80;
            SetGraceLine(datapanel, category, PName(19),
                L_("极速闪避 · 后摇硬直 -", "Lightning dodge · -") + std::to_string(pct) + "%");
        }

        if (GateOK(202, ch))
            SetGraceLine(datapanel, category, PName(202),
                L_("饥饿-30% · 自愈加速", "Hunger -30% · faster self-heal"));
    }

    // 2. 战斗大师专长
    {
        float def = SafeStat(stats, STAT_MELEE_DEFENCE);
        if (def >= MASTERY_THRESHOLD)
        {
            float reduction = (def <= 100.0f) ? ((def - 60.0f) / 40.0f) * 3.0f
                                              : 3.0f + (def - 100.0f) * 0.12f;
            if (reduction >= 0.1f)
            {
                char buf[32];
                sprintf_s(buf, L_("要害减伤 %.1f 点", "%.1f vit reduction"), reduction);
                SetGraceLine(datapanel, category, PName(16), std::string(buf));
            }
        }
    }
    if (!isAnimalChar)
    {
        float dodge = SafeStat(stats, STAT_DODGE);
        if (dodge >= MASTERY_THRESHOLD)
        {
            float bonus = (dodge <= 100.0f) ? ((dodge - 60.0f) / 40.0f) * 0.30f
                                            : 0.30f + ((dodge - 100.0f) / 50.0f) * 0.30f;
            if (bonus > 0.01f)
            {
                SetGraceLine(datapanel, category, PName(205),
                    L_("闪避攻速 +", "Dodge speed +") + std::to_string((int)(bonus * 100.0f)) + "%");
            }
        }
    }

    // 3. 韧性与体质
    {
        float tough = SafeStat(stats, STAT_TOUGHNESS);
        if (tough >= 50.0f)
        {
            float reduction = ((tough - 50.0f) / 10.0f) * 0.12f;
            if (reduction > 0.85f) reduction = 0.85f;
            if (reduction >= 0.01f)
            {
                SetGraceLine(datapanel, category, PName(206),
                    L_("伤病惩罚 -", "Wound penalty -") + std::to_string((int)(reduction * 100.0f)) + "%");
                SetGraceLine(datapanel, category, PName(18),
                    L_("濒死全属性 +50%", "+50% all attributes near death"));
            }
        }
    }

    // 4. 医疗与机器人学
    {
        float robotics = SafeStat(stats, STAT_ROBOTICS);
        if (robotics >= 80.0f)
        {
            SetGraceLine(datapanel, category, PName(23),
                L_("机械生命自愈 + 磨损自动消解 (80级已解锁)", "Robotic self-heal + auto wear repair (lvl 80)"));
        }
        else if (robotics >= 40.0f)
        {
            SetGraceLine(datapanel, category, PName(23),
                L_("机械生命自愈 (80级解锁磨损修复)", "Robotic self-heal (wear repair at 80)"));
        }
        if (GateOK(203, ch))
        {
            float buff = ((robotics - 80.0f) / 20.0f) * 0.35f;
            if (buff > 0.35f) buff = 0.35f;
            SetGraceLine(datapanel, category, PName(203),
                L_("力敏增幅 +", "Str/Dex +") + std::to_string((int)(buff * 100.0f)) + L_("% (义肢过载)", "% (cyber overdrive)"));
        }

        float medic = SafeStat(stats, STAT_MEDIC);
        float doctor = SafeStat(stats, STAT_HIVEMEDIC);
        float effMedic = medic > doctor ? medic : doctor;
        if (effMedic >= 40.0f)
        {
            char hpBuf[32];
            sprintf_s(hpBuf, L_("周期调理 +%.2f HP", "Periodic recovery +%.2f HP"), (effMedic / 100.0f) * 0.04f);
            SetGraceLine(datapanel, category, PName(204), std::string(hpBuf));
        }
        if (effMedic >= 80.0f)
        {
            float bleedCut = (effMedic / 100.0f) * 0.6f;
            if (bleedCut > 0.80f) bleedCut = 0.80f;
            SetGraceLine(datapanel, category, PName(20),
                L_("流血速度 -", "Bleed rate -") + std::to_string((int)(bleedCut * 100.0f)) + "%" + (effMedic >= 100.0f ? L_(" · 自动造血", " · auto blood") : ""));
        }
    }

    // 5. 兵刃武器专精
    if (!isAnimalChar)
    {
        float sabres = SafeStat(stats, STAT_SABRES);
        if (GateOK(8, ch))
        {
            // 仅真实存在机制: 持军刀招架防御+X (无独立"反震手腕"逻辑, 故不列)
            SetGraceLine(datapanel, category, PName(8),
                L_("招架防御 +", "Parry def +") + std::to_string((int)(1.0f + GetMasteryScale(sabres) * 7.0f)));
        }

        float hackers = SafeStat(stats, STAT_HACKERS);
        if (GateOK(9, ch))
        {
            SetGraceLine(datapanel, category, PName(9),
                L_("破甲 +", "Armor pierce +") + std::to_string((int)((0.03f + GetMasteryScale(hackers) * 0.12f) * 100.0f)) + L_("% · 克制机械", "% · anti-robot"));
        }

        float blunt = SafeStat(stats, STAT_BLUNT);
        if (GateOK(10, ch))
        {
            SetGraceLine(datapanel, category, PName(10),
                L_("破甲+", "Pierce +") + std::to_string((int)((0.05f + GetMasteryScale(blunt) * 0.20f) * 100.0f))
                + L_("% 震晕+", "% stun +") + std::to_string((int)((0.03f + GetMasteryScale(blunt) * 0.09f) * 100.0f)) + "%");
        }

        float katana = SafeStat(stats, STAT_KATANAS);
        if (GateOK(7, ch))
        {
            SetGraceLine(datapanel, category, PName(7),
                L_("流血+", "Bleed +") + std::to_string((int)(1.0f + GetMasteryScale(katana) * 3.0f))
                + L_(" 切割加成×", " cut bonus ×") + std::to_string((int)((1.04f + GetMasteryScale(katana) * 0.12f) * 100.0f)) + "%");
        }

        float heavy = SafeStat(stats, STAT_HEAVYWEAPONS);
        if (GateOK(11, ch))
        {
            char buf[64];
            sprintf_s(buf, L_("硬直×%.1f 范围+%d%%", "Stun ×%.1f  Range +%d%%"), (1.1f + GetMasteryScale(heavy) * 0.4f), (int)((0.05f + GetMasteryScale(heavy) * 0.15f) * 100.0f));
            SetGraceLine(datapanel, category, PName(11), std::string(buf));
        }

        float pole = SafeStat(stats, STAT_POLEARMS);
        if (GateOK(12, ch))
        {
            // 真机制: 范围放大 + 命中使目标长短时间攻速下降(5s窗,不可叠,幅度随长柄技能)
            SetGraceLine(datapanel, category, PName(12),
                L_("范围+", "Range +") + std::to_string((int)((0.05f + GetMasteryScale(pole) * 0.15f) * 100.0f))
                + L_("% · 命中降敌攻速5s", "% · hit slows enemy 5s"));
        }

        float ma = SafeStat(stats, STAT_MARTIALARTS);
        if (GateOK(13, ch))
        {
            // 武术=纯攻速加成(不出连击/多段)。机制在 calculateAttackOrBlockSpeed 里仅
            // 徒手(SKILL_UNARMED)时注入, 故文案标(徒手)。
            SetGraceLine(datapanel, category, PName(13),
                L_("徒手攻速 +", "(Unarmed) atk speed +") + std::to_string((int)((0.08f + GetMasteryScale(ma) * 0.22f) * 100.0f)) + "%");
        }

        float turret = SafeStat(stats, STAT_TURRETS);
        if (GateOK(215, ch))
        {
            float mult = (turret <= 100.0f) ? 1.0f + GetMasteryScale(turret) * 1.5f
                                            : 2.5f + (turret - 100.0f) * 0.02f;
            char buf[32];
            sprintf_s(buf, L_("重型击退 ×%.1f", "Heavy knockback ×%.1f"), mult);
            SetGraceLine(datapanel, category, PName(215), std::string(buf));
        }
    }

    // 6. 感知
    if (!isAnimalChar)
    {
        float perception = SafeStat(stats, STAT_PERCEPTION);
        if (GateOK(17, ch))
        {
            float bonus = perception / 10.0f;
            if (bonus > 10.0f) bonus = 10.0f;
            if (bonus >= 1.0f)
                SetGraceLine(datapanel, category, PName(17),
                    L_("近战攻防 +", "Melee atk/def +") + std::to_string((int)bonus));
        }
    }

    // 7. 田径与游泳
    {
        float ath = SafeStat(stats, STAT_ATHLETICS);
        if (GateOK(210, ch))
        {
            float bonus = (ath <= 100.0f) ? GetMasteryScale(ath) * 0.15f
                                          : 0.15f + (ath - 100.0f) * 0.005f;
            if (bonus > 0.001f)
            {
                SetGraceLine(datapanel, category, PName(210),
                    L_("出招攻速 +", "Attack speed +") + std::to_string((int)(bonus * 100.0f)) + "%");
            }
        }
        float swim = SafeStat(stats, STAT_SWIMMING);
        if (GateOK(211, ch))
        {
            // 直线公式 (与机制一致): 60级起每级+0.2, 100级+8, 之后同斜率延续(无上限)
            float bonus = (swim - MASTERY_THRESHOLD) * 0.2f;
            if (bonus > 0.01f)
            {
                SetGraceLine(datapanel, category, PName(211),
                    L_("体魄增幅 · 全属性 +", "Physical boost · all attrs +") + std::to_string((int)bonus));
            }
        }
        if (!isAnimalChar)
        {
            float ff = SafeStat(stats, STAT_FRIENDLY_FIRE);
            if (GateOK(15, ch))
            {
                float perMate = 0.04f * GetMasteryScale(ff);
                if (perMate > 0.001f)
                {
                    SetGraceLine(datapanel, category, PName(15),
                        L_("每队友装填 -", "Reload per squadmate -") + std::to_string((int)(perMate * 100.0f)) + "%");
                }
            }
        }
    }

    // 8. 弩箭精通
    if (!isAnimalChar)
    {
        float xbow = SafeStat(stats, STAT_CROSSBOWS);
        if (GateOK(207, ch))
        {
            float mult = (xbow <= 100.0f) ? 1.0f + ((xbow - 40.0f) / 60.0f) * 2.0f
                                          : 3.0f + (xbow - 100.0f) * 0.08f;
            char buf[32];
            sprintf_s(buf, L_("远程伤害 ×%.1f", "Ranged damage ×%.1f"), mult);
            SetGraceLine(datapanel, category, PName(207), std::string(buf));
        }
    }

    // 9. 敏捷身法
    if (!isAnimalChar)
    {
        float dex = SafeStat(stats, STAT_DEXTERITY);
        if (GateOK(208, ch))
            SetGraceLine(datapanel, category, PName(208),
                L_("幻影连斩 · 神速多重格挡", "Phantom slash · rapid multi-block"));
        else if (GateOK(209, ch))
            SetGraceLine(datapanel, category, PName(209),
                L_("招架瞬间必定高速反击", "Instantly counter on parry"));
    }

    // 10. 力量破防
    {
        float str = SafeStat(stats, STAT_STRENGTH);
        if (GateOK(6, ch))
        {
            float shock = 30.0f + ((str - 120.0f) / 10.0f) * 8.0f;
            if (shock > 75.0f) shock = 75.0f;
            SetGraceLine(datapanel, category, PName(6),
                L_("震荡胸腔 ", "Concuss chest ") + std::to_string((int)shock) + L_(" 钝击", " blunt"));
        }
        if (!isAnimalChar)
        {
            float def = SafeStat(stats, STAT_MELEE_DEFENCE);
            if (GateOK(3, ch))
                SetGraceLine(datapanel, category, PName(3),
                    L_("霸体 · 免疫力量硬直", "Poise · immune to strength stun"));
        }
    }

    // 11. 锻造三艺
    {
        float wS = SafeStat(stats, STAT_SMITHING_WEAPON);
        float aS = SafeStat(stats, STAT_SMITHING_ARMOUR);
        float bS = SafeStat(stats, STAT_SMITHING_BOW);
        if (GateOK(213, ch))
        {
            int pct = (int)((wS - 60.0f) * 0.5f);
            SetGraceLine(datapanel, category, PName(213),
                L_("人剑合一 · 武器切/钝伤+", "Blade Unity · weapon cut/blunt +") + std::to_string(pct) + "%");
        }
        if (GateOK(214, ch))
        {
            int pct = (int)((aS - 60.0f) * 0.5f);
            SetGraceLine(datapanel, category, PName(214),
                L_("钢铁之躯 · 受击切/钝抗+", "Steel Body · cut/blunt resist +") + std::to_string(pct)
                + L_("% 鱼叉伤-", "% harpoon dmg -") + std::to_string((int)((aS - 60.0f) * 0.75f)));
        }
        if (GateOK(212, ch))
            SetGraceLine(datapanel, category, PName(212),
                L_("弩箭行囊定期自动补充", "Auto-refill quiver periodically"));
    }

    // 12. 宗师流法 (216): 显示当前流法与实际效果 (近战>=60 解锁)
    if (!isAnimalChar && ryuGet_fn)
    {
        void** guiVar = (void**)GetProcAddress(GetModuleHandleA("KenshiLib.dll"), "?gui@@3PEAVForgottenGUI@@EA");
        void* fgui = guiVar ? *guiVar : 0;
        void* sel = 0;
        if (fgui && getSelected_fn) {
            __try { sel = getSelected_fn(fgui); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        void* chRyu = (sel && handGetChar_fn) ? handGetChar_fn(sel) : ch;   // 选中优先, 回落展示对象
        if (chRyu) {
            int st = ryuGet_fn(chRyu);
            float lv = 0.0f;
            __try { lv = SafeStat(stats, STAT_MELEE_ATTACK); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            if (st >= 0 && lv >= 60.0f) {
                // 按等级动态显示具体百分比 (60级半效 -> 100级满值 -> >100外推)
                float t = (lv <= 100.0f) ? (lv - 60.0f) / 40.0f : 1.0f + (lv - 100.0f) / 60.0f;
                char buf[96];
                if (st == 0) {
                    float spd = 20.0f * t, dmg = -20.0f * t; if (dmg < -90.0f) dmg = -90.0f;
                    sprintf_s(buf, L_("疾风流 · 攻速+%.0f%% 伤害%.0f%%", "Gale · spd +%.0f%% dmg %.0f%%"), spd, dmg);
                } else if (st == 1) {
                    sprintf_s(buf, L_("中庸流 · 攻速+%.0f%% 伤害+%.0f%%", "Balance · spd +%.0f%% dmg +%.0f%%"),
                              10.0f * t, 20.0f * t);
                } else {
                    float spd = -20.0f * t; if (spd < -45.0f) spd = -45.0f;
                    sprintf_s(buf, L_("重击流 · 攻速%.0f%% 伤害+%.0f%% 穿甲+%.0f%%", "Crush · spd %.0f%% dmg +%.0f%% pierce +%.0f%%"),
                              spd, 40.0f * t, 20.0f * t);
                }
                SetGraceLine(datapanel, category, PName(216), std::string(buf));
            } else if (lv >= 60.0f) {
                SetGraceLine(datapanel, category, PName(216),
                    L_("未入流 · 控制条点流法卡选择", "No style · click Ryu card to choose"));
            }
        }
    }
}

// ============================================================
// ② 副面板：专属四大极境天道收集
// ============================================================
static void CollectAscensionLines(Character* ch, CharStats* stats, void* datapanel, int category, bool isAscActive)
{
    if (ch == 0 || stats == 0) return;   // datapanel 允许为 0 (行收集槽模式)
    bool isAnimalChar = false;
    if (ch) isAnimalChar = IsAnimal(ch);
    if (isAnimalChar) return;

    // 状态行 (独立一行, 避免与下方提示行叠): 极境系统 开/关
    SetGraceLine(datapanel, category,
        L_("天道极境", "Transcendence"),
        isAscActive ? L_("已开启", "Enabled") : L_("已关闭", "Disabled"));

    if (!isAscActive)
    {
        // 两行短句 (避免长英文描述超宽叠字)
        SetGraceLine(datapanel, category, L_("极境休眠", "Dormant"),
            L_("极境已关闭", "Transcendence off"));
        SetGraceLine(datapanel, category, L_("点击按钮重新激活", "Click button to enable"), "");
        return;
    }

    int activeCount = 0;

    // ① 肉身成圣
    if (SafeStat(stats, STAT_STRENGTH) >= 90.0f &&
        SafeStat(stats, STAT_TOUGHNESS) >= 90.0f &&
        SafeStat(stats, STAT_DEXTERITY) >= 90.0f &&
        SafeStat(stats, STAT_ATHLETICS) >= 90.0f &&
        SafeStat(stats, STAT_SWIMMING) >= 90.0f &&
        SafeStat(stats, STAT_LABOURING) >= 90.0f)
    {
        SetGraceLine(datapanel, category, L_("肉身成圣", "Sanctified Body"),
            L_("四肢保底 · 负血免昏迷", "Limb floor · no death blow"));
        SetGraceLine(datapanel, category, L_("· 守", "- Guard"),
            L_("血液锁50%", "Blood lock 50%"));
        SetGraceLine(datapanel, category, L_("· 战", "- War"),
            L_("全伤减免10%", "-10% all dmg"));
        SetGraceLine(datapanel, category, L_("· 生", "- Life"),
            L_("血肉肢体周期再生", "Flesh limbs regenerate"));
        activeCount++;
    }

    // ② 万法皆通
    int wepCount = 0;
    if (SafeStat(stats, STAT_KATANAS) >= 90.0f) wepCount++;
    if (SafeStat(stats, STAT_SABRES) >= 90.0f) wepCount++;
    if (SafeStat(stats, STAT_HACKERS) >= 90.0f) wepCount++;
    if (SafeStat(stats, STAT_BLUNT) >= 90.0f) wepCount++;
    if (SafeStat(stats, STAT_HEAVYWEAPONS) >= 90.0f) wepCount++;
    if (SafeStat(stats, STAT_POLEARMS) >= 90.0f) wepCount++;
    if (SafeStat(stats, STAT_CROSSBOWS) >= 90.0f) wepCount++;
    if (SafeStat(stats, STAT_TURRETS) >= 90.0f) wepCount++;

    if (SafeStat(stats, STAT_MELEE_ATTACK) >= 90.0f &&
        SafeStat(stats, STAT_MELEE_DEFENCE) >= 90.0f &&
        SafeStat(stats, STAT_MARTIALARTS) >= 90.0f &&
        SafeStat(stats, STAT_DODGE) >= 90.0f && wepCount >= 3)
    {
        SetGraceLine(datapanel, category, L_("万法皆通 · 武", "All-Encompassing · Martial"),
            L_("攻速/招架 +50% · 判定距离 +50%", "+50% atk/parry speed · reach +50%"));
        SetGraceLine(datapanel, category, L_("万法皆通 · 道", "All-Encompassing · Dao"),
            L_("武器伤害+50% · 90%绝对格挡 · 追加10%真伤", "dmg +50% · 90% block · +10% true dmg"));
        activeCount++;
    }

    // ③ 幽冥主宰
    if (SafeStat(stats, STAT_STEALTH) >= 90.0f &&
        SafeStat(stats, STAT_ASSASSINATION) >= 90.0f &&
        SafeStat(stats, STAT_THIEVING) >= 90.0f &&
        SafeStat(stats, STAT_LOCKPICKING) >= 90.0f &&
        SafeStat(stats, STAT_PERCEPTION) >= 90.0f)
    {
        SetGraceLine(datapanel, category, L_("幽冥主宰 · 杀", "Nether Lord · Kill"),
            L_("暗杀恒100% · 被盯也能发起", "Assassinate always 100% · usable even watched"));
        SetGraceLine(datapanel, category, L_("幽冥主宰 · 匿", "Nether Lord · Hide"),
            L_("潜行恒蓝眼 · 无人能看见你", "Perma blue eye · nobody spots you"));
        activeCount++;
    }

    // ④ 机械至尊
    if (SafeStat(stats, STAT_SCIENCE) >= 90.0f &&
        SafeStat(stats, STAT_ROBOTICS) >= 90.0f &&
        SafeStat(stats, STAT_SMITHING_WEAPON) >= 90.0f &&
        SafeStat(stats, STAT_SMITHING_ARMOUR) >= 90.0f &&
        SafeStat(stats, STAT_SMITHING_BOW) >= 90.0f)
    {
        SetGraceLine(datapanel, category, L_("机械至尊 · 躯", "Mech Supreme · Body"),
            L_("智械零磨损极速自愈再生", "Robotic zero-wear rapid self-heal"));
        SetGraceLine(datapanel, category, L_("机械至尊 · 统", "Mech Supreme · Command"),
            L_("命中致瘫骨人 · 背负支配入队", "Hit paralyzes skeletons · dominate carried into squad"));
        activeCount++;
    }

    // ⑤ 荒原主宰 (第五天道, 2026-09-06)
    if (SafeStat(stats, STAT_MEDIC) >= 90.0f &&
        SafeStat(stats, STAT_PERCEPTION) >= 90.0f &&
        SafeStat(stats, STAT_COOKING) >= 90.0f &&
        SafeStat(stats, STAT_FARMING) >= 90.0f &&
        SafeStat(stats, STAT_SCIENCE) >= 90.0f &&
        SafeStat(stats, STAT_STRENGTH) >= 90.0f)
    {
        SetGraceLine(datapanel, category, L_("荒原主宰 · 支", "Wild Sovereign · Domain"),
            L_("背负血肉动物即入队 · 野兽不与你有仇", "Carry beast to recruit · beasts stay peaceful"));
        SetGraceLine(datapanel, category, L_("荒原主宰 · 群", "Wild Sovereign · Pack"),
            L_("队内兽: 饥饿冻结 流血-50% 回复加速", "Pack: hunger frozen bleed -50% fast heal"));
        SetGraceLine(datapanel, category, L_("荒原主宰 · 强", "Wild Sovereign · Might"),
            L_("队内兽: 全属性+20% 移速+20%", "Pack: +20% stats +20% move speed"));
        SetGraceLine(datapanel, category, L_("荒原主宰 · 庇", "Wild Sovereign · Boon"),
            L_("每只动物 +1攻/+1防 (上限15)", "+1 atk/def per animal (cap 15)"));
        if (packCount_fn) {
            int n = packCount_fn();
            SetGraceLine(datapanel, category, L_("荒原主宰 · 群态", "Wild Sovereign · Pack Status"),
                L_("队内动物 ", "Pack animals: ") + std::to_string(n) +
                L_(" 只 · 强化生效中", " · buffed"));
        }
        activeCount++;
    }

    if (activeCount == 0)
    {
        // 两行显示 (value 留空, 避免左右双栏英文超宽叠字)
        SetGraceLine(datapanel, category, L_("极境未成", "Transcendence Not Achieved"), "");
        SetGraceLine(datapanel, category, L_("属性需达到 90 级方可解锁", "Requires attributes at 90 to unlock"), "");
    }
}

// ============================================================
// 绘制双面板完整内容
// ============================================================
// 页名 (翻页钮 caption 用)
static const char* PageName(int p)
{
    switch (p) {
        case 0:  return L_("常规专长·上", "Perks 1/2");
        case 1:  return L_("常规专长·下", "Perks 2/2");
        default: return L_("四大极境天道", "Ascension");
    }
}

// ============================================================
// 渲染单面板当前页 (三页翻页制)
// 页0/页1 = 常规 27 专长按收集顺序对半; 页2 = 极境天道全量
// ============================================================
static void RenderCharacterPanels(Character* ch, bool isAscActive)
{
    if (ch == 0) return;
    CharStats* stats = charGetStats_fn ? charGetStats_fn(ch) : 0;
    if (stats == 0) return;
    if (g_gracePanel == 0 || clear_fn == 0) return;

    // 1. 收集当前页行 (sink 模式: 收集器原样复用, 不写面板)
    // 翻页内容错乱修复 (2026-09-06): ①页1 必须收常规专长 (旧代码走 else 收了极境,
    // 4页时代即错位, 3页化后页1与页0观感错乱); ②hi/lo 在收集后按全量行数计算
    // (剥离重排时丢了 hi 重置 → 页2/页1 区间恒空 → 极境页空白)。
    std::vector<UILine> lines;
    g_lineSink = &lines;
    if (g_currentPage <= 1) CollectGraceLines(ch, stats, 0, 0);
    else                    CollectAscensionLines(ch, stats, 0, 0, isAscActive);
    g_lineSink = 0;
    size_t lo = 0, hi = lines.size();
    if (g_currentPage == 0) hi = (lines.size() + 1) / 2;
    else if (g_currentPage == 1) lo = (lines.size() + 1) / 2;

    // 3. 写入当前页 (两轮卡顿修复后终版)
    // ① 行 key 集合未变 → 原地 diff 更新 (行值缓存挡住未变行, 零引擎调用);
    // ② 集合变化 (换页/门集合变化/面板重建) → clear 重建, 行创建分帧摊销 (每帧≤8行);
    // ③ 字号机制已彻底移除 (2026-09-06 用户定论: 游戏 UI 不支持改字号, 一切
    //    changeFontSize 全局/逐行调用都是纯成本无收益, 行字号用引擎创建默认值)。
    std::string keysSig;
    {
        keysSig.reserve((hi - lo) * 24 + 8);
        keysSig += std::to_string(g_currentPage);
        keysSig += '|';
        for (size_t i = lo; i < hi && i < lines.size(); ++i) { keysSig += lines[i].key; keysSig += '\x1F'; }
    }
    bool fullRebuild = (keysSig != g_lastPageKeysSig || g_lastPageKeysPanel != (void*)g_gracePanel);
    if (fullRebuild) {
        __try {
            clear_fn(g_gracePanel);
            if (updatePanel_fn) updatePanel_fn(g_gracePanel);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        g_lineVals.clear();             // 全新面板行: 缓存全失效, 重建期间逐行重写
        CancelPendingLines();
        g_pendingLines.assign(lines.begin() + lo, lines.begin() + (hi < lines.size() ? hi : lines.size()));
        g_pendingPos = 0;
        g_lastPageKeysSig = keysSig;
        g_lastPageKeysPanel = (void*)g_gracePanel;
        FlushPendingLines();            // 首批 8 行立即上屏, 其余后续帧摊销
    } else {
        for (size_t i = lo; i < hi && i < lines.size(); ++i) {
            SetGraceLine(g_gracePanel, 0, lines[i].key, lines[i].text);
        }
    }
}

// ============================================================
// 创建主副双面板
// ============================================================
static void EnsurePanelsCreated()
{
    if (g_created || createDatapanel_fn == 0) return;
    // P3 加固: 创建失败退避 - 引擎半失败创建(分配后返回0)时防每帧重试风暴/泄漏
    static int s_createFailStreak = 0;
    static DWORD s_lastCreateAttempt = 0;
    DWORD nowT = GetTickCount();
    if (s_createFailStreak >= 3 && (nowT - s_lastCreateAttempt) < 5000) return;
    s_lastCreateAttempt = nowT;
    __try
    {
        HMODULE h = GetModuleHandleA("KenshiLib.dll");
        void* pGuiVar = (void*)GetProcAddress(h, "?gui@@3PEAVForgottenGUI@@EA");
        if (pGuiVar == 0) { s_createFailStreak++; return; }
        void* fgui = *(void**)pGuiVar;
        if (fgui == 0) { s_createFailStreak++; return; }

        UiReadWindowSize();
        int mainW = UiScaleX(PANEL_FW);
        int mainH = UiScaleY(PANEL_FH);
        float mainTop = PANEL_POSY;
        // 控制条放主UI左侧: 顶部与主UI上边框对齐 (竖排向下)
        g_ctrlTop = mainTop;

        // 2026-09-06: 创建可见性尊重 F8 用户隐藏意图 (此前硬编码 true, 重建即复活 UI)
        bool createVis = !g_uiHiddenByUser;

        // 单面板 (三页翻页制, 副面板已并入): 顶部 PANEL_POSY, 高 PANEL_FH (薄边框)
        void* mainPanel = createDatapanel_fn(fgui, PANEL_POSX, mainTop, (float)mainW / g_uiWinW, (float)mainH / g_uiWinH, createVis,
                                             std::string("Middle"),
                                             std::string("Kenshi_FloatingPanelThinSkin"));
        if (mainPanel == 0) { s_createFailStreak++; return; }

        // 体检修正 (2026-09-06): 创建成功立即登记引用 — 后续定位/resize 抛异常时
        // 面板已受管理 (旧代码异常时泄漏面板且退避重试会叠加多块)
        g_gracePanel = mainPanel;
        g_created = true;
        g_needCreate = false;
        g_lastPageKeysSig.clear();      // 新面板 → 下次渲染强制全量重建
        g_lastPageKeysPanel = 0;
        g_lineVals.clear();
        CancelPendingLines();

        if (setPositionReal_fn)
        {
            setPositionReal_fn(mainPanel, PANEL_POSX, mainTop);
        }

        if (resize_fn)
        {
            resize_fn(mainPanel, mainW, mainH);
            // 视图坐标系标定 (2026-09-06): 读回真实尺寸对比传入值 — 数据先行, 再定修正
            if (getWidth_fn && getHeight_fn) {
                int aw = getWidth_fn(mainPanel);
                int ah = getHeight_fn(mainPanel);
                char cb[192];
                sprintf_s(cb, "Calibration: resize(%d,%d) -> actual(%d,%d) client=%.0fx%.0f ratio=%.4f/%.4f",
                    mainW, mainH, aw, ah, g_uiWinW, g_uiWinH,
                    mainW ? (float)aw / mainW : 0.0f, mainH ? (float)ah / mainH : 0.0f);
                ErrorLog(cb);
            }
        }

        g_panelVisible = !g_uiHiddenByUser;   // 重建尊重 F8 隐藏意图 (勿无条件复位 true)
        s_createFailStreak = 0;
        {
            char b[128];
            float mainBottom = mainTop + mainH / g_uiWinH;
            sprintf_s(b, "Layout: single panel top=%.4f bot=%.4f(px=%d) ctrlTop=%.4f",
                mainTop, mainBottom, (int)(mainBottom*g_uiWinH), g_ctrlTop);
            ErrorLog(b);
        }

        int currentScope = GetCurrentScopeMode();
        bool ascActive = GetAscensionEnabled();
        g_lastRenderedScope = currentScope;
        g_lastAscState = ascActive;

        UpdatePanelTitles(currentScope, ascActive);
        ErrorLog("DivineGraceUI: Dual-panel system (Main + Ascension) created successfully!");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        s_createFailStreak++;
    }
}

// ============================================================
// 控制条 (可点击): 生效模式 / 攻速上限 / 极境天道 切换
// 复用一键断肢的多子面板热区方案: 每卡独立矩形命中 + setPositionReal 定位
// + resize 像素定尺寸; 皮肤用 Thin 薄边框与主面板区隔
// ============================================================

// --- 攻速档位表 (与控制条点击循环顺序一致) ---
static const float g_speedTiers[5] = { 1.2f, 2.4f, 3.6f, 4.8f, 9.6f };

static const char* ScopeNameShort(int scope)
{
    switch (scope) { case 1: return L_("仅玩家", "Player"); case 2: return L_("仅NPC", "NPC"); default: return L_("全局", "Global"); }
}

static void CtrlDropRefs()
{
    for (int i = 0; i < CTRL_COUNT; ++i) g_ctrl[i].handle = 0;
    g_pageBtn.handle = 0;
    g_ctrlCreated = false;
}

static void CtrlShowAll(bool on)
{
    if (!show_fn) return;
    for (int i = 0; i < CTRL_COUNT; ++i) {
        if (g_ctrl[i].handle) {
            __try { show_fn(g_ctrl[i].handle, on); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (g_pageBtn.handle) {
        __try { show_fn(g_pageBtn.handle, on); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

// ============================================================
// 面板引用统一自愈丢弃 (2026-09-06 收敛: 原先三处复制 - fguiUpdate 异常 /
// DEGRADE drop / getGUIData 异常, 改一处漏两处的隐患源)
// 语义: 丢弃全部面板引用 (不手动 destroy 防双释, 旧面板由引擎清理),
// 待 GUI 换代后按 g_uiHiddenByUser 意图重建。
// ============================================================
static void DropAllPanelRefs(const char* reason)
{
    g_created = false;
    g_needCreate = false;
    g_gracePanel = 0;
    CtrlDropRefs();
    g_displayedChar = 0;
    g_noCharFrames = 0;
    g_lastPageKeysSig.clear();          // 面板身份失效 → 下次渲染强制全量重建
    g_lastPageKeysPanel = 0;
    g_lineVals.clear();
    CancelPendingLines();
    if (reason && *reason) ErrorLog(std::string("DivineGraceUI: panels dropped (") + reason + ")");
}

static void EnsureCtrlCreated(void* fgui)
{
    if (g_ctrlCreated || createDatapanel_fn == 0) return;
    __try {
        UiReadWindowSize();
        // 2026-09-06 比例制: 卡片宽随屏宽、高随屏高 (与大面板同款, 任意分辨率不错位)
        int wpx = SXpx(CTRL_WPX);
        int hpx = SYpx(CTRL_HPX);
        std::string skin = std::string("Kenshi_FloatingPanelThinSkin");
        float wN = (float)wpx / g_uiWinW;
        float hN = (float)hpx / g_uiWinH;
        float gapN = (float)CTRL_GAPPX / g_uiWinH;
        float y = g_ctrlTop;
        for (int i = 0; i < CTRL_COUNT; ++i) {
            g_ctrl[i].x = CTRL_POSX;
            g_ctrl[i].y = y;
            g_ctrl[i].w = wN;
            g_ctrl[i].h = hN;
            g_ctrl[i].type = i;
            void* card = createDatapanel_fn(fgui, CTRL_POSX, y, wN, hN, g_panelVisible,
                                            std::string("Middle"), skin);
            if (!card) { ErrorLog("CtrlPanel: createDatapanel fail at " + std::to_string(i)); continue; }
            g_ctrl[i].handle = card;
            if (setPositionReal_fn) {
                __try { setPositionReal_fn(card, CTRL_POSX, y); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            if (resize_fn) {
                __try { resize_fn(card, wpx, hpx); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            y += hN + gapN;
        }

        // 翻页钮 (单面板顶部横排): 复用控制卡小面板方案, 点循环 4 页
        g_pageBtn.x = PANEL_POSX;
        g_pageBtn.y = PAGEBTN_Y;
        g_pageBtn.w = (float)SXpx(PAGEBTN_WPX) / g_uiWinW;
        g_pageBtn.h = (float)SYpx(PAGEBTN_HPX) / g_uiWinH;
        g_pageBtn.type = 100;
        void* pb = createDatapanel_fn(fgui, g_pageBtn.x, g_pageBtn.y, g_pageBtn.w, g_pageBtn.h, g_panelVisible,
                                      std::string("Middle"), skin);
        if (!pb) {
            ErrorLog("CtrlPanel: page button create fail");
        } else {
            g_pageBtn.handle = pb;
            if (setPositionReal_fn) {
                __try { setPositionReal_fn(pb, g_pageBtn.x, g_pageBtn.y); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            if (resize_fn) {
                __try { resize_fn(pb, SXpx(PAGEBTN_WPX), SYpx(PAGEBTN_HPX)); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }

        g_ctrlCreated = true;
        // 中文输入卡片改为懒创建 (2026-09-06): 翻到页4才创建, 见主循环 0b
        ErrorLog("CtrlPanel: 3 control cards + page button created (fixed px, small).");
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static int g_ryuRefreshCounter = 0;
static bool ryuRefreshTick() { return (++g_ryuRefreshCounter % 5) == 0; }
static void RyuForceRefresh() { g_ryuRefreshCounter = 4; }   // 下一 tick 即触发

static void RenderCtrl()
{
    if (!g_ctrlCreated || setCaption_fn == 0) return;
    // 性能 (2026-09-06): 每 tick 只在数值变化时重写 caption (引擎 setCaption 有文本布局成本)
    static int s_lastSc = -1; static float s_lastCap = -1; static bool s_lastOn = false;
    static int s_lastPage = -1;
    int scNow = GetCurrentScopeMode(); float capNow = GetCurrentAttackSpeedCap(); bool onNow = GetAscensionEnabled();
    bool ryuTick = ryuRefreshTick();   // 流法卡每 5 帧刷 (setCaption 引擎侧有文本对比, 非全量重排)
    bool changed = (scNow != s_lastSc || capNow != s_lastCap || onNow != s_lastOn ||
                    g_currentPage != s_lastPage || ryuTick);
    if (!changed) return;   // 卡顿修复 (2026-09-06): 门闩真正生效 — 此前每帧仍写3次setCaption
    s_lastSc = scNow; s_lastCap = capNow; s_lastOn = onNow; s_lastPage = g_currentPage;
    __try {
        int sc = scNow;
        float cap = capNow;
        bool on = onNow;
        char buf[16]; sprintf_s(buf, "%.1f", cap);
        if (g_ctrl[CTRL_SCOPE].handle) {
            setCaption_fn(g_ctrl[CTRL_SCOPE].handle,
                std::string(L_("生效模式", "Mode")) + ": " + ScopeNameShort(sc));
        }
        if (g_ctrl[CTRL_SPEED].handle) {
            setCaption_fn(g_ctrl[CTRL_SPEED].handle,
                std::string(L_("攻速上限", "Atk Speed Cap")) + ": " + buf + "x");
        }
        if (g_ctrl[CTRL_ASC].handle) {
            setCaption_fn(g_ctrl[CTRL_ASC].handle,
                std::string(L_("极境天道", "Transcendence")) + ": " + (on ? L_("开", "On") : L_("关", "Off")));
        }
        if (g_ctrl[CTRL_RYU].handle) {
            std::string ryuTxt = L_("流法: -", "Ryu: -");
            void** guiVar = (void**)GetProcAddress(GetModuleHandleA("KenshiLib.dll"), "?gui@@3PEAVForgottenGUI@@EA");
            void* fgui = guiVar ? *guiVar : 0;
            if (fgui && getSelected_fn && handGetChar_fn && ryuGet_fn) {
                void* sel = 0;
                __try { sel = getSelected_fn(fgui); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                void* ch = sel ? handGetChar_fn(sel) : 0;
                if (ch) {
                    int st = ryuGet_fn(ch);
                    if (st == 0) ryuTxt = L_("流法: 疾风流", "Ryu: Gale");
                    else if (st == 1) ryuTxt = L_("流法: 中庸流", "Ryu: Balance");
                    else if (st == 2) ryuTxt = L_("流法: 重击流", "Ryu: Crush");
                }
            }
            setCaption_fn(g_ctrl[CTRL_RYU].handle, ryuTxt);
        }
        if (g_pageBtn.handle) {
            char pb[8]; sprintf_s(pb, "%d/3", g_currentPage + 1);
            setCaption_fn(g_pageBtn.handle,
                std::string(L_("翻页► ", "Page ► ")) + PageName(g_currentPage) + " [" + pb + "]");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// 点击某张控制卡 -> 执行切换
static void CtrlExecute(int type)
{
    __try {
        if (type == CTRL_SCOPE) {
            int cur = GetCurrentScopeMode();
            if (setScopeMode_fn) { setScopeMode_fn((cur + 1) % 3); ErrorLog("CtrlPanel: scope -> " + std::to_string((cur+1)%3)); }
        } else if (type == CTRL_SPEED) {
            float cur = GetCurrentAttackSpeedCap();
            int idx = 0; for (int i = 0; i < 5; ++i) if (g_speedTiers[i] >= cur - 0.01f) { idx = i; break; }
            int next = (idx + 1) % 5;
            if (setAttackSpeedCap_fn) { setAttackSpeedCap_fn(g_speedTiers[next]); ErrorLog("CtrlPanel: speed -> " + std::to_string(g_speedTiers[next])); }
        } else if (type == CTRL_ASC) {
            bool cur = GetAscensionEnabled();
            if (setAscension_fn) { setAscension_fn(!cur); ErrorLog("CtrlPanel: asc -> " + std::to_string(!cur)); }
        } else if (type == CTRL_RYU) {
            void** guiVar = (void**)GetProcAddress(GetModuleHandleA("KenshiLib.dll"), "?gui@@3PEAVForgottenGUI@@EA");
            void* fgui = guiVar ? *guiVar : 0;
            if (fgui && getSelected_fn && handGetChar_fn && ryuSet_fn && ryuGet_fn) {
                void* sel = 0;
                __try { sel = getSelected_fn(fgui); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                void* ch = sel ? handGetChar_fn(sel) : 0;
                if (ch) {
                    int cur = ryuGet_fn(ch);
                    int next = (cur + 1) % 3;   // -1(未设置)->0 疾风
                    ryuSet_fn(ch, next);
                    const char* nm = (next == 0) ? "疾风流" : (next == 1 ? "中庸流" : "重击流");
                    ErrorLog(std::string("CtrlPanel: 流法 -> ") + nm);
                    RyuForceRefresh();   // 流法卡 caption 下一 tick 刷新
                    g_refreshTimer = 149; // 主面板下帧全量重绘 (宗师流法行秒级同步)
                }
            }
        } else if (type == 100) {
            // 翻页钮: 循环 常规上 -> 常规下 -> 极境 -> 中文输入
            g_currentPage = (g_currentPage + 1) % 3;
            ErrorLog(std::string("DivineGraceUI: page -> ") + PageName(g_currentPage));
            // 体检修正 (2026-09-06): 不直接用跨帧缓存的 g_displayedChar 渲染 (死亡/卸载后
            // 悬垂, charGetStats 虚调用 AV); 置强刷标记, 下一帧主循环用当帧新取的 ch 渲染
            g_refreshTimer = 150;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// 命中: 遍历 3 张控制卡矩形
static int CtrlHitTest()
{
    HWND fg = GetForegroundWindow();
    if (!fg) return -1;
    POINT pt;
    if (!GetCursorPos(&pt)) return -1;
    RECT rc;
    if (!GetClientRect(fg, &rc) || rc.right <= 0 || rc.bottom <= 0) return -1;
    ScreenToClient(fg, &pt);
    float nx = (float)pt.x / (float)rc.right;
    float ny = (float)pt.y / (float)rc.bottom;
    for (int i = 0; i < CTRL_COUNT; ++i) {
        CtrlCard& c = g_ctrl[i];
        if (!c.handle) continue;
        if (nx >= c.x && nx <= c.x + c.w && ny >= c.y && ny <= c.y + c.h) {
            // 关闭钮(×)区域: 引擎在每张卡片内部右侧约1/4区域绘× (36px高整带)
            // 点× = 引擎关面板, 不应触发切换
            if (nx >= c.x + c.w * 0.75f) return -1;
            return c.type;
        }
    }
    // 翻页钮 (type=100)
    CtrlCard& pb = g_pageBtn;
    if (pb.handle && nx >= pb.x && nx <= pb.x + pb.w && ny >= pb.y && ny <= pb.y + pb.h) {
        if (nx >= pb.x + pb.w * 0.75f) return -1;   // 右侧×带不响应
        return 100;
    }
    return -1;
}

// 控制条点击轮询 (左键上升沿; 隐藏时忽略; Shift=描点不动作)
static void PollCtrlClick()
{
    bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    // 体检修正 (2026-09-06): 补窗口焦点过滤 — 前台非游戏窗口时坐标换算基于错误矩形,
    // 可能误触发模式/攻速切换 (F10/热键早有此过滤, 标准对齐)
    if (!IsGameWindowFocused()) { g_mousePrevDown = down; return; }
    bool clicked = down && !g_mousePrevDown;
    g_mousePrevDown = down;
    if (!clicked || !g_panelVisible) return;   // 隐藏时点击不响应
    DWORD now = GetTickCount();
    if (now - g_lastClickTick < 120) return;
    g_lastClickTick = now;
    bool trace = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    int type = CtrlHitTest();
    if (type < 0) return;
    if (trace) return;   // 描点模式: 只命中不动
    ErrorLog("CtrlPanel: CLICK type=" + std::to_string(type) + " created=" + std::to_string(g_created) + " vis=" + std::to_string(g_panelVisible));
    CtrlExecute(type);
    RenderCtrl();        // 立即刷新显示
}

// ============================================================
// ForgottenGUI::update hook：按键监听 + 同步刷新
// ============================================================
static void fguiUpdate_hook(void* fgui)
{
    // P3 修复: 引擎原版 update 包裹 SEH。若引擎在 GUI 重建后仍遍历已释放的
    // Datapanel (本插件历史根因), 此处兜底, 避免异常逃逸到引擎导致进程闪退。
    __try
    {
        if (fguiUpdate_orig)
            fguiUpdate_orig(fgui);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // 引擎 GUI 更新异常: 极可能是 GUI 换代/面板悬垂, 立即丢弃全部引用自救
        DropAllPanelRefs("engine GUI update raised exception");
    }

    if (g_needCreate && !g_created)
        EnsurePanelsCreated();

    if (!g_created || g_gracePanel == 0) return;

    __try
    {
        // 1. 【F8 快捷键监听：主副双面板同步显隐】（仅前台游戏窗口响应，松开时且未配合数字键才切换显隐）
        bool f8Down = IsGameWindowFocused() && ((GetAsyncKeyState(VK_F8) & 0x8000) != 0);
        static bool s_f8ComboUsed = false;
        if (f8Down) {
            bool k1 = ((GetAsyncKeyState('1') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0);
            bool k2 = ((GetAsyncKeyState('2') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD2) & 0x8000) != 0);
            bool k3 = ((GetAsyncKeyState('3') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD3) & 0x8000) != 0);
            bool k4 = ((GetAsyncKeyState('4') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD4) & 0x8000) != 0);
            bool k5 = ((GetAsyncKeyState('5') & 0x8000) != 0) || ((GetAsyncKeyState(VK_NUMPAD5) & 0x8000) != 0);
            if (k1 || k2 || k3 || k4 || k5) {
                s_f8ComboUsed = true;
            }
        }
        if (!f8Down && g_f8LastDown) {
            if (!s_f8ComboUsed) {
                // 2026-09-06: F8 隐藏 = 持久用户意图, 点击角色/场景切换/面板重建均不得复活 UI
                g_uiHiddenByUser = !g_uiHiddenByUser;
                g_panelVisible = !g_uiHiddenByUser;
                if (show_fn) {
                    show_fn(g_gracePanel, g_panelVisible);
                    CtrlShowAll(g_panelVisible);
                }
                ErrorLog(std::string("DivineGraceUI: F8 toggle -> ") + (g_panelVisible ? "SHOW" : "HIDE (sticky; only F8 re-shows)"));
            }
            s_f8ComboUsed = false;
        }
        g_f8LastDown = f8Down;

        // 0a. 分辨率变化监视 (2026-09-06 布局挤压根因修复): 中途改分辨率 → 旧面板的
        // 归一化坐标/像素尺寸全部失效, 必须丢弃重建 (实测: 降分辨率后卡片挤压变形)
        {
            RECT rcNow = { 0 };
            HWND gh = GetGameWindowHwnd();
            if (gh && GetClientRect(gh, &rcNow) && rcNow.right > 0 && g_created) {
                if (abs(rcNow.right - (int)g_uiWinW) > 2 || abs(rcNow.bottom - (int)g_uiWinH) > 2) {
                    ErrorLog("DivineGraceUI: resolution changed " + std::to_string((int)g_uiWinW) + "x" +
                             std::to_string((int)g_uiWinH) + " -> " + std::to_string(rcNow.right) + "x" +
                             std::to_string(rcNow.bottom) + ", panels dropped for rebuild");
                    g_uiWinW = (float)rcNow.right;
                    g_uiWinH = (float)rcNow.bottom;
                    // 体检 P2 修正: 与 DEGRADE 路径对齐, 丢弃前先隐藏 (防幽灵卡片窗口期)
                    if (show_fn && g_gracePanel) {
                        __try { show_fn(g_gracePanel, false); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                    CtrlShowAll(false);
                    DropAllPanelRefs(0);
                    return;   // 下一帧按新分辨率重建
                }
            }
        }



        // 0. 每帧锁位: 主面板/控制条/翻页钮钉死坐标 (防拖动 -> 无缝连体不漂移)
        if (setPositionReal_fn) {
            if (g_gracePanel) {
                __try { setPositionReal_fn(g_gracePanel, PANEL_POSX, PANEL_POSY); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            if (g_pageBtn.handle) {
                __try { setPositionReal_fn(g_pageBtn.handle, g_pageBtn.x, g_pageBtn.y); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            if (g_ctrlCreated) {
                for (int i = 0; i < CTRL_COUNT; ++i) {
                    if (g_ctrl[i].handle) {
                        __try { setPositionReal_fn(g_ctrl[i].handle, g_ctrl[i].x, g_ctrl[i].y); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                }
            }
        }

        // 1b. 创建+渲染+轮询控制条 (每帧, 仅当主面板存在)
        if (g_ctrlCreated && fgui) {
            RenderCtrl();
            PollCtrlClick();
        } else if (!g_ctrlCreated && fgui) {
            EnsureCtrlCreated(fgui);
            if (g_ctrlCreated) RenderCtrl();
        }

        // 2. 选中角色判断（健壮保护）
        Character* ch = 0;
        void* selPtr = 0;
        if (fgui && getSelected_fn && handGetChar_fn)
        {
            __try {
                selPtr = getSelected_fn(fgui);
                if (selPtr && !IsBadReadPtr(selPtr, 8)) {
                    ch = handGetChar_fn(selPtr);
                    if (ch && IsBadReadPtr(ch, 32)) {
                        ch = 0;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ch = 0;
            }
        }

        // 3. 检查模式是否有变动（如按了 F6, 9+0 或 F8+1..5 攻速切换）
        int currentScope = GetCurrentScopeMode();
        bool isAscActive = GetAscensionEnabled();
        float currentCap = GetCurrentAttackSpeedCap();
        static float g_lastRenderedCap = -1.0f;
        bool stateChanged = (currentScope != g_lastRenderedScope || isAscActive != g_lastAscState || currentCap != g_lastRenderedCap);

        if (stateChanged)
        {
            g_lastRenderedScope = currentScope;
            g_lastAscState = isAscActive;
            g_lastRenderedCap = currentCap;
            UpdatePanelTitles(currentScope, isAscActive);
            if (ch)
            {
                RenderCharacterPanels(ch, isAscActive);
            }
        }

        if (ch == 0)
        {
            // 修复: 仅当真正"无任何选中"(selPtr==0, 场景切换/载入) 才丢弃面板自愈。
            // 若 selPtr 非空 (选中了物品/地面/建筑等非角色), ch==0 属正常, 不丢弃,
            // 避免玩家点物品/地面时主副面板突然消失的 bug。
            if (selPtr == 0)
            {
                // P3 修复: 长时间无选中角色 = 处于菜单/载入/场景切换 (Kenshi 会在此类
                // 过渡中重建 GUI 并释放外部创建的面板)。连续 ~2 秒无角色则隐藏面板并
                // 丢弃引用 (不手动 destroy, 防双释; 旧面板由引擎下次重建时自行清理)。
                g_noCharFrames++;
                if (g_created && g_noCharFrames > 120)
                {
                    ErrorLog("DivineGraceUI: DEGRADE drop: noCharFrames=" + std::to_string(g_noCharFrames) + " selPtr=0 (true scene switch)");
                    if (show_fn)
                    {
                        __try { show_fn(g_gracePanel, false); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                    // 体检修正 (2026-09-06): 丢弃前隐藏控制卡与翻页钮 — 否则 handle 清零后
                    // 引擎侧残留一组永远无法再隐藏的"幽灵按钮"
                    CtrlShowAll(false);
                    DropAllPanelRefs(0);   // 丢弃原因已由上方 DEGRADE 日志给出
                    g_panelVisible = !g_uiHiddenByUser;   // 丢弃后保持 F8 隐藏意图 (勿复位 true)
                    ErrorLog("DivineGraceUI: no selected char for 2s, panels dropped (will recreate on next selection)");
                }
            }
            return;
        }
        g_noCharFrames = 0;
        if (!g_created)
        {
            // 界面过渡结束后重新创建面板 (基于当前 GUI 实例)
            g_needCreate = true;
            if (g_needCreate && !g_created)
            {
                EnsurePanelsCreated();
            }
        }
        if (!g_created || g_gracePanel == 0) return;

        // 3b. 重建分帧摊销: 未写完的行每帧继续 (每帧≤8行, 见 RenderCharacterPanels)
        if (!g_pendingLines.empty()) FlushPendingLines();

        // 4. F8 隐藏 = 持久用户意图, 每帧强制校验压回: 引擎在点选角色等场景可能经
        // ForgottenGUI/getGUIData 链自行 re-show 我们的面板 ("点角色 UI 复活"根因之一)
        if (g_uiHiddenByUser && g_created && g_panelVisible && show_fn)
        {
            __try {
                show_fn(g_gracePanel, false);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
            CtrlShowAll(false);
            g_panelVisible = false;
            ErrorLog("DivineGraceUI: engine re-show suppressed (F8 hidden intent kept)");
        }

        // 5. 若处于隐藏状态，不执行渲染
        if (!g_panelVisible)
        {
            return;
        }

        // 5. 角色切换 或 定期刷新 (P3 瘦身: 30帧 -> 150帧 ≈ 2.5s 一次全量重绘, 降稳态 CPU)
        g_refreshTimer++;
        if (ch != g_displayedChar || g_refreshTimer >= 150)
        {
            g_refreshTimer = 0;
            g_displayedChar = ch;

            UpdatePanelTitles(currentScope, isAscActive);
            RenderCharacterPanels(ch, isAscActive);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================
// getGUIDataForMainInfo hook：仅设创建标记
// P3 修复: orig(引擎) 调用必须 SEH 包裹 - 引擎 GUI 数据填充路径若因
// 悬垂面板/半初始化数据抛异常, 无包裹会直接逃逸成进程闪退
// (0x319930522 崩溃特征即此路径: kenshi_x64.exe CharStats GUI 填充链)。
// ============================================================
static void getGUIData_hook(CharStats* thisptr, void* datapanel, int category, bool combatMode)
{
    __try
    {
        if (getGUIData_orig)
            getGUIData_orig(thisptr, datapanel, category, combatMode);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // 引擎 GUI 数据填充异常: 自愈丢弃面板引用, 待 GUI 换代后重建
        DropAllPanelRefs("getGUIData engine call raised exception");
        return;
    }

    if (!g_created)
        g_needCreate = true;
}

// ============================================================
// 初始化
// ============================================================
static void* GetKenshiLibExport(const char* name)
{
    HMODULE h = GetModuleHandleA("KenshiLib.dll");
    if (h == 0) return 0;
    return (void*)GetProcAddress(h, name);
}

static bool HookExport(const char* exportName, void* detour, void** original)
{
    void* stub = GetKenshiLibExport(exportName);
    if (stub == 0) { ErrorLog("DivineGraceUI: export not found: " + std::string(exportName)); return false; }
    intptr_t realAddr = KenshiLib::GetRealAddress(stub);
    if (realAddr == 0) { ErrorLog("DivineGraceUI: failed to resolve " + std::string(exportName)); return false; }
    return (KenshiLib::SUCCESS == KenshiLib::AddHook((void*)realAddr, detour, original));
}

__declspec(dllexport) void startPlugin()
{
    // DPI 感知: 窗口/鼠标物理逻辑坐标一致化 - UI 与点击命中在系统缩放下不偏移
    SetProcessDPIAware();
    ErrorLog("DivineGraceUI: initializing Dual-Panel Dynamic Filtered UI...");
    ErrorLog(std::string("DivineGraceUI: build ") + __DATE__ + " " + __TIME__ + " (3-page; CN input = separate CnInputPanel mod)");

    // 原生辅助调用函数直接使用 KenshiLib 导出的桩函数指针，保证 ABI 与对象传递安全
    createDatapanel_fn = (CreateDatapanelFn)GetKenshiLibExport(
        "?createDatapanel@ForgottenGUI@@QEAAPEAVDatapanelGUI@@MMMM_NV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z");
    setCaption_fn = (SetCaptionFn)GetKenshiLibExport(
        "?setCaption@DatapanelGUI@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
    setPanelName_fn = (SetNameFn)GetKenshiLibExport(
        "?setPanelName@DatapanelGUI@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
    setPositionReal_fn = (SetPosRealFn)GetKenshiLibExport(
        "?setPositionReal@DatapanelGUI@@UEAAXMM@Z");
    resize_fn = (ResizeFn)GetKenshiLibExport(
        "?resize@DatapanelGUI@@UEAAXHH@Z");
    show_fn = (ShowFn)GetKenshiLibExport(
        "?show@DatapanelGUI@@UEAAX_N@Z");
    isVisible_fn = (IsVisibleFn)GetKenshiLibExport(
        "?isVisible@GUIWindow@@UEBA_NXZ");
    setLineStatInfo_fn = (SetLineFn)GetKenshiLibExport(
        "?setLineStatInfo@DatapanelGUI@@QEAAPEAVDataPanelLine@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0H@Z");
    clearPage_fn = (ClearPageFn)GetKenshiLibExport(
        "?clearPage@DatapanelGUI@@UEAAXH@Z");
    clear_fn = (void (*)(void*))GetKenshiLibExport(
        "?clear@DatapanelGUI@@UEAAXXZ");
    updatePanel_fn = (void (*)(void*))GetKenshiLibExport(
        "?update@DatapanelGUI@@UEAAXXZ");
    // 视图坐标系标定用 (2026-09-06): 读回控件真实尺寸
    getWidth_fn  = (GetWFn)GetKenshiLibExport("?getWidth@GUIWindow@@UEBAHXZ");
    getHeight_fn = (GetWFn)GetKenshiLibExport("?getHeight@GUIWindow@@UEBAHXZ");
    getSelected_fn = (GetSelectedFn)GetKenshiLibExport(
        "?getSelectedObject@ForgottenGUI@@QEBAAEBVhand@@XZ");
    handGetChar_fn = (HandGetCharFn)GetKenshiLibExport(
        "?getCharacter@hand@@QEBAPEAVCharacter@@XZ");
    charGetStats_fn = (CharGetStatsFn)GetKenshiLibExport(
        "?getStats@Character@@QEAAPEAVCharStats@@XZ");
    isAnimal_fn = (IsAnimalFn)GetKenshiLibExport(
        "?isAnimal@Character@@UEAAPEAVCharacterAnimal@@XZ");
    getRace_fn = (GetRaceFn)GetKenshiLibExport(
        "?getRace@Character@@UEBAPEAVRaceData@@XZ");
    getStat_orig = (GetStatFn)GetKenshiLibExport(
        "?getStat@CharStats@@QEBAMW4StatsEnumerated@@_N@Z");

    // 控制条 setter (从 DesolationSystem.dll 拉取, 与 getter 同一通道)
    HMODULE hDeso = GetModuleHandleA("DesolationSystem.dll");
    if (hDeso) {
        setScopeMode_fn        = (SetScopeModeFn)GetProcAddress(hDeso, "SetDesolationScopeMode");
        packCount_fn           = (GetPackCountFn)GetProcAddress(hDeso, "GetDesolationPackCount");
        ryuSet_fn              = (RyuSetStyleFn)GetProcAddress(hDeso, "RyuSetStyle");
        ryuGet_fn              = (RyuGetStyleFn)GetProcAddress(hDeso, "RyuGetStyle");
        setAscension_fn        = (SetAscensionFn)GetProcAddress(hDeso, "SetAscensionPerksEnabled");
        setAttackSpeedCap_fn   = (SetAttackSpeedCapFn)GetProcAddress(hDeso, "SetDesolationAttackSpeedCap");
        g_gateFn               = (PerkGateFn)GetProcAddress(hDeso, "IsDesolationPerkGateMet");
        if (setScopeMode_fn && setAscension_fn && setAttackSpeedCap_fn)
            ErrorLog("DivineGraceUI: control-panel setters resolved (scope/asc/speed).");
        else
            ErrorLog("DivineGraceUI: WARN some control setters missing (DesolationSystem version mismatch?).");
        ErrorLog(std::string("DivineGraceUI: perk gate lookup ") + (g_gateFn ? "resolved (M3 registry gates)" : "MISSING (fallback: gates assume met)"));
    } else {
        ErrorLog("DivineGraceUI: WARN DesolationSystem.dll not loaded - control panel disabled.");
    }

    if (createDatapanel_fn == 0 || setLineStatInfo_fn == 0 || getStat_orig == 0)
    {
        ErrorLog("DivineGraceUI: critical exports missing, aborting hook.");
        return;
    }

    bool h2 = HookExport("?update@ForgottenGUI@@QEAAXXZ",
                         (void*)&fguiUpdate_hook, (void**)&fguiUpdate_orig);
    if (!h2) ErrorLog("DivineGraceUI: FAILED to hook ForgottenGUI::update");
    else     ErrorLog("DivineGraceUI: hooked ?update@ForgottenGUI@@QEAAXXZ");

    bool h3 = HookExport("?getGUIDataForMainInfo@CharStats@@QEAAXPEAVDatapanelGUI@@H_N@Z",
                         (void*)&getGUIData_hook, (void**)&getGUIData_orig);
    if (!h3) ErrorLog("DivineGraceUI: FAILED to hook getGUIDataForMainInfo");
    else     ErrorLog("DivineGraceUI: hooked ?getGUIDataForMainInfo@CharStats@@QEAAXPEAVDatapanelGUI@@H_N@Z");

    ErrorLog("DivineGraceUI: Dual-Panel initialized! [F8 唤出/显隐, 底部按钮切换 模式/攻速/极境]");
}
