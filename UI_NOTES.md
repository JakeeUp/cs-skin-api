# UI notes

## Design references

Only ideas were borrowed from these projects. No code, markup, CSS or assets
were copied (one of them is GPL-licensed).

| Reference | Pattern borrowed | Decision |
|---|---|---|
| [pablo-banker/cs2-skins-platform](https://github.com/pablo-banker/cs2-skins-platform) | Dark amber visual system: near-black background, amber accent, green/red for success/danger, Inter for UI and a monospace face for prices | Adopted the palette (`#080A0D`, `#0F1217`, `#F59E0B`, `#22C55E`, `#EF4444`) as CSS tokens on `:root`; JetBrains Mono for every number. Inter was later replaced (see impeccable below) |
| [mason-krueger1957/cs2-market-skin-radar](https://github.com/mason-krueger1957/cs2-market-skin-radar) | Surfacing a "cheaper elsewhere" discount percentage on each item | Card badge "-N% vs Steam" (green when SkinsTrack is cheaper, red when dearer), computed from integer cents in `js/lib.js` |
| [ChrisPlayer/SkinCapital](https://github.com/ChrisPlayer/SkinCapital) | Side-by-side marketplace comparison for one item | Detail modal with Steam and SkinsTrack columns plus liquidity, offers, volume and update age, and a link to the Steam listing |

## Other decisions

- Rarity colors are the standard in-game CS2 colors, shown as a top strip and
  soft glow on each card.
- The detail view uses the native `<dialog>` element: Esc, focus trapping and
  inert background come from the browser; focus is returned to the card on close.
- The SkinsTrack status chip is hidden in demo mode and when `/skinstrack/status`
  is unreachable.
- API base defaults to `http://127.0.0.1:8080`; `?api=<url>` overrides it and is
  remembered in localStorage.
- Tests: `node --test` (Node 24 does not accept a bare directory argument).

## impeccable pass

Guidance from [pbakaus/impeccable](https://github.com/pbakaus/impeccable)
(Apache-2.0). Only its rules were applied; none of its code is used.

| Rule | Change |
|---|---|
| Inter is an overused default face | Chakra Petch for headings, nav, and primary actions; Barlow for body text |
| Monospace is for data, not costume | JetBrains Mono kept only for prices, counts, and liquidity |
| Emoji and Unicode glyphs aren't an icon system | Loadout slots and the image fallback use drawn SVG icons on one grid and stroke weight |
| Browser surfaces carry the design | Themed selection, caret, scrollbars, `accent-color`, and price-field focus |
| One authored motion moment | The Home trending ticker is the only continuous animation; it pauses on hover, focus, and offscreen |
| Reduced motion means less motion, not none | Movement is removed; color and opacity feedback remain |
| No eyebrow labels above headings | The Home hero heading stands on its own |

## Chaos world (SkinAPI Corp)

The look was deliberately replaced with a brutalist, "unauthorized" world: black/white plus one yellow (`#FFFF00`), Anton display type against Space Mono, thick borders, hard offset shadows, title-barred `.win` boxes, default link underlines, and `.hl`/`mark` highlighter blocks. This supersedes the earlier amber/Chakra Petch decisions above.

| Decision | Reason |
|---|---|
| Satire lives on Home and decorative layers only | Home carries the headline, tracker, countdown, About Us, tape, and (via `chaos.js`) the checkout trap and fake popups |
| Market, Budget, Loadout stay honest | Brutalist styling, but clear labels, 4.5:1+ text contrast, visible focus, no popups, no traps; prices, SkinsTrack data, liquidity, discount badges, detail modal and Steam links unchanged |
| Rarity and wear colors stay as data | They are the only extra colors, used on card borders and badges |
| Tool hover is color/shadow only | Jitter and skew belong to Home and chrome, and are disabled under `prefers-reduced-motion` |
| Fictional company only | No real brand, OS, or store dialogs are imitated; the footer says the satire is fictional |
| Hooks for `chaos.js` | `#chaosCountdown`, `#chaosCounter`, `#checkoutTrapBtn`, `#chaosLayer`, `#siteLogo`; shared classes `.win`, `.win-bar`, `.win-title`, `.win-close`, `.win-body`, `.hl`, `.btn-guilt` |
