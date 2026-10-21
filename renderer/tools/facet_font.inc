// Facet bitmap font runtime (pasted into every generated font header by tools/gen_fonts.py).
// A font = sorted glyph table + 4 bpp bitmaps (rows packed, two pixels per byte, high nibble first, no row padding);
// LVGL gets an lv_font_t with glyph callbacks that expand the 4 bpp rows to A8 on demand.
struct FacetGlyph {
  uint32_t cp;
  const uint8_t *data;
  int16_t adv, x, y;  // advance, left bearing, top of the bitmap below the top of the line
  uint16_t w, h;
};
struct FacetFont {
  const FacetGlyph *glyphs;
  int count, ascent, height;  // ascent = baseline from the top of the line, height = line height (px)
  uint8_t bpp;
  lv_font_t lv{};

  const FacetGlyph *find(uint32_t cp) const {
    int lo = 0, hi = count - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      if (glyphs[mid].cp == cp) return &glyphs[mid];
      if (glyphs[mid].cp < cp) lo = mid + 1;
      else hi = mid - 1;
    }
    return nullptr;
  }
  static bool dsc_cb(const lv_font_t *font, lv_font_glyph_dsc_t *d, uint32_t cp, uint32_t) {
    auto *f = (const FacetFont *) font->dsc;
    const FacetGlyph *g = f->find(cp);
    if (!g) return false;
    d->adv_w = g->adv;
    d->ofs_x = g->x;
    d->ofs_y = f->ascent - g->y - g->h;  // LVGL: bottom of the bitmap above the baseline
    d->box_w = g->w;
    d->box_h = g->h;
    d->is_placeholder = 0;
    d->format = LV_FONT_GLYPH_FORMAT_A4;  // bitmap_cb hands LVGL A8 rows
    d->gid.index = cp;
    return true;
  }
  static const void *bitmap_cb(lv_font_glyph_dsc_t *d, lv_draw_buf_t *buf) {
    auto *f = (const FacetFont *) d->resolved_font->dsc;
    const FacetGlyph *g = f->find(d->gid.index);
    if (!g) return nullptr;
    const uint32_t stride = lv_draw_buf_width_to_stride(g->w, LV_COLOR_FORMAT_A8);
    uint32_t bit = 0;  // nibble index into the packed bitmap
    for (int y = 0; y < g->h; y++) {
      uint8_t *row = buf->data + y * stride;
      for (int x = 0; x < g->w; x++, bit++) {
        uint8_t b = g->data[bit >> 1];
        uint8_t v = (bit & 1) ? (b & 0x0F) : (b >> 4);
        row[x] = v * 17;  // 0..15 -> 0..255
      }
    }
    return buf;
  }
  const lv_font_t *get() {
    if (!lv.dsc) {
      lv.dsc = this;
      lv.line_height = height;
      lv.base_line = height - ascent;
      lv.get_glyph_dsc = dsc_cb;
      lv.get_glyph_bitmap = bitmap_cb;
      lv.subpx = LV_FONT_SUBPX_NONE;
      lv.underline_position = -1;
      lv.underline_thickness = 1;
    }
    return &lv;
  }
};
