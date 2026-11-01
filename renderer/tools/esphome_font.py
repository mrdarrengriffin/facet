"""The glyph rasteriser of ESPHome's `font:` component, so tools/gen_fonts.py draws exactly what a panel's ESPHome fonts
do without the esphome package. Copied unchanged from ESPHome 2026.9.1, esphome/components/font/__init__.py
(glyph_comparator, pt_to_px, GlyphInfo, glyph_to_glyphinfo); ESPHome's Python code is MIT:

Copyright (c) 2019 ESPHome

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""
import logging

from freetype import FT_LOAD_NO_BITMAP, FT_LOAD_RENDER, FT_LOAD_TARGET_MONO, ft_pixel_mode_mono

_LOGGER = logging.getLogger(__name__)


def glyph_comparator(x, y):
    x_ = x.encode("utf-8")
    y_ = y.encode("utf-8")

    for c in range(min(len(x_), len(y_))):
        if x_[c] < y_[c]:
            return -1
        if x_[c] > y_[c]:
            return 1

    if len(x_) < len(y_):
        return -1
    if len(x_) > len(y_):
        return 1
    return 0



def pt_to_px(pt):
    """
    Convert a point size to pixels, rounding up to the nearest pixel
    """
    return (pt + 63) // 64



class GlyphInfo:
    def __init__(self, glyph, data, advance, offset_x, offset_y, width, height):
        self.glyph = glyph
        self.bitmap_data = data
        self.advance = advance
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.width = width
        self.height = height


def glyph_to_glyphinfo(glyph, font, size, bpp):
    # Convert to 32 bit unicode codepoint
    glyph = ord(glyph)
    scale = 256 // (1 << bpp)
    if not font.is_scalable:
        sizes = [pt_to_px(x.size) for x in font.available_sizes]
        if size in sizes:
            font.select_size(sizes.index(size))
    else:
        font.set_pixel_sizes(size, 0)
    flags = FT_LOAD_RENDER
    if bpp != 1:
        flags |= FT_LOAD_NO_BITMAP
    else:
        flags |= FT_LOAD_TARGET_MONO
    font.load_char(glyph, flags)
    width = font.glyph.bitmap.width
    height = font.glyph.bitmap.rows
    buffer = font.glyph.bitmap.buffer
    pitch = font.glyph.bitmap.pitch
    glyph_data = [0] * ((height * width * bpp + 7) // 8)
    src_mode = font.glyph.bitmap.pixel_mode
    pos = 0
    for y in range(height):
        for x in range(width):
            if src_mode == ft_pixel_mode_mono:
                pixel = (
                    (1 << bpp) - 1
                    if buffer[y * pitch + x // 8] & (1 << (7 - x % 8))
                    else 0
                )
            else:
                pixel = buffer[y * pitch + x] // scale
            for bit_num in range(bpp):
                if pixel & (1 << (bpp - bit_num - 1)):
                    glyph_data[pos // 8] |= 0x80 >> (pos % 8)
                pos += 1
    ascender = pt_to_px(font.size.ascender)
    if ascender == 0:
        if not font.is_scalable:
            ascender = size
        else:
            _LOGGER.error(
                "Unable to determine ascender of font %s %s",
                font.family_name,
                font.style_name,
            )
    return GlyphInfo(
        glyph,
        glyph_data,
        pt_to_px(font.glyph.metrics.horiAdvance),
        font.glyph.bitmap_left,
        ascender - font.glyph.bitmap_top,
        width,
        height,
    )
