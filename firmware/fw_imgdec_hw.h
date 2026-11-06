// Hardware JPEG on the ESP32-P4 (the Waveshare): the chip's JPEG decoder makes the RGB888 picture, its PPA scales it
// towards the biggest view's stored size, then the same row Sinks as the software path (renderer core/img_scale.h:
// the area filter to each box, the round mask, the shade, the dither to RGB565) finish every view. The PPA stops at
// twice the stored size (its scaler samples; the area filter after it does the rest, so no aliasing). Anything it
// cannot do (progressive, a picture too big for memory, any driver error) = false: the software path decodes it
// (fw_imgdec.h detail::jpeg). Installed by fw_images.h on a chip with both engines (SOC_JPEG_DECODE_SUPPORTED, SOC_PPA_SUPPORTED).
// The channel order (decoder and PPA) is found once from a built-in 16 x 16 picture of a known colour, not assumed.
#pragma once
#include <cmath>
#include "driver/jpeg_decode.h"
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esphome/core/log.h"
#include "fw_imgdec.h"

namespace fw::imgdec::hw {
// a 16 x 16 baseline JPEG, 4:4:4, every pixel R 200, G 0, B 100 (made with Pillow): which output byte is which channel
inline const uint8_t PROBE[] = {
    255,216,255,224,0,16,74,70,73,70,0,1,1,0,0,1,0,1,0,0,255,219,0,67,0,2,1,1,1,1,1,2,
    1,1,1,2,2,2,2,2,4,3,2,2,2,2,5,4,4,3,4,6,5,6,6,6,5,6,6,6,7,9,8,6,
    7,9,7,6,6,8,11,8,9,10,10,10,10,10,6,8,11,12,11,10,12,9,10,10,10,255,219,0,67,1,2,2,
    2,2,2,2,5,3,3,5,10,7,6,7,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,255,192,
    0,17,8,0,16,0,16,3,1,17,0,2,17,1,3,17,1,255,196,0,31,0,0,1,5,1,1,1,1,1,1,0,
    0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,255,196,0,181,16,0,2,1,3,3,2,4,3,5,
    5,4,4,0,0,1,125,1,2,3,0,4,17,5,18,33,49,65,6,19,81,97,7,34,113,20,50,129,145,161,8,35,
    66,177,193,21,82,209,240,36,51,98,114,130,9,10,22,23,24,25,26,37,38,39,40,41,42,52,53,54,55,56,57,58,
    67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,118,119,120,121,122,
    131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,169,170,178,179,180,181,182,183,
    184,185,186,194,195,196,197,198,199,200,201,202,210,211,212,213,214,215,216,217,218,225,226,227,228,229,230,231,232,233,234,241,
    242,243,244,245,246,247,248,249,250,255,196,0,31,1,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,
    2,3,4,5,6,7,8,9,10,11,255,196,0,181,17,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119,0,
    1,2,3,17,4,5,33,49,6,18,65,81,7,97,113,19,34,50,129,8,20,66,145,161,177,193,9,35,51,82,240,21,
    98,114,209,10,22,36,52,225,37,241,23,24,25,26,38,39,40,41,42,53,54,55,56,57,58,67,68,69,70,71,72,73,
    74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,118,119,120,121,122,130,131,132,133,134,135,136,
    137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,169,170,178,179,180,181,182,183,184,185,186,194,195,196,
    197,198,199,200,201,202,210,211,212,213,214,215,216,217,218,226,227,228,229,230,231,232,233,234,242,243,244,245,246,247,248,249,
    250,255,218,0,12,3,1,0,2,17,3,17,0,63,0,248,110,191,64,63,215,0,160,2,128,10,0,255,217};
struct Engine {
  bool tried = false, ok = false;
  jpeg_decoder_handle_t dec = nullptr;
  ppa_client_handle_t ppa = nullptr;
  int ri = 0, gi = 1, bi = 2;  // the decoder's RGB888 bytes: where R, G and B are
  int pr = 2, pg = 1, pb = 0, pa = 3;  // the PPA's ARGB8888 bytes
};
inline Engine &E() { static Engine e; return e; }
inline void *dma_alloc(size_t n, bool in, size_t &got) {
  jpeg_decode_memory_alloc_cfg_t c = {};
  c.buffer_direction = in ? JPEG_DEC_ALLOC_INPUT_BUFFER : JPEG_DEC_ALLOC_OUTPUT_BUFFER;
  return jpeg_alloc_decoder_mem(n, &c, &got);
}
inline size_t align(size_t n) { return (n + 127) & ~(size_t) 127; }  // the P4's L1 + L2 cache lines (PPA, DMA buffers)
// the picture as the decoder writes it: process_w x process_h (the MCU-padded size), RGB888
struct Raw { uint8_t *px = nullptr; size_t size = 0; int w = 0, h = 0, pw = 0, ph = 0; };
inline void free_raw(Raw &r) { if (r.px) heap_caps_free(r.px); r.px = nullptr; }
inline bool decode_raw(const uint8_t *b, size_t n, Raw &out, size_t most) {
  Engine &e = E();
  jpeg_decode_picture_info_t info = {};
  if (jpeg_decoder_get_info(b, n, &info) != ESP_OK || !info.width || !info.height) return false;
  // the decoder pads each side to its MCU (16 across for 4:2:0 and 4:2:2, else 8; 16 down at most)
  const int mcu = info.sample_method == JPEG_DOWN_SAMPLING_YUV420 || info.sample_method == JPEG_DOWN_SAMPLING_YUV422 ? 16 : 8;
  const int pw = ((int) info.width + mcu - 1) / mcu * mcu, ph = ((int) info.height + 15) / 16 * 16;
  const size_t need = align((size_t) pw * ph * 3);
  if (need > most) return false;
  size_t in_got = 0, out_got = 0;
  uint8_t *in = (uint8_t *) dma_alloc(align(n), true, in_got);  // the bitstream in DMA-able, aligned memory
  if (!in) return false;
  memcpy(in, b, n);
  uint8_t *px = (uint8_t *) dma_alloc(need, false, out_got);
  if (!px) { heap_caps_free(in); return false; }
  jpeg_decode_cfg_t c = {};
  c.output_format = JPEG_DECODE_OUT_FORMAT_RGB888, c.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB, c.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
  uint32_t got = 0;
  const esp_err_t r = jpeg_decoder_process(e.dec, &c, in, n, px, out_got, &got);
  heap_caps_free(in);
  if (r != ESP_OK || !got) { heap_caps_free(px); return false; }
  out.px = px, out.size = out_got, out.w = (int) info.width, out.h = (int) info.height;
  out.pw = pw, out.ph = (int) (got / 3 / pw);  // the rows as written (the MCU-padded height)
  if (out.ph < out.h) { heap_caps_free(px); out.px = nullptr; return false; }
  return true;
}
// raw -> (ow x oh) ARGB8888 through the PPA; ow, oh even, the scale a 1/16 step
inline bool scale(const Raw &raw, float k, uint8_t *&out, size_t &out_size, int &ow, int &oh) {
  Engine &e = E();
  const int bw = raw.w & ~1, bh = raw.h & ~1;
  ow = std::max(2, (int) std::floor(bw * k)) & ~1, oh = std::max(2, (int) std::floor(bh * k)) & ~1;
  out_size = align((size_t) ow * oh * 4);
  out = (uint8_t *) heap_caps_aligned_calloc(128, 1, out_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!out) return false;
  ppa_srm_oper_config_t c = {};
  c.in.buffer = raw.px, c.in.pic_w = raw.pw, c.in.pic_h = raw.ph, c.in.block_w = bw, c.in.block_h = bh;
  c.in.srm_cm = PPA_SRM_COLOR_MODE_RGB888;
  c.out.buffer = out, c.out.buffer_size = out_size, c.out.pic_w = ow, c.out.pic_h = oh, c.out.srm_cm = PPA_SRM_COLOR_MODE_ARGB8888;
  c.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  c.scale_x = (float) ow / bw, c.scale_y = (float) oh / bh;
  c.alpha_update_mode = PPA_ALPHA_FIX_VALUE, c.alpha_fix_val = 255;
  c.mode = PPA_TRANS_MODE_BLOCKING;
  if (ppa_do_scale_rotate_mirror(e.ppa, &c) != ESP_OK) { heap_caps_free(out); out = nullptr; return false; }
  return true;
}
// once: the engines, then the probe picture through both (which byte is which channel); false = the software path
inline bool ready() {
  Engine &e = E();
  if (e.tried) return e.ok;
  e.tried = true;
  jpeg_decode_engine_cfg_t dc = {};
  dc.timeout_ms = 200;
  if (jpeg_new_decoder_engine(&dc, &e.dec) != ESP_OK) { ESP_LOGW("facet_img", "hardware JPEG: no decoder engine, software decode"); return false; }
  ppa_client_config_t pc = {};
  pc.oper_type = PPA_OPERATION_SRM;
  if (ppa_register_client(&pc, &e.ppa) != ESP_OK) { ESP_LOGW("facet_img", "hardware JPEG: no PPA client, software decode"); return false; }
  Raw raw;
  if (!decode_raw(PROBE, sizeof PROBE, raw, 64 * 1024)) { ESP_LOGW("facet_img", "hardware JPEG: the probe did not decode, software decode"); return false; }
  // R 200, G 0, B 100: the byte near 200 is R, near 100 is B, the rest G
  auto near = [](int v, int want) { return std::abs(v - want) < 24; };
  const uint8_t *p = raw.px + ((size_t) 8 * raw.pw + 8) * 3;
  int ri = -1, bi = -1;
  for (int i = 0; i < 3; i++) { if (near(p[i], 200)) ri = i; else if (near(p[i], 100)) bi = i; }
  uint8_t *o = nullptr;
  size_t os = 0;
  int ow = 0, oh = 0;
  const bool scaled = ri >= 0 && bi >= 0 && scale(raw, 0.5f, o, os, ow, oh);
  free_raw(raw);
  if (!scaled) { ESP_LOGW("facet_img", "hardware JPEG: the probe's colours are wrong, software decode"); return false; }
  const uint8_t *q = o + ((size_t) (oh / 2) * ow + ow / 2) * 4;
  int pr = -1, pb = -1, pa = -1;
  for (int i = 0; i < 4; i++) { if (q[i] == 255) pa = i; else if (near(q[i], 200)) pr = i; else if (near(q[i], 100)) pb = i; }
  heap_caps_free(o);
  if (pr < 0 || pb < 0 || pa < 0) { ESP_LOGW("facet_img", "hardware JPEG: the PPA's colours are wrong, software decode"); return false; }
  e.ri = ri, e.bi = bi, e.gi = 3 - ri - bi, e.pr = pr, e.pb = pb, e.pa = pa, e.pg = 6 - pr - pb - pa;
  e.ok = true;
  ESP_LOGI("facet_img", "hardware JPEG + PPA ready");
  return true;
}
// the hook (fw_imgdec.h hw_jpeg): the boxes' pixels from the hardware, or false for the software path
inline bool jpeg(const uint8_t *b, size_t n, long, size_t mem_max, const std::vector<Box> &boxes, Result &r) {
  if (!ready()) return false;
  Engine &e = E();
  Raw raw;
  if (!decode_raw(b, n, raw, mem_max)) return false;
  // the PPA's target: twice the biggest stored size (the area filter does the rest), never above the source
  int tw = 0, th = 0;
  for (const Box &bx : boxes) {
    int ow, oh;
    fw::img::stored(bx, raw.w, raw.h, 0, ow, oh);
    if (bx.s == fw::img::pixel) tw = raw.w, th = raw.h;  // pixel art: the source's own pixels
    tw = std::max(tw, ow), th = std::max(th, oh);
  }
  float k = std::min(1.0f, std::max(2.0f * tw / raw.w, 2.0f * th / raw.h));
  k = std::max(1.0f / 16, std::ceil(k * 16) / 16);
  std::unique_ptr<fw::img::Sink[]> sinks;
  size_t need = 0;
  uint8_t *row = nullptr;
  bool ok = false;
  if (k < 1.0f) {  // the PPA: ARGB8888 at (ow x oh), then the rows
    uint8_t *o = nullptr;
    size_t os = 0;
    int ow = 0, oh = 0;
    const bool s = scale(raw, k, o, os, ow, oh);
    free_raw(raw);
    if (!s) return false;
    if (detail::plan(boxes, ow, oh, 0, false, sinks, need, r) && need + os <= mem_max && detail::begin(sinks, boxes.size(), r) &&
        (row = (uint8_t *) FW_IMGDEC_ALLOC((size_t) ow * 4))) {
      for (int y = 0; y < oh; y++) {
        const uint8_t *q = o + (size_t) y * ow * 4;
        for (int x = 0; x < ow; x++, q += 4) row[x * 4] = q[e.pr], row[x * 4 + 1] = q[e.pg], row[x * 4 + 2] = q[e.pb], row[x * 4 + 3] = 255;
        detail::feed(sinks, boxes.size(), row);
      }
      ok = detail::finish(sinks, boxes.size(), r);
      r.sw = ow, r.sh = oh, r.need = r.need_full = need + os;
    }
    heap_caps_free(o);
  } else {  // the box is as big as the source or bigger: the decoder's rows straight in
    if (detail::plan(boxes, raw.w, raw.h, 0, false, sinks, need, r) && need + raw.size <= mem_max && detail::begin(sinks, boxes.size(), r) &&
        (row = (uint8_t *) FW_IMGDEC_ALLOC((size_t) raw.w * 4))) {
      for (int y = 0; y < raw.h; y++) {
        const uint8_t *q = raw.px + (size_t) y * raw.pw * 3;
        for (int x = 0; x < raw.w; x++, q += 3) row[x * 4] = q[e.ri], row[x * 4 + 1] = q[e.gi], row[x * 4 + 2] = q[e.bi], row[x * 4 + 3] = 255;
        detail::feed(sinks, boxes.size(), row);
      }
      ok = detail::finish(sinks, boxes.size(), r);
      r.sw = raw.w, r.sh = raw.h, r.need = r.need_full = need + raw.size;
    }
    free_raw(raw);
  }
  if (row) FW_IMGDEC_FREE(row);
  if (!ok) { release(r); r.err.clear(); return false; }
  r.sc = 0, r.side = 0, r.hw = true;
  return true;
}
}  // namespace fw::imgdec::hw
