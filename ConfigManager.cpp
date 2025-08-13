// ConfigManager.cpp
#include "ConfigManager.h"
#include <windows.h>              // Required before other Windows headers
#include <shlobj.h>               // SHGetFolderPathW
#include <Shlwapi.h>              // PathFileExistsW
#include <algorithm>              // std::clamp
#include <sstream>
#include <vector>

#pragma comment(lib, "shlwapi.lib")   // For PathFileExistsW
#pragma comment(lib, "shell32.lib")   // For SHGetFolderPathW

// Static member initialization
std::vector<std::wstring> ConfigManager::symbols;
int ConfigManager::refreshInterval = 60;
int ConfigManager::scrollSpeed = 5;
std::wstring ConfigManager::colorScheme = L"Green";
int ConfigManager::windowHeight = 30;
int ConfigManager::fontSize = 16;

// Helper: Get config file path in %APPDATA%\TickerTape\ticker_config.ini
std::wstring GetConfigPath() {
    wchar_t appDataPath[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        std::wstring path = appDataPath;
        path += L"\\ARPTickerTape\\ticker_config.ini";
        return path;
    }
    // Fallback to executable directory
    return L"ticker_config.ini";
}

void ConfigManager::LoadConfig() {
    std::wstring configPath = GetConfigPath();

    // Create directory if it doesn't exist
    size_t pos = configPath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring dir = configPath.substr(0, pos);
        CreateDirectoryW(dir.c_str(), nullptr);
    }

    // Check if config file exists
    if (!PathFileExistsW(configPath.c_str())) {
        // Use defaults and save initial config
        symbols = { L"AAPL", L"MSFT", L"BTC-USD" };
        refreshInterval = 60;
        scrollSpeed = 5;
        colorScheme = L"Green";
        windowHeight = 30;
        fontSize = 16;

        SaveConfig();
        return;
    }

    // Buffer for INI reads
    wchar_t buffer[1024] = { 0 };

    // Load and parse symbols
    GetPrivateProfileStringW(L"Settings", L"Symbols", L"AAPL,MSFT,BTC-USD",
        buffer, _countof(buffer), configPath.c_str());

    symbols.clear();
    std::wstringstream ss(buffer);
    std::wstring token;
    while (std::getline(ss, token, L',')) {
        // Trim whitespace
        size_t start = token.find_first_not_of(L" \t");
        size_t end = token.find_last_not_of(L" \t");
        if (start != std::wstring::npos && end != std::wstring::npos) {
            token = token.substr(start, end - start + 1);
        }
        if (!token.empty()) {
            symbols.push_back(token);
        }
    }

    // Load and clamp settings using std::clamp<int>
    refreshInterval = std::clamp<int>(
        GetPrivateProfileIntW(L"Settings", L"RefreshInterval", 60, configPath.c_str()),
        10, 3600
    );

    scrollSpeed = std::clamp<int>(
        GetPrivateProfileIntW(L"Settings", L"ScrollSpeed", 5, configPath.c_str()),
        1, 20
    );

    GetPrivateProfileStringW(L"Settings", L"ColorScheme", L"Green",
        buffer, _countof(buffer), configPath.c_str());
    colorScheme = buffer;

    windowHeight = std::clamp<int>(
        GetPrivateProfileIntW(L"Settings", L"WindowHeight", 30, configPath.c_str()),
        20, 200
    );

    fontSize = std::clamp<int>(
        GetPrivateProfileIntW(L"Settings", L"FontSize", 16, configPath.c_str()),
        8, 72
    );
}

void ConfigManager::SaveConfig() {
    std::wstring configPath = GetConfigPath();

    // Build comma-separated symbols string
    std::wstring symbolsStr;
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i > 0) symbolsStr += L",";
        symbolsStr += symbols[i];
    }

    // Write string values
    WritePrivateProfileStringW(L"Settings", L"Symbols", symbolsStr.c_str(), configPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"ColorScheme", colorScheme.c_str(), configPath.c_str());

    // Write numeric values using buffer
    wchar_t numBuffer[32] = { 0 };

    swprintf_s(numBuffer, _countof(numBuffer), L"%d", refreshInterval);
    WritePrivateProfileStringW(L"Settings", L"RefreshInterval", numBuffer, configPath.c_str());

    swprintf_s(numBuffer, _countof(numBuffer), L"%d", scrollSpeed);
    WritePrivateProfileStringW(L"Settings", L"ScrollSpeed", numBuffer, configPath.c_str());

    swprintf_s(numBuffer, _countof(numBuffer), L"%d", windowHeight);
    WritePrivateProfileStringW(L"Settings", L"WindowHeight", numBuffer, configPath.c_str());

    swprintf_s(numBuffer, _countof(numBuffer), L"%d", fontSize);
    WritePrivateProfileStringW(L"Settings", L"FontSize", numBuffer, configPath.c_str());
}