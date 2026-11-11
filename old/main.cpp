
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <algorithm>

// ============================================================
// FPSCrosshair - Pure External Crosshair
// C++17 + Win32 API + GDI
// ============================================================

namespace Config {

    // Crosshair color (RGB)
    constexpr COLORREF COLOR = RGB(0, 255, 180);

    // Background color key: must not equal the crosshair color
    constexpr COLORREF TRANSPARENT_COLOR = RGB(1, 2, 3);

    // Crosshair geometry
    constexpr int LINE_WIDTH = 2;
    constexpr int ARM_LENGTH = 10;
    constexpr int CENTER_GAP = 5;
    constexpr int RING_RADIUS = 12;
    constexpr int DOT_RADIUS = 2;

    // Window opacity: 0 - 255
    constexpr BYTE OPACITY = 255;

    // Hotkeys
    constexpr int HOTKEY_TOGGLE = 1;
    constexpr int HOTKEY_EXIT = 2;

    constexpr UINT TOGGLE_KEY = VK_F8;
}

HWND g_hwnd = nullptr;
bool g_visible = true;

// ============================================================
// Drawing
// ============================================================

void DrawCrosshair(HDC hdc) {

    // Primary monitor center, relative to virtual desktop
    const int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    const int primaryW = GetSystemMetrics(SM_CXSCREEN);
    const int primaryH = GetSystemMetrics(SM_CYSCREEN);

    const int cx = primaryW / 2 - virtualX;
    const int cy = primaryH / 2 - virtualY;

    // Background
    RECT rc;
    GetClientRect(g_hwnd, &rc);

    HBRUSH bg = CreateSolidBrush(Config::TRANSPARENT_COLOR);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);

    HPEN pen = CreatePen(
        PS_SOLID,
        Config::LINE_WIDTH,
        Config::COLOR
    );

    HGDIOBJ oldPen = SelectObject(hdc, pen);

    // Horizontal crosshair
    MoveToEx(hdc, cx - Config::CENTER_GAP
        - Config::ARM_LENGTH, cy, nullptr);

    LineTo(hdc, cx - Config::CENTER_GAP, cy);

    MoveToEx(hdc, cx + Config::CENTER_GAP, cy, nullptr);

    LineTo(hdc, cx + Config::CENTER_GAP
        + Config::ARM_LENGTH, cy);

    // Vertical crosshair
    MoveToEx(hdc, cx, cy - Config::CENTER_GAP
        - Config::ARM_LENGTH, nullptr);

    LineTo(hdc, cx, cy - Config::CENTER_GAP);

    MoveToEx(hdc, cx, cy + Config::CENTER_GAP, nullptr);

    LineTo(hdc, cx, cy + Config::CENTER_GAP
        + Config::ARM_LENGTH);

    // Center ring
    Ellipse(
        hdc,
        cx - Config::RING_RADIUS,
        cy - Config::RING_RADIUS,
        cx + Config::RING_RADIUS + 1,
        cy + Config::RING_RADIUS + 1
    );

    // Restore pen
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Center dot
    HBRUSH dotBrush = CreateSolidBrush(Config::COLOR);

    HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
    HGDIOBJ oldDotPen = SelectObject(hdc, GetStockObject(NULL_PEN));

    Ellipse(
        hdc,
        cx - Config::DOT_RADIUS,
        cy - Config::DOT_RADIUS,
        cx + Config::DOT_RADIUS + 1,
        cy + Config::DOT_RADIUS + 1
    );

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldDotPen);

    DeleteObject(dotBrush);
}

// ============================================================
// Window procedure
// ============================================================

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {

    switch (msg) {

    case WM_PAINT: {

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        DrawCrosshair(hdc);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DISPLAYCHANGE: {

        const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        SetWindowPos(
            hwnd,
            HWND_TOPMOST,
            x, y, w, h,
            SWP_NOACTIVATE | SWP_SHOWWINDOW
        );

        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================
// Window creation
// ============================================================

bool CreateOverlay(HINSTANCE instance) {

    const wchar_t CLASS_NAME[] = L"FPSCrosshairWindow";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // External transparent overlay window
    const DWORD exStyle =
        WS_EX_LAYERED |
        WS_EX_TRANSPARENT |
        WS_EX_TOOLWINDOW |
        WS_EX_NOACTIVATE;

    g_hwnd = CreateWindowExW(
        exStyle,
        CLASS_NAME,
        L"FPSCrosshair",
        WS_POPUP,
        x, y, w, h,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!g_hwnd) {
        return false;
    }

    // Color-key transparency + global opacity
    if (!SetLayeredWindowAttributes(
        g_hwnd,
        Config::TRANSPARENT_COLOR,
        Config::OPACITY,
        LWA_COLORKEY | LWA_ALPHA
    )) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
        return false;
    }

    SetWindowPos(
        g_hwnd,
        HWND_TOPMOST,
        x, y, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );

    return true;
}

// ============================================================
// Entry point
// ============================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int
) {

    // Register global hotkeys on the message thread
    if (!RegisterHotKey(
        nullptr,
        Config::HOTKEY_TOGGLE,
        MOD_NOREPEAT,
        Config::TOGGLE_KEY
    )) {
        MessageBoxW(
            nullptr,
            L"Cannot register F8 hotkey.",
            L"FPSCrosshair",
            MB_ICONWARNING
        );
        return 1;
    }

    if (!RegisterHotKey(
        nullptr,
        Config::HOTKEY_EXIT,
        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT,
        'Q'
    )) {
        UnregisterHotKey(nullptr, Config::HOTKEY_TOGGLE);
        MessageBoxW(
            nullptr,
            L"Cannot register exit hotkey.",
            L"FPSCrosshair",
            MB_ICONWARNING
        );
        return 1;
    }

    if (!CreateOverlay(hInstance)) {

        UnregisterHotKey(nullptr, Config::HOTKEY_TOGGLE);
        UnregisterHotKey(nullptr, Config::HOTKEY_EXIT);

        MessageBoxW(
            nullptr,
            L"Failed to create overlay window.",
            L"FPSCrosshair",
            MB_ICONERROR
        );

        return 1;
    }

    // Message loop
    MSG msg = {};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {

        if (msg.message == WM_HOTKEY) {

            switch (msg.wParam) {

            case Config::HOTKEY_TOGGLE:

                g_visible = !g_visible;

                if (g_visible) {
                    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);

                    SetWindowPos(
                        g_hwnd,
                        HWND_TOPMOST,
                        0, 0, 0, 0,
                        SWP_NOMOVE |
                        SWP_NOSIZE |
                        SWP_NOACTIVATE
                    );

                    InvalidateRect(g_hwnd, nullptr, TRUE);
                }
                else {
                    ShowWindow(g_hwnd, SW_HIDE);
                }

                break;

            case Config::HOTKEY_EXIT:

                DestroyWindow(g_hwnd);
                g_hwnd = nullptr;
                break;
            }
        }
        else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    UnregisterHotKey(nullptr, Config::HOTKEY_TOGGLE);
    UnregisterHotKey(nullptr, Config::HOTKEY_EXIT);

    return 0;
}
