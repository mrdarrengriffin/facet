// QR block: the Facet QR block (docs: Components > Lists > QR block, @facet/ui lists.ts qrBlock() + qr.ts): a QR code
// with a caption, dark on light in BOTH themes (scanners want dark on light): lay::qr_block_w wide (208 / 140),
// colour token qr-bg, rad::card, padding gutter, a centred column: LVGL's own QR widget (lv_qrcode, as the docs say
// for the panel) lay::qr square in qr-fg on qr-bg with its quiet zone, sp::s2, the caption (c-caps-sm, qr-fg). Empty
// text = no code. Static. Update path: new text re-encodes in place, caption in place. Contrast pairs: "qr_block".
#include "fw.h"

namespace {
struct QR { lv_obj_t *code = nullptr, *cap = nullptr; std::string text; };
QR *of(lv_obj_t *o) { return (QR *) lv_obj_get_user_data(o); }
void encode(QR *d, const char *s) {
  d->text = s;
  if (*s) { lv_obj_remove_flag(d->code, LV_OBJ_FLAG_HIDDEN); lv_qrcode_update(d->code, s, (uint32_t) strlen(s)); }
  else lv_obj_add_flag(d->code, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

FW_COMPONENT(qr_block, "QR block", "components", R"({
  "about": "A QR code with a caption, dark on light in both themes so phones can read it.",
  "frame": [224, 256],
  "props": [
    {"name": "text", "type": "text", "default": "https://example.com/orchard-labs/lantern/pull/412"},
    {"name": "caption", "type": "text", "default": "open on your phone"}
  ],
  "variants": [
    {"label": "a pull request", "props": {}},
    {"label": "short text", "props": {"text": "https://example.com", "caption": "scan me"}}
  ]
})") {
  const fw::Theme &t = *fw::T;
  auto *d = new QR;
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_user_data(c, d);
  lv_obj_add_event_cb(c, [](lv_event_t *e) { delete (QR *) lv_event_get_user_data(e); }, LV_EVENT_DELETE, d);
  lv_obj_remove_style_all(c);
  lv_obj_remove_flag(c, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_size(c, fw::lay::qr_block_w, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(c, fw::rad::card, 0);
  lv_obj_set_style_bg_color(c, t.qr_bg, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(c, fw::lay::gutter, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(c, fw::sp::s2, 0);
  d->code = lv_qrcode_create(c);
  lv_qrcode_set_size(d->code, fw::lay::qr);
  lv_qrcode_set_dark_color(d->code, t.qr_fg);
  lv_qrcode_set_light_color(d->code, t.qr_bg);
  lv_qrcode_set_quiet_zone(d->code, true);
  lv_obj_remove_flag(d->code, (lv_obj_flag_t) (LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  encode(d, p["text"] | "https://example.com/orchard-labs/lantern/pull/412");
  d->cap = fw::text(c, fw::ty::c_caps_sm, p["caption"] | "open on your phone", t.qr_fg);
  lv_obj_set_height(d->cap, fw::style(fw::ty::c_caps_sm).box);
}

FW_UPDATE(qr_block) {
  auto *d = of(root);
  if (!d) return false;
  const char *s = p["text"] | "https://example.com/orchard-labs/lantern/pull/412";
  if (d->text != s) encode(d, s);
  fw::set_text(d->cap, fw::ty::c_caps_sm, p["caption"] | "open on your phone");
  return true;
}
