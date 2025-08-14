// ConfigDialog.cpp - FIXED VERSION
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <wtypes.h>
#include <sstream>
#include "ConfigManager.h"
#include "resource.h"
#include "ConfigDialog.h"

// External declaration for the global instance handle
extern HINSTANCE g_hInstance;

// Global variables for config dialog
COLORREF selectedTextColor = RGB(255, 255, 255);
COLORREF selectedBgColor = RGB(0, 0, 0);

// Function declarations
LRESULT CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

void ShowConfigDialog(HWND hParent) {
    DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_CONFIG), hParent, ConfigDialogProc);
}

LRESULT CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
    {
        // Set dialog title
        SetWindowText(hDlg, L"ARP Ticker Tape Configuration");

        // Load current config values and populate controls
        std::wstring symbolsText;
        for (size_t i = 0; i < ConfigManager::symbols.size(); i++) {
            if (i > 0) symbolsText += L",";
            symbolsText += ConfigManager::symbols[i];
        }
        SetDlgItemText(hDlg, IDC_SYMBOLS_EDIT, symbolsText.c_str());

        SetDlgItemInt(hDlg, IDC_REFRESH_EDIT, ConfigManager::refreshInterval, FALSE);

        wchar_t buffer[32];
        swprintf_s(buffer, 32, L"%.1f", ConfigManager::scrollSpeed);
        SetDlgItemText(hDlg, IDC_SCROLL_SPEED_EDIT, buffer);

        SetDlgItemInt(hDlg, IDC_WINDOW_HEIGHT_EDIT, ConfigManager::windowHeight, FALSE);
        SetDlgItemInt(hDlg, IDC_FONT_SIZE_EDIT, ConfigManager::fontSize, FALSE);
        SetDlgItemText(hDlg, IDC_FONT_NAME_EDIT, ConfigManager::fontName.c_str());

        // Store current colors
        selectedTextColor = RGB(
            (ConfigManager::textColor >> 16) & 0xFF,
            (ConfigManager::textColor >> 8) & 0xFF,
            ConfigManager::textColor & 0xFF
        );
        selectedBgColor = RGB(
            (ConfigManager::bgColor >> 16) & 0xFF,
            (ConfigManager::bgColor >> 8) & 0xFF,
            ConfigManager::bgColor & 0xFF
        );

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_TEXT_COLOR_BTN:
        {
            CHOOSECOLOR cc = { 0 };
            COLORREF customColors[16] = { 0 };

            cc.lStructSize = sizeof(CHOOSECOLOR);
            cc.hwndOwner = hDlg;
            cc.rgbResult = selectedTextColor;
            cc.lpCustColors = customColors;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColor(&cc)) {
                selectedTextColor = cc.rgbResult;
            }
            return TRUE;
        }

        case IDC_BG_COLOR_BTN:
        {
            CHOOSECOLOR cc = { 0 };
            COLORREF customColors[16] = { 0 };

            cc.lStructSize = sizeof(CHOOSECOLOR);
            cc.hwndOwner = hDlg;
            cc.rgbResult = selectedBgColor;
            cc.lpCustColors = customColors;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColor(&cc)) {
                selectedBgColor = cc.rgbResult;
            }
            return TRUE;
        }

        case IDC_FONT_BROWSE_BTN:
        {
            CHOOSEFONT cf = { 0 };
            LOGFONT lf = { 0 };

            // Initialize with current font
            wcscpy_s(lf.lfFaceName, LF_FACESIZE, ConfigManager::fontName.c_str());
            lf.lfHeight = -ConfigManager::fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
            lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
            lf.lfQuality = DEFAULT_QUALITY;
            lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

            cf.lStructSize = sizeof(CHOOSEFONT);
            cf.hwndOwner = hDlg;
            cf.lpLogFont = &lf;
            cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
            cf.rgbColors = selectedTextColor;

            if (ChooseFont(&cf)) {
                SetDlgItemText(hDlg, IDC_FONT_NAME_EDIT, lf.lfFaceName);
                SetDlgItemInt(hDlg, IDC_FONT_SIZE_EDIT, abs(lf.lfHeight), FALSE);
                selectedTextColor = cf.rgbColors;
            }
            return TRUE;
        }

        case IDC_OK:
        case IDC_APPLY:
        {
            // Validate and save configuration
            wchar_t buffer[1024];

            // Get symbols
            GetDlgItemText(hDlg, IDC_SYMBOLS_EDIT, buffer, 1024);
            std::wstring symbolsText = buffer;

            // Parse symbols
            ConfigManager::symbols.clear();
            std::wstringstream ss(symbolsText);
            std::wstring symbol;
            while (std::getline(ss, symbol, L',')) {
                // Trim whitespace
                symbol.erase(0, symbol.find_first_not_of(L" \t"));
                symbol.erase(symbol.find_last_not_of(L" \t") + 1);
                if (!symbol.empty()) {
                    ConfigManager::symbols.push_back(symbol);
                }
            }

            if (ConfigManager::symbols.empty()) {
                MessageBox(hDlg, L"Please enter at least one stock symbol.", L"Error", MB_OK | MB_ICONERROR);
                return TRUE;
            }

            // Get refresh interval
            BOOL translated;
            int refreshInterval = GetDlgItemInt(hDlg, IDC_REFRESH_EDIT, &translated, FALSE);
            if (!translated || refreshInterval < 1) {
                MessageBox(hDlg, L"Refresh interval must be a positive number.", L"Error", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            ConfigManager::refreshInterval = refreshInterval;

            // Get scroll speed
            GetDlgItemText(hDlg, IDC_SCROLL_SPEED_EDIT, buffer, 32);
            double scrollSpeed = _wtof(buffer);
            if (scrollSpeed <= 0.0) {
                MessageBox(hDlg, L"Scroll speed must be a positive number.", L"Error", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            ConfigManager::scrollSpeed = scrollSpeed;

            // Get window height
            int windowHeight = GetDlgItemInt(hDlg, IDC_WINDOW_HEIGHT_EDIT, &translated, FALSE);
            if (!translated || windowHeight < 10) {
                MessageBox(hDlg, L"Window height must be at least 10 pixels.", L"Error", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            ConfigManager::windowHeight = windowHeight;

            // Get font size
            int fontSize = GetDlgItemInt(hDlg, IDC_FONT_SIZE_EDIT, &translated, FALSE);
            if (!translated || fontSize < 8) {
                MessageBox(hDlg, L"Font size must be at least 8.", L"Error", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            ConfigManager::fontSize = fontSize;

            // Get font name
            GetDlgItemText(hDlg, IDC_FONT_NAME_EDIT, buffer, 256);
            ConfigManager::fontName = buffer;

            // Convert colors
            ConfigManager::textColor = (GetRValue(selectedTextColor) << 16) |
                (GetGValue(selectedTextColor) << 8) |
                GetBValue(selectedTextColor);
            ConfigManager::bgColor = (GetRValue(selectedBgColor) << 16) |
                (GetGValue(selectedBgColor) << 8) |
                GetBValue(selectedBgColor);

            // Save configuration
            ConfigManager::SaveConfig();

            if (LOWORD(wParam) == IDC_OK) {
                EndDialog(hDlg, IDOK);
            }
            else {
                // IDC_APPLY - keep dialog open but apply changes
                MessageBox(hDlg, L"Settings applied successfully!", L"Configuration", MB_OK | MB_ICONINFORMATION);
            }
            return TRUE;
        }

        case IDC_CANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}