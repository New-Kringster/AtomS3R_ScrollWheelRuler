// Runs on the PC: `pio test -e native`
#include <unity.h>
#include "RulerCore.h"

using Mode = RulerCore::Mode;

void setUp() {}
void tearDown() {}

static void test_boots_into_calibration_without_saved_value() {
  RulerCore r(0);
  TEST_ASSERT_EQUAL(Mode::Calibrating, (int)r.mode());
  TEST_ASSERT_FALSE(r.canCancelCalibration());
}

static void test_boots_into_measuring_with_saved_value() {
  RulerCore r(40);
  TEST_ASSERT_EQUAL(Mode::Measuring, (int)r.mode());
  TEST_ASSERT_EQUAL_INT32(40, r.stepsPer10cm());
}

static void test_calibration_roll_release_tap_saves() {
  RulerCore r(0);
  r.update(10, true);
  r.update(30, true);
  TEST_ASSERT_EQUAL(Mode::Calibrating, (int)r.mode());
  r.update(0, false);
  TEST_ASSERT_EQUAL(Mode::Confirming, (int)r.mode());
  TEST_ASSERT_TRUE(r.tap());
  TEST_ASSERT_EQUAL(Mode::Measuring, (int)r.mode());
  TEST_ASSERT_EQUAL_INT32(40, r.stepsPer10cm());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, r.mmPerStep());
}

static void test_releasing_before_rolling_stays_in_calibration() {
  RulerCore r(0);
  r.update(0, true);
  r.update(0, false);
  TEST_ASSERT_EQUAL(Mode::Calibrating, (int)r.mode());
}

static void test_pressing_again_continues_calibration_roll() {
  RulerCore r(0);
  r.update(20, true);
  r.update(0, false);
  r.update(15, true);
  r.update(0, false);
  TEST_ASSERT_EQUAL_INT32(35, r.calibrationSteps());
}

static void test_too_short_calibration_is_not_saved() {
  RulerCore r(0);
  r.update(2, true);
  r.update(0, false);
  TEST_ASSERT_FALSE(r.tap());
  TEST_ASSERT_EQUAL(Mode::Confirming, (int)r.mode());
}

static void test_only_counts_while_pressed() {
  RulerCore r(40);
  r.update(8, true);
  r.update(100, false);  // wheel spun while lifted: ignored
  r.update(4, true);
  TEST_ASSERT_EQUAL_INT32(12, r.steps());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.0f, r.distanceMm());
}

static void test_reversed_wheel_still_reads_positive_forward() {
  // Calibrated rolling "forward" produced negative counts.
  RulerCore r(-40);
  r.update(-20, true);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, r.distanceMm());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, r.mmPerStep());
}

static void test_tap_resets_measurement() {
  RulerCore r(40);
  r.update(16, true);
  r.update(0, false);
  TEST_ASSERT_FALSE(r.tap());
  TEST_ASSERT_EQUAL_INT32(0, r.steps());
}

static void test_hold_restarts_calibration_and_tap_can_cancel() {
  RulerCore r(40);
  r.hold();
  TEST_ASSERT_EQUAL(Mode::Calibrating, (int)r.mode());
  r.tap();  // nothing rolled yet: back to the old calibration
  TEST_ASSERT_EQUAL(Mode::Measuring, (int)r.mode());
  TEST_ASSERT_EQUAL_INT32(40, r.stepsPer10cm());
}

static void test_hold_during_confirm_discards_roll() {
  RulerCore r(0);
  r.update(30, true);
  r.update(0, false);
  r.hold();
  TEST_ASSERT_EQUAL(Mode::Calibrating, (int)r.mode());
  TEST_ASSERT_EQUAL_INT32(0, r.calibrationSteps());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boots_into_calibration_without_saved_value);
  RUN_TEST(test_boots_into_measuring_with_saved_value);
  RUN_TEST(test_calibration_roll_release_tap_saves);
  RUN_TEST(test_releasing_before_rolling_stays_in_calibration);
  RUN_TEST(test_pressing_again_continues_calibration_roll);
  RUN_TEST(test_too_short_calibration_is_not_saved);
  RUN_TEST(test_only_counts_while_pressed);
  RUN_TEST(test_reversed_wheel_still_reads_positive_forward);
  RUN_TEST(test_tap_resets_measurement);
  RUN_TEST(test_hold_restarts_calibration_and_tap_can_cancel);
  RUN_TEST(test_hold_during_confirm_discards_roll);
  return UNITY_END();
}
