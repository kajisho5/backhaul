# backhaul logo

The mark is a lowercase "b" whose bowl is an arrow coming back to the stem: remote people are
brought into the venue, and the programme goes back to them.

| File | Use |
| --- | --- |
| `icon.svg` | App icon, 64 px and larger |
| `icon-small.svg` | App icon below 64 px (no glow or thin highlights) |
| `mark-mono.svg` | One colour, takes the text colour (`currentColor`) |
| `logo-dark.svg` / `logo-light.svg` | Icon with the name, for dark / light backgrounds |
| `icon-256.png`, `icon-1024.png` | Rendered from `icon.svg` |
| `backhaul.ico` | Windows icon: 16–48 px from `icon-small.svg`, 64–256 px from `icon.svg` |

The name is set in Unbounded (weight 500, SIL Open Font License 1.1, © The Unbounded Project
Authors) and stored as outlines, so no font is needed to show it.

To rebuild every file: `python3 tools/logo/build.py <Unbounded[wght].ttf> assets/logo`
(needs fonttools, Pillow and Google Chrome).
