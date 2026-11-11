# Direct2D Crosshair

一个基于 **C++、Win32 API 和 Direct2D** 开发的 Windows 桌面准星覆盖工具。

项目采用独立透明窗口实现准星显示，不依赖游戏内部接口，不读取游戏进程数据。通过 Direct2D 提供抗锯齿绘制能力，实现平滑的圆环和中心点效果。

## 项目预览

当前默认准星样式：

- 绿色细线空心圆环
- 红色实心中心点
- 透明背景
- 鼠标穿透
- 支持快捷键显示和隐藏

## 功能特性

- **Direct2D 渲染**：使用 Direct2D 实现图形绘制，支持抗锯齿。
- **透明窗口**：使用 Win32 Layered Window 实现透明覆盖。
- **鼠标穿透**：覆盖窗口不拦截鼠标操作。
- **窗口置顶**：保持准星覆盖窗口位于其他普通窗口上方。
- **低依赖**：不依赖 Qt、SDL 等第三方 GUI 框架。
- **快捷键控制**：支持全局快捷键显示、隐藏和退出。
- **参数可配置**：可修改圆环半径、线宽和中心点大小。

## 技术栈

| 技术 | 用途 |
|---|---|
| C++17 | 核心开发语言 |
| Win32 API | 窗口管理、快捷键、窗口属性 |
| Direct2D | 图形绘制与抗锯齿 |
| GDI DIBSection | 32 位透明绘图缓冲区 |
| UpdateLayeredWindow | 分层窗口透明合成 |

## 项目结构

```text
Direct2D-Crosshair/
├── src/
    └── main.cpp
├── README.md
└── LICENSE
```

## 环境要求

- Windows 10 / Windows 11
- Visual Studio 2022 或兼容的 MSVC 工具链
- Windows SDK
- C++17 编译支持
- Direct2D 开发头文件及链接库

## 快速开始

### 1. 克隆项目

```bash
git clone https://github.com/YOUR_USERNAME/Direct2D-Crosshair.git
cd Direct2D-Crosshair
```

将 `YOUR_USERNAME` 替换为自己的 GitHub 用户名。

### 2. 编译项目

推荐使用 Visual Studio 的 x64 Native Tools Command Prompt。

执行：

```bat
cl /std:c++17 /O2 /EHsc /W4 /utf-8 /D_WIN32_WINNT=0x0A00 main.cpp /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib d2d1.lib ole32.lib /OUT:FPSCrosshair.exe
```

编译成功后生成：

```text
FPSCrosshair.exe
```

### 3. 运行

双击 `FPSCrosshair.exe` 即可启动准星覆盖程序。

## 快捷键

| 快捷键 | 功能 |
|---|---|
| F8 | 显示 / 隐藏准星 |
| Ctrl + Shift + Q | 退出程序 |

快捷键使用 Windows 全局热键机制注册。

## 准星参数配置

当前版本通过 `main.cpp` 中的 `Config` 命名空间配置准星。

```cpp
namespace Config
{
    // 绿色圆环
    constexpr float RING_RADIUS = 35.0f;
    constexpr float RING_WIDTH  = 1.0f;

    // 红色中心点
    constexpr float DOT_RADIUS = 2.2f;

    // 圆环颜色
    constexpr float RING_R = 0.0f;
    constexpr float RING_G = 1.0f;
    constexpr float RING_B = 0.0f;

    // 中心点颜色
    constexpr float DOT_R = 1.0f;
    constexpr float DOT_G = 0.08f;
    constexpr float DOT_B = 0.08f;
}
```

### 参数说明

| 参数 | 默认值 | 说明 |
|---|---:|---|
| RING_RADIUS | 35.0 | 圆环半径 |
| RING_WIDTH | 1.0 | 圆环线宽 |
| DOT_RADIUS | 2.2 | 中心点半径 |
| RING_R | 0.0 | 圆环红色通道 |
| RING_G | 1.0 | 圆环绿色通道 |
| RING_B | 0.0 | 圆环蓝色通道 |
| DOT_R | 1.0 | 中心点红色通道 |
| DOT_G | 0.08 | 中心点绿色通道 |
| DOT_B | 0.08 | 中心点蓝色通道 |

颜色通道取值范围为 `0.0 - 1.0`。

例如：

绿色：

```cpp
D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f)
```

红色：

```cpp
D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f)
```

## 实现原理

项目采用独立窗口覆盖的方式显示准星。

整体绘制流程：

```text
             Windows Desktop
                    |
                    v
          Win32 Overlay Window
                    |
                    v
             Direct2D Renderer
                    |
                    v
          32-bit BGRA DIBSection
                    |
                    v
          UpdateLayeredWindow
                    |
                    v
           Transparent Crosshair
```

### 1. 窗口实现

通过以下窗口扩展属性创建透明覆盖窗口：

```cpp
WS_EX_LAYERED |
WS_EX_TRANSPARENT |
WS_EX_TOOLWINDOW |
WS_EX_NOACTIVATE
```

各属性用途：

| 属性 | 作用 |
|---|---|
| WS_EX_LAYERED | 启用分层窗口 |
| WS_EX_TRANSPARENT | 支持鼠标穿透 |
| WS_EX_TOOLWINDOW | 避免显示在任务栏和 Alt+Tab 窗口列表中 |
| WS_EX_NOACTIVATE | 显示窗口时不抢夺焦点 |

### 2. 图形绘制

使用 Direct2D 的 `ID2D1DCRenderTarget` 进行绘制。

抗锯齿设置：

```cpp
g_target->SetAntialiasMode(
    D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
);
```

圆环绘制：

```cpp
g_target->DrawEllipse(
    ring,
    g_ringBrush,
    Config::RING_WIDTH
);
```

中心点绘制：

```cpp
g_target->FillEllipse(
    dot,
    g_dotBrush
);
```

### 3. 透明合成

绘图缓冲区使用 32 位 BGRA 格式，并采用预乘 Alpha 模式。

最终通过：

```cpp
UpdateLayeredWindow(
    hwnd,
    nullptr,
    &destination,
    &size,
    memDC,
    &source,
    0,
    &blend,
    ULW_ALPHA
);
```

将带透明度的绘图结果合成到桌面。

## 注意事项

1. 本项目是独立桌面图形覆盖工具，不涉及游戏进程读取、DLL 注入、内存修改或输入模拟。
2. 本项目不保证兼容所有游戏、反作弊系统或显示模式。
3. 部分游戏的独占全屏模式可能不显示外部窗口覆盖层。
4. 不同显示器布局、DPI 缩放和分辨率可能影响准星位置，需要根据实际环境测试。
5. 使用前应确认目标软件及游戏的相关规则是否允许使用外部准星工具。

## 后续规划

- [ ] 支持运行时调整圆环大小
- [ ] 支持运行时调整线条粗细
- [ ] 支持自定义准星颜色
- [ ] 支持多种准星样式
- [ ] 支持配置文件保存
- [ ] 支持托盘菜单
- [ ] 支持多显示器指定显示
- [ ] 增加 DPI 与显示器布局适配
- [ ] 增加独立配置界面
- [ ] 支持配置热更新，无需重新编译

## 参与贡献

欢迎提交 Issue 和 Pull Request。

如果你发现问题，建议提交以下信息：

- Windows 版本
- 显示器分辨率
- DPI 缩放比例
- 游戏显示模式
- 问题复现步骤

## License

项目许可证尚待确定。

---

**Direct2D Crosshair**  
A lightweight Windows crosshair overlay built with C++ and Direct2D.
