
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <d2d1.h>
#include <cstdint>
#include <cstddef>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

// ============================================================
// 准星预设
// ============================================================

enum class CrosshairPreset
{
    CS_MINIMAL = 0,
    GREEN_CIRCLE_RED_DOT,
    MINIMAL_GREEN_CROSS,
    COUNT
};

// ============================================================
// 配置参数
// ============================================================

namespace Config
{
    // 通用绿色
    constexpr float GREEN_R = 0.0f;
    constexpr float GREEN_G = 1.0f;
    constexpr float GREEN_B = 0.0f;

    // 红色中心点
    constexpr float RED_R = 1.0f;
    constexpr float RED_G = 0.08f;
    constexpr float RED_B = 0.08f;

    // CS Minimal
    constexpr float CS_ARM_LENGTH = 7.0f;
    constexpr float CS_CENTER_GAP = 4.0f;
    constexpr float CS_LINE_WIDTH = 1.5f;

    // Green Circle + Red Dot
    constexpr float RING_RADIUS = 35.0f;
    constexpr float RING_WIDTH = 1.0f;
    constexpr float DOT_RADIUS = 2.2f;

    // Minimal Green Cross
    constexpr float MINIMAL_CROSS_LENGTH = 9.0f;
    constexpr float MINIMAL_CROSS_WIDTH = 1.5f;
}

// ============================================================
// 全局状态
// ============================================================

static HWND g_hwnd = nullptr;

static ID2D1Factory* g_factory = nullptr;
static ID2D1DCRenderTarget* g_target = nullptr;

static ID2D1SolidColorBrush* g_greenBrush = nullptr;
static ID2D1SolidColorBrush* g_redBrush = nullptr;

static HDC g_memDC = nullptr;
static HBITMAP g_dibBitmap = nullptr;
static HGDIOBJ g_oldBitmap = nullptr;

static void* g_bitmapBits = nullptr;

static int g_virtualX = 0;
static int g_virtualY = 0;
static int g_screenWidth = 0;
static int g_screenHeight = 0;

static bool g_visible = true;

static CrosshairPreset g_currentPreset =
    CrosshairPreset::CS_MINIMAL;

// ============================================================
// 预设名称
// ============================================================

static const wchar_t* GetPresetName()
{
    switch (g_currentPreset)
    {
    case CrosshairPreset::CS_MINIMAL:
        return L"CS Minimal";

    case CrosshairPreset::GREEN_CIRCLE_RED_DOT:
        return L"Green Circle + Red Dot";

    case CrosshairPreset::MINIMAL_GREEN_CROSS:
        return L"Minimal Green Cross";

    default:
        return L"Unknown";
    }
}

// ============================================================
// 资源释放
// ============================================================

static void ReleaseRenderResources()
{
    if (g_greenBrush)
    {
        g_greenBrush->Release();
        g_greenBrush = nullptr;
    }

    if (g_redBrush)
    {
        g_redBrush->Release();
        g_redBrush = nullptr;
    }

    if (g_target)
    {
        g_target->Release();
        g_target = nullptr;
    }

    if (g_memDC)
    {
        if (g_oldBitmap)
        {
            SelectObject(g_memDC, g_oldBitmap);
            g_oldBitmap = nullptr;
        }

        DeleteDC(g_memDC);
        g_memDC = nullptr;
    }

    if (g_dibBitmap)
    {
        DeleteObject(g_dibBitmap);
        g_dibBitmap = nullptr;
    }

    g_bitmapBits = nullptr;
}

// ============================================================
// Direct2D 初始化
// ============================================================

static HRESULT InitializeDirect2D()
{
    return D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        nullptr,
        reinterpret_cast<void**>(&g_factory)
    );
}

// ============================================================
// 创建绘图资源
// ============================================================

static HRESULT CreateRenderResources()
{
    ReleaseRenderResources();

    g_virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    g_screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (g_screenWidth <= 0 || g_screenHeight <= 0)
    {
        return E_FAIL;
    }

    g_memDC = CreateCompatibleDC(nullptr);

    if (!g_memDC)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // 创建 32 位、自顶向下的 DIB
    BITMAPINFO bmi{};

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_screenWidth;
    bmi.bmiHeader.biHeight = -g_screenHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_dibBitmap = CreateDIBSection(
        g_memDC,
        &bmi,
        DIB_RGB_COLORS,
        &g_bitmapBits,
        nullptr,
        0
    );

    if (!g_dibBitmap || !g_bitmapBits)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    g_oldBitmap = SelectObject(g_memDC, g_dibBitmap);

    if (!g_oldBitmap || g_oldBitmap == HGDI_ERROR)
    {
        g_oldBitmap = nullptr;
        return E_FAIL;
    }

    ZeroMemory(
        g_bitmapBits,
        static_cast<size_t>(g_screenWidth) *
        static_cast<size_t>(g_screenHeight) * 4
    );

    // Direct2D 使用预乘 Alpha 的 BGRA 格式
    D2D1_RENDER_TARGET_PROPERTIES properties =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED
            ),
            96.0f,
            96.0f,
            D2D1_RENDER_TARGET_USAGE_NONE,
            D2D1_FEATURE_LEVEL_DEFAULT
        );

    HRESULT hr = g_factory->CreateDCRenderTarget(
        &properties,
        &g_target
    );

    if (FAILED(hr))
    {
        return hr;
    }

    RECT rect{
        0,
        0,
        g_screenWidth,
        g_screenHeight
    };

    hr = g_target->BindDC(g_memDC, &rect);

    if (FAILED(hr))
    {
        return hr;
    }

    g_target->SetAntialiasMode(
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
    );

    // 绿色画刷
    hr = g_target->CreateSolidColorBrush(
        D2D1::ColorF(
            Config::GREEN_R,
            Config::GREEN_G,
            Config::GREEN_B,
            1.0f
        ),
        &g_greenBrush
    );

    if (FAILED(hr))
    {
        return hr;
    }

    // 红色画刷
    hr = g_target->CreateSolidColorBrush(
        D2D1::ColorF(
            Config::RED_R,
            Config::RED_G,
            Config::RED_B,
            1.0f
        ),
        &g_redBrush
    );

    return hr;
}

// ============================================================
// 绘制预设 1：CS Minimal
// ============================================================

static void DrawCSMinimal(float cx, float cy)
{
    const float gap = Config::CS_CENTER_GAP;
    const float arm = Config::CS_ARM_LENGTH;
    const float width = Config::CS_LINE_WIDTH;

    // 左侧
    g_target->DrawLine(
        D2D1::Point2F(cx - gap - arm, cy),
        D2D1::Point2F(cx - gap, cy),
        g_greenBrush,
        width
    );

    // 右侧
    g_target->DrawLine(
        D2D1::Point2F(cx + gap, cy),
        D2D1::Point2F(cx + gap + arm, cy),
        g_greenBrush,
        width
    );

    // 上侧
    g_target->DrawLine(
        D2D1::Point2F(cx, cy - gap - arm),
        D2D1::Point2F(cx, cy - gap),
        g_greenBrush,
        width
    );

    // 下侧
    g_target->DrawLine(
        D2D1::Point2F(cx, cy + gap),
        D2D1::Point2F(cx, cy + gap + arm),
        g_greenBrush,
        width
    );
}

// ============================================================
// 绘制预设 2：绿色圆环 + 红色中心点
// ============================================================

static void DrawGreenCircleRedDot(float cx, float cy)
{
    const D2D1_ELLIPSE ring =
        D2D1::Ellipse(
            D2D1::Point2F(cx, cy),
            Config::RING_RADIUS,
            Config::RING_RADIUS
        );

    g_target->DrawEllipse(
        ring,
        g_greenBrush,
        Config::RING_WIDTH
    );

    const D2D1_ELLIPSE dot =
        D2D1::Ellipse(
            D2D1::Point2F(cx, cy),
            Config::DOT_RADIUS,
            Config::DOT_RADIUS
        );

    g_target->FillEllipse(
        dot,
        g_redBrush
    );
}

// ============================================================
// 绘制预设 3：Minimal Green Cross
// ============================================================

static void DrawMinimalGreenCross(float cx, float cy)
{
    const float length = Config::MINIMAL_CROSS_LENGTH;
    const float width = Config::MINIMAL_CROSS_WIDTH;

    // 水平线
    g_target->DrawLine(
        D2D1::Point2F(cx - length, cy),
        D2D1::Point2F(cx + length, cy),
        g_greenBrush,
        width
    );

    // 垂直线
    g_target->DrawLine(
        D2D1::Point2F(cx, cy - length),
        D2D1::Point2F(cx, cy + length),
        g_greenBrush,
        width
    );
}

// ============================================================
// 根据当前预设绘制准星
// ============================================================

static HRESULT DrawCrosshair()
{
    if (!g_target || !g_greenBrush || !g_redBrush)
    {
        return E_FAIL;
    }

    // 主显示器中心，转换为虚拟桌面窗口客户区坐标
    const float centerX =
        static_cast<float>(
            GetSystemMetrics(SM_CXSCREEN) / 2 - g_virtualX
        );

    const float centerY =
        static_cast<float>(
            GetSystemMetrics(SM_CYSCREEN) / 2 - g_virtualY
        );

    g_target->BeginDraw();

    // 透明背景
    g_target->Clear(
        D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)
    );

    switch (g_currentPreset)
    {
    case CrosshairPreset::CS_MINIMAL:
        DrawCSMinimal(centerX, centerY);
        break;

    case CrosshairPreset::GREEN_CIRCLE_RED_DOT:
        DrawGreenCircleRedDot(centerX, centerY);
        break;

    case CrosshairPreset::MINIMAL_GREEN_CROSS:
        DrawMinimalGreenCross(centerX, centerY);
        break;

    default:
        break;
    }

    return g_target->EndDraw();
}

// ============================================================
// 将绘图缓冲区合成到透明窗口
// ============================================================

static BOOL PresentOverlay()
{
    POINT destination{
        g_virtualX,
        g_virtualY
    };

    SIZE size{
        g_screenWidth,
        g_screenHeight
    };

    POINT source{ 0, 0 };

    BLENDFUNCTION blend{};

    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    return UpdateLayeredWindow(
        g_hwnd,
        nullptr,
        &destination,
        &size,
        g_memDC,
        &source,
        0,
        &blend,
        ULW_ALPHA
    );
}

// ============================================================
// 绘制并刷新
// ============================================================

static HRESULT RenderAndPresent()
{
    HRESULT hr = DrawCrosshair();

    if (FAILED(hr))
    {
        return hr;
    }

    if (!PresentOverlay())
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

// ============================================================
// 重建绘图资源
// ============================================================

static HRESULT RebuildOverlay()
{
    HRESULT hr = CreateRenderResources();

    if (FAILED(hr))
    {
        return hr;
    }

    hr = RenderAndPresent();

    if (FAILED(hr))
    {
        return hr;
    }

    SetWindowPos(
        g_hwnd,
        HWND_TOPMOST,
        g_virtualX,
        g_virtualY,
        g_screenWidth,
        g_screenHeight,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER
    );

    return S_OK;
}

// ============================================================
// 切换下一套准星
// ============================================================

static void NextPreset()
{
    const int count =
        static_cast<int>(CrosshairPreset::COUNT);

    int current =
        static_cast<int>(g_currentPreset);

    current = (current + 1) % count;

    g_currentPreset =
        static_cast<CrosshairPreset>(current);

    RenderAndPresent();
}

// ============================================================
// 切换上一套准星
// ============================================================

static void PreviousPreset()
{
    const int count =
        static_cast<int>(CrosshairPreset::COUNT);

    int current =
        static_cast<int>(g_currentPreset);

    current = (current - 1 + count) % count;

    g_currentPreset =
        static_cast<CrosshairPreset>(current);

    RenderAndPresent();
}

// ============================================================
// 窗口消息处理
// ============================================================

static LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message)
    {
    case WM_HOTKEY:

        // F8：显示 / 隐藏
        if (wParam == 1)
        {
            g_visible = !g_visible;

            ShowWindow(
                hwnd,
                g_visible
                    ? SW_SHOWNOACTIVATE
                    : SW_HIDE
            );
        }

        // F9：下一套
        else if (wParam == 2)
        {
            NextPreset();
        }

        // Shift + F9：上一套
        else if (wParam == 3)
        {
            PreviousPreset();
        }

        // Ctrl + Shift + Q：退出
        else if (wParam == 4)
        {
            DestroyWindow(hwnd);
        }

        return 0;

    case WM_DISPLAYCHANGE:

        RebuildOverlay();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:

        UnregisterHotKey(hwnd, 1);
        UnregisterHotKey(hwnd, 2);
        UnregisterHotKey(hwnd, 3);
        UnregisterHotKey(hwnd, 4);

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}

// ============================================================
// 程序入口
// ============================================================

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int
)
{
    // DPI 感知
    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    );

    HRESULT comHr = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED
    );

    if (FAILED(comHr))
    {
        return 1;
    }

    HRESULT hr = InitializeDirect2D();

    if (FAILED(hr))
    {
        CoUninitialize();
        return 2;
    }

    // 注册窗口类
    WNDCLASSEXW wc{};

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Direct2DCrosshairWindow";

    if (!RegisterClassExW(&wc))
    {
        g_factory->Release();
        CoUninitialize();
        return 3;
    }

    // 创建透明覆盖窗口
    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED |
        WS_EX_TRANSPARENT |
        WS_EX_TOOLWINDOW |
        WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"Direct2D Crosshair",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_hwnd)
    {
        g_factory->Release();
        CoUninitialize();
        return 4;
    }

    // F8：显示 / 隐藏
    if (!RegisterHotKey(g_hwnd, 1, 0, VK_F8))
    {
        DestroyWindow(g_hwnd);
        g_factory->Release();
        CoUninitialize();
        return 5;
    }

    // F9：下一套
    if (!RegisterHotKey(g_hwnd, 2, 0, VK_F9))
    {
        DestroyWindow(g_hwnd);
        g_factory->Release();
        CoUninitialize();
        return 6;
    }

    // Shift + F9：上一套
    if (!RegisterHotKey(
        g_hwnd,
        3,
        MOD_SHIFT,
        VK_F9
    ))
    {
        DestroyWindow(g_hwnd);
        g_factory->Release();
        CoUninitialize();
        return 7;
    }

    // Ctrl + Shift + Q：退出
    if (!RegisterHotKey(
        g_hwnd,
        4,
        MOD_CONTROL | MOD_SHIFT,
        'Q'
    ))
    {
        DestroyWindow(g_hwnd);
        g_factory->Release();
        CoUninitialize();
        return 8;
    }

    // 初始化绘图
    hr = RebuildOverlay();

    if (FAILED(hr))
    {
        DestroyWindow(g_hwnd);
        ReleaseRenderResources();
        g_factory->Release();
        CoUninitialize();
        return 9;
    }

    ShowWindow(
        g_hwnd,
        SW_SHOWNOACTIVATE
    );

    // 消息循环
    MSG msg{};

    int result = 0;

    while (true)
    {
        const BOOL ret = GetMessageW(
            &msg,
            nullptr,
            0,
            0
        );

        if (ret <= 0)
        {
            if (ret == -1)
            {
                result = 10;
            }
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 释放资源
    ReleaseRenderResources();

    if (g_factory)
    {
        g_factory->Release();
        g_factory = nullptr;
    }

    CoUninitialize();

    return result;
}
