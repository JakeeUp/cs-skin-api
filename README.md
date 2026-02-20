# CS Skin API

A REST API built in C++ that provides real-time CS2 skin market data from the Steam Marketplace, including a budget optimization engine powered by a knapsack algorithm.

## Features

- **Skin Search** — Search CS2 skins by name with live Steam Market data
- **Price Lookup** — Get current market prices for any skin
- **Budget Optimizer** — Given a budget, finds the best combination of skins to buy using a knapsack algorithm

## Tech Stack

- **C++17**
- **Crow** — C++ HTTP framework
- **libcurl** — HTTP requests to Steam Market API
- **nlohmann/json** — JSON parsing
- **CMake** — Build system

## Endpoints

### GET /health
Returns server status.

### GET /search?q={query}
Search for CS2 skins by name.

**Example:**
```
GET /search?q=AK-47+Redline
```

**Response:**
```json
{
  "total_count": 8,
  "results": [
    {
      "name": "AK-47 | Redline (Field-Tested)",
      "sell_listings": 803,
      "sell_price": 4823,
      "sell_price_text": "$48.23"
    }
  ]
}
```

### GET /price?name={market_hash_name}
Get price overview for a specific skin.

### POST /budget/optimize
Find the best combination of skins within a given budget using a knapsack algorithm.

**Request Body:**
```json
{
  "budget": 100.00,
  "query": "AK-47 Redline"
}
```

**Response:**
```json
{
  "budget": 100.00,
  "total_spent": 88.17,
  "remaining": 11.83,
  "skins": [
    { "name": "AK-47 | Redline (Field-Tested)", "price": "$48.23", "listings": 803 },
    { "name": "AK-47 | Redline (Battle-Scarred)", "price": "$39.94", "listings": 64 }
  ]
}
```

## Building

### Prerequisites
- CMake 3.20+
- GCC (via MSYS2 on Windows)
- libcurl
- nlohmann/json

### Build Steps
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
.\cs-skin-api.exe
```

Server runs on `http://localhost:8080`