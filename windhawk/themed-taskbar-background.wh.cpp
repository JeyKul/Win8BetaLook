// ==WindhawkMod==
// @id              themed-taskbar-background
// @name            Themed taskbar background
// @description     Shows the active visual style's TaskBar background behind the Windows 10 taskbar (ExplorerPatcher), like Windows 7/8 did
// @version         0.11
// @author          JeyKul
// @github          https://github.com/JeyKul
// @homepage        https://github.com/JeyKul/Win8BetaLook
// @license         GPL-3.0
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luxtheme -ldwmapi -lgdi32 -lcomctl32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Themed taskbar background

The Windows 10 taskbar fills its background itself (solid colour or acrylic)
instead of drawing it from the visual style. This mod switches that fill off,
which leaves the taskbar transparent, and puts a window directly behind it that
shows the style's TaskBar background part (1-4 for a taskbar at the bottom,
right, top or left), the way Windows 7 and 8 drew it. The image's own alpha is
kept, so a glass DWM (OpenGlass) shows through it.

Made for the Windows 8 beta style of Win8BetaLook; works with any style that has
TaskBar background images.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <vector>

// Undocumented: the taskbar sets its colour/acrylic fill with this
struct ACCENT_POLICY {
    int AccentState;  // 0 = disabled
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};
struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attrib;  // 19 = WCA_ACCENT_POLICY
    PVOID pvData;
    SIZE_T cbData;
};
using SetWindowCompositionAttribute_t = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
static SetWindowCompositionAttribute_t SetWindowCompositionAttribute_Original;

static const WCHAR kBackClass[] = L"Win8BetaLook_TaskbarBack";
static const UINT kMsgUpdate = WM_APP + 1;
static HANDLE g_thread;
static DWORD g_threadId;
static HWND g_msgWnd;

struct Bar {
    HWND taskbar;
    HWND back;
};
static std::vector<Bar> g_bars;  // touched only on the mod's thread
static std::vector<HWND> g_subclassed;

static bool IsTaskbar(HWND hwnd) {
    WCHAR cls[64];
    if (!GetClassNameW(hwnd, cls, ARRAYSIZE(cls)))
        return false;
    return !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd");
}

static void MakeTransparent(HWND hwnd) {
    ACCENT_POLICY policy = {0, 0, 0, 0};
    WINDOWCOMPOSITIONATTRIBDATA data = {19, &policy, sizeof(policy)};
    if (SetWindowCompositionAttribute_Original)
        SetWindowCompositionAttribute_Original(hwnd, &data);
    // Whatever the taskbar paints black becomes transparent
    MARGINS m = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &m);
}

// The taskbar keeps setting its accent: keep it disabled for taskbar windows
static BOOL WINAPI SetWindowCompositionAttribute_Hook(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* data) {
    if (data && data->Attrib == 19 && IsTaskbar(hwnd)) {
        ACCENT_POLICY policy = {0, 0, 0, 0};
        WINDOWCOMPOSITIONATTRIBDATA mine = {19, &policy, sizeof(policy)};
        return SetWindowCompositionAttribute_Original(hwnd, &mine);
    }
    return SetWindowCompositionAttribute_Original(hwnd, data);
}

static int BackgroundPart(const RECT& r, HWND hwnd) {
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    if (r.right - r.left > r.bottom - r.top)
        return r.top <= mi.rcMonitor.top ? 3 /* top */ : 1 /* bottom */;
    return r.left <= mi.rcMonitor.left ? 4 /* left */ : 2 /* right */;
}

// Render the TaskBar part into a 32-bit buffer and show it with per-pixel alpha
static void RenderBack(Bar& bar) {
    RECT r;
    if (!GetWindowRect(bar.taskbar, &r) || !IsWindowVisible(bar.taskbar)) {
        ShowWindow(bar.back, SW_HIDE);
        return;
    }
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0)
        return;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi = {};
    bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    void* bits;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, bmp);
    ZeroMemory(bits, (size_t)w * h * 4);
    HTHEME theme = OpenThemeData(bar.taskbar, L"TaskBar");
    if (theme) {
        RECT rc = {0, 0, w, h};
        DrawThemeBackground(theme, mem, BackgroundPart(r, bar.taskbar), 0, &rc, nullptr);
        CloseThemeData(theme);
    }
    POINT src = {0, 0}, dst = {r.left, r.top};
    SIZE size = {w, h};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(bar.back, screen, &dst, &size, mem, &src, 0, &blend, ULW_ALPHA);
    // Directly below the taskbar in the z-order
    SetWindowPos(bar.back, bar.taskbar, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
}

static void UpdateAll() {
    // Drop bars whose taskbar is gone
    for (size_t i = 0; i < g_bars.size();) {
        if (!IsWindow(g_bars[i].taskbar)) {
            DestroyWindow(g_bars[i].back);
            g_bars.erase(g_bars.begin() + i);
        } else {
            RenderBack(g_bars[i++]);
        }
    }
}

static void AddBar(HWND taskbar) {
    for (auto& b : g_bars)
        if (b.taskbar == taskbar)
            return;
    HWND back = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                kBackClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
    if (!back)
        return;
    // Glass behind the image (blurred by a glass DWM such as OpenGlass)
    DWM_BLURBEHIND bb = {DWM_BB_ENABLE, TRUE, nullptr, FALSE};
    DwmEnableBlurBehindWindow(back, &bb);
    g_bars.push_back({taskbar, back});
    RenderBack(g_bars.back());
    Wh_Log(L"back window %p for taskbar %p", back, taskbar);
}

static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == kMsgUpdate) {
        if (wp)
            AddBar((HWND)wp);
        UpdateAll();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RequestUpdate(HWND taskbar = nullptr) {
    if (g_msgWnd)
        PostMessageW(g_msgWnd, kMsgUpdate, (WPARAM)taskbar, 0);
}

static LRESULT CALLBACK TaskbarSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, DWORD_PTR) {
    switch (msg) {
        case WM_WINDOWPOSCHANGED:
        case WM_THEMECHANGED:
        case WM_DWMCOMPOSITIONCHANGED:
        case WM_SETTINGCHANGE:
        case WM_DISPLAYCHANGE:
            MakeTransparent(hwnd);
            RequestUpdate();
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void Attach(HWND hwnd) {
    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hwnd, TaskbarSubclassProc, 0)) {
        g_subclassed.push_back(hwnd);
        MakeTransparent(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        RequestUpdate(hwnd);
    }
}

static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsTaskbar(hwnd))
        Attach(hwnd);
    return TRUE;
}

using CreateWindowExW_t = decltype(&CreateWindowExW);
static CreateWindowExW_t CreateWindowExW_Original;
static HWND WINAPI CreateWindowExW_Hook(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y, int w,
                                        int h, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    HWND hwnd = CreateWindowExW_Original(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    if (hwnd && IsTaskbar(hwnd))
        Attach(hwnd);
    return hwnd;
}

// The back windows live on their own thread with a message loop
static DWORD WINAPI ThreadProc(LPVOID ready) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kBackClass;
    RegisterClassW(&wc);
    WNDCLASSW mc = {};
    mc.lpfnWndProc = MsgWndProc;
    mc.hInstance = GetModuleHandleW(nullptr);
    mc.lpszClassName = L"Win8BetaLook_TaskbarBackMsg";
    RegisterClassW(&mc);
    g_msgWnd = CreateWindowExW(0, mc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    SetEvent((HANDLE)ready);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    for (auto& b : g_bars)
        DestroyWindow(b.back);
    g_bars.clear();
    DestroyWindow(g_msgWnd);
    g_msgWnd = nullptr;
    UnregisterClassW(kBackClass, GetModuleHandleW(nullptr));
    UnregisterClassW(mc.lpszClassName, GetModuleHandleW(nullptr));
    return 0;
}

BOOL Wh_ModInit() {
    auto p = (SetWindowCompositionAttribute_t)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                             "SetWindowCompositionAttribute");
    if (p)
        Wh_SetFunctionHook((void*)p, (void*)SetWindowCompositionAttribute_Hook,
                           (void**)&SetWindowCompositionAttribute_Original);
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&CreateWindowExW_Original);
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, ThreadProc, ready, 0, &g_threadId);
    WaitForSingleObject(ready, 5000);
    CloseHandle(ready);
    return TRUE;
}

void Wh_ModAfterInit() {
    EnumWindows(EnumProc, 0);
}

void Wh_ModUninit() {
    for (HWND hwnd : g_subclassed)
        if (IsWindow(hwnd))
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hwnd, TaskbarSubclassProc);
    if (g_thread) {
        PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_thread, 5000);
        CloseHandle(g_thread);
    }
}
