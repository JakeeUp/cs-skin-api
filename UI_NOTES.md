# UI notes

## Design references

Only ideas were borrowed from these projects. No code, markup, CSS or assets
were copied (one of them is GPL-licensed).

| Reference | Pattern borrowed | Decision |
|---|---|---|
| [pablo-banker/cs2-skins-platform](https://github.com/pablo-banker/cs2-skins-platform) | Dark amber visual system: near-black background, amber accent, green/red for success/danger, Inter for UI and a monospace face for prices | Adopted the palette (`#080A0D`, `#0F1217`, `#F59E0B`, `#22C55E`, `#EF4444`) as CSS tokens on `:root`; Inter for text, JetBrains Mono for every number |
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
