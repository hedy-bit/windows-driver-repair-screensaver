#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>

// Visual simulation only: no driver access, shutdown, registry, or repair calls.
static bool preview = false;
static ULONGLONG started = 0;
static constexpr double CycleSeconds = 166.0;
struct ProgressStep { double seconds; int percent; };
static constexpr ProgressStep BlueProgress[] = {
    {0, 0}, {6, 3}, {10, 8}, {17, 12}, {20, 19}, {29, 23},
    {33, 37}, {40, 41}, {44, 58}, {50, 67}, {57, 82}, {63, 99}, {69, 100}
};
static constexpr ProgressStep RepairProgress[] = {
    {0, 0}, {5, 2}, {9, 7}, {16, 11}, {20, 18}, {29, 26},
    {34, 39}, {41, 43}, {46, 61}, {53, 74}, {58, 88}, {62, 99}, {67, 100}
};
template <size_t N>
static int SteppedProgress(double elapsed, const ProgressStep (&steps)[N]) {
    int percent = 0;
    for (const auto& step : steps) {
        if (elapsed < step.seconds) break;
        percent = step.percent;
    }
    return percent;
}

static HFONT Font(int size, int weight = FW_NORMAL) {
    return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
}
static void Label(HDC dc, int x, int y, int width, int height,
                  const std::wstring& value, int size, int weight = FW_NORMAL) {
    HFONT font = Font(size, weight);
    auto old = SelectObject(dc, font);
    RECT r{x, y, x + width, y + height};
    DrawTextW(dc, value.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(font);
}
static void Paint(HWND window, HDC target) {
    RECT rect{};
    GetClientRect(window, &rect);
    int w = rect.right, h = rect.bottom;
    if (w <= 0 || h <= 0) return;
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, w, h);
    auto oldBitmap = SelectObject(dc, bitmap);
    // Render into a logical 1280 x 720 canvas, scaled to fit even preview windows.
    double scale = std::min(w / 1280.0, h / 720.0);
    double t = static_cast<double>(GetTickCount64() - started) / 1000.0;
    double phase = std::fmod(t, CycleSeconds);
    bool blue = phase < 72.0;
    HBRUSH background = CreateSolidBrush(blue ? RGB(0, 105, 185) : RGB(12, 15, 20));
    FillRect(dc, &rect, background);
    DeleteObject(background);
    SetGraphicsMode(dc, GM_ADVANCED);
    XFORM transform{static_cast<FLOAT>(scale), 0, 0, static_cast<FLOAT>(scale),
        static_cast<FLOAT>((w - 1280 * scale) / 2), static_cast<FLOAT>((h - 720 * scale) / 2)};
    SetWorldTransform(dc, &transform);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    if (blue) {
        int percent = SteppedProgress(phase, BlueProgress);
        Label(dc, 145, 62, 1000, 150, L":(", 116);
        Label(dc, 150, 240, 970, 110,
            L"你的设备遇到问题，需要重启。\n我们正在收集错误信息，然后将自动重新启动。", 31);
        Label(dc, 150, 380, 950, 60, std::to_wstring(percent) + L"% 完成", 30);
        Label(dc, 150, 490, 1000, 100,
            L"终止代码：DRIVER_IRQL_NOT_LESS_OR_EQUAL\n失败的操作：nvlddmkm.sys", 21);
    } else {
        // Advance in visible ticks, with occasional visual stalls; never block input.
        double beat = std::fmod(phase - 72.0, 7.0);
        double animationTime = std::floor((phase - 72.0) / 7.0) * 4.5 + std::min(beat, 4.5);
        animationTime = std::floor(animationTime * 5.0) / 5.0;
        for (int i = 0; i < 8; ++i) {
            double angle = animationTime * 2.5 + i * 6.283185307 / 8;
            int x = 640 + static_cast<int>(std::cos(angle) * 27);
            int y = 252 + static_cast<int>(std::sin(angle) * 27);
            int shade = 80 + i * 25;
            HBRUSH dot = CreateSolidBrush(RGB(shade, shade, shade));
            auto oldBrush = SelectObject(dc, dot);
            auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, x - 4, y - 4, x + 4, y + 4);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(dot);
        }
        std::wstring heading, detail;
        if (phase < 80) {
            heading = L"正在准备自动修复";
            detail = L"正在检查驱动程序启动状态…";
        } else if (phase < 88) {
            heading = L"正在诊断你的电脑";
            detail = L"正在检查系统文件和驱动程序…";
        } else if (phase < 158) {
            int percent = SteppedProgress(phase - 88, RepairProgress);
            heading = L"正在修复驱动程序  " + std::to_wstring(percent) + L"%";
            detail = L"正在恢复驱动配置并验证启动文件…";
        } else if (phase < 162) {
            heading = L"自动修复已完成";
            detail = L"驱动错误已修复，即将在 " + std::to_wstring(162 - static_cast<int>(phase)) + L" 秒后重新启动";
        } else {
            heading = L"正在重新启动";
            detail = L"正在加载驱动程序…";
        }
        // Center text using DrawText, keeping all drawing in the logical canvas.
        HFONT font = Font(32);
        auto old = SelectObject(dc, font);
        RECT title{80, 330, 1200, 390};
        DrawTextW(dc, heading.c_str(), -1, &title, DT_CENTER | DT_TOP | DT_NOPREFIX);
        SelectObject(dc, old);
        DeleteObject(font);
        font = Font(20);
        old = SelectObject(dc, font);
        RECT subtitle{60, 400, 1220, 460};
        DrawTextW(dc, detail.c_str(), -1, &subtitle, DT_CENTER | DT_TOP | DT_NOPREFIX);
        SelectObject(dc, old);
        DeleteObject(font);
    }
    ModifyWorldTransform(dc, nullptr, MWT_IDENTITY);
    BitBlt(target, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE:
        started = GetTickCount64();

        SetTimer(window, 1, 33, nullptr);
        return 0;
    case WM_TIMER:
        if (preview) {
            RECT parentRect{};
            if (!IsWindow(GetParent(window))) { DestroyWindow(window); return 0; }
            GetClientRect(GetParent(window), &parentRect);
            SetWindowPos(window, nullptr, 0, 0, parentRect.right, parentRect.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        Paint(window, dc);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_SYSKEYDOWN:
        if (!preview && wp == VK_F4 && (lp & (1LL << 29))) {
            DestroyWindow(window);
        }
        return 0;
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR:
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
    case WM_MOUSEMOVE: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        return 0;
    case WM_SETCURSOR:
        if (!preview) { SetCursor(nullptr); return TRUE; }
        break;
    case WM_ACTIVATE:
        // Losing focus does not end the screen saver.
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    HWND parent = nullptr;
    if (argv && argc > 1) {
        std::wstring arg = argv[1];
        if (arg.size() >= 2 && (arg[0] == L'/' || arg[0] == L'-')) {
            wchar_t mode = arg[1];
            if (mode == L'c' || mode == L'C') {
                LocalFree(argv);
                MessageBoxW(nullptr, L"驱动错误 → 自动修复 → 模拟重启 → 再次蓝屏。\n每轮约 2 分 46 秒，进度间歇停顿后跳动。按 Alt+F4 退出，普通按键和鼠标操作被忽略。\n本程序仅绘制画面，不会修改驱动或重启电脑。", L"驱动修复循环屏保", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            if (mode == L'p' || mode == L'P') {
                preview = true;
                const wchar_t* handle = arg.size() > 3 && arg[2] == L':' ? arg.c_str() + 3 : (argc > 2 ? argv[2] : L"0");
                parent = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(std::wcstoull(handle, nullptr, 10)));
            }
        }
    }
    if (argv) LocalFree(argv);
    if (preview && !IsWindow(parent)) return 1;
    SetProcessDPIAware();
    WNDCLASSW cls{};
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = instance;
    cls.lpszClassName = L"DriverRepairLoopSaver";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&cls)) return 1;
    RECT bounds{};
    if (preview) GetClientRect(parent, &bounds);
    else {
        bounds.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        bounds.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        bounds.right = bounds.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        bounds.bottom = bounds.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }
    HWND window = CreateWindowExW(preview ? 0 : WS_EX_TOPMOST, cls.lpszClassName,
        L"驱动修复循环屏保", preview ? WS_CHILD | WS_VISIBLE : WS_POPUP,
        bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
        parent, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
