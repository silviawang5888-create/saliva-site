// =====================================================================================
//  D2D Box 4 · Assemble — under-light + backlight render functions
//  Generated but not physically tested.
//
//  Ported from the approved browser simulation (d2d_led_sim.html, "Box 4 · Assemble", 27 Oct 2026).
//  Follows the D2D Four-Box LED Animation Coding Standard (21 Oct 2026).
//
//  MERGING INTO THE 11-PANEL PROGRAM
//    #include this file after the shared definitions of BOTTOM[], REAR[], put() and dot().
//    It only calls dot() (saturating add of color × brightness) on BOTTOM[3] and REAR[3],
//    uses logical coordinates, never calls FastLED.show() or delay(), and keeps no frame state.
//
//  Run length 28 s. Ten physical sheets, numbered front to back:
//  L4 front hand (y12), L6 enclosure (y10), L10 back hand (y6).
// =====================================================================================
#pragma once
#include <FastLED.h>
#include <math.h>

namespace box4 {

// ---------------- Colours ----------------
const CRGB PINK   = CRGB(255, 0, 95);     // Box 4 Assemble (standard, main colour)
const CRGB PURPLE = CRGB(109, 51, 189);   // Box 3 Connect (standard, arriving signal)
const CRGB WHITE_ = CRGB(255, 255, 255);  // hands

// ---------------- Sheets: L1 is the front under-light row y15 ----------------
// Empty sheets L1-3, L5, and L7-9 remain dark in this animation.
const int ROW_HAND_FRONT = 12, ROW_ENCLOSURE = 10, ROW_HAND_BACK = 6;

// ---------------- Positions (measured from assembly.svg) ----------------
const int   ENCL_X0 = 3, ENCL_X1 = 15;                               // enclosure etching columns
const int   HAND_BACK_X0 = 0, HAND_BACK_X1 = 13;  const float HAND_BACK_CX = 7.3f;   // grows centre → edges
const int   HAND_FRONT_X0 = 5, HAND_FRONT_X1 = 13;                   // grows right → left
// Backlight diagonal = the enclosure's long axis. DIAG_C: point on it nearest the panel centre; DIAG_U: unit direction (to top-right)
const float DIAG_C_X = 8.29f, DIAG_C_Y = 8.36f, DIAG_U_X = 0.7337f, DIAG_U_Y = -0.6796f;
const float DIAG_IN = -11.5f;                                         // trail starts just off the bottom-left
const float DIAG_STOP_NEG = 5.6f, DIAG_STOP_POS = 5.0f;               // outward pulses stop at the enclosure's edges
// Backlight hand silhouette, traced from the hand layers: coverage 0–9 per cell (row 0 = top)
const char *const HAND_MASK[16] = {
  "0000000000000000", "0000000000000000", "0000000000000000", "0000000000000000",
  "0000000001001100", "0000026899889400", "4568999999999200", "9999999999999000",
  "9999999999995000", "9999999999960000", "9975468899920000", "4100000002100000",
  "0000000000000000", "0000000000000000", "0000000000000000", "0000000000000000" };

// ---------------- Timing (seconds into the 28 s run) ----------------
const float RUN = 28.0f;
const float TRAIL_T0 = 0.3f, TRAIL_SPEED = 3.3f, TRAIL_TAIL = 5.0f, TRAIL_B = 0.45f;   // purple trail in along the diagonal
const float T_ARR = TRAIL_T0 + (0 - DIAG_IN) / TRAIL_SPEED;                           // ≈ 3.8 s: reaches the centre, now pink
const float PULSE_EVERY = 1.2f, PULSE_SPEED = 4.0f, PULSE_TAIL = 3.0f;
const float ENCL_ON = 7.0f, ENCL_FADE = 4.0f, ENCL_B = 0.6f;     // enclosure under-light fades in pink
const float GLOW_ON = 11.0f, GLOW_FADE = 4.0f, GLOW_B = 0.22f;   // backlight hand glow, pale pinkish white
const float GLOW_DIM = 24.0f, GLOW_DIM_S = 3.0f, GLOW_HOLD = 0.20f; // hand glow dims to 20%, then stays lit
const float HB_ON = 16.0f, HF_ON = HB_ON + 1.0f;                  // back hand, then front hand 1 s later
const float SPREAD = 4.0f, COL_FADE = 0.8f, HAND_B = 0.7f;        // columns/s, per-column fade, brightness
const float BREATH_ON = 20.0f, BREATH_A = 0.15f, BREATH_P = 4.0f; // assembled: enclosure and glow breathe ±15% every 4 s
const float TWO_PI_F = 6.28318530718f;

// ---------------- Helpers ----------------
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float smooth(float a, float b, float x) { float t = clamp01((x - a) / (b - a)); return t * t * (3 - 2 * t); }
inline uint8_t ch(float v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : floorf(v + 0.5f))); }
inline CRGB mix(const CRGB &a, const CRGB &b, float t) {
  return CRGB(ch(a.r + (b.r - a.r) * t), ch(a.g + (b.g - a.g) * t), ch(a.b + (b.b - a.b) * t));
}
inline float bump(float s, float t0, float dur) { float a = (s - t0) / dur; return (a > 0 && a < 1) ? powf(sinf(PI * a), 2) : 0; }
inline float breathe(float s, float t) { return 1 + BREATH_A * smooth(BREATH_ON, BREATH_ON + 2, s) * sinf(TWO_PI_F * (t - BREATH_ON) / BREATH_P); }
inline float along(float x, float y)  { return (x - DIAG_C_X) * DIAG_U_X + (y - DIAG_C_Y) * DIAG_U_Y; }
inline float across(float x, float y) { return fabsf((x - DIAG_C_X) * DIAG_U_Y - (y - DIAG_C_Y) * DIAG_U_X); }

// Diagonal pulses: from just after the trail arrives until the enclosure is fully lit, easing down as it comes up
struct Pulse { float start, bright; };
static Pulse pulses[8];
static int   pulseCount = -1;
inline void init() {
  if (pulseCount >= 0) return;
  pulseCount = 0;
  for (float L = T_ARR + 0.3f; L <= ENCL_ON + ENCL_FADE - 1 && pulseCount < 8; L += PULSE_EVERY, pulseCount++)
    pulses[pulseCount] = { L, pulseCount == 0 ? 0.6f : 0.45f * (1 - 0.5f * smooth(ENCL_ON, ENCL_ON + ENCL_FADE, L)) };
}

}  // namespace box4

// ---------------- Render functions (standard interface) ----------------
void renderBox4Bottom(double progress, double seconds, double level) {
  using namespace box4;
  box4::init();
  const uint8_t P = BOTTOM[3];
  float s = (float)(progress < 0 ? 0 : (progress > 1 ? 1 : progress)) * RUN, t = (float)seconds, L = (float)level;
  // enclosure sheet fades in pink (and breathes once assembled)
  float e = smooth(ENCL_ON, ENCL_ON + ENCL_FADE, s);
  if (e > 0) for (int x = ENCL_X0; x <= ENCL_X1; x++) dot(P, x, ROW_ENCLOSURE, PINK, L * ENCL_B * e * breathe(s, t));
  // back hand fades in white from its centre out to its edges
  for (int x = HAND_BACK_X0; x <= HAND_BACK_X1; x++) {
    float on = HB_ON + fabsf(x - HAND_BACK_CX) / SPREAD, a = smooth(on, on + COL_FADE, s);
    if (a > 0) dot(P, x, ROW_HAND_BACK, WHITE_, L * HAND_B * a);
  }
  // a second later the front hand fades in white, right → left
  for (int x = HAND_FRONT_X0; x <= HAND_FRONT_X1; x++) {
    float on = HF_ON + (HAND_FRONT_X1 - x) / SPREAD, a = smooth(on, on + COL_FADE, s);
    if (a > 0) dot(P, x, ROW_HAND_FRONT, WHITE_, L * HAND_B * a);
  }
}

void renderBox4Rear(double progress, double seconds, double level) {
  using namespace box4;
  box4::init();
  const uint8_t P = REAR[3];
  float s = (float)(progress < 0 ? 0 : (progress > 1 ? 1 : progress)) * RUN, t = (float)seconds, L = (float)level;
  // 1) purple trail comes in on the diagonal, turning pink as it reaches the centre
  float head = fminf(0, DIAG_IN + (s - TRAIL_T0) * TRAIL_SPEED), trailFade = 1 - smooth(T_ARR, T_ARR + 0.8f, s);
  // 2) centre swell, then pulses out both ways to the enclosure's edges, pink → purple
  float swell = 0.6f * bump(s, T_ARR - 0.2f, 1.0f);
  for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
    float tA = along(x, y), w = expf(-powf(across(x, y) / 0.75f, 2));
    if (w < 0.03f) continue;
    if (s >= TRAIL_T0 && trailFade > 0) {
      float d = head - tA, b = d < 0 ? 0.6f * smooth(-1.2f, 0, d) : 1 - smooth(0, TRAIL_TAIL, d);
      if (b > 0.01f) dot(P, x, y, mix(PURPLE, PINK, smooth(DIAG_IN + 1, 0, tA)), L * TRAIL_B * b * w * trailFade);
    }
    if (swell > 0.005f) {
      float r = hypotf(x - DIAG_C_X, y - DIAG_C_Y);
      dot(P, x, y, PINK, L * swell * expf(-powf(r / 1.1f, 2)));
    }
    for (int i = 0; i < pulseCount; i++) {
      if (s < pulses[i].start) continue;
      float lim = tA >= 0 ? DIAG_STOP_POS : DIAG_STOP_NEG;
      if (fabsf(tA) > lim + 0.6f) continue;                       // nothing beyond the enclosure's edge
      float r = (s - pulses[i].start) * PULSE_SPEED, d = r - fabsf(tA);
      if (d < -1.2f || d > PULSE_TAIL) continue;
      float b = (d < 0 ? 0.6f * smooth(-1.2f, 0, d) : 1 - smooth(0, PULSE_TAIL, d)) * (1 - smooth(lim, lim + 1.5f, r));
      if (b > 0.01f) dot(P, x, y, mix(PINK, PURPLE, smooth(1, lim, fabsf(tA))), L * pulses[i].bright * b * w);
    }
  }
  // 3) keep the original hand glow until 24 s, then dim to 20% by 27 s.
  // It remains visible while the finale travels back to Box 1.
  const float dim = smooth(GLOW_DIM, GLOW_DIM + GLOW_DIM_S, s);
  float g = smooth(GLOW_ON, GLOW_ON + GLOW_FADE, s) * (1 - (1 - GLOW_HOLD) * dim);
  if (g > 0) {
    const CRGB PALE = mix(PINK, WHITE_, 0.75f);
    float br = 1 + (breathe(s, t) - 1) * (1 - dim); // settle to a steady 20% at the end
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      int c = HAND_MASK[y][x] - '0';
      if (c) dot(P, x, y, PALE, L * GLOW_B * g * br * (c / 9.0f));
    }
  }
}
