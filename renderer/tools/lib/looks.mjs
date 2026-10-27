// The one list of LOOK sizes (CONTRACT.md C18, C19): which size tokens a theme may set and their ranges. Shared by
// tools/gen_fx.mjs (fx::gen::SIZES: fx::tune clamps to the range) and tools/gen_theme.mjs (the theme parser refuses a
// value outside it). radius:* 0 .. max(2 x token, 16); stroke:* 1 .. max(2 x token, 2); stroke:pixel (the cell of a
// snapped theme, C29) 1 .. 4; radius:round (999) is a sentinel, not a length: fixed. Every other size = layout (fixed).
export const isLook = (n) => n !== 'radius:round' && /^(radius|stroke):/.test(n);
// the range at one density, from the token's px there: [lo, hi]
export function lookRange(n, v) {
  if (!isLook(n)) return [v, v];
  if (n === 'stroke:pixel') return [1, 4];
  return n.startsWith('radius:') ? [0, Math.max(2 * v, 16)] : [1, Math.max(2 * v, 2)];
}
// the same range as a C++ expression over the generated constant `e` (gen_fx.mjs: one table for both densities' builds)
export function lookRangeExpr(n, e) {
  if (!isLook(n)) return `false, ${e}, ${e}`;
  if (n === 'stroke:pixel') return 'true, 1, 4';
  return n.startsWith('radius:') ? `true, 0, (2 * ${e} > 16 ? 2 * ${e} : 16)` : `true, 1, (2 * ${e} > 2 ? 2 * ${e} : 2)`;
}
