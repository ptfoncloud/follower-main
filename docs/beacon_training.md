# Training the pixy2 beacon

A badly trained signature is the most likely thing to cause problems on
the first flight. It'll either trigger on random background colors or
not lock onto the beacon at all. Do it on the bench before the pixy is
on the drone.

## What you need

- Pixy2 plugged into a laptop over USB (not the ESP32, train it on its own first)
- PixyMon2 from pixycam.com
- The actual beacon you'll carry. Solid, saturated color that doesn't
  show up anywhere else in the room. Bright pink or cyan usually work.
  Skip red if there's red furniture around and skip green outdoors.

## Steps

1. Plug the pixy into the laptop and open PixyMon2. You should get a
   live feed in a few seconds.
2. Set the program to Color Connected Components (CCC). It's the
   default and it's what the firmware uses (`pixy.changeProg("ccc")`).
3. Hold the beacon about where it'll be in flight, a few feet away, in
   the same lighting you'll fly in. Lighting matters a lot. Train
   indoors if you fly indoors, outdoors if outdoors.
4. Drag a box around just the beacon in the feed, as tight as you can
   without cutting the edges off.
5. Check what signature number it got. PixyMon shows it in the block
   list as `SIG=` 1-7. That number goes in `config.h`:
   ```c
   #define PRIMARY_SIG   <that number>
   ```
6. Test it before unplugging. Move the beacon around: near, far, tilted,
   partly covered. You want:
   - a block every time the beacon is visible
   - the block going away cleanly when it's out of frame, no lingering
     or flickering
   - no blocks on other stuff (walls, skin, clothes). If something keeps
     matching, retrain with a tighter box or use a different color.
7. If one background object keeps false triggering, don't try to fix it
   by training more regions into the signature (ctrl+drag). That
   widens the match, which is the opposite of what you want. Change the
   beacon color or move/cover the thing.

## Check before flying

Flash `pixy_bench` (or the flight firmware) and watch the serial
monitor. With the beacon sitting still in frame it should stay on
PRIMARY (LED solid) and not flip to LOST. If it flickers the signature
isn't clean, go back to step 4.

## Lighting

Pixy matches hue/saturation, not exact pixels, so big lighting changes
between training and flying (fluorescent vs overcast vs direct sun)
do matter. If you fly in different conditions retrain each session. A
bad match just means more time in LOST (holding position). There's no
other person tracker behind the beacon, the line fallback is off because
it tracks floor seams and edges, not people.
