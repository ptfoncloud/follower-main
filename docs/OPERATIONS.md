# Follower — Operations Runbook

A real procedure, not just code comments. If you're about to fly, read
this section by section in order.

## Pre-arm checklist

Run through this every time, not just the first flight — a five-minute
habit that catches most of what actually goes wrong.

1. **Props off for the first power-up of the session.** Confirm the
   ESP32's status LED goes solid (not blinking) within a few seconds —
   solid means Pixy2 initialized and the FC heartbeat is being received.
   Slow blink = still waiting for the FC. Fast blink = FC link is
   stale or lost after previously connecting.
2. **Check the serial monitor** (`pio device monitor`) for
   `[FC] Armed state changed` and `[GATE]` lines. If you arm the FC via
   RC or GCS and DON'T see the armed-state line appear on the ESP32's
   console within ~1 second, stop — the ESP32 isn't seeing the FC's
   heartbeat, which means it will never issue velocity commands even
   if everything else looks fine.
3. **Verify GUIDED mode is actually reachable.** Switch into GUIDED
   from your transmitter (or GCS) with props off, confirm the FC
   accepts it (check Mission Planner or the dashboard) — some
   ArduPilot arming checks (EKF health, GPS if enabled) can silently
   refuse a mode change.
4. **Confirm the beacon trains cleanly** — hold it in frame, watch for
   `[FOLLOWER] Primary lost -> LOST` NOT appearing while the beacon
   sits still in view. If it flickers between PRIMARY and LOST with a
   stationary beacon, stop and retrain (`docs/beacon_training.md`)
   before flying, not after.
5. **Battery voltage check** — confirm it reads sane on the dashboard
   or OSD before arming. `BATT_ARM_VOLT` in `follower.param` should
   already refuse an unsafe pack, but verify it's not right at the
   edge of that threshold before you're relying on it mid-flight.

## Normal operation

- The ESP32 sends velocity commands only while the FC reports **armed
  AND in GUIDED**. Switching to any other mode (LOITER, ALT_HOLD,
  STABILIZE, LAND) from your transmitter immediately and silently stops the
  follower logic — the FC keeps flying whatever you told it to, the
  ESP32 just stops adding its own input on top. This is the actual
  override mechanism. No separate "disable follower" switch exists
  because none is needed — mode-switching already does it.
- Tracking state (`PRIMARY` / `LOST`) is visible on the status LED and
  in the serial log. **The first frame without the beacon commands zero
  velocity** (hold position) — the FC never keeps flying the last
  velocity on stale information. About 1 s (`FALLBACK_CYCLES`) of misses
  later the state changes to `LOST`, which keeps holding. Nothing
  descends or does anything unexpected; tracking resumes automatically
  when the beacon reappears.
- There is a `FALLBACK` state that steers using Pixy2's "line"
  program, but it is **compiled out by default**
  (`ENABLE_LINE_FALLBACK 0` in `src/config.h`). That program detects
  line features — floor seams, table edges — not people, so steering
  toward them could fly the vehicle at random structure. Leave it off
  for flight.

## Link loss

Two independent link-loss paths, handled differently:

- **RC link loss** (pilot's transmitter to the RP1 receiver): handled
  entirely by the FC's own ArduPilot failsafe (`FS_THR_ENABLE` etc in
  `follower.param`). The ESP32 has no role here — this is exactly why
  RC failsafe exists as flight-controller-level, not companion-level,
  behavior.
- **ESP32 ↔ FC link loss** (a UART/wiring fault, not RC): the ESP32
  itself detects this via `FC_LINK_TIMEOUT_MS` (3 seconds of no FC
  heartbeat) and stops sending velocity commands — verified directly
  by `test/native/test_safety_gate.cpp`. The FC, hearing nothing
  further from the companion computer, stays in GUIDED and **stops
  after 3 s** without a fresh velocity target (ArduPilot guided-mode
  docs). It does not trigger a failsafe (`FS_GCS_ENABLE,0`); switch mode
  on the transmitter to take over. Bench-test this before flying
  (REVIEW.md, hardware test "Link-loss stop").
- **No RTL on this build.** Indoors the GPS has no fix, so there is no
  home position, so ArduCopter refuses RTL — and if it ever engaged it
  would climb to `RTL_ALT` (default 15 m) first. Every failsafe in
  `follower.param` LANDs instead, and gestures use LAND too.

## Abort procedure

At any point, in order of preference:

1. **Switch flight mode** on the transmitter, out of GUIDED (to
   LOITER or ALT_HOLD to hold, STABILIZE for full manual). This is the
   fastest, most reliable override — the ESP32's velocity commands stop
   having any effect within one heartbeat (well under a second),
   whatever the tracking state machine thinks it's doing.
2. **LAND** (mode switch position or console) to bring it straight down.
3. **Disarm once landed.** Disarming in the air (e.g. the console's
   disarm button) stops the motors immediately and the drone falls —
   ArduPilot does not block it. Reserve that for a real emergency
   (vehicle out of control near people); a transmitter emergency-stop
   switch (`RCx_OPTION,31` MotorEStop, see `docs/FIRST_FLIGHT.md`) is the
   deliberate way to do it.

Do not use RTL: there is no home position on this build (see above).

There is deliberately no separate "kill the ESP32" procedure as a
primary abort path — the transmitter mode switch is faster and doesn't
depend on the companion computer being reachable at all, which is the
point.

## Post-flight

- Check the serial log for any `[GATE]` suppression messages you
  didn't expect — a pattern of "FC link stale" warnings during a
  flight that otherwise felt fine is worth investigating before the
  next flight, not dismissed as noise.
- If the beacon tracking felt unreliable, note the lighting conditions
  — see the "one thing worth knowing" section at the bottom of
  `docs/beacon_training.md` before assuming it's a hardware problem.
