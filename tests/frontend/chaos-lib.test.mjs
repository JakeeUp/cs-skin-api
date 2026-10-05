import test from 'node:test';
import assert from 'node:assert/strict';
import { formatCountdown, nextTarget, stepCounter, canShowPopup, KONAMI, konamiProgress, clickBurst, nextRefreshMs, dealIndex, msUntilNextDeal, formatMmSs } from '../../js/chaos-lib.js';

test('formatCountdown', () => {
    assert.equal(formatCountdown(0), '00D 00H 00M 00S');
    assert.equal(formatCountdown(((3 * 24 + 14) * 3600 + 7 * 60 + 22) * 1000), '03D 14H 07M 22S');
    assert.equal(formatCountdown(-5), '00D 00H 00M 00S');
    assert.equal(formatCountdown(NaN), '00D 00H 00M 00S');
});

test('nextTarget is 3-10 days out', () => {
    assert.ok(nextTarget(0, () => 0) >= 3 * 86400000);
    assert.ok(nextTarget(0, () => 0.999) < 10 * 86400000);
});

test('stepCounter always increases, bursts sometimes', () => {
    assert.ok(stepCounter(5, () => 0.5) > 5);
    assert.ok(stepCounter(0, () => 0.05) >= 40);
});

test('canShowPopup rate limits', () => {
    const base = { now: 25000, start: 0, last: null, shown: 0, homeActive: true, open: false };
    assert.equal(canShowPopup(base), true);
    assert.equal(canShowPopup({ ...base, now: 10000 }), false);
    assert.equal(canShowPopup({ ...base, homeActive: false }), false);
    assert.equal(canShowPopup({ ...base, open: true }), false);
    assert.equal(canShowPopup({ ...base, last: 10000 }), false);
    assert.equal(canShowPopup({ ...base, now: 60000, last: 10000 }), true);
    assert.equal(canShowPopup({ ...base, shown: 3 }), false);
});

test('konami progress', () => {
    let i = 0;
    for (const k of KONAMI) i = konamiProgress(i, k);
    assert.equal(i, KONAMI.length);
    assert.equal(konamiProgress(3, 'x'), 0);
    assert.equal(konamiProgress(3, 'ArrowUp'), 1);
    assert.equal(konamiProgress(8, 'B'), 9);
});

test('clickBurst', () => {
    let t = [];
    let r;
    for (let i = 0; i < 5; i++) { r = clickBurst(t, i * 100); t = r.times; }
    assert.equal(r.hit, true);
    r = clickBurst([], 0);
    r = clickBurst(r.times, 5000);
    assert.equal(r.hit, false);
});

test('deal rotation', () => {
    assert.equal(dealIndex(0, 5), 0);
    assert.equal(dealIndex(599999, 5), 0);
    assert.equal(dealIndex(600000, 5), 1);
    assert.equal(dealIndex(600000 * 7, 5), 2);
    assert.equal(dealIndex(123, 0), -1);
    assert.equal(msUntilNextDeal(0), 600000);
    assert.equal(msUntilNextDeal(599000), 1000);
    assert.equal(msUntilNextDeal(600000), 600000);
    assert.equal(formatMmSs(125000), '02:05');
    assert.equal(formatMmSs(-1), '00:00');
});

test('nextRefreshMs', () => {
    const t0 = Date.parse('2026-01-01T00:00:00Z');
    assert.equal(nextRefreshMs('2026-01-01T00:00:00Z', 6, t0 + 3600000), 5 * 3600000);
    assert.equal(nextRefreshMs('2026-01-01T00:00:00Z', 1, t0 + 9e9), 0);
    assert.equal(nextRefreshMs(null, 6, t0), null);
    assert.equal(nextRefreshMs('garbage', 6, t0), null);
    assert.equal(nextRefreshMs('2026-01-01T00:00:00Z', 0, t0), null);
});
