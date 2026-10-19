# @facet/settings

Personalisation settings for Facet panels, from one schema: [`src/settings.json`](src/settings.json).
The on-panel Settings screen, the panel's Home Assistant entities, the ESPHome web_server page and
the docs controls are all generated from it, so they never drift.

## Settings

| Group          | Setting          | Kind   | Control   | Default      |
| -------------- | ---------------- | ------ | --------- | ------------ |
| Look           | Theme            | select | segmented | Auto         |
| Look           | Accent           | select | swatches  | Ember        |
| Look           | Text size        | select | segmented | Standard     |
| Sound          | UI sounds        | switch | toggle    | On           |
| Sound          | Sound pack       | select | sheet     | Keycap       |
| Sound          | Volume           | number | slider    | 80 %         |
| Clock and idle | Clock face       | select | sheet     | Ring         |
| Clock and idle | Clock after      | number | presets   | 60 s         |
| Clock and idle | Idle brightness  | number | slider    | 70 %         |
| Clock and idle | Screen off after | number | presets   | Never        |
| Behaviour      | Transition       | select | segmented | Content rise |
| Behaviour      | Toast position   | select | segmented | Top          |

**Accent** sets carry the five accent roles (`accent`, `accent-press`, `accent-dim`,
`accent-text`, `on-accent`) for dark and light. The build fails unless every set passes: fill
3:1 against the canvas and cards, accent text 4.5:1, label on accent 4.5:1.

**Text size** Large sets the small type roles one step up (title 32, body 26, label 22) inside the
same line boxes, so nothing on a screen moves.

## Outputs

| Import                                  | What                                                                   |
| --------------------------------------- | ---------------------------------------------------------------------- |
| `@facet/settings`                       | `settings`, `groups`, `defaults()`, `resolve()`, `format()`, `check()` |
| `@facet/settings/settings.json`         | The schema                                                             |
| `@facet/settings/esphome/settings.yaml` | ESPHome template entities and web_server v3 sorting groups             |

```ts
import { resolve, format, setting, accentColors } from '@facet/settings';

const values = resolve(saved); // defaults + valid saved values
format(setting('idleAfter')!, 300); // '5 min'
accentColors('teal', 'light').accent; // '#1A9488'
```

## ESPHome

Include the YAML as a package. It declares each setting as a template select, number or switch
(config category, restored after a reboot) and groups them on the web_server page. Add what a
setting does in the firmware with `!extend`:

```yaml
packages:
  settings: !include settings.yaml

select:
  - id: !extend theme_select
    on_value:
      - script.execute: theme_apply
```

Settings with `boardDefault` (Transition) read their default from a substitution, so a slower
board can set `default_transition: Instant`.
