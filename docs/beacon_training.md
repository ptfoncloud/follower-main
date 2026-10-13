# Training the Pixy2 Beacon Color Signature

This is the single most likely thing to bite you on first flight — a
badly-trained signature will false-trigger on random background colors
or fail to lock onto the real beacon at all. Do this carefully, once,
on the bench, before it's on a flying drone.

## What you need

- Pixy2, connected to a laptop via USB (not the ESP32 — train it
  standalone first)
- PixyMon2 (download from pixycam.com — the official config/monitor tool)
- The actual beacon you'll wear/carry during flight — a solid, saturated
  color, ideally something that does NOT appear elsewhere in your flying
  space (bright pink or cyan are usually safe; avoid red if you have red
  furniture/walls nearby, avoid green outdoors on grass)

## Steps

1. **Plug Pixy2 into your laptop via USB.** Open PixyMon2. You should
   see a live camera feed within a few seconds.

2. **Set the program to Color Connected Components (CCC)** — this is
   the default mode and matches `pixy.changeProg("ccc")` in the
   firmware.

3. **Hold the beacon in front of the camera**, roughly where it'll
   actually be relative to the drone during a real flight (a few feet
   away, similar lighting to where you'll fly). Lighting genuinely
   matters — a signature trained under indoor LED light can perform
   differently outdoors in daylight. If you're flying indoors, train
   indoors. If outdoors, train outdoors, in similar light to your
   planned flight time.

4. **In PixyMon2, click and drag a selection box around just the
   beacon** in the live video feed — as tight as you reasonably can
   without cutting off edges. This teaches Pixy2 what color range
   counts as a match.

5. **Check the assigned signature number** — PixyMon2 shows this in
   the block list (top-right area) as `SIG=` followed by a number 1-7.
   Whatever number it assigns, that's what goes in `config.h`:
   ```c
   #define PRIMARY_SIG   <that number>
   ```

6. **Test it before disconnecting.** Move the beacon around the frame —
   near, far, tilted, partially occluded. Watch the block list. You
   want:
   - A block appearing consistently whenever the beacon is visible
   - The block disappearing cleanly when the beacon is out of frame
     (not lingering, not flickering on/off rapidly)
   - No FALSE blocks appearing on background objects (walls, skin tone,
     other clothing) — if something else in the room keeps matching,
     either retrain with a tighter box or pick a different beacon color

7. **If false-positives keep happening on a specific background object**
   — PixyMon2 lets you train multiple regions into the same signature
   (hold Ctrl while dragging a new box) to broaden a good match, but
   that's the wrong tool for FIXING a false-positive. To fix a false
   positive, either change the beacon color entirely, or physically
   move/cover whatever's triggering it during flight.

## Sanity check before flying

With the beacon trained, run the ESP32 firmware and watch the serial
monitor (`pio device monitor`). You should see the LED go solid
(PRIMARY mode) when the beacon is visible and NOT flicker between
PRIMARY/LOST when the beacon just sits still in frame. If it's
flickering, the signature isn't clean — go back to step 4 and retrain
with a tighter selection.

## One thing worth knowing

Pixy2's signature matching is a hue/saturation match, not exact-pixel.
It WILL be somewhat sensitive to big lighting changes between training
and flying (indoor fluorescent vs outdoor overcast vs outdoor direct
sun are genuinely different enough to matter). If you're planning to
fly in multiple lighting conditions, retrain per-session. A signature
that doesn't match the day's light means the drone spends more time in
LOST (holding position) — there is no person-tracking fallback behind
the beacon; the line-feature fallback is disabled by default because it
tracks floor seams and edges, not people.
