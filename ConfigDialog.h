#pragma once

#include <windows.h>

// External variable declarations (defined in ConfigDialog.cpp)
extern COLORREF selectedTextColor;
extern COLORREF selectedBgColor;
extern HWND g_hMainWnd;
extern HINSTANCE g_hInstance;

// Function declarations
LRESULT CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void ShowConfigDialog(HWND hWnd);
void RecalculateCharWidth(HWND hWnd);
void UpdateWindowSize(HWND hWnd);
void UpdateLayeredDisplay(HWND hWnd);
