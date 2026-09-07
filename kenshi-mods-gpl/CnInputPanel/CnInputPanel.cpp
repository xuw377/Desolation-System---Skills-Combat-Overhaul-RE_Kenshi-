// ============================================================================
// 本文件是 中文输入面板 (Cn Input Panel) 的一部分。
// Copyright (C) 2026 kenshi-mod-dev
//
// 本程序是自由软件：你可以根据自由软件基金会发布的 GNU 通用公共许可证
// (第 3 版或更高版本) 再分发和/或修改它。
// 本程序的发布希望它有用，但【不提供任何保证】。
// 详见 GNU 通用公共许可证 (LICENSE 文件)。
//
// This file is part of 中文输入面板 (Cn Input Panel).
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

// ============================================================
// CnInputPanel — Kenshi 游戏内中文输入面板 (独立 mod)
// 设计: docs/19 中文输入技术侦察报告 (用户方案)
//   点击字母卡拼拼音 → 点候选字组句 → 点[复制]写 Windows 剪贴板
//   → 玩家点目标文本框 Ctrl+V。零按键注入/零焦点操作/零引擎文本读取。
// 依赖: 仅 KenshiLib.dll 官方导出 (不依赖 DesolationSystem.dll)
// 热键: F7 显隐; 拼音表: 本 mod 目录 CnInputPinyin.txt (UTF-8, 可自行扩充)
// ============================================================
#include <Debug.h>
#include <Defines.h>
#include <core/Functions.h>
#include <kenshi/Enums.h>
#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>

#ifndef DESO_GIT_HASH
#define DESO_GIT_HASH unknown
#endif
#define CN_STR_IMPL(x) #x
#define CN_STR(x) CN_STR_IMPL(x)

// ============================================================
// 引擎接口 (KenshiLib 导出桩)
// ============================================================
typedef void*  (*CreateDatapanelFn)(void* fgui, float x, float y, float w, float h, bool visible,
                                    const std::string& layer, const std::string& skin);
typedef void   (*SetCaptionFn)(void* datapanel, const std::string& text);
typedef void   (*SetPosRealFn)(void* datapanel, float x, float y);
typedef void   (*ResizeFn)(void* datapanel, int w, int h);
typedef void   (*ShowFn)(void* datapanel, bool on);
typedef void   (*UpdateFn)(void* datapanel);

static CreateDatapanelFn createDatapanel_fn = 0;
static SetCaptionFn      setCaption_fn      = 0;
static SetPosRealFn      setPositionReal_fn = 0;
static ResizeFn          resize_fn          = 0;
static ShowFn            show_fn            = 0;
static void*             g_guiVar           = 0;   // ForgottenGUI* 全局变量所在地址

static void* GetKenshiLibExport(const char* name) {
    HMODULE h = GetModuleHandleA("KenshiLib.dll");
    return h ? (void*)GetProcAddress(h, name) : nullptr;
}

// ============================================================
// 几何 (1920x1080 基准比例制: 任意分辨率占屏比例恒定)
// ============================================================
static float SXn(int base) { return base / 1920.0f; }   // 横向归一化
static float SYn(int base) { return base / 1080.0f; }   // 纵向归一化
static const float PANEL_X = 0.015f, PANEL_Y = 0.10f;
static const int   PANEL_W = 390, PANEL_H = 480;

static void*  g_panel   = 0;
static bool   g_visible = false;    // F7 显隐 (默认隐藏)
static bool   g_created = false;
static bool   g_failed  = false;
static HWND   g_gameHwnd = 0;

static void SafeCaption(void* panel, const std::string& text) {
    if (panel && setCaption_fn) {
        __try { setCaption_fn(panel, text); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
static void SafeShow(void* panel, bool on) {
    if (panel && show_fn) {
        __try { show_fn(panel, on); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
static void SafeResize(void* panel, int w, int h) {
    if (panel && resize_fn) {
        __try { resize_fn(panel, w, h); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}
static void SafePos(void* panel, float x, float y) {
    if (panel && setPositionReal_fn) {
        __try { setPositionReal_fn(panel, x, y); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

// ============================================================
// 中文输入状态与拼音表
// ============================================================
static std::wstring cn_pinyin, cn_output;
static std::wstring cn_cands[7];
static int   cn_candCount = 0, cn_matchedLen = 0;
static bool  cn_tableLoaded = false;
struct CnEntry { std::string py; std::wstring chars; };
static std::vector<CnEntry> cn_table;

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::string GetOurDllDir() {
    HMODULE h = 0;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&GetOurDllDir, &h);
    char path[MAX_PATH] = { 0 };
    if (h && GetModuleFileNameA(h, path, MAX_PATH)) {
        char* s = strrchr(path, '\\');
        if (s) *s = 0;
        return std::string(path);
    }
    return ".";
}
static void CnLoadTable() {
    if (cn_tableLoaded) return;
    cn_tableLoaded = true;
    std::string path = GetOurDllDir() + "\\CnInputPinyin.txt";
    FILE* fp = nullptr;
    fopen_s(&fp, path.c_str(), "rb");
    if (!fp) { ErrorLog("CnInputPanel: CnInputPinyin.txt missing at " + path); return; }
    std::string all;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) all.append(buf, n);
    fclose(fp);
    size_t pos = 0;
    while (pos < all.size()) {
        size_t eol = all.find('\n', pos);
        if (eol == std::string::npos) eol = all.size();
        std::string line = all.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        cn_table.push_back({ line.substr(0, eq), Utf8ToWide(line.substr(eq + 1)) });
    }
    ErrorLog("CnInputPanel: pinyin table loaded, " + std::to_string(cn_table.size()) + " entries");
}
static void CnUpdateCandidates() {
    cn_candCount = 0; cn_matchedLen = 0;
    CnLoadTable();
    int len = (int)cn_pinyin.size();
    for (int k = (len > 6 ? 6 : len); k >= 1 && cn_candCount == 0; --k) {
        std::string suf = WideToUtf8(cn_pinyin.substr(cn_pinyin.size() - k));
        for (auto& e : cn_table) {
            if (e.py == suf) {
                cn_matchedLen = k;
                for (int i = 0; i < (int)e.chars.size() && cn_candCount < 7; ++i)
                    cn_cands[cn_candCount++] = e.chars.substr(i, 1);
                break;
            }
        }
    }
}
static bool WinSetClipboardText(const std::wstring& s) {
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t));
    if (!g) { CloseClipboard(); return false; }
    memcpy(GlobalLock(g), s.c_str(), (s.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(g);
    HANDLE ok = SetClipboardData(CF_UNICODETEXT, g);
    CloseClipboard();
    return ok != nullptr;
}

// ============================================================
// 卡片: type 0=输出(显示) 1=复制 2=退格 3=清空 10..16=候选 100..125=字母a-z
// ============================================================
struct CnCard { void* handle; float x, y, w, h; int type; };
static CnCard g_cards[40];
static int    g_cardCount = 0;

static void DropAll() {
    for (int i = 0; i < g_cardCount; ++i) g_cards[i].handle = 0;
    g_panel = 0;
    g_created = false;
    g_cardCount = 0;
}

static void RefreshOutput() {
    for (int i = 0; i < g_cardCount; ++i) {
        if (g_cards[i].type == 0) {
            std::wstring out = cn_output.empty() ? L"(\x7A7A)" : cn_output;
            SafeCaption(g_cards[i].handle, "\xE8\xBE\x93\xE5\x87\xBA\xEF\xBC\x9A" + WideToUtf8(out));   // 输出：
        }
    }
}
static void UpdateCandCaptions() {
    for (int i = 0; i < g_cardCount; ++i) {
        if (g_cards[i].type >= 10 && g_cards[i].type < 17) {
            int ci = g_cards[i].type - 10;
            SafeCaption(g_cards[i].handle, ci < cn_candCount ? WideToUtf8(cn_cands[ci]) : "-");
        }
    }
}
static void CnUIRefresh() {
    CnUpdateCandidates();
    UpdateCandCaptions();
    RefreshOutput();
}

static bool IsGameFocused() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

// ============================================================
// 面板与卡片创建 (懒: 首次 F7 打开时)
// ============================================================
static void EnsureCreated(void* fgui) {
    if (g_created || g_failed || !createDatapanel_fn || !fgui) return;
    __try {
        g_panel = createDatapanel_fn(fgui, PANEL_X, PANEL_Y, SXn(PANEL_W), SYn(PANEL_H), g_visible,
                                     std::string("Middle"), std::string("Kenshi_FloatingPanelThinSkin"));
        if (!g_panel) { g_failed = true; return; }
        SafeCaption(g_panel, "\xE4\xB8\xAD\xE6\x96\x87\xE8\xBE\x93\xE5\x85\xA5 [F7]");   // 中文输入 [F7]

        g_cardCount = 0;
        auto add = [&](float bx, float by, int w, int h, int type, const std::string& cap) {
            if (g_cardCount >= 40) return;
            CnCard& c = g_cards[g_cardCount];
            c.x = PANEL_X + SXn(bx); c.y = PANEL_Y + SYn(by);
            c.w = SXn(w);            c.h = SYn(h);
            c.type = type;
            c.handle = createDatapanel_fn(fgui, c.x, c.y, c.w, c.h, g_visible,
                                          std::string("Middle"), std::string("Kenshi_FloatingPanelThinSkin"));
            if (!c.handle) return;
            SafePos(c.handle, c.x, c.y);
            SafeResize(c.handle, w, h);
            SafeCaption(c.handle, cap);
            g_cardCount++;
        };

        add(8, 10,  290, 34, 0,    "\xE8\xBE\x93\xE5\x87\xBA\xEF\xBC\x9A(\xE7\xA9\xBA)");   // 输出：(空)
        add(302, 10, 80, 34, 1000, "\xE6\x8B\xBC\xE9\x9F\xB3");                        // 拼音 (标题卡)
        for (int i = 0; i < 7; ++i)
            add(8 + i * 52, 52, 48, 34, 10 + i, "-");                                  // 候选
        add(8,   94, 116, 34, 1, "\xE5\xA4\x8D\xE5\x88\xB6");                          // 复制
        add(128, 94, 116, 34, 2, "\xE9\x80\x80\xE6\xA0\xBC");                          // 退格
        add(248, 94, 132, 34, 3, "\xE6\xB8\x85\xE7\xA9\xBA");                          // 清空
        static const char* letters = "abcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < 26; ++i) {
            int row = i / 6, col = i % 6;
            char cap[2] = { letters[i], 0 };
            add(8 + col * 62, 136 + row * 38, 58, 34, 100 + i, cap);                   // 字母键盘 6x5
        }
        g_created = true;
        ErrorLog("CnInputPanel: panel + cards created");
        CnUIRefresh();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_failed = true;
        ErrorLog("CnInputPanel: creation raised, backoff");
    }
}

// ============================================================
// 点击与执行
// ============================================================
static int HitTest() {
    if (!g_gameHwnd) return -1;
    POINT pt;
    if (!GetCursorPos(&pt)) return -1;
    RECT rc;
    if (!GetClientRect(g_gameHwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return -1;
    ScreenToClient(g_gameHwnd, &pt);
    float nx = (float)pt.x / (float)rc.right;
    float ny = (float)pt.y / (float)rc.bottom;
    for (int i = 0; i < g_cardCount; ++i) {
        CnCard& c = g_cards[i];
        if (!c.handle || c.type < 0) continue;
        if (nx >= c.x && nx <= c.x + c.w && ny >= c.y && ny <= c.y + c.h) {
            if (nx >= c.x + c.w * 0.75f && c.type != 0 && c.type != 1000) return -1;   // 右侧×带
            return c.type;
        }
    }
    return -1;
}

static void Execute(int type) {
    if (type == 1) {   // 复制
        if (cn_output.empty()) { ErrorLog("CnInputPanel: output empty"); return; }
        if (WinSetClipboardText(cn_output)) {
            ErrorLog("CnInputPanel: copied: " + WideToUtf8(cn_output));
            for (int i = 0; i < g_cardCount; ++i)
                if (g_cards[i].type == 0)
                    SafeCaption(g_cards[i].handle, "\xE5\xB7\xB2\xE5\xA4\x8D\xE5\x88\xB6! Ctrl+V");   // 已复制! Ctrl+V
        } else {
            ErrorLog("CnInputPanel: clipboard open failed");
        }
        return;
    }
    if (type == 2) { if (!cn_pinyin.empty()) cn_pinyin.pop_back(); }
    else if (type == 3) { cn_pinyin.clear(); cn_output.clear(); }
    else if (type >= 100 && type < 126) {
        cn_pinyin += (wchar_t)(L'a' + (type - 100));
    } else if (type >= 10 && type < 17) {
        int ci = type - 10;
        if (ci < cn_candCount) {
            cn_output += cn_cands[ci];
            if (cn_matchedLen > 0 && (int)cn_pinyin.size() >= cn_matchedLen)
                cn_pinyin.erase(cn_pinyin.size() - cn_matchedLen);
        }
    } else return;
    CnUIRefresh();
}

// ============================================================
// Hook: DatapanelGUI::update (每帧每面板调用; 不与其他插件 hook 目标冲突)
// ============================================================
typedef void (*DpUpdateFn)(void*);
static DpUpdateFn dpUpdate_orig = 0;
static bool  g_f7Last = false;
static bool  g_mousePrev = false;
static DWORD g_lastClick = 0;

static void dpUpdate_hook(void* thisptr) {
    if (dpUpdate_orig) dpUpdate_orig(thisptr);

    // 性能门闩 (2026-09-06): DatapanelGUI::update 每帧每面板实例各调用一次,
    // 场景面板几十个 → 本钩子每帧被放大几十倍。未创建面板时只做轻量 F7 检测
    // (每 tick 一次); 面板创建后全部逻辑每 tick 只执行第一次调用。
    static DWORD s_lastTick = 0;
    DWORD tickNow = GetTickCount();
    bool firstThisTick = (tickNow != s_lastTick);
    if (firstThisTick) s_lastTick = tickNow;
    if (!g_created) {
        if (!firstThisTick) return;
        bool f7 = IsGameFocused() && ((GetAsyncKeyState(VK_F7) & 0x8000) != 0);
        if (f7 && !g_f7Last) g_visible = true;   // 打开动作留给下方统一处理
        g_f7Last = f7;
        if (!g_visible) return;
    } else if (!firstThisTick) {
        return;
    }

    __try {
        // 游戏主窗口定位 (进程内)
        if (!g_gameHwnd || !IsWindow(g_gameHwnd)) {
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
            g_gameHwnd = ctx.found;
        }

        // F7 显隐 (未创建时上面的门闩分支已检测过 F7)
        bool f7 = IsGameFocused() && ((GetAsyncKeyState(VK_F7) & 0x8000) != 0);
        if (f7 && !g_f7Last) {
            g_visible = !g_visible;
            if (g_visible && !g_created) {
                void** guiVar = (void**)g_guiVar;
                void* fgui = guiVar ? *guiVar : 0;
                EnsureCreated(fgui);
            }
            SafeShow(g_panel, g_visible);
            for (int i = 0; i < g_cardCount; ++i) SafeShow(g_cards[i].handle, g_visible);
            ErrorLog(std::string("CnInputPanel: F7 -> ") + (g_visible ? "SHOW" : "HIDE"));
        }
        g_f7Last = f7;
        if (!g_visible || !g_created) return;

        // 每帧锁位 (防拖动) — 引擎调用异常 = 面板被引擎销毁, 丢弃引用待重建
        bool geometryError = false;
        SafePos(g_panel, PANEL_X, PANEL_Y);
        for (int i = 0; i < g_cardCount; ++i) {
            if (g_cards[i].handle) SafePos(g_cards[i].handle, g_cards[i].x, g_cards[i].y);
        }
        (void)geometryError;

        // 点击处理
        bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool clicked = down && !g_mousePrev;
        g_mousePrev = down;
        if (clicked) {
            DWORD now = GetTickCount();
            if (now - g_lastClick >= 120) {
                g_lastClick = now;
                int t = HitTest();
                if (t >= 0) {
                    ErrorLog("CnInputPanel: click type=" + std::to_string(t));
                    Execute(t);
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DropAll();
        ErrorLog("CnInputPanel: update raised, refs dropped (will rebuild on F7)");
    }
}

// ============================================================
// 初始化
// ============================================================
static void* GetGameGlobal(const char* name) {
    HMODULE h = GetModuleHandleA("KenshiLib.dll");
    return h ? (void*)GetProcAddress(h, name) : nullptr;
}
static bool HookExport(const char* exportName, void* detour, void** original) {
    void* stub = GetKenshiLibExport(exportName);
    if (!stub) { ErrorLog("CnInputPanel: export not found: " + std::string(exportName)); return false; }
    intptr_t realAddr = KenshiLib::GetRealAddress(stub);
    if (realAddr == 0) { ErrorLog("CnInputPanel: failed to resolve " + std::string(exportName)); return false; }
    return (KenshiLib::SUCCESS == KenshiLib::AddHook((void*)realAddr, detour, original));
}

__declspec(dllexport) void startPlugin()
{
    SetProcessDPIAware();
    ErrorLog("CnInputPanel: initializing in-game Chinese input panel...");

    createDatapanel_fn = (CreateDatapanelFn)GetKenshiLibExport(
        "?createDatapanel@ForgottenGUI@@QEAAPEAVDatapanelGUI@@MMMM_NV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z");
    setCaption_fn = (SetCaptionFn)GetKenshiLibExport(
        "?setCaption@DatapanelGUI@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z");
    setPositionReal_fn = (SetPosRealFn)GetKenshiLibExport(
        "?setPositionReal@DatapanelGUI@@UEAAXMM@Z");
    resize_fn = (ResizeFn)GetKenshiLibExport(
        "?resize@DatapanelGUI@@UEAAXHH@Z");
    show_fn = (ShowFn)GetKenshiLibExport(
        "?show@DatapanelGUI@@UEAAX_N@Z");
    g_guiVar = GetGameGlobal("?gui@@3PEAVForgottenGUI@@EA");

    if (!createDatapanel_fn || !setCaption_fn || !setPositionReal_fn || !resize_fn || !show_fn) {
        ErrorLog("CnInputPanel: critical exports missing, aborting.");
        return;
    }

    // Hook DatapanelGUI::update (与 DivineGraceUI 的 ForgottenGUI::update 不冲突)
    if (!HookExport("?update@DatapanelGUI@@UEAAXXZ", (void*)&dpUpdate_hook, (void**)&dpUpdate_orig)) {
        ErrorLog("CnInputPanel: hook failed, aborting.");
        return;
    }
    ErrorLog(std::string("CnInputPanel: initialized, version ") + CN_STR(DESO_GIT_HASH) + " (F7 toggle)");
}

__declspec(dllexport) void stopPlugin()
{
    DropAll();
}
