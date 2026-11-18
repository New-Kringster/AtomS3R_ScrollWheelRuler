// Digital ruler: M5Stack AtomS3R + Atomic Battery Base + Unit Scroll (Grove).
//
// Roll the Unit Scroll's wheel along a surface while pressing it down: the
// distance counts while the wheel is pressed and stops when it is released.
// Screen button: tap = zero the reading, hold = recalibrate (roll 10 cm).
// The calibration is kept in NVS, so it survives power-off.

#include <M5Unified.h>
#include <Preferences.h>
#include <Wire.h>

#include "RulerCore.h"

// ---- Unit Scroll (STM32, I2C 0x40), register map from M5Stack's protocol sheet
namespace scroll {
constexpr uint8_t kAddr = 0x40;
constexpr uint8_t kRegButton = 0x20;   // 1 byte, 0 = pressed
constexpr uint8_t kRegLed = 0x30;      // [index, R, G, B]
constexpr uint8_t kRegReset = 0x40;    // write 1 to zero both counters
constexpr uint8_t kRegIncrement = 0x50;  // int16 LE, steps since last read, clears on read

// AtomS3R Grove port: G2 = SDA, G1 = SCL. M5Unified keeps its internal bus on
// I2C1 and never starts this port, so Arduino Wire (I2C0) owns it.
constexpr int kSda = 2;
constexpr int kScl = 1;

// Same transaction shape as M5Stack's own M5UnitScroll::readBytes().
bool read(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kAddr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool write(uint8_t reg, const uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  Wire.write(buf, len);
  return Wire.endTransmission() == 0;
}

bool present() {
  Wire.beginTransmission(kAddr);
  return Wire.endTransmission() == 0;
}

void setLed(uint32_t rgb) {
  const uint8_t data[4] = {0, uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb)};
  write(kRegLed, data, sizeof(data));
}

void resetCounters() {
  const uint8_t one = 1;
  write(kRegReset, &one, 1);
}

// One poll: steps moved since the previous poll, and the button state.
bool poll(int32_t& delta, bool& pressed) {
  uint8_t btn, inc[2];
  if (!read(kRegButton, &btn, 1) || !read(kRegIncrement, inc, 2)) return false;
  pressed = (btn == 0);
  delta = int16_t(inc[0] | (inc[1] << 8));
  return true;
}
}  // namespace scroll

// ---- Persistence
namespace store {
constexpr const char* kNamespace = "ruler";
constexpr const char* kKeySteps = "steps10cm";

int32_t loadCalibration() {
  Preferences p;
  p.begin(kNamespace, false);  // read-write so the namespace exists on first boot
  int32_t v = p.getInt(kKeySteps, 0);
  p.end();
  return v;
}

void saveCalibration(int32_t steps) {
  Preferences p;
  p.begin(kNamespace, false);
  p.putInt(kKeySteps, steps);
  p.end();
}
}  // namespace store

// ---- UI
namespace ui {
constexpr uint16_t kBg = 0x0000;
constexpr uint16_t kText = 0xFFFF;
constexpr uint16_t kDim = 0x8410;      // grey
constexpr uint16_t kLive = 0x07E8;     // green
constexpr uint16_t kCal = 0x4D7F;      // light blue
constexpr uint16_t kWarn = 0xFD20;     // amber

M5Canvas canvas(&M5.Display);

// Big centred number, shrunk until it fits the 128 px screen.
void drawBig(const char* s, int y, uint16_t color) {
  canvas.setFont(&fonts::FreeSansBold24pt7b);
  canvas.setTextColor(color);
  float size = 1.0f;
  canvas.setTextSize(size);
  while (canvas.textWidth(s) > 122 && size > 0.4f) {
    size -= 0.05f;
    canvas.setTextSize(size);
  }
  canvas.setTextDatum(middle_center);
  canvas.drawString(s, 64, y);
  canvas.setTextSize(1);
}

void drawText(const char* s, int x, int y, uint16_t color, const lgfx::IFont* font = &fonts::Font2,
              textdatum_t datum = middle_center) {
  canvas.setFont(font);
  canvas.setTextColor(color);
  canvas.setTextDatum(datum);
  canvas.drawString(s, x, y);
}

void header(const char* title, uint16_t color, const char* tag, uint16_t tagColor) {
  drawText(title, 4, 9, color, &fonts::Font2, middle_left);
  if (tag) {
    canvas.setFont(&fonts::Font0);
    int w = canvas.textWidth(tag) + 8;
    canvas.fillRoundRect(124 - w, 2, w, 14, 4, tagColor);
    drawText(tag, 124 - w / 2, 9, kBg, &fonts::Font0);
  }
  canvas.drawFastHLine(0, 19, 128, 0x2104);
}

void drawMeasuring(const RulerCore& r) {
  char buf[24];
  const bool live = r.measuring();
  header("RULER", kText, live ? "LIVE" : "IDLE", live ? kLive : kDim);

  float mm = r.distanceMm();
  snprintf(buf, sizeof(buf), "%.1f", mm / 10.0f);
  drawBig(buf, 54, live ? kLive : kText);
  drawText("cm", 64, 84, kDim, &fonts::FreeSans9pt7b);

  snprintf(buf, sizeof(buf), "%.0f mm  (%.1f/step)", mm, r.mmPerStep());
  drawText(buf, 64, 103, kDim, &fonts::Font0);
  drawText("tap: zero  hold: calib", 64, 119, 0x4208, &fonts::Font0);
}

void drawCalibrating(const RulerCore& r) {
  char buf[24];
  header("CALIBRATE", kCal, r.pressed() ? "LIVE" : nullptr, kLive);

  snprintf(buf, sizeof(buf), "%ld", long(abs(r.calibrationSteps())));
  drawBig(buf, 50, r.pressed() ? kLive : kText);
  drawText("steps", 64, 76, kDim, &fonts::Font0);

  drawText("Press the wheel and", 64, 94, kText, &fonts::Font0);
  drawText("roll exactly 10 cm", 64, 105, kText, &fonts::Font0);
  if (r.calibrationSteps() == 0 && r.canCancelCalibration()) {
    drawText("tap: cancel", 64, 119, 0x4208, &fonts::Font0);
  } else {
    drawText("then release", 64, 116, kText, &fonts::Font0);
  }
}

void drawConfirming(const RulerCore& r) {
  char buf[24];
  header("CALIBRATE", kCal, "CHECK", kWarn);

  snprintf(buf, sizeof(buf), "%ld", long(abs(r.calibrationSteps())));
  drawBig(buf, 46, kText);
  drawText("steps = 10 cm", 64, 72, kDim, &fonts::Font0);

  if (r.calibrationLongEnough()) {
    drawText("tap: save", 64, 92, kLive, &fonts::Font2);
  } else {
    drawText("too short", 64, 92, kWarn, &fonts::Font2);
  }
  drawText("hold: start over", 64, 108, kDim, &fonts::Font0);
  drawText("wheel: keep rolling", 64, 119, kDim, &fonts::Font0);
}

void drawNoUnit() {
  header("RULER", kText, "ERR", kWarn);
  drawText("Unit Scroll", 64, 50, kText, &fonts::Font2);
  drawText("not found", 64, 68, kText, &fonts::Font2);
  drawText("plug it into the", 64, 96, kDim, &fonts::Font0);
  drawText("Grove port (0x40)", 64, 108, kDim, &fonts::Font0);
}

void render(const RulerCore& r, bool unitOk) {
  canvas.fillSprite(kBg);
  if (!unitOk) {
    drawNoUnit();
  } else {
    switch (r.mode()) {
      case RulerCore::Mode::Measuring: drawMeasuring(r); break;
      case RulerCore::Mode::Calibrating: drawCalibrating(r); break;
      case RulerCore::Mode::Confirming: drawConfirming(r); break;
    }
  }
  canvas.pushSprite(0, 0);
}
}  // namespace ui

// ---- App
static RulerCore* ruler = nullptr;
static bool unitOk = false;

static uint32_t ledColorFor(const RulerCore& r) {
  switch (r.mode()) {
    case RulerCore::Mode::Measuring: return r.pressed() ? 0x002000 : 0x000000;
    case RulerCore::Mode::Calibrating: return r.pressed() ? 0x002000 : 0x000018;
    case RulerCore::Mode::Confirming: return 0x181000;
  }
  return 0;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.BtnA.setHoldThresh(800);

  ui::canvas.setColorDepth(16);
  ui::canvas.createSprite(M5.Display.width(), M5.Display.height());

  Wire.begin(scroll::kSda, scroll::kScl, 100000);
  unitOk = scroll::present();
  if (unitOk) scroll::resetCounters();

  const int32_t saved = store::loadCalibration();
  ruler = new RulerCore(saved);
  Serial.printf("ruler: unit %s, saved calibration %ld steps/10cm\n",
                unitOk ? "found" : "missing", long(saved));
}

void loop() {
  M5.update();

  static uint32_t lastPoll = 0;
  static uint32_t lastProbe = 0;
  static uint32_t lastLed = 0xFFFFFFFF;
  static uint32_t lastDraw = 0;
  bool dirty = false;

  const uint32_t now = millis();
  if (now - lastPoll >= 10) {
    lastPoll = now;
    if (unitOk) {
      int32_t delta;
      bool pressed;
      if (scroll::poll(delta, pressed)) {
        const auto modeBefore = ruler->mode();
        const bool pressedBefore = ruler->pressed();
        const int32_t stepsBefore = ruler->steps();
        const int32_t calBefore = ruler->calibrationSteps();
        ruler->update(delta, pressed);
        dirty |= ruler->mode() != modeBefore || ruler->pressed() != pressedBefore ||
                 ruler->steps() != stepsBefore || ruler->calibrationSteps() != calBefore;
      } else {
        unitOk = false;
        lastLed = 0xFFFFFFFF;
        ruler->update(0, false);  // treat a lost unit as "wheel released"
        dirty = true;
      }
    } else if (now - lastProbe >= 500) {  // unplugged: retry twice a second
      lastProbe = now;
      if (scroll::present()) {
        unitOk = true;
        scroll::resetCounters();  // drop anything counted while we weren't listening
        dirty = true;
      }
    }
  }

  if (M5.BtnA.wasHold()) {
    ruler->hold();
    dirty = true;
  } else if (M5.BtnA.wasClicked()) {
    if (ruler->tap()) {
      store::saveCalibration(ruler->stepsPer10cm());
      Serial.printf("ruler: saved calibration %ld steps/10cm\n", long(ruler->stepsPer10cm()));
    }
    dirty = true;
  }

  if (unitOk) {
    const uint32_t led = ledColorFor(*ruler);
    if (led != lastLed) {
      scroll::setLed(led);
      lastLed = led;
    }
  }

  static uint32_t lastLog = 0;
  if (now - lastLog >= 3000) {
    lastLog = now;
    Serial.printf("ruler: unit=%d mode=%d pressed=%d steps=%ld cal=%ld per10cm=%ld mm=%.1f\n", unitOk,
                  int(ruler->mode()), ruler->pressed(), long(ruler->steps()),
                  long(ruler->calibrationSteps()), long(ruler->stepsPer10cm()), ruler->distanceMm());
  }

  // Redraw on change, plus a slow refresh so the screen can never go stale.
  if (dirty || now - lastDraw >= 1000) {
    lastDraw = now;
    ui::render(*ruler, unitOk);
  }
  delay(1);
}
