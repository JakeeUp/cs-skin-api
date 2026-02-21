const API = 'http://127.0.0.1:8080';

let currentView   = 'grid';
let allResults    = [];
let selectedSide  = 'T';

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
    const baseName = getBaseName(name);
    return `
        <div class="skin-card">
            <a href="${marketUrl || '#'}" target="_blank" class="skin-link">
                <div class="skin-img-wrap">
                    ${isStatTrak ? `<div class="st-badge">StatTrak™</div>` : ''}
                    ${iconUrl
                        ? `<img src="${iconUrl}" alt="${baseName}" loading="lazy" />`
                        : `<div class="skin-img-placeholder">◈</div>`}
                </div>
                <div class="skin-body">
                    <div class="skin-wear-row">
                        ${wearBadgeHTML(wear)}
                        <span class="skin-listings-badge">${listings} listings</span>
                    </div>
                    <div class="skin-name">${baseName}</div>
                    <div class="skin-price-row">
                        <span class="skin-price">${priceText}</span>
                        <span class="btn-buy">View →</span>
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

    resultsDiv.innerHTML = '<div class="msg-loading">Fetching market data…</div>';
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
        resultsDiv.innerHTML = '<div class="msg-error">Cannot connect — is the server running?</div>';
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

    resultsDiv.innerHTML = '<div class="msg-loading">Fetching skins within budget…</div>';
    summaryDiv.innerHTML = '';

    try {
        const res  = await fetch(`${API}/budget/optimize`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ budget, query })
        });
        const data = await res.json();

        if (data.error) {
            resultsDiv.innerHTML = `<div class="msg-error">${data.error}</div>`;
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
                    <div class="summary-label">Skins Found</div>
                    <div class="summary-value">${data.skins.length}</div>
                </div>
                <div class="summary-card">
                    <div class="summary-label">Cheapest</div>
                    <div class="summary-value">$${Math.min(...data.skins.map(s => parseFloat(s.price.replace(/[^0-9.]/g, '')))).toFixed(2)}</div>
                </div>
                <div class="summary-card">
                    <div class="summary-label">Most Expensive</div>
                    <div class="summary-value positive">$${Math.max(...data.skins.map(s => parseFloat(s.price.replace(/[^0-9.]/g, '')))).toFixed(2)}</div>
                </div>
            </div>
        `;

        renderResults(data.skins, 'budgetResults');

    } catch (e) {
        resultsDiv.innerHTML = '<div class="msg-error">Cannot connect — is the server running?</div>';
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
                <div class="slot-title">${slotLabel}</div>
                <div class="slot-budget-tag">Budget: ${budgetLabel}</div>
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

    resultsDiv.innerHTML = '<div class="msg-loading">Building your loadout…</div>';

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
            resultsDiv.innerHTML = `<div class="msg-error">${data.error}</div>`;
            return;
        }

        const slots      = data.slots;
        const perWeapon  = (weapons_budget / 2).toFixed(2);
        let html = '<div class="loadout-slots">';

        if (slots.primary) {
            const label = selectedSide === 'T' ? 'Primary — AK-47 / SG 553 / Galil AR' : 'Primary — M4A4 / M4A1-S / AUG';
            html += slotSectionHTML('primary', label, '🔫', `$${perWeapon}`, slots.primary);
        }

        if (slots.secondary) {
            const label = selectedSide === 'T' ? 'Secondary — Glock-18 / Tec-9 / Deagle' : 'Secondary — USP-S / P2000 / Five-SeveN';
            html += slotSectionHTML('secondary', label, '🔫', `$${perWeapon}`, slots.secondary);
        }

        if (slots.knife) {
            html += slotSectionHTML('knife', 'Knife', '🔪', `$${knife_budget.toFixed(2)}`, slots.knife);
        }

        if (slots.gloves) {
            html += slotSectionHTML('gloves', 'Gloves', '🧤', `$${gloves_budget.toFixed(2)}`, slots.gloves);
        }

        html += '</div>';
        resultsDiv.innerHTML = html;

    } catch (e) {
        resultsDiv.innerHTML = '<div class="msg-error">Cannot connect — is the server running?</div>';
    }
}

// Init — set T-side as default active on load
document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('sideT').classList.add('active', 't-active');
});