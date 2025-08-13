// ConfigManager.h
#pragma once
#include <vector>
#include <string>

class ConfigManager {
public:
    static void LoadConfig();
    static void SaveConfig();

    // Settings
    static std::vector<std::wstring> symbols;
    static int refreshInterval;
    static int scrollSpeed;
    static std::wstring colorScheme;
    static int windowHeight;
    static int fontSize;
};