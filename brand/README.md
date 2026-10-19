# Facet brand

The Facet mark is a small panel: a rounded square with an orange status dot in the top-left
corner, a bold hero bar and a quieter line under it. It's the product in miniature: one thing
that matters, one line of detail, and the accent only where it counts.

## Files

| File                                                           | Use                                                                                         |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| `facet-mark.svg`                                               | The mark in `currentColor` (the dot stays orange). Inline it so it follows the text colour. |
| `facet-mark-light-on-dark.svg`, `facet-mark-dark-on-light.svg` | The mark with fixed colours for dark and light backgrounds                                  |
| `facet-logo-dark.svg`, `facet-logo-light.svg`                  | Mark and "Facet" wordmark (Inter 600) for dark and light backgrounds                        |
| `facet-app-dark.svg`, `facet-app-light.svg`                    | App icon tiles                                                                              |
| `favicon.svg`                                                  | Favicon that adapts to `prefers-color-scheme`                                               |
| `favicon-16.png`, `favicon-32.png`, `favicon-48.png`           | PNG favicon fallbacks                                                                       |
| `apple-touch-icon.png`                                         | 180 × 180 home screen icon                                                                  |
| `icon-192.png`, `icon-512.png`                                 | Web app manifest icons                                                                      |
| `social-card.png`                                              | 1200 × 630 card for link previews                                                           |

## Usage

- **Clear space:** keep at least a quarter of the mark's width clear on every side.
- **Minimum size:** 16 px for the mark on its own. Below 24 px use the favicon PNGs, which are
  drawn for small sizes.
- **Colours:** the mark is one neutral colour plus the accent. On dark backgrounds use
  `#F2F1EE`, on light ones `#16181C`, and keep the dot in Facet orange `#E8542A`.
- The wordmark is set in Inter 600 with slightly tight tracking. Don't retype it; use the logo
  files.

## Don'ts

- Don't recolour the dot, or add a second accent colour.
- Don't stretch, rotate, outline or add shadows or gradients to the mark.
- Don't put the mark on busy photos or low-contrast backgrounds.
- Don't rearrange the parts or use the hero bar and dot on their own as a logo.
