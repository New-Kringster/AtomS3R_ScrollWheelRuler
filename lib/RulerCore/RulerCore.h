#pragma once
// Hardware-free ruler logic: modes, calibration and distance maths.
// main.cpp feeds it encoder steps and button events; the native unit tests
// drive the same class without any M5Stack hardware.

#include <stdint.h>
#include <stdlib.h>

class RulerCore {
public:
  enum class Mode : uint8_t {
    Calibrating,  // roll exactly 10 cm with the wheel pressed
    Confirming,   // released after a calibration roll: tap saves, hold redoes
    Measuring,    // normal ruler
  };

  static constexpr float kCalibrationMm = 100.0f;  // the 10 cm reference length
  static constexpr int32_t kMinCalSteps = 4;       // shorter than this is a mistake

  // savedStepsPer10cm is what NVS holds; 0 means "never calibrated".
  explicit RulerCore(int32_t savedStepsPer10cm) {
    if (isValidCalibration(savedStepsPer10cm)) {
      stepsPer10cm_ = savedStepsPer10cm;
      mode_ = Mode::Measuring;
    } else {
      mode_ = Mode::Calibrating;
    }
  }

  static bool isValidCalibration(int32_t steps) { return abs(steps) >= kMinCalSteps; }

  // Called every poll with the steps the wheel moved since the last poll and
  // whether the wheel is pressed down. Steps only count while it is pressed.
  void update(int32_t deltaSteps, bool wheelPressed) {
    pressed_ = wheelPressed;
    if (!wheelPressed) {
      if (mode_ == Mode::Calibrating && calSteps_ != 0) mode_ = Mode::Confirming;
      return;
    }
    switch (mode_) {
      case Mode::Measuring:
        steps_ += deltaSteps;
        break;
      case Mode::Confirming:  // pressing again continues the same calibration roll
        mode_ = Mode::Calibrating;
        calSteps_ += deltaSteps;
        break;
      case Mode::Calibrating:
        calSteps_ += deltaSteps;
        break;
    }
  }

  // Short press of the screen button. Returns true when a new calibration
  // was accepted and should be written to NVS.
  bool tap() {
    switch (mode_) {
      case Mode::Measuring:
        steps_ = 0;
        return false;
      case Mode::Confirming:
        if (!isValidCalibration(calSteps_)) return false;
        stepsPer10cm_ = calSteps_;
        steps_ = 0;
        mode_ = Mode::Measuring;
        return true;
      case Mode::Calibrating:
        // Nothing rolled yet and an older calibration exists: cancel back to it.
        if (calSteps_ == 0 && isValidCalibration(stepsPer10cm_)) mode_ = Mode::Measuring;
        return false;
    }
    return false;
  }

  // Long press of the screen button: start (or restart) calibration.
  void hold() {
    calSteps_ = 0;
    mode_ = Mode::Calibrating;
  }

  Mode mode() const { return mode_; }
  bool pressed() const { return pressed_; }
  bool measuring() const { return mode_ == Mode::Measuring && pressed_; }
  bool canCancelCalibration() const { return isValidCalibration(stepsPer10cm_); }
  bool calibrationLongEnough() const { return isValidCalibration(calSteps_); }

  int32_t calibrationSteps() const { return calSteps_; }
  int32_t stepsPer10cm() const { return stepsPer10cm_; }
  int32_t steps() const { return steps_; }

  // Signed so that rolling in the calibration direction reads positive.
  float distanceMm() const {
    if (stepsPer10cm_ == 0) return 0.0f;
    return steps_ * kCalibrationMm / stepsPer10cm_;
  }
  // Length of one encoder step, i.e. the ruler's resolution.
  float mmPerStep() const {
    if (stepsPer10cm_ == 0) return 0.0f;
    return kCalibrationMm / abs(stepsPer10cm_);
  }

private:
  Mode mode_ = Mode::Calibrating;
  bool pressed_ = false;
  int32_t stepsPer10cm_ = 0;
  int32_t calSteps_ = 0;
  int32_t steps_ = 0;
};
