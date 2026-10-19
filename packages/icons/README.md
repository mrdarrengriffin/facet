# @facet/icons

Facet's icons.

- **UI icons** (`svg/*.svg`): 30 master SVGs on a 24 grid, 2 px round strokes in
  `currentColor`, a 20 px live area, each drawn to one of four keylines (circle, square,
  portrait, landscape). `icon(name, size)` renders one inline.
- **Screen icons** (`svg/screens/*.svg`): one per screen, with a single detail in the accent
  colour. `screenIcon(name, size)`, or `screenIconSvg()` for a string.
- **Weather icons**: drawn in code on a 48 grid (`weatherIcon(id, { size, animated })`), with
  static masters in `svg/weather/`. Load `@facet/icons/weather.css` for the animation.
- **Manifest** (`manifest.json`): every icon's group, keyline and the sizes screens use, which
  is what a firmware build needs to make alpha-mask images.

`src/glyphs.ts` and `src/screen-glyphs.ts` are generated from the SVGs by `pnpm build`.
