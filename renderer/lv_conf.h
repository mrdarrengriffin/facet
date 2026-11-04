// LVGL configuration of the WASM build and ASan (build.sh copies it into the generated headers; LVGL 9.5.0 in
// third_party/lvgl). The panel firmware's own lv_conf.h (ESPHome's lvgl component, its board YAML's LV_* build flags)
// with the host's changes: no LVGL log, the C library's malloc, no OS, an abort() on an LVGL assert. Keep the drawing
// options the same as the panels', or the workbench shows what a panel does not.
#pragma once
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 0
#define LV_BUILD_DEMOS 0
#define LV_BUILD_EXAMPLES 0
#define LV_COLOR_16_SWAP 0
#define LV_COLOR_CHROMA_KEY lv_color_make(0, 4, 0)
#define LV_COLOR_DEPTH 16
#define LV_DEF_REFR_PERIOD 16
#define LV_DRAW_BUF_ALIGN 64
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_SW_COMPLEX 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 0
#define LV_DRAW_SW_SUPPORT_A8 0
#define LV_DRAW_SW_SUPPORT_AL88 0
#define LV_DRAW_SW_SUPPORT_ARGB8888 1
#define LV_DRAW_SW_SUPPORT_I1 0
#define LV_DRAW_SW_SUPPORT_L8 0
#define LV_DRAW_SW_SUPPORT_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_RGB565 1
#define LV_DRAW_SW_SUPPORT_RGB565A8 1  // also LVGL's scaled RGB565 draws (a picture kept under its box: core/img_scale.h); the boards' YAML build flag
#define LV_DRAW_SW_SUPPORT_RGB888 1
#define LV_DRAW_SW_SUPPORT_SWAPPED 0
#define LV_DRAW_SW_SUPPORT_XRGB8888 0
#define LV_FILE_EXPLORER_QUICK_ACCESS 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_FONT_MONTSERRAT_14 1
#define LV_GRADIENT_MAX_STOPS 2
#define LV_IME_PINYIN_USE_DEFAULT_DICT 0
#define LV_IME_PINYIN_USE_K9_MODE 0
#define LV_LABEL_LONG_TXT_HINT 0
#define LV_LABEL_TEXT_SELECTION 0
#define LV_LINUX_DRM_USE_EGL 0
#define LV_LINUX_FBDEV_MMAP 0
#define LV_LOG_USE_FILE_LINE 0
#define LV_LOG_USE_TIMESTAMP 0
#define LV_PROFILER_BUILTIN_DEFAULT_ENABLE 0
#define LV_PROFILER_CACHE 0
#define LV_PROFILER_DECODER 0
#define LV_PROFILER_DRAW 0
#define LV_PROFILER_EVENT 0
#define LV_PROFILER_FONT 0
#define LV_PROFILER_FS 0
#define LV_PROFILER_INDEV 0
#define LV_PROFILER_LAYOUT 0
#define LV_PROFILER_REFR 0
#define LV_PROFILER_TIMER 0
#define LV_SDL_USE_EGL 0
#define LV_TEST_SCREENSHOT_CREATE_REFERENCE_IMAGE 0
#define LV_THEME_DEFAULT_GROW 0
#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 1
#define LV_USE_ARCLABEL 0
#define LV_USE_BAR 1
#define LV_USE_BTNMATRIX_BTN 0
#define LV_USE_BUTTON 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CALENDAR_HEADER_ARROW 0
#define LV_USE_CALENDAR_HEADER_DROPDOWN 0
#define LV_USE_CANVAS 1
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_CONTAINER 0
#define LV_USE_DRAW_G2D 0
#define LV_USE_DRAW_PXP 0
#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0
#define LV_USE_DROPDOWN 0
#define LV_USE_DROPDOWN_LIST 0
#define LV_USE_EGL 0
#define LV_USE_FLEX 1
#define LV_USE_FONT_PLACEHOLDER 0  // a character the font lacks (an emoji in an HA name) draws nothing, never a box
#define LV_USE_FREERTOS_TASK_NOTIFY 0
#define LV_USE_G2D_DRAW_THREAD 0
#define LV_USE_GENERIC_MIPI 0
#define LV_USE_GRADIENT 1
#define LV_USE_GRID 0
#define LV_USE_IMAGE 1
#define LV_USE_KEYBOARD 0
#define LV_USE_LABEL 1
#define LV_USE_LED 0
#define LV_USE_LIST 0
#define LV_USE_LOG 0
#define LV_USE_LV_TILEVIEW_TILE_T 0
#define LV_USE_LZ4 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_NUTTX_MOUSE_MOVE_STEP 0
#define LV_USE_OBJ 1
#define LV_USE_OBJ_ID_BUILTIN 0
#define LV_USE_OBJ_PROPERTY_NAME 0
#define LV_USE_OBSERVER 0
#define LV_USE_PAGE 0
#define LV_USE_PPA 0
#define LV_USE_PROFILER_BUILTIN 0
#define LV_USE_PXP_DRAW_THREAD 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 1
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STYLE 1
#define LV_USE_SWITCH 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TEXTAREA 0
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_MONO 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THORVG 0
#define LV_USE_TILEVIEW 0
#define LV_USE_TOUCHSCREEN 1
#define LV_USE_USER_DATA 1
#define LV_USE_WIN 0
#define LV_VG_LITE_THORVG_16PIXELS_ALIGN 0
#define LV_VG_LITE_USE_BOX_SHADOW 0
#define LV_WAYLAND_USE_EGL 0
#define LV_WAYLAND_USE_G2D 0
#define LV_WAYLAND_USE_SHM 0
#define LV_WIDGETS_HAS_DEFAULT_VALUE 0
// from the panel firmware's LV_* build flags (packages/ui.yaml)
#undef LV_USE_LINE
#define LV_USE_LINE 1
#undef LV_USE_QRCODE
#define LV_USE_QRCODE 1
#undef LV_USE_SNAPSHOT
#define LV_USE_SNAPSHOT 1
#undef LV_DRAW_SW_CIRCLE_CACHE_SIZE
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 32
// labels never break after a hyphen ("Wi-Fi" stays whole): LVGL's Kconfig default minus '-'. KEEP IDENTICAL to the
// panel firmware's sdkconfig CONFIG_LV_TXT_BREAK_CHARS (its board YAML)
#define LV_TXT_BREAK_CHARS " ,.;:_)}"
#define LV_CONF_H
#define LV_USE_OS LV_OS_NONE
// theme fonts the panel downloads (core/font_store.cpp) are drawn by TinyTTF; the panel firmware needs the same flag
#undef LV_USE_TINY_TTF
#define LV_USE_TINY_TTF 1
#undef LV_TINY_TTF_FILE_SUPPORT
#define LV_TINY_TTF_FILE_SUPPORT 0
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();
