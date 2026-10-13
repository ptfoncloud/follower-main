# follower-main

![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)
![MAVLink](https://img.shields.io/badge/protocol-MAVLink%202.0-blue)
![ArduPilot](https://img.shields.io/badge/firmware-ArduCopter-green)
![Safety tests](https://img.shields.io/badge/safety__gate-13%2F13%20passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

The flight stack. ESP32 companion firmware that tracks a subject via
Pixy2 and commands a MicoAir743-AIO flight controller running
ArduCopter — plus the parameter set, safety logic, and operations docs
that make that safe to actually fly.

![Architecture](docs/assets/architecture.svg)

Two companion repos complete the system: the ground station dashboard
lives in [gcs-ground-control](https://github.com/ptfoncloud/gcs-ground-control);
the thermal-tracking service that runs on Henri lives in
[follower-server](https://github.com/ptfoncloud/follower-server).

## Hardware

| Component | Part |
|---|---|
| Flight controller | MicoAir743-AIO (STM32H743, integrated 35A 4-in-1 ESC) |
| Companion computer | ESP32 dev board |
| Vision | Pixy2 (CMUcam5), SPI |
| Position/altitude (indoor) | MicoAir MTF-01 optical flow + rangefinder |
| Thermal (secondary tracking) | Adafruit MLX90640, 55° FOV |
| RC receiver | RadioMaster RP1 (ExpressLRS, CRSF) |
| RC transmitter | BetaFPV LiteRadio 3 |
| Firmware | ArduCopter 4.6.x (param file target; confirm in Mission Planner) |

## The one thing worth understanding before anything else

Every velocity command the ESP32 sends passes through a single
function — `evaluate_safety_gate()` in `src/safety_gate.h` — that
refuses to command unless the FC is armed, in GUIDED mode, and its
heartbeat is recent. This isn't a claim: it's compiled and tested
standalone, independent of any ESP32 hardware:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -o test_safety_gate test/native/test_safety_gate.cpp
./test_safety_gate
# ALL 13 CHECKS PASSED

# thermal wire format (esp32_thermal/thermal_wire.h)
g++ -std=c++17 -Wall -Wextra -Werror -o test_thermal_wire test/native/test_thermal_wire.cpp
./test_thermal_wire
# ALL 17 CHECKS PASSED
```

The tests include `src/config.h`, so they check the real
`FC_LINK_TIMEOUT_MS` / `ARDUCOPTER_MODE_GUIDED`, not copies.

`main.cpp` includes and calls this exact function — the tests aren't
exercising a parallel reimplementation that could quietly drift out of
sync with the real flight code.

## Repo layout

```
.
├── src/
│   ├── main.cpp           # ESP32 firmware
│   ├── safety_gate.h      # the gating logic — tested standalone
│   ├── config.h           # tuning constants
│   ├── secrets.example.h  # copy to secrets.h (gitignored) for WiFi creds
│   └── bench/
│       └── pixy_bench.cpp # Pixy2 SPI bench test (env:pixy_bench), no FC
├── test/native/
│   ├── test_safety_gate.cpp
│   └── test_thermal_wire.cpp
├── params/
│   └── follower.param     # full ArduCopter parameter set
├── esp32_thermal/
│   ├── thermal_stream.h   # MLX90640 -> WiFi UDP -> follower-server
│   └── thermal_wire.h     # 776-byte quantized frame format (tested)
├── scripts/
│   ├── launch_mavproxy.sh
│   └── launch_mavproxy.ps1
├── docs/
│   ├── OPERATIONS.md            # preflight checklist, abort procedure
│   ├── MTF01_SETUP.md           # MTF-01 sensor + FC setup, bench check
│   ├── ENGINEERING_DECISIONS.md # why, not just what
│   ├── beacon_training.md       # Pixy2 signature training walkthrough
│   └── assets/architecture.svg
└── .github/workflows/build.yml  # firmware build + safety-gate tests, separately
```

## Setup, in order

1. **Check your ArduCopter version, then read `params/follower.param`**
   and its comments before loading it. The file targets **4.6.x**; 4.7+
   renamed several parameters (the alternatives are in the file as
   comments). `FLOW_POS_X/Y/Z` still need a physical CG measurement.
   Set up the MTF-01 itself first: `docs/MTF01_SETUP.md`.
2. **Bench-test the Pixy2 link** — `pio run -e pixy_bench -t upload`,
   then `pio device monitor -e pixy_bench` (no FC needed). Then **train
   the beacon** — `docs/beacon_training.md`, before anything flies.
3. **Set `PRIMARY_SIG`** in `src/config.h`, then `pio run -e esp32dev
   --target upload`. There's deliberately no `upload_port`: COM3 is the
   flight controller. Add `--upload-port COMx` if auto-detect picks
   the wrong board.
4. **Read `docs/OPERATIONS.md`** — the actual preflight checklist and
   abort procedure, not just code comments.
5. Bench test props-off, then fly.

## Known gaps

- No telemetry radio in the current BOM — real-time link to Henri is
  USB-tethered only right now (`scripts/launch_mavproxy.sh usb`).
  Wireless range needs a telemetry radio or a WiFi relay on the ESP32
  (not built — would compete with Pixy2/thermal streaming for the same
  chip, needs its own bench-tested timing pass first).
- No compass — see `docs/ENGINEERING_DECISIONS.md` for the real
  consequence (yaw drift) and why it's disabled rather than
  half-configured.
- Gesture control via Leap Motion lives in gcs-ground-control (browser
  LeapJS → backend classifier → same command path as the dashboard).
  No gesture maps to DISARM.
- `ENABLE_LINE_FALLBACK` is 0: losing the beacon means LOST (hold
  position). Pixy2's line program tracks floor seams and edges, not
  people, so it is not used as a person-tracking fallback.

## License

MIT — see [LICENSE](LICENSE).
