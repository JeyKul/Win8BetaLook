// ==WindhawkMod==
// @id              themed-taskbar-background
// @name            Themed taskbar background
// @description     Paints the Windows 10 taskbar (ExplorerPatcher) background from the active visual style's TaskBar class, like Windows 7/8 did
// @version         0.9
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
instead of drawing it from the visual style. This mod switches that fill off for
the taskbar windows and draws the style's TaskBar background part instead
(part 1-4 for a taskbar at the bottom, right, top or left), the way Windows 7
and 8 did. Made for the Windows 8 beta style of Win8BetaLook, works with any
style that has TaskBar background images.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- glass: true
  $name: Glass behind the taskbar
  $description: Extend the DWM frame into the taskbar, so a glass DWM (OpenGlass) shows through the image's transparent parts
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vssym32.h>

#include <vector>

// Undocumented, used by the taskbar for its acrylic/colour fill
enum ACCENT_STATE { ACCENT_DISABLED = 0 };
struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};
struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attrib;  // WCA_ACCENT_POLICY = 19
    PVOID pvData;
    SIZE_T cbData;
};
using SetWindowCompositionAttribute_t = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
static SetWindowCompositionAttribute_t pSetWindowCompositionAttribute;
static SetWindowCompositionAttribute_t SetWindowCompositionAttribute_Original;

static bool g_glass = true;
static std::vector<HWND> g_subclassed;

static bool IsTaskbar(HWND hwnd) {
    WCHAR cls[64];
    if (!GetClassNameW(hwnd, cls, ARRAYSIZE(cls)))
        return false;
    return !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd");
}

static void DisableAccent(HWND hwnd) {
    ACCENT_POLICY policy = {ACCENT_DISABLED, 0, 0, 0};
    WINDOWCOMPOSITIONATTRIBDATA data = {19, &policy, sizeof(policy)};
    if (SetWindowCompositionAttribute_Original)
        SetWindowCompositionAttribute_Original(hwnd, &data);
    if (g_glass) {
        MARGINS m = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &m);
    }
}

// The taskbar keeps setting its accent (colour/acrylic): refuse that for taskbar windows
static BOOL WINAPI SetWindowCompositionAttribute_Hook(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* data) {
    if (data && data->Attrib == 19 && IsTaskbar(hwnd)) {
        ACCENT_POLICY policy = {ACCENT_DISABLED, 0, 0, 0};
        WINDOWCOMPOSITIONATTRIBDATA mine = {19, &policy, sizeof(policy)};
        return SetWindowCompositionAttribute_Original(hwnd, &mine);
    }
    return SetWindowCompositionAttribute_Original(hwnd, data);
}

static int BackgroundPart(HWND hwnd) {
    RECT r, mon;
    GetWindowRect(hwnd, &r);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    mon = mi.rcMonitor;
    bool horizontal = (r.right - r.left) > (r.bottom - r.top);
    if (horizontal)
        return (r.top <= mon.top) ? 3 /* TOP */ : 1 /* BOTTOM */;
    return (r.left <= mon.left) ? 4 /* LEFT */ : 2 /* RIGHT */;
}

static HWND TaskbarAncestor(HWND hwnd) {
    for (HWND w = hwnd; w; w = GetParent(w))
        if (IsTaskbar(w))
            return w;
    return nullptr;
}

static void PaintBackground(HWND hwnd, HDC hdc, bool clear = true) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    // Clear to transparent first: black has alpha 0 on a DWM-extended window
    if (clear)
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    HTHEME theme = OpenThemeData(hwnd, L"TaskBar");
    if (theme) {
        DrawThemeBackground(theme, hdc, BackgroundPart(hwnd), 0, &rc, nullptr);
        CloseThemeData(theme);
    }
}

static LRESULT CALLBACK TaskbarSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, DWORD_PTR) {
    switch (msg) {
        case WM_ERASEBKGND:
            PaintBackground(hwnd, (HDC)wp);
            return 1;
        // Children (task list, tray) ask for their background with
        // DrawThemeParentBackground, which lands here
        case WM_PRINTCLIENT:
            PaintBackground(hwnd, (HDC)wp);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintBackground(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_THEMECHANGED:
        case WM_DWMCOMPOSITIONCHANGED:
        case WM_WINDOWPOSCHANGED:
            DisableAccent(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// Children that paint their own background instead of asking the taskbar:
// paint the taskbar's background there, shifted to the child's position
static void PaintChildBackground(HWND child, HDC hdc) {
    HWND bar = TaskbarAncestor(child);
    if (!bar)
        return;
    POINT pt = {0, 0};
    MapWindowPoints(bar, child, &pt, 1);
    // Relative: buffered-paint DCs already carry their own origin
    POINT old;
    OffsetViewportOrgEx(hdc, pt.x, pt.y, &old);
    PaintBackground(bar, hdc, false);
    SetViewportOrgEx(hdc, old.x, old.y, nullptr);
}

static LRESULT CALLBACK ChildSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, DWORD_PTR) {
    switch (msg) {
        case WM_ERASEBKGND:
            PaintChildBackground(hwnd, (HDC)wp);
            return 1;
        case WM_PRINTCLIENT:
            PaintChildBackground(hwnd, (HDC)wp);
            break;  // then let the control draw its content on top
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static std::vector<HWND> g_children;

// Short text log for debugging without the Windhawk UI: C:\w8\tb.log, first 300 lines
static void FileLog(const wchar_t* fmt, ...) {
    static int lines = 0;
    if (lines++ > 300)
        return;
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    char out[1024];
    int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, sizeof(out) - 2, nullptr, nullptr);
    if (n <= 0)
        return;
    out[n - 1] = '\n';
    HANDLE f = CreateFileW(L"C:\\w8\\tb.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w;
        WriteFile(f, out, n, &w, nullptr);
        CloseHandle(f);
    }
}

// The taskbar's controls ask for their background with DrawThemeParentBackground(Ex),
// which on the Windows 10 taskbar gives black (transparent): draw the style's
// TaskBar background there instead (the approach of classic-taskbar-background-fix)
typedef HRESULT(WINAPI* DrawThemeParentBackground_t)(HWND, HDC, const RECT*);
typedef HRESULT(WINAPI* DrawThemeParentBackgroundEx_t)(HWND, HDC, DWORD, const RECT*);
static DrawThemeParentBackground_t DrawThemeParentBackground_Original;
static DrawThemeParentBackgroundEx_t DrawThemeParentBackgroundEx_Original;

static bool PaintForChild(HWND hwnd, HDC hdc, const RECT* clip) {
    if (!hwnd || IsTaskbar(hwnd) || !TaskbarAncestor(hwnd))
        return false;
    int saved = SaveDC(hdc);
    if (clip)
        IntersectClipRect(hdc, clip->left, clip->top, clip->right, clip->bottom);
    PaintChildBackground(hwnd, hdc);
    RestoreDC(hdc, saved);
    static int logged = 0;
    if (logged++ < 40) {
        WCHAR cls[64] = L"";
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
        FileLog(L"parentbg %s", cls);
    }
    return true;
}

static HRESULT WINAPI DrawThemeParentBackground_Hook(HWND hwnd, HDC hdc, const RECT* rc) {
    return PaintForChild(hwnd, hdc, rc) ? S_OK : DrawThemeParentBackground_Original(hwnd, hdc, rc);
}

static HRESULT WINAPI DrawThemeParentBackgroundEx_Hook(HWND hwnd, HDC hdc, DWORD flags, const RECT* rc) {
    return PaintForChild(hwnd, hdc, rc) ? S_OK : DrawThemeParentBackgroundEx_Original(hwnd, hdc, flags, rc);
}

static bool WantsChildBackground(HWND hwnd) {
    WCHAR cls[64];
    if (!GetClassNameW(hwnd, cls, ARRAYSIZE(cls)))
        return false;
    return !wcscmp(cls, L"ReBarWindow32") || !wcscmp(cls, L"MSTaskSwWClass") ||
           !wcscmp(cls, L"MSTaskListWClass");
}

static void AttachChild(HWND hwnd) {
    if (WantsChildBackground(hwnd) && TaskbarAncestor(hwnd) &&
        WindhawkUtils::SetWindowSubclassFromAnyThread(hwnd, ChildSubclassProc, 0)) {
        g_children.push_back(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        Wh_Log(L"child %p", hwnd);
    }
}

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM) {
    AttachChild(hwnd);
    return TRUE;
}

static void Attach(HWND hwnd) {
    EnumChildWindows(hwnd, EnumChildProc, 0);
    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hwnd, TaskbarSubclassProc, 0)) {
        g_subclassed.push_back(hwnd);
        DisableAccent(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        Wh_Log(L"attached to %p", hwnd);
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
static HWND WINAPI CreateWindowExW_Hook(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y,
                                        int w, int h, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    HWND hwnd = CreateWindowExW_Original(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    if (hwnd && IsTaskbar(hwnd))
        Attach(hwnd);
    else if (hwnd && parent)
        AttachChild(hwnd);
    return hwnd;
}

static void LoadSettings() {
    g_glass = Wh_GetIntSetting(L"glass");
}

BOOL Wh_ModInit() {
    LoadSettings();
    pSetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
    if (pSetWindowCompositionAttribute)
        Wh_SetFunctionHook((void*)pSetWindowCompositionAttribute, (void*)SetWindowCompositionAttribute_Hook,
                           (void**)&SetWindowCompositionAttribute_Original);
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&CreateWindowExW_Original);
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    Wh_SetFunctionHook((void*)GetProcAddress(ux, "DrawThemeParentBackground"), (void*)DrawThemeParentBackground_Hook,
                       (void**)&DrawThemeParentBackground_Original);
    Wh_SetFunctionHook((void*)GetProcAddress(ux, "DrawThemeParentBackgroundEx"),
                       (void*)DrawThemeParentBackgroundEx_Hook, (void**)&DrawThemeParentBackgroundEx_Original);
    return TRUE;
}

void Wh_ModAfterInit() {
    EnumWindows(EnumProc, 0);
}

void Wh_ModUninit() {
    for (HWND hwnd : g_children)
        if (IsWindow(hwnd))
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hwnd, ChildSubclassProc);
    for (HWND hwnd : g_subclassed) {
        if (IsWindow(hwnd)) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hwnd, TaskbarSubclassProc);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
    }
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    for (HWND hwnd : g_subclassed)
        if (IsWindow(hwnd)) {
            DisableAccent(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
}
