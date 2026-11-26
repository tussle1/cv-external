#include "injector.h"
#include "payload.h"
#include "pipe_client.h"
#include "process.h"
#include "fonts.h"

#include <Windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <process.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace fs = std::filesystem;

static constexpr wchar_t kWindowClass[] = L"CatVapeExternalWindow";
static constexpr wchar_t kWindowTitle[] = L"catvape external";
static constexpr wchar_t kRobloxExe[]   = L"RobloxPlayerBeta.exe";
static constexpr wchar_t kBridgeDll[]   = L"CatVapeBridge.dll";
static constexpr wchar_t kPipeName[]    = L"\\\\.\\pipe\\CatVapeExternal_v1";

static constexpr UINT WM_APP_STATUS = WM_APP + 1;
static constexpr UINT_PTR kAnimTimerId = 1;
static constexpr int kWidth = 620;
static constexpr int kHeight = 360;

struct UiState {
    HWND hwnd = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontBody = nullptr;
    HFONT fontSmall = nullptr;
    HFONT fontButton = nullptr;
    HANDLE fontResRegular = nullptr;
    HANDLE fontResSemiBold = nullptr;
    HANDLE fontResBold = nullptr;

    float fade = 0.0f;           // 0..1 window fade-in
    float pulse = 0.0f;          // 0..2pi background pulse
    float btnHover = 0.0f;       // 0..1 hover anim
    float btnPress = 0.0f;       // 0..1 press anim
    float statusAlpha = 1.0f;    // status text fade
    bool hovering = false;
    bool pressing = false;
    std::atomic<bool> injecting{false};
    std::atomic<bool> done{false};

    std::mutex mtx;
    std::wstring status = L"ready to inject";
    std::wstring detail = L"waiting for you to press inject";
};

static UiState g_ui;
static ULONG_PTR g_gdiplusToken = 0;

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH * 4]{};
    DWORD len = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    if (len == 0) return L".";
    fs::path p(std::wstring(buf, len));
    return p.parent_path().wstring();
}

static void SetStatus(const std::wstring& status, const std::wstring& detail = L"") {
    {
        std::lock_guard<std::mutex> lock(g_ui.mtx);
        g_ui.status = status;
        g_ui.detail = detail;
        g_ui.statusAlpha = 0.0f;
    }
    if (g_ui.hwnd) PostMessageW(g_ui.hwnd, WM_APP_STATUS, 0, 0);
}

static HFONT MakeFont(int size, int weight, const wchar_t* face) {
    return CreateFontW(
        -size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void LoadFonts() {
    DWORD n1 = 0, n2 = 0, n3 = 0;
    g_ui.fontResRegular = AddFontMemResourceEx((PVOID)kPoppinsRegular, (DWORD)kPoppinsRegularSize, nullptr, &n1);
    g_ui.fontResSemiBold = AddFontMemResourceEx((PVOID)kPoppinsSemiBold, (DWORD)kPoppinsSemiBoldSize, nullptr, &n2);
    g_ui.fontResBold = AddFontMemResourceEx((PVOID)kPoppinsBold, (DWORD)kPoppinsBoldSize, nullptr, &n3);

    g_ui.fontTitle = MakeFont(34, FW_SEMIBOLD, L"Poppins");
    g_ui.fontBody = MakeFont(20, FW_NORMAL, L"Poppins");
    g_ui.fontSmall = MakeFont(15, FW_NORMAL, L"Poppins");
    g_ui.fontButton = MakeFont(22, FW_SEMIBOLD, L"Poppins");
}

static void UnloadFonts() {
    if (g_ui.fontTitle) { DeleteObject(g_ui.fontTitle); g_ui.fontTitle = nullptr; }
    if (g_ui.fontBody) { DeleteObject(g_ui.fontBody); g_ui.fontBody = nullptr; }
    if (g_ui.fontSmall) { DeleteObject(g_ui.fontSmall); g_ui.fontSmall = nullptr; }
    if (g_ui.fontButton) { DeleteObject(g_ui.fontButton); g_ui.fontButton = nullptr; }
    if (g_ui.fontResRegular) { RemoveFontMemResourceEx(g_ui.fontResRegular); g_ui.fontResRegular = nullptr; }
    if (g_ui.fontResSemiBold) { RemoveFontMemResourceEx(g_ui.fontResSemiBold); g_ui.fontResSemiBold = nullptr; }
    if (g_ui.fontResBold) { RemoveFontMemResourceEx(g_ui.fontResBold); g_ui.fontResBold = nullptr; }
}

static RECT ButtonRect(const RECT& client) {
    int bw = 320;
    int bh = 56;
    int cx = (client.left + client.right) / 2;
    int cy = (client.top + client.bottom) / 2 + 30;
    return RECT{ cx - bw / 2, cy - bh / 2, cx + bw / 2, cy + bh / 2 };
}

static bool PointInRect(POINT p, const RECT& r) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

static void DrawRoundedRect(Gdiplus::Graphics& g, const Gdiplus::RectF& r, float radius,
                            const Gdiplus::Color& fill, const Gdiplus::Color& border, float borderWidth = 1.0f) {
    Gdiplus::GraphicsPath path;
    float d = radius * 2.0f;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();

    if (fill.GetA() > 0) {
        Gdiplus::SolidBrush brush(fill);
        g.FillPath(&brush, &path);
    }
    if (border.GetA() > 0 && borderWidth > 0.0f) {
        Gdiplus::Pen pen(border, borderWidth);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawPath(&pen, &path);
    }
}

static void Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) { EndPaint(hwnd, &ps); return; }

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

    Gdiplus::Graphics g(mem);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // background
    Gdiplus::SolidBrush bg(Gdiplus::Color(255, 14, 14, 20));
    g.FillRectangle(&bg, 0, 0, w, h);

    // animated background glow
    float pulse = g_ui.pulse;
    int glowA = (int)((18.0f + 10.0f * std::sinf(pulse)) * g_ui.fade);
    float glowX = w * 0.5f + 60.0f * std::cosf(pulse * 0.7f);
    float glowY = h * 0.35f + 30.0f * std::sinf(pulse * 0.9f);
    Gdiplus::SolidBrush glowBrush(Gdiplus::Color(glowA, 140, 100, 255));
    g.FillEllipse(&glowBrush, glowX - 180.0f, glowY - 120.0f, 360.0f, 240.0f);

    // title
    HFONT oldFont = (HFONT)SelectObject(mem, g_ui.fontTitle);
    SetBkMode(mem, TRANSPARENT);
    int titleAlpha = (int)(255 * g_ui.fade);
    SetTextColor(mem, RGB(245, 245, 250));
    RECT titleRc = { 0, 48, w, 92 };
    DrawTextW(mem, L"catvape external", -1, &titleRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // subtitle
    SelectObject(mem, g_ui.fontBody);
    SetTextColor(mem, RGB(160, 160, 180));
    RECT subRc = { 0, 88, w, 118 };
    DrawTextW(mem, L"ported by xylen", -1, &subRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // button
    RECT br = ButtonRect(rc);
    float hover = g_ui.btnHover;
    float press = g_ui.btnPress;
    float scale = 1.0f + 0.02f * hover - 0.02f * press;
    float bw2 = (br.right - br.left) * scale;
    float bh2 = (br.bottom - br.top) * scale;
    float bcx = (br.left + br.right) * 0.5f;
    float bcy = (br.top + br.bottom) * 0.5f;
    Gdiplus::RectF btnRect(bcx - bw2 / 2, bcy - bh2 / 2, bw2, bh2);

    int baseA = (int)(220 * g_ui.fade);
    int hoverR = (int)(120 + 40 * hover);
    int hoverG = (int)(90 + 30 * hover);
    int hoverB = 255;
    Gdiplus::Color btnFill(baseA, hoverR, hoverG, hoverB);
    Gdiplus::Color btnBorder((int)(120 * g_ui.fade), 180, 160, 255);

    // button glow
    if (hover > 0.01f) {
        Gdiplus::RectF glowR(btnRect.X - 8, btnRect.Y - 8, btnRect.Width + 16, btnRect.Height + 16);
        DrawRoundedRect(g, glowR, 30.0f, Gdiplus::Color((int)(35 * hover * g_ui.fade), 140, 110, 255), Gdiplus::Color(0, 0, 0, 0), 0);
    }

    DrawRoundedRect(g, btnRect, 28.0f, btnFill, btnBorder, 1.0f);

    SelectObject(mem, g_ui.fontButton);
    SetTextColor(mem, RGB(255, 255, 255));
    RECT textRc = { (LONG)btnRect.X, (LONG)btnRect.Y, (LONG)(btnRect.X + btnRect.Width), (LONG)(btnRect.Y + btnRect.Height) };
    const wchar_t* btnText = g_ui.injecting.load() ? L"injecting..." : (g_ui.done.load() ? L"done" : L"inject");
    DrawTextW(mem, btnText, -1, &textRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // status
    std::wstring status, detail;
    {
        std::lock_guard<std::mutex> lock(g_ui.mtx);
        status = g_ui.status;
        detail = g_ui.detail;
    }
    int sa = (int)(220 * std::clamp(g_ui.statusAlpha, 0.0f, 1.0f) * g_ui.fade);

    SelectObject(mem, g_ui.fontBody);
    SetTextColor(mem, RGB(220, 220, 235));
    RECT statusRc = { 24, br.bottom + 28, w - 24, br.bottom + 54 };
    DrawTextW(mem, status.c_str(), -1, &statusRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    SelectObject(mem, g_ui.fontSmall);
    SetTextColor(mem, RGB(130, 130, 150));
    RECT detailRc = { 24, br.bottom + 54, w - 24, h - 16 };
    DrawTextW(mem, detail.c_str(), -1, &detailRc, DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

    // top accent line
    Gdiplus::Pen accent(Gdiplus::Color((int)(100 * g_ui.fade), 140, 100, 255), 2.0f);
    g.DrawLine(&accent, 40, 36, w - 40, 36);

    SelectObject(mem, oldFont);
    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static void DoInject() {
    g_ui.injecting.store(true);
    g_ui.done.store(false);
    SetStatus(L"looking for roblox...", L"start roblox and join a game if its not open");

    auto pidOpt = cv::process::FindFirstByName(kRobloxExe);
    if (!pidOpt) {
        SetStatus(L"waiting for roblox...", L"open roblox and join a game");
        pidOpt = cv::process::WaitForProcess(kRobloxExe, 1000, 120000);
    }
    if (!pidOpt) {
        SetStatus(L"couldnt find roblox", L"timed out waiting for robloxplayerbeta.exe");
        g_ui.injecting.store(false);
        return;
    }

    DWORD pid = *pidOpt;
    SetStatus(L"found roblox", L"pid " + std::to_wstring(pid) + L" injecting dll...");

    const std::wstring dllPath = (fs::path(ExeDir()) / kBridgeDll).wstring();
    if (!fs::exists(dllPath)) {
        SetStatus(L"missing CatVapeBridge.dll", L"put CatVapeBridge.dll next to the exe");
        g_ui.injecting.store(false);
        return;
    }

    std::string err;
    if (!cv::inject::InjectDll(pid, dllPath, err)) {
        std::wstring werr(err.begin(), err.end());
        SetStatus(L"injection failed", werr);
        g_ui.injecting.store(false);
        return;
    }

    std::vector<unsigned char> payload;
    std::string perr;
    if (!LoadEmbeddedPayload(payload, perr)) {
        SetStatus(L"payload missing", std::wstring(perr.begin(), perr.end()));
        g_ui.injecting.store(false);
        return;
    }

    SetStatus(L"sending payload...", L"talking to bridge");

    std::string msg;
    bool ok = cv::pipe::SendPayload(kPipeName, payload.data(), payload.size(), msg, 20000);
    std::wstring wmsg(msg.begin(), msg.end());

    if (ok) {
        g_ui.done.store(true);
        SetStatus(L"injected", wmsg.empty() ? L"catvape payload sent to the bridge" : wmsg);
    } else {
        SetStatus(L"payload send failed", wmsg.empty() ? L"bridge didnt respond check execute.cpp" : wmsg);
    }

    g_ui.injecting.store(false);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_ui.hwnd = hwnd;
        LoadFonts();
        SetTimer(hwnd, kAnimTimerId, 16, nullptr);
        return 0;
    }
    case WM_TIMER: {
        if (wp == kAnimTimerId) {
            g_ui.fade = std::min(1.0f, g_ui.fade + 0.04f);
            g_ui.pulse += 0.03f;
            if (g_ui.pulse > 6.2831853f * 10.0f) g_ui.pulse -= 6.2831853f * 10.0f;

            float hoverTarget = g_ui.hovering ? 1.0f : 0.0f;
            g_ui.btnHover += (hoverTarget - g_ui.btnHover) * 0.18f;
            float pressTarget = g_ui.pressing ? 1.0f : 0.0f;
            g_ui.btnPress += (pressTarget - g_ui.btnPress) * 0.25f;
            g_ui.statusAlpha = std::min(1.0f, g_ui.statusAlpha + 0.06f);

            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT rc; GetClientRect(hwnd, &rc);
        bool h = PointInRect(p, ButtonRect(rc));
        if (h != g_ui.hovering) {
            g_ui.hovering = h;
            TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        g_ui.hovering = false;
        g_ui.pressing = false;
        return 0;
    }
    case WM_SETCURSOR: {
        POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
        RECT rc; GetClientRect(hwnd, &rc);
        if (PointInRect(p, ButtonRect(rc)) && !g_ui.injecting.load()) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT rc; GetClientRect(hwnd, &rc);
        if (PointInRect(p, ButtonRect(rc))) {
            g_ui.pressing = true;
            SetCapture(hwnd);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        bool wasPressing = g_ui.pressing;
        g_ui.pressing = false;
        ReleaseCapture();
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT rc; GetClientRect(hwnd, &rc);
        if (wasPressing && PointInRect(p, ButtonRect(rc)) && !g_ui.injecting.load()) {
            HANDLE t = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { DoInject(); return 0; }, nullptr, 0, nullptr);
            if (t) CloseHandle(t);
        }
        return 0;
    }
    case WM_APP_STATUS: {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kAnimTimerId);
        UnloadFonts();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdipIn;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdipIn, nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;

    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT wr{ 0, 0, kWidth, kHeight };
    AdjustWindowRect(&wr, style, FALSE);
    int winW = wr.right - wr.left;
    int winH = wr.bottom - wr.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    HWND hwnd = CreateWindowExW(
        0, kWindowClass, kWindowTitle, style,
        sx, sy, winW, winH,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    return (int)msg.wParam;
}
