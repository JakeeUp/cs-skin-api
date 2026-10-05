// Pure helpers: no DOM, no network. Unit-tested in tests/frontend/.

export const WEARS = {
    'Factory New':    { key: 'fn', label: 'FN' },
    'Minimal Wear':   { key: 'mw', label: 'MW' },
    'Field-Tested':   { key: 'ft', label: 'FT' },
    'Well-Worn':      { key: 'ww', label: 'WW' },
    'Battle-Scarred': { key: 'bs', label: 'BS' },
};

export const WEAPON_GROUPS = [
    ['RIFLES', ['AK-47', 'M4A4', 'M4A1-S', 'AUG', 'SG 553', 'FAMAS', 'Galil AR', 'AWP', 'SSG 08', 'SCAR-20', 'G3SG1']],
    ['PISTOLS', ['Desert Eagle', 'Glock-18', 'USP-S', 'P2000', 'P250', 'Five-SeveN', 'Tec-9', 'CZ75-Auto', 'Dual Berettas', 'R8 Revolver']],
    ['SMGS', ['MP9', 'MAC-10', 'MP7', 'MP5-SD', 'UMP-45', 'P90', 'PP-Bizon']],
    ['HEAVY', ['Nova', 'XM1014', 'Sawed-Off', 'MAG-7', 'M249', 'Negev']],
    ['KNIVES', ['Karambit', 'Bayonet', 'Butterfly Knife', 'Flip Knife', 'Gut Knife', 'Huntsman Knife', 'Falchion Knife',
        'Shadow Daggers', 'Bowie Knife', 'Ursus Knife', 'Navaja Knife', 'Stiletto Knife', 'Talon Knife', 'Classic Knife',
        'Paracord Knife', 'Survival Knife', 'Nomad Knife', 'Skeleton Knife']],
    ['GLOVES', ['Sport Gloves', 'Driver Gloves', 'Hand Wraps', 'Moto Gloves', 'Specialist Gloves', 'Hydra Gloves',
        'Bloodhound Gloves', 'Broken Fang Gloves']],
];

export const RARITY_COLORS = {
    'Consumer Grade':   '#b0c3d9',
    'Industrial Grade': '#5e98d9',
    'Mil-Spec Grade':   '#4b69ff',
    'Restricted':       '#8847ff',
    'Classified':       '#d32ce6',
    'Covert':           '#eb4b4b',
    'Contraband':       '#e4ae39',
    'Extraordinary':    '#eb4b4b',
};

export function rarityColor(rarity) {
    return RARITY_COLORS[rarity] || null;
}

export function getWear(name) {
    return Object.keys(WEARS).find(w => name.includes(w)) || '';
}

export function getBaseName(name) {
    return name
        .replace(/\s*\((Factory New|Minimal Wear|Field-Tested|Well-Worn|Battle-Scarred)\)/g, '')
        .replace(/^StatTrak™\s*/, '');
}

export function isStatTrak(name) {
    return name.includes('StatTrak');
}

const usd = new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD' });

/** Format integer cents as "$1,234.50". Non-finite input gives an em dash. */
export function formatMoney(cents) {
    return Number.isFinite(cents) ? usd.format(cents / 100) : '—';
}

/** "$1,250.00" -> 125000. Returns NaN when unparseable. */
export function parsePriceText(text) {
    const n = parseFloat(String(text ?? '').replace(/[^0-9.]/g, ''));
    return Number.isFinite(n) ? Math.round(n * 100) : NaN;
}

/** Map any API skin shape (search result, budget skin, loadout option) to one shape. */
export function normalizeSkin(raw) {
    const cents = [raw.sell_price, raw.price_cents].find(Number.isFinite)
        ?? parsePriceText(raw.sell_price_text ?? raw.price);
    const st = raw.skinstrack && Number.isFinite(raw.skinstrack.price_cents) ? raw.skinstrack : null;
    return {
        name: raw.name,
        hashName: raw.hash_name || raw.name,
        cents,
        listings: Number(raw.sell_listings ?? raw.listings) || 0,
        iconUrl: raw.icon_url || '',
        marketUrl: typeof raw.market_url === 'string' && raw.market_url.startsWith('https://') ? raw.market_url : '',
        rarity: raw.rarity || '',
        skinstrack: st,
    };
}

export function filterSkins(skins, { wears = [], stattrakOnly = false } = {}) {
    return skins.filter(s =>
        (wears.length === 0 || wears.includes(getWear(s.name))) &&
        (!stattrakOnly || isStatTrak(s.name)));
}

export const SORTS = {
    'price-asc':  (a, b) => a.cents - b.cents,
    'price-desc': (a, b) => b.cents - a.cents,
    'name':       (a, b) => getBaseName(a.name).localeCompare(getBaseName(b.name)),
    'listings':   (a, b) => b.listings - a.listings,
};

/** Returns a new sorted array; unknown key keeps API order. */
export function sortSkins(skins, key) {
    const cmp = SORTS[key];
    return cmp ? [...skins].sort(cmp) : [...skins];
}

/** Percent SkinsTrack is below Steam (positive = cheaper elsewhere). null if not comparable. */
export function discountPct(steamCents, trackCents) {
    if (!(steamCents > 0) || !Number.isFinite(trackCents)) return null;
    return Math.round(((steamCents - trackCents) / steamCents) * 100);
}

/** "3h ago" style label. */
export function timeAgo(iso, now = Date.now()) {
    const t = Date.parse(iso);
    if (!Number.isFinite(t)) return '';
    const s = Math.max(0, Math.round((now - t) / 1000));
    if (s < 60) return 'just now';
    if (s < 3600) return `${Math.floor(s / 60)}m ago`;
    if (s < 86400) return `${Math.floor(s / 3600)}h ago`;
    return `${Math.floor(s / 86400)}d ago`;
}

export function clampPct(n) {
    return Math.min(100, Math.max(0, Number(n) || 0));
}
