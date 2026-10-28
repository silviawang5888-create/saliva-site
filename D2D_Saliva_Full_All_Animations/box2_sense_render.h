// =====================================================================================
//  D2D Box 2 · Sense (Biosensor) — under-light + backlight render functions
//  Generated but not physically tested.
//
//  Ported from the approved browser simulation (d2d_led_sim.html, "Box 2 · Sense (version B)",
//  27 Oct 2026). Follows the D2D Four-Box LED Animation Coding Standard (21 Oct 2026) and the
//  Box 2 Sense handover (22 Oct 2026).
//
//  MERGING INTO THE 11-PANEL PROGRAM
//    #include this file after the shared definitions of BOTTOM[], REAR[], put() and dot().
//    It only calls dot() (saturating add of color × brightness) on BOTTOM[1] and REAR[1],
//    uses logical coordinates, never calls FastLED.show() or delay(), and keeps no frame state:
//    every frame is drawn from progress / seconds / level alone (seeded tables are built once).
//
//  Run length 28 s:  Phase I 0–7 s · Phase II 7–14 s · Phase III 14–28 s (2 s crossfades at 7 and 14).
// =====================================================================================
#pragma once
#include <FastLED.h>
#include <math.h>

// ---------------- Options (set before #include to change) ----------------
#define BOX2_BG1_TWINKLES    0      // Phase I–II backlight: rising twinkles
#define BOX2_BG1_SHIMMER     1      //                       caustic shimmer
#define BOX2_P2_SLOW_REVEAL  0      // Phase II under-light: all six protein LEDs fade up together
#define BOX2_P2_LOCK_ON      1      //                       protein breathes deeper, bubble glints close in
#define BOX2_P2_SCAN_REVEAL  2      //                       soft line sweeps front → back through the sheets
#define BOX2_P3_PROPAGATE    0      // Phase III under-light: soft pulses orange → white → purple (hand-off)
#define BOX2_P3_STREAMING    1      //                        teal glow stays, a teal pulse every 2 s
#ifndef BOX2_BG1
#define BOX2_BG1 BOX2_BG1_TWINKLES
#endif
#ifndef BOX2_P2
#define BOX2_P2 BOX2_P2_SLOW_REVEAL
#endif
#ifndef BOX2_P3
#define BOX2_P3 BOX2_P3_PROPAGATE
#endif
#ifndef BOX2_BG_BUBBLES
#define BOX2_BG_BUBBLES 1           // 1 = background bubbles sheet on, 0 = off
#endif

namespace box2 {

// ---------------- Colours ----------------
const CRGB TEAL   = CRGB(86, 209, 194);   // Box 2 Sense (standard, main colour)
const CRGB PURPLE = CRGB(109, 51, 189);   // Box 3 Connect (standard, Phase III hand-off)
const CRGB ORANGE = CRGB(255, 85, 0);     // Box 1 Pattern (standard, arrival from Bridge 1)
const CRGB BLUE   = CRGB(47, 107, 216);   // blue end of the blue → teal range (not a standard colour)
const CRGB WHITE_ = CRGB(255, 255, 255);  // accents only

// ---------------- Sheets: one etched acrylic sheet per under-light row (front sheet first) ----------------
const int ROW_FRONT = 15, ROW_SECOND = 14, ROW_THIRD = 13;        // bubble sheets in front of the protein
const int ROW_PROTEIN = 12, ROW_SENSOR = 11, ROW_BACKGROUND = 10;  // protein, sensor, background bubbles
const int FRONT_ROW = 15, NUM_SHEETS = 6;

// ---------------- Etch positions across each sheet (columns, measured from the SVGs) ----------------
const int   PROTEIN_X0 = 5, PROTEIN_X1 = 10;   // protein ring
const float PROTEIN_CX = 7.5f;
const int   CIRCLE_X0 = 6, CIRCLE_X1 = 9;      // sensor circle (column 9 shared with the wave)
const int   WAVE_X0 = 9, WAVE_X1 = 15;         // sine tail to the right edge
const int   GLINT_ROW = ROW_THIRD;             // II-B bubble glints

// ---------------- Timing (seconds into the 28 s run) ----------------
const float RUN = 28.0f, T_I_END = 7.0f, T_II_END = 14.0f, XF = 2.0f;
const float PROTEIN_ON = 7.4f, PROTEIN_FADE = 4.0f, PROTEIN_GAIN = 1.35f;
const float PULSE_A[3] = { 0.20f, 0.35f, 0.55f }, PULSE_W = 0.9f;   // Phase III protein pulses: soft, building
const float HEAD_T = 3.3f, HEAD_SPEED = 2.5f;                        // head leaves the circle 3.3 s into Phase III
const float HEAD_ARRIVE = HEAD_T + (WAVE_X1 - WAVE_X0) / HEAD_SPEED; // reaches column 15
const float CENTRE_OFF  = (WAVE_X0 - 7.5f) / HEAD_SPEED;             // a signal crosses the panel centre this long before it leaves the circle
const float STREAM_LAST = (RUN - T_II_END) - (WAVE_X1 - WAVE_X0 + 3) / HEAD_SPEED;  // last stream that can finish by 28 s
const float PULSE_T[3]  = { HEAD_T - CENTRE_OFF - 1.8f, HEAD_T - CENTRE_OFF - 0.8f, HEAD_T - CENTRE_OFF };
const float STREAM_FIRST = 6.3f, STREAM_EVERY = 2.0f;

// Box 1 arrival (Phase I): orange trail left → centre, 2×2 ball, one pulse with the sensor circle
const float ARRIVE_T0 = 0.3f, ARRIVE_SPEED = 3.0f, ARRIVE_X = 7.5f;
const float ARRIVE_AT = ARRIVE_T0 + (ARRIVE_X + 1) / ARRIVE_SPEED;  // ≈ 3.1 s
const float ARRIVE_PULSE = 1.2f, ARRIVE_TRAIL = 6.0f, ARRIVE_TRAIL_B = 0.18f, ARRIVE_FADE = 4.0f;
const float BACK_RISE = 4.0f;                                       // Phase I backlight reveal: rows/s from the bottom

struct Weighted { int8_t i; float w; };
const Weighted STREAM_ROWS[6]    = { {5, 0.04f}, {6, 0.2f}, {7, 1.0f}, {8, 1.0f}, {9, 0.2f}, {10, 0.04f} };
const Weighted GLOW_ROWS[6]      = { {5, 0.15f}, {6, 0.5f}, {7, 1.0f}, {8, 1.0f}, {9, 0.5f}, {10, 0.15f} };
const Weighted BACK_GLOW_COLS[6] = { {5, 0.15f}, {6, 0.5f}, {7, 1.0f}, {8, 1.0f}, {9, 0.5f}, {10, 0.15f} };
const Weighted BALL_ROWS[4]      = { {6, 0.3f}, {7, 1.0f}, {8, 1.0f}, {9, 0.3f} };
const Weighted BALL_COLS[4]      = { {6, 0.3f}, {7, 1.0f}, {8, 1.0f}, {9, 0.3f} };
const int   ARRIVE_UNDER_COLS[3] = { 6, 7, 8 };
const int   STREAM_START_COL = 7;
const float STREAM_TAIL = 4.5f;

// Drifting lights on the three front bubble sheets
const int   TRACKER_COUNT = 8;
const float TRACKER_LIFE_MIN = 4.0f, TRACKER_LIFE_MAX = 7.0f, TRACKER_BRIGHT = 0.85f;
const int   TRACKER_FRONT = ROW_FRONT, TRACKER_BACK = ROW_THIRD;

const uint32_t SEED_SHARED = 7, SEED_BACKGROUND = 11, SEED_TRACKERS = 41;
const float TWO_PI_F = 6.28318530718f;

// ---------------- Helpers ----------------
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float smooth(float a, float b, float x) { float t = clamp01((x - a) / (b - a)); return t * t * (3 - 2 * t); }
inline float env(float s, float t0, float dur) {
  float a = (s - t0) / dur; if (a < 0 || a > 1) return 0;
  return a < 0.15f ? a / 0.15f : 1 - (a - 0.15f) / 0.85f;
}
inline float softPeak(float v, float peak, float w) {
  return fabsf(v - peak) < w / 2 ? powf(cosf(PI * (v - peak) / w), 2) : 0;
}
inline uint8_t ch(float v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : floorf(v + 0.5f))); }
inline CRGB mix(const CRGB &a, const CRGB &b, float t) {
  return CRGB(ch(a.r + (b.r - a.r) * t), ch(a.g + (b.g - a.g) * t), ch(a.b + (b.b - a.b) * t));
}
inline float maxf(float a, float b) { return a > b ? a : b; }
inline float minf(float a, float b) { return a < b ? a : b; }
inline void vdot(uint8_t panel, int x, float yf, const CRGB &c, float b) {  // split between two rows
  int y0 = (int)floorf(yf); float f = yf - y0;
  dot(panel, x, y0, c, b * (1 - f)); dot(panel, x, y0 + 1, c, b * f);
}
// mulberry32 — the same generator the simulation used, so layouts match it exactly
struct M32 {
  uint32_t a;
  float next() {
    a += 0x6D2B79F5u; uint32_t t = a;
    t = (t ^ (t >> 15)) * (t | 1u);
    t = (t + ((t ^ (t >> 7)) * (t | 61u))) ^ t;
    return (float)((double)(t ^ (t >> 14)) / 4294967296.0);
  }
};
inline float hash01(int a, int b) { M32 h{ (uint32_t)(a * 7919 + b * 104729 + 17) }; h.next(); return h.next(); }

// ---------------- Seeded tables (built once) ----------------
struct BubbleCol { float period, ph, hue; };
struct Twinkle   { int x; float v, off, ph; };
struct Tracker   { float life, off, fx0, fx1, px0, px1, fy, py, hue; };
static BubbleCol bgCols[16];
static Twinkle   twinkles[16];
static Tracker   trackers[TRACKER_COUNT];
static bool      ready = false;

inline void init() {
  if (ready) return;
  M32 r{ SEED_SHARED };
  for (int i = 0; i < 16 * 5; i++) r.next();             // shared columns (unused here; keeps the twinkle layout identical to the sim)
  for (int i = 0; i < 16; i++) {
    twinkles[i].x = (int)floorf(r.next() * 16); twinkles[i].v = 0.9f + 0.9f * r.next();
    twinkles[i].off = r.next() * 19; twinkles[i].ph = r.next() * 6.283f;
  }
  M32 bg{ SEED_BACKGROUND };
  for (int i = 0; i < 16; i++) { bgCols[i].period = 2 + 2 * bg.next(); bgCols[i].ph = bg.next() * 6.283f; bgCols[i].hue = bg.next(); }
  M32 tr{ SEED_TRACKERS };
  for (int i = 0; i < TRACKER_COUNT; i++) {
    Tracker &p = trackers[i];
    p.life = TRACKER_LIFE_MIN + (TRACKER_LIFE_MAX - TRACKER_LIFE_MIN) * tr.next(); p.off = tr.next() * 10;
    p.fx0 = 0.35f + 0.5f * tr.next(); p.fx1 = 0.9f + 0.8f * tr.next();
    p.px0 = tr.next() * 6.283f; p.px1 = tr.next() * 6.283f;
    p.fy = 0.5f + 0.6f * tr.next(); p.py = tr.next() * 6.283f; p.hue = tr.next();
  }
  ready = true;
}

// ---------------- Phase weights ----------------
struct Weights { float I, II, III, amb, quiet; };
inline Weights weights(float s) {
  float inII = smooth(T_I_END - XF / 2, T_I_END + XF / 2, s), inIII = smooth(T_II_END - XF / 2, T_II_END + XF / 2, s);
  return { 1 - inII, inII * (1 - inIII), inIII, 1 - 0.7f * inII - 0.1f * inIII, inII };
}
// Phase III extra pulses (both options): launch times still relevant at v, none that could not finish by 28 s
inline int streams(float v, float *out) {
  int n = 0;
  float st = STREAM_FIRST + STREAM_EVERY * maxf(0, floorf((v - 5 - STREAM_FIRST) / STREAM_EVERY));
  for (; st <= minf(v + CENTRE_OFF + 0.6f, STREAM_LAST) && n < 8; st += STREAM_EVERY) out[n++] = st;
  return n;
}
inline CRGB handoffColor(float u) {   // under-light wave, III-A: soft orange → white → purple
  const CRGB SOFT_ORANGE = mix(ORANGE, WHITE_, 0.35f);
  return u < 0.5f ? mix(SOFT_ORANGE, WHITE_, smooth(0, 0.5f, u)) : mix(WHITE_, PURPLE, smooth(0.65f, 1, u));
}
inline CRGB backGradient(float u) {   // backlight signal, III-A: teal → white → purple, fixed per column
  return u < 0.5f ? mix(TEAL, WHITE_, smooth(0, 0.5f, u)) : mix(WHITE_, PURPLE, smooth(0.65f, 1, u));
}

// ---------------- Under-light ----------------
inline void drawTrackers(uint8_t P, float t, float k) {   // lights drift front → back with a fuzzy wander
  const float depth = TRACKER_FRONT - TRACKER_BACK + 0.6f;
  for (int i = 0; i < TRACKER_COUNT; i++) {
    const Tracker &p = trackers[i];
    float tt = t + p.off; int cycle = (int)floorf(tt / p.life); float age = (tt - cycle * p.life) / p.life;
    float x0 = 0.5f + 14 * hash01(i, cycle);              // each life starts at a random column, at the front
    float x = x0 + 0.9f * sinf(p.fx0 * t + p.px0) + 0.5f * sinf(p.fx1 * t + p.px1);
    float y = TRACKER_FRONT + 0.3f - depth * age + 0.3f * sinf(TWO_PI_F * p.fy * t + p.py);
    float b = k * TRACKER_BRIGHT * smooth(0, 0.15f, age) * (1 - smooth(0.7f, 1, age));
    if (b <= 0.005f) continue;
    CRGB c = mix(BLUE, TEAL, 0.3f + 0.7f * p.hue);
    int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi;
    for (int dx = 0; dx <= 1; dx++) for (int dy = 0; dy <= 1; dy++) {
      int yy = yi + dy; float w = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
      if (yy >= TRACKER_BACK && yy <= TRACKER_FRONT) dot(P, xi + dx, yy, c, b * w);
    }
  }
}
inline void ambient(uint8_t P, float s, float t, float k) {   // Phase I ambient; 30% in II, 20% in III
  if (k <= 0) return;
  float reveal = smooth(0, 2.5f, s);
  drawTrackers(P, t, k * reveal);
  if (BOX2_BG_BUBBLES) for (int x = 0; x < 16; x++) {    // background bubbles breathe on 2–4 s cycles
    const BubbleCol &c = bgCols[x];
    float br = clamp01((0.5f - 0.5f * cosf(TWO_PI_F * t / c.period + c.ph) - 0.25f) / 0.75f);
    if (br > 0) dot(P, x, ROW_BACKGROUND, mix(BLUE, TEAL, c.hue), k * reveal * 0.55f * br);
  }
}
inline void phaseII(uint8_t P, float s, float t, float k) {
  (void)t;
  float u = s - T_I_END, sync = 0.85f + 0.15f * sinf(TWO_PI_F * u / 3), shape = (sync - 0.7f) / 0.3f;
  if (BOX2_P2 == BOX2_P2_SLOW_REVEAL) {
    float a = smooth(PROTEIN_ON, PROTEIN_ON + PROTEIN_FADE, s);
    for (int x = PROTEIN_X0; x <= PROTEIN_X1; x++) dot(P, x, ROW_PROTEIN, mix(BLUE, TEAL, a), k * PROTEIN_GAIN * a * 0.6f * sync);
  } else if (BOX2_P2 == BOX2_P2_LOCK_ON) {
    float depth = 0.08f + 0.45f * clamp01(u / (T_II_END - T_I_END - 1)), breath = 0.5f - 0.5f * cosf(TWO_PI_F * u / 3.2f);
    for (int x = PROTEIN_X0; x <= PROTEIN_X1; x++) dot(P, x, ROW_PROTEIN, TEAL, k * PROTEIN_GAIN * (0.12f + depth * breath));
    shape = breath;
    float R = 7 - 3.8f * smooth(T_I_END, T_II_END - 1, s);
    for (int side = -1; side <= 1; side += 2) {
      float xf = PROTEIN_CX + side * R; int x0 = (int)floorf(xf); float f = xf - x0;
      dot(P, x0, GLINT_ROW, TEAL, k * 0.3f * (1 - f)); dot(P, x0 + 1, GLINT_ROW, TEAL, k * 0.3f * f);
    }
  } else {  // scan reveal
    float yS = FRONT_ROW + 0.5f - (s - (T_I_END + 0.3f)) / 1.1f;
    for (int y = FRONT_ROW - NUM_SHEETS + 1; y <= FRONT_ROW; y++) {
      float b = maxf(0, 1 - fabsf(y - yS) / 0.9f);
      if (b > 0) for (int x = 0; x < 16; x++) dot(P, x, y, mix(TEAL, WHITE_, 0.35f), k * 0.22f * b);
    }
    float rec = ROW_PROTEIN - yS;
    if (rec > -0.5f) {
      float fresh = maxf(0, 1 - rec / 2), a = smooth(-0.5f, 2.5f, rec);
      for (int x = PROTEIN_X0; x <= PROTEIN_X1; x++) dot(P, x, ROW_PROTEIN, mix(TEAL, WHITE_, 0.3f * fresh), k * PROTEIN_GAIN * 0.5f * sync * a);
    }
  }
  float circ = smooth(T_I_END + 3.5f, T_I_END + 5.5f, s);   // sensor circle fades in, pulsing in sync
  if (circ > 0) for (int x = CIRCLE_X0; x <= CIRCLE_X1; x++) dot(P, x, ROW_SENSOR, mix(ORANGE, WHITE_, 0.35f), k * circ * 0.4f * (0.5f + 0.5f * shape));
}
inline float pulseLevel(float v, const float *st, int n) {
  float p = 0;
  for (int i = 0; i < 3; i++) p = maxf(p, PULSE_A[i] * softPeak(v, PULSE_T[i], PULSE_W));
  for (int i = 0; i < n; i++) p = maxf(p, 0.3f * softPeak(v, st[i] - CENTRE_OFF, 1.0f));
  return p;
}
inline void phaseIII(uint8_t P, float s, float t, float k) {
  (void)s;
  const bool purple = (BOX2_P3 == BOX2_P3_PROPAGATE);
  float v = t - T_II_END, breath = sinf(TWO_PI_F * v / 3.5f), st[8]; int n = streams(v, st);
  float pulse = pulseLevel(v, st, n), settled = smooth(HEAD_ARRIVE, HEAD_ARRIVE + 2, v);
  float hold = purple ? 1 - smooth(HEAD_T, HEAD_T + 1.5f, v) : 1;
  for (int x = PROTEIN_X0; x <= PROTEIN_X1; x++)
    dot(P, x, ROW_PROTEIN, mix(TEAL, WHITE_, 0.25f * minf(1, 2 * pulse)), k * minf(1, PROTEIN_GAIN * (0.42f + 0.08f * settled * breath + pulse)));
  const CRGB SOFT_ORANGE = mix(ORANGE, WHITE_, 0.35f);
  for (int x = CIRCLE_X0; x <= CIRCLE_X1 - 1; x++)
    dot(P, x, ROW_SENSOR, SOFT_ORANGE, k * minf(1, hold * (0.4f + 0.06f * settled * breath) + 1.1f * pulse));
  float hx = WAVE_X0 + (v - HEAD_T) * HEAD_SPEED, glow = purple ? 0 : 0.3f + 0.08f * breath;
  for (int x = WAVE_X0; x <= WAVE_X1; x++) {   // head runs the wave left → right, then soft pulses follow
    float d = hx - x, b = (x == WAVE_X0) ? hold * 0.4f + 1.1f * pulse : 0;
    if (d >= 0) b = maxf(b, maxf(glow * smooth(0, 1, d), 1 - d / 2.5f));
    else if (d > -1) b = maxf(b, (1 + d) * 0.5f);
    for (int i = 0; i < n; i++) {
      float sd = WAVE_X0 + (v - st[i]) * HEAD_SPEED - x;
      if (sd > -1 && sd < 3) b = maxf(b, 0.6f * (sd < 0 ? 0.6f * smooth(-1, 0, sd) : 1 - smooth(0, 3, sd)));
    }
    CRGB c = purple ? handoffColor((float)(x - WAVE_X0) / (WAVE_X1 - WAVE_X0)) : TEAL;
    if (d >= 0 && d < 1) c = mix(c, WHITE_, 0.2f * (1 - d));
    if (purple && x == WAVE_X1) {               // brief near-white flash at the tip
      float fl = env(v, HEAD_ARRIVE, 0.6f);
      if (fl > 0) { c = mix(c, WHITE_, 0.8f * fl); b = maxf(b, fl); }
    }
    dot(P, x, ROW_SENSOR, c, k * minf(1, b));
  }
}
inline float arrivalPulse(float s) { float a = (s - ARRIVE_AT) / ARRIVE_PULSE; return (a > 0 && a < 1) ? powf(sinf(PI * a), 2) : 0; }
inline void arrivalUnder(uint8_t P, float s, float level) {
  float p = arrivalPulse(s);
  if (p > 0.005f) for (int i = 0; i < 3; i++) dot(P, ARRIVE_UNDER_COLS[i], ROW_SENSOR, ORANGE, level * 0.4f * p);
}

// ---------------- Backlight ----------------
inline float tau(float t) { return t < T_I_END - 1 ? t : (T_I_END - 1) + (t - (T_I_END - 1)) * 0.6f; }
inline void ambientRear(uint8_t P, float s, float t, float k, float quiet) {
  if (k <= 0) return;
  float rise = 15.5f - s * BACK_RISE;   // soft front rising from the bottom
  if (BOX2_BG1 == BOX2_BG1_TWINKLES) {
    for (int i = 0; i < 16; i++) {
      const Twinkle &p = twinkles[i];
      float vis = i < 8 ? 1 : 1 - quiet;   // half of them leave in Phase II
      if (vis <= 0) continue;
      float pos = fmodf(p.off + p.v * tau(t), 19), yf = 15 - pos;
      float b = k * vis * 0.3f * (0.65f + 0.35f * sinf(1.3f * t + p.ph)) * smooth(0, 1.5f, pos) * smooth(rise - 1, rise + 2.5f, yf);
      vdot(P, p.x, yf, TEAL, b); vdot(P, p.x, yf + 1, TEAL, b * 0.35f);
    }
  } else {
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      float yy = y + 0.6f * tau(t);
      float n = (sinf(0.8f * x + 0.5f * yy) + sinf(0.55f * x - 0.9f * yy + 1.7f) + sinf(0.3f * x + 1.3f * yy + 4.1f + 0.2f * t)) / 3;
      float v = clamp01((n - 0.3f) / 0.7f);
      if (v > 0) dot(P, x, y, TEAL, k * 0.22f * v * (1 - 0.3f * quiet) * smooth(rise - 1, rise + 2.5f, (float)y));
    }
  }
}
inline void arrivalRear(uint8_t P, float s, float level) {
  if (s < ARRIVE_T0 || s > ARRIVE_AT + 0.8f + ARRIVE_FADE) return;
  float head = minf(ARRIVE_X, -1 + (s - ARRIVE_T0) * ARRIVE_SPEED), tailFade = 1 - smooth(ARRIVE_AT, ARRIVE_AT + 1.0f, s);
  if (tailFade > 0) for (int x = 0; x <= 6; x++) {   // trail stays left of the ball
    float d = head - x, b = d < 0 ? smooth(-1, 0, d) * 0.6f : 1 - smooth(0, ARRIVE_TRAIL, d);
    if (b <= 0.01f) continue;
    for (int i = 0; i < 4; i++) dot(P, x, BALL_ROWS[i].i, ORANGE, level * ARRIVE_TRAIL_B * b * BALL_ROWS[i].w * tailFade);
  }
  float ball = 0.25f * smooth(ARRIVE_AT - 0.3f, ARRIVE_AT, s) * (1 - smooth(ARRIVE_AT + 0.8f, ARRIVE_AT + 0.8f + ARRIVE_FADE, s))
             + 0.35f * arrivalPulse(s);
  if (ball > 0.005f) for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
    dot(P, BALL_COLS[i].i, BALL_ROWS[j].i, ORANGE, level * ball * BALL_COLS[i].w * BALL_ROWS[j].w);
}
inline void streamPulse(uint8_t P, float head, float b0, bool purple) {   // signal centre → right, locked to the under-light head
  if (head - STREAM_TAIL - 1 > 15) return;
  float fadeIn = smooth(STREAM_START_COL - 1, STREAM_START_COL + 0.5f, head);
  for (int x = STREAM_START_COL; x <= 15; x++) {
    float d = head - x, b = d < 0 ? smooth(-1.6f, 0, d) * 0.7f : 1 - smooth(0, STREAM_TAIL, d);
    if (b <= 0.01f) continue;
    CRGB base = purple ? backGradient((float)(x - STREAM_START_COL) / (15 - STREAM_START_COL)) : TEAL;
    CRGB c = mix(base, WHITE_, 0.15f * maxf(0, 1 - fabsf(d)));
    for (int i = 0; i < 6; i++) dot(P, x, STREAM_ROWS[i].i, c, b0 * fadeIn * b * STREAM_ROWS[i].w);
  }
}
inline void rearIII(uint8_t P, float s, float t, float k) {
  (void)s;
  const bool purple = (BOX2_P3 == BOX2_P3_PROPAGATE);
  float v = t - T_II_END, st[8]; int n = streams(v, st);
  float p = minf(1, 1.5f * pulseLevel(v, st, n));   // centre glow behind the protein: same envelope, stays teal
  if (p > 0.01f) for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++)
    dot(P, BACK_GLOW_COLS[i].i, GLOW_ROWS[j].i, TEAL, k * p * BACK_GLOW_COLS[i].w * GLOW_ROWS[j].w);
  streamPulse(P, WAVE_X0 + (v - HEAD_T) * HEAD_SPEED, k * 0.85f, purple);
  for (int i = 0; i < n; i++) streamPulse(P, WAVE_X0 + (v - st[i]) * HEAD_SPEED, k * 0.6f, purple);
}

}  // namespace box2

// ---------------- Render functions (standard interface) ----------------
void renderBox2Bottom(double progress, double seconds, double level) {
  box2::init();
  const uint8_t P = BOTTOM[1];
  float s = (float)(progress < 0 ? 0 : (progress > 1 ? 1 : progress)) * box2::RUN, t = (float)seconds, L = (float)level;
  box2::Weights w = box2::weights(s);
  box2::arrivalUnder(P, s, L);
  box2::ambient(P, s, t, w.amb * L);
  if (w.II > 0)  box2::phaseII(P, s, t, w.II * L);
  if (w.III > 0) box2::phaseIII(P, s, t, w.III * L);
}

void renderBox2Rear(double progress, double seconds, double level) {
  box2::init();
  const uint8_t P = REAR[1];
  float s = (float)(progress < 0 ? 0 : (progress > 1 ? 1 : progress)) * box2::RUN, t = (float)seconds, L = (float)level;
  box2::Weights w = box2::weights(s);
  box2::arrivalRear(P, s, L);
  box2::ambientRear(P, s, t, L * (w.I + 0.45f * w.II), w.quiet);   // ambient backlight is gone by Phase III
  if (w.III > 0) box2::rearIII(P, s, t, w.III * L);
}
