#pragma once
#ifndef API_FETCHER_H
#define API_FETCHER_H

#include <string>

class ApiFetcher {
public:
    static double FetchPrice(const std::wstring& symbol);
};

#endif
