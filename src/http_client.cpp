#include "http_client.hpp"
#include <curl/curl.h>
#include <iostream>

static std::string g_caBundle;

void setHttpCaBundle(const std::string& path) {
    g_caBundle = path;
}

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

HttpResponse httpGet(const std::string& url, const std::vector<std::string>& extraHeaders,
                     long timeoutSeconds) {
    HttpResponse result;
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[httpGet] Failed to init CURL" << std::endl;
        return result;
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &result.body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS,      3L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR,  "https");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // gzip/deflate when offered

    // Certificates are always verified. MSYS2 needs an explicit bundle
    // (CURL_CA_BUNDLE in .env) because it has no system certificate store.
    if (!g_caBundle.empty())
        curl_easy_setopt(curl, CURLOPT_CAINFO, g_caBundle.c_str());

    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // Steam requires browser-like headers to serve JSON
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "Accept: application/json, text/javascript, */*; q=0.01");
    for (const auto& h : extraHeaders)
        headers = curl_slist_append(headers, h.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[httpGet] CURL error: " << curl_easy_strerror(res)
                  << " | URL: " << url << std::endl;
        result.body.clear();
    }
    return result;
}

std::string fetchURL(const std::string& url) {
    HttpResponse r = httpGet(url);
    if (r.status < 200 || r.status >= 300) {
        if (r.status != 0)
            std::cerr << "[fetchURL] HTTP " << r.status << " | URL: " << url << std::endl;
        return "";
    }
    return r.body;
}
