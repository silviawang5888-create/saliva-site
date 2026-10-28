/*
 * =============================================================================
 *  Box3_Connect.h  —  D2D Box 3 "Connect" · Electronics Integration Lab
 *  Animation module for the Box 3 under-light (BOTTOM[2]) and backlight (REAR[2])
 * =============================================================================
 *  Follows : "D2D Four-Box LED Animation Coding Standard" (21 Oct 2026)
 *  Design  : Chan Yeuk Hei — code generated with Claude (Anthropic), 21 Oct 2026
 *  STATUS  : Generated but not physically tested.
 *
 *  STANDARD FUNCTIONS (§6) — merge these into the complete 11-panel program:
 *    void renderBox3Bottom(double progress, double seconds, double level);  // writes BOTTOM[2] only
 *    void renderBox3Rear  (double progress, double seconds, double level);  // writes REAR[2]  only
 *
 *  Must already be declared BEFORE this file is included:
 *    CRGB (FastLED.h), BOTTOM[4], REAR[4], and the shared put(panel, x, y, color, brightness).
 *  This file only calls put(). It never calls FastLED.show(), delay(), FastLED.addLeds() or dot(),
 *  never declares a CRGB array, and never touches GPIO, brightness, colour order or power limits.
 *
 *  Drawing : effects are mixed in a private 16x16 float buffer, then every LED of the panel is
 *            written at most ONCE per call with put() (works with any put() blend mode).
 *  Timing  : everything follows `progress` (designed for 28 s, the standard default; another box
 *            duration stretches or compresses the story automatically). `seconds` is not needed.
 *            `level` (0–1) scales everything. progress >= 1 is the finished scene (all six layers
 *            on, faint trace on the backlight) — safe to keep calling as a retained scene.
 *  Coordinates (§4): under-light x 0–15 left→right, y = 15 front (L1), then every second row: 13, 11, 9, 7, y = 5 (L6); y = 0 rear;
 *                    backlight  x 0–15 left→right, y = 0 top … y = 15 bottom.
 * -----------------------------------------------------------------------------
 *  STORY — a body signal becomes a wireless device. One signal dot travels along the vias of the
 *  stacked circuit layers on the backlight, leaving a faint trace; each layer of the under-light
 *  lights only in the columns where that layer has artwork, and stays on (layers build up).
 *  (design seconds; progress = t / 28)
 *    0.0  a teal dot enters from the left edge (from Bridge 2) …
 *    2.0  … reaches the sensor: L1 sensor layer lights (teal); ring at the sensor
 *    3.0  weak teal signal crawls to the amplifier
 *    6.0  L2 lights (purple, amplified); ring; the dot becomes a bright purple packet
 *    8.6  L3 lights (processor): the chip "thinks" (3 pulses), status LED blinks twice
 *   13.2  L4 lights (power: coin cell); ring
 *   13.8  the packet runs through the regulator, round the coil 1.5 turns (L5 lights as it
 *         passes the coil start, ≈14.7 s), straight on to USB-C (L6 lights ≈27.3 s) and out of
 *         the right edge — no stop and no flash between L5 and L6
 *   28.0  the dot has just left the panel → Bridge 3 starts here (progress = 1)
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include <math.h>

namespace Box3Connect {

// ---------------------------------------------------------------------------
// 1) Installation — logical positions (change these, never the drawing code)
// ---------------------------------------------------------------------------
// Under-light row under each acrylic layer (L1 = front layer).   ⚠ confirm on the real box
static const int8_t LAYER_Y[6]  = { 15, 13, 11, 9, 7, 5 };   // every second row (27 Oct)
static const int8_t LAYER_Y2[6] = { -1, -1, -1, -1, -1, -1 };   // optional second row under a layer (-1 = none)

// Each layer lights only the columns where it has artwork (bit c = column x = c, 0 = left).
static const uint16_t LAYER_COLS[6] = {
  0x007C,   // L1 sensor layer     columns 2–6   (left: sensor pad + signal line)
  0x1FC0,   // L2 analogue layer   columns 6–12  (amplifier + ADC)
  0x3F00,   // L3 processor layer  columns 8–13  (Bluetooth chip)
  0x1FF8,   // L4 power layer      columns 3–12  (coin cell + regulator)
  0x7FFE,   // L5 antenna coil     columns 1–14  (whole coil)
  0xF820    // L6 USB-C layer      columns 11–15 + column 5 (regulator footprint)
};
// Column where the signal arrives in each layer; the layer lights outwards from there
static const float LAYER_ENTRY[6] = { 3.0f, 6.1f, 11.9f, 9.5f, 3.1f, 12.8f };

// Signal route on the backlight, in LED units (vias and chips from the Box 3 drawings).
// Assumes the backlight's bottom edge is level with the bottom edge of the acrylic.   ⚠ confirm on the real box
struct P2 { float x, y; };
static const int ROUTE_N = 20;
static const P2 ROUTE[ROUTE_N] = {
  { -0.8f, 10.8f },  //  0 enters at the left edge (from Bridge 2)       ← match Bridge 2's exit row
  {  3.0f, 10.8f },  //  1 sensor pad (L1)
  {  6.1f,  9.0f },  //  2 via → L2
  {  8.85f, 8.6f },  //  3 amplifier / ADC (L2)
  { 11.9f,  8.6f },  //  4 via → L3
  { 11.5f,  5.6f },  //  5 processor (L3)
  {  9.5f,  3.4f },  //  6 via → L4
  {  7.7f,  1.7f },  //  7 coin cell (L4)
  {  5.0f,  2.1f },  //  8 regulator
  {  3.1f,  0.3f },  //  9 via → L5 (coil start, top left)
  { 14.1f,  0.3f },  // 10 coil, outer turn: top right
  { 14.1f, 14.1f },  // 11 bottom right
  {  0.9f, 14.1f },  // 12 bottom left
  {  0.9f,  1.1f },  // 13 top left → inner turn
  { 13.3f,  1.1f },  // 14 inner turn: top right
  { 13.3f,  8.9f },  // 15 inner end of the coil
  { 12.8f,  8.9f },  // 16 via → L6
  { 12.4f, 10.7f },  // 17 ESD protection chip (L6)
  { 14.8f, 10.7f },  // 18 USB-C
  { 19.0f, 10.7f }   // 19 leaves the right edge (to Bridge 3); whole trail is past x = 15 at progress = 1
};
static const P2 STATUS_LED = { 8.9f, 4.3f };   // L3 status LED

// ---------------------------------------------------------------------------
// 2) Colours (standard §5: Box 3 = Connect #6D33BD) and levels (0–1)
// ---------------------------------------------------------------------------
struct C3 { uint8_t r, g, b; };
static constexpr C3 PURPLE      = { 109,  51, 189 };   // Box 3 colour #6D33BD — main colour
static constexpr C3 LILAC       = { 190, 150, 255 };   // highlight: packet head, a layer's first flash
static constexpr C3 PURPLE_DEEP = {  45,  10, 120 };   // packet tail
static constexpr C3 TEAL        = {  86, 209, 194 };   // Box 2 colour #56D1C2 — the signal arriving from Box 2
// Accent used by the L1 sensor layer, the incoming signal and the status LED.
// Chan's design = TEAL (needs colour approval, standard §5). Purple-only version: set to PURPLE.
static constexpr C3 SENSOR_COL  = TEAL;

static const float BASE_ON   = 200 / 255.0f;   // a layer that is on
static const float BASE_PEAK = 1.0f;           // the moment a layer lights
static const float BACK_MAX  = 120 / 255.0f;   // backlight cap (keeps the engraving readable)
static const float TRACE_LVL = 0.26f;          // faint trace left on the route the signal has passed

// ---------------------------------------------------------------------------
// 3) Timeline — design seconds; progress 0..1 is mapped onto STORY_S.
//    The dot never moves faster than ~0.6 LED per frame at 10 fps.
// ---------------------------------------------------------------------------
static const float STORY_S = 28.0f;   // standard default box length
struct Seg { uint8_t from, to; float t0, dur, bright, tail; };
static const int N_SEGS = 5;
static const Seg SEGS[N_SEGS] = {     // route points from → to, start, duration, brightness, trail length
  {  0,  1,  0.0f,  2.0f, 0.60f, 1.6f },   // enters from the left → sensor (teal)
  {  1,  3,  3.0f,  3.0f, 0.45f, 1.5f },   // weak sensor signal → amplifier (teal, dim, slow)
  {  3,  5,  6.8f,  1.8f, 1.00f, 2.4f },   // amplified → processor (purple)
  {  5,  7, 11.4f,  1.8f, 1.00f, 2.4f },   // processor → coin cell
  {  7, 19, 13.8f, 14.2f, 1.00f, 2.8f }    // regulator → coil 1.5 turns → USB-C → out, one continuous run
};
static const float WIPE_S    = 0.8f;   // a layer lights outwards from its entry column
static const float FLASH_S   = 1.0f;   // a layer's first flash fades
static const float BURST_S   = 0.9f;   // ring when the signal arrives
static const float PROCESS_S = 2.8f;   // processor "thinking"

// ---------------------------------------------------------------------------
// Implementation — normally no need to edit below this line
// ---------------------------------------------------------------------------
static float g_acc[16][16][3];   // private mixing buffer for ONE panel (floats, not an LED array; never shown directly)
static bool  g_ready = false;
static float g_cum[ROUTE_N];     // route length up to each point
static float g_onT[6];           // when each layer lights (design seconds)
static float g_wipe[6];          // how long each layer takes to light
static float g_flash[6];         // how long its first flash lasts

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
static inline float storyTime(double progress) { return clamp01((float)progress) * STORY_S; }

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
static void splat(float fx, float fy, C3 c, float v) {   // sub-LED position spread over 4 LEDs (smooth at 10 fps)
  if (!(v > 0.0f)) return;
  const int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
  const float ax = fx - x0, ay = fy - y0;
  add(x0,     y0,     c, v * (1.0f - ax) * (1.0f - ay));
  add(x0 + 1, y0,     c, v * ax * (1.0f - ay));
  add(x0,     y0 + 1, c, v * (1.0f - ax) * ay);
  add(x0 + 1, y0 + 1, c, v * ax * ay);
}
static void flush(uint8_t panel, double level) {   // every LED at most once, brightness kept within 0–1
  const float lv = clamp01((float)level);
  if (lv <= 0.0f) return;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      float r = g_acc[y][x][0], g = g_acc[y][x][1], b = g_acc[y][x][2];
      if (r > 1.0f) r = 1.0f;
      if (g > 1.0f) g = 1.0f;
      if (b > 1.0f) b = 1.0f;
      const float m = r > g ? (r > b ? r : b) : (g > b ? g : b);
      const float bri = m * lv;
      if (bri < 0.002f) continue;   // would round to "off"
      CRGB col((uint8_t)(r / m * 255.0f + 0.5f), (uint8_t)(g / m * 255.0f + 0.5f), (uint8_t)(b / m * 255.0f + 0.5f));
      put(panel, x, y, col, bri);
    }
}

// ── Route helpers
static float segTimeAt(int k, int idx) {   // when segment k's dot passes route point idx
  const Seg& s = SEGS[k];
  const float u = (g_cum[idx] - g_cum[s.from]) / (g_cum[s.to] - g_cum[s.from]);
  return s.t0 + u * s.dur;
}
static void prepare() {   // derived values, computed once (pure function of the constants above)
  if (g_ready) return;
  g_cum[0] = 0.0f;
  for (int i = 1; i < ROUTE_N; i++) {
    const float dx = ROUTE[i].x - ROUTE[i - 1].x, dy = ROUTE[i].y - ROUTE[i - 1].y;
    g_cum[i] = g_cum[i - 1] + sqrtf(dx * dx + dy * dy);
  }
  for (int k = 0; k < 6; k++) { g_wipe[k] = WIPE_S; g_flash[k] = FLASH_S; }
  g_onT[0] = SEGS[0].t0 + SEGS[0].dur;       // L1: signal reaches the sensor
  g_onT[1] = SEGS[1].t0 + SEGS[1].dur;       // L2: weak signal reaches the amplifier
  g_onT[2] = SEGS[2].t0 + SEGS[2].dur;       // L3: reaches the processor
  g_onT[3] = SEGS[3].t0 + SEGS[3].dur;       // L4: reaches the coin cell
  g_onT[4] = segTimeAt(4, 9);                // L5: passes the coil start …
  g_wipe[4] = segTimeAt(4, 11) - g_onT[4];   //     … and lights while the dot runs the first half turn
  g_onT[5] = segTimeAt(4, 18);               // L6: reaches USB-C
  float left = STORY_S - g_onT[5] - 0.1f;    // L6 must be complete (no flash left) at progress = 1
  if (left < 0.05f) left = 0.05f;
  if (g_wipe[5]  > left) g_wipe[5]  = left;
  if (g_flash[5] > left) g_flash[5] = left;
  g_ready = true;
}
static void routeAt(float s, float& x, float& y) {   // point at distance s along the route
  if (s <= 0.0f) { x = ROUTE[0].x; y = ROUTE[0].y; return; }
  for (int i = 1; i < ROUTE_N; i++) {
    if (s <= g_cum[i] || i == ROUTE_N - 1) {
      const float len = g_cum[i] - g_cum[i - 1];
      const float u = len > 0.0f ? clamp01((s - g_cum[i - 1]) / len) : 1.0f;
      x = ROUTE[i - 1].x + (ROUTE[i].x - ROUTE[i - 1].x) * u;
      y = ROUTE[i - 1].y + (ROUTE[i].y - ROUTE[i - 1].y) * u;
      return;
    }
  }
}
static float segPos(int k, float t) {   // where segment k's dot is at time t
  const Seg& s = SEGS[k];
  return g_cum[s.from] + (g_cum[s.to] - g_cum[s.from]) * ramp(t, s.t0, s.dur);
}

// ── Backlight pieces
static void drawTrace(float s1, float lvl) {   // faint trace on the route already passed
  for (float s = 0.0f; s <= s1; s += 0.5f) {
    float x, y;
    routeAt(s, x, y);
    splat(x, y, s < g_cum[2] ? SENSOR_COL : PURPLE, lvl * 0.5f * BACK_MAX);
  }
}
static void drawPacket(float s, float bright, float tail, C3 head) {   // the signal dot with its trail
  if (bright <= 0.0f) return;
  for (int j = 0; j < 12; j++) {
    const float d = j * 0.35f;
    if (d > tail) break;
    const float ss = s - d;
    if (ss < 0.0f) break;
    const float w = 1.0f - d / tail;
    float x, y;
    routeAt(ss, x, y);
    splat(x, y, j == 0 ? head : mix(PURPLE_DEEP, head, w), bright * w * w * BACK_MAX * (j == 0 ? 1.0f : 0.6f));
  }
}
static void drawRing(P2 c, float r, float width, float lvl, C3 col) {
  if (lvl <= 0.0f) return;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      const float dx = x - c.x, dy = y - c.y;
      const float v = 1.0f - absf(sqrtf(dx * dx + dy * dy) - r) / width;
      if (v > 0.0f) add(x, y, col, v * lvl * BACK_MAX);
    }
}
static void drawBurst(P2 c, float t, float t0, float rMax, C3 col) {   // ring when the signal arrives
  if (t < t0 || t > t0 + BURST_S) return;
  const float u = (t - t0) / BURST_S;
  drawRing(c, 0.2f + u * rMax, 1.1f, (1.0f - u) * (1.0f - u) * 0.9f, col);
}

// ── Under-light: one layer, only its artwork columns, lighting outwards from the entry column
static void drawLayer(int k, C3 c, float level, float wipe) {
  if (level <= 0.0f || wipe <= 0.0f) return;
  float far = 0.0f;
  for (int x = 0; x < 16; x++)
    if ((LAYER_COLS[k] >> x) & 1) { const float d = absf(x - LAYER_ENTRY[k]); if (d > far) far = d; }
  for (int x = 0; x < 16; x++) {
    if (!((LAYER_COLS[k] >> x) & 1)) continue;
    const float v = level * clamp01((wipe * (far + 1.5f) - absf(x - LAYER_ENTRY[k])) / 1.5f);
    if (LAYER_Y[k]  >= 0) add(x, LAYER_Y[k],  c, v);
    if (LAYER_Y2[k] >= 0) add(x, LAYER_Y2[k], c, v);
  }
}

static void drawBase(float t) {   // layers light as the signal arrives and stay on (they build up)
  for (int k = 0; k < 6; k++) {
    if (t < g_onT[k]) continue;
    const float wipe   = smooth(ramp(t, g_onT[k], g_wipe[k]));
    const float arrive = 1.0f - ramp(t, g_onT[k], g_flash[k]);
    const float level  = BASE_ON + (BASE_PEAK - BASE_ON) * arrive;
    drawLayer(k, mix(k == 0 ? SENSOR_COL : PURPLE, LILAC, arrive * 0.7f), level, wipe);
  }
}

static void drawBack(float t) {
  float head = 0.0f;                                          // how far the signal has got
  for (int k = 0; k < N_SEGS; k++) if (t >= SEGS[k].t0) head = segPos(k, t);
  drawTrace(head < g_cum[18] ? head : g_cum[18], TRACE_LVL);   // trace up to USB-C
  for (int k = 0; k < N_SEGS; k++)                            // the moving dot
    if (t >= SEGS[k].t0 && t < SEGS[k].t0 + SEGS[k].dur)
      drawPacket(segPos(k, t), SEGS[k].bright, SEGS[k].tail, k <= 1 ? SENSOR_COL : LILAC);
  drawBurst(ROUTE[1], t, g_onT[0], 2.8f, SENSOR_COL);         // sensor receives the signal
  drawBurst(ROUTE[3], t, g_onT[1], 3.4f, LILAC);              // amplified: bigger ring
  drawBurst(ROUTE[7], t, g_onT[3], 2.6f, LILAC);              // power
  if (t >= g_onT[2] && t < g_onT[2] + PROCESS_S) {             // processor "thinking" + status LED
    const float u = (t - g_onT[2]) / PROCESS_S;
    const float pulse = (0.5f - 0.5f * cosf(6.2831853f * 3.0f * u)) * (1.0f - u * 0.5f);
    for (int y = 3; y <= 8; y++)
      for (int x = 9; x <= 14; x++) {
        const float dx = x - ROUTE[5].x, dy = y - ROUTE[5].y;
        const float v = clamp01(1.0f - sqrtf(dx * dx + dy * dy) / 1.8f);
        if (v > 0.0f) add(x, y, LILAC, v * pulse * 0.8f * BACK_MAX);
      }
    if ((u > 0.10f && u < 0.25f) || (u > 0.45f && u < 0.60f))   // each blink ≈ 0.4 s (readable at 10 fps)
      splat(STATUS_LED.x, STATUS_LED.y, SENSOR_COL, 0.9f * BACK_MAX);
  }
}

}  // namespace Box3Connect

// =============================================================================
//  STANDARD FUNCTIONS (§6)
// =============================================================================
void renderBox3Bottom(double progress, double seconds, double level) {
  (void)seconds;
  Box3Connect::prepare();
  Box3Connect::clearAcc();
  Box3Connect::drawBase(Box3Connect::storyTime(progress));
  Box3Connect::flush(BOTTOM[2], level);
}

void renderBox3Rear(double progress, double seconds, double level) {
  (void)seconds;
  Box3Connect::prepare();
  Box3Connect::clearAcc();
  Box3Connect::drawBack(Box3Connect::storyTime(progress));
  Box3Connect::flush(REAR[2], level);
}
