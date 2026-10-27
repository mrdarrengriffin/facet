# The Facet component contract

The rules every Facet component, layout and screen follows, why each exists, and how each is checked. A component that
follows this contract cannot have a layout or colour bug the proofs miss: the structure makes those bugs impossible
or the gate catches them. The API is `core/fx.h`; the lint is `tools/contract_lint.mjs`; the proofs and test helpers
are `core/test_hooks.cpp` (`fw_test`), `core/main.cpp` (`fw_lab`) and `tests/functional/_release.mjs`. "Done" is the
release gate (section 13).

## Contents

1. [Reading this document](#1-reading-this-document)
2. [Words](#2-words)
3. [Structure: Screen, Page, Header, Body, Overlay](#3-structure-screen-page-header-body-overlay) (C1-C4)
4. [Size and placement](#4-size-and-placement) (C5-C12)
5. [Escapes: bleed and decor](#5-escapes-bleed-and-decor) (C13-C14)
6. [Style and tokens](#6-style-and-tokens) (C15-C23)
7. [Drawing](#7-drawing) (C24-C30)
8. [Interaction](#8-interaction) (C31-C37)
9. [Robustness](#9-robustness) (C38-C43)
10. [Themes and parts](#10-themes-and-parts) (C44-C49)
11. [Proofs](#11-proofs) (P1-P10)
12. [Anti-patterns we have seen](#12-anti-patterns-we-have-seen) (L1-L10)
13. [Writing a component](#13-writing-a-component) (recipe, example, checklist, definition of done)
14. [Loopholes closed](#14-loopholes-closed)
15. [Check status](#15-check-status)
16. [Files](#16-files)
- [Appendix A: theme file reference](#appendix-a-theme-file-reference)

## 1. Reading this document

- Every rule has an id (`C12`, `P4`, `L3`). Tests, lint messages, reviews and commit messages cite it by id.
- Every rule has three parts: the rule (**MUST** / **MUST NOT**), **Why** (one line), **Check** (the lint rule,
  proof or test that fails when the rule is broken).
- A check marked **TODO** does not exist yet. Section 15 lists every TODO; each is a gate item. Until its check
  exists, the rule still binds and the reviewer (gate section 9) checks it by reading the code.
- **C0. The reason binds, not only the words.** A change that satisfies a rule's text but defeats its **Why** breaks
  the rule. *Why:* rules are written by people; loopholes exist (section 14 lists the ones we know). *Check:* review
  (gate section 9) reads every rule's Why against the change.

## 2. Words

| Word | Meaning |
|---|---|
| MUST / MUST NOT | Always, no exceptions unless the rule itself names one. |
| MAY | Allowed only in the case the sentence names. |
| component | A `FW_COMPONENT` whose meta says `"contract": 2` (a "migrated" component; `fx::migrated()`). Components without it are legacy: the lint counts them as backlog, never as passing. |
| layout component | A component whose job is to arrange other components (Grid, Card grid, Column, Row). The only place arrangement happens. |
| parent content box | The parent's box minus its padding and border: the only space a component may use. |
| token | A named design value from `tokens/*.json`, generated into shared styles (`tools/gen_fx.mjs` -> `fx_gen.h`). |
| shared style | One `lv_style_t` per token per display, owned by the core (`core/fx_core.cpp`), attached by name with `fx::use`. |
| layout size | A size token in `space:`, `layout:`, `touch:` and `radius:round`. Fixed by Facet. |
| look size | A size token in `radius:*` (except `round`) and `stroke:*`. Themeable within a range. |
| theme | A theme file (`themes/<id>.json`, C44): the look of the contract components. The panel setting `look`, labelled Theme, chooses it. |
| mode | Dark, light or auto: the panel setting `theme` (ESPHome `theme_select`; the id is kept so automations keep working), labelled Mode. A one-mode theme fixes it. |
| part | A themeable element from the fixed list (Appendix A Parts), attached through a `part:*` token (C46). |
| step | One of a component's fixed arrangements (full, compact, ...), chosen from its own content box. |
| minimum | The smallest parent content box a component works in, declared in its meta `"min"`. |
| Pen | `fx::Pen`: the only way a component draws pixels itself. |
| bleed | A background that reaches the page edge (`fx::bleed`). |
| decor | A mark drawn outside its owner's box on purpose (a focus ring). |
| tested | Proven by an automated test that FAILS when the behaviour is removed. A test that would pass without the behaviour proves nothing. |

## 3. Structure: Screen, Page, Header, Body, Overlay

**C1. A screen is Screen > Page > Header slot + Body, built with `fx::page(obj, body)`.**
The Page's children are the head (`page-head`: the header row, `layout:top` less the inset and header gap) and the
Body (`body` + `body:list|cards|center|page`). Nothing else is a child of the Page.
*Why:* one structure means one set of proofs covers every screen. *Check:* `tools/audit_screens.mjs` (regions never
overlap, nothing escapes its region); bounds proof P2 on the Page body (`fw_test("bounds")`).

**C2. The Body scrolls; the page inset is padding INSIDE the Body; the gutter is the gap between its children.**
The inset (`layout:inset`) and gutter (`layout:gutter`) come from the `body` / `body:cards` shared styles. No
component adds the page inset, a margin or an outer gap of its own.
*Why:* content then scrolls the full page height and a bleed has room to reach the edge; spacing changes in one place.
*Check:* P2 at insets 0..64 (`fx::inset`, `_release.mjs bounds(t, insets)`); `tools/lab_test.mjs` (page tokens are
"page only": a component mounted alone does not read them).

**C3. Screens are trees of components only.** A screen MUST NOT build a body by hand (no `*_view` bodies, no objects
created by the screen code between components). A new arrangement is a new layout component.
*Why:* hand-built bodies are where every untested layout lived. *Check:* `audit_screens.mjs`; **TODO** lint: screen
specs (`core/spec.h`) create only components inside a Page body.

**C4. Sheets, toasts and dialogs live on the Overlay layer, never in the Body.**
- The overlay root fills the screen and is not part of the Body's layout; opening it MUST NOT move, resize or reflow
  anything in the Body.
- The scrim is the only full-screen paint. The overlay's panel is a parent like any other: its children follow C5-C43.
- An overlay closes completely: after close, object count and animations are back to the baseline.
- At most one modal overlay at a time; toasts stack within their layer's `layout.json` `toast` limits.
*Why:* overlays that sat in the layout pushed content and leaked objects. *Check:* `audit_screens.mjs` overlay marking
(`fw::qa::free(obj, "overlay: ...")`); leak proof P9 on open/close; **TODO** bounds proof P2 for overlay panels (the
Bottom sheet and Toast layer are not on the contract yet).

## 4. Size and placement

**C5. A component fills its parent's content box.** Its root is `fx::box(parent, layout)` (full width, height from
its content) or uses `"fill"` (full width and height: a component that IS its parent's size, such as a Card in a
grid cell). `max-w:<layout token>` MAY cap the width (the Dial caps at `max-w:dial-scale`); nothing else sets it.
A Header at 100 % inside a 50 px parent is 50 px.
*Why:* a component that fills can be placed anywhere; one that knows a size breaks everywhere else.
*Check:* P2 at several parent sizes (`_release.mjs stage(t, w, h)` + `boundsAll`); the component's test "fills its
parent" (`tests/functional/card.test.mjs` is the pattern).

**C6. A component never knows where it is.** It MUST NOT read the screen, display, card, content or parent size
(`lay::screen_w`, `lay::screen_h`, `lay::card_w`, `lay::content_w`, `lv_display_get_*_resolution`,
`lv_obj_get_width(parent)` ...) or the density, and MUST NOT branch on which screen or parent it is in.
The one size it MAY read is its OWN content box, only to choose a step (C9).
*Why:* the patch that hard-coded a content width (L1) worked on one screen and broke the rest.
*Check:* lint rule `width` (`lay::screen_w|screen_h|card_w|content_w`); **TODO** lint: `lv_display_get_*`,
`lv_obj_get_*width/height` on anything but the component's own objects.

**C7. No component has a predefined width, cards included.** The root MUST NOT use a `w:` token, `lv_obj_set_size`,
`lv_obj_set_width/height` or a `card-w`-style token. A size prop (`"size": "m"`) is a layout component's choice of
cells, never a component's width.
*Why:* a fixed width is a hidden assumption about the parent (C6). *Check:* lint rule `local`
(`lv_obj_set_(size|width|height|pos|x|y)`); **TODO** lint: `fx::use(<root>, "w:...")` in a component.

**C8. Arrangement happens only in layout components.** Columns, rows, grids and wrapping are made by a layout
component (Grid, Card grid, Column) or by `fx::row` + `fx::cell` inside one component's own content. A component
MUST NOT place a sibling, size a sibling, or position anything with coordinates.
*Why:* one owner for arrangement means one place to prove it. *Check:* lint `local` (no positions); P2.

**C8a. Every slot declares what it accepts and how many.** A component that holds other components (a layout, the
Header) declares each slot ONCE, in its meta `slots`: `{"name", "about", "accepts": [component ids], "max": n}` (max 0
= no limit), plus `"count": n` for n numbered slots (`"in": k, "slot": i`) or `"template": true` for a repeated cell
template (the Grid's `cell`); a slot with neither is a named slot (`"in": "header", "slot": "end"`). A spec that puts a
component a slot does not accept, more than its max (items the component adds itself count: the Header's settings
button), or a repeated (`each`) node into a slot with a max is refused at load with a path error (`bad_slot` at
`.nodes[i].c`, `.nodes[i].in`, `.nodes[i].each` or `.nodes[i].cell.c`), never built and clipped. The editor and the
workbench offer only what the meta allows. Code (C++ screens) is not capped by the meta; an overfull slot is then a
bounds failure (C11). A bare count (`"slots": 4`) is the legacy form: that many slots, any component, no max.
*Why:* a slot that takes anything can be filled into a broken layout; one declaration means the check, the editor and
the docs can never disagree. *Check:* the spec checker (`core/spec.h` `slot_decls`); each slotted component's
functional test has a refusal case per rule and checks every accepted id is a real component.

**C9. A component declares its minimum and scales above it in steps.**
- Meta `"min": ["layout:<w>", "layout:<h>"]`: layout tokens, so the minimum is on the fixed grid.
- Between the minimum and any larger parent it changes only by **steps**: fixed arrangements made of tokens, chosen
  from its own content box after layout (the Card: full, then compact). A step switches both ways: the parent grows,
  the fuller step comes back.
- Type never scales continuously; a step picks another type token.
- The minimum MUST equal what the smallest step really needs, as a token relationship a test proves
  (`card-min-h = 2 x card-pad + label box + title box + space 2 + body box`).
- Every step is listed in the component's header comment and has a test and a golden.
*Why:* a component that works only at its approved size is not a component. *Check:* the component's tests "declared
minimum = sum", "scales both ways" (`card.test.mjs`); **TODO** lint: every component's meta has `"min"`.

**C10. Below its minimum, the bounds proof fails, by design.** A component MUST NOT hide, clip, shrink type or drop
content to pass below its declared minimum.
*Why:* a too-small parent is a layout bug the proof must show, not a silently clipped component.
*Check:* the component's test "below the declared minimum the bounds proof FAILS" (`card.test.mjs`).

**C11. Fixed-size children fit the minimum.** Every child with a fixed token size (`w:` / `h:` tokens, an icon size,
a drawn minimum, a button) MUST fit inside the component's declared minimum at every step, with the longest text
fixtures. If it does not, the minimum is wrong or the child must scale (a step).
*Why:* a fixed toggle and caption inside the scaling Dial overflowed at small sizes (L4).
*Check:* P2 at the declared minimum with `_release.mjs fixtures`; **TODO** lint: each `w:` token used inside a
component is <= its declared min width (generator check).

**C12. Data is not layout.** A position or length computed from DATA (a bar's length, a day on a temperature scale,
an arc's end) is drawn with the Pen (C24) in Pen coordinates. A component has **0 local style properties**; there is
no exception today.
*Why:* local styles are where colours and sizes hid from themes and proofs. *Check:* lint `local`
(`lv_obj_set_style_*`, `lv_style_init`); inspector local count (`fx_tree`); **TODO** an `fx` API for an object
placed by data (until then, none exists and none is allowed).

## 5. Escapes: bleed and decor

**C13. Bleed is explicit: `fx::bleed(obj)`, nothing else.** A bleed extends an object's background to the page edge
(the inset, via `transform_width` in the `bleed` style). It is only for a background or rule that meets the page edge
(a divider). The component's header comment names each bleed and why. `transform_width`, negative padding or
negative margins MUST NOT be used any other way.
*Why:* the bounds proof skips bleeds, so an undeclared bleed is an unchecked escape. *Check:* P2 skips only objects
with the `bleed` style; **TODO** test: each bleed reaches exactly the page edge; lint: `transform_width` / negative
`margin` outside the core.

**C14. Decor is explicit: only core atoms draw outside their owner's box** (today the focus ring, `fw::focus_ring`).
A component MUST NOT draw outside its own box itself.
*Why:* undeclared decor overlaps neighbours and survives as stale paint. *Check:* **TODO** an `fx` decor API that
marks decor for P2; **TODO** pixel check: nothing outside a component's box changes except declared decor.

## 6. Style and tokens

**C15. Every look is a shared style, attached by name.** Colours, type, sizes, radii and strokes come only from
`fx::use(obj, "token", selector)`, `fx::text(parent, s, "type:*", "fg:*")`, `fx::set_text`, `fx::icon(parent,
name, size, "fg:*")` and the Pen. An unknown token name aborts (`fx::use`): a typo crashes the workbench, never
falls back silently.
*Why:* a theme, accent or token change then rewrites the shared styles once (`fx::look`, one
`lv_obj_report_style_change`) and everything repaints. *Check:* lint `theme`, `local`; P3, P4.

**C16. A component never holds, resolves or chooses a colour, font or theme.** It MUST NOT use `fw::T`, `T->`,
`theme_of(`, `fw::Theme`, `lv_color_hex/make`, `lv_font_*`, `LV_SYMBOL_*`, `#include "theme.h"`, `th::`, or keep a
colour in a variable, static or struct.
*Why:* the per-component theme lookup patch (L2) and cached stroke colours (L3) both showed stale colours after a
switch. *Check:* lint `theme`; **TODO** lint: `lv_font`, `LV_SYMBOL`, `theme.h`, `th::`, `lv_color_t` members.

**C17. States are LVGL states with a token per state.** Pressed, checked (on), focused and `fw::st::OFF` (disabled /
unavailable) are set with `lv_obj_add_state` / `fx::state(obj, state, on)`, and the look per state is
`fx::use(obj, token, LV_STATE_...)`. A component MUST NOT swap styles or colours in an event handler to show a state.
*Why:* a tap and a theme switch can then never disagree (the tile accent bug, L5). *Check:* P4 with states set
before the switch.

**C18. Layout sizes are fixed by Facet and not themeable.** Everything in `space`, `layout`, `touch` and
`radius:round` comes from the 4 px grid, the size class (density) and the type scale. A theme MUST NOT change them;
`fx::tune` moves them only for proofs and the workbench Layout lab, never as a product setting.
*Why:* themeable spacing lets a theme break layouts the proofs were run on. *Check:* `tools/lib/looks.mjs` (read by
`gen_fx.mjs` and `gen_theme.mjs`) marks each size `look` or not; `fx::tunables()` reports `"fixed"`; the theme parser
refuses a layout size (`size.layout:inset: layout sizes are fixed by Facet (C18)`; `theme_parse` tests).

**C19. Look sizes are themeable within ranges.** `radius:*` within 0 .. max(2 x token, 16); `stroke:*` within 1 ..
max(2 x token, 2); `stroke:pixel` (the cell of a snapped theme, C29) within 1 .. 4. `fx::tune` clamps to the range
(proofs, the Layout lab); a theme value outside it is refused with the range (`size.radius:card: 40 at 1.5x is outside
0 .. 16`), so an author never ships a value they did not get. A look size changes how a thing looks, never where
anything goes.
*Why:* corners and edges are taste; positions are usability; a silent clamp hides a change from the author.
*Check:* `tools/lib/looks.mjs` (the one list); `fx::tune` clamp; `tools/lab_test.mjs`; `theme_parse` test "look
ranges are the one list" (every look size: the parser's range = `fx::tune`'s, hi loads, hi + 1 refused).

**C20. Token relationships are enforced by generators and tests, never by convention.** Today: every spacing,
layout, radius and touch value is a multiple of 4 (`tools/gen_space.mjs`); 2 cards / 3 tiles + gutters + insets fill
the screen (`gen_space.mjs`); the header row leaves a header gap and the header items fit (`tools/gen_layout.mjs`);
a component's minimum is its step's sum (C9 test). Required and not yet checked: the header row >= `touch:min`, the
gutter <= the inset, every row height >= `touch:min`.
*Why:* a relationship nobody checks drifts. *Check:* the generators fail the build; **TODO** the three missing
relationships in `gen_layout.mjs`.

**C21. A missing value becomes a token first.** A component that needs a colour, size, font, shape or derived sum
gets a token (`tokens/*.json`) or a derived style (`DERIVED_N` in `core/fx_core.cpp`, e.g. `dial-wing-up`) first.
A component MUST NOT do token arithmetic to size objects.
*Why:* a sum in a component is a hidden token that themes and the Layout lab cannot see. *Check:* lint `local`;
**TODO** lint: arithmetic on `fx::size()` outside draw and hit functions.

**C22. `fx::size` and `fx::type_box` are for drawing and hit geometry only.** They MUST NOT size or place objects.
*Why:* objects sized from them stop following `fx::tune` and the shared styles. *Check:* **TODO** lint (calls outside
the draw callback and pointer handlers); review.

**C23. Every token a component uses visibly changes it at run time.** Changing any size, radius, stroke or colour
token the component reads (`fx::uses`) repaints it live, with no rebuild. A token it reads but that changes nothing
is a bug (dead token) or must be removed.
*Why:* a size token nobody tested did nothing when changed (L6). *Check:* P3 (`tools/lab_test.mjs` for the Card;
**TODO** the same loop for every component).

## 7. Drawing

**C24. Pixels a component draws itself go only through the Pen.** `fx::drawn(parent, draw)` makes the box (fills its
parent's width, at least as tall as wide, never taller than its parent); `draw` gets an `fx::Pen` on the largest
centred square, coordinates relative to its centre. Primitives: `line`, `arc`, `dot`, `tick`, `knob`, `text`.
A component MUST NOT call `lv_draw_*`, add its own draw callbacks or make canvases.
*Why:* the Pen is where token reads, snapping and partial redraw are enforced. *Check:* lint `draw`; **TODO** lint:
`LV_EVENT_DRAW_*` callbacks and `lv_canvas_*` in components.

**C25. Inks and strokes are named and read at draw time.** Every stroke names a size token (`"stroke:indicator"`),
every ink a colour style (`"fg:accent"`); the Pen resolves both on every draw. A component MUST NOT pass, cache or
compute a colour or stroke width.
*Why:* shared stroke helpers that cached their colour at first draw kept it after a theme switch (L3).
*Check:* P4 pixel proof (a drawn part with a cached colour passes the style proof and fails only the pixel proof).

**C26. Partial redraw is mandatory.** The static layer (scale, track) is drawn once; a change invalidates only the
changed area with `fx::redraw(o, x, y, w, h)` or `fx::redraw_arc(o, r, deg0, deg1, pad)`; Pen primitives outside the
area skip themselves (`Pen::sees`). A component MUST NOT call `lv_obj_invalidate` on a moving or updating part.
*Why:* a dial that redrew its whole square ran at 7.5 fps on the slower panel against 27 with partial redraw (L7).
*Check:* P10 (`_release.mjs frames`: dirty px per frame within the changed area); **TODO** per drawn component.

**C27. Hit geometry is the drawn geometry.** Pointer maths uses `fx::pointer`, `fx::half` and the same token sizes as
the draw function, so what you see is what you press.
*Why:* hit maths that drifted from the drawing made presses miss. *Check:* the component's interaction tests (tap at
drawn positions, `dial.test.mjs`).

**C28. Whole pixels, fully opaque, RGB565-safe.** No opacity to make a colour (desaturate instead), gradients only
dithered, faint tints land on whole 565 steps.
*Why:* the panels are 16-bit; opacity tricks band and shift hue. *Check:* goldens in RGB565 (P7); contrast pairs
on 565 values (`tools/contrast.mjs`).

**C29. Snapped themes draw on the cell grid with no anti-aliasing.** With `shape.snap`, every Pen and core primitive
is whole cells of the `stroke:pixel` token on a screen-fixed grid.
*Why:* a retro theme with anti-aliased edges looks broken. *Check:* **TODO** palette proof (no rendered pixel outside
the theme's palette except gradient faces and rounded corners) when the theme engine lands (C44).

**C30. Drawing never computes a negative or zero geometry it then uses.** Radii, lengths and diameters derived from
the box are clamped; a box with `half < 1` draws nothing (core guard in `on_draw`).
*Why:* a negative radius in a squeezed parent crashed the renderer (L8). *Check:* P9 fuzz down to 0 x 0 (C43).

## 8. Interaction

**C31. Hit areas never overlap.** Two clickable areas MUST NOT intersect unless one is inside the other and the inner
one stops the event (`lv_obj_remove_flag(inner, LV_OBJ_FLAG_EVENT_BUBBLE)`, as the Dial's button does).
*Why:* a full-width -/+ row swallowed presses meant for the lower ring of a dial. *Check:* P5
(`fw_lab(id, "proof")` -> `overlap`); the component's "hit areas" test.

**C32. Touch targets are at least `touch:min` on both axes** (ext click area included); the standard is
`touch:target`. Targets are spaced so a finger cannot hit two.
*Why:* usability on a wall panel. *Check:* `tools/audit.mjs` touch targets; **TODO** P5 adds the size check per
component.

**C33. Components never call Home Assistant.** They emit events: `fw::emit(obj, name, detail)` for a final action,
`fw::notify(obj, name, detail)` for live values while dragging. The binding (`core/spec.h`) maps each event to its
HA call, with rate limits (`LIGHT_THROTTLE_MS`, `CLIMATE_THROTTLE_MS`), hold-off after a local change
(`ha::hold(obj, LIGHT_HOLD_MS, ...)`: HA's echoes do not jump the control back) and a pending window (`PENDING_MS`).
Every event a component emits has a documented HA call in its binding and a test with the fake HA.
*Why:* one place owns HA traffic, so limits and hold-off are the same everywhere. *Check:* functional tests with
`FakeHA` (`tools/functional.mjs`); `_release.mjs faults.outOfOrder`; **TODO** a per-binding test that every emitted
event maps to a call.

**C34. Nothing is sent on open.** Building, opening, restoring and an opening animation emit no action events and
play no sound.
*Why:* opening a screen must never change the house. *Check:* behaviour "silent" checks; **TODO** a generic test:
mount every component and screen, assert zero events and zero HA calls.

**C35. External changes arrive through `FW_UPDATE` and animate silently.** An update applies props in place with the
same transition a tap would play, with no sound and no pressed flash; texts change in place (`fx::set_text`); while a
finger holds the control, the user wins. First builds appear at rest. `FW_UPDATE` returns false only when a rebuild
is truly needed.
*Why:* a value that jumps looks broken; a sound from an automation is noise. *Check:* behaviour `silent`;
`tools/audit.mjs` update check (a state prop that jumps in under 3 frames); the component's update tests.

**C36. Every action gives feedback within one frame:** the pressed state style, and the sound its meaning names in
`tokens/sound.json`. A blocked action (disabled, unavailable) calls `fw::st::blocked(obj, detail)` (event + error
sound) and changes nothing.
*Why:* a touch with no answer gets pressed again. *Check:* behaviour tests (one sound per tap, disabled fires
nothing).

**C37. Taps use the shared state rules:** `fw::st::on_tap(obj, cb, user)` + `fw::st::tap_now(e)` (blocked on finger
down, a real tap on `CLICKED`); dragging off cancels.
*Why:* every component then feels the same. *Check:* behaviour (slide-off cancel); functional tap tests.

## 9. Robustness

**C38. Every state is handled:** normal, pressed, on, off, disabled, loading, unavailable, unknown, empty, error.
Each is a prop value with a variant and a test, or is listed in the header comment as "n/a: <why>".
*Why:* the untested state is the one the house shows at 3 am. *Check:* the component's tests; **TODO** lint: meta
state options + header n/a list cover all ten.

**C39. Bad values never break it.** Out-of-range values clamp; wrong types fall back to the default; NaN and
infinities are refused (`std::isfinite`); a missing entity shows its "missing" state; attributes may disappear.
*Why:* Home Assistant sends whatever it has. *Check:* `_release.mjs faults` (missing, unavailable, unknown,
wrongType, outOfRange, dropped, outOfOrder) and `fuzz`.

**C40. Any text fits by rule.** Long, empty and non-ASCII text (accents, emoji, CJK, right-to-left) is one line ending
in "..." or wraps to a declared line count; it never clips, overlaps or pushes anything out of its box.
*Why:* real names are long and not English. *Check:* `_release.mjs fixtures` in the component's tests;
`fw_lab(id, "proof")` -> `trunc`.

**C41. Rapid input never sticks.** Repeated taps, drags, flicks, long presses and multi-touch leave a consistent state
and no stuck animation.
*Why:* people press again when nothing happens. *Check:* `_release.mjs monkey`.

**C42. Lifecycle is clean.** Open / close / reopen cycles return objects and animations to the baseline and keep
heap growth inside budget; a theme or accent switch mid-interaction, and a rebuild mid-animation, keep it working.
Async calls and animations are cancelled on delete (`lv_async_call_cancel`, `lv_anim_delete`).
*Why:* the panel runs for months. *Check:* `_release.mjs leak`; ASan (`tools/asan.sh`); **TODO** a test switching
theme during a drag.

**C43. Zero-size and tiny parents never crash.** At 0 x 0, 1 x 1 and every size up to its minimum the component builds
without abort, crash or negative geometry (the bounds proof MAY fail below the minimum, C10).
*Why:* the Layout lab's squeeze found a negative-radius crash (L8). *Check:* **TODO** fuzz sizes 0 .. minimum (the
Card's fuzz covers sizes at or above its minimum only).

## 10. Themes and parts

Status: the renderer applies colour themes and accents live today (`fx::look`). Theme FILES are read, checked and
compiled by the renderer (`core/theme_file.{h,cpp}`): a file becomes an immutable compiled theme (a Skin) or is
refused whole, with every error's path; the static guards (contrast, press feedback, geometry) run there. Applying a
Skin to the shared styles (parts, edges, shadows), theme fonts and snapped drawing land next; their checks are TODO
until then.

**C44. A theme is one data file applied live.** `themes/<id>.json`, parsed at run time, applied as one style change
(`fx::look`): every shared style is rewritten; components are never rebuilt and never told. A key left out inherits
Facet's value; an unknown key, token, family or value rejects the whole file with an error naming the path
(`parts.card.pressed.edges[1].side: ...`). A refused file is never partly applied: a load builds a new immutable
Skin, the live one is swapped by pointer only when the new one is complete and guarded, and the apply path repaints
when a generation counter changes, never on pointer identity (a freed Skin's address can be reused). The panel's
Mode setting (`theme`) and Theme setting (`look`) are separate (section 2 Words).
*Why:* a theme must never need component code, and a half-read file must never reach the screen. *Check:* P4;
`theme_parse` tests (one per error class: refused, its path, the live theme and the frame unchanged; never-partial
apply and its break-it), `theme_fuzz` (10k seeded mutations, WASM) and `tools/theme_fuzz.sh` (100k under ASan + UBSan);
built-in themes are checked when the build runs (`tools/theme_check.mjs`), so the panel never meets a bad one;
`theme_apply` tests: Facet after any theme = the frame before (S1), each built-in theme switched live = a fresh build
in it (and theme to theme), a refused file leaves every pixel and the generation, and the break-it (`fx::break_stale`,
no repaint) turns the live = fresh proof red.

**C45. Themes paint; they never lay out.** A theme MUST NOT move, hide, reorder or resize anything beyond the look
ranges (C19). Layout sizes are not in a theme's reach (C18).
*Why:* layout is proven once, for every theme. *Check:* P4 per theme (`theme_apply`); part styles set only paint
keys (face, radius within its look range, the border's ink, never its width: `part_style` in `core/fx_core.cpp`);
**TODO** P2 (bounds) per theme.

**C46. A theme styles a fixed list of parts** (Appendix A): each with face, edges (up to 8 layers), shadow, text
colour and weight, and state overrides (on < focus < pressed < disabled). A component makes an object a box part only
through its `part:*` token (`fx::use(o, "part:card")`), never by naming a theme; drawn parts (knob, ticks, arcs) are
the Pen's, text parts follow their type token, the icon is `fx::icon`'s. The core attaches a part's MERGED block
(base <- on <- focus <- pressed <- disabled for the states set) for every combination of the four states (16), after
the component's own tokens: LVGL takes each property from the matching style with the largest state and, at equal
state, the style added last, so only the exact combination's block is always right (a tap focuses a focusable
object: on + focus must keep the on look). A key the theme leaves out is absent and the component's token shows
through.
*Why:* a fixed list is what a theme author and the guardrails can rely on; a block attached only at the base state
would lose to a component's pressed token, and a block per single state would let focus hide on. *Check:* the parser
compiles every combination (`Skin::merged`; `theme_parse` "state merge"); the core applies them (`part:*` = 16 shared
styles + one draw callback for edges, shadow and pattern faces; the Pen reads knob, tick and arc blocks at draw time;
text parts by type, their shadow drawn from the label's own draw task); each component's meta lists its parts and
the lint (rule `parts`) holds the list to the code; `theme_apply` "parts drawn" checks pixels of a card edge and a
header gradient. **TODO** P3 per part; the header's caption buttons, the scrollbar, `track` parts and text weight.

**C47. The look is free; only usability is guarded.** A theme is refused if it fails a guardrail (Appendix A):
contrast in every mode, accent, state and every stop of a gradient or pattern; press feedback (C36: a part whose
theme fixes its face still looks different pressed); touch targets; readable type floors; text fits; edges inside
padding; nothing outside its parent; no stale paint.
*Why:* taste is the theme author's; usability is everyone's; a theme that sets a face and no pressed face would
otherwise silently remove press feedback. *Check:* the static guards in the parser (contrast on the exact and the
RGB565 values, press feedback, geometry; `theme_parse` tests, `tools/contrast.mjs --theme <id>`); the real pixels of
every contract component in every built-in theme (`tools/pixel_contrast.mjs --theme all`, in the build); **TODO** fit
and bounds per theme (P2, P6).

**C48. Technical caps exist only for performance or memory** (8 edge layers per block). No other limit on looks.
*Why:* caps for taste are not the contract's job. *Check:* review.

**C49. Fonts are data.** A font is a family in the registry (`fonts/fonts.json`) built at every size a type token or
theme names; a component never names a font (C16).
*Why:* a hard-coded font whitelist blocked a theme's font (L8). *Check:* the parser accepts only a family this build
has (`font.body: "x" is not in this panel's build (...)`); **TODO** every type token's family is built at its size
(theme fonts).

## 11. Proofs

Every component passes every proof, as an automated test that fails without the behaviour. Testing detail and the
full bar: the Facet release gate (sections 1-10). Helpers: `tests/functional/_release.mjs`.

| Id | Proof | How | Status |
|---|---|---|---|
| P1 | Lint | `tools/contract_lint.mjs` (rules `width`, `theme`, `draw`, `local`) fails the build for a component | enforced; rule gaps in section 15 |
| P2 | Bounds | every object inside its parent's content box (bleed skipped): 720 and 480, insets 0..64, odd and wide parents, squeezed to the declared minimum; FAILS below it. `fw_test("bounds")`, `bounds(t, insets)`, `stage`, `boundsAll` | Card, Dial |
| P3 | Live token propagation | every size / radius / stroke / colour token the component reads (`fx::uses`) changes its pixels live and back (`fw_lab` `uses`, `probe`, `tune`); every contract component lists at least one | all contract components (`tools/lab_test.mjs`, fails the build) |
| P4 | Colour + pixel | after every theme x mode x accent switch, live (`fw_test("look")`), with states set BEFORE the switch (pressed, on, released): every resolved colour equals a fresh build's and is a current token, and the RGB565 frame equals a fresh build's, every pixel (`frame565`, `diff565`); and the PIXEL contrast of every shown text / icon on what is really behind it, built and switched live (`tools/pixel_contrast.mjs`, every contract component, fails the build) | Card, Header, Icon button, Status chip, Time (`liveLook`); TODO all |
| P5 | Hit areas | no overlapping clickable areas (`fw_lab(id, "proof")` -> `overlap`); targets >= `touch:min` | Card; TODO size check |
| P6 | Text fit | no text cut except by its rule (`proof` -> `trunc`) with `fixtures` | Card |
| P7 | Goldens | every state x step x density (x theme when themes land), RGB565, reviewed by a person, never auto-accepted (`golden`; `FW_GOLDEN_ACCEPT=1` only after review) | Card |
| P8 | States and faults | every state (C38), every fault (`faults`), every update path (C35) | per component |
| P9 | Fuzz, monkey, leak | `fuzz` (props x parent sizes x HA states, seeded), `monkey` (random gestures, seeded), `leak` (open/close cycles), ASan before merge | Card |
| P10 | Frame time + dirty area | `frames(t, fn)`: dirty px per interaction inside the changed area, no full redraws; device fps budgets in the gate | Card press; TODO drawn parts |

A proof that passes MUST also be shown to fail: each new proof ships with a "break it" case (inject the bug, see the
proof go red), as the fresh-start experiment did for widths, held colours and cached drawn colours.

## 12. Anti-patterns we have seen

Never again. Each one shipped or nearly shipped; each rule above that cites it exists because of it.

| Id | What happened | Rule |
|---|---|---|
| L1 | d1eb999 patched a layout by hard-coding the content width in a component: right on one screen, wrong on every other size. | C6, C7 |
| L2 | fdf56a7 fixed taps by looking the theme up per component: each component then kept its own copy of a colour. | C15, C16 |
| L3 | Shared stroke helpers cached their colour at first draw; a theme switch left old colours in drawn parts (the style proof passed, the pixels did not). | C25, P4 |
| L4 | Fixed-size children (the Dial's toggle and caption) inside a component that scales: they overflowed at small sizes. | C11 |
| L5 | A tile set its accent in a tap handler; an accent switch afterwards left the old accent (the tile accent bug). | C17, P4 |
| L6 | A size token used by a component did nothing when changed; nobody had tested it. | C23, P3 |
| L7 | A dial redrew its whole square on every move: 7.5 fps against 27 with partial redraw. | C26, P10 |
| L8 | A squeezed parent gave a drawn part a negative radius and crashed; a hard-coded font whitelist refused a valid font. | C30, C43, C49 |
| L9 | A preview built from older source was reported as the new work. | 13 (definition of done) |
| L10 | "Passes" claimed for behaviour no test covered. | 1 ("tested"), C0 |

## 13. Writing a component

### Recipe

1. **Find the approved design** and port it; the contract changes how it is built, not how it looks.
2. **Write the header comment:** what it is, its steps, its states (and "n/a: why" for the rest), each bleed, the
   events it emits, its update path.
3. **Meta:** `"contract": 2`, `"min": [w, h]` (layout tokens), props with ranges and defaults, variants for every
   state and step.
4. **Build** with `fx::box` / `fx::text` / `fx::icon` / `fx::row` + `fx::cell` / `fx::drawn`; looks by `fx::use`
   with a token per state. Missing token: add it first (C21).
5. **Interact** through `fw::st::on_tap`, `fw::emit` / `fw::notify`, `fw::sound`; never HA (C33).
6. **Update path:** `FW_UPDATE` in place, silent (C35).
7. **Steps** (if the content can outgrow the minimum): pick from the own content box after layout, both ways (C9).
8. **Tests:** `tests/functional/<id>.test.mjs` covering every rule in the checklist below, using `_release.mjs`;
   `tests/behaviour/<id>.json` for what it does.
9. **Run the gate** (section 13 definition of done) on a build from the current source.

### A complete minimal component

```cpp
// Switch row: a name and an On / Off word; a tap switches it. Fills its parent's width; height = list-row-h.
// Steps: one (the name takes the free width and ends in "..."). Bleed: none. Events: toggle on|off, blocked.
// States: default, pressed, on, off, disabled + unavailable (fw::st::OFF). n/a: loading, unknown, empty, error (no data
// of its own: the binding maps those to unavailable). Update: label in place, on/off as a tap does it (silent).
#include "fw.h"
#include "fx.h"

namespace {
lv_obj_t *word(lv_obj_t *r) { return lv_obj_get_child(r, 1); }
void set(lv_obj_t *r, bool on) { fx::state(r, LV_STATE_CHECKED, on); fx::set_text(word(r), on ? "On" : "Off", "type:label"); }
void tapped(lv_event_t *e) {
  if (!fw::st::tap_now(e)) return;
  lv_obj_t *r = (lv_obj_t *) lv_event_get_current_target(e);
  if (lv_obj_has_state(r, fw::st::OFF)) { fw::st::blocked(r, "switch"); return; }
  const bool on = !lv_obj_has_state(r, LV_STATE_CHECKED);
  set(r, on);
  fw::emit(r, "toggle", on ? "on" : "off");
  fw::sound(r, on ? "toggle-on" : "toggle-off");
}
}  // namespace

FW_COMPONENT(switch_row, "Switch row", "components", R"({
  "about": "A row with a name and an On / Off word; a tap switches it.", "contract": 2,
  "min": ["layout:card-min-w", "layout:list-row-h"],
  "props": [{"name": "label", "about": "The name.", "type": "text", "default": "Desk lamp"},
            {"name": "on", "about": "Whether it is on.", "type": "bool", "default": false},
            {"name": "state", "about": "default, disabled or unavailable.", "type": "select",
             "options": ["default", "disabled", "unavailable"], "default": "default"}],
  "variants": [{"label": "off", "props": {}}, {"label": "on", "props": {"on": true}},
               {"label": "disabled", "props": {"state": "disabled"}}]
})") {
  lv_obj_t *r = fx::box(parent, "row");                      // the parent's width, gutter between children
  fx::name(r, "switch_row");
  for (const char *t : {"h:list-row-h", "line:line", "fade-release"}) fx::use(r, t);
  fx::use(r, "bg:pressed-surface", LV_STATE_PRESSED);
  fx::use(r, "fade-press", LV_STATE_PRESSED);
  fx::use(fx::text(r, p["label"] | "", "type:body", "fg:text"), "grow");  // takes the free width, ends in "..."
  lv_obj_t *w = fx::text(r, "Off", "type:label", "fg:text-dim");
  fx::use(w, "fg:accent-text", LV_STATE_CHECKED);            // a token per state: no colour held anywhere
  fx::use(w, "fg:disabled-ink", fw::st::OFF);
  lv_obj_add_flag(r, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_STATE_TRICKLE));
  set(r, p["on"] | false);
  if (strcmp(p["state"] | "default", "default")) fx::state(r, fw::st::OFF, true);
  fw::st::on_tap(r, tapped, nullptr);
}

FW_UPDATE(switch_row) {
  const bool off = strcmp(p["state"] | "default", "default") != 0;
  if (off != lv_obj_has_state(root, fw::st::OFF)) return false;  // a new state: rebuild
  fx::set_text(lv_obj_get_child(root, 0), p["label"] | "", "type:body");
  set(root, p["on"] | false);                                // the same change a tap makes: no event, no sound
  return true;
}
```

### Checklist (each line = a rule and its check; all must be green)

| Rule | Check in the component's tests |
|---|---|
| C5-C8 fills, knows nothing, no width | lint P1 clean; "fills its parent" at full, odd, wide sizes |
| C9-C11 minimum, steps, fixed children | "minimum = sum"; "scales both ways"; P2 at the minimum with `fixtures`; "below the minimum FAILS" |
| C12-C14 no local styles, escapes | lint `local` clean; every bleed named in the header |
| C15-C17, C23 tokens and states | P3 per token it reads; P4 with states set before switching |
| C24-C30 drawing | lint `draw`; P4 pixels; P10 dirty area; fuzz to 0 x 0 |
| C31-C37 interaction | P5; one test per event (emit, sound, blocked, drag-off cancel); silent update; nothing on open |
| C38-C43 robustness | every state or n/a; `faults`; `fixtures`; `monkey`; `leak`; ASan |
| P7 goldens | every state x step x density, reviewed |

### Definition of done

A component, layout or screen is done when it passes every applicable section of the Facet release gate (sections
1-10), on a build made from the exact source under review (the build is stamped with the source hash and the hash is
checked before anyone reports "done"), with each claim backed by a test that fails without the behaviour. Anything
less is reported as "N of 10 gate sections" with the open items named. A golden is never accepted by the person who
changed the code without review.

## 14. Loopholes closed

Ways to follow the letter and still ship something broken. Each is a violation of the rule named.

1. **Bleed or decor as a bounds escape.** Marking an overflowing child `fx::bleed` makes P2 skip it. Closed by C13:
   bleed only for a background meeting the page edge, named in the header, and (TODO) a test that it reaches exactly
   the page edge.
2. **Clipping or hiding to pass below the minimum.** A component that hides its content when small passes P2 at any
   size. Closed by C10 (it MUST fail below the minimum, tested) and C9 (every step is declared and golden-tested;
   hiding is a step).
3. **A minimum declared huge.** `"min"` set to the screen size passes every squeeze. Closed by C9: the minimum MUST
   equal the smallest step's real need, as a token relationship a test proves.
4. **Token arithmetic as a hidden size.** `fx::size("layout:x") * 2` used to size an object passes lint `local` only
   if routed cleverly. Closed by C21 and C22 (sums are tokens or derived styles; `fx::size` only in draw and hit
   code).
5. **A token "used" but dead.** Attaching a token that changes nothing satisfies "every look is a token". Closed by
   C23 / P3 (every token read must change pixels live).
6. **Tests that cannot fail.** A test asserting what the component always does proves nothing. Closed by the
   definition of "tested" (section 2) and section 11's "break it" requirement.
7. **Hit areas that do not overlap on paper.** Two buttons with a 1 px gap pass P5 and are still pressed together.
   Closed by C32 (spacing so a finger cannot hit two) and C27 (hit = drawn geometry).
8. **Silent defaults for unknown tokens or props.** A typo falling back to a default would pass every proof.
   Closed by C15 (`fx::use` aborts on an unknown name) and C39 (wrong types fall back visibly to the documented
   default, tested).
9. **States shown by handlers.** Changing a colour in a tap handler with a "token" passes lint. Closed by C17
   (states are LVGL states with a token per state) and P4 (states set before a switch).
10. **A reported pass from a stale build.** Closed by the definition of done (build stamped with the source hash,
    checked before reporting).
11. **Legacy marked as migrated, or migrated without the meta.** A component without `"contract": 2` is not linted.
    Closed by section 2: without the meta it is legacy and counts as not passing.

## 15. Check status

Rules whose check is TODO (each is a gate item; until it exists, review checks the rule by reading the code):

| Rule | Missing check |
|---|---|
| C3 | lint: screens create only components inside a Page body |
| C4 | bounds proof for overlay panels (Bottom sheet, Toast layer to the contract) |
| C6 | lint: `lv_display_get_*`, size reads of objects the component does not own |
| C7 | lint: `w:` token on a component root |
| C9 | lint: every component's meta has `"min"` |
| C11 | generator: each `w:` token inside a component <= its declared min width |
| C12 | `fx` API for an object placed by data |
| C13 | test: each bleed reaches exactly the page edge; lint: `transform_width` / negative margin outside the core |
| C14 | pixel check outside the box (`fx::focus_ring` is the decor API, 2026-10-27) |
| C16 | lint: `theme.h`, `th::` (`lv_font_*`, `LV_SYMBOL_*`, `lv_color_t` and fixed `lay::` / `sp::` / `ic::` numbers: lint rules `font`, `fixed`, 2026-10-27) |
| C44-C47 | per-theme P2 / P6 (bounds, fit); caption buttons, scrollbar, `track` parts, text weight (the apply path, P4 and pixel contrast per theme are in) |
| C49 | every type token's family built at its size (theme fonts) |
| C20 | `gen_layout.mjs`: header row >= `touch:min`, gutter <= inset, row heights >= `touch:min` |
| C21, C22 | lint: `fx::size` / `fx::type_box` outside draw and hit code |
| C23 / P3 | a component whose tokens show only inside a Page (the Header row: alone on a stage it is centred) is checked by its own test, not the loop |
| C24 | lint: `LV_EVENT_DRAW_*` callbacks and `lv_canvas_*` in components |
| C26 / P10 | dirty-area test per drawn component |
| C29 | palette proof for snapped themes |
| C32 / P5 | touch-target size check in the lab proof |
| C33 | per-binding test: every emitted event maps to an HA call |
| C34 | generic test: mount every component and screen, zero events and HA calls |
| C38 | lint: states covered by meta options or the header's n/a list |
| C42 | test: theme switch during a drag |
| C43 | fuzz sizes 0 x 0 .. minimum |

## 16. Files

| File | What |
|---|---|
| `core/fx.h` | the component API (this contract's functions) |
| `core/fx_core.cpp` | shared token styles per display, structure and derived styles, the Pen, `fx::page`, `fx::tune` |
| `tools/gen_fx.mjs` | tokens -> `fx_gen.h` (colours, state roles, sizes with look ranges, type) |
| `tools/contract_lint.mjs` | the lint (P1), run by `build.sh` |
| `tools/lab_test.mjs` | live token propagation (P3), run by `build.sh` |
| `core/test_hooks.cpp` | `fw_test`: `look`, `inset`, `bounds`, `stack`, `scrolls` |
| `core/main.cpp` | `fw_lab`: `tokens`, `tune`, `uses`, `probe`, `proof`, `theme_*` (`core/theme_lab.cpp`) |
| `tests/functional/theme_apply.test.mjs` | the apply path per theme: Facet no-op, live = fresh, reach, refusals, break-it, golden candidates |
| `core/theme_file.h`, `core/theme_file.cpp` | theme files -> Skin: strict JSON, the schema, compile, the static guards, the live slot |
| `tokens/theme.json`, `tools/gen_theme.mjs` | the theme schema as data -> `themes/facet.json` (Facet's values: what every theme inherits) + `theme_gen.h` |
| `tools/lib/looks.mjs` | the one list of look sizes and their ranges (C18, C19) |
| `tools/theme_check.mjs`, `tools/theme_fuzz.sh` | built-in themes checked at build time; the parser fuzzed under ASan + UBSan |
| `tests/functional/_release.mjs` | gate helpers: bounds, stage, look, frame565, golden, faults, fuzz, monkey, leak, frames |
| `components/card.cpp`, `components/dial.cpp` | the reference components (box + steps; drawn + partial redraw) |

## Appendix A: theme file reference

### Keys

| Key | What |
|---|---|
| `schema` | required: `1`. A newer schema is refused (`schema: 2 is newer than this panel (1)`); an older one is upgraded first |
| `id`, `label`, `about` | `id` required, 1 to 32 of `a-z 0-9 -`; `label` up to 48 characters (default the id); `about` up to 1000 |
| `modes` | `["dark", "light"]` or one of them |
| `colour` | the colour tokens per mode |
| `accents` | 1 to 8 accent sets: an `id`, a `label`, and all 5 roles (`accent`, `accent-press`, `accent-dim`, `accent-text`, `on-accent`) for every mode the theme has |
| `size` | look sizes only (`radius:*`, `stroke:*`, `stroke:pixel`) at [1.5x, 1x], within their ranges (C19: refused outside); layout sizes are refused (C18) |
| `type` | sizes at [1.5x, 1x] of `display`, `hero`, `value`, `headline`, `title`, `body`, `label`, each within [floor, the style's line box]; the box, case and letter spacing stay Facet's, so no line moves (C45) |
| `font` | a family per role (`display`, `body`, `label`, `mono`) of this build (`fonts/fonts.json` once theme fonts land; today `inter`, `jetbrains-mono`) |
| `parts` | style blocks per part (below); merge over the presets |
| `shape.corner` | preset: `round` or `square` for cards and controls |
| `shape.cap` | `round` or `square` for arcs, lines, tracks, the scrollbar |
| `shape.knob` | preset: `circle`, `square`, `diamond`, `ring`, `pixel`; `pointer` via `parts.knob.shape` |
| `shape.ticks` | preset: `lines`, `dots`, `blocks`, `none` |
| `shape.card` | preset: `fill`, `outline`, `double` |
| `shape.indicator` | preset: how an on tile shows besides its accent: `text`, `border`, `bar` |
| `shape.icons` | `line` or `pixel` (8 x 8 cells) |
| `shape.snap` | `true`: every drawn primitive in whole cells of the `stroke:pixel` token (C29) |

Every key but `schema` and `id` is optional; what a file leaves out is Facet's (`themes/facet.json`, generated from the
tokens). A preset at Facet's value (the first of each list) adds nothing. Caps (C48): 16 KB a file, 8 levels deep,
1400 values, 8 edge layers a block and 128 a theme; at most 32 errors are listed.

### Parts

| Part | The token that makes an object the part |
|---|---|
| `card` | `part:card` |
| `control` | `part:control` (buttons) |
| `track`, `track-fill` | `part:track`, `part:track-fill` |
| `header`, `list-row` | `part:header`, `part:list-row` |
| `page`, `rule` | `part:page` (the Page frame), `part:rule` (a rule not in a row) |
| `caption` | the header's caption buttons and the scrollbar's arrows (drawn by the core) |
| `scrollbar` | the Body's scrollbar: thumb, optional lane and arrows |
| `knob`, `tick`, `tick-major`, `arc`, `arc-fill` | the Pen's handle, scale marks and arcs |
| `title`, `body`, `label`, `value`, `display` | text by type role |
| `icon` | `fx::icon` |

### Style block (every key optional)

| Key | Applies to | What |
|---|---|---|
| `face` | box parts, caption, scrollbar thumb | an ink, a two-stop gradient (dithered 4 x 4), a checker or blocks pattern |
| `radius`, `pad` | box parts | within the look range; padding on the 4 px grid (or whole cells inside) |
| `text`, `weight` | box parts (their texts), text parts, icon | an ink; `bold` (synthetic) or `regular` |
| `ink` | knob, ticks, arcs | the drawn colour |
| `edges` | box parts, knob, caption, scrollbar thumb, arc | up to 8 layers `{side, w, inset, ink, style}`; the first plain layer at inset 0 is the border |
| `shadow` | box parts, text, icon | `{dx, dy, ink}`, no blur |
| `shape` | knob, ticks | as the presets |
| `buttons` | header | decorative caption buttons, drawn only where the header's own content leaves room |
| `w`, `lane`, `arrows` | scrollbar | width; lane face (the Body's right padding grows by it); arrow buttons |
| `on`, `focus`, `pressed`, `disabled` | any part | a block merged over the base, in that order; a list is replaced whole |

An ink is a colour token, an accent role (`accent`, `accent-press`, `accent-dim`, `accent-text`, `on-accent`), or
`"#RRGGBB"`. A length is pixel cells (of `stroke:pixel`), a size
token name, or `[1.5x, 1x]` px.

### Guardrails (a theme that fails one is refused)

- **Contrast (WCAG), every mode, accent and state, on the exact and the RGB565 values (the worse decides):** every
  declared pair (`tokens/colour.json` `contrast.pairs`) of the foundations and of every contract component, on the
  theme's palette; text and text-dim on every face and every stop of a gradient or pattern, in every combination of
  states, 4.5 (large header text 3.0; disabled states: text-mute 3.0; pressed + disabled is never shown, C36); on-accent on accent 4.5; accent, outer edges of cards and controls, knob,
  major ticks, arc span and scrollbar thumb on their background 3.0 (the strongest outer layer counts). A shadow
  never counts as contrast. Hairlines and minor ticks are decoration.
- **Press feedback (C36):** a pressable part (card, control, list row) whose theme fixes its face looks different
  pressed (face, edges, text or shadow), at rest and on, in every mode and accent.
- **Touch:** `touch:min` and row heights unchanged (they are layout sizes).
- **Type floors (1.5x / 1x):** display 32 / 24, hero 32 / 24, headline 24 / 16, title 24 / 16, value 20 / 14,
  body 20 / 14, label 16 / 12; and at most the style's line box. Icons are sized by the Icons foundation, not a theme.
- **Geometry:** a part's edges fit inside its padding; the page's edges fit inside the inset; a track's edges leave
  a cell for its fill; every radius <= half the height it rounds, in every state; a checker or blocks face needs
  square corners.
- **Fit, bounds, colour, pixels:** P2, P4 and P6 pass for the theme on the live page.
- **Fonts:** every type token's family is built at its size.

Not expressible (by design, for now): bitmaps and textures beyond the listed patterns, opacity, per-part font
families, moving text, blurred shadows or glows, non-box shapes for box parts, rounded patterned faces.
