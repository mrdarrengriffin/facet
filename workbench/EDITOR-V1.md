# Facet editor v1: scope

The panel's own web page (`http://<panel>/`): build your screens from cards on a grid, set the panel, then Save and
see the panel check it, with a way back. It runs on the panel's address with no hub, the same page on both panels.
The design was settled as wireframe rounds (layout, page, settings, saving); nothing is in firmware yet.

## Layout
The live square panel in the middle, drawn by the panel's own renderer at its own size. On a laptop the settings and
pickers open in a sidebar; on a phone at the panel, in a bottom sheet. The top bar: the screens (tabs, in swipe
order, plus "+ Screen"), Undo, ⚙ Settings, Save · N.

## The four parts

**1. Screens.** The panel starts blank: no factory deck, no preloaded Home. You make your own screens (Home, Kitchen,
Gaming, TV controls…), name them, order them (drag, or up / down for touch and keyboard) and pick the start page.
Left / right swipes go from screen to screen in that order. Built-in screens (Clock, Weather, Media, Timer…) are
never rebuilt from parts: each is a card on a page or its own swipe page.

**2. Editing a screen.** Each screen is a grid of cells that scrolls up / down, so it need not fit the glass. Tap an
empty cell: it becomes a 1 × 1 card and the list shows everything that fits a 1 × 1. Once added, resize it to 2 × 2,
1 × 2, 2 × 1 or half height. A half with space below offers "add another half underneath" (optional, never a forced
empty slot). Every part declares the sizes it accepts and a slot accepts only what fits, so a screen always looks
right and cannot be broken. A card's options (its entity, what a tap does…) are a form made from `/fw/schema`: an
entity is a picker of that domain from the panel's own Home Assistant link, a select a segmented control (three
options or fewer) or a list, text a field. Data sources follow the source rule: a keyless public source by default
where there is one, then one entry per Home Assistant integration that provides the data, greyed when it is not set
up.

**3. Settings.** One ⚙ Settings button, top right, opens one grouped list that mirrors the panel's own Settings
screen: the Home Assistant link first (address and a long-lived token, entered once, sent to the panel directly by
the `ha.connect` path; the token is never shown back, its last four characters only), then the settings in
`/fw/schema` in their groups (Look, Sound, Clock and idle, Behaviour, Units, AI agents), each control from the
setting's own `control` (segmented, sheet, swatches, toggle, slider, presets). A "Screen settings" item lists every
built-in screen in the firmware with its options, as the panel's Settings does. No separate screen-settings button.
The live panel shows each change as it is made.

**4. Save.** The button is called Save, never "send". Save · N counts the changes; tapping it opens the review: each
change in words ("Added a Lights card to Kitchen", "Theme: Dark → Facet"), then the panel's own check (the deck check
`POST /fw/spec`; the workbench runs the same code in WASM). Every error is said next to the screen, card or setting
it is about, never as a JSON path, and Save waits until it is fixed. Save = the deck (`POST /fw/spec` kind `deck`) and
the settings that changed (`POST /fw/settings`). A refused save keeps the edits. Undo after a save goes back to the
deck and settings before it (kept by the editor); "Back to factory" is kind `revert`.

**Backup (#170).** Export the config as one file and import it (through the same review and check), plus a read-only
config endpoint for scripts. Neither carries anything sensitive: no Wi-Fi, keys or tokens.

## Rules
- Every form comes from `/fw/schema`; a new param or setting shows up with no editor change.
- Nothing reaches the panel without the review; the check runs before every save.
- Both densities: the previews are the panel's own renderer at its own size.
- Tests: the v1 flows get Playwright tests in the gate tier as each part is built.

## Not in v1
Theme editing, a free canvas you drag around (kept as an idea: it would snap so no card is cut off), per-cell header
slots.

## Rounds
One part per round, in the order above: build it, put it on both panels, a preview page with screenshots at 720 and
480, then use it on the glass and say what is wrong; again until it is a yes.

## Decided
- The panel's check messages are plain words, one wording for people and agents, the technical id on a second line;
  they are changed in the panel (core/spec.h) and the editor shows them as they come.
