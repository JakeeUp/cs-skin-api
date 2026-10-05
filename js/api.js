// Backend calls. Network failures throw ApiUnavailable (caller falls back to
// demo mode); non-2xx responses throw ApiError carrying the API's message.

const DEFAULT_BASE = 'http://127.0.0.1:8080';

export class ApiUnavailable extends Error {}
export class ApiError extends Error {}

function resolveBase() {
    try {
        const q = new URLSearchParams(location.search).get('api');
        if (q) localStorage.setItem('apiBase', q);
        return (localStorage.getItem('apiBase') || DEFAULT_BASE).replace(/\/+$/, '');
    } catch {
        return DEFAULT_BASE;
    }
}

export const API_BASE = resolveBase();

async function request(path, options, timeoutMs = 15000) {
    let res;
    try {
        res = await fetch(API_BASE + path, { ...options, signal: AbortSignal.timeout(timeoutMs) });
    } catch (e) {
        throw new ApiUnavailable(e.message);
    }
    let body = null;
    try { body = await res.json(); } catch { /* non-JSON body */ }
    if (!res.ok) throw new ApiError((body && body.error) || `Request failed (${res.status})`);
    return body ?? {};
}

const post = (path, payload) => request(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
});

export async function isServerUp() {
    try { await request('/health', undefined, 2000); return true; } catch { return false; }
}

// Empty limits are left off so the server applies none.
export function search(q, min, max) {
    const params = new URLSearchParams({ q });
    if (min !== '') params.set('min', min);
    if (max !== '') params.set('max', max);
    return request(`/search?${params}`);
}
export const optimizeBudget = (budget, query) => post('/budget/optimize', { budget, query });
export const buildLoadout = payload => post('/loadout/build', payload);
export const skinstrackStatus = () => request('/skinstrack/status', undefined, 4000);
