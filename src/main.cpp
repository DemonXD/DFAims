#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <cstdint>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

// ============================================================
// 准星参数
// ============================================================

namespace Config
{
    // 绿色圆环
    constexpr float RING_RADIUS = 35.0f;
    constexpr float RING_WIDTH  = 0.2f;

    // 红色中心点
    constexpr float DOT_RADIUS = 1.5f;

    // 圆环颜色
    constexpr float RING_R = 0.0f;
    constexpr float RING_G = 1.0f;
    constexpr float RING_B = 0.0f;

    // 中心点颜色
    constexpr float DOT_R = 1.0f;
    constexpr float DOT_G = 0.08f;
    constexpr float DOT_B = 0.08f;
}

// ============================================================
// 全局资源
// ============================================================

static HWND g_hwnd = nullptr;

static ID2D1Factory* g_factory = nullptr;
static ID2D1DCRenderTarget* g_target = nullptr;

static ID2D1SolidColorBrush* g_ringBrush = nullptr;
static ID2D1SolidColorBrush* g_dotBrush = nullptr;

static HDC g_memDC = nullptr;
static HBITMAP g_dibBitmap = nullptr;
static HGDIOBJ g_oldBitmap = nullptr;

static void* g_bitmapBits = nullptr;

static int g_virtualX = 0;
static int g_virtualY = 0;
static int g_screenWidth = 0;
static int g_screenHeight = 0;

static bool g_visible = true;

// ============================================================
// 资源释放
// ============================================================

static void ReleaseRenderResources()
{
    if (g_ringBrush)
    {
        g_ringBrush->Release();
        g_ringBrush = nullptr;
    }

    if (g_dotBrush)
    {
        g_dotBrush->Release();
        g_dotBrush = nullptr;
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
// 创建 Direct2D Factory
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
// 创建绘图缓冲区
// ============================================================

static HRESULT CreateRenderResources()
{
    ReleaseRenderResources();

    // 获取虚拟桌面范围
    g_virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);

    g_screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (g_screenWidth <= 0 || g_screenHeight <= 0)
    {
        return E_FAIL;
    }

    // 创建内存 DC
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

    // 清空缓冲区，确保背景 Alpha = 0
    ZeroMemory(
        g_bitmapBits,
        static_cast<size_t>(g_screenWidth) *
        static_cast<size_t>(g_screenHeight) * 4
    );

    // 创建 Direct2D DC Render Target
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

    // 绑定 DIB 对应的 DC
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

    // 设置抗锯齿
    g_target->SetAntialiasMode(
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
    );

    // 创建绿色圆环画刷
    hr = g_target->CreateSolidColorBrush(
        D2D1::ColorF(
            Config::RING_R,
            Config::RING_G,
            Config::RING_B,
            1.0f
        ),
        &g_ringBrush
    );

    if (FAILED(hr))
    {
        return hr;
    }

    // 创建红色中心点画刷
    hr = g_target->CreateSolidColorBrush(
        D2D1::ColorF(
            Config::DOT_R,
            Config::DOT_G,
            Config::DOT_B,
            1.0f
        ),
        &g_dotBrush
    );

    return hr;
}

// ============================================================
// 绘制准星
// ============================================================

static HRESULT DrawCrosshair()
{
    if (!g_target || !g_ringBrush || !g_dotBrush)
    {
        return E_FAIL;
    }

    // 以主显示器中心作为准星中心
    // 转换为虚拟桌面窗口客户区坐标
    const float centerX =
        static_cast<float>(
            GetSystemMetrics(SM_CXSCREEN) / 2 - g_virtualX
        );

    const float centerY =
        static_cast<float>(
            GetSystemMetrics(SM_CYSCREEN) / 2 - g_virtualY
        );

    g_target->BeginDraw();

    // 完全透明背景
    g_target->Clear(
        D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)
    );

    // 绿色空心圆环
    const D2D1_ELLIPSE ring =
        D2D1::Ellipse(
            D2D1::Point2F(centerX, centerY),
            Config::RING_RADIUS,
            Config::RING_RADIUS
        );

    g_target->DrawEllipse(
        ring,
        g_ringBrush,
        Config::RING_WIDTH
    );

    // 红色实心中心点
    const D2D1_ELLIPSE dot =
        D2D1::Ellipse(
            D2D1::Point2F(centerX, centerY),
            Config::DOT_RADIUS,
            Config::DOT_RADIUS
        );

    g_target->FillEllipse(
        dot,
        g_dotBrush
    );

    return g_target->EndDraw();
}

// ============================================================
// 更新分层窗口
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
// 重建并刷新绘图资源
// ============================================================

static HRESULT RebuildOverlay()
{
    HRESULT hr = CreateRenderResources();

    if (FAILED(hr))
    {
        return hr;
    }

    hr = DrawCrosshair();

    if (FAILED(hr))
    {
        return hr;
    }

    if (!PresentOverlay())
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    SetWindowPos(
        g_hwnd,
        HWND_TOPMOST,
        g_virtualX,
        g_virtualY,
        g_screenWidth,
        g_screenHeight,
        // SWP_NOACTIVATE | SWP_SHOWWINDOW
        SWP_NOACTIVATE
    );

    return S_OK;
}

// ============================================================
// 窗口消息
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
                g_visible ? SW_SHOWNOACTIVATE : SW_HIDE
            );
        }

        // Ctrl + Shift + Q：退出
        else if (wParam == 2)
        {
            DestroyWindow(hwnd);
        }

        return 0;

    case WM_DISPLAYCHANGE:

        // 显示器布局或分辨率发生变化时重建缓冲区
        RebuildOverlay();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:

        UnregisterHotKey(hwnd, 1);
        UnregisterHotKey(hwnd, 2);

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
    // 使用物理像素坐标，避免 DPI 虚拟化
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

    // 创建全虚拟桌面覆盖窗口
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

    // 注册全局快捷键
    if (!RegisterHotKey(g_hwnd, 1, 0, VK_F8))
    {
        DestroyWindow(g_hwnd);
        ReleaseRenderResources();
        g_factory->Release();
        CoUninitialize();
        return 5;
    }

    if (!RegisterHotKey(
        g_hwnd,
        2,
        MOD_CONTROL | MOD_SHIFT,
        'Q'
    ))
    {
        UnregisterHotKey(g_hwnd, 1);
        DestroyWindow(g_hwnd);
        ReleaseRenderResources();
        g_factory->Release();
        CoUninitialize();
        return 6;
    }

    // 初始化绘图资源
    hr = RebuildOverlay();

    if (FAILED(hr))
    {
        DestroyWindow(g_hwnd);
        ReleaseRenderResources();
        g_factory->Release();
        CoUninitialize();
        return 7;
    }

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);

    // 消息循环
    MSG msg{};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
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

    return static_cast<int>(msg.wParam);
}
