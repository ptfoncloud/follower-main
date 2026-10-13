# follower-main

![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)
![MAVLink](https://img.shields.io/badge/protocol-MAVLink%202.0-blue)
![ArduPilot](https://img.shields.io/badge/firmware-ArduCopter-green)
![Safety tests](https://img.shields.io/badge/safety__gate-13%2F13%20passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

Flight side of the follower drone. ESP32 companion firmware that tracks
a beacon with a Pixy2 and sends velocity commands to a MicoAir743v2 AIO
running ArduCopter, plus the param file and the docs for setting it up
and flying it.

![Architecture](docs/assets/architecture.svg)

The other two repos: ground station in
[gcs-ground-control](https://github.com/ptfoncloud/gcs-ground-control),
thermal tracking service (runs on Henri) in
[follower-server](https://github.com/ptfoncloud/follower-server).

## Hardware

| Component | Part |
|---|---|
| Flight controller | MicoAir743v2 AIO 35A (STM32H743, built-in Bluejay 35A 4-in-1 ESC, firmware target MicoAir743v2) |
| Companion | ESP32 dev board |
| Vision | Pixy2 (CMUcam5) over SPI |
| Flow + rangefinder | MicoAir MTF-01 |
| Thermal | Adafruit MLX90640, 55° |
| GPS + compass | Walksnail WS-M181 (GPS on SERIAL3, QMC5883 compass on I2C, compass is the yaw source) |
| RC | RadioMaster RP1 (ELRS/CRSF) + BetaFPV LiteRadio 3 |
| Companion power | Pololu 5V 3A for the ESP32 + Pixy2, not the FC's 5V |
| Firmware | ArduCopter 4.7.2 |

## Safety gate

Every velocity command goes through `evaluate_safety_gate()` in
`src/safety_gate.h`. It only lets a command through if the FC is armed,
in GUIDED, and its last heartbeat is under 3 s old. main.cpp calls that
same function, and it's tested on desktop with no ESP32 needed:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -o test_safety_gate test/native/test_safety_gate.cpp
./test_safety_gate
# ALL 13 CHECKS PASSED

# thermal frame format (esp32_thermal/thermal_wire.h)
g++ -std=c++17 -Wall -Wextra -Werror -o test_thermal_wire test/native/test_thermal_wire.cpp
./test_thermal_wire
# ALL 15 CHECKS PASSED  (17 with --golden <file>)
```

The tests pull `FC_LINK_TIMEOUT_MS` and `ARDUCOPTER_MODE_GUIDED` from
`src/config.h` so they test the real values.

## Layout

```
.
├── src/
│   ├── main.cpp           # firmware
│   ├── safety_gate.h      # gating logic, tested standalone
│   ├── config.h           # all the tunables
│   ├── secrets.example.h  # copy to secrets.h (gitignored) for wifi creds
│   └── bench/
│       └── pixy_bench.cpp # pixy2 spi bench test, no FC (env:pixy_bench)
├── test/native/
│   ├── test_safety_gate.cpp
│   └── test_thermal_wire.cpp
├── params/
│   └── follower.param     # ArduCopter params
├── esp32_thermal/
│   ├── thermal_stream.h   # MLX90640 -> wifi udp -> follower-server
│   └── thermal_wire.h     # 776 byte frame format
├── scripts/
│   ├── launch_mavproxy.sh
│   └── launch_mavproxy.ps1
├── docs/
│   ├── FIRST_FLIGHT.md          # wiring, setup, bench tests, go/no-go
│   ├── OPERATIONS.md            # preflight, abort procedure
│   ├── MTF01_SETUP.md           # MTF-01 setup
│   ├── ENGINEERING_DECISIONS.md # why things are the way they are
│   ├── beacon_training.md       # training the pixy2 signature
│   └── assets/architecture.svg
└── .github/workflows/build.yml  # firmware builds + native tests
```

## Setup

1. Flash ArduCopter 4.7.2 and read `params/follower.param` before
   loading it. It's written for 4.7, the 4.6 names are in there as
   comments. FLOW_POS_X/Y/Z still need a real CG measurement. Set up
   the MTF-01 first (`docs/MTF01_SETUP.md`).
2. Bench test the pixy2: `pio run -e pixy_bench -t upload` then
   `pio device monitor -e pixy_bench`. No FC needed. Then train the
   beacon (`docs/beacon_training.md`).
3. Set `PRIMARY_SIG` in `src/config.h` and `pio run -t upload`. There's
   no `upload_port` on purpose, COM3 is the flight controller. Use
   `--upload-port COMx` if it picks the wrong board.
4. Go through `docs/FIRST_FLIGHT.md` top to bottom (the wiring table in
   there replaces the soldering sheet in the BOM), then read
   `docs/OPERATIONS.md`.

## Known gaps

- No telemetry radio yet, the link to Henri is USB only
  (`scripts/launch_mavproxy.sh usb`). A wifi relay on the ESP32 would
  compete with the pixy2/thermal work on the same chip so it needs its
  own timing test first.
- Heading comes from the WS-M181 compass and indoor steel/wiring can
  throw it off. Check it in the room you fly in. GPS isn't used for
  navigation indoors and there's no RTL on this build.
- Gesture control (Leap Motion) lives in gcs-ground-control. No gesture
  can arm or disarm, that's transmitter or console button only.
- `ENABLE_LINE_FALLBACK` is 0. Losing the beacon means hold position.
  Pixy2's line mode tracks floor seams and edges, not people.

## License

MIT, see [LICENSE](LICENSE).
