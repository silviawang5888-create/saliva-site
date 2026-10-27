/*
 * =============================================================================
 *  D2D_Saliva_Full.ino — "Inside a Drop of Saliva" · complete 11-panel program
 *  4 boxes + 3 bridges · 1 Hall sensor (Box 1 disc) · 4 buttons · 2 speakers · Wi-Fi control page
 *  Kyra · assembled with Claude (Anthropic) · 23 Oct 2026
 *  STATUS: Generated but not physically tested.
 * =============================================================================
 *
 *  BOARD     ESP32-S3 Dev Module (Arduino ESP32 core 3.x)
 *            Tools → USB CDC On Boot: Enabled
 *  LIBRARIES FastLED and PubSubClient (Nick O'Leary) from the library manager.
 *            Everything else comes with the ESP32 core (WiFi, WebServer, Preferences, ESP_I2S).
 *
 *  WIRING (all on the Box 1 perfboard)
 *    LED data   GPIO 4  → 330 Ω → Box 1 under-light DIN (first panel of the single chain)
 *    Hall       GPIO 1  ← DRV5055A1 OUT   (VCC 3V3, GND)   Box 1 mask disc
 *    Button 1   GPIO 21 → button → GND    (Box 1, on top of the box)
 *    Button 2   GPIO 42 → button → GND    (Box 2)
 *    Button 3   GPIO 41 → button → GND    (Box 3)
 *    Button 4   GPIO 2  → button → GND    (Box 4 — can be switched off on the control page)
 *    Speaker 1  amp A, louder speaker  (I2S port 0): BCLK GPIO 38 · LRC GPIO 39 · DIN GPIO 40
 *    Speaker 2  amp B, quieter speaker (I2S port 1): BCLK GPIO 15 · LRC GPIO 16 · DIN GPIO 17
 *               Both amps: SD → 3V3 · GAIN not connected · Vin 5V · GND  (no resistors —
 *               the same setup as Kyra's working two-port breadboard test)
 *               Both: Vin 5V · GND · GAIN not connected. Same sound, separate volume
 *               (set on the control page / developer console, saved in the ESP32).
 *
 *  CHAIN ORDER (standard §2): 0 Box1 under · 1 Box1 back · 2 Bridge 1 (8x32) · 3 Box2 back · 4 Box2 under
 *                             5 Bridge 2 · 6 Box3 back · 7 Box3 under · 8 Bridge 3 · 9 Box4 back · 10 Box4 under
 *  POWER: supply A = panels 0–4 (LEDs 0–1279), supply B = panels 5–10 (LEDs 1280–2815).
 *         Each group is limited in software (SUPPLY_LIMIT_MA, below).
 *
 *  WHAT IT DOES
 *    IDLE   Box 1 guide light (turn the disc), other boxes breathe faintly.
 *    TOUR   Disc aligned (Hall) → Box 1 → Bridge 1 → Box 2 → Bridge 2 → Box 3 → Bridge 3 → Box 4 → hold → fade.
 *           Relay: when Box n+1 starts, Box n and Bridge n go dark. After Box 4 has finished every
 *           box and bridge comes back on (finished scenes) for 5 s, then fades (HANDOFF_FADE_S, FINALE_IN_S).
 *    FOCUS  Button n → only Box n plays (28 s) → hold → fade. If a tour was running it pauses and then
 *           continues from where it stopped. Buttons have a cooldown, and a running focus cannot be
 *           replaced for FOCUS_LOCK seconds, so several visitors cannot make the boxes flicker.
 *
 *  VISITOR WEBSITE  Visitors tap an NFC tag → the website (GitHub Pages) → HiveMQ cloud → this ESP32.
 *                ESP32 joins the internet through any Wi-Fi saved on http://192.168.4.1/wifi
 *                (up to 3: RMIT-University / eduroam with ID + password, or a phone hotspot / 4G router).
 *                Visitor requests follow the same rules as the buttons (cooldown + focus lock).
 *  CONTROL PAGE  Join Wi-Fi "Saliva-Control" (password d2d-saliva), open http://192.168.4.1
 *                or, on RMIT-University, open the ESP32's address shown in Serial.
 *                Mode, play buttons, brightness, volume, Hall sensitivity, lock time, buttons on/off.
 *                Settings are saved and survive a power cut.
 *
 *  NO USB NEEDED AFTER THE BOX IS CLOSED (everything Serial did is on the pages):
 *    http://192.168.4.1        play / stop / test pattern / Hall readings / settings / log / restart
 *    http://192.168.4.1/wifi   saved Wi-Fi networks
 *    http://192.168.4.1/update upload a new program (.bin from Sketch → Export Compiled Binary)
 *    Arduino IDE: laptop on "Saliva-Control" → Tools → Port → network port "saliva-esp32" (password d2d-saliva)
 *    IMPORTANT: upload once by USB with Tools → Flash Size 8MB, Partition Scheme
 *               "8M with spiffs (3MB APP/1.5MB SPIFFS)" — a scheme without OTA can't update over Wi-Fi.
 *
 *  SERIAL 115200   g = tour · 1–4 = focus box · s = stop (idle) · t = test pattern on/off · h = Hall readings on/off
 *                  w = backup way to save RMIT-University ID + password · x = erase all saved networks
 *
 *  TEST PATTERN (t): every 16x16 panel shows red/green/blue corners and (panel index + 1) white dots in a row;
 *    bridges show red at the Box n end, green at the Box n+1 end and (bridge number) white dots.
 *    Fix any wrong direction in PANEL_* / BRIDGE_* below — never inside an animation.
 *
 *  Box 1 and Box 3 modules: Chan Yeuk Hei (unchanged). Box 2: ported from the 23 Oct handoff spec
 *  (defaults BG1 twinkles · P2 pieces · P3 propagate). Box 4 and the bridges: new placeholder designs.
 * =============================================================================
 */
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "esp_eap_client.h"            // to switch between school (802.1X) and normal Wi-Fi
#include <ESP_I2S.h>
#include <FFat.h>                       // music files for the four labs (partition "16M Flash (3MB APP/9.9MB FATFS)")
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Update.h>                     // update the program from the web page
#include <HTTPUpdate.h>                 // or from a link (remote developer console)
#include <DNSServer.h>                  // phones open the console by themselves when joining Saliva-Control
#include "mbedtls/md.h"                 // checks the developer key signature
#include <time.h>
#include <sys/time.h>
#include <ArduinoOTA.h>                 // or straight from Arduino IDE over Wi-Fi
#include "esp_system.h"
#include <math.h>

// =============================================================================
//  1) HARDWARE  [STANDARD]
// =============================================================================
#define LED_PIN     4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
const uint16_t NUM_LEDS  = 2816;          // 11 panels × 256
const uint8_t  BRIGHTNESS_DEFAULT = 179;
const uint16_t FRAME_MS  = 100;           // about 10 fps (one 2816-LED chain needs ~85 ms per frame)
CRGB leds[NUM_LEDS];

const int HALL_PIN = 1;
const int BUTTON_PIN[4] = { 21, 42, 41, 2 };   // button 1 = GPIO 21, button 4 = GPIO 2 (as wired); GPIO 40 is speaker 1
const int I2S_BCLK = 38, I2S_LRC = 39, I2S_DOUT = 40;      // speaker 1 = amp A, louder (I2S port 0)
const int I2S2_BCLK = 15, I2S2_LRC = 16, I2S2_DOUT = 17;   // speaker 2 = amp B, quieter (I2S port 1)

const uint16_t SUPPLY_SPLIT_LED  = 1280;  // first LED on supply B (panel 5)
const uint32_t SUPPLY_LIMIT_MA   = 5500;  // software limit per 10 A supply (leaves headroom)

#define ENABLE_WIFI 1                      // 0 = no control page (use this if the LEDs flicker with Wi-Fi on)
const char* AP_SSID = "Saliva-Control";
const char* AP_PASS = "d2d-saliva";       // at least 8 characters
const char* FW_VERSION = __DATE__ " " __TIME__;   // shown on both consoles — tells you an update really arrived

// Internet Wi-Fi is set on the page http://192.168.4.1/wifi (up to 3 networks, tried in order).
// Nothing about networks or passwords is written in this code.
// Cloud relay for the visitor website (must match the website's settings)
const char* MQTT_HOST   = "broker.hivemq.com";   // free public broker (test result: port 8883 works on campus)
const uint16_t MQTT_PORT = 8883;                  // encrypted
const char* MQTT_TOPIC  = "d2d-saliva-p6w58uii9y"; // random channel name — keep it the same as in index.html

// =============================================================================
//  2) PANEL TABLES  [STANDARD §2]
// =============================================================================
const uint8_t BOTTOM[4] = { 0, 4, 7, 10 };   // under-light of Box 1..4
const uint8_t REAR[4]   = { 1, 3, 6, 9 };    // backlight of Box 1..4
const uint8_t BRIDGE[3] = { 2, 5, 8 };       // 8x32 bridges between the boxes

// Physical direction of every 16x16 panel (index = chain position). Bridge slots are ignored.
// Defaults come from the calibrated Box 1 / Box 3 previews: under-lights column-major, backlights
// row-major with a vertical flip. ⚠ Box 2 and Box 4 have not been checked — use the test pattern.
//                                   0      1      2      3      4      5      6      7      8      9      10
const bool PANEL_SERPENTINE[11]   = { true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true  };
const bool PANEL_COLUMN_MAJOR[11] = { true,  false, false, false, true,  false, false, true,  false, false, true  };
const bool PANEL_FLIP_X[11]       = { false, false, false, false, false, false, false, false, false, false, false };
const bool PANEL_FLIP_Y[11]       = { false, true,  false, true,  false, false, true,  false, false, true,  false };

// 8x32 bridges: x 0 = end next to Box n … 31 = end next to Box n+1; y 0 = top … 7 = bottom.
// Most 8x32 panels are column-major serpentine. ⚠ check with the test pattern.
const bool BRIDGE_COLUMN_MAJOR[3] = { true,  true,  true  };
const bool BRIDGE_SERPENTINE[3]   = { true,  true,  true  };
const bool BRIDGE_FLIP_X[3]       = { false, false, false };
const bool BRIDGE_FLIP_Y[3]       = { false, false, false };

static inline bool isBridgePanel(uint8_t p) { return p == 2 || p == 5 || p == 8; }

uint16_t xy(uint8_t panel, uint8_t x, uint8_t y) {
  // Box 2 under-light: the animation module uses six adjacent logical sheet rows
  // (15..10, front to back).  The physical acrylic layers sit on every second
  // LED row, so place them on 15, 13, 11, 9, 7, 5.  The rows in between remain dark.
  if (panel == BOTTOM[1] && y >= 10 && y <= 15) {
    y = (uint8_t)(2 * y - 15);
  }

  // Box 2 rear: rotate the complete visible animation 90 degrees left
  // (counter-clockwise) when viewed from the front of the box.
  if (panel == REAR[1]) {
    const uint8_t oldX = x;
    x = y;
    y = 15 - oldX;
  }

  // Box 3 rear panel (chain panel 6): rotate 180 degrees.
  // 27 Oct 2026 test pattern: the old 90-degree compensation (x = 15 - y, y = x) left the
  // whole Box 3 backlight turned 90 degrees counter-clockwise (top-left showed GREEN,
  // bottom-left RED, bottom-right BLUE). A 180-degree rotation brings it upright,
  // the same correction Box 4 uses.
  if (panel == REAR[2]) {
    x = 15 - x;
    y = 15 - y;
  }

  // Box 4 rear: rotate the complete visible animation 180 degrees when viewed
  // from the front of the box.
  if (panel == REAR[3]) {
    x = 15 - x;
    y = 15 - y;
  }
  if (PANEL_FLIP_X[panel]) x = 15 - x;
  if (PANEL_FLIP_Y[panel]) y = 15 - y;
  const uint8_t line = PANEL_COLUMN_MAJOR[panel] ? x : y;
  uint8_t pos = PANEL_COLUMN_MAJOR[panel] ? y : x;
  if (PANEL_SERPENTINE[panel] && (line & 1)) pos = 15 - pos;
  return (uint16_t)panel * 256 + (uint16_t)line * 16 + pos;
}

uint16_t bridgeXY(uint8_t b, uint8_t x, uint8_t y) {
  if (BRIDGE_FLIP_X[b]) x = 31 - x;
  if (BRIDGE_FLIP_Y[b]) y = 7 - y;
  const uint16_t base = (uint16_t)BRIDGE[b] * 256;
  if (BRIDGE_COLUMN_MAJOR[b]) {
    uint8_t pos = y;
    if (BRIDGE_SERPENTINE[b] && (x & 1)) pos = 7 - pos;
    return base + (uint16_t)x * 8 + pos;
  }
  uint8_t pos = x;
  if (BRIDGE_SERPENTINE[b] && (y & 1)) pos = 31 - pos;
  return base + (uint16_t)y * 32 + pos;
}

uint8_t scaleChannel(uint8_t c, float brightness) { return (uint8_t)(c * brightness + 0.5f); }

// put(): set one LED of a 16x16 panel to colour × brightness (0–1). Outside the panel → ignored.
void putBridge(uint8_t b, int x, int y, const CRGB& color, float brightness);
void put(uint8_t panel, int x, int y, const CRGB& color, float brightness) {
  if (isBridgePanel(panel)) { putBridge(panel == 2 ? 0 : (panel == 5 ? 1 : 2), x, y, color, brightness); return; }   // bridges: x 0–31, y 0–7
  if (panel > 10 || x < 0 || x > 15 || y < 0 || y > 15) return;
  if (!(brightness > 0.0f)) return;
  if (brightness > 1.0f) brightness = 1.0f;
  leds[xy(panel, (uint8_t)x, (uint8_t)y)] = CRGB(scaleChannel(color.r, brightness),
                                                 scaleChannel(color.g, brightness),
                                                 scaleChannel(color.b, brightness));
}
// dot(): add colour × brightness on top of what is there (saturating).
void dot(uint8_t panel, int x, int y, const CRGB& color, float brightness) {
  if (panel > 10 || isBridgePanel(panel) || x < 0 || x > 15 || y < 0 || y > 15) return;
  if (!(brightness > 0.0f)) return;
  if (brightness > 1.0f) brightness = 1.0f;
  CRGB& p = leds[xy(panel, (uint8_t)x, (uint8_t)y)];
  const uint16_t r = p.r + scaleChannel(color.r, brightness);
  const uint16_t g = p.g + scaleChannel(color.g, brightness);
  const uint16_t b = p.b + scaleChannel(color.b, brightness);
  p = CRGB(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
}
// putBridge(): same as put() for a bridge (x 0–31, y 0–7).
void putBridge(uint8_t b, int x, int y, const CRGB& color, float brightness) {
  if (b > 2 || x < 0 || x > 31 || y < 0 || y > 7) return;
  if (!(brightness > 0.0f)) return;
  if (brightness > 1.0f) brightness = 1.0f;
  leds[bridgeXY(b, (uint8_t)x, (uint8_t)y)] = CRGB(scaleChannel(color.r, brightness),
                                                   scaleChannel(color.g, brightness),
                                                   scaleChannel(color.b, brightness));
}

// =============================================================================
//  3) ANIMATION MODULES
// =============================================================================
// Final animation set 2026-10-27: Box 1 Pattern / Deposition v6
#include "Box1_Pattern.h"
#include "D2D_Box1_HallTrigger.h"

// Final animation set 2026-10-27: Box 2 Sense / Biosensor
#include "box2_sense_render.h"

// Final animation set 2026-10-27: Box 3 Connect / Electronics
#include "Box3_Connect.h"

// Final animation set 2026-10-27: Box 4 Assemble / Machining
#include "box4_assemble_render.h"

// Final animation set 2026-10-27: all three 8x32 bridge animations
#include "D2D_Bridges_1_2.h"
#include "D2D_Bridge3.h"

// Quiet bridge glow used only while the installation is waiting for a visitor.
// The active Bridge 1/2/3 sequences above use the approved animation modules.
void renderBridgeIdle(int b, double seconds, double level) {
  static const CRGB COL[4] = { CRGB(255, 85, 0), CRGB(86, 209, 194), CRGB(109, 51, 189), CRGB(255, 0, 95) };
  const float phase = (float)seconds * 0.8f + b * 1.3f;
  for (int x = 0; x < 32; x++) {
    const float u = x / 31.0f;
    const CRGB c(
      (uint8_t)(COL[b].r + (COL[b + 1].r - COL[b].r) * u + 0.5f),
      (uint8_t)(COL[b].g + (COL[b + 1].g - COL[b].g) * u + 0.5f),
      (uint8_t)(COL[b].b + (COL[b + 1].b - COL[b].b) * u + 0.5f));
    const float wave = 0.5f + 0.5f * sinf(phase - x * 0.35f);
    putBridge(b, x, 3, c, 0.05f * wave * wave * (float)level);
    putBridge(b, x, 4, c, 0.05f * wave * wave * (float)level);
  }
}

// Faint breathing of a box's layer rows while waiting (Box 2–4; Box 1 has its own idle scene)
void renderBoxIdleGlow(int box, double seconds, double level) {
  static const CRGB COL[4] = { CRGB(255, 85, 0), CRGB(86, 209, 194), CRGB(109, 51, 189), CRGB(255, 64, 160) };
  static const int8_t ROWS_STD[6] = { 15, 13, 11, 9, 7, 5 };
  static const int8_t ROWS_BOX2[5] = { 15, 14, 13, 12, 11 };
  const int n = box == 1 ? 5 : 6;
  for (int k = 0; k < n; k++) {
    const int y = box == 1 ? ROWS_BOX2[k] : ROWS_STD[k];
    const float b = (0.05f + 0.07f * (0.5f + 0.5f * sinf(6.2831853f * (float)seconds / 6.0f - k * 0.6f - box))) * (float)level;
    for (int x = 2; x <= 13; x++) put(BOTTOM[box], x, y, COL[box], b);
  }
}

// =============================================================================
//  4) SOUND — synthesised cues (no sound files needed), played on core 0
//     Both MAX98357A boards get the same sound, each on its own I2S port with its own volume.
// =============================================================================
enum Cue : uint8_t { CUE_TICK, CUE_START, CUE_BOX1, CUE_UV, CUE_GOLD, CUE_BOX2, CUE_PING, CUE_BOX3,
                     CUE_BOX4, CUE_CLICK, CUE_POWER, CUE_BRIDGE, CUE_TEST, CUE_COUNT };
enum Wave : uint8_t { W_SINE, W_SQUARE, W_TRI, W_NOISE };
struct Note { uint8_t cue; float delay; uint8_t wave; float f0, f1, dur, vol, atk; };
static const Note NOTES[] = {
  { CUE_TICK,   0.00f, W_SINE,   1800, 1800, 0.06f, 0.25f, 0.002f },
  { CUE_START,  0.00f, W_SINE,    523,  523, 0.50f, 0.28f, 0.01f },
  { CUE_START,  0.12f, W_SINE,    659,  659, 0.50f, 0.28f, 0.01f },
  { CUE_START,  0.24f, W_SINE,    784,  784, 0.50f, 0.28f, 0.01f },
  { CUE_START,  0.36f, W_SINE,   1047, 1047, 0.70f, 0.28f, 0.01f },
  { CUE_BOX1,   0.00f, W_TRI,     110,  110, 1.60f, 0.28f, 0.30f },
  { CUE_BOX1,   0.10f, W_SINE,    220,  220, 1.30f, 0.12f, 0.30f },
  { CUE_UV,     0.00f, W_NOISE,     0,    0, 0.15f, 0.30f, 0.005f },
  { CUE_UV,     0.05f, W_SQUARE,  880, 1760, 0.25f, 0.10f, 0.01f },
  { CUE_UV,     0.40f, W_NOISE,     0,    0, 0.15f, 0.25f, 0.005f },
  { CUE_GOLD,   0.00f, W_SINE,   1319, 1319, 0.80f, 0.20f, 0.01f },
  { CUE_GOLD,   0.10f, W_SINE,   1568, 1568, 0.80f, 0.20f, 0.01f },
  { CUE_GOLD,   0.20f, W_SINE,   2093, 2093, 0.90f, 0.20f, 0.01f },
  { CUE_BOX2,   0.00f, W_SINE,    400,  900, 0.12f, 0.30f, 0.005f },
  { CUE_BOX2,   0.25f, W_SINE,    450, 1000, 0.12f, 0.30f, 0.005f },
  { CUE_BOX2,   0.45f, W_SINE,    380,  850, 0.12f, 0.30f, 0.005f },
  { CUE_BOX2,   0.70f, W_SINE,    600, 1200, 0.15f, 0.30f, 0.005f },
  { CUE_PING,   0.00f, W_SINE,   1200, 1150, 0.45f, 0.18f, 0.005f },
  { CUE_BOX3,   0.00f, W_SQUARE, 1200, 1200, 0.06f, 0.14f, 0.003f },
  { CUE_BOX3,   0.10f, W_SQUARE, 1200, 1200, 0.06f, 0.14f, 0.003f },
  { CUE_BOX3,   0.20f, W_SQUARE, 1800, 1800, 0.06f, 0.14f, 0.003f },
  { CUE_BOX3,   0.35f, W_SQUARE, 1500, 1500, 0.06f, 0.14f, 0.003f },
  { CUE_BOX3,   0.50f, W_SQUARE, 2000, 2000, 0.12f, 0.14f, 0.003f },
  { CUE_BOX4,   0.00f, W_SINE,    392,  392, 1.50f, 0.16f, 0.20f },
  { CUE_BOX4,   0.00f, W_SINE,    494,  494, 1.50f, 0.14f, 0.20f },
  { CUE_BOX4,   0.00f, W_SINE,    587,  587, 1.50f, 0.12f, 0.20f },
  { CUE_CLICK,  0.00f, W_NOISE,     0,    0, 0.03f, 0.35f, 0.001f },
  { CUE_CLICK,  0.00f, W_SINE,   2400, 2400, 0.05f, 0.20f, 0.001f },
  { CUE_POWER,  0.00f, W_SINE,    220,  880, 1.20f, 0.25f, 0.05f },
  { CUE_POWER,  0.00f, W_TRI,     440, 1760, 1.20f, 0.10f, 0.05f },
  { CUE_BRIDGE, 0.00f, W_SINE,    300,  700, 1.00f, 0.12f, 0.40f },
  { CUE_BRIDGE, 0.00f, W_NOISE,     0,    0, 0.80f, 0.05f, 0.40f },
  { CUE_TEST,   0.00f, W_SINE,    440,  440, 1.50f, 1.00f, -1.0f },   // steady test tone (atk < 0 = flat, like the speaker test sketch)
};
// How loud each cue is compared with the others (1 = as loud as the steady test tone at the same volume)
static float cueLevel(uint8_t c) { return c == CUE_TICK ? 0.5f : c == CUE_CLICK ? 0.8f : 1.0f; }
// Envelope of one note at time t (shared by the ESP32 synth and the website simulator)
static inline float noteEnv(float t, float dur, float atk) {
  if (t < 0.0f || t >= dur) return 0.0f;
  if (atk < 0.0f) { float e = t / 0.01f; const float o = (dur - t) / 0.05f; if (o < e) e = o; return e > 1.0f ? 1.0f : e; }
  const float u = t / dur;
  return t < atk ? t / atk : (1.0f - u) * (1.0f - u);
}
static inline float waveAmp(uint8_t w) { return w == W_SQUARE ? 0.6f : 1.0f; }
static const int N_NOTES = sizeof(NOTES) / sizeof(NOTES[0]);

namespace Audio {
static const int SR = 22050, BLOCK = 256, MAX_VOICES = 12;
// Levels tested on the real speakers (full sine): amp A 100–1000, amp B 500–5000 (ratio 1 : 5).
// Total volume 0 → MIN_LEVEL, 100 → MAX_LEVEL (with speaker 1 = 20, speaker 2 = 100):
//   now: speaker 1 only, volume 0 → 2000 · volume 100 → 20000 (speaker sliders are ignored).
static const float MIN_LEVEL = 2000.0f, MAX_LEVEL = 20000.0f;  // 25 Oct: only speaker 1 in use — volume 0 → 2000, 100 → 20000
// Speaker 2 (amp B, GPIO 15/16/17) is switched off for now (part broken). To use it again: set USE_SPEAKER2 = true.
static const bool USE_SPEAKER2 = false;
// Every cue is scaled so its loudest moment reaches full scale × cueLevel() — at volume 100 that is exactly
// the maximum level (A 2000 / B 10000).
static float cueGain[CUE_COUNT];
static void computeCueGains() {
  for (int c = 0; c < CUE_COUNT; c++) {
    float end = 0.0f;
    for (int n = 0; n < N_NOTES; n++) if (NOTES[n].cue == c && NOTES[n].delay + NOTES[n].dur > end) end = NOTES[n].delay + NOTES[n].dur;
    float peak = 0.0f;
    for (float t = 0.0f; t < end; t += 0.002f) {
      float a = 0.0f;
      for (int n = 0; n < N_NOTES; n++) if (NOTES[n].cue == c) a += NOTES[n].vol * waveAmp(NOTES[n].wave) * noteEnv(t - NOTES[n].delay, NOTES[n].dur, NOTES[n].atk);
      if (a > peak) peak = a;
    }
    cueGain[c] = peak > 0.0f ? cueLevel(c) / peak : 0.0f;
  }
}
struct Voice { bool on; uint8_t wave; float f0, f1, dur, vol, atk, t, phase, wait; };
static Voice voices[MAX_VOICES];
static QueueHandle_t queue = nullptr;
static I2SClass i2s(I2S_NUM_0), i2s2(I2S_NUM_1);   // fixed ports, mono — as in the working breadboard test
static volatile float masterVol = 0.6f;
static volatile float spkVol[2] = { 1.0f, 1.0f };
static volatile uint8_t solo = 0;         // 0 = both · 1 / 2 = only that speaker (speaker test)
static volatile uint32_t soloUntil = 0;
static bool ok = false, ok2 = false;
static uint32_t noiseState = 22222;

// ---- Sound files on the flash (22050 Hz, mono, 16-bit — the upload page converts) ----
//  /journey.wav  = continuous soundtrack Box 1 → Bridge 1 → Box 2 → Bridge 2 → Box 3 → Bridge 3 → Box 4 (135 s).
//                  Starts once at tour time 0 and plays straight through; it replaces the synth cues of Box 1–4,
//                  the bridge whoosh and the start chime. In a single-box focus the matching part of it plays.
//  /box1.wav … /box4.wav = one track per lab (used where the journey does not cover the box, i.e. Box 4).
static const int NTRACK = 5, JOURNEY = 4, JOURNEY_BOXES = 4;   // 135 s journey covers Box 1–4
static bool fsOk = false;
static volatile bool hasTrack[NTRACK] = { false, false, false, false, false };
static uint32_t trackData[NTRACK] = { 0 }, trackLen[NTRACK] = { 0 };   // data offset / bytes
static void trackPath(int t, char* p) { if (t == JOURNEY) strcpy(p, "/journey.wav"); else snprintf(p, 16, "/box%d.wav", t + 1); }
static volatile int8_t wantBox = -1;        // set by the LED loop every frame
static volatile float wantPos = 0.0f;       // seconds into the box
static volatile int8_t lockBox = -1;        // being uploaded → don't touch the file
static volatile int8_t previewBox = -1;     // "试听" on the music page
static volatile uint32_t previewUntil = 0;
static const float MUSIC_GAIN = 1.0f;

static bool scanWav(int box) {              // read the header, remember where the samples are
  hasTrack[box] = false;
  if (!fsOk) return false;
  char path[16]; trackPath(box, path);
  File f = FFat.open(path, "r");
  if (!f) return false;
  uint8_t h[12];
  if (f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { f.close(); return false; }
  bool fmtOk = false;
  while (f.available() >= 8) {
    uint8_t ch[8]; f.read(ch, 8);
    const uint32_t len = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((uint32_t)ch[7] << 24);
    if (!memcmp(ch, "fmt ", 4)) {
      uint8_t fm[16]; f.read(fm, 16);
      const uint16_t fmt = fm[0] | (fm[1] << 8), chn = fm[2] | (fm[3] << 8), bits = fm[14] | (fm[15] << 8);
      const uint32_t rate = fm[4] | (fm[5] << 8) | (fm[6] << 16) | ((uint32_t)fm[7] << 24);
      fmtOk = fmt == 1 && chn == 1 && bits == 16 && rate == (uint32_t)SR;
      if (len > 16) f.seek(f.position() + len - 16);
    } else if (!memcmp(ch, "data", 4)) {
      trackData[box] = f.position();
      trackLen[box] = min(len, (uint32_t)(f.size() - f.position())) & ~1u;
      f.close();
      hasTrack[box] = fmtOk && trackLen[box] > 0;
      return hasTrack[box];
    } else {
      f.seek(f.position() + len + (len & 1));
    }
  }
  f.close();
  return false;
}
float trackSeconds(int box) { return hasTrack[box] ? trackLen[box] / 2.0f / SR : 0.0f; }

// playback state, used only inside the audio task
static File tf;
static int8_t tBox = -1;
static uint32_t tLeft = 0;
static float tGain = 0.0f;

static void openTrack(int box, float pos) {
  if (tf) tf.close();
  tBox = -1; tLeft = 0;
  if (box < 0 || !hasTrack[box] || box == lockBox) return;
  char path[16]; trackPath(box, path);
  tf = FFat.open(path, "r");
  if (!tf) return;
  uint32_t off = (uint32_t)(pos * SR) * 2;
  if (off >= trackLen[box]) { tf.close(); return; }
  tf.seek(trackData[box] + off);
  tLeft = trackLen[box] - off;
  tBox = box;
}

static float noise() { noiseState = noiseState * 1664525u + 1013904223u; return ((noiseState >> 9) / 8388608.0f) * 2.0f - 1.0f; }

static void startCue(uint8_t cue) {
  for (int n = 0; n < N_NOTES; n++) {
    if (NOTES[n].cue != cue) continue;
    for (int v = 0; v < MAX_VOICES; v++) {
      if (voices[v].on) continue;
      voices[v] = { true, NOTES[n].wave, NOTES[n].f0, NOTES[n].f1, NOTES[n].dur, NOTES[n].vol * cueGain[cue], NOTES[n].atk, 0.0f, 0.0f, NOTES[n].delay };
      break;
    }
  }
}

static void task(void*) {
  static int16_t buf[BLOCK], buf2[BLOCK], mus[BLOCK];
  const float dt = 1.0f / SR;
  uint32_t lastPreview = 0;
  for (;;) {
    uint8_t c;
    while (xQueueReceive(queue, &c, 0) == pdTRUE) startCue(c);
    // ---- music: follow what the LED loop asks for ----
    int8_t wb = wantBox; float wp = wantPos;
    if (previewBox >= 0) {
      if ((int32_t)(millis() - previewUntil) > 0) previewBox = -1;
      else { wb = previewBox; wp = (millis() - lastPreview) / 1000.0f; }
    } else lastPreview = millis();
    if (wb != tBox && tGain <= 0.001f) openTrack(wb, wp);          // switch only after the old one faded out
    else if (wb >= 0 && wb == tBox) {                               // same box started again / jumped → re-sync
      const float at = (trackLen[tBox] - tLeft) / 2.0f / SR;
      if (fabsf(at - wp) > 1.0f && wp < trackSeconds(tBox)) openTrack(wb, wp);
    }
    const float target = (wb == tBox && tBox >= 0) ? 1.0f : 0.0f;
    int got = 0;
    if (tBox >= 0 && tLeft > 0) {
      const int want = min((uint32_t)(BLOCK * 2), tLeft);
      got = tf.read((uint8_t*)mus, want) / 2;
      tLeft -= got * 2;
      if (got == 0) tLeft = 0;
    }
    for (int i = got; i < BLOCK; i++) mus[i] = 0;
    if (solo && (int32_t)(millis() - soloUntil) > 0) solo = 0;
    const float g1 = (solo == 2) ? 0.0f : masterVol * spkVol[0];
    const float g2 = (solo == 1) ? 0.0f : masterVol * spkVol[1];
    for (int i = 0; i < BLOCK; i++) {
      float s = 0.0f;
      for (int v = 0; v < MAX_VOICES; v++) {
        Voice& vc = voices[v];
        if (!vc.on) continue;
        if (vc.wait > 0.0f) { vc.wait -= dt; continue; }
        const float u = vc.t / vc.dur;
        if (u >= 1.0f) { vc.on = false; continue; }
        const float envl = noteEnv(vc.t, vc.dur, vc.atk);
        const float f = vc.f0 + (vc.f1 - vc.f0) * u;
        vc.phase += f * dt;
        if (vc.phase >= 1.0f) vc.phase -= 1.0f;
        float w;
        switch (vc.wave) {
          case W_SQUARE: w = vc.phase < 0.5f ? 0.6f : -0.6f; break;
          case W_TRI:    w = 4.0f * fabsf(vc.phase - 0.5f) - 1.0f; break;
          case W_NOISE:  w = noise(); break;
          default:       w = sinf(6.2831853f * vc.phase);
        }
        s += w * envl * vc.vol;
        vc.t += dt;
      }
      tGain += (target - tGain) * 0.0005f;                        // ~0.1 s fade in / out
      if (tGain < 0.0001f) tGain = 0.0f;
      s += mus[i] * (1.0f / 32768.0f) * tGain * MUSIC_GAIN;
      if (s > 1.0f) s = 1.0f;
      if (s < -1.0f) s = -1.0f;
      const int16_t q1 = (int16_t)(s * g1 * MAX_LEVEL);
      const int16_t q2 = (int16_t)(s * g2 * MAX_LEVEL);
      buf[i] = q1;              // speaker 1
      buf2[i] = q2;             // speaker 2
    }
    if (ok) i2s.write((uint8_t*)buf, sizeof(buf));
    if (ok2) i2s2.write((uint8_t*)buf2, sizeof(buf2));
    if (!ok && !ok2) vTaskDelay(pdMS_TO_TICKS(12));
  }
}

void begin() {
  computeCueGains();
  queue = xQueueCreate(16, sizeof(uint8_t));
  fsOk = FFat.begin(true);
  if (fsOk) { for (int b = 0; b < NTRACK; b++) scanWav(b); }
  Serial.println(fsOk ? F("Music: flash storage ready") : F("Music: no FATFS partition — choose 16M Flash (3MB APP/9.9MB FATFS)"));
  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT, -1);
  ok = i2s.begin(I2S_MODE_STD, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  if (USE_SPEAKER2) {
    i2s2.setPins(I2S2_BCLK, I2S2_LRC, I2S2_DOUT, -1);
    ok2 = i2s2.begin(I2S_MODE_STD, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  }
  Serial.println(ok ? F("Sound: speaker 1 ready") : F("Sound: speaker 1 I2S failed"));
  Serial.println(!USE_SPEAKER2 ? F("Sound: speaker 2 switched off") : ok2 ? F("Sound: speaker 2 ready") : F("Sound: speaker 2 I2S failed"));
  xTaskCreatePinnedToCore(task, "audio", 4096, nullptr, 3, nullptr, 0);
}
void play(uint8_t cue) { if (queue) xQueueSend(queue, &cue, 0); }
void want(int box, float pos) { wantPos = pos; wantBox = box; }
void testSpeaker(uint8_t n, uint8_t cue) { solo = n; soloUntil = millis() + 2500; play(cue); }   // n = 0 both, 1, 2
}  // namespace Audio

// Sound moments inside each box (box-local design seconds). t = 0 is the start of the box.
struct CueAt { uint8_t box; float t; uint8_t cue; };
static const CueAt BOX_CUES[] = {
  { 0, 0.0f, CUE_BOX1 }, { 0, 3.8f, CUE_UV }, { 0, 13.4f, CUE_GOLD },
  { 1, 0.0f, CUE_BOX2 }, { 1, 19.5f, CUE_PING }, { 1, 20.9f, CUE_PING }, { 1, 21.9f, CUE_PING },
  { 2, 0.0f, CUE_BOX3 }, { 2, 8.6f, CUE_BOX3 }, { 2, 13.2f, CUE_PING },
  { 3, 0.0f, CUE_BOX4 }, { 3, 4.0f, CUE_CLICK }, { 3, 6.2f, CUE_CLICK }, { 3, 8.4f, CUE_CLICK },
  { 3, 10.6f, CUE_CLICK }, { 3, 12.8f, CUE_CLICK }, { 3, 15.0f, CUE_CLICK }, { 3, 16.0f, CUE_POWER },
};
static const int N_BOX_CUES = sizeof(BOX_CUES) / sizeof(BOX_CUES[0]);
static inline bool journeyCovers(int box) { return Audio::hasTrack[Audio::JOURNEY] && box < Audio::JOURNEY_BOXES; }
static void fireBoxCues(int box, float prevT, float curT) {
  if (Audio::hasTrack[box] || journeyCovers(box)) return;   // this lab has music / the soundtrack
  for (int i = 0; i < N_BOX_CUES; i++)
    if (BOX_CUES[i].box == box && BOX_CUES[i].t > prevT && BOX_CUES[i].t <= curT) Audio::play(BOX_CUES[i].cue);
}

// =============================================================================
//  5) SETTINGS (saved in flash, changed on the control page)
// =============================================================================
enum Mode : uint8_t { MODE_AUTO = 0, MODE_MANUAL = 1, MODE_IDLE = 2 };
struct Settings {
  uint8_t brightness = BRIGHTNESS_DEFAULT;
  uint8_t volume = 60;          // 0–100, both speakers
  uint8_t spk1 = 20;            // 0–100, speaker 1 = amp A, louder speaker (tested: A 1000 : B 5000 = 1 : 5)
  uint8_t spk2 = 100;           // 0–100, speaker 2 on its own
  uint8_t sleepMin = 10;        // minutes without visitors → LEDs sleep (0 = never sleep)
  uint8_t mode = MODE_AUTO;     // AUTO: sensors and buttons work · MANUAL: only this page · IDLE: always waiting
  int     hallThreshold = 500;  // ADC counts away from the resting value
  uint8_t focusLockS = 10;      // a running focus cannot be replaced for this many seconds
  bool    buttonOn[4] = { true, true, true, true };
  bool    remoteOn = true;      // visitor website can request boxes
} cfg;
Preferences prefs;

void loadSettings() {
  prefs.begin("d2d", true);
  cfg.brightness = prefs.getUChar("bri", cfg.brightness);
  cfg.volume = prefs.getUChar("vol", cfg.volume);
  cfg.spk1 = prefs.getUChar("s1", cfg.spk1);
  cfg.spk2 = prefs.getUChar("s2", cfg.spk2);
  cfg.sleepMin = prefs.getUChar("slp", cfg.sleepMin);
  cfg.mode = prefs.getUChar("mode", cfg.mode);
  cfg.hallThreshold = prefs.getInt("hall", cfg.hallThreshold);
  cfg.focusLockS = prefs.getUChar("lock", cfg.focusLockS);
  for (int i = 0; i < 4; i++) { char k[4] = { 'b', (char)('1' + i), 0 }; cfg.buttonOn[i] = prefs.getBool(k, cfg.buttonOn[i]); }
  cfg.remoteOn = prefs.getBool("remote", cfg.remoteOn);
  prefs.end();
}
void saveSettings() {
  prefs.begin("d2d", false);
  prefs.putUChar("bri", cfg.brightness);
  prefs.putUChar("vol", cfg.volume);
  prefs.putUChar("s1", cfg.spk1);
  prefs.putUChar("s2", cfg.spk2);
  prefs.putUChar("slp", cfg.sleepMin);
  prefs.putUChar("mode", cfg.mode);
  prefs.putInt("hall", cfg.hallThreshold);
  prefs.putUChar("lock", cfg.focusLockS);
  for (int i = 0; i < 4; i++) { char k[4] = { 'b', (char)('1' + i), 0 }; prefs.putBool(k, cfg.buttonOn[i]); }
  prefs.putBool("remote", cfg.remoteOn);
  prefs.end();
}
void applySettings() {
  FastLED.setBrightness(cfg.brightness);
  // total volume 0–100 → level between MIN_LEVEL and MAX_LEVEL (never silent; use speaker 1/2 = 0 to mute one)
  Audio::masterVol = (Audio::MIN_LEVEL + (Audio::MAX_LEVEL - Audio::MIN_LEVEL) * cfg.volume / 100.0f) / Audio::MAX_LEVEL;
  Audio::spkVol[0] = Audio::USE_SPEAKER2 ? cfg.spk1 / 100.0f : 1.0f;   // one speaker: the volume alone sets 2000–20000
  Audio::spkVol[1] = Audio::USE_SPEAKER2 ? cfg.spk2 / 100.0f : 0.0f;
}

// =============================================================================
//  6) DIRECTOR — one place decides what plays, so inputs can never fight each other
// =============================================================================
enum State : uint8_t { ST_IDLE, ST_TOUR, ST_FOCUS };
// Segment lengths (s) — final set 27 Oct: Box 1–4 = 28 s each; every bridge = 5 s.
// The animations follow `progress`, so only these numbers decide the timing.
const float BOX_DUR[4]    = { 28.0f, 28.0f, 28.0f, 28.0f };
const float BRIDGE_DUR[3] = { 5.0f, 5.0f, 5.0f };
const float TOUR_HOLD_S = 5.0f, TOUR_FADE_S = 2.0f;   // finale: every panel on for 5 s, then 2 s fade
// Relay tour (27 Oct): Box n and Bridge n switch off when Box n+1 starts; after Box 4 everything comes back on.
const float HANDOFF_FADE_S = 0.5f;   // how long Box n + Bridge n take to go dark (0 = switch off instantly)
const float FINALE_IN_S    = 0.5f;   // how long Box 1–3 and the bridges take to come back on (0 = instantly)
const float FOCUS_HOLD_S = 5.0f, FOCUS_FADE_S = 1.5f;
const uint32_t BUTTON_COOLDOWN_MS = 3000, ANY_INPUT_GAP_MS = 1000;

static inline float boxStart(int i)    { float t = 0; for (int k = 0; k < i; k++) t += BOX_DUR[k] + BRIDGE_DUR[k]; return t; }
static inline float bridgeStart(int b) { return boxStart(b) + BOX_DUR[b]; }
static inline float tourEnd()          { return boxStart(3) + BOX_DUR[3] + TOUR_HOLD_S + TOUR_FADE_S; }
static inline float focusEnd(int box)  { return BOX_DUR[box] + FOCUS_HOLD_S + FOCUS_FADE_S; }

D2DBox1HallTrigger hall;
State    state = ST_IDLE;
uint32_t stateStartMs = 0;
uint32_t tourStartMs = 0;
bool     tourPaused = false;
float    tourPausedAt = 0.0f;
int      focusBox = 0;
uint32_t focusStartMs = 0;
float    prevLocal[4]  = { -1, -1, -1, -1 };   // last box-local time seen (for sound cues)
float    prevBridge[3] = { -1, -1, -1 };
uint32_t lastAcceptMs = 0, lastButtonMs[4] = { 0, 0, 0, 0 };
bool     testPattern = false, printHall = false;
// Developer-console panel calibration. Values are temporary and are not saved.
// panel 0..10 follows the physical data-chain order; 255 means off.
volatile uint8_t panelTestPanel = 255, panelTestAxis = 0, panelTestLine = 0;
volatile uint8_t panelTestBrightness = 128, panelTestColor = 0;
volatile bool panelTestActive = false;
const char* lastEvent = "start";

// ---- Log for the control page (replaces the Serial Monitor once the box is closed) ----
const int LOG_N = 40;
String logBuf[LOG_N];
int logHead = 0, logCount = 0;
volatile uint32_t logSeq = 0;
SemaphoreHandle_t logMx = nullptr;   // the log is written from two cores
void logLine(const String& msg) {
  if (logMx) xSemaphoreTake(logMx, portMAX_DELAY);
  const uint32_t t = millis() / 1000;
  char ts[16];
  snprintf(ts, sizeof(ts), "%02lu:%02lu:%02lu ", (unsigned long)(t / 3600), (unsigned long)(t / 60 % 60), (unsigned long)(t % 60));
  logBuf[logHead] = String(ts) + msg;
  logHead = (logHead + 1) % LOG_N;
  if (logCount < LOG_N) logCount++;
  logSeq++;
  if (logMx) xSemaphoreGive(logMx);
  Serial.println(msg);
}
const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "通电开机";
    case ESP_RST_SW:       return "页面/程序重启";
    case ESP_RST_PANIC:    return "程序出错重启（请截图日志给 Claude）";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "卡住后自动重启（看门狗）";
    case ESP_RST_BROWNOUT: return "电压不够重启（检查 5V 电源和线）";
    case ESP_RST_EXT:      return "按了 RST";
    default:               return "其他";
  }
}
bool updating = false;   // true while a new program is being received

// "Black box": survives a crash restart (not a power cut). Updated every second, read after a crash.
#define BLACKBOX_MAGIC 0x5A11BA11u
RTC_NOINIT_ATTR uint32_t bbMagic, bbUptime, bbFlags;
RTC_NOINIT_ATTR char bbEvent[24];
String blackBoxReport() {
  if (bbMagic != BLACKBOX_MAGIC) return "";
  String r = "上次出错前运行了 " + String(bbUptime) + " 秒；当时：";
  r += (bbFlags & 1) ? "热点开 " : "热点关 ";
  r += (bbFlags & 2) ? "云端已连 " : "云端未连 ";
  r += (bbFlags & 4) ? "有人看实时画面 " : "";
  r += (bbFlags & 8) ? "正在更新程序 " : "";
  r += (bbFlags & 16) ? "省电待机 " : "";
  bbEvent[sizeof(bbEvent) - 1] = 0;
  r += "· 最后事件 " + String(bbEvent);
  return r;
}

volatile bool statusDirty = false;   // LED core sets it, network core publishes the status
// Power saving
volatile bool sleeping = false;          // standby: simple idle animation, dimmer, 5 frames/s
const uint32_t SLEEP_FRAME_MS = 200;     // 5 frames/s instead of 10 → the chip works about half as much
const uint8_t SLEEP_DIM = 140;           // 140/255 ≈ 55 % of normal brightness
uint32_t lastActivityMs = 0;             // last visitor / developer action
volatile uint32_t apUntilMs = 10UL * 60 * 1000;   // local hotspot "Saliva-Control" stays on until this time
volatile bool apOn = true;
// ---- Live view for the websites: a small copy of what the LEDs show ----
// Only sent while someone has a website open (the page asks every 15 s), 4 frames/s.
// Frame = 1 version byte, then RGB for: Box 1–4 (under 8x8, back 8x8 — each 16x16 panel shrunk 2x,
// brightest LED of every 2x2 kept so single sparkles survive), then Bridge 1–3 (4 rows x 16, x 0 = Box n end).
const uint16_t LIVE_PIXELS = 4 * 128 + 3 * 64;          // 704
const uint32_t LIVE_FRAME_MS = 250;
volatile uint32_t liveFrameMs = LIVE_FRAME_MS;            // 250 ms normally; 1000 ms on a weak signal or after a failed send
uint8_t liveBuf[2][1 + LIVE_PIXELS * 3];
volatile int8_t liveReady = -1;                          // buffer waiting to be sent (network task)
volatile uint32_t watchUntilMs = 0;
uint16_t xy(uint8_t panel, uint8_t x, uint8_t y);
uint16_t bridgeXY(uint8_t b, uint8_t x, uint8_t y);
void captureLive(uint32_t now) {
  static uint32_t lastCap = 0;
  static uint8_t which = 0;
  if ((int32_t)(watchUntilMs - now) <= 0 || liveReady >= 0 || now - lastCap < liveFrameMs) return;
  lastCap = now;
  uint8_t* o = liveBuf[which];
  int k = 0;
  o[k++] = 1;
  auto put4 = [&](uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
    o[k++] = max(max(leds[a].r, leds[b].r), max(leds[c].r, leds[d].r));
    o[k++] = max(max(leds[a].g, leds[b].g), max(leds[c].g, leds[d].g));
    o[k++] = max(max(leds[a].b, leds[b].b), max(leds[c].b, leds[d].b));
  };
  for (int box = 0; box < 4; box++) {
    const uint8_t panels[2] = { BOTTOM[box], REAR[box] };
    for (int q = 0; q < 2; q++)
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
          put4(xy(panels[q], 2 * x, 2 * y), xy(panels[q], 2 * x + 1, 2 * y), xy(panels[q], 2 * x, 2 * y + 1), xy(panels[q], 2 * x + 1, 2 * y + 1));
  }
  for (int b = 0; b < 3; b++)
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 16; x++)
        put4(bridgeXY(b, 2 * x, 2 * y), bridgeXY(b, 2 * x + 1, 2 * y), bridgeXY(b, 2 * x, 2 * y + 1), bridgeXY(b, 2 * x + 1, 2 * y + 1));
  liveReady = which;
  which ^= 1;
}

void wakeUp(uint32_t now) {
  lastActivityMs = now;
  if (sleeping) { sleeping = false; statusDirty = true; }
}

void resetCueClocks() {
  for (int i = 0; i < 4; i++) prevLocal[i] = -1;
  for (int b = 0; b < 3; b++) prevBridge[b] = -1;
}

void goIdle(uint32_t now) {
  panelTestActive = false;
  state = ST_IDLE; stateStartMs = now; tourPaused = false;
  resetCueClocks();
  lastEvent = "idle";
  statusDirty = true;
}
void startTour(uint32_t now) {
  panelTestActive = false;
  state = ST_TOUR; stateStartMs = now; tourStartMs = now; tourPaused = false;
  resetCueClocks();
  if (!Audio::hasTrack[Audio::JOURNEY]) Audio::play(CUE_START);   // the soundtrack starts silent at the trigger
  lastEvent = "tour";
  statusDirty = true;
}
void startFocus(int box, uint32_t now, bool pauseRunningTour) {
  panelTestActive = false;
  if (pauseRunningTour && state == ST_TOUR) {
    tourPaused = true;
    tourPausedAt = (now - tourStartMs) / 1000.0f;
  }
  state = ST_FOCUS; stateStartMs = now; focusStartMs = now; focusBox = box;
  prevLocal[box] = -1;
  static const char* NAMES[4] = { "focus box 1", "focus box 2", "focus box 3", "focus box 4" };
  lastEvent = NAMES[box];
  statusDirty = true;
}
void resumeTour(uint32_t now) {   // continue a paused tour where it stopped
  state = ST_TOUR; stateStartMs = now;
  tourStartMs = now - (uint32_t)(tourPausedAt * 1000.0f);
  tourPaused = false;
  const float T = tourPausedAt;
  for (int i = 0; i < 4; i++) prevLocal[i] = T - boxStart(i);   // do not replay sounds already heard
  for (int b = 0; b < 3; b++) prevBridge[b] = T - bridgeStart(b);
  lastEvent = "tour resumed";
  statusDirty = true;
}

// Inputs: the Hall disc starts a tour only while waiting. A button focuses its box:
//  - while waiting: plays that box;  - during a tour: pauses the tour, plays the box, then continues;
//  - during a focus: another box may take over only after FOCUS_LOCK seconds; the same box is ignored.
// A visitor asks for box n (physical button or visitor website). Returns true if accepted.
bool requestBox(int n, uint32_t now) {
  if (now - lastAcceptMs < ANY_INPUT_GAP_MS || now - lastButtonMs[n] < BUTTON_COOLDOWN_MS) return false;
  if (state == ST_FOCUS) {
    if (n == focusBox) return false;
    if ((now - focusStartMs) / 1000.0f < cfg.focusLockS) return false;
    startFocus(n, now, false);
  } else {
    startFocus(n, now, state == ST_TOUR);
  }
  lastAcceptMs = now; lastButtonMs[n] = now;
  Audio::play(CUE_TICK);
  statusDirty = true;
  return true;
}
void onButton(int n, uint32_t now) {
  if (!cfg.buttonOn[n]) return;
  requestBox(n, now);
}

struct Button { int pin; bool stable = true; bool raw = true; bool held = false; uint32_t changed = 0, downAt = 0; };
Button buttons[4];

// ---- One queue for everything that changes what plays ----------------------------------
// Buttons (input task), control page, visitor website (network task) only *ask*;
// the LED loop does the change between two frames, so nothing can interrupt an animation midway.
enum CmdType : uint8_t { CMD_TOUR, CMD_STOP, CMD_TEST, CMD_FOCUS, CMD_WEB_TOUR, CMD_WEB_BOX, CMD_BUTTON, CMD_APPLY, CMD_SPK, CMD_AP, CMD_SLEEP, CMD_PANEL_TEST };
struct Cmd { uint8_t type; int8_t arg; };
QueueHandle_t cmdQ = nullptr;
void sendCmd(uint8_t type, int8_t arg = 0) {
  if (!cmdQ) return;
  const Cmd c = { type, arg };
  xQueueSend(cmdQ, &c, 0);
}

// Button task: core 1, checks every 5 ms with 40 ms debounce, so even a quick tap is caught
// while the LEDs are being sent (one frame of 2816 LEDs takes about 85 ms).
uint8_t tapCount = 0;          // button 1 quick presses in a row
uint32_t lastTapMs = 0;
void inputTask(void*) {
  for (;;) {
    const uint32_t now = millis();
    for (int i = 0; i < 4; i++) {
      const bool r = digitalRead(buttons[i].pin) == HIGH;   // HIGH = released (INPUT_PULLUP)
      if (r != buttons[i].raw) { buttons[i].raw = r; buttons[i].changed = now; }
      if (r != buttons[i].stable && now - buttons[i].changed >= 40) {   // 40 ms debounce (long wires)
        buttons[i].stable = r;
        if (!r) {                                                   // pressed
          sendCmd(CMD_BUTTON, i);
          // press button 1 three times quickly (each within 0.7 s) → local hotspot "Saliva-Control" on for 10 minutes
          if (i == 0) {
            tapCount = (now - lastTapMs < 700) ? tapCount + 1 : 1;
            lastTapMs = now;
            if (tapCount >= 3) { tapCount = 0; sendCmd(CMD_AP); }
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void processCmds(uint32_t now) {
  Cmd c;
  while (cmdQ && xQueueReceive(cmdQ, &c, 0) == pdTRUE) {
    if (c.type != CMD_SLEEP) wakeUp(now);   // any button, page or website request wakes the installation
    switch (c.type) {
      case CMD_AP:
        // the first of the three presses started Box 1 → undo that, so only the hotspot opens
        if (state == ST_FOCUS && focusBox == 0 && now - focusStartMs < 3000) {
          if (tourPaused) resumeTour(now); else goIdle(now);
          lastButtonMs[0] = 0; lastAcceptMs = 0;
        }
        apUntilMs = now + 10UL * 60 * 1000;
        lastEvent = "hotspot on 10 min";
        Audio::play(CUE_TICK);
        break;
      case CMD_SLEEP:
        lastActivityMs = now - (uint32_t)cfg.sleepMin * 60000UL - 1;   // sleep now
        if (state != ST_IDLE) goIdle(now);
        break;
      case CMD_TOUR:  startTour(now); break;
      case CMD_STOP:  goIdle(now); break;
      case CMD_TEST:  panelTestActive = false; testPattern = !testPattern; break;
      case CMD_PANEL_TEST:
        state = ST_IDLE; stateStartMs = now; tourPaused = false; testPattern = false;
        resetCueClocks(); panelTestActive = panelTestPanel < 11;
        lastEvent = panelTestActive ? "panel test" : "panel test off";
        break;
      case CMD_FOCUS: startFocus(c.arg, now, state == ST_TOUR); break;
      case CMD_BUTTON:
        if (cfg.mode == MODE_AUTO) onButton(c.arg, now);
        break;
      case CMD_WEB_TOUR:
        if (cfg.remoteOn && cfg.mode == MODE_AUTO && state == ST_IDLE) { startTour(now); lastEvent = "web: tour"; }
        break;
      case CMD_WEB_BOX:
        if (cfg.remoteOn && cfg.mode == MODE_AUTO && requestBox(c.arg, now)) {
          static const char* W[4] = { "web: box 1", "web: box 2", "web: box 3", "web: box 4" };
          lastEvent = W[c.arg];
        }
        break;
      case CMD_SPK:                         // speaker test: 0 both, 1, 2 — plays the start sound
        applySettings();
        Audio::testSpeaker(c.arg, CUE_TEST);
        break;
      case CMD_APPLY:
        applySettings();
        saveSettings();
        if (cfg.mode == MODE_IDLE && state != ST_IDLE) goIdle(now);
        break;
    }
    statusDirty = true;
  }
}

// ---- rendering ----------------------------------------------------------------
void renderBox(int i, double p, double s, double lv) {
  switch (i) {
    case 0: renderBox1Bottom(p, s, lv); renderBox1Rear(p, s, lv); break;
    case 1: renderBox2Bottom(p, s, lv); renderBox2Rear(p, s, lv); break;
    case 2: renderBox3Bottom(p, s, lv); renderBox3Rear(p, s, lv); break;
    case 3: renderBox4Bottom(p, s, lv); renderBox4Rear(p, s, lv); break;
  }
}

void renderIdle(float sec) {
  renderBox1IdleBottom(0.0, sec, 1.0);
  renderBox1IdleRear(0.0, sec, 1.0);
  for (int i = 1; i < 4; i++) renderBoxIdleGlow(i, sec, 1.0);
  for (int b = 0; b < 3; b++) renderBridgeIdle(b, sec, 1.0);
}

// Relay level for Box n / Bridge n (n = 0..2): full while it plays, dark once Box n+1 has started,
// and back on (finished scene) after Box 4 has finished. Box 4 never switches off.
static float relayLevel(int n, float T) {
  if (n >= 3) return 1.0f;                          // Box 4 stays on to the end
  const float finaleT = boxStart(3) + BOX_DUR[3];
  if (T >= finaleT) return FINALE_IN_S > 0.0f ? fminf(1.0f, (T - finaleT) / FINALE_IN_S) : 1.0f;
  const float off = T - boxStart(n + 1);            // time since the next box started
  if (off < 0.0f) return 1.0f;                      // still playing (or Bridge n still running)
  if (off >= HANDOFF_FADE_S) return 0.0f;           // dark
  return 1.0f - off / HANDOFF_FADE_S;               // short fade-out
}

void renderTour(float T) {
  const float end = tourEnd();
  const float lv = T > end - TOUR_FADE_S ? (end - T) / TOUR_FADE_S : 1.0f;
  for (int i = 0; i < 4; i++) {
    const float lt = T - boxStart(i);
    if (lt < 0) continue;
    fireBoxCues(i, prevLocal[i], lt);
    prevLocal[i] = lt;
    const float k = lv * relayLevel(i, T);
    if (k > 0.0f) renderBox(i, lt < BOX_DUR[i] ? lt / BOX_DUR[i] : 1.0, lt, k);
  }
  for (int b = 0; b < 3; b++) {
    const float lt = T - bridgeStart(b);
    if (lt < 0) continue;
    if (prevBridge[b] < 0 && !(Audio::hasTrack[Audio::JOURNEY] && T < Audio::trackSeconds(Audio::JOURNEY))) Audio::play(CUE_BRIDGE);
    prevBridge[b] = lt;
    const float k = lv * relayLevel(b, T);          // Bridge n goes dark together with Box n
    if (k <= 0.0f) continue;
    const double pr = lt < BRIDGE_DUR[b] ? lt / BRIDGE_DUR[b] : 1.0;   // >= 1: dim retained scene, still flowing
    if (b == 0)      renderBridge1(pr, lt, k);
    else if (b == 1) renderBridge2(pr, lt, k);
    else if (b == 2) renderBridge3(pr, lt, k);
  }
}

// returns false when the focus has finished
bool renderFocus(float ft) {
  const float D = BOX_DUR[focusBox], end = focusEnd(focusBox);
  if (ft >= end) return false;
  const float lv = ft > D + FOCUS_HOLD_S ? 1.0f - (ft - D - FOCUS_HOLD_S) / FOCUS_FADE_S : 1.0f;
  fireBoxCues(focusBox, prevLocal[focusBox], ft);
  prevLocal[focusBox] = ft;
  renderBox(focusBox, ft < D ? ft / D : 1.0, ft, lv);
  return true;
}

// Which sound file plays now and from where (track 0–3 = a lab, 4 = the journey soundtrack, -1 = none).
// Used for the speaker and sent to the visitor website, so phones can play the same sound in sync.
void soundAt(uint32_t now, int& wb, float& wp, float& we) {   // we = where this part of the file ends
  wb = -1; wp = 0.0f; we = 0.0f;
  if (sleeping || panelTestActive) return;
  if (state == ST_TOUR) {
    const float T = (now - tourStartMs) / 1000.0f;
    if (Audio::hasTrack[Audio::JOURNEY] && T < Audio::trackSeconds(Audio::JOURNEY)) { wb = Audio::JOURNEY; wp = T; we = Audio::trackSeconds(Audio::JOURNEY); }   // one file, straight through
    else for (int i = 0; i < 4; i++) { const float lt = T - boxStart(i); if (lt >= 0 && lt < BOX_DUR[i] && !journeyCovers(i)) { wb = i; wp = lt; we = BOX_DUR[i]; } }
  } else if (state == ST_FOCUS) {
    const float ft = (now - focusStartMs) / 1000.0f;
    if (journeyCovers(focusBox)) { if (ft < BOX_DUR[focusBox]) { wb = Audio::JOURNEY; wp = boxStart(focusBox) + ft; we = boxStart(focusBox) + BOX_DUR[focusBox]; } }   // this box's part of the soundtrack
    else { wb = focusBox; wp = ft; we = BOX_DUR[focusBox]; }
  }
}

// Test pattern — check every panel's direction before judging the animations.
void drawTestPattern() {
  for (int i = 0; i < 4; i++) {
    const uint8_t pb = BOTTOM[i], pr = REAR[i];
    put(pb, 0, 15, CRGB(255, 0, 0), 0.6f);    // under-light: front-left RED
    put(pb, 15, 15, CRGB(0, 255, 0), 0.6f);   //              front-right GREEN
    put(pb, 0, 0, CRGB(0, 0, 255), 0.6f);     //              rear-left BLUE
    put(pr, 0, 0, CRGB(255, 0, 0), 0.6f);     // backlight:   top-left RED
    put(pr, 15, 0, CRGB(0, 255, 0), 0.6f);    //              top-right GREEN
    put(pr, 0, 15, CRGB(0, 0, 255), 0.6f);    //              bottom-left BLUE
    for (int k = 0; k <= pb && k < 16; k++) put(pb, k, 7, CRGB(255, 255, 255), 0.25f);   // (index + 1) dots
    for (int k = 0; k <= pr && k < 16; k++) put(pr, k, 7, CRGB(255, 255, 255), 0.25f);
  }
  for (int b = 0; b < 3; b++) {
    for (int y = 0; y < 8; y++) { putBridge(b, 0, y, CRGB(255, 0, 0), 0.5f); putBridge(b, 31, y, CRGB(0, 255, 0), 0.5f); }
    for (int x = 1; x < 31; x++) putBridge(b, x, 0, CRGB(0, 0, 255), 0.15f);   // top row BLUE
    for (int k = 0; k <= b; k++) putBridge(b, 3 + k * 2, 4, CRGB(255, 255, 255), 0.4f);
  }
  if (hall.active())
    for (int y = 7; y <= 8; y++)
      for (int x = 7; x <= 8; x++) put(REAR[0], x, y, CRGB(0, 255, 0), 0.8f);
}

// Developer calibration: light one complete panel, one row, or one column.
// axis 0 = whole panel, 1 = horizontal row, 2 = vertical column.
void drawPanelTest() {
  const uint8_t p = panelTestPanel;
  if (p > 10) return;
  static const CRGB COLORS[8] = {
    CRGB(255,255,255), CRGB(255,0,0), CRGB(0,255,0), CRGB(0,0,255),
    CRGB(255,85,0), CRGB(86,209,194), CRGB(109,51,189), CRGB(255,0,95)
  };
  const CRGB col = COLORS[panelTestColor & 7];
  const float lv = panelTestBrightness / 255.0f;
  if (isBridgePanel(p)) {
    const uint8_t b = p == 2 ? 0 : (p == 5 ? 1 : 2);
    for (int y = 0; y < 8; y++) for (int x = 0; x < 32; x++)
      if (panelTestAxis == 0 || (panelTestAxis == 1 && y == panelTestLine) || (panelTestAxis == 2 && x == panelTestLine))
        putBridge(b, x, y, col, lv);
  } else {
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
      if (panelTestAxis == 0 || (panelTestAxis == 1 && y == panelTestLine) || (panelTestAxis == 2 && x == panelTestLine))
        put(p, x, y, col, lv);
  }
}

// Software power limit per supply (supply A = LEDs 0–1279, supply B = 1280–2815)
void limitSupply(uint16_t from, uint16_t to) {
  uint32_t sum = 0;
  for (uint16_t i = from; i < to; i++) sum += leds[i].r + leds[i].g + leds[i].b;
  const float mA = sum * (20.0f / 255.0f) * (cfg.brightness / 255.0f) + (to - from) * 0.6f;
  if (mA <= SUPPLY_LIMIT_MA) return;
  const uint8_t scale = (uint8_t)(255.0f * SUPPLY_LIMIT_MA / mA);
  for (uint16_t i = from; i < to; i++) leds[i].nscale8(scale);
}

// =============================================================================
//  7) CONTROL PAGE (Wi-Fi access point "Saliva-Control" → http://192.168.4.1)
// =============================================================================
WebServer server(80);

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Saliva 控制台</title>
<style>
body{font-family:-apple-system,system-ui,sans-serif;margin:0;background:#101417;color:#e6ecea}
.w{max-width:520px;margin:0 auto;padding:16px;display:flex;flex-direction:column;gap:14px}
h1{font-size:20px;margin:4px 0}.card{background:#1a2125;border:1px solid #2a3338;border-radius:10px;padding:14px;display:flex;flex-direction:column;gap:10px}
.row{display:flex;gap:8px;flex-wrap:wrap}button{flex:1;min-width:90px;padding:12px;border:0;border-radius:8px;font-size:15px;background:#2c6fbf;color:#fff}
button.b1{background:#e2611c}button.b2{background:#2a9d8f}button.b3{background:#6d33bd}button.b4{background:#d23c8c}button.stop{background:#55606a}
label{font-size:14px;display:flex;justify-content:space-between;align-items:center;gap:10px}input[type=range]{flex:1}
select,input[type=number]{font-size:15px;padding:6px;border-radius:6px;border:1px solid #39444a;background:#101417;color:#e6ecea}
#st{font-family:ui-monospace,monospace;font-size:13px;line-height:1.6;white-space:pre-wrap}.muted{color:#8f9ba0;font-size:13px}
</style></head><body><div class="w">
<h1>Inside a Drop of Saliva · 控制台</h1>
<div class="card"><div class="row"><button onclick="c('tour')">完整导览</button><button class="stop" onclick="c('stop')">停止 / 待机</button></div>
<div class="row"><button class="b1" onclick="c('box1')">Box 1</button><button class="b2" onclick="c('box2')">Box 2</button>
<button class="b3" onclick="c('box3')">Box 3</button><button class="b4" onclick="c('box4')">Box 4</button></div>
<div class="row"><button class="stop" onclick="c('test')">测试图案 开/关</button></div></div>
<div class="card">
<label>模式<select id="mode" onchange="s('mode',this.value)"><option value="0">自动（传感器和按钮有效）</option><option value="1">手动（只用这个页面）</option><option value="2">一直待机</option></select></label>
<label>亮度 <input type="range" id="bri" min="20" max="255" onchange="s('bri',this.value)"><span id="briv"></span></label>
<label>总音量（0 = 最小，100 = 最大） <input type="range" id="vol" min="0" max="100" onchange="s('vol',this.value)"><span id="volv"></span></label>
<label>音响 1 <input type="range" id="s1" min="0" max="100" onchange="s('s1',this.value)"><span id="s1v"></span></label>
<label>音响 2 <input type="range" id="s2" min="0" max="100" onchange="s('s2',this.value)"><span id="s2v"></span></label>
<div class="row"><button class="stop" onclick="c('spk1')">试听 音响 1</button><button class="stop" onclick="c('spk2')">试听 音响 2</button><button class="stop" onclick="c('spk0')">两个一起</button></div>
<label>霍尔灵敏度（越小越灵敏）<input type="number" id="hall" min="50" max="2000" step="10" onchange="s('hall',this.value)"></label>
<label>聚焦锁定秒数 <input type="number" id="lock" min="0" max="60" onchange="s('lock',this.value)"></label>
<label>没人互动几分钟后省电待机（0 = 不用） <input type="number" id="slp" min="0" max="240" onchange="s('slp',this.value)"></label>
<div class="row" id="btns"></div>
<label><span><input type="checkbox" id="remote" onchange="s('remote',this.checked?1:0)"> 观众网站可以点播</span></label>
<div class="muted">设置会自动保存，断电后也不会丢。</div></div>
<div class="card"><div id="st">连接中…</div><a href="/wifi" style="color:#7fb6f0">网络设置（换 Wi-Fi）→</a></div>
<div class="card"><b>日志</b><div id="log" style="font-family:ui-monospace,monospace;font-size:12px;line-height:1.5;white-space:pre-wrap;max-height:260px;overflow:auto;background:#101417;border-radius:6px;padding:8px"></div>
<div class="row"><button class="stop" onclick="if(confirm('重启 ESP32？灯会黑几秒'))fetch('/api/restart',{method:'POST'})">重启 ESP32</button>
<button class="stop" onclick="location.href='/update'">更新程序</button><button class="stop" onclick="location.href='/audio'">实验室音乐</button></div></div></div>
<script>
const g=id=>document.getElementById(id);
var c=(x)=>{fetch('/api/cmd?c='+x).then(r).catch(()=>{})}
var s=(k,v)=>{fetch('/api/set?'+k+'='+encodeURIComponent(v)).then(r).catch(()=>{})}
let first=true;
var r=()=>{fetch('/api/status').then(x=>x.json()).then(d=>{
 if(first){g('mode').value=d.mode;g('bri').value=d.bri;g('vol').value=d.vol;g('s1').value=d.s1;g('s2').value=d.s2;g('hall').value=d.hallTh;g('lock').value=d.lock;g('slp').value=d.slp;
  g('remote').checked=d.remote;g('btns').innerHTML=d.btnOn.map((v,i)=>'<label><input type="checkbox" '+(v?'checked':'')+' onchange="s(\'b'+(i+1)+'\',this.checked?1:0)"> 按钮 '+(i+1)+'</label>').join('');first=false}
 g('briv').textContent=d.bri;g('volv').textContent=d.vol;g('s1v').textContent=d.s1;g('s2v').textContent=d.s2;
 g('st').textContent='状态：'+(d.sleep?'省电待机（简单动画；有人按按钮、转盘或用网站就恢复）':d.state)+'\n本地热点 Saliva-Control：'+(d.ap?'开（'+d.apLeft+'）':'关（快速连按按钮 1 三下可以打开）')+'\n'+'\n最近事件：'+d.event+'\n时间：'+d.t.toFixed(1)+' s\n霍尔：'+d.hall+'（静止 '+d.hallBase+'）'+(d.magnet?'  磁铁!':'')+'\n按钮：'+d.btn.map(b=>b?'●':'○').join(' ')+'\n测试图案：'+(d.test?'开':'关')+'\n芯片温度：'+d.temp+' °C'+(d.temp>=75?'  ⚠ 太热':d.temp>=60?'（偏热，正常范围）':'（正常）')+'\n开机：'+d.boot+'，已运行 '+Math.floor(d.up/3600)+' 小时 '+Math.floor(d.up/60%60)+' 分'+'\n上网：'+(d.wifi?'已连上 '+d.ssid+' '+d.ip:'未连上')+'\n云端（观众网站）：'+(d.mqtt?'已连上':'未连上')})
 .catch(()=>{g('st').textContent='连不上 ESP32'})}
var lg=()=>{fetch('/api/log').then(x=>x.text()).then(t=>{const e=g('log');const end=e.scrollTop+e.clientHeight>=e.scrollHeight-8;e.textContent=t;if(end)e.scrollTop=e.scrollHeight}).catch(()=>{})}
r();lg();setInterval(r,1000);setInterval(lg,3000);
</script></body></html>)HTML";

bool mqttConnected();   // defined in the cloud section
String jsonEsc(const String& in);
void handleStatus() {
  const uint32_t now = millis();
  static const char* ST[3] = { "待机", "导览", "聚焦" };
  float t = (now - stateStartMs) / 1000.0f;
  if (state == ST_TOUR) t = (now - tourStartMs) / 1000.0f;
  String j = "{";
  j += "\"sleep\":" + String(sleeping ? "true" : "false") + ",\"slp\":" + String(cfg.sleepMin) + ",";
  {
    const long apLeft = (long)(apUntilMs - now) / 60000;
    j += "\"ap\":" + String(apOn ? "true" : "false") + ",\"apLeft\":\"";
    j += (WiFi.status() != WL_CONNECTED) ? String("没连上网时一直开") : (apLeft > 0 ? String(apLeft + 1) + " 分钟后自动关" : String("马上关"));
    j += "\",";
  }
  j += "\"state\":\""; j += ST[state]; if (state == ST_FOCUS) { j += " Box "; j += String(focusBox + 1); if (tourPaused) j += "（导览暂停中）"; } j += "\",";
  j += "\"event\":\""; j += lastEvent; j += "\",";
  j += "\"t\":" + String(t, 1) + ",";
  j += "\"hall\":" + String(hall.raw()) + ",\"hallBase\":" + String(hall.baseline()) + ",\"magnet\":" + String(hall.active() ? "true" : "false") + ",";
  j += "\"btn\":[";
  for (int i = 0; i < 4; i++) { j += buttons[i].stable ? "0" : "1"; if (i < 3) j += ","; }
  j += "],\"btnOn\":[";
  for (int i = 0; i < 4; i++) { j += cfg.buttonOn[i] ? "1" : "0"; if (i < 3) j += ","; }
  j += "],\"mode\":" + String(cfg.mode) + ",\"bri\":" + String(cfg.brightness) + ",\"vol\":" + String(cfg.volume);
  j += ",\"s1\":" + String(cfg.spk1) + ",\"s2\":" + String(cfg.spk2);
  j += ",\"hallTh\":" + String(cfg.hallThreshold) + ",\"lock\":" + String(cfg.focusLockS) + ",\"test\":" + String(testPattern ? "true" : "false");
  j += ",\"remote\":" + String(cfg.remoteOn ? "true" : "false");
  j += ",\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  j += ",\"ssid\":\"" + jsonEsc(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("")) + "\"";
  j += ",\"ip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("-")) + "\"";
  j += ",\"boot\":\"" + String(resetReasonText()) + "\",\"up\":" + String(millis() / 1000);
  j += ",\"temp\":" + String((int)temperatureRead());
  j += ",\"mqtt\":" + String(mqttConnected() ? "true" : "false") + "}";
  server.send(200, "application/json", j);
}

void handleCmd() {
  const String c = server.arg("c");
  if (c == "tour") sendCmd(CMD_TOUR);
  else if (c == "stop") sendCmd(CMD_STOP);
  else if (c == "test") sendCmd(CMD_TEST);
  else if (c.length() == 4 && c.startsWith("box") && c[3] >= '1' && c[3] <= '4') sendCmd(CMD_FOCUS, c[3] - '1');
  else if (c.length() == 4 && c.startsWith("spk") && c[3] >= '0' && c[3] <= '2') sendCmd(CMD_SPK, c[3] - '0');
  handleStatus();
}

void handleSet() {
  if (server.hasArg("bri"))  cfg.brightness = constrain(server.arg("bri").toInt(), 20, 255);
  if (server.hasArg("vol"))  cfg.volume = constrain(server.arg("vol").toInt(), 0, 100);
  if (server.hasArg("s1"))   cfg.spk1 = constrain(server.arg("s1").toInt(), 0, 100);
  if (server.hasArg("s2"))   cfg.spk2 = constrain(server.arg("s2").toInt(), 0, 100);
  if (server.hasArg("slp"))  cfg.sleepMin = constrain(server.arg("slp").toInt(), 0, 240);
  if (server.hasArg("mode")) cfg.mode = constrain(server.arg("mode").toInt(), 0, 2);
  if (server.hasArg("hall")) { cfg.hallThreshold = constrain(server.arg("hall").toInt(), 50, 2000); hall.threshold = cfg.hallThreshold; }
  if (server.hasArg("lock")) cfg.focusLockS = constrain(server.arg("lock").toInt(), 0, 60);
  for (int i = 0; i < 4; i++) { char k[4] = { 'b', (char)('1' + i), 0 }; if (server.hasArg(k)) cfg.buttonOn[i] = server.arg(k).toInt() != 0; }
  if (server.hasArg("remote")) cfg.remoteOn = server.arg("remote").toInt() != 0;
  sendCmd(CMD_APPLY);   // brightness / volume / saving happen on the LED loop between frames
  handleStatus();
}


// =============================================================================
//  7b) SCHOOL WI-FI + CLOUD RELAY (visitor website ⇄ HiveMQ ⇄ this ESP32)
//      Topics:  <MQTT_TOPIC>/cmd     ← "tour" | "box1".."box4"   (from the website)
//               <MQTT_TOPIC>/status  → {"s":"idle|tour|focus","b":1-4,"left":s,"lock":s,"paused":0/1,"on":0/1}  (retained)
// =============================================================================
WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);
uint32_t lastWifiTry = 0, lastMqttTry = 0, lastStatusMs = 0;

// ---- Saved networks (Preferences namespace "net"; passwords never leave the ESP32) ----
struct NetCfg { String ssid, user, pass; bool ent; };   // ent = school-style login (802.1X / PEAP)
const int NET_MAX = 3;
NetCfg nets[NET_MAX];
int netCount = 0;
int netTry = -1;            // which saved network we are trying / using
bool wasConnected = false;
// ---- why the connection drops (shown on the developer console) ----
volatile uint8_t lastDiscReason = 0;     // Wi-Fi reason code from the router (8 = we left, 200+ = beacon/handshake lost)
uint16_t wifiDrops = 0, mqttDrops = 0;
int lastRssi = 0, dropRssi = 0;
bool mqttWasUp = false;
uint32_t liveSlowUntil = 0;
// ---- radio power: low and cool by default, stronger only when the signal is weak, always low when hot ----
const int RSSI_WEAK = -75, RSSI_GOOD = -68;     // dBm
const int TEMP_HOT = 70, TEMP_OK = 62;          // °C, chip sensor
uint8_t radioLevel = 0;                         // 0 = 15 dBm (normal) · 1 = 19.5 dBm (weak signal) · 2 = 11 dBm (too hot)
uint32_t weakSince = 0;
bool tooHot = false;
void setRadio(uint8_t lvl, const char* why) {
  if (lvl == radioLevel) return;
  radioLevel = lvl;
  WiFi.setTxPower(lvl == 1 ? WIFI_POWER_19_5dBm : lvl == 2 ? WIFI_POWER_11dBm : WIFI_POWER_15dBm);
  logLine(String("无线功率 → ") + (lvl == 1 ? "19.5 dBm" : lvl == 2 ? "11 dBm" : "15 dBm") + "（" + why + "）");
}
void manageRadio(uint32_t now) {               // called about every second from the network task
  static uint32_t last = 0;
  if (now - last < 1000) return;
  last = now;
  const int t = (int)temperatureRead();
  if (!tooHot && t >= TEMP_HOT) { tooHot = true; liveSlowUntil = now + 600000; }
  if (tooHot && t <= TEMP_OK) tooHot = false;
  if (tooHot) { setRadio(2, "芯片太热，降低功率"); return; }
  if (lastRssi && lastRssi < RSSI_WEAK) { if (!weakSince) weakSince = now; }
  else if (lastRssi > RSSI_GOOD) weakSince = 0;
  if (weakSince && now - weakSince > 30000) setRadio(1, "信号弱，加大功率");
  else if (!weakSince) setRadio(0, "信号够好，省电");
}
String lastDrop = "-";
const char* discText(uint8_t r) {
  switch (r) {
    case 2: case 15: case 204: return "认证超时（信号弱或学校网登录慢）";
    case 3: case 8: return "设备自己断开";
    case 4: return "长时间没有活动";
    case 6: case 7: return "路由器不认这台设备了";
    case 23: case 202: return "认证失败（用户名/密码）";
    case 200: return "收不到路由器信号（信号太弱或被挡）";
    case 201: return "找不到这个网络";
    case 203: return "连接被拒绝";
    case 205: return "连接超时";
    default: return "其他";
  }
}
String devKey;              // developer key for the remote console (set on the local Wi-Fi page)
String netNote = "";        // last message for the Wi-Fi page

bool mqttConnected() { return mqtt.connected(); }

void saveNets() {
  prefs.begin("net", false);
  prefs.clear();
  prefs.putUChar("n", netCount);
  for (int i = 0; i < netCount; i++) {
    const String k = String(i);
    prefs.putString(("s" + k).c_str(), nets[i].ssid);
    prefs.putString(("u" + k).c_str(), nets[i].user);
    prefs.putString(("p" + k).c_str(), nets[i].pass);
    prefs.putBool(("e" + k).c_str(), nets[i].ent);
  }
  prefs.end();
}

void loadNetCreds() {
  prefs.begin("net", true);
  if (prefs.isKey("n")) {
    netCount = min((int)prefs.getUChar("n", 0), NET_MAX);
    for (int i = 0; i < netCount; i++) {
      const String k = String(i);
      nets[i].ssid = prefs.getString(("s" + k).c_str(), "");
      nets[i].user = prefs.getString(("u" + k).c_str(), "");
      nets[i].pass = prefs.getString(("p" + k).c_str(), "");
      nets[i].ent  = prefs.getBool(("e" + k).c_str(), false);
    }
    prefs.end();
  } else {
    // older program version: only a student ID + password for RMIT-University
    const String u = prefs.getString("user", ""), pw = prefs.getString("pass", "");
    prefs.end();
    netCount = 0;
    if (u.length()) { nets[0] = { "RMIT-University", u, pw, true }; netCount = 1; saveNets(); }
  }
}

void connectNet(int i) {
  if (i < 0 || i >= netCount) return;
  netTry = i;
  const NetCfg& n = nets[i];
  WiFi.disconnect(false);
  delay(50);
  Serial.print(F("Wi-Fi: trying ")); Serial.println(n.ssid);
  if (n.ent) {
    WiFi.begin(n.ssid.c_str(), WPA2_AUTH_PEAP, n.user.c_str(), n.user.c_str(), n.pass.c_str());
  } else {
    esp_wifi_sta_enterprise_disable();      // leave school-login mode before a normal network
    if (n.pass.length()) WiFi.begin(n.ssid.c_str(), n.pass.c_str());
    else WiFi.begin(n.ssid.c_str());       // open network (pages that need a browser login won't work)
  }
  netNote = "正在连接 " + n.ssid + " …";
  lastWifiTry = millis();
}

void startSchoolWifi() {   // try the next saved network
  if (netCount == 0) { netNote = "还没有保存网络"; Serial.println(F("Wi-Fi: no saved network — open http://192.168.4.1/wifi")); return; }
  connectNet((netTry + 1) % netCount);
}

String readSerialLine() {   // used only by the w command (LEDs pause while typing)
  while (true) {
    if (Serial.available()) { String s = Serial.readStringUntil('\n'); s.trim(); if (s.length()) return s; }
    delay(20);
  }
}

// add or replace a network; the newest one becomes first choice
void addNet(const String& ssid, bool ent, const String& user, const String& pass) {
  int found = -1;
  for (int i = 0; i < netCount; i++) if (nets[i].ssid == ssid) found = i;
  NetCfg n = { ssid, user, pass, ent };
  if (found >= 0 && pass.length() == 0) n.pass = nets[found].pass;   // empty password = keep the old one
  if (found < 0) found = (netCount < NET_MAX) ? netCount++ : NET_MAX - 1;  // full → replace the last one
  for (int i = found; i > 0; i--) nets[i] = nets[i - 1];
  nets[0] = n;
  saveNets();
  connectNet(0);
}

void enterNetCreds() {      // Serial backup: school network only
  Serial.println(F("Student ID (e.g. s1234567), then Enter:"));
  const String u = readSerialLine();
  Serial.println(F("RMIT password, then Enter (saved inside the ESP32 so it can reconnect after power cuts):"));
  const String pw = readSerialLine();
  addNet("RMIT-University", true, u, pw);
  Serial.println(F("Saved as first choice."));
}

void forgetNetCreds() {
  prefs.begin("net", false);
  prefs.clear();
  prefs.putUChar("n", 0);
  prefs.end();
  netCount = 0; netTry = -1;
  WiFi.disconnect(false);
  netNote = "所有网络已删除";
  Serial.println(F("All saved networks erased."));
}

void publishStatus() {
  if (!mqtt.connected()) return;
  const uint32_t now = millis();
  const char* S[3] = { "idle", "tour", "focus" };
  float left = 0, lock = 0;
  if (state == ST_TOUR) left = tourEnd() - (now - tourStartMs) / 1000.0f;
  if (state == ST_FOCUS) {
    const float ft = (now - focusStartMs) / 1000.0f;
    left = focusEnd(focusBox) - ft;
    lock = cfg.focusLockS - ft;
  }
  if (left < 0) left = 0;
  if (lock < 0) lock = 0;
  // sound for the visitors' phones: track + position + the exact clock time it was measured (ms since 1970)
  int wb; float wp, we;
  soundAt(now, wb, wp, we);
  struct timeval tv; gettimeofday(&tv, nullptr);
  const long long ms = (tv.tv_sec > 1700000000) ? (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000 : 0;   // 0 = clock not set yet
  char buf[300];
  snprintf(buf, sizeof(buf), "{\"s\":\"%s\",\"b\":%d,\"left\":%d,\"lock\":%d,\"paused\":%d,\"on\":%d,\"z\":%d,\"ts\":%ld,\"at\":%d,\"ap\":%.2f,\"ae\":%.2f,\"ms\":%lld}",
           S[state], state == ST_FOCUS ? focusBox + 1 : 0, (int)(left + 0.5f), (int)(lock + 0.5f), tourPaused ? 1 : 0,
           (cfg.remoteOn && cfg.mode == MODE_AUTO) ? 1 : 0, sleeping ? 1 : 0, (long)time(nullptr), wb, wp, we, ms);
  const String topic = String(MQTT_TOPIC) + "/status";
  mqtt.publish(topic.c_str(), buf, true);
  lastStatusMs = now;
}

void onDevCmd(const String& msg);   // remote developer console (below)
void onMqtt(char* topic, byte* payload, unsigned int len) {
  if (String(topic).endsWith("/watch")) { watchUntilMs = millis() + 30000; return; }   // a website is showing the live view
  if (String(topic).endsWith("/dev/cmd")) {
    String m; m.reserve(len);
    for (unsigned int i = 0; i < len; i++) m += (char)payload[i];
    onDevCmd(m);
    return;
  }
  char cmd[12];
  const unsigned int n = len < sizeof(cmd) - 1 ? len : sizeof(cmd) - 1;
  memcpy(cmd, payload, n);
  cmd[n] = 0;
  if (!cfg.remoteOn || cfg.mode != MODE_AUTO) { publishStatus(); return; }
  if (strcmp(cmd, "tour") == 0) sendCmd(CMD_WEB_TOUR);          // a tour only starts from waiting
  else if (strlen(cmd) == 4 && strncmp(cmd, "box", 3) == 0 && cmd[3] >= '1' && cmd[3] <= '4')
    sendCmd(CMD_WEB_BOX, cmd[3] - '1');                          // same rules as the physical buttons
}

bool devDirty = true;
uint32_t lastDevMs = 0, lastLogPubMs = 0, logPubSeq = 0;
void publishDev();
void publishLogTail();

void netLoop(uint32_t now) {
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;     // don't interrupt a scan from the Wi-Fi page
  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) {
      wasConnected = false; lastWifiTry = now; netNote = "网络断开，稍后重连";
      wifiDrops++; dropRssi = lastRssi; mqttWasUp = false;
      lastDrop = "Wi-Fi 断开：原因 " + String(lastDiscReason) + " " + discText(lastDiscReason) + "，断开前信号 " + String(dropRssi) + " dBm";
      logLine(lastDrop);
    }
    if (netCount && now - lastWifiTry > 20000) {
      if (netTry >= 0 && netTry < netCount) netNote = nets[netTry].ssid + " 20 秒没连上，换下一个";
      startSchoolWifi();
    }
    return;
  }
  if (!wasConnected) {
    wasConnected = true;
    netNote = "已连上 " + WiFi.SSID();
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // clock for checking developer commands
    Serial.print(F("Wi-Fi: connected to ")); Serial.print(WiFi.SSID());
    Serial.print(F(", IP ")); Serial.println(WiFi.localIP());
  }
  lastRssi = WiFi.RSSI();
  manageRadio(now);
  liveFrameMs = (lastRssi < -75 || (int32_t)(liveSlowUntil - now) > 0) ? 1000 : LIVE_FRAME_MS;   // weak signal → 1 frame/s
  if (!mqtt.connected()) {
    if (mqttWasUp) {
      mqttWasUp = false; mqttDrops++;
      lastDrop = "云端断开（Wi-Fi 还连着）：代码 " + String(mqtt.state()) + "，信号 " + String(lastRssi) + " dBm";
      logLine(lastDrop);
    }
    if (now - lastMqttTry < 10000) return;   // connecting can take a few seconds; only this task waits
    lastMqttTry = now;
    char id[32];
    snprintf(id, sizeof(id), "d2d-esp32-%06lx", (unsigned long)(esp_random() & 0xFFFFFF));
    const String willTopic = String(MQTT_TOPIC) + "/status";
    if (mqtt.connect(id, willTopic.c_str(), 0, true, "{\"s\":\"offline\"}")) {
      const String cmdTopic = String(MQTT_TOPIC) + "/cmd";
      mqtt.subscribe(cmdTopic.c_str());
      mqtt.subscribe((String(MQTT_TOPIC) + "/dev/cmd").c_str());
      mqtt.subscribe((String(MQTT_TOPIC) + "/watch").c_str());
      netNote = "云端已连上";
      mqttWasUp = true;
      publishStatus();
      devDirty = true; logPubSeq = 0;
    }
    return;
  }
  mqtt.loop();
  if (statusDirty || now - lastStatusMs > 3000) { statusDirty = false; publishStatus(); }
  if (devDirty || now - lastDevMs > 10000) publishDev();
  if (logPubSeq != logSeq && now - lastLogPubMs > 1000) publishLogTail();
  if (liveReady >= 0) {
    if (!mqtt.publish((String(MQTT_TOPIC) + "/live").c_str(), liveBuf[liveReady], sizeof(liveBuf[0]), false))
      liveSlowUntil = now + 30000;          // the link is struggling: send fewer pictures for 30 s
    liveReady = -1;
  }
}

// =============================================================================
//  7c) WI-FI MANAGER PAGE  (http://192.168.4.1/wifi — only reachable after joining "Saliva-Control")
// =============================================================================
static const char WIFI_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Saliva 网络设置</title>
<style>
body{font-family:-apple-system,system-ui,sans-serif;margin:0;background:#101417;color:#e6ecea}
.w{max-width:520px;margin:0 auto;padding:16px;display:flex;flex-direction:column;gap:14px}
h1{font-size:20px;margin:4px 0}h2{font-size:15px;margin:0}a{color:#7fb6f0}
.card{background:#1a2125;border:1px solid #2a3338;border-radius:10px;padding:14px;display:flex;flex-direction:column;gap:10px}
button{padding:10px 12px;border:0;border-radius:8px;font-size:15px;background:#2c6fbf;color:#fff}button.g{background:#55606a}button.r{background:#a8423a}
input,select{font-size:16px;padding:8px;border-radius:6px;border:1px solid #39444a;background:#101417;color:#e6ecea;width:100%;box-sizing:border-box}
label{font-size:14px;display:flex;flex-direction:column;gap:4px}.muted{color:#8f9ba0;font-size:13px}
.it{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:8px 0;border-top:1px solid #2a3338}
.it:first-child{border-top:0}.it b{font-weight:600}.ok{color:#5fd39a}.bad{color:#f0a37f}
#st{font-family:ui-monospace,monospace;font-size:13px;line-height:1.6;white-space:pre-wrap}
</style></head><body><div class="w">
<h1>网络设置</h1><a href="/">← 回控制台</a>
<div class="card"><h2>现在</h2><div id="st">读取中…</div></div>
<div class="card"><h2>已保存的网络（按顺序尝试，最多 3 个）</h2><div id="saved"></div>
<div class="muted">断网时每 20 秒换下一个。密码只存在 ESP32 里，页面上不会显示。</div></div>
<div class="card"><h2>添加 / 修改网络</h2>
<div><button class="g" onclick="scan()">扫描附近网络</button> <span id="sc" class="muted"></span></div>
<div id="list"></div>
<label>网络名称（SSID）<input id="ssid" autocapitalize="off" autocorrect="off"></label>
<label>类型<select id="ent" onchange="u()"><option value="0">普通 Wi-Fi（只要密码：手机热点、家里、4G 路由器）</option>
<option value="1">学校登录（用户名 + 密码：RMIT-University、eduroam）</option></select></label>
<label id="ul">用户名（RMIT-University 填学号 s1234567；eduroam 一般填 s1234567@student.rmit.edu.au，以 RMIT 说明为准）<input id="user" autocapitalize="off" autocorrect="off"></label>
<label>密码（修改已保存的网络时可以留空 = 用旧密码）<input id="pass" type="password"></label>
<button onclick="add()">保存并连接（成为第一选择）</button></div>
<div class="card"><h2>远程开发者控制台密钥</h2>
<div class="muted">在网站 dev.html 上远程看日志、换已保存的网络、更新程序时要用。至少 8 个字，自己记住；它只存在 ESP32 和你的浏览器里，不会经过云端。</div>
<div id="ks" class="muted"></div>
<label>新密钥<input id="dk" type="password" autocomplete="new-password"></label>
<button onclick="if(g('dk').value.length<8){alert('至少 8 个字');return}post('/api/devkey','k='+encodeURIComponent(g('dk').value));g('dk').value=''">保存密钥</button></div>
<div class="card"><button class="r" onclick="if(confirm('删除所有保存的网络？'))post('/api/net/clear','')">删除全部网络</button></div>
</div><script>
const g=id=>document.getElementById(id);
const esc=t=>String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
var u=()=>{g('ul').style.display=g('ent').value=='1'?'flex':'none'};u();
var post=(url,body)=>{return fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body}).then(r).catch(()=>{})}
var add=()=>{const s=g('ssid').value.trim();if(!s){alert('请填网络名称');return}
 post('/api/net/add',new URLSearchParams({ssid:s,ent:g('ent').value,user:g('user').value.trim(),pass:g('pass').value}).toString());g('pass').value=''}
var pick=(s,e)=>{if(/^(RMIT-University|eduroam)$/.test(s))e=1;g('ssid').value=s;g('ent').value=e?'1':'0';u();(e?g('user'):g('pass')).focus()}
var scan=()=>{g('sc').textContent='扫描中…（约 5 秒，手机可能会短暂断开，再点一次就好）';
 fetch('/api/scan').then(x=>x.json()).then(d=>{if(d.running){setTimeout(scan,1500);return}
  const seen={};const L=d.list.filter(n=>n.s&&!seen[n.s]&&(seen[n.s]=1)).sort((a,b)=>b.r-a.r);
  g('sc').textContent='找到 '+L.length+' 个，点名字填入';
  g('list').innerHTML=L.map(n=>'<div class="it"><span><b>'+esc(n.s)+'</b> <span class="muted">'+n.r+' dBm'+(n.e?' · 学校登录':n.o?' · 无密码':'')+'</span></span><button class="g" onclick="pick(this.dataset.s,'+(n.e?1:0)+')" data-s="'+esc(n.s)+'">选</button></div>').join('')})
 .catch(()=>{g('sc').textContent='扫描失败，再试一次'})}
var r=()=>{fetch('/api/net').then(x=>x.json()).then(d=>{
 g('st').innerHTML=(d.on?'<span class="ok">● 已连上 '+esc(d.ssid)+'</span>\nIP '+d.ip+'　信号 '+d.rssi+' dBm':'<span class="bad">● 没有连上网络</span>')+
  '\n云端（观众网站）：'+(d.cloud?'<span class="ok">已连上</span>':'<span class="bad">未连上</span>')+'\n'+esc(d.note)+'\nESP32 MAC：'+d.mac+'\n程序版本：'+esc(d.fw);
 g('ks').textContent=d.key?'✓ 已设置密钥（可以远程控制）':'还没设置 → 远程控制台只能看，不能操作';
 g('saved').innerHTML=d.saved.length?d.saved.map((n,i)=>'<div class="it"><span>'+(i+1)+'. <b>'+esc(n.s)+'</b> <span class="muted">'+(n.e?'学校登录 · '+esc(n.u):'普通')+'</span></span><span>'+
  (i?'<button class="g" onclick="post(\'/api/net/first\',\'i='+i+'\')">设为首选</button> ':'')+'<button class="r" onclick="post(\'/api/net/del\',\'i='+i+'\')">删</button></span></div>').join(''):'<div class="muted">还没有。</div>'
}).catch(()=>{g('st').textContent='连不上 ESP32（手机要连着 Saliva-Control）'})}
r();setInterval(r,2000);
</script></body></html>)HTML";

String jsonEsc(const String& in) {
  String o;
  for (unsigned int i = 0; i < in.length(); i++) {
    const char c = in[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o;
}

void handleNetStatus() {
  const bool on = WiFi.status() == WL_CONNECTED;
  String j = "{\"on\":" + String(on ? "true" : "false");
  j += ",\"ssid\":\"" + jsonEsc(on ? WiFi.SSID() : String("")) + "\"";
  j += ",\"ip\":\"" + (on ? WiFi.localIP().toString() : String("-")) + "\"";
  j += ",\"rssi\":" + String(on ? WiFi.RSSI() : 0);
  j += ",\"cloud\":" + String(mqttConnected() ? "true" : "false");
  j += ",\"note\":\"" + jsonEsc(netNote) + "\"";
  j += ",\"key\":" + String(devKey.length() ? "true" : "false") + ",\"fw\":\"" + String(FW_VERSION) + "\"";
  j += ",\"mac\":\"" + WiFi.macAddress() + "\",\"saved\":[";
  for (int i = 0; i < netCount; i++) {
    if (i) j += ",";
    j += "{\"s\":\"" + jsonEsc(nets[i].ssid) + "\",\"e\":" + String(nets[i].ent ? 1 : 0) + ",\"u\":\"" + jsonEsc(nets[i].user) + "\"}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void handleScan() {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) { server.send(200, "application/json", "{\"running\":true}"); return; }
  if (n < 0) {                                   // not started yet (or failed) → start one
    if (WiFi.status() != WL_CONNECTED) WiFi.disconnect(false);   // a half-finished login blocks scanning
    lastWifiTry = millis();
    WiFi.scanNetworks(true);
    server.send(200, "application/json", "{\"running\":true}");
    return;
  }
  String j = "{\"list\":[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    const wifi_auth_mode_t a = WiFi.encryptionType(i);
    j += "{\"s\":\"" + jsonEsc(WiFi.SSID(i)) + "\",\"r\":" + String(WiFi.RSSI(i));
    // enterprise (802.1X) networks: WPA2-Ent = 5, WPA3-Ent-192 = 10, WPA3-Ent = 14, WPA2/WPA3-Ent = 15 (newer cores)
    const int ai = (int)a;
    const bool ent = ai == 5 || ai == 10 || ai == 14 || ai == 15 || WiFi.SSID(i) == "RMIT-University" || WiFi.SSID(i) == "eduroam";
    j += ",\"e\":" + String(ent ? 1 : 0) + ",\"o\":" + String(a == WIFI_AUTH_OPEN ? 1 : 0) + "}";
  }
  j += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

void handleNetAdd() {
  const String ssid = server.arg("ssid");
  if (ssid.length() == 0 || ssid.length() > 32) { server.send(400, "text/plain", "bad ssid"); return; }
  addNet(ssid, server.arg("ent") == "1", server.arg("user"), server.arg("pass"));
  handleNetStatus();
}

void handleNetDel() {
  const int i = server.arg("i").toInt();
  if (i >= 0 && i < netCount) {
    for (int k = i; k < netCount - 1; k++) nets[k] = nets[k + 1];
    netCount--;
    saveNets();
    if (i == netTry || netCount == 0) { WiFi.disconnect(false); netTry = -1; wasConnected = false; lastWifiTry = 0; }
    netNote = "已删除";
  }
  handleNetStatus();
}

void handleNetFirst() {
  const int i = server.arg("i").toInt();
  if (i > 0 && i < netCount) {
    const NetCfg n = nets[i];
    for (int k = i; k > 0; k--) nets[k] = nets[k - 1];
    nets[0] = n;
    saveNets();
    connectNet(0);
  }
  handleNetStatus();
}

void handleNetClear() { forgetNetCreds(); handleNetStatus(); }

// =============================================================================
//  7d) UPDATE + LOG + RESTART  (no USB needed)
// =============================================================================
static const char UPDATE_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Saliva 更新程序</title>
<style>body{font-family:-apple-system,system-ui,sans-serif;margin:0;background:#101417;color:#e6ecea}
.w{max-width:520px;margin:0 auto;padding:16px;display:flex;flex-direction:column;gap:14px}a{color:#7fb6f0}
.card{background:#1a2125;border:1px solid #2a3338;border-radius:10px;padding:14px;display:flex;flex-direction:column;gap:10px}
button{padding:12px;border:0;border-radius:8px;font-size:15px;background:#2c6fbf;color:#fff}.muted{color:#8f9ba0;font-size:13px;line-height:1.6}
progress{width:100%;height:18px}</style></head><body><div class="w">
<h1 style="font-size:20px;margin:4px 0">更新程序</h1><a href="/">← 回控制台</a>
<div class="card"><div class="muted">1. Arduino IDE：Sketch → Export Compiled Binary（导出已编译的二进制文件）<br>
2. 在程序文件夹 build 里选 <b>D2D_Saliva_Full.ino.bin</b>（不要选 bootloader / merged / partitions）<br>
3. 上传时灯会关掉，约 30–60 秒，完成后自动重启。不要关页面、不要断电。</div>
<input type="file" id="f" accept=".bin"><button onclick="up()">上传并更新</button>
<progress id="p" max="100" value="0"></progress><div id="m" class="muted"></div></div></div>
<script>
var up=()=>{const f=document.getElementById('f').files[0],m=document.getElementById('m');if(!f){m.textContent='先选 .bin 文件';return}
 if(!/\.bin$/i.test(f.name)||/bootloader|partitions|merged/i.test(f.name)){m.textContent='这个文件不对，要选 D2D_Saliva_Full.ino.bin';return}
 const x=new XMLHttpRequest(),d=new FormData();d.append('fw',f,f.name);
 x.upload.onprogress=e=>{if(e.lengthComputable)document.getElementById('p').value=e.loaded/e.total*100};
 x.onload=()=>{m.textContent=x.status==200?'✓ 更新成功，ESP32 正在重启，约 15 秒后回控制台':'✗ 失败：'+x.responseText+'（旧程序还在，没坏）';if(x.status==200)setTimeout(()=>location.href='/',15000)};
 x.onerror=()=>{m.textContent='✗ 连接断了，回控制台看日志；旧程序还在'};
 m.textContent='上传中…';x.open('POST','/update');x.send(d)}
</script></body></html>)HTML";

// =============================================================================
//  7f) MUSIC PAGE  (http://192.168.4.1/audio, or the ESP32's address on the same Wi-Fi)
//      The browser converts any audio file (wav / mp3 / m4a …) to 22050 Hz mono 16-bit WAV,
//      then uploads it as /boxN.wav. Max 60 s per lab.
// =============================================================================
static const char AUDIO_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Saliva 音乐</title>
<style>body{font-family:-apple-system,system-ui,sans-serif;margin:0;background:#fff;color:#00004d}
.w{max-width:560px;margin:0 auto;padding:16px;display:flex;flex-direction:column;gap:14px}a{color:#6d33bd}
.card{border:1.5px solid #e3e3ee;border-left:6px solid var(--c);border-radius:14px;padding:14px;display:flex;flex-direction:column;gap:8px}
h1{font-size:22px;margin:4px 0}b{font-size:17px}.muted{color:#55557f;font-size:13px;line-height:1.6}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
button{padding:10px 12px;border:0;border-radius:10px;font-size:14px;font-weight:600;background:#00004d;color:#fff}
button.g{background:#f5f5fa;color:#00004d;border:1.5px solid #e3e3ee}progress{width:100%;height:14px}</style></head><body><div class="w">
<h1>每个实验室的音乐</h1><a href="/">← 回控制台</a>
<div class="muted">选一个音频文件（wav、mp3、m4a 都可以），网页会自动转成 ESP32 能播的格式（22050 Hz 单声道），再上传。每个实验室最长 60 秒；盒子播放时音乐从头开始，盒子结束时淡出。<br><b>完整旅程</b>（最下面）是一整段声音，Box 1 开始时播放、一直播到底（最长 135 秒），会代替 Box 1–4 和光桥的电子提示音。有它的时候，Box 1–4 单独播放会播它对应的那一段。</div>
<div id="fs" class="muted"></div><div id="list"></div></div>
<script>
const g=id=>document.getElementById(id);
const LABS=[['#ff5500','Box 1 · 图案 Pattern'],['#56d1c2','Box 2 · 感知 Sense'],['#6d33bd','Box 3 · 连接 Connect'],['#ff005f','Box 4 · 组装 Assemble'],['#00004d','完整旅程 Box 1 → 4（135 秒，从 Box 1 开始播到底）']];
var draw=(d)=>{g('fs').textContent=d.fs?'存储空间：剩 '+(d.free/1024).toFixed(1)+' MB / 共 '+(d.total/1024).toFixed(1)+' MB':'⚠ 没有存储分区：Tools → Partition Scheme 选 16M Flash (3MB APP/9.9MB FATFS) 再上传程序';
 g('list').innerHTML=LABS.map((l,i)=>'<div class="card" style="--c:'+l[0]+'"><b>'+l[1]+'</b><div class="muted" id="st'+i+'">'+(d.boxes[i].sec?'现在：'+d.boxes[i].sec.toFixed(1)+' 秒，'+d.boxes[i].kb+' KB':'现在：没有音乐（用电子提示音）')+'</div>'+
 '<input type="file" accept="audio/*,.wav,.mp3,.m4a" id="f'+i+'"><progress id="p'+i+'" max="100" value="0" hidden></progress><div class="row">'+
 '<button onclick="up('+i+')">转换并上传</button>'+(d.boxes[i].sec?'<button class="g" onclick="act(\'test\','+i+')">在喇叭上试听</button><button class="g" onclick="if(confirm(\'删除这个音乐？\'))act(\'del\','+i+')">删除</button>':'')+'</div></div>').join('')};
var load=()=>fetch('/api/audio').then(r=>r.json()).then(draw);
var act=(a,i)=>fetch('/api/audio/'+a+'?box='+(i+1),{method:'POST'}).then(load);
var toWav=async (file,msg,maxS)=>{msg('读取文件…');const ab=await file.arrayBuffer();
 const AC=window.AudioContext||window.webkitAudioContext;const ac=new AC();const dec=await ac.decodeAudioData(ab);
 const sec=Math.min(dec.duration,maxS),n=Math.floor(sec*22050);msg('转换中（'+sec.toFixed(1)+' 秒）…');
 const oc=new OfflineAudioContext(1,n,22050);const src=oc.createBufferSource();src.buffer=dec;src.connect(oc.destination);src.start();
 const out=(await oc.startRendering()).getChannelData(0);let pk=0;for(let i=0;i<n;i++)pk=Math.max(pk,Math.abs(out[i]));
 const k=pk>0?0.95/pk:1,fade=Math.min(n,11025);
 const buf=new ArrayBuffer(44+n*2),v=new DataView(buf);const w4=(o,s)=>{for(let i=0;i<4;i++)v.setUint8(o+i,s.charCodeAt(i))};
 w4(0,'RIFF');v.setUint32(4,36+n*2,true);w4(8,'WAVE');w4(12,'fmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);
 v.setUint32(24,22050,true);v.setUint32(28,44100,true);v.setUint16(32,2,true);v.setUint16(34,16,true);w4(36,'data');v.setUint32(40,n*2,true);
 for(let i=0;i<n;i++){let x=out[i]*k;if(i>n-fade)x*=(n-i)/fade;x=Math.max(-1,Math.min(1,x));v.setInt16(44+i*2,x*32767,true)}
 return new Blob([buf],{type:'audio/wav'})};
var up=async (i)=>{const f=g('f'+i).files[0],st=g('st'+i),p=g('p'+i);if(!f){st.textContent='先选一个音频文件';return}
 try{const wav=await toWav(f,t=>st.textContent=t,i==4?135:60);st.textContent='上传中…（'+(wav.size/1024/1024).toFixed(1)+' MB）';p.hidden=false;
 const x=new XMLHttpRequest(),d=new FormData();d.append('f',wav,i==4?'journey.wav':'box'+(i+1)+'.wav');
 x.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded/e.total*100};
 x.onload=()=>{st.textContent=x.status==200?'✓ 上传好了':'✗ '+x.responseText;setTimeout(load,800)};
 x.onerror=()=>{st.textContent='✗ 上传中断，再试一次'};x.open('POST','/api/audio/up?box='+(i+1));x.send(d)}
 catch(e){st.textContent='✗ 这个文件转换不了：'+e.message}};
load();
</script></body></html>)HTML";

File audioUpFile;
int audioUpBox = -1;
bool audioUpOk = false;

void handleAudioList() {
  String j = "{\"fs\":" + String(Audio::fsOk ? "true" : "false");
  if (Audio::fsOk) {
    j += ",\"free\":" + String((unsigned long)((FFat.totalBytes() - FFat.usedBytes()) / 1024));
    j += ",\"total\":" + String((unsigned long)(FFat.totalBytes() / 1024));
  }
  j += ",\"boxes\":[";
  for (int b = 0; b < Audio::NTRACK; b++) {
    if (b) j += ",";
    j += "{\"sec\":" + String(Audio::trackSeconds(b), 1) + ",\"kb\":" + String((unsigned long)(Audio::trackLen[b] / 1024)) + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void handleAudioUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    audioUpBox = server.arg("box").toInt() - 1;
    audioUpOk = false;
    if (!Audio::fsOk || audioUpBox < 0 || audioUpBox >= Audio::NTRACK) { audioUpBox = -1; return; }
    Audio::lockBox = audioUpBox;
    delay(60);                                   // let the audio task let go of the file
    audioUpFile = FFat.open("/upload.tmp", "w");
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (audioUpFile) audioUpFile.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    if (!audioUpFile) return;
    audioUpFile.close();
    char path[16]; Audio::trackPath(audioUpBox, path);
    FFat.remove(path);
    FFat.rename("/upload.tmp", path);
    audioUpOk = Audio::scanWav(audioUpBox);
    Audio::lockBox = -1;
    logLine(String(audioUpBox == Audio::JOURNEY ? "声音：完整旅程" : "音乐：Box " + String(audioUpBox + 1)) + (audioUpOk ? " 已更新（" + String(Audio::trackSeconds(audioUpBox), 1) + " 秒）" : " 文件格式不对"));
    devDirty = true;
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (audioUpFile) audioUpFile.close();
    FFat.remove("/upload.tmp");
    Audio::lockBox = -1;
  }
}

void handleAudioUpDone() {
  if (audioUpOk) server.send(200, "text/plain", "OK");
  else server.send(400, "text/plain", Audio::fsOk ? "文件没收到或格式不对" : "没有存储分区");
}

void handleAudioDel() {
  const int b = server.arg("box").toInt() - 1;
  if (Audio::fsOk && b >= 0 && b < Audio::NTRACK) {
    Audio::lockBox = b;
    delay(60);
    char path[16]; Audio::trackPath(b, path);
    FFat.remove(path);
    Audio::scanWav(b);
    Audio::lockBox = -1;
    logLine(String(b == Audio::JOURNEY ? "声音：完整旅程" : "音乐：Box " + String(b + 1)) + " 已删除，改回电子提示音");
    devDirty = true;
  }
  handleAudioList();
}

void handleAudioTest() {
  const int b = server.arg("box").toInt() - 1;
  if (b >= 0 && b < Audio::NTRACK && Audio::hasTrack[b]) { Audio::previewUntil = millis() + 15000; Audio::previewBox = b; }
  handleAudioList();
}

void stopForUpdate() {
  if (updating) return;
  updating = true;
  logLine("开始更新程序，灯暂时关闭");
}

void handleUpdateUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    stopForUpdate();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) logLine("更新：开始失败 " + String(Update.errorString()));
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (Update.isRunning() && Update.write(u.buf, u.currentSize) != u.currentSize) logLine("更新：写入失败 " + String(Update.errorString()));
  } else if (u.status == UPLOAD_FILE_END) {
    if (Update.end(true)) logLine("更新：收到 " + String((unsigned long)(u.totalSize / 1024)) + " KB，检查通过");
    else logLine("更新：失败 " + String(Update.errorString()));
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    logLine("更新：上传中断，继续用旧程序");
    updating = false;
  }
}

void handleUpdateDone() {
  if (!Update.hasError() && Update.isFinished()) {
    server.send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
  } else {
    server.send(500, "text/plain", Update.hasError() ? Update.errorString() : "not finished");
    updating = false;
  }
}

void handleLog() {
  String t;
  if (logMx) xSemaphoreTake(logMx, portMAX_DELAY);
  for (int i = 0; i < logCount; i++) { t += logBuf[(logHead - logCount + i + LOG_N) % LOG_N]; t += '\n'; }
  if (logMx) xSemaphoreGive(logMx);
  server.send(200, "text/plain; charset=utf-8", t);
}

void handleRestart() {
  logLine("页面要求重启");
  server.send(200, "text/plain", "OK");
  delay(300);
  ESP.restart();
}

// =============================================================================
//  7e) REMOTE DEVELOPER CONSOLE (dev.html on the website ⇄ HiveMQ ⇄ here)
//      <topic>/dev/status   → retained JSON: version, network, saved networks, settings, state
//      <topic>/dev/log      → retained text: the last 40 log lines
//      <topic>/dev/cmd      ← "time|nonce|command|argument|signature"   (signature = HMAC-SHA256 with the
//                              developer key; the key is set on http://192.168.4.1/wifi and never sent)
//      <topic>/dev/ack      → {"n":nonce,"ok":true/false,"m":"message"}
//      Commands: tour · stop · test · box 1-4 · spk 0-2 (speaker test) · set bri=..&vol=..&s1=..&s2=.. · net <i> (use saved network i first)
//                del <i> · restart · log · ota <https link to .bin>
//      Adding a NEW network (with its password) is only possible on the local page — passwords never
//      go through the cloud.
// =============================================================================
String usedNonces[16];
int usedNonceHead = 0;

void loadDevKey() { prefs.begin("dev", true); devKey = prefs.getString("k", ""); prefs.end(); }
void saveDevKey(const String& k) { prefs.begin("dev", false); prefs.putString("k", k); prefs.end(); devKey = k; devDirty = true; }

String hmacHex(const String& key, const String& msg) {
  uint8_t out[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t*)key.c_str(), key.length(),
                  (const uint8_t*)msg.c_str(), msg.length(), out);
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", out[i]);
  return String(hex);
}

void devAck(const String& nonce, bool ok, const String& m) {
  const String j = "{\"n\":\"" + jsonEsc(nonce) + "\",\"ok\":" + (ok ? "true" : "false") + ",\"m\":\"" + jsonEsc(m) + "\"}";
  mqtt.publish((String(MQTT_TOPIC) + "/dev/ack").c_str(), j.c_str());
}

void publishLogTail() {
  String t;
  if (logMx) xSemaphoreTake(logMx, portMAX_DELAY);
  for (int i = 0; i < logCount; i++) { t += logBuf[(logHead - logCount + i + LOG_N) % LOG_N]; t += '\n'; }
  const uint32_t seq = logSeq;
  if (logMx) xSemaphoreGive(logMx);
  if (mqtt.publish((String(MQTT_TOPIC) + "/dev/log").c_str(), t.c_str(), true)) logPubSeq = seq;
  lastLogPubMs = millis();
}

void publishDev() {
  static const char* ST[3] = { "idle", "tour", "focus" };
  const bool on = WiFi.status() == WL_CONNECTED;
  String j = "{\"fw\":\"" + String(FW_VERSION) + "\",\"up\":" + String(millis() / 1000);
  j += ",\"boot\":\"" + String(resetReasonText()) + "\",\"heap\":" + String(ESP.getFreeHeap() / 1024);
  j += ",\"temp\":" + String((int)temperatureRead());
  j += ",\"ssid\":\"" + jsonEsc(on ? WiFi.SSID() : String("")) + "\",\"ip\":\"" + (on ? WiFi.localIP().toString() : String("-")) + "\"";
  j += ",\"rssi\":" + String(on ? WiFi.RSSI() : 0) + ",\"mac\":\"" + WiFi.macAddress() + "\"";
  j += ",\"radio\":" + String(radioLevel == 1 ? 19.5f : radioLevel == 2 ? 11.0f : 15.0f, 1) + ",\"hot\":" + String(tooHot ? 1 : 0);
  j += ",\"drops\":{\"w\":" + String(wifiDrops) + ",\"m\":" + String(mqttDrops) + ",\"last\":\"" + jsonEsc(lastDrop) + "\"}";
  j += ",\"key\":" + String(devKey.length() ? "true" : "false");
  j += ",\"sleep\":" + String(sleeping ? "true" : "false") + ",\"ap\":" + String(apOn ? "true" : "false");
  j += ",\"ts\":" + String((long)time(nullptr));     // clock time, so the console can tell old news from live news
  j += ",\"state\":\"" + String(ST[state]) + "\",\"box\":" + String(state == ST_FOCUS ? focusBox + 1 : 0);
  j += ",\"event\":\"" + jsonEsc(lastEvent) + "\",\"test\":" + String(testPattern ? "true" : "false");
  j += ",\"hall\":" + String(hall.raw()) + ",\"hallBase\":" + String(hall.baseline());
  j += ",\"cfg\":{\"mode\":" + String(cfg.mode) + ",\"bri\":" + String(cfg.brightness) + ",\"vol\":" + String(cfg.volume);
  j += ",\"s1\":" + String(cfg.spk1) + ",\"s2\":" + String(cfg.spk2) + ",\"slp\":" + String(cfg.sleepMin);
  j += ",\"hall\":" + String(cfg.hallThreshold) + ",\"lock\":" + String(cfg.focusLockS) + ",\"remote\":" + String(cfg.remoteOn ? 1 : 0);
  j += ",\"btn\":[";
  for (int i = 0; i < 4; i++) { j += cfg.buttonOn[i] ? "1" : "0"; if (i < 3) j += ","; }
  j += "]},\"aud\":[";
  for (int b = 0; b < Audio::NTRACK; b++) { if (b) j += ","; j += String(Audio::trackSeconds(b), 1); }
  j += "],\"saved\":[";
  for (int i = 0; i < netCount; i++) {
    if (i) j += ",";
    j += "{\"s\":\"" + jsonEsc(nets[i].ssid) + "\",\"e\":" + String(nets[i].ent ? 1 : 0) + "}";
  }
  j += "]}";
  mqtt.publish((String(MQTT_TOPIC) + "/dev/status").c_str(), j.c_str(), true);
  devDirty = false;
  lastDevMs = millis();
}

// "bri=120&vol=50&mode=0&hall=300&lock=10&b1=1&remote=1" — same limits as the local page
void applySetString(const String& a) {
  int i = 0;
  while (i < (int)a.length()) {
    int amp = a.indexOf('&', i); if (amp < 0) amp = a.length();
    const String kv = a.substring(i, amp);
    const int eq = kv.indexOf('=');
    if (eq > 0) {
      const String k = kv.substring(0, eq);
      const int v = kv.substring(eq + 1).toInt();
      if (k == "bri") cfg.brightness = constrain(v, 20, 255);
      else if (k == "vol") cfg.volume = constrain(v, 0, 100);
      else if (k == "s1") cfg.spk1 = constrain(v, 0, 100);
      else if (k == "s2") cfg.spk2 = constrain(v, 0, 100);
      else if (k == "slp") cfg.sleepMin = constrain(v, 0, 240);
      else if (k == "mode") cfg.mode = constrain(v, 0, 2);
      else if (k == "hall") { cfg.hallThreshold = constrain(v, 50, 2000); hall.threshold = cfg.hallThreshold; }
      else if (k == "lock") cfg.focusLockS = constrain(v, 0, 60);
      else if (k == "remote") cfg.remoteOn = v != 0;
      else if (k.length() == 2 && k[0] == 'b' && k[1] >= '1' && k[1] <= '4') cfg.buttonOn[k[1] - '1'] = v != 0;
    }
    i = amp + 1;
  }
  sendCmd(CMD_APPLY);
}

void remoteOta(const String& url) {
  updating = true;
  logLine("远程更新：开始下载");
  publishLogTail();
  delay(300);                           // the LED loop switches the LEDs off
  mqtt.disconnect();                    // free memory for the download
  tlsClient.stop();
  WiFiClientSecure c;
  c.setInsecure();
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  const t_httpUpdate_return r = httpUpdate.update(c, url);
  if (r == HTTP_UPDATE_OK) {
    logLine("远程更新：成功，重启");
    delay(300);
    ESP.restart();
  }
  logLine("远程更新：失败 " + httpUpdate.getLastErrorString() + "（继续用旧程序）");
  updating = false;
  lastMqttTry = 0;                      // reconnect now and report
}

void onDevCmd(const String& msg) {
  // time|nonce|command|argument|signature
  const int p4 = msg.lastIndexOf('|');
  if (p4 < 0) return;
  const String body = msg.substring(0, p4), sig = msg.substring(p4 + 1);
  const int p1 = body.indexOf('|'), p2 = body.indexOf('|', p1 + 1), p3 = body.indexOf('|', p2 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0) return;
  const String nonce = body.substring(p1 + 1, p2), cmd = body.substring(p2 + 1, p3), arg = body.substring(p3 + 1);
  if (devKey.length() == 0) { devAck(nonce, false, "ESP32 还没设置开发者密钥（在 192.168.4.1/wifi 设置）"); return; }
  if (!sig.equalsIgnoreCase(hmacHex(devKey, body))) { devAck(nonce, false, "密钥不对"); logLine("远程：密钥不对的命令被拒绝"); return; }
  const long t = body.substring(0, p1).toInt();
  const long nowT = (long)time(nullptr);
  if (nowT < 1700000000L) { devAck(nonce, false, "ESP32 还没对时，稍等再试"); return; }
  if (labs(nowT - t) > 300) { devAck(nonce, false, "命令过期（电脑时间不对？）"); return; }
  for (int i = 0; i < 16; i++) if (usedNonces[i] == nonce) return;      // same command twice
  usedNonces[usedNonceHead] = nonce; usedNonceHead = (usedNonceHead + 1) % 16;

  logLine("远程：" + cmd + (arg.length() && cmd != "ota" ? " " + arg : String("")));
  if (cmd == "tour") sendCmd(CMD_TOUR);
  else if (cmd == "stop") sendCmd(CMD_STOP);
  else if (cmd == "test") sendCmd(CMD_TEST);
  else if (cmd == "box" && arg.toInt() >= 1 && arg.toInt() <= 4) sendCmd(CMD_FOCUS, arg.toInt() - 1);
  else if (cmd == "set") applySetString(arg);
  else if (cmd == "spk" && arg.toInt() >= 0 && arg.toInt() <= 2) sendCmd(CMD_SPK, arg.toInt());
  else if (cmd == "log") logPubSeq = logSeq - 1;
  else if (cmd == "ap") sendCmd(CMD_AP);
  else if (cmd == "atest" && arg.toInt() >= 1 && arg.toInt() <= Audio::NTRACK && Audio::hasTrack[arg.toInt() - 1]) {
    Audio::previewUntil = millis() + 15000; Audio::previewBox = arg.toInt() - 1;
  }
  else if (cmd == "ptest") {
    int p = -1, a = 0, l = 1, br = 128, c = 0;
    if (sscanf(arg.c_str(), "%d,%d,%d,%d,%d", &p, &a, &l, &br, &c) != 5 || p < 0 || p > 11 || a < 0 || a > 2 || br < 0 || br > 255 || c < 0 || c > 7) {
      devAck(nonce, false, "板子测试参数不对"); return;
    }
    if (p == 0) panelTestPanel = 255;
    else {
      const uint8_t q = p - 1;
      const int maxLine = isBridgePanel(q) ? (a == 2 ? 32 : 8) : 16;
      if (a != 0 && (l < 1 || l > maxLine)) { devAck(nonce, false, "这一块板没有这条线"); return; }
      panelTestPanel = q; panelTestAxis = a; panelTestLine = l - 1;
      panelTestBrightness = br; panelTestColor = c;
    }
    sendCmd(CMD_PANEL_TEST);
  }
  else if (cmd == "sleep") sendCmd(CMD_SLEEP);
  else if (cmd == "wake") sendCmd(CMD_STOP);
  else if (cmd == "restart") { devAck(nonce, true, "重启中"); publishLogTail(); mqtt.loop(); delay(500); ESP.restart(); }
  else if (cmd == "net" || cmd == "del") {
    const int i = arg.toInt();
    if (i < 0 || i >= netCount) { devAck(nonce, false, "没有这个网络"); return; }
    if (cmd == "del") {
      if (i == netTry) { devAck(nonce, false, "不能删除正在用的网络"); return; }
      for (int k = i; k < netCount - 1; k++) nets[k] = nets[k + 1];
      netCount--;
      if (netTry > i) netTry--;
      saveNets();
      netNote = "远程删除了一个网络";
    } else {
      devAck(nonce, true, "切换到 " + nets[i].ssid + "，云端会断开一会儿；连不上会自动换回其他网络");
      publishLogTail();
      mqtt.loop();
      const NetCfg n = nets[i];
      for (int k = i; k > 0; k--) nets[k] = nets[k - 1];
      nets[0] = n;
      saveNets();
      connectNet(0);
      return;
    }
  }
  else if (cmd == "ota") {
    if (!arg.startsWith("https://") || !arg.endsWith(".bin")) { devAck(nonce, false, "链接要以 https:// 开头、.bin 结尾"); return; }
    devAck(nonce, true, "开始下载，灯会关掉 30–90 秒；成功后版本号会变");
    mqtt.loop();
    remoteOta(arg);
    return;
  }
  else { devAck(nonce, false, "不认识的命令"); return; }
  devAck(nonce, true, "OK");
  devDirty = true;
}

// ---- Captive portal: joining "Saliva-Control" opens the console by itself ----
DNSServer dns;

// Local hotspot "Saliva-Control" uses the radio all the time and stops the Wi-Fi from resting.
// It is on: for 10 minutes after power-on · whenever the internet Wi-Fi is not connected ·
// for 10 minutes after holding button 1 for 5 s (or "打开本地热点" in dev.html).
// Otherwise it is off and the Wi-Fi rests between messages (modem sleep) → cooler, less power.
uint32_t staUpSince = 0;
void apManage(uint32_t now) {
  const bool sta = WiFi.status() == WL_CONNECTED;
  if (!sta) staUpSince = 0; else if (!staUpSince) staUpSince = now;
  const bool staStable = sta && now - staUpSince > 30000;
  const bool want = !staStable || (int32_t)(apUntilMs - now) > 0;
  if (want == apOn) return;
  if (want) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    dns.start(53, "*", WiFi.softAPIP());
    netNote = "本地热点 Saliva-Control 已打开";
  } else {
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);                 // modem sleep: the radio rests between router beacons → much less heat
    netNote = "本地热点已关闭（省电）。要打开：快速连按按钮 1 三下";
  }
  apOn = want;
  devDirty = true;
}
void handleNotFound() {
  const String host = server.hostHeader();
  const String ap = WiFi.softAPIP().toString();
  if (host != ap && host != WiFi.localIP().toString()) {
    server.sendHeader("Location", "http://" + ap + "/", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "not found");
  }
}

volatile char netSerialReq = 0;   // 'w' / 'x' typed in Serial, handled by the network task

// Network task: core 0 (with Wi-Fi itself and the sound task). Slow things — school login,
// cloud connection, page requests, program updates — wait here and never hold up the LEDs.
void netTask(void*) {
  String loggedNote;
  for (;;) {
    if (apOn) dns.processNextRequest();
    if (!updating) apManage(millis());
    server.handleClient();
    ArduinoOTA.handle();
    if (!updating) netLoop(millis());
    if (netSerialReq == 'w') { enterNetCreds(); netSerialReq = 0; }
    else if (netSerialReq == 'x') { forgetNetCreds(); netSerialReq = 0; }
    if (netNote != loggedNote) { loggedNote = netNote; logLine("网络：" + netNote); }
    static uint32_t lastBB = 0;
    if (millis() - lastBB >= 1000) {           // black box: what was going on, in case of a crash
      lastBB = millis();
      bbMagic = BLACKBOX_MAGIC;
      bbUptime = millis() / 1000;
      bbFlags = (apOn ? 1 : 0) | (mqtt.connected() ? 2 : 0) | ((int32_t)(watchUntilMs - millis()) > 0 ? 4 : 0) |
                (updating ? 8 : 0) | (sleeping ? 16 : 0);
      strncpy(bbEvent, lastEvent, sizeof(bbEvent) - 1);
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void startArduinoOta() {
  ArduinoOTA.setHostname("saliva-esp32");
  ArduinoOTA.setPassword(AP_PASS);
  ArduinoOTA.onStart([]() { stopForUpdate(); });
  ArduinoOTA.onEnd([]() { logLine("Arduino IDE 更新完成，重启"); });
  ArduinoOTA.onError([](ota_error_t e) { logLine("Arduino IDE 更新失败，错误 " + String((int)e)); updating = false; });
  ArduinoOTA.begin();
}

// =============================================================================
//  8) SETUP / LOOP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(2500);  // gives the computer time to open the Serial Monitor (no while(!Serial): runs without a computer)
  Serial.println(F("D2D Inside a Drop of Saliva — full program. Generated but not physically tested."));
  Serial.print(F("BOOT — last restart reason: ")); Serial.println(resetReasonText());
  setCpuFrequencyMhz(160);               // 160 MHz is plenty and runs cooler than 240
  Serial.println(F("step 1/4: LEDs"));
  logMx = xSemaphoreCreateMutex();
  cmdQ = xQueueCreate(16, sizeof(Cmd));

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  loadSettings();
  FastLED.setBrightness(cfg.brightness);
  FastLED.clear(true);

  for (int i = 0; i < 4; i++) { buttons[i].pin = BUTTON_PIN[i]; pinMode(BUTTON_PIN[i], INPUT_PULLUP); }
  analogReadResolution(12);
  hall.threshold = cfg.hallThreshold;
  hall.begin(HALL_PIN);   // keep the disc at rest (magnet away) while powering on
  xTaskCreatePinnedToCore(inputTask, "buttons", 3072, nullptr, 2, nullptr, 1);

  Serial.println(F("step 2/4: sound"));
  Audio::begin();
  applySettings();

#if ENABLE_WIFI
  Serial.println(F("step 3/4: Wi-Fi"));
  WiFi.mode(WIFI_AP_STA);
  WiFi.setTxPower(WIFI_POWER_15dBm);     // starts cool; manageRadio() raises it only while the signal is weak
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) { lastDiscReason = info.wifi_sta_disconnected.reason; },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.softAP(AP_SSID, AP_PASS);
  dns.start(53, "*", WiFi.softAPIP());
  loadDevKey();
  loadNetCreds();
  startSchoolWifi();
  tlsClient.setInsecure();   // public test broker: encrypted, certificate not checked
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqtt);
  mqtt.setKeepAlive(60);                // more patient on a slow campus network
  mqtt.setBufferSize(6144);             // the developer log is a few KB
  server.onNotFound(handleNotFound);
  server.on("/api/devkey", HTTP_POST, []() {
    const String k = server.arg("k");
    if (k.length() == 0) { saveDevKey(""); logLine("开发者密钥已删除"); }
    else if (k.length() >= 8) { saveDevKey(k); logLine("开发者密钥已设置"); }
    else { server.send(400, "text/plain", "at least 8 characters"); return; }
    handleNetStatus();
  });
  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/api/status", handleStatus);
  server.on("/api/cmd", handleCmd);
  server.on("/api/set", handleSet);
  server.on("/update", HTTP_GET, []() { server.send_P(200, "text/html", UPDATE_PAGE); });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/api/log", handleLog);
  server.on("/audio", HTTP_GET, []() { server.send_P(200, "text/html", AUDIO_PAGE); });
  server.on("/api/audio", HTTP_GET, handleAudioList);
  server.on("/api/audio/up", HTTP_POST, handleAudioUpDone, handleAudioUpload);
  server.on("/api/audio/del", HTTP_POST, handleAudioDel);
  server.on("/api/audio/test", HTTP_POST, handleAudioTest);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/wifi", []() { server.send_P(200, "text/html", WIFI_PAGE); });
  server.on("/api/net", handleNetStatus);
  server.on("/api/scan", handleScan);
  server.on("/api/net/add", HTTP_POST, handleNetAdd);
  server.on("/api/net/del", HTTP_POST, handleNetDel);
  server.on("/api/net/first", HTTP_POST, handleNetFirst);
  server.on("/api/net/clear", HTTP_POST, handleNetClear);
  server.begin();
  startArduinoOta();
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);   // TLS needs a big stack
  Serial.print(F("Control page: join Wi-Fi ")); Serial.print(AP_SSID);
  Serial.print(F(" and open http://")); Serial.println(WiFi.softAPIP());
#endif

  Serial.println(F("step 4/4: running"));
  logLine(String("开机：") + resetReasonText());
  {
    const esp_reset_reason_t rr = esp_reset_reason();
    if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT) {
      const String bb = blackBoxReport();
      if (bb.length()) logLine(bb);
    }
    bbMagic = 0;
  }
  goIdle(millis());
}

void loop() {
  // LED core (core 1): sensors → decide → draw → send. Network and buttons run in their own tasks.
  if (updating) {                       // a new program is coming in: LEDs off, nothing else
    static bool off = false;
    if (!off) { FastLED.clear(true); off = true; }
    delay(20);
    return;
  }

  static uint32_t last = 0;
  const uint32_t now = millis();
  const uint32_t frameMs = sleeping ? SLEEP_FRAME_MS : FRAME_MS;   // standby: fewer frames = cooler
  if (now - last < frameMs) { delay(sleeping ? 10 : 1); return; }
  last = now;

#if ENABLE_WIFI
  if (!netSerialReq)                    // while the network task is reading an ID/password, leave Serial alone
#endif
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == 'g') startTour(now);
    else if (c >= '1' && c <= '4') startFocus(c - '1', now, state == ST_TOUR);
    else if (c == 's') goIdle(now);
    else if (c == 't') testPattern = !testPattern;
    else if (c == 'h') printHall = !printHall;
#if ENABLE_WIFI
    else if (c == 'w' || c == 'x') { netSerialReq = c; break; }
#endif
  }

  processCmds(now);                     // buttons, control page, visitor website
  static const char* loggedEvent = nullptr;
  if (lastEvent != loggedEvent) { loggedEvent = lastEvent; logLine(String("事件：") + lastEvent); }

  const bool inputsOn = cfg.mode == MODE_AUTO;
  const bool visitor = hall.update(now, inputsOn && state == ST_IDLE);
  if (visitor) { wakeUp(now); startTour(now); }
  if (cfg.mode == MODE_IDLE && state != ST_IDLE) goIdle(now);
  if (state != ST_IDLE || testPattern || hall.active()) lastActivityMs = now;   // something is happening

  // ---- standby: nobody for cfg.sleepMin minutes → the same idle animation, dimmer and at 5 frames/s ----
  if (!sleeping && cfg.sleepMin > 0 && state == ST_IDLE && !testPattern && now - lastActivityMs > (uint32_t)cfg.sleepMin * 60000UL) {
    sleeping = true;
    statusDirty = true;
    lastEvent = "standby";
  }
  if (sleeping) {                           // hall, buttons and website still wake it (checked above)
    Audio::want(-1, 0);
    FastLED.clear();
    renderIdle((now - stateStartMs) / 1000.0f);
    for (int i = 0; i < NUM_LEDS; i++) leds[i].nscale8_video(SLEEP_DIM);
    captureLive(now);
    FastLED.show();
    return;
  }

  if (printHall) {
    Serial.print(F("hall raw ")); Serial.print(hall.raw());
    Serial.print(F("  resting ")); Serial.print(hall.baseline());
    Serial.println(hall.active() ? F("  MAGNET") : F(""));
  }

  FastLED.clear();   // standard §7 rule 7: clear, then every module redraws its panels
  if (panelTestActive) {
    drawPanelTest();
  } else if (testPattern) {
    drawTestPattern();
  } else if (state == ST_IDLE) {
    renderIdle((now - stateStartMs) / 1000.0f);
  } else if (state == ST_TOUR) {
    const float T = (now - tourStartMs) / 1000.0f;
    if (T >= tourEnd()) { goIdle(now); renderIdle(0.0f); }
    else renderTour(T);
  } else {
    const float ft = (now - focusStartMs) / 1000.0f;
    if (!renderFocus(ft)) {
      if (tourPaused) { resumeTour(now); renderTour(tourPausedAt); }
      else { goIdle(now); renderIdle(0.0f); }
    }
  }
  limitSupply(0, SUPPLY_SPLIT_LED);
  limitSupply(SUPPLY_SPLIT_LED, NUM_LEDS);
  // music follows the box that is playing right now
  {
    int wb; float wp, we;
    soundAt(now, wb, wp, we);
    Audio::want(wb, wp);
  }
  captureLive(now);
  FastLED.show();
}
