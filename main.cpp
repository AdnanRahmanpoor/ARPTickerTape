// main.cpp
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>  // For SHAppBarMessage
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <chrono>

#include "TickerManager.h"
#include "ApiFetcher.h"
#include "ConfigManager.h"
#include "Renderer.h"

#define TIMER_SCROLL 1
#define APPBAR_CALLBACK WM_APP + 1
#define SCROLL_INTERVAL 33

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void DockWindow(HWND hWnd, bool top);
void UndockWindow(HWND hWnd);
void UpdateWindowSize(HWND hWnd);
void RecalculateCharWidth(HWND hWnd);
void UpdateLayeredDisplay(HWND hWnd);  // New: render via UpdateLayeredWindow

// Global variables
std::wstring tickerText;
std::mutex textMutex;
std::atomic<bool> appRunning(true);
std::atomic<bool> dataUpdated(false);
double scrollOffset = 0.0;
int charWidth = 10;
int windowWidth = 0;
int windowHeight = 30;
bool isPaused = false;
bool isHidden = false;
HMENU hMenu;
bool isDocked = false;
bool isDockedTop = true;

// Worker thread function
void APIWorkerThread() {
    while (appRunning.load()) {
        std::wstring newText;
        bool hasData = false;

        for (const auto& symbol : ConfigManager::symbols) {
            double price = ApiFetcher::FetchPrice(symbol);
            if (price > 0.0) {
                wchar_t buffer[64];
                swprintf(buffer, 64, L"%s: $%.2f   ", symbol.c_str(), price);
                newText += buffer;
                hasData = true;
            }
        }

        if (hasData) {
            std::lock_guard<std::mutex> lock(textMutex);
            tickerText = newText + newText; // Seamless loop
            dataUpdated = true;
        }

        std::this_thread::sleep_for(std::chrono::seconds(ConfigManager::refreshInterval));
    }
}

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow) {
    ConfigManager::LoadConfig();

    WNDCLASS wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = TEXT("TickerTapeClass");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    HWND hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,  // ✅ Use WS_EX_LAYERED
        TEXT("TickerTapeClass"),
        TEXT("TickerTape"),
        WS_POPUP | WS_VISIBLE,
        100, 100, screenWidth, ConfigManager::windowHeight,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) return 1;

    Renderer::Init(hWnd);

    // Create context menu
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, 1, TEXT("Pause"));
    AppendMenu(hMenu, MF_STRING, 2, TEXT("Resume"));
    AppendMenu(hMenu, MF_STRING, 3, TEXT("Reload Config"));
    AppendMenu(hMenu, MF_STRING, 5, TEXT("Dock Top"));
    AppendMenu(hMenu, MF_STRING, 6, TEXT("Dock Bottom"));
    AppendMenu(hMenu, MF_STRING, 7, TEXT("Undock"));
    AppendMenu(hMenu, MF_STRING, 4, TEXT("Exit"));

    // Register hotkeys
    RegisterHotKey(hWnd, 100, MOD_CONTROL | MOD_ALT, 'P');
    RegisterHotKey(hWnd, 101, MOD_CONTROL | MOD_ALT, 'H');

    std::thread apiThread(APIWorkerThread);

    // Initial setup
    RecalculateCharWidth(hWnd);
    {
        std::lock_guard<std::mutex> lock(textMutex);
        tickerText = L"Loading...   ";
        tickerText += tickerText;
    }
    dataUpdated = true;

    SetTimer(hWnd, TIMER_SCROLL, SCROLL_INTERVAL, NULL);

    // ✅ Initial render via UpdateLayeredWindow
    UpdateLayeredDisplay(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    appRunning = false;
    if (apiThread.joinable()) apiThread.join();
    Renderer::Cleanup();

    UnregisterHotKey(hWnd, 100);
    UnregisterHotKey(hWnd, 101);

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static ULONGLONG lastClickTime = 0;

    switch (message) {
    case WM_PAINT:
        // ❌ No GDI painting — handled by UpdateLayeredWindow
        ValidateRect(hWnd, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_SCROLL && !isPaused && !isHidden) {
            if (!tickerText.empty()) {
                int textPixelWidth = ((int)tickerText.length() / 2) * charWidth;
                if (textPixelWidth > 0) {
                    scrollOffset += ConfigManager::scrollSpeed;
                    if (scrollOffset >= textPixelWidth) {
                        scrollOffset = fmod(scrollOffset, textPixelWidth);
                    }
                }
                UpdateLayeredDisplay(hWnd);  // ✅ Redraw
            }
        }
        return 0;

    case WM_HOTKEY:
        if (wParam == 100) isPaused = !isPaused;
        else if (wParam == 101) {
            isHidden = !isHidden;
            ShowWindow(hWnd, isHidden ? SW_HIDE : SW_SHOW);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        ULONGLONG currentTime = GetTickCount64();
        if (currentTime - lastClickTime < GetDoubleClickTime()) {
            // Double-click: toggle dock
            if (isDocked) {
                UndockWindow(hWnd);
            }
            else {
                DockWindow(hWnd, true);
            }
        }
        else {
            // Single click: drag if not docked
            if (!isDocked) {
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
                return 0;
            }
        }
        lastClickTime = currentTime;
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1: isPaused = true; break;
        case 2: isPaused = false; break;
        case 3:
            ConfigManager::LoadConfig();
            Renderer::Init(hWnd);
            RecalculateCharWidth(hWnd);
            UpdateWindowSize(hWnd);
            {
                std::lock_guard<std::mutex> lock(textMutex);
                tickerText = L"Reloading...   ";
                tickerText += tickerText;
            }
            dataUpdated = true;
            UpdateLayeredDisplay(hWnd);
            break;
        case 5: DockWindow(hWnd, true); break;
        case 6: DockWindow(hWnd, false); break;
        case 7: UndockWindow(hWnd); break;
        case 4: DestroyWindow(hWnd); break;
        }
        return 0;

    case WM_SIZE:
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        UpdateLayeredDisplay(hWnd);  // ✅ Redraw on resize
        return 0;

    case WM_CREATE:
        RecalculateCharWidth(hWnd);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, isDocked ? IDC_ARROW : IDC_SIZEALL));
            return TRUE;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case APPBAR_CALLBACK:
        if (wParam == ABN_POSCHANGED && isDocked) {
            UpdateLayeredDisplay(hWnd);  // ✅ Redraw after dock resize
            return 0;
        }
        break;

    case WM_DESTROY:
        if (isDocked) UndockWindow(hWnd);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void UpdateLayeredDisplay(HWND hWnd) {
    // Check if window is visible
    if (!IsWindowVisible(hWnd) || isHidden) {
        return;
    }

    RECT clientRect;
    if (!GetClientRect(hWnd, &clientRect)) return;

    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    if (width <= 0 || height <= 0) return;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HBITMAP hbmMem = CreateCompatibleBitmap(hdcScreen, width, height);
    if (!hbmMem) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HGDIOBJ hOldBmp = SelectObject(hdcMem, hbmMem);
    HGDIOBJ hOldFont = SelectObject(hdcMem, Renderer::GetFont());

    // Clear background with full alpha
    RECT fillRect = { 0, 0, width, height };
    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdcMem, &fillRect, hBrush);
    DeleteObject(hBrush);

    // Render text
    {
        std::lock_guard<std::mutex> lock(textMutex);
        if (!tickerText.empty()) {
            Renderer::Render(hdcMem, tickerText, scrollOffset, width, height, charWidth);
        }
    }

    // ✅ Get window position for UpdateLayeredWindow
    RECT windowRect;
    GetWindowRect(hWnd, &windowRect);
    POINT ptDst = { windowRect.left, windowRect.top };

    BLENDFUNCTION bf = { 0 };
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;  // Full opacity
    bf.AlphaFormat = 0;

    SIZE sizeWnd = { width, height };
    POINT ptSrc = { 0, 0 };

    // ✅ Use window position as destination
    BOOL result = UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &sizeWnd,
        hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);

    if (!result) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        swprintf(errorMsg, 256, L"UpdateLayeredWindow failed! Error: %lu\n", error);
        OutputDebugStringW(errorMsg);

        // ✅ Fallback: try without destination point
        result = UpdateLayeredWindow(hWnd, hdcScreen, nullptr, &sizeWnd,
            hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
    }

    // Cleanup
    SelectObject(hdcMem, hOldFont);
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}


void DockWindow(HWND hWnd, bool top) {
    static bool isDocking = false;
    if (isDocking) return;
    isDocking = true;

    // First undock if already docked
    if (isDocked) {
        APPBARDATA abd = { 0 };
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = hWnd;
        SHAppBarMessage(ABM_REMOVE, &abd);
        isDocked = false;
    }

    APPBARDATA abd = { 0 };
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = hWnd;
    abd.uCallbackMessage = APPBAR_CALLBACK;

    if (SHAppBarMessage(ABM_NEW, &abd) == 0) {
        isDocking = false;
        return;
    }

    isDocked = true;
    isDockedTop = top;

    // Get monitor information
    HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMon, &mi)) {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }

    // Set initial rectangle to full monitor width
    abd.rc = mi.rcWork;
    abd.uEdge = top ? ABE_TOP : ABE_BOTTOM;

    // Query position first
    if (SHAppBarMessage(ABM_QUERYPOS, &abd) == 0) {
        SHAppBarMessage(ABM_REMOVE, &abd);
        isDocking = false;
        return;
    }

    // Adjust height
    int desiredHeight = ConfigManager::windowHeight;
    if (top) {
        abd.rc.bottom = abd.rc.top + desiredHeight;
    }
    else {
        abd.rc.top = abd.rc.bottom - desiredHeight;
    }

    // Set final position
    if (SHAppBarMessage(ABM_SETPOS, &abd) == 0) {
        SHAppBarMessage(ABM_REMOVE, &abd);
        isDocking = false;
        return;
    }

    // ✅ CRITICAL: Temporarily remove layered style during positioning
    LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);

    // Position the window
    SetWindowPos(hWnd, HWND_TOPMOST,
        abd.rc.left, abd.rc.top,
        abd.rc.right - abd.rc.left,
        abd.rc.bottom - abd.rc.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // ✅ Restore layered style
    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);

    // Update global dimensions
    windowWidth = abd.rc.right - abd.rc.left;
    windowHeight = abd.rc.bottom - abd.rc.top;

    // Reinitialize renderer with new dimensions
    Renderer::Init(hWnd);
    RecalculateCharWidth(hWnd);

    // ✅ Add a small delay to ensure window is properly positioned
    Sleep(50);

    // Force a complete redraw
    InvalidateRect(hWnd, NULL, TRUE);
    UpdateLayeredDisplay(hWnd);

    isDocking = false;
}

void UndockWindow(HWND hWnd) {
    if (!isDocked) return;

    APPBARDATA abd = { 0 };
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = hWnd;
    SHAppBarMessage(ABM_REMOVE, &abd);  // ✅ Remove from AppBar

    isDocked = false;

    RECT currentRect;
    GetWindowRect(hWnd, &currentRect);

    // Restore as normal topmost window
    SetWindowPos(hWnd, HWND_TOPMOST,
        currentRect.left, currentRect.top,
        GetSystemMetrics(SM_CXSCREEN), ConfigManager::windowHeight,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Update size
    windowWidth = GetSystemMetrics(SM_CXSCREEN);
    windowHeight = ConfigManager::windowHeight;

    UpdateLayeredDisplay(hWnd);
}

void UpdateWindowSize(HWND hWnd) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    SetWindowPos(hWnd, NULL, 0, 0, screenWidth, ConfigManager::windowHeight,
        SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    UpdateLayeredDisplay(hWnd);
}

void RecalculateCharWidth(HWND hWnd) {
    HDC hdc = GetDC(hWnd);
    HFONT hFont = Renderer::GetFont();
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SIZE size;
    GetTextExtentPoint32(hdc, TEXT("A"), 1, &size);
    charWidth = size.cx;

    SelectObject(hdc, hOldFont);
    ReleaseDC(hWnd, hdc);
}