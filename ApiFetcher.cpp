#include "ApiFetcher.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <stdexcept>

#pragma comment(lib, "winhttp.lib")

static std::mutex apiMutex;

double ApiFetcher::FetchPrice(const std::wstring& symbol) {
    std::lock_guard<std::mutex> lock(apiMutex);
    double price = 0.0;

    HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;

    try {
        hSession = WinHttpOpen(
            L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!hSession) throw std::runtime_error("WinHttpOpen failed");

        hConnect = WinHttpConnect(hSession, L"query1.finance.yahoo.com",
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) throw std::runtime_error("WinHttpConnect failed");

        std::wstring path = L"/v8/finance/chart/" + symbol + L"?interval=1d";

        hRequest = WinHttpOpenRequest(
            hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) throw std::runtime_error("WinHttpOpenRequest failed");

        // Add realistic headers
        WinHttpAddRequestHeaders(
            hRequest,
            L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
        );

        WinHttpAddRequestHeaders(
            hRequest,
            L"Accept: application/json",
            (DWORD)-1,
            WINHTTP_ADDREQ_FLAG_ADD
        );

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            throw std::runtime_error("SendRequest failed");
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            throw std::runtime_error("ReceiveResponse failed");
        }

        std::string response;
        DWORD dwSize = 0, dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buf(dwSize);
            if (WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded)) {
                response.append(buf.data(), dwDownloaded);
            }
        } while (dwSize > 0);

        // Debug logging
        if (response.empty()) {
            OutputDebugStringW(L"API: Empty response (likely blocked)\n");
        }
        else if (response.find("crumb") != std::string::npos && response.find("login") != std::string::npos) {
            OutputDebugStringW(L"API: Redirected to login — blocked\n");
        }
        else {
            std::string dbg = "API Response (first 200): " + response.substr(0, 200) + "\n";
            OutputDebugStringA(dbg.c_str());
        }

        // Parse the price from JSON response
        size_t pos = response.find("\"regularMarketPrice\":");
        if (pos != std::string::npos) {
            pos += 21; // Length of "\"regularMarketPrice\":"
            size_t end = response.find_first_of(",}", pos);
            if (end != std::string::npos) {
                std::string priceStr = response.substr(pos, end - pos);
                try {
                    price = std::stod(priceStr);
                }
                catch (...) {
                    OutputDebugStringW(L"Failed to parse price\n");
                }
            }
        }
        else {
            OutputDebugStringW(L"regularMarketPrice not found\n");
        }
    }
    catch (const std::exception& e) {
        std::string error = "Exception in FetchPrice: " + std::string(e.what()) + "\n";
        OutputDebugStringA(error.c_str());
    }
    catch (...) {
        OutputDebugStringW(L"Unknown exception in FetchPrice\n");
    }

    // Cleanup
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return price;
}
