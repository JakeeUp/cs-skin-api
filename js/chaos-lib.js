// Pure helpers for chaos.js (no DOM).
const pad = (n) => String(n).padStart(2, '0');

export function formatCountdown(ms) {
    const total = Math.max(0, Math.floor((Number(ms) || 0) / 1000));
    const d = Math.floor(total / 86400);
    const h = Math.floor((total % 86400) / 3600);
    const m = Math.floor((total % 3600) / 60);
    const s = total % 60;
    return `${pad(d)}D ${pad(h)}H ${pad(m)}M ${pad(s)}S`;
}

// Next target after `now`: a pointless 3-9 day span, chosen from rnd() in [0,1).
export function nextTarget(now, rnd = Math.random) {
    const days = 3 + Math.floor(rnd() * 7);
    return now + days * 86400000 + Math.floor(rnd() * 86400000);
}

// Irregular climb: usually small, sometimes a burst.
export function stepCounter(value, rnd = Math.random) {
    const r = rnd();
    const inc = r < 0.1 ? 40 + Math.floor(rnd() * 60) : 1 + Math.floor(r * 7);
    return value + inc;
}

// Rate limit for popups: first after 20s, >=45s apart, max 3.
export function canShowPopup({ now, start, last, shown, homeActive, open }, opts = {}) {
    const { first = 20000, gap = 45000, max = 3 } = opts;
    if (!homeActive || open || shown >= max) return false;
    if (now - start < first) return false;
    return last == null || now - last >= gap;
}

export const KONAMI = ['ArrowUp', 'ArrowUp', 'ArrowDown', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'ArrowLeft', 'ArrowRight', 'b', 'a'];

// Returns new progress index for a key press.
export function konamiProgress(idx, key) {
    const k = key.length === 1 ? key.toLowerCase() : key;
    if (k === KONAMI[idx]) return idx + 1;
    return k === KONAMI[0] ? 1 : 0;
}

// Click burst: hit when `count` clicks fall within `windowMs`. Returns {times, hit}.
export function clickBurst(times, now, count = 5, windowMs = 2000) {
    const next = [...times, now].filter((t) => now - t <= windowMs);
    return next.length >= count ? { times: [], hit: true } : { times: next, hit: false };
}

export const DEAL_MS = 600000;

// Deterministic deal slot: same skin for everyone within a 10-minute window.
export function dealIndex(now, n) {
    if (!(n > 0)) return -1;
    return Math.floor(now / DEAL_MS) % n;
}

export function msUntilNextDeal(now) {
    return DEAL_MS - (now % DEAL_MS);
}

export function formatMmSs(ms) {
    const t = Math.max(0, Math.ceil((Number(ms) || 0) / 1000));
    return `${pad(Math.floor(t / 60))}:${pad(t % 60)}`;
}

// ms until the next real price refresh (fetched_at + refresh_hours), or null if unknown.
export function nextRefreshMs(fetchedAtIso, refreshHours, now) {
    const t = Date.parse(fetchedAtIso);
    const h = Number(refreshHours);
    if (!fetchedAtIso || !Number.isFinite(t) || !Number.isFinite(h) || h <= 0) return null;
    return Math.max(0, t + h * 3600000 - now);
}
