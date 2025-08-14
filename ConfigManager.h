#pragma once
#include <vector>
#include <string>
#include <windows.h>  // Make sure this is included

class ConfigManager {
public:
    static std::vector<std::wstring> symbols;
    static int refreshInterval;
    static double scrollSpeed;
    static int windowHeight;
    static int fontSize;
    static std::wstring fontName;
    static DWORD textColor;
    static DWORD bgColor;
    static std::wstring configPath;

    // ADD THIS LINE to your existing ConfigManager.h:
    static std::wstring colorScheme;

    static void LoadConfig();
    static void SaveConfig();
    static void SetDefaults();

private:
    static std::wstring GetConfigPath();
    static std::wstring Trim(const std::wstring& str);
};
