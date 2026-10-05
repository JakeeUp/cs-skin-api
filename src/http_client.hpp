#pragma once
#include <string>

// Percent-encodes a string for use in a URL query component.
std::string urlEncode(const std::string& str);

// Performs an HTTPS GET and returns the body, or "" on transport failure.
std::string fetchURL(const std::string& url);
