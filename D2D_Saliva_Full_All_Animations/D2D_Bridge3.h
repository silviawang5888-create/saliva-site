/*
 * =============================================================================
 *  D2D_Bridge3.h  —  Bridge 3 (Box 3 Electronics → Box 4 Assembly)                    v1, 27 Oct 2026
 *  32×8 bridge panel BRIDGE[2]
 * =============================================================================
 *  Follows : "D2D Four-Box LED Animation Coding Standard" (21 Oct 2026), §3, §4 (32×8 bridge), §6, §7
 *  Design  : Chan Yeuk Hei — code generated with Claude (Anthropic)
 *  STATUS  : Generated but not physically tested.
 *
 *  STANDARD FUNCTION (§6):
 *    void renderBridge3(double progress, double seconds, double level);   // writes BRIDGE[2] only
 *
 *  Must already be declared BEFORE this file is included:
 *    CRGB (FastLED.h), BRIDGE[3], and the shared put(panel, x, y, color, brightness).
 *  Only put() is called, every LED at most once per call. No show(), delay(), addLeds(), GPIO,
 *  brightness or power settings. No state is kept: every frame is a pure function of
 *  (progress, seconds, level). Designed for 5 s (same as Bridges 1 and 2); the story follows
 *  `progress`, so 6 s also works. progress >= 1 = retained scene (dim, the belt still steps).
 *  Nothing moves faster than ~0.9 LED per frame at 10 fps.
 *
 *  ONE SCREEN: bridge rows 0–7 = backlight rows 4–11 (BRIDGE_TOP_ROW = 4).
 *    In : Box 3's purple packet leaves through USB-C at backlight (15, 11)   → bridge row 7
 *    Out: Box 4's purple trail enters its backlight at the bottom-left corner (below row 11),
 *         so the bridge hands over on its lowest row, 7 (the closest it can get).
 *
 *  BRIDGE 3 — "assembly line" (Electronics → Assembly). The finished signal is carried along a
 *  stepping conveyor and parts are fitted onto it one by one:
 *      0.0  Box 3's purple packet enters at row 7 and becomes the HEAD (7 LED/s, leaves at ≈ 4.6 s)
 *      behind the head a conveyor belt appears on row 7: short dashes that move in mechanical STEPS
 *           (1.5 LED in 0.2 s, every 0.5 s), with a faint guide rail on row 4
 *      1.14 / 2.0 / 2.86 / 3.71  four PARTS (2-LED pink pieces = Box 4's colour) drop from the top and
 *           SNAP onto the belt exactly as the head passes x = 7 / 13 / 19 / 25: a short flash + sparks.
 *           Fitted parts then ride the belt step by step.
 *      4.6  the purple head leaves on row 7 → Box 4's purple trail starts at its bottom-left corner
 *      4.0–5.0  everything settles into the retained scene
 *      retained: belt and fitted parts at 30 %, still stepping
 *  Colours: Box 3 purple #6D33BD → Box 4 pink #FF005F (standard §5: a bridge may blend its neighbours).
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include <math.h>

namespace D2DBridge3 {

// ---- 1) installation ---------------------------------------------------------
static const float BRIDGE_TOP_ROW = 4.0f;
static const float IN_Y  = 11.0f - BRIDGE_TOP_ROW;   // Box 3 USB-C exit row (PATH_COIL ends at (15, 11))
static const float OUT_Y = 7.0f;                     // lowest bridge row, towards Box 4's bottom-left entry
static const float RAIL_Y = 4.0f;                    // faint guide rail

// ---- 2) colours ---------------------------------------------------------------
struct C3 { uint8_t r, g, b; };
static constexpr C3 PURPLE  = { 109,  51, 189 };    // Box 3 #6D33BD
static constexpr C3 LILAC   = { 170, 130, 240 };    // packet highlight (Box 3's HI is purple + white)
static constexpr C3 PINK    = { 255,   0,  95 };    // Box 4 #FF005F
static constexpr C3 PINK_HI = { 255, 120, 170 };    // snap flash (kept away from white)
static constexpr C3 MAGENTA = { 190,  30, 150 };    // half-way

// ---- 3) levels (0–1, before `level`) --------------------------------------------
static const float HEAD_MAX  = 240 / 255.0f;
static const float BELT_MAX  = 120 / 255.0f;
static const float RAIL_MAX  =  28 / 255.0f;
static const float PART_MAX  = 200 / 255.0f;
static const float SNAP_MAX  = 230 / 255.0f;
static const float SPARK_MAX = 170 / 255.0f;
static const float REST      = 0.30f;

// ---- 4) timeline (design seconds) ------------------------------------------------
static const float STORY_S = 5.0f;
static const float HEAD_X0 = -1.0f, HEAD_V = 7.0f;          // head x = X0 + V·t → leaves x = 31 at ≈ 4.57 s
static const float SETTLE_T = 4.0f;
static const int   PARTS = 4;
static const float PART_X[PARTS] = { 7.0f, 13.0f, 19.0f, 25.0f };   // snap where the head passes
static const float PART_FALL = 1.6f;                        // a part falls for 1.6 s before it snaps
static const float BELT_PERIOD = 3.0f;                      // a dash every 3 LED
static const float STEP_EVERY = 0.5f, STEP_LEN = 1.5f, STEP_DUR = 0.2f;   // mechanical steps

// ---- implementation ---------------------------------------------------------------
static float g_b[8][32][3];
static inline float clamp01(float v) { return v > 0.0f ? (v < 1.0f ? v : 1.0f) : 0.0f; }
static inline float smooth(float u) { u = clamp01(u); return u * u * (3.0f - 2.0f * u); }
static inline C3 mix(C3 a, C3 b, float t) {
  t = clamp01(t);
  C3 c = { (uint8_t)(a.r + (b.r - a.r) * t + 0.5f), (uint8_t)(a.g + (b.g - a.g) * t + 0.5f), (uint8_t)(a.b + (b.b - a.b) * t + 0.5f) };
  return c;
}
static inline float storyTime(double p) { return (p > 0.0) ? (p < 1.0 ? (float)p * STORY_S : STORY_S) : 0.0f; }
static inline float safeSeconds(double s) { return (s > 0.0 && s < 1.0e6) ? (float)s : 0.0f; }
static inline float hash01(uint32_t a, uint32_t b) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return (float)(h & 0xFFFFFFu) / 16777216.0f;
}
static void clear() { for (int y = 0; y < 8; y++) for (int x = 0; x < 32; x++) g_b[y][x][0] = g_b[y][x][1] = g_b[y][x][2] = 0.0f; }
static inline void add(int x, int y, C3 c, float v) {
  if (!(v > 0.0f) || x < 0 || x > 31 || y < 0 || y > 7) return;
  if (v > 1.0f) v = 1.0f;
  const float k = v / 255.0f;
  g_b[y][x][0] += c.r * k; g_b[y][x][1] += c.g * k; g_b[y][x][2] += c.b * k;
}
static void splat(float fx, float fy, C3 c, float v) {
  if (!(v > 0.0f)) return;
  const int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
  const float ax = fx - x0, ay = fy - y0;
  add(x0, y0, c, v * (1 - ax) * (1 - ay)); add(x0 + 1, y0, c, v * ax * (1 - ay));
  add(x0, y0 + 1, c, v * (1 - ax) * ay);   add(x0 + 1, y0 + 1, c, v * ax * ay);
}
static void flush(uint8_t panel, double level) {   // every LED at most once; overlaps keep their hue
  const float lv = (level > 0.0) ? (level < 1.0 ? (float)level : 1.0f) : 0.0f;
  if (lv <= 0.0f) return;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 32; x++) {
      const float r = g_b[y][x][0], g = g_b[y][x][1], b = g_b[y][x][2];
      const float m = r > g ? (r > b ? r : b) : (g > b ? g : b);
      if (!(m > 0.0f)) continue;
      const float bri = (m < 1.0f ? m : 1.0f) * lv;
      if (bri < 0.002f) continue;
      put(panel, x, y, CRGB((uint8_t)(r / m * 255 + 0.5f), (uint8_t)(g / m * 255 + 0.5f), (uint8_t)(b / m * 255 + 0.5f)), bri);
    }
}

static inline float headX(float t) { return HEAD_X0 + HEAD_V * t; }
static inline float snapT(int i) { return (PART_X[i] - HEAD_X0) / HEAD_V; }
// belt offset (LED): mechanical steps of STEP_LEN every STEP_EVERY, eased over STEP_DUR
static inline float beltOffset(float sec) {
  const float n = floorf(sec / STEP_EVERY), f = sec - n * STEP_EVERY;
  return STEP_LEN * (n + smooth(f / STEP_DUR));
}
static inline C3 alongCol(float x) { return mix(PURPLE, PINK, clamp01(x / 31.0f) * 0.55f); }   // purple → magenta along the belt

static void bridge3(float t, float sec) {
  const float head = headX(t);
  const float settle = smooth((t - SETTLE_T) / (STORY_S - SETTLE_T));
  const float rest = 1.0f - (1.0f - REST) * settle;
  const float belt = beltOffset(sec);

  // guide rail (appears behind the head)
  for (int x = 0; x < 32; x++) {
    const float on = smooth((head - x) / 3.0f);
    add(x, (int)RAIL_Y, PURPLE, RAIL_MAX * on * rest);
  }
  // conveyor belt: dashes on row 7 behind the head, stepping right
  for (int x = 0; x < 32; x++) {
    const float on = smooth((head - 1.0f - x) / 2.5f);
    if (on <= 0.0f) continue;
    float ph = fmodf(x - belt, BELT_PERIOD); if (ph < 0) ph += BELT_PERIOD;
    const float dash = ph < 1.0f ? 1.0f - 0.5f * ph : (ph < 2.0f ? 0.15f : 0.0f);
    add(x, (int)IN_Y, alongCol((float)x), BELT_MAX * on * dash * rest);
    add(x, (int)IN_Y - 1, alongCol((float)x), 0.25f * BELT_MAX * on * dash * rest);   // belt edge
  }
  // parts: fall, snap as the head passes, then ride the belt
  for (int i = 0; i < PARTS; i++) {
    const float ts = snapT(i), t0 = ts - PART_FALL;
    if (t < t0) continue;
    const C3 pc = mix(MAGENTA, PINK, (float)i / (PARTS - 1));
    float px, py;
    if (t < ts) {                                   // falling: accelerating from above the panel
      const float u = (t - t0) / PART_FALL;
      px = PART_X[i]; py = -1.5f + (IN_Y - 1.0f + 1.5f) * u * u;
    } else {                                        // fitted: rides the belt
      px = PART_X[i] + (belt - beltOffset(sec - (t - ts))); py = IN_Y - 1.0f;
    }
    const float v = PART_MAX * (t < ts ? 0.75f : 1.0f) * rest;
    splat(px - 0.5f, py, pc, v); splat(px + 0.5f, py, pc, v);
    // snap: flash + sparks for 0.6 s
    const float a = t - ts;
    if (a >= 0.0f && a < 0.6f) {
      const float fl = SNAP_MAX * expf(-a / 0.12f);
      for (int dx = -2; dx <= 2; dx++) for (int dy = -2; dy <= 1; dy++) {
        const float d = sqrtf((float)(dx * dx + dy * dy));
        add((int)PART_X[i] + dx, (int)(IN_Y - 1) + dy, PINK_HI, fl * expf(-d * d / 1.2f));
      }
      for (int k = 0; k < 4; k++) {                 // sparks fly up and out
        const float ang = 3.14159f * (0.15f + 0.7f * hash01(i, k)), sp = 5.0f + 3.0f * hash01(k, i + 7);
        splat(PART_X[i] + cosf(ang) * sp * a * (k & 1 ? 1 : -1), IN_Y - 1.0f - sinf(ang) * sp * a, PINK_HI,
              SPARK_MAX * (1.0f - a / 0.6f));
      }
    }
  }
  // the head: Box 3's purple packet, with a short tail
  if (head > -1.5f && head < 33.0f) {
    const float fade = 1.0f - smooth((head - 31.0f) / 1.5f);
    splat(head, IN_Y, LILAC, HEAD_MAX * fade);
    for (int j = 1; j <= 5; j++) splat(head - 0.5f * j, IN_Y, PURPLE, HEAD_MAX * 0.7f * (1.0f - j / 6.0f) * fade);
    for (int y = 4; y < 7; y++) add((int)floorf(head), y, PURPLE, 0.12f * HEAD_MAX * (y - 3) / 3.0f * fade);
  }
}

}  // namespace D2DBridge3

void renderBridge3(double progress, double seconds, double level) {
  using namespace D2DBridge3;
  clear();
  bridge3(storyTime(progress), safeSeconds(seconds));
  flush(BRIDGE[2], level);
}
