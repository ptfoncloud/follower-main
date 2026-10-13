#pragma once
/*  safety_gate.h - decides whether the ESP32 is allowed to send a
    velocity command. No Arduino deps on purpose so it can be compiled
    and tested on desktop.

    main.cpp includes this for the flight build and
    test/native/test_safety_gate.cpp includes it for the tests, so the
    tests are testing the real thing. */

#include <stdint.h>

struct SafetyGateResult {
    bool     should_command;
    const char* reason;   // printed to serial in flight
};

/*  FC has to be armed, in the required mode, and have a recent heartbeat.
    All three, every call. Any one missing = no command. */
inline SafetyGateResult evaluate_safety_gate(
    bool fc_armed,
    uint32_t fc_custom_mode,   // full 32 bit HEARTBEAT.custom_mode. don't truncate
    uint32_t required_mode,    // to 8 bits or 0x104 reads as GUIDED (4)
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
    // millis() rolls over every ~49 days. Treat that as stale instead of
    // letting it look like a fresh heartbeat. Won't happen mid flight but
    // it's cheap to check.
    if (now_ms < last_heartbeat_ms) {
        return { false, "clock rollover — treating link as stale" };
    }
    if ((now_ms - last_heartbeat_ms) > link_timeout_ms) {
        return { false, "FC link stale" };
    }
    return { true, "ok" };
}
