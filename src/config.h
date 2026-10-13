#pragma once
/*  config.h - every tunable number in the firmware lives here.
    Changing behavior = change it here. main.cpp should only need
    touching to change logic, not numbers. */

// -- Pixy2 ------------------------------------------------------------
#define PRIMARY_SIG         1
// Which trained color signature (1-7) is the beacon. Train it in PixyMon
// first (docs/beacon_training.md) and put whatever number it gives here.

#define FRAME_W             316
#define FRAME_H             208
// Pixy2's output resolution in CCC mode. Fixed by the hardware, not a
// tuning value. All the center-of-frame math depends on these.

// -- Dead zones (pixels) ----------------------------------------------
#define DEAD_X              25
#define DEAD_Y              20
// How far off center the beacon has to be before it reacts at all.
// Without this, tracking noise near center turns into constant tiny
// jittery corrections.

// -- Distance control (blob area, px^2) -------------------------------
#define TARGET_AREA_NEAR    8000
#define TARGET_AREA_FAR     2000
// Blob area is the distance proxy, bigger = closer. Above NEAR means too
// close so back off, below FAR means too far so move in, in between
// it doesn't move forward/back (dead zone like DEAD_X/Y). These assume a
// specific beacon size, change the beacon and these need retuning.

// -- Timeouts ---------------------------------------------------------
#define LOST_TIMEOUT_MS     2000
// How long without a detection (PRIMARY or FALLBACK) before going to
// LOST and holding position.

#define FALLBACK_CYCLES     10
// Consecutive missed frames in PRIMARY before giving up on it. Goes to
// LOST (hold) by default, or FALLBACK if ENABLE_LINE_FALLBACK is 1.
// Frame count not time, so it scales with the loop rate.

#define ENABLE_LINE_FALLBACK 0
// 0 (default): losing the beacon goes straight to LOST = hold position.
// 1: switches pixy to its "line" program and steers at the first line
//    vector. That finds line features (floor seams, table edges, door
//    frames) not people, so it can steer the drone into random stuff.
//    Bench experiments only.

// -- Velocity limits (m/s) --------------------------------------------
// Hard caps on every command right before it's sent, whatever the
// tracking math comes up with. Start lower on the first flight (try
// halving both) until the tuning is trusted.
#define VEL_MAX_XY          1.5f
#define VEL_MAX_Z           0.5f

// -- Serial link to the FC --------------------------------------------
// ESP32 UART2 (GPIO16 RX, GPIO17 TX) to SERIAL1 / UART1 on the
// MicoAir743-AIO (full UART table is at the top of follower.param).
// FC_BAUD has to match SERIAL1_BAUD in follower.param (57 there = 57600).
#define FC_SERIAL       Serial2
#define FC_BAUD         57600
#define STATUS_LED      2
// Onboard LED. Blink patterns are in docs/OPERATIONS.md.

// -- MAVLink ids ------------------------------------------------------
// This ESP32 is a peripheral (compid 158) on vehicle sysid 1, the FC is
// AUTOPILOT1 on the same sysid. Only change if there are multiple
// vehicles/companions on the same network.
#define MY_SYSID            1
#define MY_COMPID           MAV_COMP_ID_PERIPHERAL
#define FC_SYSID            1
#define FC_COMPID           MAV_COMP_ID_AUTOPILOT1

// -- Safety gate ------------------------------------------------------
#define ARDUCOPTER_MODE_GUIDED  4
// GUIDED is mode 4 in ArduCopter. It's the only mode the ESP32 is allowed
// to send velocity in. Gate logic is in safety_gate.h, reasoning in
// docs/ENGINEERING_DECISIONS.md.

#define GATE_LOG_INTERVAL_MS    1000
// "[GATE] Suppressed: ..." prints when the reason changes, otherwise at
// most this often. Logging only, doesn't affect gating.

#define FC_LINK_TIMEOUT_MS      3000
// No FC heartbeat for this long = link is dead, stop commanding no matter
// what the tracker thinks. test/native/test_safety_gate.cpp uses this
// exact value at the boundary (3000 ms still allows, 3001 refuses), so
// rerun the native tests if you change it.
