#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    std::string encoded;
    if (curl) {
        char* out = curl_easy_escape(curl, str.c_str(), static_cast<int>(str.length()));
        if (out) {
            encoded = out;
            curl_free(out);
        }
        curl_easy_cleanup(curl);
    }
    return encoded;
}

std::string fetchURL(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    if (!curl) {
        std::cerr << "[fetchURL] Failed to init CURL" << std::endl;
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // SSL verification disabled: MSYS2/MinGW lacks a system CA bundle, causing
    // certificate validation failures against Steam's CDN. In a production
    // deployment, set CURLOPT_CAINFO to a valid CA bundle path instead.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // Steam requires browser-like headers to serve JSON
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "Accept: application/json, text/javascript, */*; q=0.01");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[fetchURL] CURL error: " << curl_easy_strerror(res)
                  << " | URL: " << url << std::endl;
        return "";
    }

    return response;
}
