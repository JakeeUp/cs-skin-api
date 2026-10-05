import * as api from './api.js';
import { getDemoSkins } from './demo-data.js';
import {
    WEARS, WEAPON_GROUPS, getWear, getBaseName, isStatTrak, formatMoney,
    normalizeSkin, filterSkins,
} from './lib.js';

const $ = id => document.getElementById(id);

const state = {
    side: 'T',
    demoMode: false,
    skins: [],
};

// ─── DOM helper ────────────────────────────────────────────
// Builds elements with textContent only, so no HTML escaping is needed.

function h(tag, attrs = {}, ...children) {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) {
        if (v === false || v == null) continue;
        if (k === 'class') el.className = v;
        else el.setAttribute(k, v === true ? '' : v);
    }
    for (const c of children.flat()) {
        if (c == null || c === false) continue;
        el.append(c.nodeType ? c : document.createTextNode(c));
    }
    return el;
}

function setMessage(container, text, cls = 'msg-error') {
    container.replaceChildren(h('div', { class: cls }, text));
}

// ─── Demo mode ─────────────────────────────────────────────
// When the backend isn't reachable (e.g. GitHub Pages), the UI
// populates with sample data so visitors can explore the interface.

function enterDemoMode() {
    if (state.demoMode) return;
    state.demoMode = true;
    document.body.prepend(h('div', { id: 'demoBanner' },
        'DEMO MODE — Viewing sample data. Run the C++ backend locally for live Steam Market results.'));
}

/** Run an API call; on network failure switch to demo mode and return null. */
async function tryApi(call, onError) {
    try {
        return await call();
    } catch (e) {
        if (e instanceof api.ApiError) { onError(e.message); return undefined; }
        enterDemoMode();
        return null;
    }
}

// ─── Page navigation / view toggle ─────────────────────────

function showPage(id) {
    document.querySelectorAll('.page').forEach(p => p.classList.toggle('active', p.id === 'page-' + id));
    document.querySelectorAll('.topnav-link').forEach(l => l.classList.toggle('active', l.dataset.page === id));
}

function setView(mode) {
    $('searchResults').classList.toggle('list-mode', mode === 'list');
    $('viewGrid').classList.toggle('active', mode === 'grid');
    $('viewList').classList.toggle('active', mode === 'list');
}

// ─── Rendering ─────────────────────────────────────────────

function wearBadge(wear) {
    const info = WEARS[wear];
    return info ? h('span', { class: `skin-wear-badge wear-${info.key}` }, info.label) : null;
}

function skinCard(skin) {
    const wear = getWear(skin.name);
    const baseName = getBaseName(skin.name);
    return h('div', { class: 'skin-card' },
        h('a', {
            href: skin.marketUrl || '#', target: '_blank', rel: 'noopener noreferrer', class: 'skin-link',
        },
            h('div', { class: 'skin-img-wrap' },
                isStatTrak(skin.name) && h('div', { class: 'st-badge' }, 'StatTrak™'),
                skin.iconUrl
                    ? h('img', { src: skin.iconUrl, alt: baseName, loading: 'lazy' })
                    : h('div', { class: 'skin-img-placeholder' }, '◈')),
            h('div', { class: 'skin-body' },
                h('div', { class: 'skin-wear-row' },
                    wearBadge(wear),
                    h('span', { class: 'skin-listings-badge' }, `${skin.listings} listings`)),
                h('div', { class: 'skin-name' }, baseName),
                h('div', { class: 'skin-price-row' },
                    h('span', { class: 'skin-price' }, formatMoney(skin.cents)),
                    h('span', { class: 'btn-buy' }, 'View →')))));
}

function renderSkins(rawSkins, container) {
    if (!rawSkins || rawSkins.length === 0) return setMessage(container, 'No skins found.');
    container.replaceChildren(...rawSkins.map(s => skinCard(normalizeSkin(s))));
}

// ─── Market search ─────────────────────────────────────────

function currentFilters() {
    return {
        wears: [...document.querySelectorAll('.wear-chip input:checked')].map(c => c.value),
        stattrakOnly: $('stattrakOnly').checked,
    };
}

function showFiltered(raw) {
    const filtered = filterSkins(raw.map(normalizeSkin), currentFilters());
    $('resultsCount').textContent = `${filtered.length} listing${filtered.length !== 1 ? 's' : ''}`;
    // filtered holds normalized skins; skinCard consumes those directly
    const grid = $('searchResults');
    if (filtered.length === 0) return setMessage(grid, 'No skins found.');
    grid.replaceChildren(...filtered.map(skinCard));
}

function clearFilters() {
    $('minPrice').value = '';
    $('maxPrice').value = '';
    document.querySelectorAll('.wear-chip input').forEach(c => { c.checked = false; });
    $('stattrakOnly').checked = false;
}

async function searchSkins() {
    const q = $('weaponSelect').value;
    const min = $('minPrice').value || 0;
    const max = $('maxPrice').value || 999999;
    const grid = $('searchResults');

    if (!q) return setMessage(grid, 'Please select a weapon first.');

    setMessage(grid, 'Fetching market data…', 'msg-loading');
    $('resultsCount').textContent = '';

    const data = await tryApi(() => api.search(q, min, max), msg => setMessage(grid, msg));
    if (data === undefined) return;
    if (data === null) {
        state.skins = getDemoSkins(q);
    } else {
        if (!data.results || data.results.length === 0) {
            return setMessage(grid, 'No skins found in that price range.');
        }
        state.skins = data.results;
    }
    showFiltered(state.skins);
}

// ─── Budget optimizer ──────────────────────────────────────

function summaryCard(label, value, positive = false) {
    return h('div', { class: 'summary-card' },
        h('div', { class: 'summary-label' }, label),
        h('div', { class: 'summary-value' + (positive ? ' positive' : '') }, value));
}

function renderSummary({ budget, spent, remaining, selected, found }) {
    $('budgetSummary').replaceChildren(h('div', { class: 'budget-summary' },
        summaryCard('Budget', formatMoney(budget * 100)),
        summaryCard('Total Spent', formatMoney(spent * 100), true),
        summaryCard('Remaining', formatMoney(remaining * 100)),
        summaryCard('Selected', `${selected} of ${found}`)));
}

/** Greedy knapsack over demo data (stand-in for the backend optimizer). */
function demoOptimize(budget, weapon) {
    const skins = getDemoSkins(weapon || 'AK-47');
    const sorted = skins.map(normalizeSkin).sort((a, b) => b.cents - a.cents);
    let remaining = Math.round(budget * 100);
    const selected = [];
    for (const s of sorted) {
        if (s.cents <= remaining) { selected.push(s); remaining -= s.cents; }
    }
    const spent = (Math.round(budget * 100) - remaining) / 100;
    return { selected, spent, found: skins.length };
}

async function optimizeBudget() {
    const budget = parseFloat($('budgetInput').value);
    const query = $('budgetWeapon').value;
    const results = $('budgetResults');

    if (!budget || !query) return setMessage(results, 'Please enter a budget and select a weapon.');

    setMessage(results, 'Running optimization…', 'msg-loading');
    $('budgetSummary').replaceChildren();

    const data = await tryApi(() => api.optimizeBudget(budget, query), msg => setMessage(results, msg));
    if (data === undefined) return;

    if (data === null) {
        const d = demoOptimize(budget, query);
        renderSummary({ budget, spent: d.spent, remaining: budget - d.spent, selected: d.selected.length, found: d.found });
        results.replaceChildren(...d.selected.map(skinCard));
        return;
    }
    if (!data.skins || data.skins.length === 0) return setMessage(results, 'No skins found within budget.');
    renderSummary({
        budget: data.budget, spent: data.total_spent, remaining: data.remaining,
        selected: data.skins_selected, found: data.skins_found,
    });
    renderSkins(data.skins, results);
}

// ─── Loadout builder ───────────────────────────────────────

function setSide(side) {
    state.side = side;
    $('sideT').classList.toggle('active', side === 'T');
    $('sideT').classList.toggle('t-active', side === 'T');
    $('sideCT').classList.toggle('active', side === 'CT');
    $('sideCT').classList.toggle('ct-active', side === 'CT');
}

const num = id => parseFloat($(id).value) || 0;

function updateLoadoutTotal() {
    const total = num('loadoutWeapons') + num('loadoutKnife') + num('loadoutGloves');
    $('loadoutTotal').textContent = `Total: ${formatMoney(total * 100)}`;
}

function slotSection(slotKey, label, icon, budget, skins) {
    return h('div', { class: 'slot-section' },
        h('div', { class: 'slot-header' },
            h('div', { class: `slot-icon ${slotKey}` }, icon),
            h('div', { class: 'slot-title' }, label),
            h('div', { class: 'slot-budget-tag' }, `Budget: ${formatMoney(budget * 100)}`)),
        h('div', { class: 'slot-options skin-grid' },
            skins.length ? skins.map(s => skinCard(normalizeSkin(s)))
                : h('div', { class: 'msg-error' }, 'No options found in this range.')));
}

const SLOT_LABELS = {
    T:  { primary: 'Primary — AK-47 / SG 553 / Galil AR', secondary: 'Secondary — Glock-18 / Tec-9 / Deagle' },
    CT: { primary: 'Primary — M4A4 / M4A1-S / AUG', secondary: 'Secondary — USP-S / P2000 / Five-SeveN' },
};
const GUN = '🔫';

function demoSlots(side, knifeBudget, glovesBudget) {
    const slots = side === 'T'
        ? { primary: getDemoSkins('AK-47').slice(0, 3), secondary: getDemoSkins('Glock-18') }
        : { primary: getDemoSkins('M4A4'), secondary: getDemoSkins('USP-S') };
    if (knifeBudget > 0) slots.knife = getDemoSkins('Karambit');
    if (glovesBudget > 0) slots.gloves = [];
    return slots;
}

async function buildLoadout() {
    const weapons = num('loadoutWeapons');
    const knife = num('loadoutKnife');
    const gloves = num('loadoutGloves');
    const results = $('loadoutResults');

    if (weapons <= 0) return setMessage(results, 'Please enter a weapons budget.');
    setMessage(results, 'Building your loadout…', 'msg-loading');

    const data = await tryApi(
        () => api.buildLoadout({ side: state.side, weapons_budget: weapons, knife_budget: knife, gloves_budget: gloves }),
        msg => setMessage(results, msg));
    if (data === undefined) return;

    const slots = data === null ? demoSlots(state.side, knife, gloves) : data.slots;
    const per = weapons / 2;
    const labels = SLOT_LABELS[state.side];
    const sections = [];
    if (slots.primary)   sections.push(slotSection('primary', labels.primary, GUN, per, slots.primary));
    if (slots.secondary) sections.push(slotSection('secondary', labels.secondary, GUN, per, slots.secondary));
    if (slots.knife)     sections.push(slotSection('knife', 'Knife', '🔪', knife, slots.knife));
    if (slots.gloves)    sections.push(slotSection('gloves', 'Gloves', '🧤', gloves, slots.gloves));
    results.replaceChildren(h('div', { class: 'loadout-slots' }, sections));
}

// ─── Init ──────────────────────────────────────────────────

function fillWeaponSelect(select) {
    for (const [group, weapons] of WEAPON_GROUPS) {
        select.append(h('optgroup', { label: group }, weapons.map(w => h('option', { value: w }, w))));
    }
}

function init() {
    fillWeaponSelect($('weaponSelect'));
    fillWeaponSelect($('budgetWeapon'));

    document.querySelectorAll('.topnav-link').forEach(l => l.addEventListener('click', () => showPage(l.dataset.page)));
    $('searchBtn').addEventListener('click', searchSkins);
    $('applyBtn').addEventListener('click', searchSkins);
    $('resetBtn').addEventListener('click', clearFilters);
    $('viewGrid').addEventListener('click', () => setView('grid'));
    $('viewList').addEventListener('click', () => setView('list'));
    $('optimizeBtn').addEventListener('click', optimizeBudget);
    $('buildBtn').addEventListener('click', buildLoadout);
    $('sideT').addEventListener('click', () => setSide('T'));
    $('sideCT').addEventListener('click', () => setSide('CT'));
    ['loadoutWeapons', 'loadoutKnife', 'loadoutGloves'].forEach(id => $(id).addEventListener('input', updateLoadoutTotal));

    setSide('T');

    api.isServerUp().then(up => {
        if (up) return;
        enterDemoMode();
        $('weaponSelect').value = 'AK-47';
        state.skins = getDemoSkins('AK-47');
        showFiltered(state.skins);
    });
}

// Modules are deferred, so the DOM is parsed by now.
init();
