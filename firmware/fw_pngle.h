/* Facet firmware (firmware/fw_imgdec.h): pngle by kikuchan, MIT, https://github.com/kikuchan/pngle (b1c6819, 2025-05-18); unchanged but for the names block (and a cast in read_uint32). */
/* Facet: own names for every exported symbol, so this copy never meets another pngle / miniz (an ESPHome component,
 * the chip ROM's tinfl) at link time */
#ifndef FW_PNGLE_NAMES
#define FW_PNGLE_NAMES
#define PNGLE_NO_GAMMA_CORRECTION
#include <stddef.h>
#define PNGLE_T_SIZE fw_PNGLE_T_SIZE
#define mz_adler32 fw_mz_adler32
#define mz_crc32 fw_mz_crc32
#define mz_free fw_mz_free
#define pngle_destroy fw_pngle_destroy
#define pngle_error fw_pngle_error
#define pngle_feed fw_pngle_feed
#define pngle_get_background_color fw_pngle_get_background_color
#define pngle_get_height fw_pngle_get_height
#define pngle_get_ihdr fw_pngle_get_ihdr
#define pngle_get_user_data fw_pngle_get_user_data
#define pngle_get_width fw_pngle_get_width
#define pngle_new fw_pngle_new
#define pngle_reset fw_pngle_reset
#define pngle_set_display_gamma fw_pngle_set_display_gamma
#define pngle_set_done_callback fw_pngle_set_done_callback
#define pngle_set_draw_callback fw_pngle_set_draw_callback
#define pngle_set_init_callback fw_pngle_set_init_callback
#define pngle_set_user_data fw_pngle_set_user_data
#define tinfl_decompress fw_tinfl_decompress
#define tinfl_decompress_mem_to_callback fw_tinfl_decompress_mem_to_callback
#define tinfl_decompress_mem_to_heap fw_tinfl_decompress_mem_to_heap
#define tinfl_decompress_mem_to_mem fw_tinfl_decompress_mem_to_mem
#endif
/*-
 * MIT License
 *
 * Copyright (c) 2019 kikuchan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __PNGLE_H__
#define __PNGLE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Main Pngle object
typedef struct _pngle_t pngle_t;

extern const size_t PNGLE_T_SIZE;  // size of pngle_t structure

// Callback signatures
typedef void (*pngle_init_callback_t)(pngle_t *pngle, uint32_t w, uint32_t h);
typedef void (*pngle_draw_callback_t)(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4]);
typedef void (*pngle_done_callback_t)(pngle_t *pngle);

// ----------------
// Basic interfaces
// ----------------
pngle_t *pngle_new();
void pngle_destroy(pngle_t *pngle);
void pngle_reset(pngle_t *pngle); // clear its internal state (not applied to pngle_set_* functions)
const char *pngle_error(pngle_t *pngle);
int pngle_feed(pngle_t *pngle, const void *buf, size_t len); // returns -1: On error, 0: Need more data, n: n bytes eaten

uint32_t pngle_get_width(pngle_t *pngle);
uint32_t pngle_get_height(pngle_t *pngle);
const uint8_t *pngle_get_background_color(pngle_t *pngle);

void pngle_set_init_callback(pngle_t *png, pngle_init_callback_t callback);
void pngle_set_draw_callback(pngle_t *png, pngle_draw_callback_t callback);
void pngle_set_done_callback(pngle_t *png, pngle_done_callback_t callback);

void pngle_set_display_gamma(pngle_t *pngle, double display_gamma); // enables gamma correction by specifying display gamma, typically 2.2. No effect when gAMA chunk is missing

void pngle_set_user_data(pngle_t *pngle, void *user_data);
void *pngle_get_user_data(pngle_t *pngle);

// ----------------
// Debug interfaces
// ----------------

typedef struct _pngle_ihdr_t {
	uint32_t width;
	uint32_t height;
	uint8_t depth;
	uint8_t color_type;
	uint8_t compression;
	uint8_t filter;
	uint8_t interlace;
} pngle_ihdr_t;

// Get IHDR information
pngle_ihdr_t *pngle_get_ihdr(pngle_t *pngle);

#ifdef __cplusplus
}
#endif

#endif /* __PNGLE_H__ */
