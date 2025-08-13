// Renderer.cpp
#include "Renderer.h"
#include "ConfigManager.h"
#include <windows.h>

static HFONT g_font = nullptr;

HFONT Renderer::GetFont() {
    return g_font;
}

COLORREF Renderer::GetTextColor() {
    const std::wstring& scheme = ConfigManager::colorScheme;
    if (scheme == L"Red") return RGB(255, 0, 0);
    else if (scheme == L"Blue") return RGB(0, 0, 255);
    else if (scheme == L"Yellow") return RGB(255, 255, 0);
    else if (scheme == L"Cyan") return RGB(0, 255, 255);
    else if (scheme == L"Magenta") return RGB(255, 0, 255);
    else if (scheme == L"White") return RGB(255, 255, 255);
    else return RGB(0, 255, 0);
}

COLORREF Renderer::GetBackgroundColor() {
    return RGB(0, 0, 0);
}

void Renderer::Init(HWND hWnd) {
    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }

    HDC hdc = GetDC(hWnd);  // ✅ Correct: use window's DC
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hWnd, hdc);   // ✅ Matched: same hWnd

    int scaledSize = -MulDiv(ConfigManager::fontSize, dpiY, 72);
    g_font = CreateFontW(scaledSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier New");

    if (!g_font) {
        g_font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
    }
}

void Renderer::Cleanup() {
    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }
}

void Renderer::Render(HDC hdcWindow, const std::wstring& text, double offset, int width, int height, int charWidth) {
    if (!hdcWindow || width <= 0 || height <= 0 || text.empty()) return;

    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    if (!hdcMem) return;

    HBITMAP hbmMem = CreateCompatibleBitmap(hdcWindow, width, height);
    if (!hbmMem) {
        DeleteDC(hdcMem);
        return;
    }

    HGDIOBJ hOldBmp = SelectObject(hdcMem, hbmMem);
    HGDIOBJ hOldFont = SelectObject(hdcMem, g_font);

    RECT rect = { 0, 0, width, height };
    HBRUSH hBrush = CreateSolidBrush(GetBackgroundColor());
    FillRect(hdcMem, &rect, hBrush);
    DeleteObject(hBrush);

    SetTextColor(hdcMem, GetTextColor());
    SetBkMode(hdcMem, TRANSPARENT);

    TEXTMETRIC tm = {};
    GetTextMetrics(hdcMem, &tm);
    int yPos = (height - tm.tmHeight) / 2;

    TextOutW(hdcMem, -static_cast<int>(offset), yPos, text.c_str(), static_cast<int>(text.length()));

    BitBlt(hdcWindow, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOldFont);
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}