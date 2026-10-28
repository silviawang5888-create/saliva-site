/*
 * =============================================================================
 *  D2D_Bridges_1_2.h  —  Bridge 1 (Box 1 → Box 2) and Bridge 2 (Box 2 → Box 3)     v2, 24 Oct 2026
 *  32×8 bridge panels BRIDGE[0] and BRIDGE[1]
 * =============================================================================
 *  Follows : "D2D Four-Box LED Animation Coding Standard" (21 Oct 2026), §3, §4 (32×8 bridge), §6, §7
 *  Design  : Chan Yeuk Hei — code generated with Claude (Anthropic)
 *  STATUS  : Generated but not physically tested.
 *
 *  v2 (24 Oct): richer, more "flowing / carried across"; Bridge 1 stays teal the whole way
 *               (the orange spark that turned into Box 2's pattern is gone).
 *
 *  STANDARD FUNCTIONS (§6):
 *    void renderBridge1(double progress, double seconds, double level);   // writes BRIDGE[0] only
 *    void renderBridge2(double progress, double seconds, double level);   // writes BRIDGE[1] only
 *
 *  Must already be declared BEFORE this file is included:
 *    CRGB (FastLED.h), BRIDGE[3], and the shared put(panel, x, y, color, brightness).
 *  Only put() is called, every LED at most once per call. No show(), delay(), addLeds(), GPIO,
 *  brightness or power settings. No state is kept between calls: every frame is a pure function
 *  of (progress, seconds, level), so frames can be skipped or repeated safely.
 *
 *  Designed for 5 s per bridge (the standard default is 6 s — the story follows `progress`,
 *  so 6 s also works, just a little slower). progress >= 1 = finished scene (retained, dim, still
 *  flowing with `seconds`). Nothing moves faster than ~0.9 LED per frame at 10 fps.
 *
 *  ONE SCREEN: the bridge's 8 rows continue rows 4–11 of the 16×16 backlights ("starts at the 5th LED
 *  from the bottom"), i.e. backlight row = bridge row + BRIDGE_TOP_ROW.
 *    Bridge 1 in : Box 1's last teal hand-over dot leaves Box 1 at backlight row 7.2   → bridge row 3.2
 *    Bridge 1 out: into Box 2 (Biosensor) at its centre row ≈ 7.5                     → bridge row 3.5
 *    Bridge 2 in : Box 2 ends with PURPLE pulses running right at its row ≈ 7.5         → bridge row 3.5
 *    Bridge 2 out: Box 3 (Electronics) starts with a TEAL dot entering at row 10.8     → bridge row 6.8
 *
 *  BRIDGE 1 — "twin teal light streams" (Deposition → Biosensor). Teal only (Box 1's hand-over colour =
 *  Box 2's colour; Box 2 opens with teal / blue bubbles, so the colour carries straight on).
 *      0.0  Box 1's last teal dot flies in at row 3.2 → it becomes the HEAD of a stream; two ripple
 *           rings spread from the entry
 *      0.0–3.6  the head runs right (8.9 → 8.0 LED/s); behind it the light unwinds into TWO STRANDS
 *           (teal + blue-teal) that twist around each other: the knots where they cross drift right,
 *           light streaks run along both strands, droplets are carried along in the dark "eyes"
 *           between them, faint water with glints fills the eyes, and a soft pulse of light passes
 *           through every 1.25 s. The loops vary in size (the strands wander), so it looks like water.
 *      3.6  the head leaves the right edge at row 3.5 → a short flare where it pours into Box 2
 *      4.15–5.0  the stream settles (narrower, dimmer) into the retained scene
 *      retained: the same stream at 34 %, still flowing; pulses weaker
 *  BRIDGE 2 — "signal + counter-flow" (Biosensor → Electronics), from Chan's sketch (24 Oct):
 *      0.0  the purple HEAD of the signal line enters at row 3.5 (continues Box 2's purple pulses)
 *      0.0–4.6  the head draws a wavy signal line (7 LED/s). The line turns PURPLE → BLUE → TEAL and
 *           its wave turns from a smooth "biological" sine (left) into a square "electronic" wave
 *           (right, levels on rows 2 and 5), then steps down to Box 3's entry row 6.8. Data bits
 *           (short / long) ride along it. Behind the head, 2 + 2 flows fan out above (rows 0, 1) and
 *           below (rows 6, 7), straight like the sketch and coloured the opposite way (TEAL → PURPLE):
 *           irregular streaks on the left, regular clock-like dashes on the right, brighter towards
 *           Electronics; brighter beads overtake along them. At the right end the upper flows bend
 *           down towards the exit and the lower flows merge into the line.
 *      4.6  the teal head leaves the right edge at row 6.8 → Box 3's teal dot takes over at progress 0
 *      4.0–5.0  everything settles into the retained scene
 *      retained: line, bits and flows at 32 %, still moving
 * =============================================================================
 */
#pragma once
#include <stdint.h>
#include <math.h>

namespace D2DBridge {

// ---- 1) installation ---------------------------------------------------------
static const float BRIDGE_TOP_ROW = 4.0f;   // backlight row level with bridge row 0 (bridge = backlight rows 4–11)   ⚠ check
static const float B1_IN_Y  = 7.2f - BRIDGE_TOP_ROW;    // Box 1 exit row (EXIT_Y in Box1_Pattern.h)
static const float B1_OUT_Y = 7.5f - BRIDGE_TOP_ROW;    // Box 2 centre row (from the Biosensor animation)
static const float B2_IN_Y  = 7.5f - BRIDGE_TOP_ROW;    // Box 2 exit row (its purple pulses)
static const float B2_OUT_Y = 10.8f - BRIDGE_TOP_ROW;   // Box 3 entry row (ROUTE[0].y in Box3_Connect.h)

// ---- 2) colours (standard §5: a bridge may blend its two neighbours' colours) -------
struct C3 { uint8_t r, g, b; };
static constexpr C3 TEAL      = {  86, 209, 194 };   // Box 2 #56D1C2
static constexpr C3 TEAL_HI   = { 160, 240, 226 };   // highlights (kept away from white)
static constexpr C3 TEAL_DEEP = {  10,  84, 112 };   // deep water
static constexpr C3 AQUA      = {  36, 160, 226 };   // blue-teal, outer lanes (Box 2's bubbles are teal / blue)
static constexpr C3 PURPLE    = { 109,  51, 189 };   // Box 3 #6D33BD (also Box 2's ending colour)
static constexpr C3 LILAC     = { 182, 140, 255 };
static constexpr C3 BLUE_MID  = {  64, 112, 228 };   // half-way between purple and teal ("a bit blue")

// ---- 3) levels (0–1, before `level`) --------------------------------------------
static const float B1_HEAD_MAX   = 240 / 255.0f;   // the head (Box 1's dot)
static const float B1_STREAK_MAX = 235 / 255.0f;   // the two strands and the light running along them
static const float B1_BODY_MAX   =  34 / 255.0f;   // faint water between the strands
static const float B1_GLINT_MAX  =  80 / 255.0f;   // glints in the water
static const float B1_PART_MAX   = 220 / 255.0f;   // particles carried along
static const float B1_PULSE_MAX  = 100 / 255.0f;   // pulses travelling through the stream
static const float B1_RING_MAX   = 150 / 255.0f;   // entry ripples / exit flare
static const float B1_REST       = 0.34f;          // retained scene = 34 % of the story levels

static const float B2_HEAD_MAX   = 240 / 255.0f;   // the head of the signal line
static const float B2_LINE_MAX   = 150 / 255.0f;   // the signal line
static const float B2_BIT_MAX    = 210 / 255.0f;   // data bits riding the line
static const float B2_FLOW_MAX   = 175 / 255.0f;   // the 2 + 2 flows
static const float B2_SPARK_MAX  = 190 / 255.0f;   // beads overtaking along the flows
static const float B2_REST       = 0.32f;          // retained scene = 32 % of the story levels

// ---- 4) timeline (design seconds) and speeds (LED per second; 10 = 1 LED per frame) ----
static const float STORY_S = 5.0f;
static const float B1_HEAD_X0 = 0.6f, B1_HEAD_V0 = 8.9f, B1_HEAD_DEC = 0.12f;    // x = X0 + (V0 − DEC·t)·t; Box 1's dot is
                                                                                   // ≈ 1 LED into the bridge at progress 1
static const float B1_SETTLE_T = 4.15f;         // stream settles 4.15 → 5.0 s
static const float B1_PULSE_EVERY = 1.25f, B1_PULSE_V = 8.8f;
static const float B2_HEAD_X0 = -1.0f, B2_HEAD_V = 7.0f;   // head leaves x = 31 at ≈ 4.6 s
static const float B2_SETTLE_T = 4.0f;          // settles 4.0 → 5.0 s
static const float B2_WAVE_V = 4.3f;            // the waveform (and the data bits on it) scrolls right

// ---- implementation ---------------------------------------------------------------
static float g_b[8][32][3];
static inline float clamp01(float v) { return v > 0.0f ? (v < 1.0f ? v : 1.0f) : 0.0f; }
static inline float ramp(float t, float t0, float d) { return clamp01((t - t0) / d); }
static inline float smooth(float u) { u = clamp01(u); return u * u * (3.0f - 2.0f * u); }
static inline float absf(float v) { return v < 0.0f ? -v : v; }
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline C3 mix(C3 a, C3 b, float t) {
  t = clamp01(t);
  C3 c = { (uint8_t)(a.r + (b.r - a.r) * t + 0.5f), (uint8_t)(a.g + (b.g - a.g) * t + 0.5f), (uint8_t)(a.b + (b.b - a.b) * t + 0.5f) };
  return c;
}
static inline C3 mix3(C3 a, C3 b, C3 c, float u) { u = clamp01(u); return u < 0.5f ? mix(a, b, smooth(u * 2.0f)) : mix(b, c, smooth(u * 2.0f - 1.0f)); }
static inline float storyTime(double p) { return (p > 0.0) ? (p < 1.0 ? (float)p * STORY_S : STORY_S) : 0.0f; }   // NaN → 0
static inline float safeSeconds(double s) { return (s > 0.0 && s < 1.0e6) ? (float)s : 0.0f; }
static inline float hash01(uint32_t a, uint32_t b) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return (float)(h & 0xFFFFFFu) / 16777216.0f;
}
static inline uint32_t slotId(float n) { return (uint32_t)(int32_t)n; }   // floor() result → stable id (negative ok)
static void clear() { for (int y = 0; y < 8; y++) for (int x = 0; x < 32; x++) g_b[y][x][0] = g_b[y][x][1] = g_b[y][x][2] = 0.0f; }
static inline void add(int x, int y, C3 c, float v) {
  if (!(v > 0.0f) || x < 0 || x > 31 || y < 0 || y > 7) return;
  if (v > 1.0f) v = 1.0f;
  const float k = v / 255.0f;
  g_b[y][x][0] += c.r * k; g_b[y][x][1] += c.g * k; g_b[y][x][2] += c.b * k;
}
static void splat(float fx, float fy, C3 c, float v) {   // anti-aliased dot at a fractional position
  if (!(v > 0.0f)) return;
  const int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
  const float ax = fx - x0, ay = fy - y0;
  add(x0, y0, c, v * (1 - ax) * (1 - ay)); add(x0 + 1, y0, c, v * ax * (1 - ay));
  add(x0, y0 + 1, c, v * (1 - ax) * ay);   add(x0 + 1, y0 + 1, c, v * ax * ay);
}
static inline void addRow(int x, float fy, C3 c, float v) {   // anti-aliased in y only (column x)
  if (!(v > 0.0f)) return;
  const int y0 = (int)floorf(fy);
  const float a = fy - y0;
  add(x, y0, c, v * (1 - a)); add(x, y0 + 1, c, v * a);
}
static void flush(uint8_t panel, double level) {   // every LED at most once; overlaps keep their hue
  const float lv = (level > 0.0) ? (level < 1.0 ? (float)level : 1.0f) : 0.0f;   // NaN → 0
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

// =========================== BRIDGE 1: twin teal light streams ===========================
// two strands of light (teal + blue-teal) twist around each other along the centre row like a twisted
// stream; they unwind from the head, cross in bright knots that drift right, leave dark "eyes" where
// droplets are carried along, and gather again into Box 2's entry. Light runs along both strands.
static const int B1_STRANDS = 2;
static const float B1_STRAND_A = 2.2f;      // half-height of the twist (rows)
static const float B1_STRAND_L = 16.0f;     // twist wavelength (LED) → a knot every 8 LED
static const float B1_STRAND_V = 5.0f;      // the twist drifts right (LED/s)
static const float B1_COMET_V[B1_STRANDS] = { 8.6f, 7.8f };   // light running along each strand
static const int B1_PARTICLES = 10;

static inline float b1Head(float t) { return B1_HEAD_X0 + (B1_HEAD_V0 - B1_HEAD_DEC * t) * t; }
static inline float b1HeadExitT() {   // time the head passes x = 31
  const float a = B1_HEAD_DEC, b = -B1_HEAD_V0, c = 31.0f - B1_HEAD_X0;
  return (-b - sqrtf(b * b - 4.0f * a * c)) / (2.0f * a);
}
static inline float b1Centre(float x) { return B1_IN_Y + (B1_OUT_Y - B1_IN_Y) * clamp01(x / 31.0f); }   // 3.2 → 3.5
static inline float b1Open(float x, float head, float settle) {   // 0..1: how far the twist is opened at x
  const float u = clamp01(x / 31.0f);
  const float ends = 0.2f + 0.8f * smooth(u * 4.0f) * smooth((1.0f - u) * 4.5f);   // gathered at entry / exit
  return ends * smooth((head - x) / 5.5f) * (1.0f - 0.3f * settle);                 // opens behind the head
}
static inline float b1Amp(float x, float sec) { return 0.8f + 0.2f * sinf(6.2831853f * (x / 21.0f + 0.17f * sec)); }   // loops vary in size
static inline float b1Twist(float x, float sec) { return sinf(6.2831853f * (x - B1_STRAND_V * sec) / B1_STRAND_L); }
static inline float b1StrandY(int k, float x, float sec, float w) {
  const float wander = k ? 0.55f * sinf(6.2831853f * (x / 29.0f - 0.13f * sec)) : 0.0f;   // strand B drifts against A
  const float ph = 6.2831853f * (x - B1_STRAND_V * sec) / B1_STRAND_L + wander;
  return b1Centre(x) + (k ? -1.0f : 1.0f) * w * B1_STRAND_A * b1Amp(x, sec) * sinf(ph);
}
static inline float b1Pulse(float x, float sec) {   // soft pulses travelling through the stream every 1.25 s
  const int j1 = (int)floorf(sec / B1_PULSE_EVERY);
  float p = 0.0f;
  for (int j = j1; j >= j1 - 4; j--) {
    const float d = (x - (-4.0f + B1_PULSE_V * (sec - j * B1_PULSE_EVERY))) / 1.6f;
    p += expf(-d * d);
  }
  return p;
}
// light streak pattern along a strand (u = position in the moving frame); bright head at the front
static float b1Streak(float u, uint32_t lane, float& headness) {
  const float SLOT = 8.0f + 2.0f * lane;
  const float n = floorf(u / SLOT), p = u - n * SLOT;
  const uint32_t id = slotId(n);
  headness = 0.0f;
  if (hash01(id, lane * 7 + 1) < 0.1f) return 0.0f;                  // some gaps
  const float len = 2.5f + 3.5f * hash01(id, lane * 7 + 2);           // 2.5 – 6 LED long
  const float hp = len + (SLOT - len - 0.8f) * hash01(id, lane * 7 + 3);
  const float d = hp - p;                                             // distance behind the streak's head
  float v;
  if (d < 0.0f) v = clamp01(1.0f + d / 0.8f);
  else if (d < len) { const float a = 1.0f - d / len; v = a * sqrtf(a); }
  else return 0.0f;
  headness = clamp01(1.0f - absf(d) / 1.3f);
  return v * (0.6f + 0.4f * hash01(id, lane * 7 + 4));
}

static void bridge1(float t, float sec) {
  const float head = b1Head(t);
  const float settle = smooth(ramp(t, B1_SETTLE_T, STORY_S - B1_SETTLE_T));
  const float E = 1.0f - (1.0f - B1_REST) * settle;
  const float pulseK = 1.0f - 0.55f * settle;
  // ---- faint water between the strands, with a few glints ----
  for (int x = 0; x < 32; x++) {
    const float pres = clamp01((head - x) / 1.2f);
    if (pres <= 0.0f) continue;
    const float w = b1Open((float)x, head, settle);
    const float half = 0.6f + 0.5f * absf(b1StrandY(0, (float)x, sec, w) - b1StrandY(1, (float)x, sec, w));   // the "eye" here
    const float yc = b1Centre((float)x);
    const float pl = b1Pulse((float)x, sec) * pulseK;
    for (int y = 0; y < 8; y++) {
      const float dy = absf(y - yc) / half;
      if (dy > 1.0f) continue;
      const float prof = 1.0f - dy * dy;
      add(x, y, mix(TEAL_DEEP, TEAL, 0.3f + 0.3f * pl), prof * (B1_BODY_MAX + pl * B1_PULSE_MAX * 0.3f) * pres * E);
      const float c = sinf(0.95f * x + 1.25f * y - 5.1f * sec) + sinf(0.72f * x - 1.05f * y - 3.9f * sec + 1.3f)
                    + sinf(1.2f * x + 0.35f * y - 6.4f * sec + 2.2f);
      const float g = clamp01((c - 1.8f) / 0.8f);
      if (g > 0.0f) add(x, y, TEAL_HI, g * g * prof * B1_GLINT_MAX * pres * E * smooth((head - x) / 6.0f));
    }
  }
  // ---- the two strands, with light running along them ----
  const float STEP = 0.25f;
  for (int k = 0; k < B1_STRANDS; k++) {
    const C3 base = k ? AQUA : TEAL;
    float py = b1StrandY(k, -0.5f, sec, b1Open(-0.5f, head, settle));
    for (float x = -0.5f + STEP; x <= 31.5f; x += STEP) {
      const float pres = clamp01((head - x) / 1.2f);
      if (pres <= 0.0f) break;
      const float w = b1Open(x, head, settle);
      const float y = b1StrandY(k, x, sec, w);
      const float ds = sqrtf(STEP * STEP + (y - py) * (y - py));
      py = y;
      float hd;
      const float st = b1Streak(x - B1_COMET_V[k] * sec - 5.3f * k, (uint32_t)k, hd);
      const float pl = b1Pulse(x, sec) * pulseK;
      const float v = (0.32f * (1.0f + 1.1f * pl) + 0.85f * st) * B1_STREAK_MAX * pres * E * ds;
      splat(x, y, mix(base, TEAL_HI, hd * 0.85f + 0.3f * pl), v);
      const float side = (k ? -1.0f : 1.0f) * (y > b1Centre(x) ? 1.0f : -1.0f);   // soft spray on the outside
      splat(x, y + side * 0.9f, base, v * 0.12f);
    }
  }
  // ---- droplets carried along inside the eyes ----
  for (int i = 0; i < B1_PARTICLES; i++) {
    const float s = -0.8f + 1.6f * hash01(i, 21);
    const float v = 6.0f + 3.0f * hash01(i, 22);
    const float px = fmodf(44.0f * hash01(i, 23) + v * sec, 44.0f) - 6.0f;
    if (px < -1.5f || px > 33.5f) continue;
    const float pp = clamp01((head - 1.0f - px) / 2.0f);
    if (pp <= 0.0f) continue;
    const float sx = clampf(px, 0.0f, 31.0f);
    const float wx = b1Open(sx, head, settle);
    const float eye = 0.5f * absf(b1StrandY(0, sx, sec, wx) - b1StrandY(1, sx, sec, wx));
    const float py = b1Centre(sx) + s * eye + 0.25f * sinf(6.2831853f * (0.55f * sec + hash01(i, 24)));
    const float tw = 0.6f + 0.4f * sinf(6.2831853f * (0.8f * sec + hash01(i, 25)));
    const float a = B1_PART_MAX * tw * pp * E * (0.35f + 0.65f * clamp01(eye / 1.2f));   // dimmer where the strands cross
    splat(px, py, TEAL_HI, a);
    for (int j = 1; j <= 3; j++) splat(px - 0.55f * j, py, TEAL, a * 0.45f * (1.0f - j / 4.0f));
  }
  // ---- the head: Box 1's last dot; the twin streams unwind from it ----
  if (head > -3.0f && head < 35.0f) {
    const float hy = b1Centre(clampf(head, 0.0f, 31.0f));
    splat(head, hy, TEAL_HI, B1_HEAD_MAX);
    for (int j = 1; j <= 5; j++) {
      const float d = 0.5f * j, w = 1.0f - d / 3.0f;
      splat(head - d, b1Centre(clampf(head - d, 0.0f, 31.0f)), TEAL, B1_HEAD_MAX * 0.6f * w * w);
    }
    const float bx = head - 0.6f;                       // bow: soft vertical glow at the front of the stream
    const int x0 = (int)floorf(bx);
    const float a = bx - x0;
    for (int y = 0; y < 8; y++) {
      const float d = (y - hy) / 1.1f, v = 0.2f * B1_HEAD_MAX * expf(-d * d);
      add(x0, y, TEAL, v * (1.0f - a)); add(x0 + 1, y, TEAL, v * a);
    }
  }
  // ---- entry ripples: two rings spread from where the dot comes in ----
  for (int r = 0; r < 2; r++) {
    const float age = t - (0.12f + 0.3f * r);
    if (age < 0.0f || age > 1.0f) continue;
    const float R = 0.8f + 8.0f * age, fade = (1.0f - age) * (1.0f - age) * (r ? 0.6f : 1.0f);
    for (int x = 0; x <= 12; x++)
      for (int y = 0; y < 8; y++) {
        const float dx = x + 0.8f, dy = y - B1_IN_Y, d = (sqrtf(dx * dx + dy * dy) - R) / 0.6f;
        const float v = expf(-d * d) * fade;
        if (v > 0.01f) add(x, y, TEAL_HI, v * B1_RING_MAX);
      }
  }
  // ---- exit flare: the head pours into Box 2 ----
  const float fa = t - b1HeadExitT();
  if (fa > 0.0f && fa < 1.3f) {
    const float f = expf(-fa / 0.33f) * clamp01(fa / 0.1f);
    for (int y = 0; y < 8; y++) {
      const float d = (y - B1_OUT_Y) / 1.6f, v = f * expf(-d * d) * B1_RING_MAX;
      add(31, y, TEAL_HI, v); add(30, y, TEAL, v * 0.55f); add(29, y, TEAL, v * 0.22f);
    }
  }
}

// ===================== BRIDGE 2: signal line + counter-flows =====================
// flows: 2 above (rows 0, 1) and 2 below (rows 6, 7) the line, straight like Chan's sketch, then bending
// towards Box 3's entry at the right end (the lower two merge into the line as it steps down)
static const int B2_FLOWS = 4;
static const float B2_FLOW_Y0[B2_FLOWS] = { 0.0f, 1.0f, 6.0f, 7.0f };   // rows in the straight part
static const float B2_FLOW_Y1[B2_FLOWS] = { 3.4f, 4.8f, 6.8f, 7.0f };   // rows at x = 31 (converging)
static const float B2_FLOW_V[B2_FLOWS]  = { 6.0f, 7.4f, 7.4f, 6.0f };   // inner flows faster
static const float B2_FLOW_LAG[B2_FLOWS] = { 4.0f, 2.0f, 2.0f, 4.0f };  // they fan out behind the head
static const int B2_BEADS = 6;

static inline float b2Head(float t) { return B2_HEAD_X0 + B2_HEAD_V * t; }
static inline float b2Base(float x) { return B2_IN_Y + (B2_OUT_Y - B2_IN_Y) * smooth((x - 21.0f) / 9.5f); }   // 3.5 → 6.8
static inline float b2Digital(float x) { return smooth((x - 6.0f) / 10.0f); }   // 0 = biological sine, 1 = square wave
static inline float b2Phase(float x) {   // wave cycles up to x: wavelength 7 LED (sine) → 5 LED (square)
  const float dk = 1.0f / 5.0f - 1.0f / 7.0f;
  float M;
  if (x <= 6.0f) M = 0.0f;
  else if (x <= 16.0f) { const float u = (x - 6.0f) / 10.0f; M = 10.0f * (u * u * u - 0.5f * u * u * u * u); }
  else M = 5.0f + (x - 16.0f);
  return x / 7.0f + dk * M;
}
static inline float b2Y(float x, float sec) {   // the signal line
  const float m = b2Digital(x);
  const float amp = (1.0f + 0.5f * m) * (0.2f + 0.8f * smooth((x + 1.0f) / 6.0f)) * (1.0f - smooth((x - 23.5f) / 5.5f));
  const float s = sinf(6.2831853f * (b2Phase(x) - B2_WAVE_V / 7.0f * sec));
  const float sq = tanhf(3.2f * s) / 0.99668f;     // square wave: levels on rows 2 and 5 (edges ≈ 0.8 LED wide)
  return b2Base(x) + amp * (s + (sq - s) * m);
}
static inline C3 b2LineCol(float x) { return mix3(PURPLE, BLUE_MID, TEAL, (x - 1.0f) / 28.0f); }   // purple → blue → teal
static inline C3 b2HiCol(float x) { return mix(LILAC, TEAL_HI, smooth((x - 4.0f) / 22.0f)); }
static inline C3 b2FlowCol(float x) { return mix(TEAL, PURPLE, smooth((x - 3.0f) / 24.0f)); }      // teal → purple (opposite)
static inline float b2FlowY(int k, float x) { return B2_FLOW_Y0[k] + (B2_FLOW_Y1[k] - B2_FLOW_Y0[k]) * smooth((x - 22.0f) / 9.0f); }
static inline float b2FlowFade(int k, float x) { return k >= 2 ? 1.0f - smooth((x - 22.0f) / 6.0f) : 1.0f; }
// dashes in a flow: irregular streaks (m = 0) → regular clock-like dashes (m = 1)
static float b2Dash(float u, uint32_t lane, float m) {
  const float SLOT = 5.0f;
  const float n = floorf(u / SLOT), p = u - n * SLOT;
  const uint32_t id = slotId(n);
  const float len = (0.9f + 2.3f * hash01(id, lane * 5 + 1)) * (1.0f - m) + 2.0f * m;
  const float st = 0.6f + (SLOT - len - 1.2f) * hash01(id, lane * 5 + 2) * (1.0f - m);
  const float q = p - st;
  if (q < -0.6f || q > len + 0.6f) return 0.0f;
  const float edge = clamp01((q + 0.6f) / 0.6f) * clamp01((len + 0.6f - q) / 0.6f);
  const float shape = (1.0f - m) * (0.3f + 0.7f * clamp01(q / len)) + m * 0.9f;
  const float on = (hash01(id, lane * 5 + 3) < 0.22f) ? m : 1.0f;
  const float b = (0.55f + 0.45f * hash01(id, lane * 5 + 4)) * (1.0f - m) + m;
  return edge * shape * on * b;
}
// data bits riding the line: short / long, with gaps
static float b2Bit(float u) {
  const float SLOT = 4.5f;
  const float n = floorf(u / SLOT), p = u - n * SLOT;
  const uint32_t id = slotId(n);
  if (hash01(id, 91) < 0.3f) return 0.0f;
  const float len = hash01(id, 92) < 0.5f ? 0.8f : 2.0f;
  const float q = p - 0.8f;
  if (q < -0.5f || q > len + 0.5f) return 0.0f;
  return clamp01((q + 0.5f) / 0.5f) * clamp01((len + 0.5f - q) / 0.5f);
}

static void bridge2(float t, float sec) {
  const float head = b2Head(t);
  const float settle = smooth(ramp(t, B2_SETTLE_T, STORY_S - B2_SETTLE_T));
  const float E = 1.0f - (1.0f - B2_REST) * settle;
  // ---- 2 + 2 flows: a faint continuous line + dashes flowing along it, fanning out behind the head ----
  for (int k = 0; k < B2_FLOWS; k++) {
    const float kb = (k == 1 || k == 2) ? 1.0f : 0.75f;
    for (int x = 0; x < 32; x++) {
      float pres = clamp01((head - B2_FLOW_LAG[k] - x) / 2.5f);
      if (settle > pres) pres = settle;                     // complete in the retained scene
      if (pres <= 0.0f) continue;
      const float fade = b2FlowFade(k, (float)x);
      if (fade <= 0.0f) continue;
      const float d = b2Dash(x - B2_FLOW_V[k] * sec - 7.7f * k, (uint32_t)k, b2Digital((float)x));
      const float dens = 0.65f + 0.6f * smooth((x - 12.0f) / 16.0f);    // brighter (and purple) towards Electronics
      const C3 c = mix(b2FlowCol((float)x), b2HiCol((float)x), 0.25f * d);
      addRow(x, b2FlowY(k, (float)x), c, (0.2f + 0.8f * d) * kb * dens * fade * pres * B2_FLOW_MAX * E);
    }
  }
  // ---- beads: brighter drops overtaking along the flows ----
  for (int i = 0; i < B2_BEADS; i++) {
    const int k = i % B2_FLOWS;
    const float v = B2_FLOW_V[k] + 1.2f + 0.8f * hash01(i, 42);
    const float px = fmodf(40.0f * hash01(i, 43) + v * sec, 40.0f) - 4.0f;
    if (px < -1.0f || px > 32.5f) continue;
    float pp = clamp01((head - B2_FLOW_LAG[k] - px) / 2.5f);
    if (settle > pp) pp = settle;
    const float sx = clampf(px, 0.0f, 31.0f);
    const float a = B2_SPARK_MAX * pp * b2FlowFade(k, sx) * E;
    if (a <= 0.0f) continue;
    const float py = b2FlowY(k, sx);
    splat(px, py, mix(b2FlowCol(sx), b2HiCol(sx), 0.4f), a);
    for (int j = 1; j <= 3; j++) splat(px - 0.6f * j, py, b2FlowCol(sx), a * 0.45f * (1.0f - j / 4.0f));
  }
  // ---- the signal line (drawn up to the head) with its data bits ----
  const float STEP = 0.125f;
  const float end = head < 31.6f ? head : 31.6f;
  float py = b2Y(-0.6f, sec);
  for (float x = -0.6f + STEP; x <= end; x += STEP) {
    const float y = b2Y(x, sec);
    const float ds = sqrtf(STEP * STEP + (y - py) * (y - py));   // equal brightness per length (also on the steps)
    py = y;
    const float glow = 0.6f + 0.4f * expf(-(head - x) / 3.0f);
    const float bit = b2Bit(7.0f * b2Phase(x) - B2_WAVE_V * sec - 1.7f) * clamp01((head - 1.5f - x) / 1.5f);   // bits ride the wave
    const float lift = 1.0f + 0.3f * (1.0f - smooth((x - 1.0f) / 20.0f));     // purple reads darker than teal
    const float v = (glow * B2_LINE_MAX + bit * B2_BIT_MAX) * E * ds * lift;
    splat(x, y, mix(b2LineCol(x), b2HiCol(x), 0.55f * bit), v);
  }
  // ---- the head ----
  if (head > -2.0f && head < 34.0f) {
    const float hy = (b2Y(head, sec) + b2Y(head - 0.35f, sec) + b2Y(head - 0.7f, sec)) / 3.0f;   // eases over the steps
    splat(head, hy, mix(b2LineCol(head), b2HiCol(head), 0.6f), B2_HEAD_MAX);
    for (int j = 1; j <= 4; j++) {
      const float d = 0.45f * j;
      splat(head - d, b2Y(head - d, sec), b2LineCol(head - d), B2_HEAD_MAX * 0.6f * (1.0f - j / 5.0f));
    }
    const float bx = head - 0.4f;                       // small vertical glow around the head
    const int x0 = (int)floorf(bx);
    const float a = bx - x0;
    for (int y = 0; y < 8; y++) {
      const float d = (y - hy) / 0.9f, v = 0.3f * B2_HEAD_MAX * expf(-d * d);
      add(x0, y, b2LineCol(head), v * (1.0f - a)); add(x0 + 1, y, b2LineCol(head), v * a);
    }
  }
}

}  // namespace D2DBridge

// progress >= 1 → the story at its end (t = 5 s): the retained scene, still moving with `seconds`
void renderBridge1(double progress, double seconds, double level) {
  using namespace D2DBridge;
  clear();
  bridge1(storyTime(progress), safeSeconds(seconds));
  flush(BRIDGE[0], level);
}

void renderBridge2(double progress, double seconds, double level) {
  using namespace D2DBridge;
  clear();
  bridge2(storyTime(progress), safeSeconds(seconds));
  flush(BRIDGE[1], level);
}
