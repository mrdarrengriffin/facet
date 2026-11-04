// Media art: what is playing as a full-screen picture, the idle screen's "Album art when playing" (screens/idle.cpp,
// Settings > Idle shows), on the contract (renderer/CONTRACT.md, "contract": 2).
//   box      a strip (fx::strip) the size of its parent ("fill"): on the idle screen, the whole screen
//   picture  the album art (art: the Media feed's picture URL, core/images.h Swap) scaled to cover the box edge to edge;
//            only the art: no words over it
//   when     only while it plays (playing) and the art is in: otherwise it draws nothing at all, so what is under it
//            (the idle clock) shows. A new track keeps the old art until the new one is in; no art, a failed fetch or
//            a pause = nothing drawn (no error)
// A read-out: no touch (the idle screen takes the tap under it), no events, no sound. States: n/a (it shows or not).
// Update path: art / playing in place; the whole box is redrawn (a new picture: rare). Contrast: n/a (no ink of its own).
#include "fw.h"
#include "fx.h"
#include "images.h"

namespace {
void changed(void *o);
struct MA {
  lv_obj_t *obj = nullptr;
  std::string url;
  bool playing = false;
  int w = 0, h = 0;
  fw::img::Swap art{changed, this};
};
MA *of(lv_obj_t *o) { return (MA *) lv_obj_get_user_data(o); }
void all(MA *d) { if (d->obj && d->w > 0) fx::redraw(d->obj, -d->w / 2, -d->h / 2, d->w, d->h); }
void changed(void *p) { all((MA *) p); }
// the picture at this size (the box's size is known only after layout: SIZE_CHANGED asks again)
void want(MA *d) {
  if (d->w > 0 && d->h > 0) d->art.set(d->url, d->w, d->h);
}
void draw(const fx::Pen &p) {
  const MA *d = of(p.obj);
  const lv_image_dsc_t *px = d ? d->art.pixels() : nullptr;
  if (!px || !d->playing) return;  // nothing: what is under it shows
  p.image(-p.w / 2, -p.h / 2, px, d->w, d->h);  // its box (the pixels may be stored smaller: images.h max_side)
}
bool playing_of(fw::Props p) {
  if (p["playing"].is<bool>()) return p["playing"].as<bool>();
  const char *v = p["playing"] | "true";
  return !strcmp(v, "true") || !strcmp(v, "1");
}
void apply(MA *d, fw::Props p) {
  d->url = p["art"] | "";
  d->playing = playing_of(p);
  want(d);
}
}  // namespace

FW_COMPONENT(media_art, "Media art", "components", R"({
  "about": "What is playing as a full-screen picture: the album art edge to edge, nothing over it. The idle screen shows it while music plays (Settings > Idle shows). It draws nothing while paused or without art, so the clock under it shows.",
  "contract": 2,
  "parts": [],
  "frame": [480, 480],
  "props": [
    {"name": "art", "about": "The album art's address (the player's picture). The default is the docs' demo picture.", "type": "text", "default": "/api/media_player_proxy/media_player.sitting_room_speaker?token=demo&cache=sitting_room_speaker"},
    {"name": "playing", "about": "Whether it is playing: only then is the picture shown.", "type": "bool", "default": true}
  ],
  "variants": []
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
  const bool pl = d->playing;
  apply(d, p);
  if (pl != d->playing) all(d);
  return true;
}
