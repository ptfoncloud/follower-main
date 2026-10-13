#pragma once
/*  ================================================================
    safety_gate.h — the single most safety-relevant decision in this
    firmware, deliberately isolated with ZERO Arduino/hardware
    dependency so it can be compiled and unit tested on a desktop,
    not just trusted because it "looked right" on the bench.

    This header is included by main.cpp for the real flight build, and
    by test/native/test_safety_gate.cpp for the desktop test build —
    same function, two build targets, so a passing test actually means
    something about the real flight code.
    ================================================================ */

#include <stdint.h>

struct SafetyGateResult {
    bool     should_command;
    const char* reason;   // human-readable, goes straight to Serial.printf() in flight
};

/*  The FC must be armed, in the required flight mode, and its
    heartbeat must be recent — all three, every single call. This is
    intentionally NOT allowed to "fail open" on any single missing
    condition. */
inline SafetyGateResult evaluate_safety_gate(
    bool fc_armed,
    uint32_t fc_custom_mode,   // full HEARTBEAT.custom_mode — never truncate
    uint32_t required_mode,    // to 8 bits: 0x104 would then read as GUIDED (4)
    uint32_t last_heartbeat_ms,
    uint32_t now_ms,
    uint32_t link_timeout_ms
) {
    if (!fc_armed) {
        return { false, "FC not armed" };
    }
    if (fc_custom_mode != required_mode) {
        return { false, "FC not in required mode" };
    }
    // Guards against millis() rollover (happens every ~49 days) producing
    // a huge underflowed value that would otherwise look like a fresh
    // heartbeat. Rare in a short flight, cheap to guard against anyway.
    if (now_ms < last_heartbeat_ms) {
        return { false, "clock rollover — treating link as stale" };
    }
    if ((now_ms - last_heartbeat_ms) > link_timeout_ms) {
        return { false, "FC link stale" };
    }
    return { true, "ok" };
}
