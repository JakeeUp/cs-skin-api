import * as api from './api.js';
import { DEMO_SKINS, getDemoSkins } from './demo-data.js';
import {
    WEARS, WEAPON_GROUPS, getWear, getBaseName, isStatTrak, formatMoney,
    normalizeSkin, filterSkins, sortSkins, rarityColor,
    discountPct, clampPct, timeAgo,
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

// ─── Icons ─────────────────────────────────────────────────
// Drawn on a 24px grid with one 1.5px stroke so every icon matches.

const ICON_PATHS = {
    rifle:  ['M2 11h13l2-2h5v3h-5l-1.5 2H12l-1 5H7.5l1-5H2z', 'M5 11V9'],
    pistol: ['M3 7h16v4h-6.5l-1 2H10l-1.2 6H5.5l1.2-6H3z'],
    knife:  ['M3 21l6.5-6.5', 'M9.5 14.5L20 3l-1.5 7.5-6 6z'],
    gloves: ['M7 21v-4.5L4 12.5l1.2-1.2L8 13V5.5a1.2 1.2 0 0 1 2.4 0V11V4.2a1.2 1.2 0 0 1 2.4 0V11V5.5a1.2 1.2 0 0 1 2.4 0V12V8a1.2 1.2 0 0 1 2.4 0v8L16 21z'],
    crosshair: ['M12 3v5M12 16v5M3 12h5M16 12h5', 'M12 5.5a6.5 6.5 0 1 0 0 13 6.5 6.5 0 1 0 0-13z'],
};

function icon(name, size = 18) {
    const NS = 'http://www.w3.org/2000/svg';
    const svg = document.createElementNS(NS, 'svg');
    for (const [k, v] of Object.entries({
        viewBox: '0 0 24 24', width: size, height: size, fill: 'none', stroke: 'currentColor',
        'stroke-width': '1.5', 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'aria-hidden': 'true',
    })) svg.setAttribute(k, v);
    for (const d of ICON_PATHS[name]) {
        const path = document.createElementNS(NS, 'path');
        path.setAttribute('d', d);
        svg.append(path);
    }
    return svg;
}

function setLoading(container, count = 8) {
    const card = () => h('div', { class: 'skin-card skeleton', 'aria-hidden': 'true' },
        h('div', { class: 'sk-img' }), h('div', { class: 'sk-line' }),
        h('div', { class: 'sk-line w60' }), h('div', { class: 'sk-line w40' }));
    container.replaceChildren(
        h('span', { class: 'sr-only', role: 'status' }, 'Loading…'),
        ...Array.from({ length: count }, card));
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
    document.querySelectorAll('.topnav-link').forEach(l => {
        const current = l.dataset.page === id;
        l.classList.toggle('active', current);
        if (current) l.setAttribute('aria-current', 'page');
        else l.removeAttribute('aria-current');
    });
}

function setView(mode) {
    $('searchResults').classList.toggle('list-mode', mode === 'list');
    $('viewGrid').classList.toggle('active', mode === 'grid');
    $('viewList').classList.toggle('active', mode === 'list');
    $('viewGrid').setAttribute('aria-pressed', String(mode === 'grid'));
    $('viewList').setAttribute('aria-pressed', String(mode === 'list'));
}

// ─── Rendering ─────────────────────────────────────────────

function wearBadge(wear) {
    const info = WEARS[wear];
    return info ? h('span', { class: `skin-wear-badge wear-${info.key}` }, info.label) : null;
}

function skinCard(skin) {
    const wear = getWear(skin.name);
    const baseName = getBaseName(skin.name);
    const color = rarityColor(skin.rarity);
    const st = skin.skinstrack;
    const pct = st ? discountPct(skin.cents, st.price_cents) : null;
    const card = h('button', {
        type: 'button', class: 'skin-card', style: color && `--rarity:${color}`,
        'aria-label': `${skin.name}, ${formatMoney(skin.cents)}. Open details`,
    },
        h('span', { class: 'skin-img-wrap blk' },
            isStatTrak(skin.name) && h('span', { class: 'st-badge' }, 'StatTrak™'),
            pct != null && pct !== 0 && discountBadge(pct),
            skin.iconUrl
                ? h('img', { src: skin.iconUrl, alt: '', loading: 'lazy' })
                : h('span', { class: 'skin-img-placeholder' }, icon('crosshair', 34))),
        h('span', { class: 'skin-body blk' },
            h('span', { class: 'skin-wear-row' },
                wearBadge(wear),
                h('span', { class: 'skin-listings-badge' }, `${skin.listings} listings`)),
            h('span', { class: 'skin-name blk' }, baseName),
            h('span', { class: 'skin-price-row' },
                h('span', { class: 'skin-price' }, formatMoney(skin.cents)),
                h('span', { class: 'btn-buy' }, 'Details →')),
            st && h('span', { class: 'skin-price-row alt' },
                h('span', { class: 'skin-price-alt' }, `SkinsTrack ${formatMoney(st.price_cents)}`),
                liquidityMeter(st.liquidity)),
        ));
    card.addEventListener('click', () => openDetail(skin, card));
    return card;
}

function discountBadge(pct) {
    const cheaper = pct > 0;
    return h('span', { class: 'discount-badge ' + (cheaper ? 'good' : 'bad') },
        `${cheaper ? '−' : '+'}${Math.abs(pct)}% vs Steam`);
}

function liquidityMeter(value) {
    const v = clampPct(value);
    const level = v >= 66 ? 'high' : v >= 33 ? 'mid' : 'low';
    return h('span', {
        class: `meter ${level}`, role: 'meter', 'aria-label': 'Liquidity',
        'aria-valuemin': 0, 'aria-valuemax': 100, 'aria-valuenow': v, title: `Liquidity ${v}/100`,
    }, h('span', { class: 'meter-fill', style: `width:${v}%` }));
}

// ─── Detail modal ──────────────────────────────────────────

function statRow(label, value) {
    return h('div', { class: 'cmp-row' }, h('dt', {}, label), h('dd', {}, value));
}

function openDetail(skin, opener) {
    const dlg = $('skinModal');
    const st = skin.skinstrack;
    const pct = st ? discountPct(skin.cents, st.price_cents) : null;
    const color = rarityColor(skin.rarity);
    dlg.style.setProperty('--rarity', color || 'var(--border-strong)');

    const steam = h('section', { class: 'cmp-col', 'aria-labelledby': 'cmpSteam' },
        h('h3', { id: 'cmpSteam' }, 'Steam Market'),
        h('p', { class: 'cmp-price' }, formatMoney(skin.cents)),
        h('dl', {}, statRow('Listings', String(skin.listings))));
    const track = h('section', { class: 'cmp-col', 'aria-labelledby': 'cmpTrack' },
        h('h3', { id: 'cmpTrack' }, 'SkinsTrack'),
        st ? [
            h('p', { class: 'cmp-price' }, formatMoney(st.price_cents)),
            h('dl', {},
                statRow('Liquidity', liquidityMeter(st.liquidity)),
                statRow('Offers', Number(st.count || 0).toLocaleString('en-US')),
                statRow('Volume', Number(st.volume || 0).toLocaleString('en-US')),
                st.updated_at && statRow('Updated', timeAgo(st.updated_at))),
        ] : h('p', { class: 'cmp-none' }, 'No SkinsTrack data for this skin.'));

    dlg.replaceChildren(h('div', { class: 'modal-card' },
        h('button', { type: 'button', class: 'modal-close', 'aria-label': 'Close details' }, '×'),
        h('header', { class: 'modal-head' },
            skin.iconUrl && h('img', { src: skin.iconUrl, alt: '' }),
            h('div', {},
                h('h2', { id: 'skinModalTitle' }, getBaseName(skin.name)),
                h('p', { class: 'modal-tags' },
                    wearBadge(getWear(skin.name)),
                    isStatTrak(skin.name) && h('span', { class: 'st-badge static' }, 'StatTrak™'),
                    skin.rarity && h('span', { class: 'rarity-tag' }, skin.rarity),
                    pct != null && pct !== 0 && discountBadge(pct)))),
        h('div', { class: 'cmp' }, steam, track),
        skin.marketUrl && h('a', {
            class: 'btn-optimize modal-link', href: skin.marketUrl, target: '_blank', rel: 'noopener noreferrer',
        }, 'Open on Steam Market ↗')));

    dlg.querySelector('.modal-close').addEventListener('click', () => dlg.close());
    dlg.returnTarget = opener;
    dlg.showModal();
}

function initModal() {
    const dlg = $('skinModal');
    // Clicking the dim backdrop (the dialog element itself) closes it.
    dlg.addEventListener('click', e => { if (e.target === dlg) dlg.close(); });
    // Esc closes natively; restore focus explicitly for older browsers.
    dlg.addEventListener('close', () => {
        const t = dlg.returnTarget;
        if (t && t.isConnected) t.focus();
    });
}

// ─── SkinsTrack status chip ────────────────────────────────

async function showSkinstrackStatus() {
    const chip = $('stChip');
    try {
        const s = await api.skinstrackStatus();
        let text = 'SkinsTrack · not configured';
        if (s.configured) {
            text = `SkinsTrack · ${Number(s.items || 0).toLocaleString('en-US')} items`;
            if (s.fetched_at) text += ` · updated ${timeAgo(s.fetched_at)}`;
        }
        chip.textContent = text;
        chip.classList.toggle('on', Boolean(s.configured));
        chip.classList.toggle('err', Boolean(s.last_error));
        if (s.last_error) chip.title = `Last error: ${s.last_error}`;
        chip.hidden = false;
    } catch {
        chip.hidden = true;
    }
}

function renderSkins(rawSkins, container) {
    if (!rawSkins || rawSkins.length === 0) return setMessage(container, 'No skins found.');
    container.replaceChildren(...rawSkins.map(s => skinCard(normalizeSkin(s))));
}

// ─── Home: trending ticker + premium picks ─────────────────

// Stark product tile: image, name, oversized price, and a small catalogue number.
function premiumTile(skin, n) {
    const tile = h('button', {
        type: 'button', class: 'prem-tile',
        'aria-label': `${skin.name}, ${formatMoney(skin.cents)}. Open details`,
    },
        h('span', { class: 'prem-no' }, `№ ${String(n).padStart(3, '0')}`),
        skin.iconUrl ? h('img', { src: skin.iconUrl, alt: '', loading: 'lazy' }) : h('span', { class: 'prem-img-empty' }),
        h('span', { class: 'prem-name' }, getBaseName(skin.name)),
        h('span', { class: 'prem-price' }, formatMoney(skin.cents)));
    tile.addEventListener('click', () => openDetail(skin, tile));
    return tile;
}

function renderPremium(rawSkins, container) {
    if (!rawSkins || rawSkins.length === 0) return setMessage(container, 'No skins found.');
    container.replaceChildren(...rawSkins.map((s, i) => premiumTile(normalizeSkin(s), i + 1)));
}

const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');

// "★ Karambit | Fade" -> weapon line above the finish name.
function trendName(base) {
    const [weapon, finish] = base.split(' | ');
    return h('span', { class: 'trend-name' },
        finish ? h('span', { class: 'trend-weapon' }, weapon) : null,
        finish ?? weapon);
}

function trendTile(skin, clone) {
    const color = rarityColor(skin.rarity);
    const st = skin.skinstrack;
    const tile = h('button', {
        type: 'button', class: 'trend-tile', style: color && `--rarity:${color}`,
        tabindex: clone ? '-1' : false,
        'aria-label': clone ? false : `${skin.name}, ${formatMoney(skin.cents)}. Open details`,
    },
        skin.iconUrl ? h('img', { src: skin.iconUrl, alt: '', loading: 'lazy', width: '128', height: '96' })
                     : h('span', { class: 'trend-img-empty' }),
        trendName(getBaseName(skin.name)),
        h('span', { class: 'trend-meta' },
            wearBadge(getWear(skin.name)),
            h('span', { class: 'trend-price' }, formatMoney(skin.cents))),
        st && h('span', { class: 'trend-liq' }, `Liquidity ${clampPct(st.liquidity)}`));
    if (!clone) tile.addEventListener('click', () => openDetail(skin, tile));
    return tile;
}

// The track holds the list twice so translating it by -50% loops seamlessly.
// The copy is hidden from assistive tech and the tab order.
function renderTicker(rawSkins) {
    const ticker = $('trendTicker');
    const track = $('trendTrack');
    const skins = rawSkins.map(normalizeSkin);
    if (skins.length === 0) { ticker.hidden = true; return; }

    const items = skins.map(s => h('li', {}, trendTile(s, false)));
    if (!reducedMotion.matches) {
        items.push(...skins.map(s => h('li', { 'aria-hidden': 'true' }, trendTile(s, true))));
        track.style.setProperty('--ticker-duration', `${skins.length * 3.5}s`);
    }
    track.replaceChildren(...items);
    ticker.classList.toggle('is-static', reducedMotion.matches);
    ticker.hidden = false;
}

// A nonessential loop must stop while it can't be seen.
function pauseTickerOffscreen() {
    const ticker = $('trendTicker');
    new IntersectionObserver(([entry]) => ticker.classList.toggle('is-paused', !entry.isIntersecting))
        .observe(ticker);
}

function demoTrending() {
    const all = Object.values(DEMO_SKINS).flat();
    const byLiquidity = [...all].sort((a, b) => (b.skinstrack?.liquidity ?? 0) - (a.skinstrack?.liquidity ?? 0));
    const cents = s => normalizeSkin(s).cents;
    const premium = all.filter(s => cents(s) >= 10000).sort((a, b) => cents(b) - cents(a));
    return { trending: byLiquidity.slice(0, 16), premium: premium.slice(0, 8) };
}

async function loadHome() {
    const grid = $('premiumGrid');
    setLoading(grid, 4);
    if (state.demoMode) {
        const demo = demoTrending();
        renderTicker(demo.trending);
        return renderPremium(demo.premium, grid);
    }
    try {
        const [hot, premium] = await Promise.all([api.trending(20, 1), api.trending(8, 100)]);
        renderTicker(hot.items);
        renderPremium(premium.items, grid);
    } catch (e) {
        $('trendTicker').hidden = true;
        setMessage(grid, e instanceof api.ApiError
            ? 'Trending prices load once SkinsTrack data is available. Add SKINSTRACK_API_KEY to .env and restart the API.'
            : 'Could not reach the API. Start it with scripts/dev.bat, then reload.');
    }
}

// ─── Market search ─────────────────────────────────────────

function currentFilters() {
    return {
        wears: [...document.querySelectorAll('.wear-chip input:checked')].map(c => c.value),
        stattrakOnly: $('stattrakOnly').checked,
    };
}

function showFiltered(raw) {
    const filtered = sortSkins(filterSkins(raw.map(normalizeSkin), currentFilters()), $('sortSelect').value);
    $('resultsCount').textContent = `${filtered.length} listing${filtered.length !== 1 ? 's' : ''}`;
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
    const min = $('minPrice').value;
    const max = $('maxPrice').value;
    const grid = $('searchResults');

    if (!q) return setMessage(grid, 'Please select a weapon first.');

    setLoading(grid);
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

    setLoading(results, 4);
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
    $('sideT').setAttribute('aria-pressed', String(side === 'T'));
    $('sideCT').setAttribute('aria-pressed', String(side === 'CT'));
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
    setLoading(results, 4);

    const data = await tryApi(
        () => api.buildLoadout({ side: state.side, weapons_budget: weapons, knife_budget: knife, gloves_budget: gloves }),
        msg => setMessage(results, msg));
    if (data === undefined) return;

    const slots = data === null ? demoSlots(state.side, knife, gloves) : data.slots;
    const per = weapons / 2;
    const labels = SLOT_LABELS[state.side];
    const sections = [];
    if (slots.primary)   sections.push(slotSection('primary', labels.primary, icon('rifle'), per, slots.primary));
    if (slots.secondary) sections.push(slotSection('secondary', labels.secondary, icon('pistol'), per, slots.secondary));
    if (slots.knife)     sections.push(slotSection('knife', 'Knife', icon('knife'), knife, slots.knife));
    if (slots.gloves)    sections.push(slotSection('gloves', 'Gloves', icon('gloves'), gloves, slots.gloves));
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
    document.querySelectorAll('[data-goto]').forEach(b => b.addEventListener('click', () => showPage(b.dataset.goto)));
    pauseTickerOffscreen();
    $('searchForm').addEventListener('submit', e => { e.preventDefault(); searchSkins(); });
    $('sortSelect').addEventListener('change', () => { if (state.skins.length) showFiltered(state.skins); });
    // On narrow screens the filters start collapsed above the results.
    if (matchMedia('(max-width: 900px)').matches) $('filters').open = false;
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

    initModal();

    api.isServerUp().then(up => {
        if (up) {
            loadHome();
            return showSkinstrackStatus();
        }
        enterDemoMode();
        loadHome();
        $('weaponSelect').value = 'AK-47';
        state.skins = getDemoSkins('AK-47');
        showFiltered(state.skins);
    });
}

// Modules are deferred, so the DOM is parsed by now.
init();
