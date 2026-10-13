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
  AND in GUIDED**. Switching to any other mode (LOITER, STABILIZE,
  RTL) from your transmitter immediately and silently stops the
  follower logic — the FC keeps flying whatever you told it to, the
  ESP32 just stops adding its own input on top. This is the actual
  override mechanism. No separate "disable follower" switch exists
  because none is needed — mode-switching already does it.
- Tracking state (`PRIMARY` / `LOST`) is visible on the status LED and
  in the serial log. About 1 s (`FALLBACK_CYCLES`) after the beacon
  disappears the ESP32 goes to `LOST` and commands zero velocity —
  holding position, not descending or doing anything unexpected. It
  resumes tracking automatically when the beacon reappears in frame.
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
  further from the companion computer, just continues under manual RC
  control or its own current mode — it does not fail safe *because*
  the companion went quiet, since GUIDED-mode velocity targets
  naturally stop being refreshed and the FC's own timeout behavior on
  stale guided targets takes over (verify this on your specific
  ArduPilot version/params — `GUID_OPTIONS` and stale-target handling
  can vary by firmware version).

## Abort procedure

At any point, in order of preference:

1. **Switch flight mode** on the transmitter (to LOITER, STABILIZE, or
   RTL). This is the fastest, most reliable override — it stops the
   ESP32's velocity commands from having any effect within one
   heartbeat cycle (well under a second) regardless of what the
   tracking state machine thinks it's doing.
2. **Disarm** if altitude and situation allow — the ESP32's gate
   refuses to command an unarmed vehicle by construction, so this is
   an unambiguous stop.
3. **RTL** if you want the vehicle to come home rather than just hold
   or land immediately.

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
