// main.cpp - Complete implementation with all features
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>  // For SHAppBarMessage and system tray
#include <commctrl.h>  // For common controls
#include <commdlg.h>   // For color and font dialogs
#include <windowsx.h>  // For additional Windows macros
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
#include "resource.h"
#include "ConfigDialog.h"  // Include header instead of .cpp

#define TIMER_SCROLL 1
#define APPBAR_CALLBACK WM_APP + 1
#define WM_TRAYICON WM_APP + 2
#define WM_SHOW_EXISTING WM_APP + 3
#define SCROLL_INTERVAL 33

// Single instance mutex name
#define MUTEX_NAME L"ARPTickerTapeSingleInstance"

// Function declarations
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void DockWindow(HWND hWnd, bool top);
void UndockWindow(HWND hWnd);
void UpdateWindowSize(HWND hWnd);
void RecalculateCharWidth(HWND hWnd);
void UpdateLayeredDisplay(HWND hWnd);
void CreateSystemTrayIcon(HWND hWnd);
void RemoveSystemTrayIcon();
void CleanupAndExit();
bool CheckSingleInstance();
void ShowExistingInstance();
void APIWorkerThread();

// Global variables
std::wstring tickerText;
std::mutex textMutex;
std::atomic<bool> appRunning(true);
std::atomic<bool> dataUpdated(false);
std::atomic<bool> forceExit(false);
double scrollOffset = 0.0;
int charWidth = 10;
int windowWidth = 0;
int windowHeight = 30;
bool isPaused = false;
bool isHidden = false;
bool isMinimized = false;
HMENU hMenu;
HMENU hTrayMenu;
bool isDocked = false;
bool isDockedTop = true;
NOTIFYICONDATA nid = { 0 };
HINSTANCE g_hInstance;
HWND g_hMainWnd = NULL;
HANDLE g_hMutex = NULL;


bool CheckSingleInstance() {
    // Create or open a named mutex
    g_hMutex = CreateMutex(NULL, TRUE, MUTEX_NAME);

    if (g_hMutex == NULL) {
        return false; // Error creating mutex
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
        return false;
    }

    return true; // We are the first instance
}

void ShowExistingInstance() {
    // Find the existing instance window
    HWND existingWnd = FindWindow(TEXT("TickerTapeClass"), NULL);
    if (existingWnd) {
        // Send a message to show the existing instance
        PostMessage(existingWnd, WM_SHOW_EXISTING, 0, 0);

        // Bring the existing instance to foreground if it's visible
        if (IsWindowVisible(existingWnd)) {
            SetForegroundWindow(existingWnd);
            BringWindowToTop(existingWnd);
        }
    }
}

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
            // Create seamless endless loop by repeating the text multiple times
            // This ensures smooth scrolling without visible gaps
            tickerText = newText + newText + newText;
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

    // Check for single instance
    if (!CheckSingleInstance()) {
        // Another instance is running, show it and exit
        ShowExistingInstance();
        MessageBox(NULL, L"ARP Ticker Tape is already running. Check the system tray.",
            L"ARP Ticker Tape", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    g_hInstance = hInstance;
    ConfigManager::LoadConfig();

    // Initialize common controls
    InitCommonControls();

    WNDCLASS wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = TEXT("TickerTapeClass");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

    RegisterClass(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    HWND hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        TEXT("TickerTapeClass"),
        TEXT("TickerTape"),
        WS_POPUP | WS_VISIBLE,
        100, 100, screenWidth, ConfigManager::windowHeight,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        if (g_hMutex) {
            ReleaseMutex(g_hMutex);
            CloseHandle(g_hMutex);
        }
        return 1;
    }

    g_hMainWnd = hWnd;
    Renderer::Init(hWnd);

    // Create main context menu
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_PAUSE, TEXT("Pause"));
    AppendMenu(hMenu, MF_STRING, IDM_RESUME, TEXT("Resume"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_DOCK_TOP, TEXT("Dock Top"));
    AppendMenu(hMenu, MF_STRING, IDM_DOCK_BOTTOM, TEXT("Dock Bottom"));
    AppendMenu(hMenu, MF_STRING, IDM_UNDOCK, TEXT("Undock"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_MINIMIZE, TEXT("Minimize to Tray"));
    AppendMenu(hMenu, MF_STRING, IDM_SETTINGS, TEXT("Settings..."));
    AppendMenu(hMenu, MF_STRING, IDM_RELOAD, TEXT("Reload Config"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, TEXT("Exit"));

    // Create system tray menu
    hTrayMenu = CreatePopupMenu();
    AppendMenu(hTrayMenu, MF_STRING, IDM_SHOW, TEXT("Show"));
    AppendMenu(hTrayMenu, MF_STRING, IDM_PAUSE, TEXT("Pause"));
    AppendMenu(hTrayMenu, MF_STRING, IDM_RESUME, TEXT("Resume"));
    AppendMenu(hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hTrayMenu, MF_STRING, IDM_SETTINGS, TEXT("Settings..."));
    AppendMenu(hTrayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hTrayMenu, MF_STRING, IDM_EXIT, TEXT("Exit"));

    // Create system tray icon
    CreateSystemTrayIcon(hWnd);

    // Register hotkeys
    RegisterHotKey(hWnd, 100, MOD_CONTROL | MOD_ALT, 'P');
    RegisterHotKey(hWnd, 101, MOD_CONTROL | MOD_ALT, 'H');

    std::thread apiThread(APIWorkerThread);

    // Initial setup
    RecalculateCharWidth(hWnd);
    {
        std::lock_guard<std::mutex> lock(textMutex);
        std::wstring loadingText = L"Loading...   ";
        tickerText = loadingText + loadingText + loadingText;
    }
    dataUpdated = true;

    SetTimer(hWnd, TIMER_SCROLL, SCROLL_INTERVAL, NULL);
    UpdateLayeredDisplay(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    CleanupAndExit();

    if (apiThread.joinable()) {
        apiThread.join();
    }

    return (int)msg.wParam;
}

void CleanupAndExit() {
    appRunning = false;

    // Remove system tray icon
    RemoveSystemTrayIcon();

    // Cleanup renderer
    Renderer::Cleanup();

    // Destroy menus
    if (hMenu) {
        DestroyMenu(hMenu);
        hMenu = NULL;
    }
    if (hTrayMenu) {
        DestroyMenu(hTrayMenu);
        hTrayMenu = NULL;
    }

    // Release mutex
    if (g_hMutex) {
        ReleaseMutex(g_hMutex);
        CloseHandle(g_hMutex);
        g_hMutex = NULL;
    }
}

void CreateSystemTrayIcon(HWND hWnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = IDI_TRAY;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcscpy_s(nid.szTip, sizeof(nid.szTip) / sizeof(wchar_t), L"ARP Ticker Tape");

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveSystemTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static ULONGLONG lastClickTime = 0;

    switch (message) {
    case WM_PAINT:
        ValidateRect(hWnd, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_SCROLL && !isPaused && !isHidden && !isMinimized) {
            if (!tickerText.empty()) {
                // Calculate the width of one complete cycle of tickers
                int singleCycleWidth = ((int)tickerText.length() / 3) * charWidth;
                if (singleCycleWidth > 0) {
                    scrollOffset += ConfigManager::scrollSpeed;
                    // Reset scroll when we've completed one full cycle
                    if (scrollOffset >= singleCycleWidth) {
                        scrollOffset = fmod(scrollOffset, singleCycleWidth);
                    }
                }
                UpdateLayeredDisplay(hWnd);
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

        // Update menu items based on current state
        EnableMenuItem(hMenu, IDM_PAUSE, isPaused ? MF_GRAYED : MF_ENABLED);
        EnableMenuItem(hMenu, IDM_RESUME, isPaused ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(hMenu, IDM_DOCK_TOP, isDocked && isDockedTop ? MF_GRAYED : MF_ENABLED);
        EnableMenuItem(hMenu, IDM_DOCK_BOTTOM, isDocked && !isDockedTop ? MF_GRAYED : MF_ENABLED);
        EnableMenuItem(hMenu, IDM_UNDOCK, isDocked ? MF_ENABLED : MF_GRAYED);

        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        return 0;
    }

    case WM_SHOW_EXISTING:
        // Message from another instance trying to start
        if (isMinimized || isHidden) {
            ShowWindow(hWnd, SW_SHOW);
            isMinimized = false;
            isHidden = false;
        }
        SetForegroundWindow(hWnd);
        BringWindowToTop(hWnd);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDOWN) {
            // Left click on tray icon - show/hide window
            if (isMinimized) {
                ShowWindow(hWnd, SW_SHOW);
                isMinimized = false;
            }
            else {
                ShowWindow(hWnd, SW_HIDE);
                isMinimized = true;
            }
        }
        else if (lParam == WM_RBUTTONDOWN) {
            // Right click on tray icon - show context menu
            POINT pt;
            GetCursorPos(&pt);

            // Update tray menu items
            EnableMenuItem(hTrayMenu, IDM_SHOW, isMinimized ? MF_ENABLED : MF_GRAYED);
            EnableMenuItem(hTrayMenu, IDM_PAUSE, isPaused ? MF_GRAYED : MF_ENABLED);
            EnableMenuItem(hTrayMenu, IDM_RESUME, isPaused ? MF_ENABLED : MF_GRAYED);

            SetForegroundWindow(hWnd); // Required for tray menus
            TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            PostMessage(hWnd, WM_NULL, 0, 0); // Required for tray menus
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_PAUSE:
            isPaused = true;
            break;
        case IDM_RESUME:
            isPaused = false;
            break;
        case IDM_RELOAD:
            ConfigManager::LoadConfig();
            Renderer::Init(hWnd);
            RecalculateCharWidth(hWnd);
            UpdateWindowSize(hWnd);
            {
                std::lock_guard<std::mutex> lock(textMutex);
                std::wstring reloadingText = L"Reloading...   ";
                tickerText = reloadingText + reloadingText + reloadingText;
            }
            dataUpdated = true;
            UpdateLayeredDisplay(hWnd);
            break;
        case IDM_DOCK_TOP:
            DockWindow(hWnd, true);
            break;
        case IDM_DOCK_BOTTOM:
            DockWindow(hWnd, false);
            break;
        case IDM_UNDOCK:
            UndockWindow(hWnd);
            break;
        case IDM_MINIMIZE:
            ShowWindow(hWnd, SW_HIDE);
            isMinimized = true;
            break;
        case IDM_SHOW:
            ShowWindow(hWnd, SW_SHOW);
            isMinimized = false;
            break;
        case IDM_SETTINGS:
            ShowConfigDialog(hWnd);
            break;
        case IDM_EXIT:
            PostMessage(hWnd, WM_DESTROY, 0, 0);
            break;
        }
        return 0;

    case WM_SIZE:
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        if (!isMinimized) {
            UpdateLayeredDisplay(hWnd);
        }
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

    case WM_WINDOWPOSCHANGED:
        if (isDocked && !isMinimized) {
            WINDOWPOS* wp = (WINDOWPOS*)lParam;
            if (!(wp->flags & SWP_NOSIZE) || !(wp->flags & SWP_NOMOVE)) {
                UpdateLayeredDisplay(hWnd);
            }
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case APPBAR_CALLBACK:
        if (wParam == ABN_POSCHANGED && isDocked && !isMinimized) {
            UpdateLayeredDisplay(hWnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        // Check if this is a forced exit (from Exit menu) or just a close attempt
        if (forceExit.load()) {
            // Actually exit the application
            DestroyWindow(hWnd);
        }
        else {
            // Hide window instead of destroying it (minimize to tray)
            ShowWindow(hWnd, SW_HIDE);
            isMinimized = true;
        }
        return 0;

    case WM_DESTROY:
        // Unregister hotkeys
        UnregisterHotKey(hWnd, 100);
        UnregisterHotKey(hWnd, 101);

        // Undock if docked
        if (isDocked) UndockWindow(hWnd);

        // Kill timer
        KillTimer(hWnd, TIMER_SCROLL);

        // Post quit message
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void UpdateLayeredDisplay(HWND hWnd) {
    // Check if window is visible
    if (!IsWindowVisible(hWnd) || isHidden || isMinimized) {
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

    // Clear background
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

    // Get window position for UpdateLayeredWindow
    RECT windowRect;
    GetWindowRect(hWnd, &windowRect);
    POINT ptDst = { windowRect.left, windowRect.top };

    BLENDFUNCTION bf = { 0 };
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = 0;

    SIZE sizeWnd = { width, height };
    POINT ptSrc = { 0, 0 };

    BOOL result = UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &sizeWnd,
        hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);

    if (!result) {
        // Fallback: try without destination point
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

    HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMon, &mi)) {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }

    abd.rc = mi.rcWork;
    abd.uEdge = top ? ABE_TOP : ABE_BOTTOM;

    if (SHAppBarMessage(ABM_QUERYPOS, &abd) == 0) {
        SHAppBarMessage(ABM_REMOVE, &abd);
        isDocking = false;
        return;
    }

    int desiredHeight = ConfigManager::windowHeight;
    if (top) {
        abd.rc.bottom = abd.rc.top + desiredHeight;
    }
    else {
        abd.rc.top = abd.rc.bottom - desiredHeight;
    }

    if (SHAppBarMessage(ABM_SETPOS, &abd) == 0) {
        SHAppBarMessage(ABM_REMOVE, &abd);
        isDocking = false;
        return;
    }

    // Temporarily remove layered style during positioning
    LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);

    SetWindowPos(hWnd, HWND_TOPMOST,
        abd.rc.left, abd.rc.top,
        abd.rc.right - abd.rc.left,
        abd.rc.bottom - abd.rc.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Restore layered style
    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);

    windowWidth = abd.rc.right - abd.rc.left;
    windowHeight = abd.rc.bottom - abd.rc.top;

    Renderer::Init(hWnd);
    RecalculateCharWidth(hWnd);

    Sleep(50);

    InvalidateRect(hWnd, NULL, TRUE);
    UpdateLayeredDisplay(hWnd);

    isDocking = false;
}

void UndockWindow(HWND hWnd) {
    if (!isDocked) return;

    APPBARDATA abd = { 0 };
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = hWnd;
    SHAppBarMessage(ABM_REMOVE, &abd);

    isDocked = false;

    RECT currentRect;
    GetWindowRect(hWnd, &currentRect);

    SetWindowPos(hWnd, HWND_TOPMOST,
        currentRect.left, currentRect.top,
        GetSystemMetrics(SM_CXSCREEN), ConfigManager::windowHeight,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

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