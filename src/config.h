#pragma once
/*  config.h — every tunable number in the firmware, in one place,
    each with the reasoning behind it. If you're changing behavior,
    change it here, not in main.cpp — main.cpp should only ever need
    touching to change LOGIC, not numbers. */

// ── Pixy2 ─────────────────────────────────────────────────────────
#define PRIMARY_SIG         1
// Which trained color signature (1-7) counts as "the beacon."
// Train this in PixyMon first (docs/beacon_training.md) — whatever
// signature number PixyMon assigns your beacon's color goes here.

#define FRAME_W             316
#define FRAME_H             208
// Pixy2's native output resolution in CCC (color-connected-components)
// mode. Fixed by the hardware, not something to tune — every pixel
// coordinate the tracking code works with is relative to this frame
// size, so changing these without actually changing what Pixy2 outputs
// would silently break the center-of-frame math everywhere below.

// ── Dead zones (pixels) ───────────────────────────────────────────
#define DEAD_X              25
#define DEAD_Y              20
// How far off-center the beacon has to be before the drone reacts at
// all. Without this, small tracking noise near dead-center would
// produce a constant stream of tiny, jittery velocity corrections —
// this is what makes the drone hold still when the beacon is "close
// enough" to centered rather than micro-adjusting forever.

// ── Distance control (blob area, px²) ────────────────────────────
#define TARGET_AREA_NEAR    8000
#define TARGET_AREA_FAR     2000
// The tracked blob's pixel area is used as a proxy for distance —
// bigger blob = beacon is closer to the camera. Area above
// TARGET_AREA_NEAR means "too close, back off"; below
// TARGET_AREA_FAR means "too far, approach." Between the two, the
// drone doesn't adjust forward/back at all — this is a dead zone for
// distance, the same idea as DEAD_X/DEAD_Y but for the Z axis.
// These two starting values assume a beacon of a specific real-world
// size — if you change the beacon's physical size, these will need
// re-tuning to match, since a bigger beacon reads as "closer" at any
// given real distance than a smaller one does.

// ── Timeouts ──────────────────────────────────────────────────────
#define LOST_TIMEOUT_MS     2000
// How long the tracker can go without a beacon detection (in either
// PRIMARY or FALLBACK mode) before giving up and switching to LOST,
// where the drone holds position instead of continuing to guess.

#define FALLBACK_CYCLES     10
// How many CONSECUTIVE frames of "no beacon found" in PRIMARY mode
// before giving up on PRIMARY — to LOST (hold position) by default, or
// to the FALLBACK line tracker if ENABLE_LINE_FALLBACK is 1. Not
// time-based like LOST_TIMEOUT_MS — this is frame-count-based, so it
// scales naturally with however fast the main loop actually runs.

#define ENABLE_LINE_FALLBACK 0
// 0 (default): losing the beacon goes straight to LOST = hold position.
// 1: losing the beacon switches Pixy2 to its "line" program and steers
//    toward the first line vector it finds. SAFETY: that program detects
//    line FEATURES (floor seams, table edges, door frames), not people —
//    commanding velocity toward them can steer the vehicle at random
//    structure. Only enable for deliberate bench experiments.

// ── Velocity limits (m/s) ─────────────────────────────────────────
// Hard caps applied to every velocity command right before it's sent —
// this is the actual ceiling on how fast the drone can be told to
// move by the tracking logic, regardless of what the raw tracking
// math computes. START LOWER than this on first flight (try halving
// both) until you trust the tuning.
#define VEL_MAX_XY          1.5f
#define VEL_MAX_Z           0.5f

// ── MAVLink / serial link to the FC ──────────────────────────────
// FC_SERIAL is UART2 on the ESP32 side (GPIO16=RX, GPIO17=TX), wired
// to SERIAL1 (physical UART1) on the MicoAir743-AIO — that port
// defaults to MAVLink2 already, confirmed via ArduPilot's own board
// page (see params/follower.param's header comment for the full real
// UART table). FC_BAUD MUST match SERIAL1_BAUD in follower.param
// (57 there means 57600 here — ArduPilot's param stores it /100).
#define FC_SERIAL       Serial2
#define FC_BAUD         57600
#define STATUS_LED      2
// Onboard LED, used as the at-a-glance tracking/link state indicator —
// see docs/OPERATIONS.md's pre-arm checklist for what each blink
// pattern means.

// ── MAVLink identity ──────────────────────────────────────────────
// Every MAVLink message needs a system ID + component ID on both
// ends. These aren't arbitrary — MY_SYSID/MY_COMPID identify THIS
// ESP32 as a peripheral (component type 158) on vehicle system 1;
// FC_SYSID/FC_COMPID identify the flight controller we're talking to,
// which is also system 1 (same vehicle) but the AUTOPILOT1 component.
// Only worth changing if you're running multiple vehicles/companions
// on the same MAVLink network and need to disambiguate them.
#define MY_SYSID            1
#define MY_COMPID           MAV_COMP_ID_PERIPHERAL
#define FC_SYSID            1
#define FC_COMPID           MAV_COMP_ID_AUTOPILOT1

// ── Safety gate ────────────────────────────────────────────────────
#define ARDUCOPTER_MODE_GUIDED  4
// ArduCopter's own mode numbering — GUIDED is mode 4 in every current
// ArduCopter release, not something specific to this board or vehicle.
// This is the ONLY mode the ESP32 is allowed to command velocity in —
// see safety_gate.h for the actual gating logic, and
// docs/ENGINEERING_DECISIONS.md for why gating on mode+armed exists
// at all rather than trusting the tracking loop to just behave.

#define FC_LINK_TIMEOUT_MS      3000
// No FC heartbeat this recent = treat the link as dead and stop
// commanding, regardless of what the tracking state machine thinks
// it's doing. Verified directly in test/native/test_safety_gate.cpp —
// that test checks THIS exact number's behavior at the boundary
// (3000ms since last heartbeat still allows a command, 3001ms
// refuses), so if you change this value, re-run the native tests
// before trusting the new number in flight.
