// SkinAPI Corp interactive chaos. Self-contained; every hook is optional.
import { formatCountdown, nextTarget, stepCounter, canShowPopup, konamiProgress, clickBurst, nextRefreshMs, dealIndex, msUntilNextDeal, formatMmSs, rejectMessage, queueNumber } from './chaos-lib.js';

const $ = (sel) => document.querySelector(sel);
const start = Date.now();

function layer() {
    let el = $('#chaosLayer');
    if (!el) {
        el = document.createElement('div');
        el.id = 'chaosLayer';
        document.body.appendChild(el);
    }
    return el;
}

function makeWin({ title, bodyNodes, role = 'dialog', cls = '' }) {
    const win = document.createElement('div');
    win.className = `win chaos-win ${cls}`.trim();
    win.setAttribute('role', role);
    const bar = document.createElement('div');
    bar.className = 'win-bar';
    const t = document.createElement('span');
    t.className = 'win-title';
    t.textContent = title;
    t.id = `chaos-t-${Math.random().toString(36).slice(2, 8)}`;
    const close = document.createElement('button');
    close.type = 'button';
    close.className = 'win-close';
    close.setAttribute('aria-label', 'Close');
    close.textContent = 'X';
    bar.append(t, close);
    const body = document.createElement('div');
    body.className = 'win-body';
    body.append(...bodyNodes);
    win.append(bar, body);
    win.setAttribute('aria-labelledby', t.id);
    return { win, body, close };
}

const el = (tag, text, cls) => {
    const n = document.createElement(tag);
    if (text != null) n.textContent = text;
    if (cls) n.className = cls;
    return n;
};
const btn = (text, cls = '') => {
    const b = el('button', text, cls);
    b.type = 'button';
    return b;
};

// 1. Countdown
let target = nextTarget(Date.now());
function tickCountdown() {
    const node = $('#chaosCountdown');
    if (!node) return;
    const left = target - Date.now();
    if (left <= 0) {
        target = nextTarget(Date.now());
        node.textContent = 'THE EVENT HAS OCCURRED. NOTHING HAPPENED.';
        return;
    }
    node.textContent = formatCountdown(left);
}

// 1b. Drop schedule: real price-refresh row beside the pointless event.
let refreshAt = null, refreshNode = null;
async function initSchedule() {
    const anchor = $('#chaosCountdown')?.closest('p');
    if (!anchor) return;
    let st;
    try { st = await (await import('./api.js')).skinstrackStatus(); } catch { return; }
    const ms = st && st.configured ? nextRefreshMs(st.fetched_at, st.refresh_hours, Date.now()) : null;
    if (ms == null) return;
    refreshAt = Date.now() + ms;
    const list = el('ul', null, 'chaos-schedule');
    const row = (label) => {
        const li = el('li');
        const v = el('span', '--', 'chaos-sched-val');
        li.append(el('span', label), ' — ', v);
        list.append(li);
        return v;
    };
    row('THE EVENT').id = 'chaosSchedEvent';
    refreshNode = row('NEXT PRICE REFRESH');
    anchor.after(list);
    tickSchedule();
}
function tickSchedule() {
    if (refreshAt == null || !refreshNode) return;
    const ev = $('#chaosSchedEvent');
    if (ev) ev.textContent = formatCountdown(target - Date.now());
    const left = refreshAt - Date.now();
    refreshNode.textContent = left > 0 ? formatCountdown(left) : 'DUE (any moment now)';
}

// 1c. Deal of the 10 minutes: one real trending skin, rotated deterministically.
let dealList = [], dealSlot = null, dealRefs = null;
async function initDeal() {
    const home = $('#page-home');
    if (!home) return;
    let items = [];
    try {
        const { formatMoney, normalizeSkin } = await import('./lib.js');
        try {
            const api = await import('./api.js');
            items = (await api.trending(20, 1))?.items || [];
        } catch { /* fall back to demo data */ }
        if (!items.length) {
            const { DEMO_SKINS } = await import('./demo-data.js');
            items = Object.values(DEMO_SKINS).flat();
        }
        dealList = items.map(normalizeSkin).filter((s) => s.name && Number.isFinite(s.cents) && s.marketUrl);
        dealRefs = { formatMoney };
    } catch { return; }
    if (!dealList.length) return;

    const img = el('img', null, 'chaos-deal-img');
    img.alt = '';
    img.width = 96; img.height = 72;
    const name = el('p', null, 'chaos-deal-name');
    const price = el('p', null, 'big-num chaos-deal-price');
    const link = el('a', 'View on Steam Market', 'chaos-deal-link');
    link.target = '_blank';
    link.rel = 'noopener noreferrer';
    const clock = el('span', '--:--');
    const note = el('p', null, 'tiny');
    note.append('Price is real. Urgency is not. Next deal in ', clock, '.');
    const { win } = makeWin({ title: 'DEAL OF THE 10 MINUTES', bodyNodes: [img, name, price, link, note], role: 'region', cls: 'chaos-deal-win' });
    win.removeAttribute('aria-modal');
    win.querySelector('.win-close')?.remove();
    const wrap = el('div', null, 'chaos-deal');
    wrap.append(win);
    home.append(wrap);
    dealRefs = { ...dealRefs, img, name, price, link, clock };
    tickDeal();
}
function tickDeal() {
    if (!dealRefs?.clock) return;
    const now = Date.now();
    const slot = Math.floor(now / 600000);
    if (slot !== dealSlot) {
        dealSlot = slot;
        const s = dealList[dealIndex(now, dealList.length)];
        const { img, name, price, link, formatMoney } = dealRefs;
        if (/^https:\/\//.test(s.iconUrl)) { img.src = s.iconUrl; img.hidden = false; } else img.hidden = true;
        name.textContent = s.name;
        price.textContent = formatMoney(s.cents);
        link.href = s.marketUrl;
    }
    dealRefs.clock.textContent = formatMmSs(msUntilNextDeal(now));
}

// 2. Dead-end counter
let counter = 1000 + Math.floor(Math.random() * 9000);
const homeActive = () => !!$('#page-home')?.classList.contains('active');
function tickCounter() {
    const node = $('#chaosCounter');
    if (!node) return;
    if (homeActive()) counter = stepCounter(counter);
    node.textContent = counter.toLocaleString('en-US');
}
function scheduleCounter() {
    tickCounter();
    setTimeout(scheduleCounter, 400 + Math.random() * 1800);
}

// 3. Checkout trap
const STEPS = [
    { h: 'Confirm you want to confirm', p: 'Before you confirm, please confirm that you intend to confirm. Nothing is being collected.', yes: 'Confirm', no: 'No thanks, I enjoy overpaying' },
    { h: 'Are you sure you are sure?', p: 'Our records show you were sure 4 seconds ago. We need it in writing, which we will not accept.', yes: 'I am sure', no: 'I like waiting in line' },
    { h: 'Prove you are human', p: 'Select all squares containing regret.', captcha: true },
    { h: 'Waiting room', p: 'Your session is important to us. It is not important enough to hurry.', waiting: true },
    { h: 'Processing', p: 'Verifying your verification.', progress: true },
    { h: 'Almost there', p: 'You are 99 steps from step 1. Please return to the start of the start.', yes: 'Start over', no: 'Abandon cart (the cart abandons you)' },
];
let trapOpen = false;
function openTrap(opener) {
    if (trapOpen) return;
    trapOpen = true;
    let step = 0, timer = null;
    const h = el('h2', '', 'chaos-h');
    const p = el('p');
    const actions = el('div', null, 'chaos-actions');
    const bar = el('div', null, 'chaos-progress');
    const fill = el('span');
    bar.setAttribute('role', 'presentation');
    bar.append(fill);
    const { win, close } = makeWin({ title: 'CHECKOUT.EXE (unauthorized)', bodyNodes: [h, p, bar, actions] });
    win.setAttribute('aria-modal', 'true');
    const backdrop = el('div', null, 'chaos-backdrop');
    const holder = el('div', null, 'chaos-modal');
    holder.append(backdrop, win);

    const shut = () => {
        clearInterval(timer);
        document.removeEventListener('keydown', onKey, true);
        holder.remove();
        trapOpen = false;
        if (opener && opener.isConnected) opener.focus();
    };
    const render = () => {
        clearInterval(timer);
        const s = STEPS[step];
        h.textContent = s.h;
        p.textContent = s.p;
        actions.replaceChildren();
        bar.hidden = !s.progress;
        fill.style.width = '0%';
        if (s.progress) {
            let pct = 0;
            timer = setInterval(() => {
                pct += 3 + Math.random() * 6;
                if (pct >= 99) {
                    pct = 0;
                    p.textContent = 'Reached 99%. Resetting for your safety.';
                    fill.style.width = '0%';
                    clearInterval(timer);
                    setTimeout(() => { step++; render(); actions.querySelector('button')?.focus(); }, 1200);
                    return;
                }
                fill.style.width = `${pct}%`;
            }, 180);
            return;
        }
        if (s.captcha) {
            const grid = el('div', null, 'chaos-grid');
            grid.setAttribute('role', 'group');
            grid.setAttribute('aria-label', 'Select all squares containing regret');
            const LABELS = ['Sunk cost', 'Old wishlist', 'Wrong wear', 'Impulse buy', 'Float 0.999', 'Sticker craft', 'Majority of trades', 'Last Tuesday', 'This tab'];
            LABELS.forEach((l) => {
                const sq = btn(l, 'chaos-sq');
                sq.setAttribute('aria-pressed', 'false');
                sq.addEventListener('click', () => sq.setAttribute('aria-pressed', sq.getAttribute('aria-pressed') === 'true' ? 'false' : 'true'));
                grid.append(sq);
            });
            const verify = btn('Verify', 'btn-guilt chaos-yes');
            let tries = 0;
            verify.addEventListener('click', () => {
                tries++;
                grid.querySelectorAll('.chaos-sq').forEach((q) => q.setAttribute('aria-pressed', 'false'));
                if (tries >= 4) { step++; render(); (actions.querySelector('button') || close).focus(); return; }
                p.textContent = rejectMessage(tries);
            });
            actions.append(grid, verify);
            return;
        }
        if (s.waiting) {
            let place = queueNumber(48113, 0);
            const show = () => { p.textContent = `You are #${place.toLocaleString('en-US')} in line. Your position has been updated (upward).`; };
            show();
            timer = setInterval(() => { place = queueNumber(place, 1); show(); }, 1500);
            const go = btn('Skip the line (not offered)', 'btn-guilt chaos-yes');
            go.addEventListener('click', () => { step++; render(); (actions.querySelector('button') || close).focus(); });
            actions.append(go);
            return;
        }
        const yes = btn(s.yes, 'btn-guilt chaos-yes');
        const no = btn(s.no, 'btn-guilt chaos-no');
        yes.addEventListener('click', () => { step = (step + 1) % STEPS.length; render(); (actions.querySelector('button') || close).focus(); });
        no.addEventListener('click', () => { step = 0; render(); p.textContent = 'Declining has been noted and ignored. Returning you to the start.'; (actions.querySelector('button') || close).focus(); });
        actions.append(yes, no);
    };
    const onKey = (e) => {
        if (e.key === 'Escape') { e.stopPropagation(); shut(); return; }
        if (e.key !== 'Tab') return;
        const f = [...win.querySelectorAll('button')].filter((b) => !b.disabled);
        if (!f.length) return;
        const first = f[0], last = f[f.length - 1];
        if (!win.contains(document.activeElement)) { e.preventDefault(); first.focus(); }
        else if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }
        else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
    };
    close.addEventListener('click', shut);
    backdrop.addEventListener('click', shut);
    document.addEventListener('keydown', onKey, true);
    layer().append(holder);
    render();
    (actions.querySelector('button') || close).focus();
}

// 4. Fake error popups (home only, non-modal, never steal focus)
const POPUPS = [
    ['SKINAPI.EXE has stopped caring', 'Your attention has been logged and found insufficient.'],
    ['Fatal Notice', 'A price has changed somewhere. We are not going to tell you where.'],
    ['Compliance Alert', 'You have been looking at skins for a suspicious amount of time. Keep going.'],
    ['Error 0x00BUDGET', 'Your budget could not be found. It was last seen leaving with your savings.'],
];
let popShown = 0, popLast = null, popEl = null;
function closePopup() { popEl?.remove(); popEl = null; }
function showPopup() {
    const [title, msg] = POPUPS[popShown % POPUPS.length];
    const ok = btn('Acknowledge and ignore', 'btn-guilt');
    const { win, close } = makeWin({ title, bodyNodes: [el('p', msg), ok], role: 'alert', cls: 'chaos-popup' });
    win.removeAttribute('aria-labelledby');
    win.setAttribute('role', 'status');
    win.style.setProperty('--px', `${8 + Math.random() * 40}%`);
    win.style.setProperty('--py', `${Math.floor(Math.random() * 25)}%`);
    ok.addEventListener('click', closePopup);
    close.addEventListener('click', closePopup);
    layer().append(win);
    popEl = win;
    popShown++;
    popLast = Date.now();
}
function popupTick() {
    if (popEl && !homeActive()) closePopup();
    if (canShowPopup({ now: Date.now(), start, last: popLast, shown: popShown, homeActive: homeActive(), open: !!popEl || trapOpen })) showPopup();
}

// 5. Easter eggs
let kIdx = 0, clicks = [], banner = null;
function toggleOverride() {
    const on = document.documentElement.classList.toggle('chaos-override');
    if (on) {
        banner = el('div', 'CORPORATE OVERRIDE ACTIVE. ALL SKINS NOW BELONG TO SKINAPI CORP. (Press the code again to revoke.)', 'chaos-banner');
        banner.setAttribute('role', 'status');
        document.body.append(banner);
    } else banner?.remove();
}
function eggWindow() {
    const ok = btn('Fine', 'btn-guilt');
    const { win, close } = makeWin({ title: 'LOGO.EXE', bodyNodes: [el('p', 'Clicking the logo does not make it more authorized. It has been reported to the logo.'), ok], cls: 'chaos-popup chaos-egg' });
    win.style.setProperty('--px', '30%');
    win.style.setProperty('--py', '20%');
    const shut = () => win.remove();
    ok.addEventListener('click', shut);
    close.addEventListener('click', shut);
    layer().append(win);
}

function init() {
    layer();
    tickCountdown();
    setInterval(() => { tickCountdown(); tickSchedule(); tickDeal(); }, 1000);
    initSchedule();
    initDeal();
    scheduleCounter();
    setInterval(popupTick, 2000);
    // Close a popup the moment Home is hidden, not on the next tick, so it
    // never shows over the Market, Budget, or Loadout tools.
    const home = $('#page-home');
    if (home) new MutationObserver(() => { if (popEl && !homeActive()) closePopup(); })
        .observe(home, { attributes: true, attributeFilter: ['class'] });

    $('#checkoutTrapBtn')?.addEventListener('click', (e) => openTrap(e.currentTarget));

    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape' && popEl && !trapOpen) { closePopup(); return; }
        if (e.target instanceof Element && e.target.matches('input, textarea, select, [contenteditable]')) return;
        kIdx = konamiProgress(kIdx, e.key);
        if (kIdx === 10) { kIdx = 0; toggleOverride(); }
    });
    document.addEventListener('click', (e) => {
        if (!(e.target instanceof Element) || !e.target.closest('.logo')) return;
        const r = clickBurst(clicks, Date.now());
        clicks = r.times;
        if (r.hit) eggWindow();
    });
}
init();
