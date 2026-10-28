/*
 * =============================================================================
 *  Box1_Pattern.h  —  D2D Box 1 "Pattern" · Pattern & Deposition Lab      (v6, 23 Oct 2026)
 *  Animation module for the Box 1 under-light (BOTTOM[0]) and backlight (REAR[0])
 * =============================================================================
 *  Follows : "D2D Four-Box LED Animation Coding Standard" (21 Oct 2026)
 *  Design  : Chan Yeuk Hei — code generated with Claude (Anthropic)
 *  STATUS  : Generated but not physically tested.
 *
 *  v5 (23 Oct): rewritten for the NEW acrylic boards (Deposition v2): bigger disc (centre 8 mm higher),
 *      L5 = hatched H, L6 = complete H + wafer grid; boards in EVERY OTHER slot → under-light rows
 *      15, 13, 11, 9, 7, 5; L1 switches off after the alignment; white light for the polariser film;
 *      teal hand-over to the Biosensor Lab.
 *  v6 (23 Oct): richer effects —
 *      · alignment "lock": the two alignment marks, the magnet and the knob mark flash, two light beams
 *        run from the marks into the disc centre, a burst + two ripples + 12 sparks fly out, the angle
 *        scale fills up like a meter and blinks twice; L1/L2 flash and a highlight runs out along them
 *      · every layer lights with a bright running "head"; L1 dissolves from both ends to the centre
 *      · exposure: lamp halo, twinkles inside the UV light, the 17 H shapes collect "dose" (latent image)
 *        and flash when the exposure is complete; the L4 row shimmers outwards
 *      · deposition: metal atoms fly from the source at the bottom into the 17 H shapes, which fill with
 *        warm light; white light rises from the bottom for the polariser film; L5 sparkles
 *      · result: reveal ring, the H shapes light from the centre, a glint sweeps across them, breathing
 *      · hand-over: teal wave over the H shapes, teal particles stream to the exit, teal light along the
 *        rows, three teal dots leave the right edge
 *      · waiting: highlights drift along L1 and L2 towards each other; the alignment marks pulse
 *
 *  STANDARD FUNCTIONS (§6) — merge these into the complete 11-panel program:
 *    void renderBox1Bottom(double progress, double seconds, double level);  // writes BOTTOM[0] only
 *    void renderBox1Rear  (double progress, double seconds, double level);  // writes REAR[0]  only
 *
 *  ADDED BY CHAN — optional; only used if the sequencer gets a "waiting for a visitor" state:
 *    void renderBox1IdleBottom(double progress, double seconds, double level);  // BOTTOM[0]; progress unused
 *    void renderBox1IdleRear  (double progress, double seconds, double level);  // REAR[0];   progress unused
 *
 *  Must already be declared BEFORE this file is included:
 *    CRGB (FastLED.h), BOTTOM[4], REAR[4], and the shared put(panel, x, y, color, brightness).
 *  This file only calls put(). It never calls FastLED.show(), delay(), FastLED.addLeds() or dot(),
 *  never declares a CRGB array, and never touches GPIO, brightness, colour order or power limits.
 *
 *  Drawing : effects are mixed in a private 16x16 float buffer, then every LED of the panel is
 *            written at most ONCE per call with put(). Where effects overlap, the brightest channel is
 *            limited to 1 while the hue is kept (so overlapping highlights never turn white).
 *  Timing  : story events follow `progress` (designed for 28 s). Breathing / twinkles follow `seconds`
 *            on the backlight. The under-light uses only `progress` (its finished scene is static).
 *            `level` (0–1) scales everything. progress >= 1 = finished scene (retained).
 *            Every moving light moves at most about one LED per frame at 10 fps; blinks last ≥ 0.2 s.
 *  Coordinates (§4): under-light x 0–15 left→right, y = 15 front … y = 0 rear;
 *                    backlight  x 0–15 left→right, y = 0 top … y = 15 bottom.
 * -----------------------------------------------------------------------------
 *  STORY (design seconds; progress = t / 28)
 *    0.0  LOCK: alignment marks + magnet + knob mark flash; beams run from the marks into the centre;
 *         angle-scale meter fills and blinks twice; L1 + L2 flash, a highlight runs out along them
 *    0.35 burst in the disc centre → ripple, 12 sparks; 0.8 echo ripple
 *    0.6  L3 (wafer grid) lights from the centre outwards (bright head)
 *    1.5  L1 (mask) dissolves from both ends to the centre and stays off
 *    3.0  L4 (UV) lights; 3.4 UV lamp strikes (+ halo), layers turn purple; 4.2 purple sweeps down;
 *         6.2 bands flow down, twinkles, the 17 H shapes collect "dose"; 9.6 they flash (exposed)
 *   10.0  L5 (deposition) lights; UV fades; colours back to orange; 10.2 atoms fly from the source
 *         into the H shapes, which fill with warm light; L5 sparkles
 *   12.6  (BOX1_POLAR_FILM 1) white light rises from the bottom over the wafer, out at 15–16 s
 *         (BOX1_POLAR_FILM 0) a warm glint runs over the filled H shapes instead
 *   16.0  L6 (result) lights; reveal ring; the H shapes light from the centre; 17.2 glint sweep
 *   18.0  slow orange breathing (keeps going in the finished scene)
 *   23.5  teal wave over the H shapes, teal particles stream to the exit
 *   25.2  teal light runs along the lit rows; 25.4 three teal dots leave the right edge …
 *   28.0  … the last one has just left → Bridge 1 starts here (progress = 1)
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include <math.h>

#ifndef BOX1_UV_ACCENT
#define BOX1_UV_ACCENT 1     // 1 = UV exposure in purple/lilac (Chan's design, needs colour approval, standard §5)
                             // 0 = orange-only exposure
#endif
#ifndef BOX1_POLAR_FILM
#define BOX1_POLAR_FILM 1    // 1 = white backlight during the deposition step (12.6–16 s), for the polariser film +
                             //     clear-tape colours. White is needed: coloured light only gets brighter/darker.
                             // 0 = no film fitted: a warm glint runs over the filled H shapes instead
#endif
#ifndef BOX1_HANDOFF_TEAL
#define BOX1_HANDOFF_TEAL 1  // 1 = hand-over light in teal #56D1C2 = Box 2 (Biosensor) colour (needs colour approval)
                             // 0 = hand-over light in Box 1 orange
#endif

namespace Box1Pattern {

// ---------------------------------------------------------------------------
// 1) Installation — logical positions (change these, never the drawing code)
// ---------------------------------------------------------------------------
// Under-light row under each acrylic layer. L1 is the FRONT layer (the visitor turns its disc).
// The boards stand in every other slot (20 mm apart = 2 LED rows): rows 15, 13, 11, 9, 7, 5.   ⚠ confirm on the real box
static const int8_t LAYER_Y[6]  = { 15, 13, 11, 9, 7, 5 };
static const int8_t LAYER_Y2[6] = { -1, -1, -1, -1, -1, -1 };   // optional second row under a layer (-1 = none)

// Backlight positions in LED units, from the Deposition v2 drawings (LED x = (mm_x − 5) / 10,
// LED y = (155 − mm_from_bottom) / 10, i.e. the backlight's bottom edge level with the acrylic).   ⚠ confirm on the real box
static const float DISC_CX = 7.5f, DISC_CY = 6.4f;   // L1 disc centre (80, 91 mm)
static const float DISC_R_LED = 6.9f;                // L1 disc radius (Ø138 mm)
static const float WAFER_R = 7.0f;                   // L3 wafer edge
static const float UV_R    = 6.6f;                   // L4 UV area
static const float FILM_R  = 6.5f;                   // clear tape area on L5 (white light only needed here)
static const float TGT_X[2] = { 2.82f, 12.18f }, TGT_Y = 6.4f;   // the two moiré alignment marks (L1 + L2)
static const float MAG_X = 7.39f, MAG_Y = 0.0f;      // magnet at 12 o'clock (over the Hall sensor)
static const float KNOB_X = 14.3f, KNOB_Y = 10.6f;   // frame mark where the knob stops when aligned
static const float SRC_X = 7.5f, SRC_Y = 15.4f;      // deposition source (below the wafer, bottom centre)
static const int   LAMP_Y = 0, LAMP_X0 = 5, LAMP_X1 = 10;   // UV lamp along the top row
static const float EXIT_Y = 7.2f;                    // row where the hand-over dots leave the right edge (match Bridge 1)
static const int   N_H = 17;                         // the 17 H shapes (L5 / L6), top row first, left → right
static const float HX[N_H] = { 5.17f, 7.50f, 9.84f,
                               2.82f, 5.15f, 7.50f, 9.83f, 12.18f,
                               5.16f, 9.85f,
                               2.83f, 5.16f, 7.49f, 9.83f,
                               5.16f, 7.50f, 9.84f };
static const float HY[N_H] = { 1.73f, 1.72f, 1.72f,
                               4.06f, 4.06f, 4.06f, 4.07f, 4.06f,
                               6.39f, 6.40f,
                               8.74f, 8.74f, 8.74f, 8.74f,
                               11.08f, 11.08f, 11.08f };
static const int   GUIDE_LEN = 10;                   // L1 angle scale, 6 o'clock → 4 o'clock (guide light + meter)
static const uint8_t GUIDE_X[GUIDE_LEN] = {  8,  9, 10, 11, 12, 12, 13, 13, 14, 14 };
static const uint8_t GUIDE_Y[GUIDE_LEN] = { 14, 14, 14, 13, 13, 12, 12, 11, 11, 10 };
static const int   N_EXIT = 3;                       // hand-over dots start at the right-hand H shapes
static const float EXIT_X0[N_EXIT] = { 9.85f, 12.18f, 9.83f };
static const float EXIT_Y0[N_EXIT] = { 6.40f,  4.06f, 8.74f };

// ---------------------------------------------------------------------------
// 2) Colours (standard §5: Box 1 = Pattern #FF5500) and levels (0–1)
// ---------------------------------------------------------------------------
struct C3 { uint8_t r, g, b; };
static constexpr C3 ORANGE       = { 255,  85,   0 };   // Box 1 colour #FF5500 — main colour
static constexpr C3 ORANGE_DEEP  = { 255,  45,   0 };
static constexpr C3 ORANGE_LIGHT = { 255, 140,  50 };
static constexpr C3 GOLD         = { 255, 165,  60 };   // deposited metal
static constexpr C3 PEACH        = { 255, 185, 130 };   // highlights (flashes, heads, glints)
#if BOX1_UV_ACCENT
static constexpr C3 UV_MID = { 255,   0,  95 };          // pink #FF005F — passing colour orange → purple
static constexpr C3 UV     = { 109,  51, 189 };          // purple #6D33BD — "UV light"
static constexpr C3 UV_HI  = { 190, 150, 255 };          // lilac — UV lamp, brightest bands, exposed H shapes
#else
static constexpr C3 UV_MID = { 255,  65,   0 };
static constexpr C3 UV     = { 255,  45,   0 };
static constexpr C3 UV_HI  = { 255, 185, 130 };
#endif
#if BOX1_HANDOFF_TEAL
static constexpr C3 HAND    = {  86, 209, 194 };         // teal #56D1C2 = Box 2 (Biosensor) colour
static constexpr C3 HAND_HI = { 175, 245, 235 };
#else
static constexpr C3 HAND    = ORANGE_LIGHT;
static constexpr C3 HAND_HI = PEACH;
#endif
static constexpr C3 FILM_WHITE = { 255, 238, 215 };      // slightly warm white (all three colours on)

static const float BASE_IDLE     = 170 / 255.0f;   // L1, L2 while waiting for a visitor
static const float BASE_ON       = 1.00f;          // the layer that is "performing"
static const float BASE_GHOST    =  40 / 255.0f;   // layers that have stepped back (0 = switch them off)
static const float BASE_L1_AFTER =   0 / 255.0f;   // L1 after the mask dissolves (0 = off; the disc may hang crooked)
static const float BASE_L3_END   = 115 / 255.0f;   // L3 in the finished scene
static const float HEAD_MAX      = 200 / 255.0f;   // bright head running in front of a layer that lights up
static const float BACK_MAX      = 110 / 255.0f;   // cap for full-panel backlight washes
static const float WHITE_MAX     =  90 / 255.0f;   // white phase (≈ 2 A on this panel at global brightness 179)
static const float LOCK_MAX      = 235 / 255.0f;   // alignment flash (marks, magnet, knob)
static const float BEAM_MAX      = 200 / 255.0f;
static const float RIPPLE_MAX    = 200 / 255.0f;
static const float SPARK_MAX     = 220 / 255.0f;
static const float METER_MAX     = 200 / 255.0f;
static const float LAMP_MAX      = 230 / 255.0f;
static const float TWINKLE_MAX   = 150 / 255.0f;
static const float LATENT_MAX    = 140 / 255.0f;   // "dose" collected by the H shapes during the exposure
static const float ATOM_MAX      = 220 / 255.0f;
static const float HFILL_MAX     = 130 / 255.0f;   // H shapes filled with metal
static const float HGLOW_MAX     = 150 / 255.0f;   // H shapes in the result
static const float GLINT_MAX     = 170 / 255.0f;
static const float COMET_MAX     = 210 / 255.0f;   // waiting guide light
static const float TARGET_GLOW   =  60 / 255.0f;   // waiting: alignment marks pulse up to this (0 = off)
static const float IDLE_DRIFT    =  90 / 255.0f;   // waiting: highlight drifting along L1 / L2 (0 = off)
static const float EXIT_MAX      = 220 / 255.0f;
static const float HAND_ROW_MAX  = 230 / 255.0f;   // teal light along the under-light rows
static const float FILM_BACK_GAIN = 1.0f;          // with the polariser films fitted, everything behind the disc looks
                                                   // only about 1/4 as bright. After a real test this can be raised
                                                   // (e.g. 1.5–2.0) — it boosts the backlight inside the disc only,
                                                   // and costs more current. 1.0 = no change.

// ---------------------------------------------------------------------------
// 3) Timeline — design seconds; progress 0..1 is mapped onto STORY_S.
// ---------------------------------------------------------------------------
static const float STORY_S    = 28.0f;
// alignment lock
static const float LOCK_S     = 0.6f;    // flash at the marks / magnet / knob mark
static const float BEAM_S     = 0.35f;   // beams from the two marks reach the centre
static const float T_BURST    = 0.35f;   // centre burst, ripple and sparks start
static const float RIPPLE_S   = 1.8f, T_ECHO = 0.8f;
static const int   N_SPARK    = 12;
static const float SPARK_SPEED = 5.5f, SPARK_LIFE = 1.2f;   // LED/s, s
static const float METER_FILL_S = 0.5f;  // angle-scale meter fills, blinks 0.6–1.4 s, fades to 2.2 s
static const float RUN_S      = 0.8f;    // highlight runs out along L1 / L2
// layers
static const float T_L3       = 0.6f;
static const float T_MASK_OFF = 1.5f,  MASK_OFF_S = 1.0f;
static const float T_L4       = 3.0f;
static const float T_UV       = 3.4f;    // lit layers start shifting to the UV colour
static const float T_LAMP     = 3.4f;
static const float T_CURTAIN  = 4.2f,  CURTAIN_S = 2.0f;
static const float T_RAYS     = 6.2f;
static const float LATENT_S   = 3.0f,  T_EXPOSED = 9.6f;   // H shapes collect dose, then flash
static const float T_L5       = 10.0f;   // deposition
static const float T_BACK     = 10.2f;   // colours return purple → orange
static const float T_ATOMS    = 10.2f, ATOMS_S = 2.2f, ATOM_FLIGHT_S = 1.7f;   // launch window, flight time
static const float T_FILM     = 12.6f, FILM_IN_S = 1.6f, T_FILM_OUT = 15.0f, FILM_OUT_S = 1.0f;
static const float T_L6       = 16.0f;   // result
static const float T_HGLOW    = 16.3f, HGLOW_STEP_S = 0.07f, HGLOW_IN_S = 0.6f;
static const float T_GLINT    = 17.2f, GLINT_S = 1.4f;
static const float T_BREATH   = 18.0f, BREATH_IN_S = 1.5f, BREATH_PERIOD_S = 4.0f;
static const float T_WAVE     = 23.5f, WAVE_S = 2.0f, WAVE_DECAY_S = 1.2f;   // teal wave over the H shapes
static const float PARTICLE_S = 1.5f;                                        // teal particles: H shape → exit
static const float T_ROWS     = 25.2f, ROWS_S = 2.6f;                        // teal along the under-light rows
static const float T_EXIT     = 25.4f, EXIT_GAP_S = 0.5f, EXIT_S = 1.6f;     // last dot has left at 28.0 s
static const float WIPE_S     = 1.0f;
static const float SHIFT_S    = 1.8f;
static const float STAGGER_S  = 0.3f;
static const float FOCUS_S    = 1.2f;
static const float UV_OUT_S   = 0.8f;
static const float RAY_SPEED  = 5.5f, RAY_PERIOD = 4.0f;
static const float COMET_STEP_S = 0.20f, COMET_ATTACK_S = 0.10f, COMET_TAU_S = 0.60f, COMET_GAP_S = 1.6f;

// ---------------------------------------------------------------------------
// Implementation — normally no need to edit below this line
// ---------------------------------------------------------------------------
static float g_acc[16][16][3];   // private mixing buffer for ONE panel (never shown directly)
static const uint8_t COL_OFF[16] = { 0, 1, 2, 1, 0, 2, 3, 2, 1, 0, 1, 3, 2, 1, 0, 1 };

static inline float clamp01(float v) { return v > 0.0f ? (v < 1.0f ? v : 1.0f) : 0.0f; }   // also turns NaN into 0
static inline float ramp(float t, float t0, float dur) { return clamp01((t - t0) / dur); }
static inline float smooth(float u) { return u * u * (3.0f - 2.0f * u); }
static inline float absf(float v) { return v < 0.0f ? -v : v; }
static inline C3 mix(C3 a, C3 b, float t) {
  t = clamp01(t);
  C3 c = { (uint8_t)(a.r + (b.r - a.r) * t + 0.5f),
           (uint8_t)(a.g + (b.g - a.g) * t + 0.5f),
           (uint8_t)(a.b + (b.b - a.b) * t + 0.5f) };
  return c;
}
static inline C3 uvColor(float u) { return u < 0.5f ? mix(ORANGE, UV_MID, u * 2.0f) : mix(UV_MID, UV, (u - 0.5f) * 2.0f); }
static inline float storyTime(double progress) { return clamp01((float)progress) * STORY_S; }
static inline float safeSeconds(double seconds) { return (seconds > 0.0 && seconds < 1.0e6) ? (float)seconds : 0.0f; }
// fixed pseudo-random number 0..1 from two integers (no random(), same result every time)
static inline float hash01(uint32_t a, uint32_t b) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return (float)(h & 0xFFFFFFu) / 16777216.0f;
}

static void clearAcc() {
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) g_acc[y][x][0] = g_acc[y][x][1] = g_acc[y][x][2] = 0.0f;
}
static inline void add(int x, int y, C3 c, float v) {
  if (!(v > 0.0f) || x < 0 || x > 15 || y < 0 || y > 15) return;   // §7 rule 8: never outside the panel
  if (v > 1.0f) v = 1.0f;
  const float k = v / 255.0f;
  g_acc[y][x][0] += c.r * k;
  g_acc[y][x][1] += c.g * k;
  g_acc[y][x][2] += c.b * k;
}
static void splat(float fx, float fy, C3 c, float v) {   // light centred on a point between LEDs
  if (!(v > 0.0f)) return;
  const int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
  const float ax = fx - x0, ay = fy - y0;
  add(x0,     y0,     c, v * (1.0f - ax) * (1.0f - ay));
  add(x0 + 1, y0,     c, v * ax * (1.0f - ay));
  add(x0,     y0 + 1, c, v * (1.0f - ax) * ay);
  add(x0 + 1, y0 + 1, c, v * ax * ay);
}
static void glow(float cx, float cy, float r, C3 c, float v) {   // soft round spot of radius r
  if (!(v > 0.0f) || !(r > 0.0f)) return;
  const int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r), y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const float dx = x - cx, dy = y - cy, w = clamp01(1.0f - sqrtf(dx * dx + dy * dy) / r);
      if (w > 0.0f) add(x, y, c, v * w * w);
    }
}
static void ring(float cx, float cy, float r, float width, C3 c, float v) {   // circle outline
  if (!(v > 0.0f)) return;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float dx = x - cx, dy = y - cy;
      const float w = 1.0f - absf(sqrtf(dx * dx + dy * dy) - r) / width;
      if (w > 0.0f) add(x, y, c, v * w);
    }
}
// Optional boost of the backlight behind the disc (polariser films absorb most of it)
static void filmGain() {
#if BOX1_POLAR_FILM
  if (FILM_BACK_GAIN == 1.0f) return;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float dx = x - DISC_CX, dy = y - DISC_CY;
      const float k = 1.0f + (FILM_BACK_GAIN - 1.0f) * clamp01(DISC_R_LED + 0.5f - sqrtf(dx * dx + dy * dy));
      for (int c = 0; c < 3; c++) g_acc[y][x][c] *= k;
    }
#endif
}
// Send the buffer to ONE panel: every LED at most once. Overlaps keep their hue (no drift to white).
static void flush(uint8_t panel, double level) {
  const float lv = clamp01((float)level);
  if (lv <= 0.0f) return;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float r = g_acc[y][x][0], g = g_acc[y][x][1], b = g_acc[y][x][2];
      const float m = r > g ? (r > b ? r : b) : (g > b ? g : b);
      if (!(m > 0.0f)) continue;
      const float bri = (m < 1.0f ? m : 1.0f) * lv;
      if (bri < 0.002f) continue;
      CRGB col((uint8_t)(r / m * 255.0f + 0.5f), (uint8_t)(g / m * 255.0f + 0.5f), (uint8_t)(b / m * 255.0f + 0.5f));
      put(panel, x, y, col, bri);
    }
}
static inline float discDist(float x, float y) {
  const float dx = x - DISC_CX, dy = y - DISC_CY;
  return sqrtf(dx * dx + dy * dy);
}
// teal light running left → right along the rows: weight 0..1 at column x
static inline float rowWave(float t, float x) {
  if (t < T_ROWS || t > T_ROWS + ROWS_S) return 0.0f;
  const float front = -2.0f + 20.0f * ramp(t, T_ROWS, ROWS_S);
  const float d = front - x;
  if (d < -1.2f) return 0.0f;
  return d < 0.0f ? 1.0f + d / 1.2f : expf(-d / 2.2f);
}

// =============================================================================
//  UNDER-LIGHT
// =============================================================================
// One layer row. wipe 0..1 = lit from the centre outwards; head = bright running front (0..1);
// offMask = optional per-column dissolve (L1); shimmer = travelling ripple amplitude (0..1)
static void drawRow(int k, C3 c, float level, float wipe, float head, float t, float shimmer, bool dissolve) {
  for (int x = 0; x < 16; x++) {
    const float dc = absf(x - 7.5f);
    float v = level * clamp01((wipe * 9.5f - dc) / 1.5f);
    if (dissolve) {                                   // L1: switch off from both ends towards the centre
      const float off = smooth(ramp(t, T_MASK_OFF + (1.0f - dc / 7.5f) * 0.6f, MASK_OFF_S - 0.6f));
      v = v * (1.0f - off) + BASE_L1_AFTER * off * (level > 0.0f ? 1.0f : 0.0f);
    }
    if (shimmer > 0.0f) v *= 1.0f + 0.22f * shimmer * sinf(6.2831853f * (dc / 5.0f - t * 0.8f));
    const float w = (level > 0.0f) ? rowWave(t, (float)x) : 0.0f;
    const C3 col = mix(c, HAND, w);
    if (w * HAND_ROW_MAX > v) v = v + (w * HAND_ROW_MAX - v) * w;
    float h = 0.0f;
    if (head > 0.0f && wipe > 0.0f && wipe < 1.0f) {  // the bright front of the wipe
      const float front = wipe * 9.5f - 0.75f;
      h = head * clamp01(1.0f - absf(dc - front) / 1.4f);
    }
    for (int r = 0; r < 2; r++) {
      const int8_t yy = r == 0 ? LAYER_Y[k] : LAYER_Y2[k];
      if (yy < 0) continue;
      if (v > 0.0f) add(x, yy, col, v);
      if (h > 0.0f) add(x, yy, PEACH, h * HEAD_MAX);
    }
  }
}
// a highlight that runs from the centre to both ends of a row (u 0..1), or along the row (pos in LEDs)
static void rowSpark(int k, float pos, C3 c, float v) {
  if (!(v > 0.0f)) return;
  const int x0 = (int)floorf(pos);
  const float a = pos - x0;
  add(x0, LAYER_Y[k], c, v * (1.0f - a));
  add(x0 + 1, LAYER_Y[k], c, v * a);
}

static void drawBaseStory(float t) {
  const float on12 = BASE_IDLE + (BASE_ON - BASE_IDLE) * smooth(ramp(t, 0.0f, 0.5f));
  const float expo = smooth(ramp(t, T_L4, FOCUS_S));
  const float depo = smooth(ramp(t, T_L5, FOCUS_S));
  const float res  = smooth(ramp(t, T_L6, FOCUS_S));
  for (int k = 0; k < 6; k++) {
    const float up   = (k >= 4) ? 0.0f : smooth(ramp(t, T_UV + k * STAGGER_S, SHIFT_S));
    const float down = smooth(ramp(t, T_BACK + (3 - k) * STAGGER_S, SHIFT_S));
    const C3 c = uvColor(up * (1.0f - down));
    float level, wipe = 1.0f, shimmer = 0.0f;
    if (k == 0)      { level = on12; }
    else if (k == 1) { level = on12 * (1.0f - expo) + BASE_GHOST * expo; }
    else if (k == 2) { wipe = ramp(t, T_L3, WIPE_S);
                       level = BASE_ON * (1.0f - expo) + BASE_GHOST * expo;
                       level = level * (1.0f - res) + BASE_L3_END * res; }
    else if (k == 3) { wipe = ramp(t, T_L4, WIPE_S);
                       level = BASE_ON * (1.0f - depo) + BASE_GHOST * depo;
                       shimmer = smooth(ramp(t, T_CURTAIN, 1.0f)) * (1.0f - ramp(t, T_L5 - 0.5f, 0.5f)); }
    else if (k == 4) { wipe = ramp(t, T_L5, WIPE_S);
                       level = BASE_ON * (1.0f - res) + BASE_GHOST * res; }
    else             { wipe = ramp(t, T_L6, WIPE_S); level = BASE_ON; }
    if (wipe > 0.0f) drawRow(k, c, level, smooth(wipe), 1.0f, t, shimmer, k == 0);
  }
  // LOCK: L1 + L2 flash, then a highlight runs out to both ends of each
  const float f = t < 0.2f ? 1.0f : expf(-(t - 0.2f) / 0.15f);
  if (f > 0.01f)
    for (int k = 0; k < 2; k++)
      for (int x = 0; x < 16; x++) add(x, LAYER_Y[k], PEACH, f * HEAD_MAX);
  const float u = ramp(t, 0.25f, RUN_S);
  if (u > 0.0f && u < 1.0f)
    for (int k = 0; k < 2; k++) {
      const float d = 0.5f + 8.0f * u, v = (1.0f - u * 0.5f) * HEAD_MAX;
      rowSpark(k, 7.5f - d, PEACH, v);
      rowSpark(k, 7.5f + d, PEACH, v);
    }
  // deposition: gold sparkles on L5 while the atoms land (3 at a time, 0.4 s each)
  if (t > T_L5 + 0.4f && t < T_ATOMS + ATOMS_S + ATOM_FLIGHT_S)
    for (int s = 0; s < 3; s++) {
      const float ph = (t - T_L5) / 0.4f + s * 0.33f;
      const uint32_t n = (uint32_t)ph;
      const float e = sinf(3.14159265f * (ph - n));
      const int x = 1 + (int)(hash01(n, 50 + s) * 14.0f);
      add(x, LAYER_Y[4], GOLD, e * 0.8f * HEAD_MAX);
    }
}

// =============================================================================
//  BACKLIGHT
// =============================================================================
// ── LOCK: the marks, the magnet and the knob mark flash; beams run from the two marks into the centre
static void drawLock(float t) {
  if (t < 0.0f || t > 1.2f) return;
  const float f = t < 0.2f ? 1.0f : expf(-(t - 0.2f) / 0.18f);
  for (int i = 0; i < 2; i++) glow(TGT_X[i], TGT_Y, 1.6f, PEACH, f * LOCK_MAX);
  glow(MAG_X, MAG_Y + 0.2f, 1.3f, PEACH, f * LOCK_MAX * 0.9f);
  glow(KNOB_X, KNOB_Y, 1.3f, PEACH, f * LOCK_MAX * 0.8f);
  // beams: grow from each mark to the centre in BEAM_S, then fade
  const float u = ramp(t, 0.0f, BEAM_S), fade = 1.0f - ramp(t, BEAM_S, 0.5f);
  if (fade <= 0.0f) return;
  for (int i = 0; i < 2; i++) {
    const float len = DISC_CX - TGT_X[i];
    for (int j = 0; j <= 18; j++) {
      const float s = j / 18.0f;
      if (s > u) break;
      const float bright = (0.35f + 0.65f * (u > 0.0f ? s / u : 0.0f)) * fade;
      splat(TGT_X[i] + len * s, TGT_Y, mix(ORANGE_LIGHT, PEACH, s), bright * BEAM_MAX * 0.35f);
    }
  }
}
// ── centre burst + ripple + echo ripple
static void drawRipples(float t) {
  const float b = t - T_BURST;
  if (b >= 0.0f && b < 0.8f) glow(DISC_CX, DISC_CY, 2.2f, PEACH, expf(-b / 0.25f) * LOCK_MAX);
  for (int k = 0; k < 2; k++) {
    const float a = t - (k == 0 ? T_BURST : T_ECHO);
    if (a < 0.0f || a > RIPPLE_S) continue;
    const float u = a / RIPPLE_S, r = 0.3f + u * 10.5f, fade = (1.0f - u) * (1.0f - u);
    ring(DISC_CX, DISC_CY, r, 1.4f, mix(PEACH, ORANGE, u), fade * RIPPLE_MAX * (k == 0 ? 1.0f : 0.55f));
  }
}
// ── 12 sparks fly out of the burst and fade at the wafer edge
static void drawSparks(float t) {
  for (int i = 0; i < N_SPARK; i++) {
    const float age = t - T_BURST - 0.05f * (i % 3);
    if (age < 0.0f || age > SPARK_LIFE) continue;
    const float ang = (i * 30.0f + 15.0f + 8.0f * hash01(i, 7)) * 0.0174533f;
    const float dx = cosf(ang), dy = sinf(ang);
    const float spd = SPARK_SPEED * (0.85f + 0.3f * hash01(i, 11));
    const float d = spd * age * (1.0f - 0.3f * age / SPARK_LIFE);
    const float v = powf(1.0f - age / SPARK_LIFE, 1.5f) * SPARK_MAX * clamp01((WAFER_R + 0.5f - d) / 1.5f);
    if (v <= 0.0f) continue;
    splat(DISC_CX + dx * d, DISC_CY + dy * d, mix(PEACH, ORANGE_LIGHT, age / SPARK_LIFE), v);
    if (d > 0.7f) splat(DISC_CX + dx * (d - 0.7f), DISC_CY + dy * (d - 0.7f), ORANGE, v * 0.45f);
    if (d > 1.4f) splat(DISC_CX + dx * (d - 1.4f), DISC_CY + dy * (d - 1.4f), ORANGE_DEEP, v * 0.2f);
  }
}
// ── the angle scale fills up like a meter (6 → 4 o'clock), blinks twice, fades
static void drawMeter(float t) {
  if (t < 0.0f || t > 2.2f) return;
  float lv;
  if      (t < 0.6f) lv = 0.85f;
  else if (t < 0.8f) lv = 1.0f;
  else if (t < 1.0f) lv = 0.2f;
  else if (t < 1.2f) lv = 1.0f;
  else if (t < 1.4f) lv = 0.2f;
  else               lv = 0.8f * (1.0f - ramp(t, 1.4f, 0.8f));
  for (int k = 0; k < GUIDE_LEN; k++) {
    const float on = ramp(t, k * METER_FILL_S / GUIDE_LEN, 0.08f);
    if (on <= 0.0f) break;
    const bool lead = (t < METER_FILL_S + 0.1f) && (k == (int)(t / METER_FILL_S * GUIDE_LEN) || k == GUIDE_LEN - 1);
    add(GUIDE_X[k], GUIDE_Y[k], lead ? PEACH : ORANGE_LIGHT, on * lv * METER_MAX);
  }
}

// ── UV lamp strikes (two flickers, ≥ 0.2 s each), then glows with a soft halo until the deposition
static void drawLamp(float t, float sec) {
  const float a = t - T_LAMP;
  if (a < 0.0f) return;
  float v;
  if      (a < 0.2f) v = 1.0f;
  else if (a < 0.4f) v = 0.10f;
  else if (a < 0.6f) v = 0.85f;
  else if (a < 0.8f) v = 0.15f;
  else               v = 0.4f + 0.6f * smooth(ramp(a, 0.8f, 0.4f));
  v *= 1.0f - ramp(t, T_L5, UV_OUT_S);
  if (v <= 0.0f) return;
  const float halo = smooth(ramp(a, 0.8f, 0.6f)) * (0.75f + 0.25f * sinf(6.2831853f * sec / 1.4f));
  for (int x = LAMP_X0; x <= LAMP_X1; x++) {
    add(x, LAMP_Y,     UV_HI, v * LAMP_MAX);
    add(x, LAMP_Y + 1, UV,    v * LAMP_MAX * (0.3f + 0.25f * halo));
  }
  for (int x = LAMP_X0 - 1; x <= LAMP_X1 + 1; x++) add(x, LAMP_Y + 2, UV, v * halo * LAMP_MAX * 0.12f);
  add(LAMP_X0 - 1, LAMP_Y, UV, v * halo * LAMP_MAX * 0.3f);
  add(LAMP_X1 + 1, LAMP_Y, UV, v * halo * LAMP_MAX * 0.3f);
}
// ── exposure: purple sweeps down over the wafer, then bands flow down; twinkles inside the light
static float rayDash(float x, float y, float flow) {
  float ph = fmodf(y - flow + COL_OFF[((int)x) & 15] * 0.25f, RAY_PERIOD);
  if (ph < 0.0f) ph += RAY_PERIOD;
  return clamp01(1.0f - absf(ph - 1.0f) / 0.9f);
}
static void drawUV(float t, float sec) {
  if (t < T_CURTAIN) return;
  const float fadeOut = 1.0f - ramp(t, T_L5, UV_OUT_S);
  if (fadeOut <= 0.0f) return;
  const float front  = -1.0f + 19.5f * ramp(t, T_CURTAIN, CURTAIN_S);
  const float raysOn = smooth(ramp(t, T_RAYS - 0.4f, 0.8f));
  const float flow   = sec * RAY_SPEED;
  for (int y = 0; y < 16; y++) {
    if (y > front + 1.0f) break;
    const float cur = (y > front) ? front + 1.0f - y : 0.18f + 0.82f * clamp01(1.0f - (front - y) / 2.5f);
    const float edge = (y > front - 1.2f && y <= front + 1.0f && front < 16.5f) ? 0.5f : 0.0f;   // bright sweep front
    for (int x = 0; x < 16; x++) {
      const float inside = clamp01(UV_R + 0.6f - discDist(x, y));
      float v;
      if (inside > 0.0f) {
        v = inside * (cur + raysOn * 0.55f * rayDash(x, y, flow));
        if (v > 1.0f) v = 1.0f;
      } else {
        v = 0.08f * (cur > 0.18f ? 0.18f : cur) / 0.18f;
      }
      v *= fadeOut;
      if (v > 0.0f) add(x, y, mix(UV, UV_HI, (v - 0.6f) / 0.4f), v * BACK_MAX);
      if (edge > 0.0f && inside > 0.0f) add(x, y, UV_HI, edge * inside * fadeOut * BACK_MAX);
    }
  }
  // twinkles: 7 at a time inside the UV light, 0.8 s each
  const float tw = smooth(ramp(t, T_RAYS, 0.8f)) * fadeOut;
  if (tw > 0.0f)
    for (int s = 0; s < 7; s++) {
      const float ph = (sec + s * 0.37f) / 0.8f;
      const uint32_t n = (uint32_t)ph;
      const float e = sinf(3.14159265f * (ph - n));
      const float ang = 6.2831853f * hash01(n, 100 + s), rad = UV_R * 0.92f * sqrtf(hash01(n, 200 + s));
      splat(DISC_CX + rad * cosf(ang), DISC_CY + rad * sinf(ang), UV_HI, e * tw * TWINKLE_MAX);
    }
}
// ── exposure: the 17 H shapes collect "dose" as the bands pass (top rows first), then flash at 9.6 s
static void drawLatent(float t, float sec) {
  if (t < T_RAYS || t > T_L5 + UV_OUT_S + 0.4f) return;
  const float flow = sec * RAY_SPEED;
  const float flash = (t >= T_EXPOSED) ? expf(-(t - T_EXPOSED) / 0.35f) : 0.0f;
  const float out = 1.0f - smooth(ramp(t, T_L5, UV_OUT_S + 0.4f));
  for (int i = 0; i < N_H; i++) {
    const float dose = smooth(ramp(t, T_RAYS + HY[i] * 0.08f, LATENT_S));
    if (dose <= 0.0f) continue;
    const float v = (dose * (0.7f + 0.3f * rayDash(HX[i], HY[i], flow)) * LATENT_MAX + flash * LATENT_MAX * 0.9f) * out;
    splat(HX[i], HY[i], flash > 0.3f ? mix(UV_HI, PEACH, flash) : UV_HI, v);
  }
}

// ── deposition: atoms fly from the source at the bottom into the 17 H shapes, which fill with warm light
static void drawAtoms(float t) {
  if (t < T_ATOMS) return;
  const float out = 1.0f - smooth(ramp(t, T_L6 - 0.4f, 0.8f));   // hands over to the result glow
  // source glow
  const float src = smooth(ramp(t, T_ATOMS, 0.4f)) * (1.0f - ramp(t, T_ATOMS + ATOMS_S + 0.4f, 0.8f));
  if (src > 0.0f) {
    glow(SRC_X, SRC_Y, 1.6f, ORANGE_LIGHT, src * 0.7f * ATOM_MAX);
    glow(SRC_X, SRC_Y + 0.4f, 2.6f, ORANGE_DEEP, src * 0.35f * ATOM_MAX);
  }
  for (int i = 0; i < N_H; i++) {
    float fill = 0.0f;
    for (int w = 0; w < 2; w++) {                                  // two atoms per H shape
      const float tl = T_ATOMS + ((i * 7 + w * 5) % N_H) / (float)N_H * ATOMS_S + w * 0.35f;
      const float fl = ATOM_FLIGHT_S * (0.9f + 0.25f * hash01(i, 30 + w));
      const float u = (t - tl) / fl;
      if (u >= 1.0f) {
        fill += 0.5f * smooth(ramp(t, tl + fl, 0.5f));
        const float imp = t - tl - fl;                             // small flash where it lands
        if (imp < 0.4f) glow(HX[i], HY[i], 1.2f, PEACH, (1.0f - imp / 0.4f) * 0.8f * ATOM_MAX * out);
      } else if (u > 0.0f) {
        const float side = (HX[i] - SRC_X) * 0.12f + (w ? 0.35f : -0.35f);
        const float bend = sinf(3.14159265f * u) * side;
        const float px = SRC_X + (HX[i] - SRC_X) * u + bend, py = SRC_Y + (HY[i] - SRC_Y) * u;
        const float v = clamp01(u / 0.15f) * ATOM_MAX * out;
        splat(px, py, mix(PEACH, GOLD, u), v);
        const float u2 = u - 0.06f;
        if (u2 > 0.0f) splat(SRC_X + (HX[i] - SRC_X) * u2 + sinf(3.14159265f * u2) * side,
                             SRC_Y + (HY[i] - SRC_Y) * u2, ORANGE, v * 0.35f);
      }
    }
    if (fill > 0.0f) splat(HX[i], HY[i], GOLD, fill * HFILL_MAX * out);
  }
}
// ── deposition: white light for the polariser film (rises from the bottom), or a warm glint without film
static void drawFilm(float t, float sec) {
#if BOX1_POLAR_FILM
  const float rise = -1.5f + 18.0f * ramp(t, T_FILM, FILM_IN_S);   // fill front moves up from the bottom
  const float out  = 1.0f - smooth(ramp(t, T_FILM_OUT, FILM_OUT_S));
  if (t < T_FILM || out <= 0.0f) return;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float inside = clamp01(FILM_R + 0.8f - discDist(x, y));
      if (inside <= 0.0f) continue;
      const float filled = clamp01((rise - (15.0f - y)) / 2.0f);   // 1 below the front
      if (filled <= 0.0f) continue;
      const float sheen = 0.88f + 0.12f * sinf(6.2831853f * ((x + y) / 14.0f - sec / 3.0f));
      add(x, y, FILM_WHITE, out * filled * inside * sheen * WHITE_MAX);
    }
#else
  (void)sec;
  if (t < T_FILM || t > T_FILM_OUT + FILM_OUT_S) return;
  const float b = -3.0f + 26.0f * ramp(t, T_FILM + 0.6f, FILM_IN_S + 0.8f);   // glint band, lower-left → upper-right
  for (int i = 0; i < N_H; i++) {
    const float s = (HX[i] + (15.0f - HY[i])) * 0.7071f;
    const float w = expf(-(s - b) * (s - b) / 1.6f);
    if (w > 0.02f) glow(HX[i], HY[i], 1.3f, PEACH, w * GLINT_MAX);
  }
#endif
}

// ── result: reveal ring, the H shapes light from the centre and follow the breathing; glint; teal wave
static void drawResult(float t, float sec) {
  const float a = t - T_L6;
  if (a >= 0.0f && a < 1.4f) {
    const float u = a / 1.4f;
    ring(DISC_CX, DISC_CY, 0.5f + u * 9.0f, 1.3f, mix(PEACH, ORANGE, u), (1.0f - u) * (1.0f - u) * RIPPLE_MAX * 0.7f);
  }
  if (t < T_HGLOW) return;
  const float br  = (t >= T_BREATH) ? 0.5f - 0.5f * cosf(6.2831853f * sec / BREATH_PERIOD_S) : 0.0f;
  const float bin = smooth(ramp(t, T_BREATH, BREATH_IN_S));
  const float gb  = -3.0f + 26.0f * ramp(t, T_GLINT, GLINT_S);    // glint band position
  const bool  glint = t >= T_GLINT && t < T_GLINT + GLINT_S;
  for (int i = 0; i < N_H; i++) {
    const float on = smooth(ramp(t, T_HGLOW + discDist(HX[i], HY[i]) * HGLOW_STEP_S * 4.0f, HGLOW_IN_S));
    if (on <= 0.0f) continue;
    float v = on * (1.0f - bin * 0.35f * (1.0f - br));
    float w = 0.0f;                                                // teal wave
    if (t >= T_WAVE && t < STORY_S) {
      const float front = 1.0f + 13.0f * ramp(t, T_WAVE, WAVE_S);
      const float since = (front - HX[i]) / 13.0f * WAVE_S;
      if (since >= 0.0f) w = expf(-since / WAVE_DECAY_S);
      w *= 1.0f - ramp(t, STORY_S - 1.0f, 0.7f);                 // gone before progress = 1
    }
    splat(HX[i], HY[i], mix(ORANGE_LIGHT, HAND, w), v * (HGLOW_MAX + w * 0.25f));
    if (glint) {
      const float s = (HX[i] + (15.0f - HY[i])) * 0.7071f;
      const float g = expf(-(s - gb) * (s - gb) / 1.6f);
      if (g > 0.02f) glow(HX[i], HY[i], 1.3f, PEACH, g * on * GLINT_MAX);
    }
  }
}
// ── finished: slow orange breathing (halo around the wafer + soft centre)
static void drawBreath(float t, float sec) {
  if (t < T_BREATH) return;
  const float in  = smooth(ramp(t, T_BREATH, BREATH_IN_S));
  const float s   = 0.5f - 0.5f * cosf(6.2831853f * sec / BREATH_PERIOD_S);
  const float lvl = (0.15f + 0.85f * s * s) * in;
  if (lvl <= 0.0f) return;
  const C3 c = mix(ORANGE_DEEP, ORANGE_LIGHT, s);
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float d    = discDist(x, y);
      const float fill = 1.0f - 0.85f * clamp01((d - 2.5f) / 6.5f);
      const float halo = clamp01(1.0f - absf(d - WAFER_R - 0.3f) / 1.8f);
      add(x, y, c, lvl * (0.35f * fill + 0.45f * halo) * BACK_MAX);
    }
}
// ── hand-over: teal particles stream from each H shape to the exit, then three teal dots leave the right edge
static void drawHandover(float t) {
  if (t < T_WAVE) return;
  const float ex = 15.6f;
  for (int i = 0; i < N_H; i++) {
    const float t0 = T_WAVE + (HX[i] - 1.0f) / 13.0f * WAVE_S;
    const float u = (t - t0) / PARTICLE_S;
    if (u <= 0.0f || u >= 1.0f) continue;
    const float e = u * u * (3.0f - 2.0f * u);
    const float px = HX[i] + (ex - HX[i]) * e, py = HY[i] + (EXIT_Y - HY[i]) * e;
    const float v = sinf(3.14159265f * u) * 0.55f * EXIT_MAX;
    splat(px, py, HAND, v);
  }
  for (int k = 0; k < N_EXIT; k++) {
    const float t0 = T_EXIT + k * EXIT_GAP_S;
    if (t < t0) continue;
    const float u = ramp(t, t0, EXIT_S);
    const float x0 = EXIT_X0[k], y0 = EXIT_Y0[k], x1 = 19.2f, y1 = EXIT_Y;   // whole trail past x = 15 at u = 1
    const float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    const float s = len * u * (0.5f + 0.5f * u);
    for (int j = 0; j < 10; j++) {
      const float d = j * 0.35f;
      if (d > 3.0f || s - d < 0.0f) break;
      const float w = 1.0f - d / 3.0f, f = (s - d) / len;
      splat(x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, j == 0 ? HAND_HI : HAND, w * w * EXIT_MAX * (j == 0 ? 1.0f : 0.6f));
    }
  }
}

// ── waiting: guide light runs 6 → 4 o'clock along the L1 angle scale; the alignment marks pulse
static void drawComet(float sec) {
  const float cycle = GUIDE_LEN * COMET_STEP_S + COMET_GAP_S;
  const float t = fmodf(sec, cycle);
  for (int k = 0; k < GUIDE_LEN; k++) {
    const float age = t - k * COMET_STEP_S;
    if (age < 0.0f) continue;
    const float v = age < COMET_ATTACK_S ? age / COMET_ATTACK_S : expf(-(age - COMET_ATTACK_S) / COMET_TAU_S);
    if (v < 0.004f) continue;
    add(GUIDE_X[k], GUIDE_Y[k], mix(ORANGE_DEEP, PEACH, v), v * COMET_MAX);
  }
  if (TARGET_GLOW > 0.0f) {
    const float p = 0.5f - 0.5f * cosf(6.2831853f * sec / 2.4f);
    for (int i = 0; i < 2; i++) glow(TGT_X[i], TGT_Y, 1.4f, ORANGE, (0.3f + 0.7f * p) * TARGET_GLOW);
    add(GUIDE_X[GUIDE_LEN - 1], GUIDE_Y[GUIDE_LEN - 1], ORANGE, (0.4f + 0.6f * p) * TARGET_GLOW);
  }
}

}  // namespace Box1Pattern

// =============================================================================
//  STANDARD FUNCTIONS (§6)
// =============================================================================
void renderBox1Bottom(double progress, double seconds, double level) {
  (void)seconds;   // the under-light follows progress only, so its finished scene is static
  Box1Pattern::clearAcc();
  Box1Pattern::drawBaseStory(Box1Pattern::storyTime(progress));
  Box1Pattern::flush(BOTTOM[0], level);
}

void renderBox1Rear(double progress, double seconds, double level) {
  using namespace Box1Pattern;
  const float t = storyTime(progress), sec = safeSeconds(seconds);
  clearAcc();
  drawLock(t);
  drawMeter(t);
  drawRipples(t);
  drawSparks(t);
  drawLamp(t, sec);
  drawUV(t, sec);
  drawLatent(t, sec);
  drawAtoms(t);
  drawFilm(t, sec);
  drawBreath(t, sec);
  drawResult(t, sec);
  drawHandover(t);
  filmGain();
  flush(REAR[0], level);
}

// =============================================================================
//  ADDED BY CHAN — waiting-for-a-visitor state (optional, see 00_READ_ME_FIRST.md §5)
//  seconds = time since the waiting state began; progress is not used.
// =============================================================================
void renderBox1IdleBottom(double progress, double seconds, double level) {
  using namespace Box1Pattern;
  (void)progress;
  const float sec = safeSeconds(seconds);
  clearAcc();
  drawRow(0, ORANGE, BASE_IDLE, 1.0f, 0.0f, -1.0f, 0.0f, false);   // L1 + L2 on, so the visitor
  drawRow(1, ORANGE, BASE_IDLE, 1.0f, 0.0f, -1.0f, 0.0f, false);   // sees both parts of the H
  if (IDLE_DRIFT > 0.0f) {                                        // highlights drift towards each other
    const float u = fmodf(sec, 3.2f) / 3.2f, e = sinf(3.14159265f * u);
    rowSpark(0, -0.5f + 16.0f * u, PEACH, e * IDLE_DRIFT);
    rowSpark(1, 15.5f - 16.0f * u, PEACH, e * IDLE_DRIFT);
  }
  flush(BOTTOM[0], level);
}

void renderBox1IdleRear(double progress, double seconds, double level) {
  (void)progress;
  Box1Pattern::clearAcc();
  Box1Pattern::drawComet(Box1Pattern::safeSeconds(seconds));
  Box1Pattern::flush(REAR[0], level);
}
