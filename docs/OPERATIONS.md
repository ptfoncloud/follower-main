# Operations

Read this before every flying session. First time setup is in
`FIRST_FLIGHT.md`.

## Pre-arm checklist

Every session, not just the first one.

1. **Props off for the first power-up.** Watch the ESP32 LED:
   - slow blink (1 Hz): no FC heartbeat yet
   - fast blink: FC link stale, or tracker is LOST (no beacon)
   - solid: beacon is being tracked
2. **Serial monitor** (`pio device monitor`). Arm the FC from the
   transmitter or GCS and you should see `[FC] Armed state changed`
   within about a second. If you don't, stop. The ESP32 isn't hearing
   the FC and will never send commands.
3. **Check GUIDED is reachable.** Props off, switch to GUIDED and make
   sure the FC actually takes it (Mission Planner or the dashboard).
   EKF health checks can refuse the mode change.
4. **Beacon check.** Hold the beacon still in frame. If the log keeps
   flipping between PRIMARY and `Primary lost -> LOST`, retrain it
   (`beacon_training.md`) before flying.
5. **Battery.** Make sure voltage reads sane before arming.
   `BATT_ARM_VOLT` (15.2 V) will refuse a low pack but don't fly right
   at the edge of it.

## Normal operation

- The ESP32 only sends velocity commands while the FC is **armed and in
  GUIDED**. Switching to anything else (LOITER, ALT_HOLD, STABILIZE,
  LAND) stops the follower right away. That mode switch is the
  override, there's no separate "disable follower" switch.
- On the first frame without the beacon the ESP32 sends zero velocity,
  so the FC never keeps flying the last command on stale info. After
  about 1 s of misses (`FALLBACK_CYCLES`) it goes to LOST and keeps
  holding. It picks back up when the beacon comes back.
- There's a FALLBACK state that steers with pixy2's "line" program but
  it's compiled out (`ENABLE_LINE_FALLBACK 0` in `config.h`). Line mode
  finds floor seams and table edges, not people. Leave it off.

## Link loss

- **RC link loss** (transmitter to RP1): the FC's own failsafe handles
  it (`FS_THR_ENABLE,3` = land). The ESP32 isn't involved.
- **ESP32 to FC link loss** (wiring/uart fault): after 3 s with no FC
  heartbeat the ESP32 stops sending (`FC_LINK_TIMEOUT_MS`, covered by
  `test_safety_gate.cpp`). The FC stays in GUIDED and stops on its own
  after 3 s with no new velocity target. No failsafe triggers
  (`FS_GCS_ENABLE,0`), so switch modes on the transmitter to take over.
  Bench test this before flying (`FIRST_FLIGHT.md` section 6).
- **No RTL on this build.** Indoors the GPS has no fix so there's no
  home, and ArduCopter refuses RTL. If it ever did engage it would climb
  to `RTL_ALT` (15 m default) first. All failsafes LAND instead and the
  gestures use LAND too.

## Abort

In this order:

1. **Switch out of GUIDED** on the transmitter: LOITER or ALT_HOLD to
   hold, STABILIZE for full manual. Fastest option, the ESP32's commands
   stop mattering within one heartbeat no matter what the tracker
   thinks.
2. **LAND** (mode switch or console) to bring it straight down.
3. **Disarm once it's on the ground.** Disarming in the air (like the
   console disarm button) kills the motors and it falls, ArduPilot
   doesn't block that. Only for a real emergency like out of control
   near people, and the emergency stop switch (`RCx_OPTION,31`, see
   `FIRST_FLIGHT.md`) is the proper way to do it.

Don't use RTL, there's no home position (see above).

There's no "kill the ESP32" step on purpose. The transmitter mode
switch is faster and doesn't depend on the companion being reachable.

## Post-flight

- Look through the serial log for `[GATE]` messages you didn't expect.
  A bunch of "FC link stale" during a flight that felt fine is worth
  chasing down before the next one.
- If tracking was flaky, write down the lighting. See the bottom of
  `beacon_training.md` before blaming the hardware.
