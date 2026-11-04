// Media art: what is playing as a full-screen picture, the idle screen's "Album art when playing" (screens/idle.cpp,
// Settings > Idle shows), on the contract (renderer/CONTRACT.md, "contract": 2).
//   box      a strip (fx::strip) the size of its parent ("fill"): on the idle screen, the whole screen
//   picture  the album art (art: the Media feed's picture URL, core/images.h Swap) scaled to cover the box edge to edge,
//            its bottom darkened by the image pipeline's shade (images.h: a smoothstep ramp, then SHADE_MIN; dithered, no
//            opacity: C28) so the words read over any art: at its darkest from space:5 above the title's top down
//   words    centred, layout:inset above the bottom edge: the title (type:headline, fg:art-text) over the artist
//            (type:body, fg:art-text-dim), gap:2 apart, each one line cut to the inset width with "..." (ASCII dots)
//   when     only while it plays (playing) and the art is in: otherwise it draws nothing at all, so what is under it
//            (the idle clock) shows. A new track keeps the old art until the new one is in; no art, a failed fetch or
//            a pause = nothing drawn (no error)
// A read-out: no touch (the idle screen takes the tap under it), no events, no sound. States: n/a (it shows or not).
// Update path: art / title / artist / playing in place; the whole box is redrawn (a new picture or new words: rare).
// Contrast pairs: tokens/colour.json section "media_art" (the text on the shade's lightest: art-shade).
#include <cmath>
#include "fw.h"
#include "fx.h"
#include "images.h"

namespace {
void changed(void *o);
struct MA {
  lv_obj_t *obj = nullptr;
  std::string url, title, artist;
  bool playing = false;
  int w = 0, h = 0;
  fw::img::Swap art{changed, this};
};
MA *of(lv_obj_t *o) { return (MA *) lv_obj_get_user_data(o); }
int S(const char *t) { return fx::size(t); }
void all(MA *d) { if (d->obj && d->w > 0) fx::redraw(d->obj, -d->w / 2, -d->h / 2, d->w, d->h); }
void changed(void *p) { all((MA *) p); }
// the words' block: its height from the bottom edge (the inset, the two line boxes, the gap); the shade is at its
// darkest over it and space:5 more (the shade's lower half), its ramp above
int words_h() { return S("layout:inset") + fx::type_box("type:body") + S("space:2") + fx::type_box("type:headline"); }
// the picture at this size (the box's size is known only after layout: SIZE_CHANGED asks again)
void want(MA *d) {
  if (d->w > 0 && d->h > 0) d->art.set(d->url, d->w, d->h, 0, std::min(d->h, 2 * (words_h() + S("space:5"))));
}
// one line of s in type, cut with "..." to w px (ASCII: every type's glyph set has the dots)
std::string fit(const fx::Pen &p, std::string s, const char *type, int w) {
  if (p.text_w(s.c_str(), type) <= w) return s;
  while (!s.empty()) {
    s.pop_back();
    while (!s.empty() && ((unsigned char) s.back() & 0xC0) == 0x80) s.pop_back();  // whole UTF-8 characters
    if (p.text_w((s + "...").c_str(), type) <= w) return s + "...";
  }
  return "...";
}
void draw(const fx::Pen &p) {
  const MA *d = of(p.obj);
  const lv_image_dsc_t *px = d ? d->art.pixels() : nullptr;
  if (!px || !d->playing) return;  // nothing: what is under it shows
  p.image(-p.w / 2, -p.h / 2, px);
  const int in = S("layout:inset"), w = p.w - 2 * in;
  if (w < 1) return;
  const int hb = fx::type_box("type:body"), hh = fx::type_box("type:headline");
  const int ya = p.h / 2 - in - hb / 2, yt = ya - hb / 2 - S("space:2") - hh / 2;
  if (!d->artist.empty()) p.text(0, ya, fit(p, d->artist, "type:body", w).c_str(), "type:body", "fg:art-text-dim");
  if (!d->title.empty()) p.text(0, d->artist.empty() ? ya + hb / 2 - hh / 2 : yt, fit(p, d->title, "type:headline", w).c_str(), "type:headline", "fg:art-text");
}
bool playing_of(fw::Props p) {
  if (p["playing"].is<bool>()) return p["playing"].as<bool>();
  const char *v = p["playing"] | "true";
  return !strcmp(v, "true") || !strcmp(v, "1");
}
void apply(MA *d, fw::Props p) {
  d->url = p["art"] | "";
  d->title = p["title"] | "";
  d->artist = p["artist"] | "";
  d->playing = playing_of(p);
  want(d);
}
}  // namespace

FW_COMPONENT(media_art, "Media art", "components", R"({
  "about": "What is playing as a full-screen picture: the album art edge to edge with the track and the artist over its darkened foot. The idle screen shows it while music plays (Settings > Idle shows). It draws nothing while paused or without art, so the clock under it shows.",
  "contract": 2,
  "parts": [],
  "frame": [480, 480],
  "props": [
    {"name": "art", "about": "The album art's address (the player's picture).", "type": "text", "default": ""},
    {"name": "title", "about": "The track, one line.", "type": "text", "default": "Harbour Lights"},
    {"name": "artist", "about": "The artist, one line under it.", "type": "text", "default": "The Lanterns"},
    {"name": "playing", "about": "Whether it is playing: only then is the picture shown.", "type": "bool", "default": true}
  ],
  "variants": [
    {"label": "no art: draws nothing", "props": {}},
    {"label": "paused: draws nothing", "props": {"playing": false}}
  ]
})") {
  auto *d = new MA;
  lv_obj_t *o = fx::strip(parent, draw, "fill");
  fx::name(o, "media_art");
  lv_obj_set_user_data(o, d);
  d->obj = o;
  lv_obj_remove_flag(o, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_event_cb(o, [](lv_event_t *e) {
    auto *d = (MA *) lv_event_get_user_data(e);
    d->obj = nullptr;
    delete d;  // its Swap lets the art go
  }, LV_EVENT_DELETE, d);
  lv_obj_add_event_cb(o, [](lv_event_t *e) {
    auto *d = (MA *) lv_event_get_user_data(e);
    lv_obj_t *o = (lv_obj_t *) lv_event_get_current_target(e);
    const int w = lv_obj_get_content_width(o), h = lv_obj_get_content_height(o);
    if (w == d->w && h == d->h) return;
    d->w = w, d->h = h;
    want(d);
  }, LV_EVENT_SIZE_CHANGED, d);
  apply(d, p);
}

FW_UPDATE(media_art) {
  MA *d = of(root);
  if (!d) return false;
  const std::string t = d->title, a = d->artist;
  const bool pl = d->playing;
  apply(d, p);
  if (t != d->title || a != d->artist || pl != d->playing) all(d);
  return true;
}
