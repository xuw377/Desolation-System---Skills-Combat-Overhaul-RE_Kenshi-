// ============================================================================
// 本文件是 一键断肢 (One-Click Amputation) 的一部分。
// Copyright (C) 2026 kenshi-mod-dev
//
// 本程序是自由软件：你可以根据自由软件基金会发布的 GNU 通用公共许可证
// (第 3 版或更高版本) 再分发和/或修改它。
// 本程序的发布希望它有用，但【不提供任何保证】。
// 详见 GNU 通用公共许可证 (LICENSE 文件)。
//
// This file is part of 一键断肢 (One-Click Amputation).
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

// SkeletonLimbHotSwap.cpp
// 一键断肢 (One-Click Amputation) v2.7 - 游戏内可点击小面板 + 键位自定义
//
// 架构 (v2.6.1+): 多面板数组 - 每个按钮 = 一个独立超小 datapanel
//   - 解决了单 datapanel 多行 setLineStatInfo 行距与热区错位的根因
//   - 每个子面板 setPositionReal 钉在固定坐标, 只有一行标题文字
//   - 点击热区 = 各自矩形, 文字渲染位置与可点击区天然一致
//
// 特性：
//   1. 纯净底层直调断肢 (crush=true 零碎片)，瞬间满血止血
//   2. 【小面板 UI】游戏内 Ctrl+E 呼出/隐藏 (可在面板内自行改此键)
//      - 每个按钮 = 独立子面板: 普通模式单击即断该肢
//      - 最底 [改键模式] 子面板: 点击进入改键 -> 点呼出键卡 -> 按下新键 (Esc 取消)
//   3. 呼出键 INI 化 (mods/SkeletonLimbHotSwap/SkeletonLimbHotSwap.ini [Hotkeys])
//   4. 生命周期自愈: 引擎 GUI 换代自动丢弃重建; 全程 SEH 包裹
//
// 方案A: 断肢全走面板鼠标点击 (无 Ctrl+1/2/3/4/L 快捷键)
// 默认键位:
//   - Ctrl + E          : 呼出/隐藏面板 (可在面板内改)

#include <windows.h>
#include <string>
#include <fstream>
#include <cstdio>

#include <Defines.h>
#include <core/Functions.h>   // KenshiLib::GetRealAddress / AddHook (hook 场景)

class Character;
class MedicalSystem;

struct Vector3 {
    float x, y, z;
};

// ============================================================
// 引擎函数 (直调场景 - 手写桩解析, 实证可靠)
// ============================================================
typedef void* (*GetSelectedFn)(void* fgui);
typedef Character* (*HandGetCharFn)(void* hand);
typedef MedicalSystem* (*GetMedicalFn)(Character* ch);
typedef void (*AmputateFn)(MedicalSystem* med, int limb, bool crush, const Vector3& pos);
typedef void (*HealCompletelyFn)(Character* ch);
typedef void* (*IsAnimalFn)(Character* thisptr);

static GetSelectedFn      getSelected_fn       = 0;
static HandGetCharFn      handGetChar_fn       = 0;
static GetMedicalFn       getMedical_fn        = 0;
static AmputateFn         amputate_fn          = 0;
static HealCompletelyFn   healCompletely_fn    = 0;
static IsAnimalFn         isAnimal_fn          = 0;

// ============================================================
// 面板 UI 引擎函数 (与 DivineGraceUI 同款导出)
// ============================================================
typedef void* (*CreateDatapanelFn)(void* fgui, float x, float y, float w, float h, bool visible,
                                   const std::string& name, const std::string& skin);
typedef void  (*SetCaptionFn)(void* datapanel, const std::string& caption);
typedef void* (*SetLineFn)(void* datapanel, const std::string& s1, const std::string& s2, int category);
typedef void  (*ShowFn)(void* datapanel, bool on);
typedef void  (*SetPosRealFn)(void* datapanel, float x, float y);
typedef void  (*ResizeFn)(void* datapanel, int w, int h);   // 像素尺寸 (Divine 同款)

static CreateDatapanelFn createDatapanel_fn = 0;
static SetCaptionFn      setCaption_fn      = 0;
static SetLineFn         setLineStatInfo_fn = 0;
static ShowFn            show_fn            = 0;
static SetPosRealFn      setPositionReal_fn = 0;
static ResizeFn          resize_fn          = 0;

// ============================================================
// 键位绑定系统 (INI: [Hotkeys])
// ============================================================
static void LogMsg(const std::string& msg);   // 前向 (定义见基础设施区)
enum {
    BIND_TOGGLE = 0,   // 呼出/隐藏面板 (唯一保留的键盘绑定)
    BIND_COUNT         // 方案A: 只此一个快捷键
};

struct BindDef {
    const char* iniKey;
    const char* label;
    int  vk;             // 当前主键
    bool ctrl, shift, alt;
    int  dVk;            // 默认
    bool dCtrl;
};

static BindDef g_binds[BIND_COUNT] = {
    { "TogglePanel", "呼出键", 'E', true, false, false, 'E', true },
};

// 断肢动作标签 (鼠标点击触发, 方案A 下无键盘绑定)
// UI 运行时双语 (2026-09-07): 外国玩家缺 CJK 字体时中文 UI 显示空白, 加 Language 卡切换。
// Language 卡 caption 恒英文, 保证任何字体环境可读可点。选择持久化至 ini [UI] English。
static bool g_langEn = false;
static const char* T(const char* zh, const char* en) { return g_langEn ? en : zh; }
static const char* g_limbLabels[5] = { "左臂", "右臂", "左腿", "右腿", "全部四肢" };
static const char* g_limbLabelsEn[5] = { "L Arm", "R Arm", "L Leg", "R Leg", "All Limbs" };

static std::string GetIniPath()
{
    static std::string iniPath = "";
    if (iniPath.empty()) {
        char exePath[MAX_PATH] = { 0 };
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) *lastSlash = '\0';
        iniPath = std::string(exePath) + "\\mods\\SkeletonLimbHotSwap\\SkeletonLimbHotSwap.ini";
    }
    return iniPath;
}

// 键名解析: 支持 A-Z / 0-9 / F1-F12 / Esc / 大小写不敏感
static int KeyNameToVk(const std::string& name)
{
    if (name.size() == 1) {
        char c = (char)toupper(name[0]);
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return c;
    }
    if (name.size() >= 2 && name[0] == 'F') {
        int n = atoi(name.c_str() + 1);
        if (n >= 1 && n <= 12) return VK_F1 + (n - 1);
    }
    if (name == "Esc" || name == "ESC") return VK_ESCAPE;
    return 0;
}

static std::string VkToDisplay(int vk)
{
    char buf[16] = { 0 };
    if (vk >= 'A' && vk <= 'Z') { buf[0] = (char)vk; return buf; }
    if (vk >= '0' && vk <= '9') { buf[0] = (char)vk; return buf; }
    if (vk >= VK_F1 && vk <= VK_F12) { sprintf_s(buf, "F%d", vk - VK_F1 + 1); return buf; }
    if (vk == VK_ESCAPE) return "Esc";
    sprintf_s(buf, "VK%d", vk);
    return buf;
}

static std::string BindToStr(int idx)
{
    std::string s;
    if (g_binds[idx].ctrl) s += "Ctrl+";
    if (g_binds[idx].shift) s += "Shift+";
    if (g_binds[idx].alt) s += "Alt+";
    s += VkToDisplay(g_binds[idx].vk);
    return s;
}

// 解析 "Ctrl+E" 式字符串 -> 绑定
static bool ParseBindStr(const std::string& str, int& vk, bool& ctrl, bool& shift, bool& alt)
{
    vk = 0; ctrl = shift = alt = false;
    std::string s = str;
    size_t pos;
    while ((pos = s.find('+')) != std::string::npos) {
        std::string mod = s.substr(0, pos);
        s = s.substr(pos + 1);
        if (mod == "Ctrl" || mod == "CTRL" || mod == "ctrl") ctrl = true;
        else if (mod == "Shift" || mod == "SHIFT") shift = true;
        else if (mod == "Alt" || mod == "ALT") alt = true;
        else return false;
    }
    vk = KeyNameToVk(s);
    return vk != 0;
}

static void LoadBinds()
{
    std::string ini = GetIniPath();
    for (int i = 0; i < BIND_COUNT; ++i) {
        char buf[64] = { 0 };
        GetPrivateProfileStringA("Hotkeys", g_binds[i].iniKey, "", buf, sizeof(buf), ini.c_str());
        if (buf[0]) {
            int vk; bool c, sh, al;
            if (ParseBindStr(buf, vk, c, sh, al)) {
                g_binds[i].vk = vk; g_binds[i].ctrl = c; g_binds[i].shift = sh; g_binds[i].alt = al;
            }
        }
    }
    g_langEn = GetPrivateProfileIntA("UI", "English", 0, ini.c_str()) != 0;
}

static void SaveBind(int idx)
{
    std::string ini = GetIniPath();
    WritePrivateProfileStringA("Hotkeys", g_binds[idx].iniKey, BindToStr(idx).c_str(), ini.c_str());
    LogMsg("[Bind] " + std::string(g_binds[idx].label) + " -> " + BindToStr(idx));
}

// ============================================================
// 断肢执行 (SEH 全覆盖; 不触碰引擎 STL)
// ============================================================
static bool IsCharacterAnimal(Character* ch);
static const char* GetLimbName(int limb);

static void ExecutePreciseLimbUnlock(Character* ch, int targetLimb)
{
    if (!ch) return;
    __try
    {
        if (IsCharacterAnimal(ch)) {
            LogMsg("[PreciseUnlock] Target is animal. Skipped.");
            return;
        }

        const std::string charName = "Character";   // 不取引擎名 (VS2010 STL 跨 ABI)

        MedicalSystem* med = nullptr;
        if (getMedical_fn) {
            __try { med = getMedical_fn(ch); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        if (!med) med = (MedicalSystem*)((char*)ch + 0x458);   // 官方 Character::medical@0x458

        Vector3 zeroPos = { 0.0f, 0.0f, 0.0f };
        if (amputate_fn && med) {
            if (targetLimb >= 0 && targetLimb < 4) {
                __try { amputate_fn(med, targetLimb, true, zeroPos); }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            } else {
                for (int limb = 0; limb < 4; ++limb) {
                    __try { amputate_fn(med, limb, true, zeroPos); }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
            if (healCompletely_fn) {
                __try { healCompletely_fn(ch); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            std::string limbDesc = (targetLimb >= 0 && targetLimb < 4) ? GetLimbName(targetLimb) : "全部四肢";
            LogMsg("★ [一键断肢] 角色【" + charName + "】的【" + limbDesc + "】已安全解锁！止血急救完毕！");
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        LogMsg("[PreciseUnlock] Exception in ExecutePreciseLimbUnlock.");
    }
}

// ============================================================
// 基础设施
// ============================================================
static void**             g_pGuiVar            = nullptr;
static HANDLE             g_hThread            = NULL;
static bool               g_threadRunning      = true;
static DWORD              g_lastActionTime     = 0;
static bool               g_keyPrev[BIND_COUNT] = { false };   // 防按住连发 (0->1 沿触发)
static volatile LONG      g_pendingTogglePanel = 0;            // 线程 -> 主线程(hook) 呼出标志

// ============================================================
// 面板状态: 多面板数组 (仅主线程 hook 内访问)
// ============================================================
// 面板卡动作类型 (与键盘绑定解耦; 断肢卡始终可由鼠标点击触发)
enum {
    ACT_NONE   = -1,   // 标题卡 (不可点击)
    ACT_TOGGLE = 0,    // 呼出/隐藏面板 (点击即隐藏)
    ACT_LA,            // 左臂
    ACT_RA,            // 右臂
    ACT_LL,            // 左腿
    ACT_RL,            // 右腿
    ACT_ALL,           // 全部四肢
    ACT_MODE,          // 改键模式
    ACT_LANG,          // 语言切换 (caption 恒英文)
    ACT_COUNT
};
enum {
    SUB_TITLE = 0,       // 顶部标题
    // ACT_TOGGLE..ACT_ALL 对应 index SUB_TITLE+1..SUB_TITLE+6
    SUB_MODE  = SUB_TITLE + ACT_COUNT,   // 最底改键模式子面板
    SUB_LANG  = SUB_MODE + 1,            // 语言切换子面板 (最底)
};
#define SUB_TOTAL   (SUB_LANG + 1)   // 标题 + 8 动作卡

struct SubPanel {
    void* handle;
    float x, y, w, h;
    int   act;      // ACT_* 值
};

static SubPanel g_subs[SUB_TOTAL];

static void*  g_panelOwnerBar    = 0;      // 创建时的 MainBarGUI thisptr (换代检测)
static bool   g_panelCreated     = false;
static bool   g_panelVisible     = false;  // 默认隐藏, Ctrl+E 呼出
static bool   g_panelDirty       = false;
static bool   g_bindMode         = false;
static int    g_bindingIdx       = -1;     // >=0 等待按键
static bool   g_mousePrevDown    = false;
static DWORD  g_lastClickTick    = 0;
static int    g_createFailStreak = 0;
static DWORD  g_lastCreateTry    = 0;
static int    g_renderTick       = 0;

static inline bool IsGameWindowFocused() {
    HWND fgWnd = GetForegroundWindow();
    if (!fgWnd) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fgWnd, &pid);
    return (pid == GetCurrentProcessId());
}

static void LogMsg(const std::string& msg)
{
    static std::string logPath = "";
    if (logPath.empty()) {
        char exePath[MAX_PATH] = { 0 };
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) *lastSlash = '\0';
        logPath = std::string(exePath) + "\\mods\\SkeletonLimbHotSwap\\SkeletonLimbHotSwap.log";
    }
    std::ofstream ofs(logPath, std::ios::app);
    if (ofs.is_open()) { ofs << msg << "\n"; ofs.flush(); }
}

static void* GetKenshiLibExport(const char* name)
{
    HMODULE h = GetModuleHandleA("KenshiLib.dll");
    if (h == 0) return 0;
    return (void*)GetProcAddress(h, name);
}

// 直调场景: 手写桩解析 (实证可靠)
static void* ResolveKenshiExport(const char* name)
{
    void* stub = GetKenshiLibExport(name);
    if (stub == 0) return 0;
    unsigned char* p = (unsigned char*)stub;
    if (p[0] == 0xE9) return (void*)(p + 5 + *(int*)(p + 1));
    if (p[0] == 0xFF && p[1] == 0x25) return *(void**)(p + 6 + *(int*)(p + 2));
    return stub;
}

// Hook 场景: 官方 GetRealAddress + AddHook
static bool HookEngineExport(const char* exportName, void* detour, void** original)
{
    void* stub = GetKenshiLibExport(exportName);
    if (stub == 0) { LogMsg("[Hook] export missing: " + std::string(exportName)); return false; }
    intptr_t real = KenshiLib::GetRealAddress(stub);
    if (real == 0) { LogMsg("[Hook] resolve failed: " + std::string(exportName)); return false; }
    return (KenshiLib::SUCCESS == KenshiLib::AddHook((void*)real, detour, original));
}

static bool IsCharacterAnimal(Character* ch)
{
    if (!ch) return false;
    if (isAnimal_fn) {
        __try { return isAnimal_fn(ch) != nullptr; } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return false;
}

static const char* GetLimbName(int limb)
{
    switch (limb) {
    case 0: return "左臂"; case 1: return "右臂";
    case 2: return "左腿"; case 3: return "右腿";
    default: return "四肢";
    }
}

// ============================================================
// 多面板 UI (仅主线程调用)
// ============================================================
// 子面板几何: 左上角纵向堆叠 (标题 + 7 个按钮)
// 位置用归一化 setPositionReal 锁定; 尺寸用 resize() 以像素精确控制
// 卡片间留少量像素间隙, 视觉上既贴近又不粘连
static const float PANEL_X   = 0.03f;   // 面板列左边缘 (归一化)
static const float PANEL_Y   = 0.25f;   // 顶部起点 (归一化; 2026-09-07 上移避让 HUD 信息, 原 0.40)
static const int   PANEL_WPX = 300;     // 每个子面板像素宽
static const int   TITLE_HPX = 38;      // 标题子面板像素高
static const int   BTN_HPX   = 36;      // 按钮子面板像素高 (足够容纳文字+留白)
static const float CARD_GAPPX = -2.0f;  // 大负间距测试: 验证引擎是否接受负值/是否被clamp到0

static void InitSubLayout()
{
    // 归一化尺寸: 用窗口像素换算 (1080p 基准, 其他分辨率按比例)
    float winW = 1920.0f, winH = 1080.0f;
    {
        HWND fg = GetForegroundWindow();
        if (fg) {
            RECT rc;
            if (GetClientRect(fg, &rc)) {
                if (rc.right > 0)  winW = (float)rc.right;
                if (rc.bottom > 0) winH = (float)rc.bottom;
            }
        }
    }
    float titleHn = (float)TITLE_HPX / winH;   // 归一化高
    float btnHn   = (float)BTN_HPX   / winH;
    float gapN    = (float)CARD_GAPPX / winH;  // 归一化间隙
    float wN      = (float)PANEL_WPX / winW;    // 归一化宽

    float y = PANEL_Y;
    for (int i = 0; i < SUB_TOTAL; ++i) {
        g_subs[i].x = PANEL_X;
        g_subs[i].w = wN;
        g_subs[i].act = (i == SUB_TITLE) ? ACT_NONE
                      : (i == SUB_MODE)  ? ACT_MODE
                      : (i == SUB_LANG)  ? ACT_LANG
                      : (ACT_TOGGLE + (i - SUB_TITLE - 1));
        g_subs[i].h = (i == SUB_TITLE) ? titleHn : btnHn;
        g_subs[i].y = y;
        y += g_subs[i].h + gapN;   // 边贴边 (gap=0)
    }
}

static void DropPanelRefs()
{
    for (int i = 0; i < SUB_TOTAL; ++i) g_subs[i].handle = 0;
    g_panelCreated = false;
    g_panelDirty = false;
    g_panelOwnerBar = 0;
    g_bindMode = false;
    g_bindingIdx = -1;
}

// 显示/隐藏所有子面板
static void ShowAll(bool on)
{
    if (!show_fn) return;
    for (int i = 0; i < SUB_TOTAL; ++i) {
        if (g_subs[i].handle) {
            __try { show_fn(g_subs[i].handle, on); } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
}

// 重绘所有子面板标题 (每个按钮独立 setCaption, 无行距问题)
static void RenderPanel()
{
    if (!g_panelCreated || !setCaption_fn) return;
    __try {
        // 标题子面板
        if (g_subs[SUB_TITLE].handle) {
            setCaption_fn(g_subs[SUB_TITLE].handle,
                std::string(T("【一键断肢】呼出键 [", "[One-Click Amputation] Toggle [")) + BindToStr(BIND_TOGGLE) + "]");
        }
        // 7 个动作按钮子面板 (ACT_TOGGLE..ACT_ALL 对应 SUB_TITLE+1+act .. )
        for (int act = ACT_TOGGLE; act <= ACT_ALL; ++act) {
            SubPanel& sp = g_subs[SUB_TITLE + 1 + act];
            if (!sp.handle) continue;
            std::string text;
            if (act == ACT_TOGGLE) {
                // 呼出键卡: 改键模式下显示捕获提示, 否则显示呼出键
                if (g_bindingIdx == BIND_TOGGLE) text = T("<< 呼出键 << 按新键(Esc取消)", "<< Toggle << press new key (Esc to cancel)");
                else text = std::string(T("呼出键 [", "Toggle [")) + BindToStr(BIND_TOGGLE) + T("] (点击隐藏)", "] (click to hide)");
            } else {
                // 断肢卡: 明确点击动作 (无快捷键)
                int limb = act - ACT_LA;   // ACT_LA=1 -> limb0, ACT_ALL=5 -> limb4
                int labelIdx = (limb >= 4) ? 4 : limb;   // 全部四肢
                text = std::string(T("断 ", "Cut ")) + (g_langEn ? g_limbLabelsEn[labelIdx] : g_limbLabels[labelIdx]);
            }
            setCaption_fn(sp.handle, text);
        }
        // 模式按钮子面板
        SubPanel& mode = g_subs[SUB_MODE];
        if (mode.handle) {
            std::string m;
            if (g_bindingIdx >= 0) m = T(">> 请按键 <<", ">> press a key <<");
            else m = g_bindMode ? T("改键模式: 开 [点击退出]", "Rebind: ON [click to exit]")
                                : T("改键模式: 关 [点击进入]", "Rebind: OFF [click to bind]");
            setCaption_fn(mode.handle, m);
        }
        // 语言卡 (caption 恒英文)
        SubPanel& lang = g_subs[SUB_LANG];
        if (lang.handle) {
            setCaption_fn(lang.handle,
                std::string("Language: ") + (g_langEn ? "EN" : "CN 中文"));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void EnsurePanelCreated(void* barThis)
{
    if (g_panelCreated || createDatapanel_fn == 0) return;
    // 创建失败退避 (引擎半失败创建防护)
    DWORD now = GetTickCount();
    if (g_createFailStreak >= 3 && (now - g_lastCreateTry) < 5000) return;
    g_lastCreateTry = now;

    __try {
        HMODULE h = GetModuleHandleA("KenshiLib.dll");
        void* pGuiVar = (void*)GetProcAddress(h, "?gui@@3PEAVForgottenGUI@@EA");
        if (!pGuiVar) { g_createFailStreak++; return; }
        void* fgui = *(void**)pGuiVar;
        if (!fgui) { g_createFailStreak++; return; }

        InitSubLayout();
        bool allOk = true;
        for (int i = 0; i < SUB_TOTAL; ++i) {
            SubPanel& sp = g_subs[i];
            void* panel = createDatapanel_fn(fgui, sp.x, sp.y, sp.w, sp.h, g_panelVisible,
                                             std::string("Middle"), std::string("Kenshi_FloatingPanelThinSkin"));
            if (!panel) { allOk = false; break; }
            sp.handle = panel;
            if (setPositionReal_fn) {
                __try { setPositionReal_fn(panel, sp.x, sp.y); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            // resize 以像素精确控制卡片尺寸 (Divine 同款, 覆盖 createDatapanel 的 w/h)
            if (resize_fn) {
                int wpx = PANEL_WPX;
                int hpx = (i == SUB_TITLE) ? TITLE_HPX : BTN_HPX;
                __try { resize_fn(panel, wpx, hpx); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        if (!allOk) {
            LogMsg("[UI] createDatapanel partial failure, rolling back.");
            for (int i = 0; i < SUB_TOTAL; ++i) g_subs[i].handle = 0;
            g_createFailStreak++;
            return;
        }

        g_panelOwnerBar = barThis;
        g_panelCreated = true;
        g_panelDirty = true;
        g_createFailStreak = 0;

        RenderPanel();
        LogMsg(std::string("[UI] ") + std::to_string(SUB_TOTAL) + " sub-panels created.");
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_createFailStreak++; }
}

// ============================================================
// 点击采样 (描点排障): 屏幕像素 + 归一化坐标 + 命中子面板
// ============================================================
struct ClickSample {
    int   px = 0, py = 0, winW = 0, winH = 0;
    float nx = -1.0f, ny = -1.0f;
    int   row = -1;   // -1 = 未命中; 否则 = ACT_* 值 (0=呼出,1..4=肢,5=全部,6=模式)
    int   miss = 0;   // 0=命中 1=x外部 2=y外部 4=命中标题(忽略) 3=无窗口
};

static std::string FloatToStr(float v)
{
    char b[32] = { 0 };
    sprintf_s(b, "%.4f", v);
    return b;
}

// 屏幕坐标 -> 子面板命中 (遍历所有子面板矩形, 文字渲染与矩形一致)
static void SamplePanelClick(ClickSample& s)
{
    HWND fg = GetForegroundWindow();
    if (!fg) { s.miss = 3; return; }
    POINT pt;
    if (!GetCursorPos(&pt)) { s.miss = 3; return; }
    RECT rc;
    if (!GetClientRect(fg, &rc) || rc.right <= 0 || rc.bottom <= 0) { s.miss = 3; return; }
    ScreenToClient(fg, &pt);
    s.px = pt.x; s.py = pt.y;
    s.winW = rc.right; s.winH = rc.bottom;
    s.nx = (float)pt.x / (float)rc.right;
    s.ny = (float)pt.y / (float)rc.bottom;
    // 横向: 所有子面板同一 x 范围 (用标题/首卡片的 w 判定)
    if (s.nx < PANEL_X || s.nx > PANEL_X + g_subs[0].w) { s.miss = 1; return; }
    // 逐个子面板判纵向命中
    for (int i = 0; i < SUB_TOTAL; ++i) {
        SubPanel& sp = g_subs[i];
        if (!sp.handle) continue;
        if (s.ny >= sp.y && s.ny <= sp.y + sp.h) {
            if (sp.act < 0) { s.miss = 4; return; }   // 标题
            s.row = sp.act;
            s.miss = 0;
            return;
        }
    }
    s.miss = 2;   // 面板列内但落在间距/下方
}

// 主线程: 对当前选中角色执行断肢
static void AmputateSelected(int limb)
{
    __try {
        if (g_pGuiVar && *g_pGuiVar && getSelected_fn && handGetChar_fn) {
            void* fgui = *g_pGuiVar;
            void* sel = getSelected_fn(fgui);
            if (sel) {
                Character* ch = handGetChar_fn(sel);
                if (ch) ExecutePreciseLimbUnlock(ch, limb);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// 主线程: 处理面板点击 (左键上升沿)
// 记录无条件 (描点数据); 动作仅在 非Shift 且面板存在 时执行
static void PollPanelClick()
{
    bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool clicked = down && !g_mousePrevDown;
    g_mousePrevDown = down;
    if (!clicked || !IsGameWindowFocused()) return;

    // 修复: 面板隐藏后热区仍可点击(隐形可点)的隐患。
    // 面板一旦不可见, 点击完全不响应 - 不采样/不记录/不动作,
    // 防止玩家在背包/地图等场景鼠标漂到左缘热区时误触断肢。
    if (!g_panelCreated || !g_panelVisible) return;

    DWORD now = GetTickCount();
    if (now - g_lastClickTick < 120) return;   // 防抖 (描点连点允许更快)
    g_lastClickTick = now;

    // ---- 无条件记录 (描点数据: 像素/窗口/归一化/命中/状态) ----
    ClickSample cs;
    SamplePanelClick(cs);
    bool trace = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    {
        std::string missTxt;
        if (cs.row >= 0) missTxt = "HIT";
        else if (cs.miss == 1) missTxt = "XOUT";
        else if (cs.miss == 2) missTxt = "YOUT";
        else if (cs.miss == 4) missTxt = "TITLE";
        else missTxt = "NOPOS";
        LogMsg(std::string("[UI-DBG] ") + (trace ? "TRACE " : "") +
               "click px=" + std::to_string(cs.px) + "," + std::to_string(cs.py) +
               " win=" + std::to_string(cs.winW) + "x" + std::to_string(cs.winH) +
               " nx=" + FloatToStr(cs.nx) + " ny=" + FloatToStr(cs.ny) +
               " row=" + std::to_string(cs.row) + "(" + missTxt + ")" +
               " vis=" + (g_panelCreated && g_panelVisible ? "1" : "0") +
               " mode=" + (g_bindMode ? "1" : "0") +
               " bind=" + std::to_string(g_bindingIdx) +
               " created=" + (g_panelCreated ? "1" : "0"));
    }
    if (trace) return;              // 描点模式: 只记录不动作
    if (!g_panelCreated) return;

    int act = cs.row;             // cs.row 现在承载 ACT_* 值
    if (act < 0) return;

    if (g_bindingIdx >= 0) {
        // 捕获状态中, 点击任意处 = 取消本次捕获 (Esc 也可)
        g_bindingIdx = -1;
        g_panelDirty = true;
        return;
    }

    if (act == ACT_MODE) {        // 模式子面板
        g_bindMode = !g_bindMode;
        LogMsg(std::string("[UI] Bind mode: ") + (g_bindMode ? "ON" : "OFF"));
        g_panelDirty = true;
        return;
    }
    if (act == ACT_LANG) {        // 语言卡: 切换 + 持久化
        g_langEn = !g_langEn;
        WritePrivateProfileStringA("UI", "English", g_langEn ? "1" : "0", GetIniPath().c_str());
        LogMsg(std::string("[UI] Language: ") + (g_langEn ? "EN" : "CN"));
        g_panelDirty = true;
        return;
    }
    if (act == ACT_TOGGLE) {
        // 改键模式: 点击呼出卡 = 进入改键捕获; 普通模式: 点击 = 隐藏面板
        if (g_bindMode) {
            g_bindingIdx = BIND_TOGGLE;
            g_panelDirty = true;
            LogMsg("[UI] Awaiting new key for: " + std::string(g_binds[BIND_TOGGLE].label));
            return;
        }
        g_panelVisible = false;
        ShowAll(false);
        return;
    }
    if (act >= ACT_LA && act <= ACT_ALL) {
        int limb = act - ACT_LA;   // ACT_LA=1 -> limb0 .. ACT_ALL=5 -> limb4
        int all  = (act == ACT_ALL) ? -1 : limb;
        AmputateSelected(all);
        return;
    }
}

// 主线程: 改键捕获 (轮询新按下的主键)
static void PollBindCapture()
{
    if (g_bindingIdx < 0) return;

    // Esc 取消
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
        g_bindingIdx = -1;
        g_panelDirty = true;
        return;
    }

    bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt   = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

    // 扫描常见主键 (字母/数字/F1-12/常用功能键)
    int candidates[] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        '0','1','2','3','4','5','6','7','8','9',
        VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12,
        VK_SPACE, VK_TAB, VK_RETURN
    };
    for (int k : candidates) {
        if ((GetAsyncKeyState(k) & 0x8000) != 0) {
            int idx = g_bindingIdx;
            g_binds[idx].vk = k;
            g_binds[idx].ctrl = ctrl;
            g_binds[idx].shift = shift;
            g_binds[idx].alt = alt;
            g_bindingIdx = -1;
            SaveBind(idx);
            g_panelDirty = true;
            LogMsg("[UI] Bound: " + std::string(g_binds[idx].label) + " -> " + BindToStr(idx));
            return;
        }
    }
}

// ============================================================
// MainBarGUI::update hook (游戏内 HUD 每帧; 面板主线程时机)
// ============================================================
static void (*mainBarUpdate_orig)(void* bar) = 0;

static void mainBarUpdate_hook(void* bar)
{
    __try {
        if (mainBarUpdate_orig) mainBarUpdate_orig(bar);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // 引擎 HUD 更新异常: 丢弃面板引用自愈
        if (g_panelCreated) { DropPanelRefs(); LogMsg("[UI] engine HUD update exception, panel dropped."); }
    }

    if (!bar) return;
    __try {
        // Esc 快捷隐藏面板 (普通模式, 上升沿)
        {
            static bool s_escPrev = false;
            bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
            if (g_panelCreated && !g_bindMode && escDown && !s_escPrev) {
                g_panelVisible = false;
                ShowAll(false);
            }
            s_escPrev = escDown;
        }

        // GUI 换代检测: MainBarGUI 实例变化 -> 旧面板已随引擎释放
        if (g_panelCreated && g_panelOwnerBar && bar != g_panelOwnerBar) {
            DropPanelRefs();
            LogMsg("[UI] MainBar regenerated, panel dropped (recreate on next frame).");
        }

        // 线程呼出/隐藏请求 (仅此处可安全操作 GUI)
        if (g_pendingTogglePanel) {
            InterlockedExchange(&g_pendingTogglePanel, 0);
            if (g_panelCreated) {
                g_panelVisible = !g_panelVisible;
                ShowAll(g_panelVisible);
            } else {
                g_panelVisible = true;
                g_panelDirty = true;
            }
        }

        // 每帧锁位: 所有子面板钉在固定坐标 (FloatingPanelSkin 可拖动, 热区命中依赖固定位置)
        if (g_panelCreated && setPositionReal_fn) {
            for (int i = 0; i < SUB_TOTAL; ++i) {
                SubPanel& sp = g_subs[i];
                if (sp.handle) {
                    __try { setPositionReal_fn(sp.handle, sp.x, sp.y); } __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
        }

        if (!g_panelCreated) {
            EnsurePanelCreated(bar);
            if (g_panelCreated) RenderPanel();
        }

        // 点击采样/描点 (无条件记录; 动作需面板存在, 函数内部有守卫)
        PollPanelClick();

        if (!g_panelCreated) return;

        // 心跳 (每 ~5s): 确认 hook 存活 + 面板状态机 + 位置锁
        {
            static DWORD s_lastBeat = 0;
            DWORD nowBeat = GetTickCount();
            if (nowBeat - s_lastBeat >= 5000) {
                s_lastBeat = nowBeat;
                LogMsg(std::string("[UI-BEAT] vis=") + (g_panelVisible ? "1" : "0") +
                       " created=" + (g_panelCreated ? "1" : "0") +
                       " mode=" + (g_bindMode ? "1" : "0") +
                       " bind=" + std::to_string(g_bindingIdx) +
                       " ownerOk=" + (g_panelOwnerBar == bar ? "1" : "0"));
            }
        }

        // 每 ~12 帧重绘一次 (键位显示变化即时由 dirty 触发)
        if (g_panelDirty || (++g_renderTick >= 12)) {
            g_renderTick = 0;
            RenderPanel();
            g_panelDirty = false;
        }

        PollBindCapture();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // 吞异常自愈: 仅当面板操作异常时丢弃重建
    }
}

// ============================================================
// 后台输入线程 (方案A: 仅监听呼出键; 断肢全走鼠标点击)
// ============================================================
static DWORD WINAPI InputThread(LPVOID)
{
    LogMsg("[Input] Background thread running (toggle key only).");
    while (g_threadRunning) {
        Sleep(40);
        if (!IsGameWindowFocused()) {
            for (int i = 0; i < BIND_COUNT; ++i) g_keyPrev[i] = false;
            continue;
        }

        for (int i = 0; i < BIND_COUNT; ++i) {
            bool modOk = true;
            if (g_binds[i].ctrl)  modOk = modOk && ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            if (g_binds[i].shift) modOk = modOk && ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
            if (g_binds[i].alt)   modOk = modOk && ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
            bool keyDown = modOk && ((GetAsyncKeyState(g_binds[i].vk) & 0x8000) != 0);
            if (keyDown && !g_keyPrev[i]) {
                // 0->1 沿触发: 呼出/隐藏面板
                InterlockedExchange(&g_pendingTogglePanel, 1);
            }
            g_keyPrev[i] = keyDown;
        }
    }
    return 0;
}

// ============================================================
// 初始化
// ============================================================
static void InitPluginCore()
{
    LogMsg("==================================================");
    LogMsg("[SkeletonLimbHotSwap] v2.6.1 (multi-subpanel UI + rebindable keys) initializing...");
    LogMsg("==================================================");

    LoadBinds();
    for (int i = 0; i < BIND_COUNT; ++i)
        LogMsg(std::string("  bind ") + g_binds[i].iniKey + " = " + BindToStr(i));

    // 引擎直调函数 (符号与 KenshiLib.dll 导出表对齐)
    getSelected_fn     = (GetSelectedFn)ResolveKenshiExport("?getSelectedObject@ForgottenGUI@@QEBAAEBVhand@@XZ");
    handGetChar_fn     = (HandGetCharFn)ResolveKenshiExport("?getCharacter@hand@@QEBAPEAVCharacter@@XZ");
    getMedical_fn      = (GetMedicalFn)ResolveKenshiExport("?getMedical@Character@@QEAAPEAVMedicalSystem@@XZ");
    amputate_fn        = (AmputateFn)ResolveKenshiExport("?amputate@MedicalSystem@@QEAAXW4Limb@RobotLimbs@@_NAEBVVector3@Ogre@@@Z");
    healCompletely_fn  = (HealCompletelyFn)ResolveKenshiExport("?healCompletely@Character@@QEAAXXZ");
    isAnimal_fn        = (IsAnimalFn)ResolveKenshiExport("?isAnimal@Character@@UEAAPEAVCharacterAnimal@@XZ");
    g_pGuiVar          = (void**)GetKenshiLibExport("?gui@@3PEAVForgottenGUI@@EA");

    if (!getSelected_fn) LogMsg("[Init] FAILED: getSelectedObject");
    if (!handGetChar_fn) LogMsg("[Init] FAILED: getCharacter@hand");
    if (!getMedical_fn)  LogMsg("[Init] FAILED: getMedical");
    if (!amputate_fn)    LogMsg("[Init] FAILED: amputate");
    if (!healCompletely_fn) LogMsg("[Init] FAILED: healCompletely");
    if (!isAnimal_fn)    LogMsg("[Init] FAILED: isAnimal");
    if (!g_pGuiVar)      LogMsg("[Init] FAILED: gui global");

    // 面板 UI 函数
    createDatapanel_fn = (CreateDatapanelFn)GetKenshiLibExport(
        "?createDatapanel@ForgottenGUI@@QEAAPEAVDatapanelGUI@@MMMM_NV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z");
    setCaption_fn = (SetCaptionFn)GetKenshiLibExport(
        "?setCaption@DatapanelGUI@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
    setLineStatInfo_fn = (SetLineFn)GetKenshiLibExport(
        "?setLineStatInfo@DatapanelGUI@@QEAAPEAVDataPanelLine@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0H@Z");
    show_fn = (ShowFn)GetKenshiLibExport("?show@DatapanelGUI@@UEAAX_N@Z");
    setPositionReal_fn = (SetPosRealFn)GetKenshiLibExport("?setPositionReal@DatapanelGUI@@UEAAXMM@Z");
    resize_fn = (ResizeFn)GetKenshiLibExport("?resize@DatapanelGUI@@UEAAXHH@Z");

    if (!createDatapanel_fn || !setCaption_fn)
        LogMsg("[Init] WARN: panel UI exports missing - UI disabled, hotkeys only.");

    // MainBarGUI::update hook (游戏内 HUD 每帧; 不与 Divine 的 ForgottenGUI::update 冲突)
    if (!HookEngineExport("?update@MainBarGUI@@UEAAXXZ", (void*)&mainBarUpdate_hook, (void**)&mainBarUpdate_orig))
        LogMsg("[Init] FAILED: MainBarGUI::update hook (UI 不可用, 热键仍工作)");
    else
        LogMsg("[Init] MainBarGUI::update hooked.");

    g_hThread = CreateThread(NULL, 0, InputThread, NULL, 0, NULL);
    LogMsg("[Init] Background thread active. Panel toggle: " + BindToStr(BIND_TOGGLE));
}

__declspec(dllexport) void startPlugin()
{
    // DPI 感知: 鼠标物理/逻辑坐标一致化 (点击命中必需; 系统已高 DPI 时返回 FALSE 无妨)
    SetProcessDPIAware();
    InitPluginCore();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        g_threadRunning = false;
        if (g_hThread) {
            WaitForSingleObject(g_hThread, 500);
            CloseHandle(g_hThread);
        }
    }
    return TRUE;
}
