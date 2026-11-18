# Scroll Ruler

A digital ruler made from an M5Stack **AtomS3R**, an **Atomic Battery Base** and a
**Unit Scroll** plugged into the AtomS3R's Grove port. You press the scroll wheel
down and roll it along whatever you want to measure, and the screen shows the
distance in centimetres.

I built it during my internship at M5Stack. The write-up, with a demo video and a
working copy of the ruler, is at
[chiambucket.com/scroll-ruler](https://www.chiambucket.com/scroll-ruler).

## Flash it

Open [Flashdeck](https://flashdeck.shebang.pro/) in Chrome or Edge, choose
**Scroll Wheel Ruler**, plug the AtomS3R in over USB-C and press **Install**.
Flashdeck writes the firmware straight from the browser.

To use esptool instead, download the
[firmware image](https://github.com/New-Kringster/AtomS3R_ScrollWheelRuler/releases/latest/download/scroll-ruler-atoms3r.bin)
and write it at offset 0:

```sh
esptool --chip esp32s3 --port <your port> --baud 921600 write_flash 0x0 scroll-ruler-atoms3r.bin
```

## What you need

- [AtomS3R](https://shop.m5stack.com/products/atoms3r-dev-kit), which runs the ruler; its whole screen is also its button
- [Atomic Battery Base](https://shop.m5stack.com/products/atomic-battery-base-200mah), so it runs without a cable
- [Unit Scroll](https://docs.m5stack.com/en/unit/UNIT-Scroll), the wheel you roll, plugged into the Grove port

## Use it

- **Measure.** Press the wheel down and roll it forward. The distance counts only
  while the wheel stays pressed, and the Unit Scroll's LED glows green while it
  counts. Releasing the wheel stops the count, and rolling backwards while it is
  pressed takes distance off again.
- **Zero.** Tap the screen.
- **Recalibrate.** Hold the screen for about a second.

## Calibrate it

The first time it starts, and whenever you recalibrate, the ruler measures 10 cm to
learn how far the wheel travels per step:

1. Press the wheel down and roll it exactly 10 cm along a real ruler, then release
   it. The LED glows blue while it waits for you to start.
2. The screen shows how many steps that took. Tap the screen to save it, press the
   wheel again to keep adding to the same roll, or hold the screen to start over.
3. Before you have rolled anything, a tap cancels and keeps the previous calibration.

The calibration is kept in flash, so it survives a power cut, and the direction you
rolled in becomes forward. The wheel only has so many steps in a turn, so the ruler
measures in steps of about 2 mm (49 steps for 10 cm on mine), and the screen shows
the exact step length under each reading.

## Build it yourself

```sh
pio test -e native                      # unit tests for lib/RulerCore, run on the computer
pio run -e m5stack-atoms3r              # build the firmware
pio run -e m5stack-atoms3r -t upload    # flash it over USB-C
pio device monitor                      # serial log at 115200
```

| Path | What it is |
|---|---|
| `lib/RulerCore/RulerCore.h` | the ruler's logic with no hardware code: the modes, the calibration and the distance maths |
| `src/main.cpp` | the hardware side: the Unit Scroll over I2C (address 0x40, G2 SDA, G1 SCL), saving the calibration, the screen and the button |
| `test/test_ruler_core/test_main.cpp` | eleven unit tests for `RulerCore` |
