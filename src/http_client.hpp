#pragma once
#include <string>
#include <vector>

struct HttpResponse {
    long        status = 0;   // 0 = transport failure
    std::string body;
};

// Sets the CA bundle used to verify HTTPS certificates. Empty = libcurl default.
void setHttpCaBundle(const std::string& path);

// Percent-encodes a string for use in a URL query component.
std::string urlEncode(const std::string& str);

// Performs an HTTPS GET. Pass secrets as headers, never in the URL, since
// URLs are logged on failure.
HttpResponse httpGet(const std::string& url, const std::vector<std::string>& headers = {});

// Convenience wrapper: body on HTTP 2xx, "" otherwise.
std::string fetchURL(const std::string& url);
