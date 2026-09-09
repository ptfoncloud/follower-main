#pragma once
/*  config.h — single place to tune the follower without touching logic */

// ── Pixy2 ─────────────────────────────────────────────────────────
#define PRIMARY_SIG         1       // Pixy2 colour signature for the beacon
                                     // (train this in PixyMon first — see
                                     //  docs/beacon_training.md)
#define FRAME_W             316
#define FRAME_H             208

// ── Dead zones (pixels) ───────────────────────────────────────────
#define DEAD_X              25
#define DEAD_Y              20

// ── Distance control (blob area, px²) ────────────────────────────
#define TARGET_AREA_NEAR    8000
#define TARGET_AREA_FAR     2000

// ── Timeouts ──────────────────────────────────────────────────────
#define LOST_TIMEOUT_MS     2000
#define FALLBACK_CYCLES     10

// ── Velocity limits (m/s) ─────────────────────────────────────────
// START LOWER than this on first flight — halve both until you trust it.
#define VEL_MAX_XY          1.5f
#define VEL_MAX_Z           0.5f

// ── MAVLink / serial ──────────────────────────────────────────────
// FC_SERIAL is UART2 on the ESP32 side (GPIO16=RX, GPIO17=TX), wired to
// SERIAL1 (physical UART1) on the MicoAir743-AIO — that port defaults
// to MAVLink2 already, confirmed via ArduPilot's own board page. Baud
// below MUST match SERIAL1_BAUD in follower.param (57 -> 57600).
#define FC_SERIAL       Serial2
#define FC_BAUD         57600
#define STATUS_LED      2

#define MY_SYSID            1
#define MY_COMPID           MAV_COMP_ID_PERIPHERAL
#define FC_SYSID            1
#define FC_COMPID           MAV_COMP_ID_AUTOPILOT1
