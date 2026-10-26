# ESP Web Tools 10.4.0, Facet patch 1

Vendored copy of `esp-web-tools@10.4.0` (npm, `dist/web/`, Apache-2.0, see `LICENSE`; upstream
https://github.com/esphome/esp-web-tools, bundles esptool-js 0.6). Used only by the `/install-test/` page.

Changes (everything else is byte-for-byte upstream):

- `install-dialog-im156JnI.js`: the inlined `flash()` step (upstream `src/flash.ts`) is replaced by a call to
  `facetFlash()` from `facet-flash.js`, passing the bundle's own `Transport`, `ESPLoader` and hard-reset helper. One
  `import` line added at the top.
- `facet-flash.js` (new): same states and messages as upstream `flash()`, plus
  1. flashes at **460800 baud** (upstream: fixed 115200); if connecting / changing speed or writing fails, it resets
     the board and starts once more at **115200**. `window.FACET_FLASH_BAUD` sets the first speed.
  2. **Erase device** erases only the data partitions the image does not rewrite (NVS, plus otadata / phy_init /
     coredump / filesystems when they lie outside the image), read from the partition table at 0x8000 inside the image.
     The written range is erased by the write itself. No table found = full chip erase, as upstream.
  3. checks every written part against the flasher stub's MD5 (upstream skips it). A mismatch counts as a write
     failure, so it is retried at 115200.
  Compression stays on. It fires a `facet-flash-report` window event with the speed, fallback, erase plan and time.

Test: `node site/scripts/test-install-flash.mjs` (fallback logic with a fake loader, erase plans for both boards).
