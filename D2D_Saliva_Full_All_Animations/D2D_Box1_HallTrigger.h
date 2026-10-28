/*
 * =============================================================================
 *  D2D_Box1_HallTrigger.h  —  ADDED BY CHAN (Box 1). Optional helper for the main program.
 * =============================================================================
 *  Starts the four-box sequence when a visitor turns the Box 1 disc into position.
 *  This is NOT an animation function: it reads one analogue input and returns a flag.
 *  It never touches the LEDs, never blocks and never calls delay().
 *  STATUS: Generated but not physically tested.
 *
 *  Sensor : DRV5055 linear Hall sensor under the disc's magnet (analogue output).
 *           Power it from 3.3 V so its output stays inside the ESP32-S3 ADC range.
 *  Pin    : any free ADC1 pin (GPIO 1–10) EXCEPT GPIO 4, which is the LED data line.
 *           Suggested: GPIO 5. (The integrator decides; this pin is not in the standard yet.)
 *
 *  Use:
 *    D2DBox1HallTrigger hall;
 *    setup():  hall.begin(5);        // reads the resting value — disc at rest (magnet away) at power-up
 *    loop():   if (hall.update(millis(), waitingForVisitor)) { start the sequence at Box 1, progress 0 }
 *  Call update() every frame, also while the sequence runs (allow = false), so it can see the
 *  disc being turned back and re-arm.
 *
 *  Behaviour:
 *    - triggers once when the magnet has been detected for dwellMs (disc held in position);
 *    - after a trigger it re-arms only when the disc has been away for rearmMs, so a disc left
 *      in position does not restart the installation again and again.
 * =============================================================================
 */
#pragma once
#include <Arduino.h>

class D2DBox1HallTrigger {
public:
  int      threshold = 500;   // ADC counts away from the resting value = magnet detected (print raw() to tune)
  uint16_t dwellMs   = 300;   // disc must stay in position this long (ignores turning past the mark)
  uint16_t rearmMs   = 800;   // disc must be away this long before the next trigger is allowed

  void begin(int pin) {
    pin_ = pin;
    if (pin_ < 0) return;
    long sum = 0;
    for (int i = 0; i < 64; i++) sum += analogRead(pin_);   // resting value (no delay needed)
    baseline_ = (int)(sum / 64);
  }

  // allow = true only while the installation is waiting for a visitor.
  // Returns true once, on the frame the visitor's alignment is confirmed.
  bool update(uint32_t now, bool allow) {
    if (pin_ < 0) return false;
    raw_ = analogRead(pin_);
    const int diff = raw_ - baseline_;
    const bool on = (diff > threshold) || (diff < -threshold);   // either magnet pole works
    if (on != active_) { active_ = on; changedAt_ = now; }
    if (!on && !armed_ && (uint32_t)(now - changedAt_) >= rearmMs) armed_ = true;
    if (allow && armed_ && on && (uint32_t)(now - changedAt_) >= dwellMs) { armed_ = false; return true; }
    return false;
  }

  bool active()   const { return active_; }     // magnet seen right now (for a test pattern)
  int  raw()      const { return raw_; }
  int  baseline() const { return baseline_; }

private:
  int      pin_ = -1, raw_ = 0, baseline_ = 2048;
  bool     active_ = false, armed_ = true;
  uint32_t changedAt_ = 0;
};
