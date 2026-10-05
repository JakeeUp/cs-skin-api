# Security Policy

## Secrets & Configuration

### What Secrets Does SkinAPI Use?

- **SkinsTrack API Key** (`SKINSTRACK_API_KEY`): Allows querying third-party price data. Free plan: 50 calls/month.

### Where Are They Stored?

- **Local development**: In `.env` file (gitignored; never committed)
- **Environment variables**: Real env vars override `.env` at runtime
- **Server logs**: The API key is never logged. At startup, logs only print `skinstrack_key=set` or `skinstrack_key=missing`

### How to Manage Secrets

1. Copy `.env.example` to `.env`:
   ```bash
   cp .env.example .env
   ```

2. Fill in the `SKINSTRACK_API_KEY` from [skinstrack.com/api-pricing](https://skinstrack.com/api-pricing)

3. Never commit `.env`. It is listed in `.gitignore`.

4. On a server, set environment variables securely (e.g., via GitHub Secrets for CI, or a secrets manager for production).

## What Does the Server Do?

### TLS & HTTPS

- The frontend communicates with the Steam API over HTTPS. libcurl verifies the certificate chain by default.
- An optional `CURL_CA_BUNDLE` environment variable can specify a custom CA bundle path (e.g., for corporate proxies or MSYS2 setups).

### API Key Handling

- The SkinsTrack API key is sent **only in HTTP headers** to the third-party service, never exposed in URLs or logs.
- The key is never transmitted to the browser or logged.

### Input Validation

All user inputs are validated:
- **Query strings**: Max 64 characters; stripped of leading/trailing whitespace
- **Dollar amounts**: 0–10,000 USD; must be valid floats
- **JSON bodies**: Max 4 KB total size; must be valid JSON objects
- **Side parameter**: Only "T" or "CT" accepted

Invalid inputs return a 400 error with a descriptive message, e.g., `{"error": "budget must be 0–10000"}`.

### Response Headers

All JSON responses include:
- `Content-Type: application/json`
- `Cache-Control: no-store` (prevents caching sensitive data)
- `X-Content-Type-Options: nosniff` (prevents MIME type sniffing attacks)

### CORS Policy

The `ALLOWED_ORIGIN` configuration restricts which browser origins can call the API. This is enforced at the HTTP level via the `Access-Control-Allow-Origin` header. Set this to your exact frontend origin, e.g.:
- Local dev: `http://127.0.0.1:5500`
- GitHub Pages: `https://jakeeup.github.io`

## Known Limits & Threats

### No Rate Limiting

The API does not rate-limit incoming requests. A malicious client can hammer the server or the Steam API. In production, consider:
- Implementing per-IP rate limiting (e.g., using a reverse proxy like nginx)
- Adding request timeouts to protect against slow clients
- Monitoring API usage for abuse patterns

### Steam API Scraping

SkinAPI scrapes the Steam Community Market API, which is:
- **Unofficial**: Not part of Valve's public API contract; subject to change without notice
- **Unofficial access**: May violate Steam's ToS; use at your own risk
- **Rate limited by Steam**: We implement backoff, but extended scraping may trigger temporary IP bans

### No Input Sanitization for Display

Skin names from Steam are inserted into the frontend DOM without HTML-escaping. If Steam's API ever returned malicious content, the frontend could be vulnerable. In practice, Steam sanitizes all user-generated skin names, so this is low-risk. To be safer, the frontend could use textContent instead of innerHTML.

### Cache Staleness

The SkinsTrack price cache is refreshed every `SKINSTRACK_REFRESH_HOURS` (default 24). During that window, prices may be stale. The response includes an `updated_at` timestamp so clients can decide if the data is fresh enough for their use case.

### No Authentication

The API is public. Anyone can call `/search`, `/budget/optimize`, `/loadout/build`, etc. This is by design for a demo app, but a production service should consider:
- API key authentication for rate-limit tracking
- User-level quotas
- A real backend database for user state

## Reporting Security Issues

If you discover a security vulnerability, **do not open a public issue**. Instead:

1. Report it privately through the repository's **Security → Report a vulnerability** tab on GitHub
2. Include steps to reproduce and potential impact
3. Allow 30 days for a fix and coordinated disclosure

## Compliance & Disclaimers

- **No warranty**: SkinAPI is provided as-is, without warranty. Use at your own risk.
- **Steam Terms of Service**: SkinAPI scrapes Steam's unofficial API. Ensure your use complies with Valve's ToS.
- **Cookies / Tracking**: The frontend stores UI state (grid/list view, filters) in localStorage. No tracking or analytics are included.
- **Data Retention**: The server does not store user requests or session data. The SkinsTrack cache is temporary and can be cleared by deleting the cache file.
