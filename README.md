<div align="center">

# SkinAPI

**Real-time CS2 skin market data, budget optimization, and loadout building — powered by C++17**

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake)
![Steam API](https://img.shields.io/badge/data-Steam%20Market-171a21?logo=steam)
![License](https://img.shields.io/badge/license-MIT-green)

[Live Demo](https://jakeeup.github.io/cs-skin-api/) · [Getting Started](#getting-started) · [API Reference](#api-reference)

</div>

---

## Overview

SkinAPI is a REST API that pulls live CS2 skin pricing from the Steam Community Market and exposes search, price lookup, budget optimization, and full loadout building through a clean JSON interface. The frontend provides a responsive dark-themed UI with grid/list views and client-side filtering.

<!-- Replace with a screenshot of the full app UI -->
![App Overview](Screenshot.png)

---

## Features

### Market Search
Search CS2 skins by weapon name with live Steam Market data. Filter results by price range, wear condition (FN/MW/FT/WW/BS), and StatTrak status. Toggle between grid and list views.

<!-- Replace with a GIF showing a search being performed and filters being applied -->
![Market Search Demo](assets/search-demo.gif)

### Budget Optimizer
Enter a dollar budget and a weapon — the optimizer fetches available skins and runs a **0/1 knapsack algorithm** to select the combination that maximizes total value without exceeding your budget. Uses dynamic programming for budgets up to $500, with a greedy fallback for larger inputs.

<!-- Replace with a GIF showing the budget optimizer in action -->
![Budget Optimizer Demo](assets/budget-demo.gif)

### Loadout Builder
Pick T or CT side and set separate budgets for primary weapons, secondary weapons, knife, and gloves. A **round-robin interleaving algorithm** ensures variety across weapon types — you won't get five AK-47 skins when you wanted a diverse loadout.

<!-- Replace with a GIF showing the loadout builder for T and CT side -->
![Loadout Builder Demo](assets/loadout-demo.gif)

---

## Tech Stack

| Layer | Technology | Purpose |
|-------|-----------|---------|
| **Backend** | C++17 | Server, algorithms, API logic |
| **HTTP Framework** | [Crow](https://crowcpp.org/) | Routing, CORS, JSON responses |
| **HTTP Client** | libcurl | Steam Market API requests |
| **JSON** | [nlohmann/json](https://github.com/nlohmann/json) | Parsing and serialization |
| **Build** | CMake 3.20+ | Cross-platform builds |
| **Frontend** | Vanilla JS / HTML / CSS | UI with no framework dependencies |

---

## Architecture

```
┌─────────────┐       HTTP        ┌──────────────────────────────────┐
│   Browser    │ ◄──────────────► │         Crow HTTP Server         │
│  (index.html │                  │           port 8080              │
│   app.js)    │                  ├──────────────────────────────────┤
└─────────────┘                   │  /search    → fetchQuery()       │
                                  │  /price     → fetchURL()         │
                                  │  /budget    → knapsackOptimize() │
                                  │  /loadout   → fetchSlotOptions() │
                                  ├──────────────────────────────────┤
                                  │         libcurl + rate limiter   │
                                  └────────────┬─────────────────────┘
                                               │ HTTPS
                                  ┌────────────▼─────────────────────┐
                                  │   Steam Community Market API     │
                                  └──────────────────────────────────┘
```

---

## Getting Started

### Prerequisites

- CMake 3.20+
- C++17 compiler (GCC, Clang, or MSVC)
- libcurl development headers

### Build & Run

**Windows (MSYS2/MinGW)**
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
./cs-skin-api.exe
```

**Linux / macOS**
```bash
mkdir build && cd build
cmake ..
cmake --build .
./cs-skin-api
```

The server starts on `http://localhost:8080`. Open `index.html` in a browser to use the UI.

---

## API Reference

### `GET /health`

Returns server status.

```json
{ "status": "ok", "message": "CS Skin API is alive" }
```

---

### `GET /search`

Search for CS2 skins by name with optional price range filtering.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `q` | string | Yes | Weapon or skin name |
| `min` | float | No | Minimum price in USD (default: 0) |
| `max` | float | No | Maximum price in USD (default: 999999) |

**Example:** `GET /search?q=AK-47+Redline&min=10&max=100`

<details>
<summary>Response</summary>

```json
{
  "total_count": 8,
  "results": [
    {
      "name": "AK-47 | Redline (Field-Tested)",
      "hash_name": "AK-47 | Redline (Field-Tested)",
      "sell_listings": 803,
      "sell_price": 4823,
      "sell_price_text": "$48.23",
      "sale_price_text": "",
      "icon_url": "https://community.akamai.steamstatic.com/economy/image/...",
      "market_url": "https://steamcommunity.com/market/listings/730/..."
    }
  ]
}
```
</details>

---

### `GET /price`

Get price overview for a specific skin.

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `name` | string | Yes | Market hash name of the skin |

**Example:** `GET /price?name=AK-47+Redline+(Field-Tested)`

<details>
<summary>Response</summary>

```json
{
  "name": "AK-47 | Redline (Field-Tested)",
  "lowest_price": "$45.00",
  "median_price": "$48.23",
  "volume": "342"
}
```
</details>

---

### `POST /budget/optimize`

Select the optimal combination of skins within a budget using a 0/1 knapsack algorithm.

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `budget` | float | Yes | Maximum budget in USD (max $10,000) |
| `query` | string | Yes | Weapon or skin name to search |

<details>
<summary>Request / Response</summary>

**Request:**
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
  "skins_found": 42,
  "skins_selected": 2,
  "algorithm": "knapsack_dp",
  "skins": [
    { "name": "AK-47 | Redline (Field-Tested)", "price": "$48.23", "price_cents": 4823, "listings": 803 },
    { "name": "AK-47 | Redline (Battle-Scarred)", "price": "$39.94", "price_cents": 3994, "listings": 64 }
  ]
}
```
</details>

---

### `POST /loadout/build`

Build a full loadout for T or CT side with per-slot budgets.

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `side` | string | Yes | `"T"` or `"CT"` |
| `weapons_budget` | float | Yes | Budget split between primary and secondary |
| `knife_budget` | float | No | Budget for knife slot (0 = skip) |
| `gloves_budget` | float | No | Budget for gloves slot (0 = skip) |

<details>
<summary>Request / Response</summary>

**Request:**
```json
{
  "side": "T",
  "weapons_budget": 100.00,
  "knife_budget": 50.00,
  "gloves_budget": 30.00
}
```

**Response:**
```json
{
  "side": "T",
  "weapons_budget": 100.00,
  "knife_budget": 50.00,
  "gloves_budget": 30.00,
  "slots": {
    "primary": [{ "name": "AK-47 | Redline (FT)", "price": "$48.23", "price_cents": 4823, "listings": 803 }],
    "secondary": [{ "name": "Glock-18 | Fade (FN)", "price": "$42.10", "price_cents": 4210, "listings": 12 }],
    "knife": [{ "name": "Gut Knife | Doppler (FN)", "price": "$49.99", "price_cents": 4999, "listings": 5 }],
    "gloves": [{ "name": "Sport Gloves | Arid (FT)", "price": "$28.50", "price_cents": 2850, "listings": 8 }]
  }
}
```
</details>

---

## Project Structure

```
cs-skin-api/
├── src/
│   └── main.cpp           # API server, knapsack algorithm, Steam fetcher
├── index.html              # Frontend UI
├── app.js                  # Client-side logic and API calls
├── style.css               # Dark theme styling
├── CMakeLists.txt          # Build configuration
├── third_party/
│   └── crow_all.h          # Crow HTTP framework (single header)
└── assets/                 # Screenshots and demo GIFs
    ├── search-demo.gif
    ├── budget-demo.gif
    └── loadout-demo.gif
```
