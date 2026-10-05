#pragma once
#include <string>

struct Skin {
    std::string name;
    std::string hash_name;
    std::string price_text;
    std::string sale_price_text;
    std::string icon_url;
    std::string market_url;
    int         price_cents;
    int         listings;
};
