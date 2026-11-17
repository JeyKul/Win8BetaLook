# DWMWindow: 8250 -> Win11 26200 part mapping

Found from the data, not from memory: property signatures (`msstyles_diff.py`) plus
the atlas crops of every part (`dwm_atlas_sheet.py`; prop 8002 is the part's
rectangle l,t,r,b in the first STREAM atlas).

- Same container format: 8250 already has AMAP, BCMAP, CMAP, RMAP, VMAP, VARIANT,
  STREAM atlases (2 in 8250, 3 in Win11). The record walk of both ends exactly at
  the end of VARIANT/NORMAL.
- Rule: 8250 part `p` = Win11 part `p` for p <= 10, Win11 part `p - 1` for p >= 12.
  8250 has no part 11.

| 8250 | Win11 | What |
|---|---|---|
| 3-6, 7-10 | 3-6, 7-10 | caption button backgrounds (8250: glass, 31-32 px wide; Win11: flat, 51 px) |
| 12-31 | 11-30 | glyphs: close, help, maximize, minimize, restore (4 sizes each) |
| 32-36 | 31-35 | frame pieces |
| 37-42 | 36-41 | tool-window close button and glyphs |
| 43-47 | 42-46 | frame parts (45-47 -> 44-46: identical property sets) |
| 48-57 | 47-56 | shadows, small frame bits |
| 58-63 | 57-62 | button outlines, the 22x396 / 65x780 strips, glass texture |
| (none) | 64-83 | white glyph set, dark mode: fill from 8250 12-31 (already white) |
| (none) | 84-87 | small white close glyphs: fill from 8250 39-42 |
| (none) | 88-91 | dark-mode button backgrounds: fill from 8250 3-6 |

Taskbar/Start: Win11's taskbar and Start are XAML and ignore msstyles. 8250's
TaskBar*/StartMenu* classes only matter with the Win10 taskbar back (e.g. ExplorerPatcher).
