# zDR UI style guide

Use this when editing or adding screens. The reference implementations are
**ADSB list** (`ADSBUIRenderer`) and **radar / radar-scope**
(`RadarUIRenderer`). New pages should match those, not invent a new theme.

## Canvas

- Display: **320×170**, RGB565.
- Default canvas: flat **`COLOR_BLACK`** (`fb.clear(COLOR_BLACK)`).
- **No filled status / stats bars.** Chrome is text on the shared black
  canvas. A hairline rule separates regions.
- Selection: **full-width amber row** (`COLOR_ADSB_SELECTED` / `#F6A90A`) with
  **black text**. Unselected rows: no zebra, no chips, no side accent bars.

## Typography (`UiText`)

| Style | Use |
| --- | --- |
| `TextStyle::Caption` | Top title, bottom hints, column headers, compact labels |
| `TextStyle::Mono` | Table body cells (MMSI, ICAO, numbers) |
| `TextStyle::Body` / `Title` | Rare; callsigns / emphasis only if needed |

Do **not** draw raw 8×16 / 6×8 glyphs for list/radar chrome. Prefer
`uiText().draw(...)` / `uiText().width(...)`.

## Palette

| Token | Role |
| --- | --- |
| `COLOR_ADSB_TEXT` / `COLOR_RADAR_TEXT` (`#6EDEF5`) | Labels, rows, rings |
| `COLOR_ADSB_SELECTED` / `COLOR_RADAR_SELECTED` (`#F6A90A`) | Selection / selected target |
| `COLOR_RADAR_GRID` | Hairlines, range rings, column rule |
| `COLOR_BLACK` | Canvas |

Avoid cyan-on-navy “SONAR” panels, green phosphor accents, and filled
teal status bars.

## List layout (reference: ADSB)

```
y≈2     Caption title left, Caption counters right   (no bar fill)
y≈20    Hairline + Caption column headers
y≈34    Mono rows, ROW_HEIGHT = 14, ≈8 visible
y≈154   Hairline + Caption stats / hints             (no bar fill)
```

Constants to reuse conceptually:

- `HEADER_Y = 2`
- `COL_HEADER_Y = 20`, `COL_HEADER_H = 14`
- `LIST_Y = 34`, `ROW_HEIGHT = 14`, `MAX_VISIBLE_ROWS = 8`
- `STATS_BAR_Y = 154`

## Detail / dossier layout (ADSB detail, AIS dossier)

```
Left (~168px)   Title callsign/name (amber) + Caption type
                Dashed compass rose + silhouette rotated to heading
Split           Vertical 1px rule (COLOR_ADSB_TEXT)
Right           Mono LABEL + value rows (ALT/SPD/… or MMSI/SOG/…)
Bottom          Hairline + Caption hints
```

Silhouettes: `AircraftGlyph` (ADSB), `ShipGlyph` (AIS). Heading 0° = nose/bow up.

## Scope / plot layout (reference: radar-scope)

```
y≈2     Caption title + range / counters
y≈22–154  Plot / scope on black (optional map under rings)
y≈154   Hairline + Caption selected name + info + hints
```

- Range rings and compass ticks: `COLOR_RADAR_GRID` / text color.
- Selected target: amber; no floating glass cards over the scope.
- Selected details live in the **bottom chrome**, not a side overlay.

## Interaction chrome

- `Enter` cycles or opens detail.
- Bottom hints: short Caption string, right-aligned
  (e.g. `Enter Det  H Help`).
- Help overlay: black panel + grid border + Caption lines (see ACARS / 433).

## External decoder pages

ACARS / 433 follow the **same list chrome**. They wrap child processes; they
do not introduce a second visual language.

## Preview

`zdr-ui-preview` renders mock PNGs. After UI changes, rebuild and check
`out/*.png` before shipping. Map draws are stubbed under `ZDR_UI_PREVIEW`.

## Checklist for a new screen

1. Flat black canvas; Caption top + bottom; Mono table body.
2. Amber selection row only; no bar fills; no zebra.
3. Same vertical metrics as ADSB list or radar-scope.
4. Add a preview PNG with mock data.
5. Page menu icon stays simple (18×18 line art).
