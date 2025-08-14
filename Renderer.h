#pragma once
#ifndef RENDERER_H
#define RENDERER_H

#include <windows.h>
#include <string>

class Renderer {
public:
    // Initialize resources (font)
    static void Init(HWND hWnd);

    // Clean up GDI resources
    static void Cleanup();

    // Render the ticker text
    // Note: scrollOffset is now double for sub-pixel scrolling support
    static void Render(HDC hdcWindow, const std::wstring& text, double offset,
        int width, int height, int charWidth);

    // Accessor for font (used in text measurement)
    static HFONT GetFont();

private:
    // Color helpers (based on ConfigManager)
    static COLORREF GetTextColor();
    static COLORREF GetBackgroundColor();
};

#endif
