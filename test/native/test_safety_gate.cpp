/*  Desktop-compilable tests for safety_gate.h. Plain asserts, no test
    framework dependency — keeps the native build trivial (just g++,
    no PlatformIO test runner needed to at least run this locally).
    CI (.github/workflows/build.yml) compiles it the same way, with
    g++ -std=c++17 -Wall -Wextra -Werror. */

#include <cassert>
#include <cstdio>
#include "../../src/safety_gate.h"
// The real flight constants, not test-local copies: if FC_LINK_TIMEOUT_MS
// or ARDUCOPTER_MODE_GUIDED change in config.h, these tests follow.
// (config.h's MAVLink macros are never expanded here, so it needs no
// Arduino/MAVLink headers.)
#include "../../src/config.h"

#define LINK_TIMEOUT_MS FC_LINK_TIMEOUT_MS

int tests_run = 0;
#define CHECK(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while (0)

int main() {
    // ---- Not armed: refuse regardless of anything else ----
    {
        auto r = evaluate_safety_gate(false, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                       1000, 1000, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- Armed but wrong mode: refuse ----
    {
        auto r = evaluate_safety_gate(true, 0 /* STABILIZE */, ARDUCOPTER_MODE_GUIDED,
                                       1000, 1000, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- Armed, correct mode, fresh heartbeat: allow ----
    {
        auto r = evaluate_safety_gate(true, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                       1000, 1000, LINK_TIMEOUT_MS);
        CHECK(r.should_command == true);
    }

    // ---- Armed, correct mode, heartbeat right at the timeout boundary: allow ----
    {
        auto r = evaluate_safety_gate(true, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                       1000, 1000 + LINK_TIMEOUT_MS, LINK_TIMEOUT_MS);
        CHECK(r.should_command == true);
    }

    // ---- Armed, correct mode, heartbeat JUST past the timeout: refuse ----
    {
        auto r = evaluate_safety_gate(true, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                       1000, 1000 + LINK_TIMEOUT_MS + 1, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- Armed, correct mode, heartbeat WAY stale: refuse ----
    {
        auto r = evaluate_safety_gate(true, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                       1000, 60000, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- Pilot switches OUT of GUIDED mid-flight: refuse immediately,
    // even though armed and link is fresh - this is the exact scenario
    // that motivated building this gate in the first place ----
    {
        auto r = evaluate_safety_gate(true, 5 /* LOITER */, ARDUCOPTER_MODE_GUIDED,
                                       5000, 5010, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- Clock rollover guard: now_ms < last_heartbeat_ms should not
    // be treated as "impossibly fresh" ----
    {
        auto r = evaluate_safety_gate(true, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                       4000000000u, 100, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- Exactly at arm/disarm transition: disarmed should immediately
    // block, no residual "was armed a moment ago" grace period ----
    {
        auto armed_r = evaluate_safety_gate(true, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                             1000, 1000, LINK_TIMEOUT_MS);
        CHECK(armed_r.should_command == true);
        auto disarmed_r = evaluate_safety_gate(false, ARDUCOPTER_MODE_GUIDED, ARDUCOPTER_MODE_GUIDED,
                                                1000, 1001, LINK_TIMEOUT_MS);
        CHECK(disarmed_r.should_command == false);
    }

    // ---- HEARTBEAT.custom_mode is uint32. A mode whose low byte is 4
    // (260 = 0x104) is NOT GUIDED and must not pass by truncation ----
    {
        uint32_t custom_mode_260 = 0x104;
        auto r = evaluate_safety_gate(true, custom_mode_260, ARDUCOPTER_MODE_GUIDED,
                                       1000, 1000, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- Same with the high bits set: 0xFFFFFF04 is not GUIDED ----
    {
        uint32_t custom_mode_hi = 0xFFFFFF04u;
        auto r = evaluate_safety_gate(true, custom_mode_hi, ARDUCOPTER_MODE_GUIDED,
                                       1000, 1000, LINK_TIMEOUT_MS);
        CHECK(r.should_command == false);
    }

    // ---- The test really is using the flight timeout from config.h ----
    CHECK(LINK_TIMEOUT_MS == 3000);

    printf("ALL %d CHECKS PASSED\n", tests_run);
    return 0;
}
