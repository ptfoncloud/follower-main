/*  Desktop tests for esp32_thermal/thermal_wire.h, same style as
    test_safety_gate.cpp:

      g++ -std=c++17 -Wall -Wextra -Werror -o test_thermal_wire test/native/test_thermal_wire.cpp
      ./test_thermal_wire

    `./test_thermal_wire --golden out.bin` also writes the golden frame
    below. follower-server's tests/data/golden_frame_776.bin is that file,
    so the python decoder gets tested against real output from this
    encoder. */

#include <cmath>
#include <cstdio>
#include <cstring>
#include "../../esp32_thermal/thermal_wire.h"

int tests_run = 0;
#define CHECK(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while (0)

// golden frame: gradient 20..~34.7 C, a 33.5 C "person" at rows 10-13
// cols 14-17, pixel 0 = NaN. the python test builds the same thing in float32
static void golden_frame(float* f) {
    for (int i = 0; i < THERMAL_WIRE_PIXELS; i++) {
        int row = i / 32, col = i % 32;
        f[i] = 20.0f + 0.4f * (float)col + 0.1f * (float)row;
        if (row >= 10 && row < 14 && col >= 14 && col < 18) f[i] = 33.5f;
    }
    f[0] = NAN;
}

static float decode(const uint8_t* buf, int i) {
    float lo, scale;
    memcpy(&lo, buf, 4);
    memcpy(&scale, buf + 4, 4);
    return lo + (float)buf[THERMAL_WIRE_HEADER_BYTES + i] * scale;
}

int main(int argc, char** argv) {
    // ---- Fits in one WiFiUDP datagram ----
    CHECK(THERMAL_WIRE_FRAME_BYTES == 776);
    CHECK(THERMAL_WIRE_FRAME_BYTES <= 1460);

    float f[THERMAL_WIRE_PIXELS];
    uint8_t buf[THERMAL_WIRE_FRAME_BYTES];

    // ---- Golden frame: header + bounded quantization error ----
    golden_frame(f);
    CHECK(thermal_wire_encode(f, buf));
    float lo, scale;
    memcpy(&lo, buf, 4);
    memcpy(&scale, buf + 4, 4);
    // pixel 0 is NaN and has to be skipped, so min is row 1 col 0 = 20.1
    // not 20.0
    CHECK(fabsf(lo - 20.1f) < 1e-4f);
    CHECK(scale > 0.0f);
    float worst = 0.0f;
    for (int i = 1; i < THERMAL_WIRE_PIXELS; i++) {
        float err = fabsf(decode(buf, i) - f[i]);
        if (err > worst) worst = err;
    }
    CHECK(worst <= scale / 2.0f + 1e-4f);
    CHECK(buf[THERMAL_WIRE_HEADER_BYTES + 0] == 0);  // NaN -> coldest
    CHECK(decode(buf, 10 * 32 + 15) > 33.0f);         // hot block survives

    if (argc == 3 && strcmp(argv[1], "--golden") == 0) {
        FILE* out = fopen(argv[2], "wb");
        CHECK(out != nullptr);
        CHECK(fwrite(buf, 1, sizeof(buf), out) == sizeof(buf));
        fclose(out);
        printf("wrote %s (%d bytes)\n", argv[2], (int)sizeof(buf));
    }

    // ---- Uniform frame: scale 0, every value decodes exactly ----
    for (int i = 0; i < THERMAL_WIRE_PIXELS; i++) f[i] = 22.25f;
    CHECK(thermal_wire_encode(f, buf));
    memcpy(&scale, buf + 4, 4);
    CHECK(scale == 0.0f);
    CHECK(decode(buf, 0) == 22.25f && decode(buf, 767) == 22.25f);

    // ---- Extremes map to 0 and 255 ----
    for (int i = 0; i < THERMAL_WIRE_PIXELS; i++) f[i] = 25.0f;
    f[3] = -10.0f;
    f[700] = 120.0f;
    CHECK(thermal_wire_encode(f, buf));
    CHECK(buf[THERMAL_WIRE_HEADER_BYTES + 3] == 0);
    CHECK(buf[THERMAL_WIRE_HEADER_BYTES + 700] == 255);

    // ---- All pixels non-finite: refuse to encode ----
    for (int i = 0; i < THERMAL_WIRE_PIXELS; i++) f[i] = NAN;
    f[5] = INFINITY;
    CHECK(!thermal_wire_encode(f, buf));

    printf("ALL %d CHECKS PASSED\n", tests_run);
    return 0;
}
