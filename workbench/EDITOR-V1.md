# Facet editor v1: scope

The panel's own web page (`http://<panel>/`): choose the screens, set each one up, set the panel, then send it and
see the panel check it, with a way back. It runs on the panel's address with no hub, the same page on both panels.
Round 1 is a clickable mock in the workbench (`/wasm/editor-v1`); nothing is in firmware yet.

## The four parts

**1. Screens.** Every factory screen this firmware has (`/fw/schema` screens of kind `spec`), plus Home. Each row: a
live thumbnail, its name, where its data comes from, an on / off switch and a handle to drag it into order (up / down
buttons too, for touch and keyboard). One list in deck order: a screen that is off stays in its place, greyed, and is
left out of the deck sent; Home and the start page cannot be off. The order is the swipe order and the order of Home's
cards. The start page is picked here (Home by default).

**2. Screen settings.** Tap a screen: its preview, large, and its settings as a form made from `/fw/schema` (the
spec's params): an entity is a picker of that domain from the panel's own Home Assistant link, a list of entities a
multi-pick, a select a segmented control (three options or fewer) or a list, text a field. The preview follows every
change. "Back to defaults" per screen.

**3. Panel settings.** The panel's own Home Assistant link first: its address and a long-lived token, entered once on
the editor page the panel serves, sent to the panel directly (the page's own origin, the `ha.connect` path; no Home
Assistant service call). The token is never shown back (its last four characters only). Then Home's header buttons (the deck's Home `header_actions`: the button, Settings or a
screen it opens; the status, nothing or a running timer; sent with the deck), then the settings in `/fw/schema` in
their groups (Look, Sound, Clock and idle, Behaviour, Units, AI agents), each control from the setting's own
`control` (segmented, sheet, swatches, toggle, slider, presets). The preview is Home with the look applied.

**4. Send, check, revert.** The top bar counts the changes. Review lists each in words (was → now). Check is the
panel's own validation (the deck check `POST /fw/spec`; the workbench runs the same code in WASM) and every error is
said next to the screen and setting it is about, never as a JSON path. Send = the deck (`POST /fw/spec` kind `deck`)
and the settings that changed (`POST /fw/settings`). A refused send keeps the edits. Revert: "Undo last send" (the
deck and settings before it, kept by the editor) and "Back to factory" (kind `revert`).

## Rules
- Every form comes from `/fw/schema`; a new param or setting shows up with no editor change.
- Nothing reaches the panel without the review; the check runs before every send.
- Both densities: the previews are the panel's own renderer at its own size.
- Tests: the v1 flows get Playwright tests in the gate tier as each part is built (round 2 on).

## Not in v1
Composing screens from parts, header slots, theme editing, Home card sizes and the per-card editing of the current
concept (`/wasm/editor`, which stays as the lab).

## Rounds
One part per round, in the order above: build it, put it on both panels, a preview page with screenshots, Darren uses
it on the glass and says what is wrong; again until he says yes.

## Decided
- Off screens stay in the one list, greyed, in their deck order (round 2).
- The panel's check messages are plain words, one wording for people and agents, the technical id on a second line;
  changed in the panel (core/spec.h), the editor shows them as they come (round 2).
