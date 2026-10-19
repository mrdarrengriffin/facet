# @facet/icons

Facet's icons.

- **UI icons** (`svg/*.svg`): 30 master SVGs on a 24 grid, 2 px round strokes in
  `currentColor`, a 20 px live area, each drawn to one of five keylines (circle, square,
  portrait, landscape, glyph). `icon(name, size)` renders one inline, with the stroke for
  that size (`opticalStroke(size)`).
- **Screen icons** (`svg/screens/*.svg`): one per screen, with a single detail in the accent
  colour. `screenIcon(name, size)`, or `screenIconSvg()` for a string.
- **Weather icons**: drawn in code on a 48 grid (`weatherIcon(id, { size, animated })`), with
  static masters in `svg/weather/`. Load `@facet/icons/weather.css` for the animation.
- **Manifest** (`manifest.json`): every icon's group, keyline and the sizes screens use, which
  is what a firmware build needs to make alpha-mask images.

`src/glyphs.ts` and `src/screen-glyphs.ts` are generated from the SVGs by `pnpm build`.

## Rules

Every icon follows these, and the site's [Icons](https://mrdarrengriffin.github.io/facet/design-system/icons/)
page draws each one over its grid.

- **Grid:** 24 × 24, one unit = one pixel at 24. Weather icons: 48 × 48, the same rules doubled.
- **Live area:** the middle 20 × 20 (2 units of padding), measured to the outer edge of the stroke. Nothing
  crosses it.
- **Keylines:** circle Ø 20, square 18, portrait 16 × 20, landscape 20 × 16 for shapes; a shape reaches its
  keyline on at least one axis (±1) and never passes it by more than 2. Marks (arrows, chevrons, close, plus,
  minus, check, media transport) use the 16 × 16 **glyph** box, long side 14 to 16. The keyline is named in
  `data-keyline`. Centred, unless the shape has an optical centre (play).
- **Stroke:** 2, round caps, round joins, `currentColor`. Horizontal and vertical strokes sit on whole units
  (never x.5), so they stay sharp at 24.
- **Corners:** radius 2 on the path (outer 3) by default, 1 for shapes under 10 units, 4 for a soft shape.
  Sharp only where the point is the meaning (arrows, play, bolt).
- **Fill:** stroke by default. Only media transport is filled, plus dots: a zero-length stroke (Ø 2) in a UI
  icon, a filled circle of r 1.5 (a detail) or r 2 (a knob or end point) in a screen icon.
- **Dim:** one level, opacity 0.4, for a secondary part.
- **Optical sizes:** one master; the stroke is 1.5 / 2 / 2.5 / 3 px at 16 / 24 / 32 / 48 px
  (`OPTICAL_STROKE`, linear in between). Weather: 2 px up to 72, 3 px from 96.
- **Colour:** UI icons are one colour, never the accent. Screen icons have exactly one accent part (at most two
  elements). Weather: accent = sun, bolt, warnings; blue = water; dim text = the cloud behind and fog.
- **Naming:** kebab-case; nouns for things (`speaker`), verbs for actions (`play`), a direction as a suffix
  (`chevron-down`); screen icons by screen (`screen-weather` in the manifest); weather by condition, with
  `-day` / `-night` when it matters.
- **Export:** `pnpm build` writes `src/glyphs.ts` and `src/screen-glyphs.ts`. A firmware build renders each UI icon
  to a white alpha mask at every size in the manifest (screen icons as a base and an accent mask) and recolours
  them on the device.
