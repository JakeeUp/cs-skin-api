const API = 'http://127.0.0.1:8080';

let currentView   = 'grid';
let allResults    = [];
let selectedSide  = 'T';
let demoMode      = false;

// ─── Demo Mode ────────────────────────────────────────────
// When the backend isn't reachable (e.g. GitHub Pages), the UI
// populates with sample data so visitors can explore the interface.

const STEAM_IMG = 'https://community.akamai.steamstatic.com/economy/image/';
const STEAM_MKT = 'https://steamcommunity.com/market/listings/730/';

const DEMO_SKINS = {
    'AK-47': [
        { name: 'AK-47 | Redline (Field-Tested)',         sell_price_text: '$48.23',  sell_listings: 803,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUubOdMATuDKx5KQhvatYXIzKamkL-LYzn5cdVe3r-XodT03VXm80NqZmoyfIbBJBs4NFuC_VK7ye28hMK-tJnIyHZluSh05HfD30vgHGX1a3Y', market_url: STEAM_MKT + 'AK-47%20%7C%20Redline%20%28Field-Tested%29' },
        { name: 'AK-47 | Asiimov (Field-Tested)',         sell_price_text: '$38.91',  sell_listings: 412,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AK-47%20%7C%20Asiimov%20%28Field-Tested%29' },
        { name: 'AK-47 | Neon Rider (Factory New)',       sell_price_text: '$72.50',  sell_listings: 95,   icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNNvoj-x9LeZKL1a7rQlTkAOsAhj7uX8I7w2VHg-EBtYDrwdYLXfRv8MtSX', market_url: STEAM_MKT + 'AK-47%20%7C%20Neon%20Rider%20%28Factory%20New%29' },
        { name: 'StatTrak\u2122 AK-47 | Redline (Field-Tested)', sell_price_text: '$95.00',  sell_listings: 234, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUubOdMATuDKx5KQhvatYXIzKamkL-LYzn5cdVe3r-XodT03VXm80NqZmoyfIbBJBs4NFuC_VK7ye28hMK-tJnIyHZluSh05HfD30vgHGX1a3Y', market_url: STEAM_MKT + 'StatTrak%E2%84%A2%20AK-47%20%7C%20Redline%20%28Field-Tested%29' },
        { name: 'AK-47 | Ice Coaled (Minimal Wear)',      sell_price_text: '$15.44',  sell_listings: 652,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AK-47%20%7C%20Ice%20Coaled%20%28Minimal%20Wear%29' },
        { name: 'AK-47 | Slate (Factory New)',            sell_price_text: '$8.72',   sell_listings: 1204, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AK-47%20%7C%20Slate%20%28Factory%20New%29' },
        { name: 'AK-47 | Bloodsport (Well-Worn)',         sell_price_text: '$22.15',  sell_listings: 188,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AK-47%20%7C%20Bloodsport%20%28Well-Worn%29' },
        { name: 'AK-47 | Phantom Disruptor (Battle-Scarred)', sell_price_text: '$4.18', sell_listings: 932, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AK-47%20%7C%20Phantom%20Disruptor%20%28Battle-Scarred%29' },
    ],
    'AWP': [
        { name: 'AWP | Asiimov (Field-Tested)',            sell_price_text: '$72.45',  sell_listings: 520,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AWP%20%7C%20Asiimov%20%28Field-Tested%29' },
        { name: 'AWP | Hyper Beast (Minimal Wear)',        sell_price_text: '$34.20',  sell_listings: 315,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AWP%20%7C%20Hyper%20Beast%20%28Minimal%20Wear%29' },
        { name: 'AWP | Chromatic Aberration (Factory New)', sell_price_text: '$18.90', sell_listings: 445,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'AWP%20%7C%20Chromatic%20Aberration%20%28Factory%20New%29' },
    ],
    'M4A4': [
        { name: 'M4A4 | Howl (Field-Tested)',              sell_price_text: '$1,850.00', sell_listings: 12, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'M4A4%20%7C%20Howl%20%28Field-Tested%29' },
        { name: 'M4A4 | Neo-Noir (Factory New)',           sell_price_text: '$24.50',    sell_listings: 287, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'M4A4%20%7C%20Neo-Noir%20%28Factory%20New%29' },
        { name: 'M4A4 | The Emperor (Minimal Wear)',      sell_price_text: '$45.80',    sell_listings: 134, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'M4A4%20%7C%20The%20Emperor%20%28Minimal%20Wear%29' },
    ],
    'Desert Eagle': [
        { name: 'Desert Eagle | Blaze (Factory New)',      sell_price_text: '$425.00',  sell_listings: 68,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'Desert%20Eagle%20%7C%20Blaze%20%28Factory%20New%29' },
        { name: 'Desert Eagle | Printstream (Factory New)', sell_price_text: '$52.30', sell_listings: 189, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'Desert%20Eagle%20%7C%20Printstream%20%28Factory%20New%29' },
    ],
    'Glock-18': [
        { name: 'Glock-18 | Fade (Factory New)',           sell_price_text: '$1,250.00', sell_listings: 22, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'Glock-18%20%7C%20Fade%20%28Factory%20New%29' },
        { name: 'Glock-18 | Water Elemental (Field-Tested)', sell_price_text: '$6.45', sell_listings: 1580, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'Glock-18%20%7C%20Water%20Elemental%20%28Field-Tested%29' },
    ],
    'USP-S': [
        { name: 'USP-S | Kill Confirmed (Minimal Wear)',   sell_price_text: '$68.90',  sell_listings: 245, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'USP-S%20%7C%20Kill%20Confirmed%20%28Minimal%20Wear%29' },
        { name: 'USP-S | Neo-Noir (Factory New)',          sell_price_text: '$12.30',  sell_listings: 890, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + 'USP-S%20%7C%20Neo-Noir%20%28Factory%20New%29' },
    ],
    'Karambit': [
        { name: '\u2605 Karambit | Doppler (Factory New)',  sell_price_text: '$850.00', sell_listings: 45,  icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + '%E2%98%85%20Karambit%20%7C%20Doppler%20%28Factory%20New%29' },
        { name: '\u2605 Karambit | Fade (Factory New)',     sell_price_text: '$1,400.00', sell_listings: 18, icon_url: STEAM_IMG + '-9a81dlWLwJ2UXp-WlFABnZADMIel-J_skHicBIfZYTmI-1VLM9CgfIBe71lFdXZBrWMnvBMi-RpQhKc3YnBRiGs0c6AJGJS0p1YRFUu7vyDITu_kxNEvoD-x9Le2qH2ZbrTwmsBu5Up2LiUo92h0Qbm_0VoYD36LIeVdlU-MFnT_le3wu3uhcjWtc-Ky3Ew6Kclm7amA', market_url: STEAM_MKT + '%E2%98%85%20Karambit%20%7C%20Fade%20%28Factory%20New%29' },
    ],
};

// Fallback skins when a weapon isn't in the demo set
const DEMO_DEFAULT = DEMO_SKINS['AK-47'];

function getDemoSkins(weapon) {
    return DEMO_SKINS[weapon] || DEMO_DEFAULT;
}

async function checkServerAvailable() {
    try {
        const res = await fetch(`${API}/health`, { signal: AbortSignal.timeout(2000) });
        return res.ok;
    } catch { return false; }
}

function showDemoBanner() {
    if (document.getElementById('demoBanner')) return;
    const banner = document.createElement('div');
    banner.id = 'demoBanner';
    banner.innerHTML = 'DEMO MODE — Viewing sample data. Run the C++ backend locally for live Steam Market results.';
    document.body.prepend(banner);
}

// ─── Page Navigation ───────────────────────────────────────

function showPage(id) {
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.topnav-link').forEach(l => l.classList.remove('active'));
    document.getElementById('page-' + id).classList.add('active');
    document.querySelector(`.topnav-link[onclick="showPage('${id}')"]`).classList.add('active');
}

// ─── View Toggle ───────────────────────────────────────────

function setView(mode) {
    currentView = mode;
    const grid = document.getElementById('searchResults');
    grid.classList.toggle('list-mode', mode === 'list');
    document.getElementById('viewGrid').classList.toggle('active', mode === 'grid');
    document.getElementById('viewList').classList.toggle('active', mode === 'list');
}

// ─── Helpers ───────────────────────────────────────────────

function escapeHTML(str) {
    const el = document.createElement('span');
    el.textContent = str;
    return el.innerHTML;
}

const WEAR_MAP = {
    'Factory New':    { key: 'fn', label: 'FN' },
    'Minimal Wear':   { key: 'mw', label: 'MW' },
    'Field-Tested':   { key: 'ft', label: 'FT' },
    'Well-Worn':      { key: 'ww', label: 'WW' },
    'Battle-Scarred': { key: 'bs', label: 'BS' },
};

function getWear(name) {
    for (const wear of Object.keys(WEAR_MAP)) {
        if (name.includes(wear)) return wear;
    }
    return '';
}

function getBaseName(name) {
    return name
        .replace(/\s*\(Factory New\)|\s*\(Minimal Wear\)|\s*\(Field-Tested\)|\s*\(Well-Worn\)|\s*\(Battle-Scarred\)/g, '')
        .replace(/^StatTrak™\s*/, '');
}

function wearBadgeHTML(wear) {
    const info = WEAR_MAP[wear];
    if (!info) return '';
    return `<span class="skin-wear-badge wear-${info.key}">${info.label}</span>`;
}

function skinCardHTML(name, priceText, listings, isStatTrak = false, iconUrl = '', marketUrl = '') {
    const wear     = getWear(name);
    const baseName = escapeHTML(getBaseName(name));
    const safePrice = escapeHTML(priceText);
    const safeUrl  = marketUrl && marketUrl.startsWith('https://') ? marketUrl : '#';
    return `
        <div class="skin-card">
            <a href="${safeUrl}" target="_blank" rel="noopener noreferrer" class="skin-link">
                <div class="skin-img-wrap">
                    ${isStatTrak ? `<div class="st-badge">StatTrak\u2122</div>` : ''}
                    ${iconUrl
                        ? `<img src="${escapeHTML(iconUrl)}" alt="${baseName}" loading="lazy" />`
                        : `<div class="skin-img-placeholder">\u25C8</div>`}
                </div>
                <div class="skin-body">
                    <div class="skin-wear-row">
                        ${wearBadgeHTML(wear)}
                        <span class="skin-listings-badge">${Number(listings)} listings</span>
                    </div>
                    <div class="skin-name">${baseName}</div>
                    <div class="skin-price-row">
                        <span class="skin-price">${safePrice}</span>
                        <span class="btn-buy">View \u2192</span>
                    </div>
                </div>
            </a>
        </div>
    `;
}

function renderResults(results, containerId) {
    const div = document.getElementById(containerId);
    if (!results || results.length === 0) {
        div.innerHTML = '<div class="msg-error">No skins found.</div>';
        return;
    }
    div.innerHTML = results.map(skin => {
        const isStatTrak = skin.name.includes('StatTrak');
        return skinCardHTML(
            skin.name,
            skin.sell_price_text || skin.price,
            skin.sell_listings   || skin.listings,
            isStatTrak,
            skin.icon_url,
            skin.market_url
        );
    }).join('');
}

// ─── Market Search ─────────────────────────────────────────

function clearFilters() {
    document.getElementById('minPrice').value = '';
    document.getElementById('maxPrice').value = '';
    document.querySelectorAll('.wear-chip input').forEach(c => c.checked = false);
    document.getElementById('stattrakOnly').checked = false;
}

async function searchSkins() {
    const q        = document.getElementById('weaponSelect').value;
    const minPrice = document.getElementById('minPrice').value || 0;
    const maxPrice = document.getElementById('maxPrice').value || 999999;
    const resultsDiv = document.getElementById('searchResults');
    const countDiv   = document.getElementById('resultsCount');

    if (!q) {
        resultsDiv.innerHTML = '<div class="msg-error">Please select a weapon first.</div>';
        return;
    }

    resultsDiv.innerHTML = '<div class="msg-loading">Fetching market data\u2026</div>';
    countDiv.textContent = '';

    try {
        const res  = await fetch(`${API}/search?q=${encodeURIComponent(q)}&min=${minPrice}&max=${maxPrice}`);
        const data = await res.json();

        if (!data.results || data.results.length === 0) {
            resultsDiv.innerHTML = '<div class="msg-error">No skins found in that price range.</div>';
            return;
        }

        allResults = data.results;

        const checkedWears = [...document.querySelectorAll('.wear-chip input:checked')].map(c => c.value);
        const stattrakOnly = document.getElementById('stattrakOnly').checked;
        let filtered = allResults;

        if (checkedWears.length > 0) {
            filtered = filtered.filter(s => checkedWears.includes(getWear(s.name)));
        }
        if (stattrakOnly) {
            filtered = filtered.filter(s => s.name.includes('StatTrak'));
        }

        countDiv.textContent = `${filtered.length} listing${filtered.length !== 1 ? 's' : ''}`;
        renderResults(filtered, 'searchResults');

    } catch (e) {
        if (!demoMode) {
            demoMode = true;
            showDemoBanner();
        }
        // Fall back to demo data
        const weapon = q || 'AK-47';
        allResults = getDemoSkins(weapon);

        const checkedWears = [...document.querySelectorAll('.wear-chip input:checked')].map(c => c.value);
        const stattrakOnly = document.getElementById('stattrakOnly').checked;
        let filtered = allResults;
        if (checkedWears.length > 0)
            filtered = filtered.filter(s => checkedWears.includes(getWear(s.name)));
        if (stattrakOnly)
            filtered = filtered.filter(s => s.name.includes('StatTrak'));

        countDiv.textContent = `${filtered.length} listing${filtered.length !== 1 ? 's' : ''}`;
        renderResults(filtered, 'searchResults');
    }
}

// ─── Budget Optimizer ──────────────────────────────────────

async function optimizeBudget() {
    const budget     = parseFloat(document.getElementById('budgetInput').value);
    const query      = document.getElementById('budgetWeapon').value;
    const resultsDiv = document.getElementById('budgetResults');
    const summaryDiv = document.getElementById('budgetSummary');

    if (!budget || !query) {
        resultsDiv.innerHTML = '<div class="msg-error">Please enter a budget and select a weapon.</div>';
        return;
    }

    resultsDiv.innerHTML = '<div class="msg-loading">Running optimization\u2026</div>';
    summaryDiv.innerHTML = '';

    try {
        const res  = await fetch(`${API}/budget/optimize`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ budget, query })
        });
        const data = await res.json();

        if (data.error) {
            resultsDiv.innerHTML = `<div class="msg-error">${escapeHTML(data.error)}</div>`;
            return;
        }

        if (!data.skins || data.skins.length === 0) {
            resultsDiv.innerHTML = '<div class="msg-error">No skins found within budget.</div>';
            return;
        }

        summaryDiv.innerHTML = `
            <div class="budget-summary">
                <div class="summary-card">
                    <div class="summary-label">Budget</div>
                    <div class="summary-value">$${data.budget.toFixed(2)}</div>
                </div>
                <div class="summary-card">
                    <div class="summary-label">Total Spent</div>
                    <div class="summary-value positive">$${data.total_spent.toFixed(2)}</div>
                </div>
                <div class="summary-card">
                    <div class="summary-label">Remaining</div>
                    <div class="summary-value">$${data.remaining.toFixed(2)}</div>
                </div>
                <div class="summary-card">
                    <div class="summary-label">Selected</div>
                    <div class="summary-value">${data.skins_selected} of ${data.skins_found}</div>
                </div>
            </div>
        `;

        renderResults(data.skins, 'budgetResults');

    } catch (e) {
        if (!demoMode) {
            demoMode = true;
            showDemoBanner();
        }
        // Demo budget optimizer — simulate knapsack on mock data
        const weapon = query || 'AK-47';
        const mockSkins = getDemoSkins(weapon);
        const parseCents = s => Math.round(parseFloat(s.sell_price_text.replace(/[$,]/g, '')) * 100);

        // Greedy knapsack on demo data
        const sorted = [...mockSkins].sort((a, b) => parseCents(b) - parseCents(a));
        const budgetCents = Math.round(budget * 100);
        let remaining = budgetCents;
        const selected = [];
        for (const s of sorted) {
            const c = parseCents(s);
            if (c <= remaining) { selected.push(s); remaining -= c; }
        }
        const totalSpent = (budgetCents - remaining) / 100;

        summaryDiv.innerHTML = `
            <div class="budget-summary">
                <div class="summary-card"><div class="summary-label">Budget</div><div class="summary-value">$${budget.toFixed(2)}</div></div>
                <div class="summary-card"><div class="summary-label">Total Spent</div><div class="summary-value positive">$${totalSpent.toFixed(2)}</div></div>
                <div class="summary-card"><div class="summary-label">Remaining</div><div class="summary-value">$${(budget - totalSpent).toFixed(2)}</div></div>
                <div class="summary-card"><div class="summary-label">Selected</div><div class="summary-value">${selected.length} of ${mockSkins.length}</div></div>
            </div>`;
        renderResults(selected, 'budgetResults');
    }
}

// ─── Loadout Builder ───────────────────────────────────────

function setSide(side) {
    selectedSide = side;
    document.getElementById('sideT').classList.remove('active', 't-active');
    document.getElementById('sideCT').classList.remove('active', 'ct-active');
    if (side === 'T') {
        document.getElementById('sideT').classList.add('active', 't-active');
    } else {
        document.getElementById('sideCT').classList.add('active', 'ct-active');
    }
}

function updateLoadoutTotal() {
    const w = parseFloat(document.getElementById('loadoutWeapons').value) || 0;
    const k = parseFloat(document.getElementById('loadoutKnife').value)   || 0;
    const g = parseFloat(document.getElementById('loadoutGloves').value)  || 0;
    const total = w + k + g;
    document.getElementById('loadoutTotal').textContent = `Total: $${total.toFixed(2)}`;
}

function slotSectionHTML(slotKey, slotLabel, slotIcon, budgetLabel, skins) {
    const iconClass = slotKey;
    const optionsHTML = skins.map(skin => {
        const isStatTrak = skin.name.includes('StatTrak');
        return skinCardHTML(skin.name, skin.price, skin.listings, isStatTrak, skin.icon_url, skin.market_url);
    }).join('');

    return `
        <div class="slot-section">
            <div class="slot-header">
                <div class="slot-icon ${iconClass}">${slotIcon}</div>
                <div class="slot-title">${escapeHTML(slotLabel)}</div>
                <div class="slot-budget-tag">Budget: ${escapeHTML(budgetLabel)}</div>
            </div>
            <div class="slot-options skin-grid">
                ${optionsHTML || '<div class="msg-error">No options found in this range.</div>'}
            </div>
        </div>
    `;
}

async function buildLoadout() {
    const weapons_budget = parseFloat(document.getElementById('loadoutWeapons').value) || 0;
    const knife_budget   = parseFloat(document.getElementById('loadoutKnife').value)   || 0;
    const gloves_budget  = parseFloat(document.getElementById('loadoutGloves').value)  || 0;
    const resultsDiv     = document.getElementById('loadoutResults');

    if (weapons_budget <= 0) {
        resultsDiv.innerHTML = '<div class="msg-error">Please enter a weapons budget.</div>';
        return;
    }

    resultsDiv.innerHTML = '<div class="msg-loading">Building your loadout\u2026</div>';

    try {
        const res  = await fetch(`${API}/loadout/build`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                side: selectedSide,
                weapons_budget,
                knife_budget,
                gloves_budget
            })
        });
        const data = await res.json();

        if (data.error) {
            resultsDiv.innerHTML = `<div class="msg-error">${escapeHTML(data.error)}</div>`;
            return;
        }

        const slots      = data.slots;
        const perWeapon  = (weapons_budget / 2).toFixed(2);
        let html = '<div class="loadout-slots">';

        if (slots.primary) {
            const label = selectedSide === 'T' ? 'Primary \u2014 AK-47 / SG 553 / Galil AR' : 'Primary \u2014 M4A4 / M4A1-S / AUG';
            html += slotSectionHTML('primary', label, '\uD83D\uDD2B', `$${perWeapon}`, slots.primary);
        }

        if (slots.secondary) {
            const label = selectedSide === 'T' ? 'Secondary \u2014 Glock-18 / Tec-9 / Deagle' : 'Secondary \u2014 USP-S / P2000 / Five-SeveN';
            html += slotSectionHTML('secondary', label, '\uD83D\uDD2B', `$${perWeapon}`, slots.secondary);
        }

        if (slots.knife) {
            html += slotSectionHTML('knife', 'Knife', '\uD83D\uDD2A', `$${knife_budget.toFixed(2)}`, slots.knife);
        }

        if (slots.gloves) {
            html += slotSectionHTML('gloves', 'Gloves', '\uD83E\uDDE4', `$${gloves_budget.toFixed(2)}`, slots.gloves);
        }

        html += '</div>';
        resultsDiv.innerHTML = html;

    } catch (e) {
        if (!demoMode) {
            demoMode = true;
            showDemoBanner();
        }
        // Demo loadout builder
        const perWeapon = weapons_budget / 2;
        let html = '<div class="loadout-slots">';

        if (selectedSide === 'T') {
            html += slotSectionHTML('primary',   'Primary \u2014 AK-47 / SG 553 / Galil AR',    '\uD83D\uDD2B', `$${perWeapon.toFixed(2)}`, getDemoSkins('AK-47').slice(0, 3));
            html += slotSectionHTML('secondary', 'Secondary \u2014 Glock-18 / Tec-9 / Deagle',  '\uD83D\uDD2B', `$${perWeapon.toFixed(2)}`, getDemoSkins('Glock-18'));
        } else {
            html += slotSectionHTML('primary',   'Primary \u2014 M4A4 / M4A1-S / AUG',          '\uD83D\uDD2B', `$${perWeapon.toFixed(2)}`, getDemoSkins('M4A4'));
            html += slotSectionHTML('secondary', 'Secondary \u2014 USP-S / P2000 / Five-SeveN', '\uD83D\uDD2B', `$${perWeapon.toFixed(2)}`, getDemoSkins('USP-S'));
        }

        if (knife_budget > 0)
            html += slotSectionHTML('knife', 'Knife', '\uD83D\uDD2A', `$${knife_budget.toFixed(2)}`, getDemoSkins('Karambit'));
        if (gloves_budget > 0)
            html += slotSectionHTML('gloves', 'Gloves', '\uD83E\uDDE4', `$${gloves_budget.toFixed(2)}`, []);

        html += '</div>';
        resultsDiv.innerHTML = html;
    }
}

// Init
document.addEventListener('DOMContentLoaded', async () => {
    document.getElementById('sideT').classList.add('active', 't-active');

    // Check if backend is available; if not, enter demo mode and show default skins
    const serverUp = await checkServerAvailable();
    if (!serverUp) {
        demoMode = true;
        showDemoBanner();
        // Auto-populate market page with AK-47 demo skins
        document.getElementById('weaponSelect').value = 'AK-47';
        allResults = getDemoSkins('AK-47');
        document.getElementById('resultsCount').textContent = `${allResults.length} listings`;
        renderResults(allResults, 'searchResults');
    }
});
