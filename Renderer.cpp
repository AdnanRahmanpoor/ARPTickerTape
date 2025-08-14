#include "Renderer.h"
#include "ConfigManager.h"

static HFONT g_font = nullptr;

HFONT Renderer::GetFont() {
    return g_font;
}

COLORREF Renderer::GetTextColor() {
    // Use the textColor from ConfigManager directly
    DWORD color = ConfigManager::textColor;
    return RGB(
        (color >> 16) & 0xFF,  // Red
        (color >> 8) & 0xFF,   // Green
        color & 0xFF           // Blue
    );
}

COLORREF Renderer::GetBackgroundColor() {
    // Use the bgColor from ConfigManager directly
    DWORD color = ConfigManager::bgColor;
    return RGB(
        (color >> 16) & 0xFF,  // Red
        (color >> 8) & 0xFF,   // Green
        color & 0xFF           // Blue
    );
}

void Renderer::Init(HWND hWnd) {
    if (g_font) {
        DeleteObject(g_font);
        g_font = nullptr;
    }

    HDC hdc = GetDC(hWnd);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hWnd, hdc);

    int scaledSize = -MulDiv(ConfigManager::fontSize, dpiY, 72);
    g_font = CreateFontW(
        scaledSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        ConfigManager::fontName.c_str()
    );

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

    // Clear background
    RECT rect = { 0, 0, width, height };
    HBRUSH hBrush = CreateSolidBrush(GetBackgroundColor());
    FillRect(hdcMem, &rect, hBrush);
    DeleteObject(hBrush);

    // Set text properties
    SetTextColor(hdcMem, GetTextColor());
    SetBkMode(hdcMem, TRANSPARENT);

    // Calculate vertical centering
    TEXTMETRIC tm = {};
    GetTextMetrics(hdcMem, &tm);
    int yPos = (height - tm.tmHeight) / 2;

    // Draw the text with scrolling offset
    TextOutW(hdcMem, -static_cast<int>(offset), yPos, text.c_str(), static_cast<int>(text.length()));

    // Copy to the target DC
    BitBlt(hdcWindow, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(hdcMem, hOldFont);
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}
