import test from 'node:test';
import assert from 'node:assert/strict';
import {
    getWear, getBaseName, isStatTrak, formatMoney, parsePriceText, normalizeSkin,
    filterSkins, sortSkins, discountPct, rarityColor, timeAgo, clampPct, WEAPON_GROUPS, apiBlockedByBrowser } from '../../js/lib.js';

test('getWear finds wear names or empty string', () => {
    assert.equal(getWear('AK-47 | Redline (Field-Tested)'), 'Field-Tested');
    assert.equal(getWear('★ Karambit | Fade (Factory New)'), 'Factory New');
    assert.equal(getWear('Sport Gloves | Vice'), '');
});

test('getBaseName strips wear and StatTrak prefix', () => {
    assert.equal(getBaseName('StatTrak™ AK-47 | Redline (Field-Tested)'), 'AK-47 | Redline');
    assert.equal(getBaseName('AWP | Asiimov (Battle-Scarred)'), 'AWP | Asiimov');
    assert.equal(getBaseName('Plain'), 'Plain');
});

test('isStatTrak', () => {
    assert.equal(isStatTrak('StatTrak™ M4A4 | X (Well-Worn)'), true);
    assert.equal(isStatTrak('M4A4 | X (Well-Worn)'), false);
});

test('formatMoney formats integer cents', () => {
    assert.equal(formatMoney(4823), '$48.23');
    assert.equal(formatMoney(125000), '$1,250.00');
    assert.equal(formatMoney(5), '$0.05');
    assert.equal(formatMoney(0), '$0.00');
    assert.equal(formatMoney(NaN), '—');
    assert.equal(formatMoney(undefined), '—');
});

test('parsePriceText', () => {
    assert.equal(parsePriceText('$1,250.00'), 125000);
    assert.equal(parsePriceText('$0.07'), 7);
    assert.ok(Number.isNaN(parsePriceText('n/a')));
    assert.ok(Number.isNaN(parsePriceText(undefined)));
});

test('normalizeSkin handles search, budget and loadout shapes', () => {
    const search = normalizeSkin({
        name: 'A', hash_name: 'A (FN)', sell_price: 1234, sell_price_text: '$12.34', sell_listings: 7,
        icon_url: 'i', market_url: 'https://steamcommunity.com/x', rarity: 'Covert',
        skinstrack: { price_cents: 1000, liquidity: 80 },
    });
    assert.equal(search.cents, 1234);
    assert.equal(search.listings, 7);
    assert.equal(search.rarity, 'Covert');
    assert.equal(search.skinstrack.price_cents, 1000);

    const budget = normalizeSkin({ name: 'B', price: '$3.00', price_cents: 300, listings: 2 });
    assert.equal(budget.cents, 300);
    assert.equal(budget.listings, 2);
    assert.equal(budget.skinstrack, null);

    const textOnly = normalizeSkin({ name: 'C', sell_price_text: '$1.50' });
    assert.equal(textOnly.cents, 150);
});

test('normalizeSkin rejects non-https market urls and bad skinstrack', () => {
    const s = normalizeSkin({ name: 'D', sell_price: 1, market_url: 'javascript:alert(1)', skinstrack: {} });
    assert.equal(s.marketUrl, '');
    assert.equal(s.skinstrack, null);
});

const skins = [
    { name: 'StatTrak™ AK-47 | Redline (Field-Tested)', cents: 9500, listings: 234 },
    { name: 'AK-47 | Slate (Factory New)', cents: 872, listings: 1204 },
    { name: 'AK-47 | Asiimov (Field-Tested)', cents: 3891, listings: 412 },
];

test('filterSkins by wear and stattrak', () => {
    assert.equal(filterSkins(skins).length, 3);
    assert.equal(filterSkins(skins, { wears: ['Field-Tested'] }).length, 2);
    assert.equal(filterSkins(skins, { wears: ['Field-Tested'], stattrakOnly: true }).length, 1);
    assert.equal(filterSkins(skins, { wears: ['Well-Worn'] }).length, 0);
});

test('sortSkins sorts without mutating input', () => {
    const copy = [...skins];
    assert.deepEqual(sortSkins(skins, 'price-asc').map(s => s.cents), [872, 3891, 9500]);
    assert.deepEqual(sortSkins(skins, 'price-desc').map(s => s.cents), [9500, 3891, 872]);
    assert.deepEqual(sortSkins(skins, 'listings').map(s => s.listings), [1204, 412, 234]);
    assert.equal(sortSkins(skins, 'name')[0].name.includes('Asiimov'), true);
    assert.deepEqual(skins, copy);
    assert.deepEqual(sortSkins(skins, 'bogus'), skins);
});

test('discountPct', () => {
    assert.equal(discountPct(1000, 880), 12);
    assert.equal(discountPct(1000, 1100), -10);
    assert.equal(discountPct(0, 100), null);
    assert.equal(discountPct(1000, undefined), null);
});

test('rarityColor', () => {
    assert.equal(rarityColor('Covert'), '#eb4b4b');
    assert.equal(rarityColor('Mil-Spec Grade'), '#4b69ff');
    assert.equal(rarityColor(''), null);
    assert.equal(rarityColor('Nope'), null);
});

test('timeAgo', () => {
    const now = Date.parse('2026-01-01T12:00:00Z');
    assert.equal(timeAgo('2026-01-01T11:59:50Z', now), 'just now');
    assert.equal(timeAgo('2026-01-01T11:30:00Z', now), '30m ago');
    assert.equal(timeAgo('2026-01-01T09:00:00Z', now), '3h ago');
    assert.equal(timeAgo('2025-12-30T12:00:00Z', now), '2d ago');
    assert.equal(timeAgo('garbage', now), '');
});

test('clampPct', () => {
    assert.equal(clampPct(150), 100);
    assert.equal(clampPct(-5), 0);
    assert.equal(clampPct('abc'), 0);
    assert.equal(clampPct(42), 42);
});

test('weapon list has no duplicates', () => {
    const all = WEAPON_GROUPS.flatMap(([, w]) => w);
    assert.equal(new Set(all).size, all.length);
});

test('apiBlockedByBrowser: public page cannot reach a loopback API', () => {
    assert.equal(apiBlockedByBrowser('jakeeup.github.io', 'http://127.0.0.1:8080'), true);
    assert.equal(apiBlockedByBrowser('jakeeup.github.io', 'http://localhost:8080'), true);
});

test('apiBlockedByBrowser: local pages and remote APIs are allowed', () => {
    assert.equal(apiBlockedByBrowser('127.0.0.1', 'http://127.0.0.1:8080'), false);
    assert.equal(apiBlockedByBrowser('localhost', 'http://127.0.0.1:8080'), false);
    assert.equal(apiBlockedByBrowser('', 'http://127.0.0.1:8080'), false);          // file://
    assert.equal(apiBlockedByBrowser('jakeeup.github.io', 'https://api.example.com'), false);
    assert.equal(apiBlockedByBrowser('jakeeup.github.io', 'not a url'), false);
});
