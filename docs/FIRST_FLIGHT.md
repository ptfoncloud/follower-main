# First Flight: Wiring, Setup, Go/No-Go

Work top to bottom. Every box must be ticked before props go on. If a
step fails, stop and fix it; don't skip ahead.

> **This table supersedes the "SOLDERING BLUEPRINT" sheet in the BOM
> spreadsheet.** That sheet describes an older design: it puts the
> ESP32 on UART2, the Pixy2 on the FC, the optical flow on I2C, and the
> GPS on UART4 (which collides with the MTF-01). None of that matches the
> firmware or `params/follower.param`.

## 1. Wiring

| From | To | Notes |
|---|---|---|
| Battery XT60 | FC VBAT / GND pads | 4S only |
| Motors | FC M1–M4 pads | order/direction checked in step 3 |
| ESP32 GPIO17 (TX2) | FC **RX1** (UART1 / SERIAL1) | 57600 MAVLink2 |
| ESP32 GPIO16 (RX2) | FC **TX1** | |
| Pixy2 ICSP header | ESP32 VSPI: MISO 19, SCK 18, MOSI 23 | **no CS wire**; PixyMon interface = "Arduino ICSP SPI" |
| MTF-01 TX / RX | FC **RX4 / TX4** (UART4 / SERIAL4) | MAVLink1, 115200; setup in `MTF01_SETUP.md` |
| WS-M181 GPS TX / RX | FC **RX3 / TX3** (UART3 / SERIAL3) | |
| WS-M181 compass SDA / SCL | FC I2C SDA / SCL | QMC5883; mount away from ESC and battery leads |
| RP1 TX / RX | FC **RX6 / TX6** (UART6 / SERIAL5) | CRSF; both wires needed for telemetry |
| MLX90640 SDA / SCL | ESP32 GPIO21 / GPIO22 | only if thermal streaming is integrated |

TX always goes to RX. All grounds are common.

### Power

| Rail | Feeds |
|---|---|
| **Pololu 5V 3A** (from VBAT) | ESP32 5V/VIN, Pixy2 5V, MLX90640 |
| FC 5V pad (2 A BEC) | RP1, MTF-01, WS-M181 |

The ESP32's WiFi current spikes stay off the FC rail that feeds the
receiver and nav sensors. Check that your ESP32 dev board has a diode on
USB 5V before plugging USB in while the Pololu is powered (most
DevKitC-style boards do).

- [ ] Before the first battery: multimeter shows **no short** between VBAT and GND.
- [ ] First power-up through a smoke stopper or current-limited supply.

## 2. Flight controller setup (props OFF)

- [ ] Note the ArduCopter version (Mission Planner title bar / Messages).
  The param file targets **4.6.x**. On 4.7+ apply the commented 4.7 lines.
- [ ] Load `params/follower.param`, write, reboot, load again (some
  parameters only appear after a reboot), reboot.
- [ ] Accelerometer calibration.
- [ ] **Compass calibration** (the FC won't arm without it).
- [ ] Radio calibration. Flip each LiteRadio switch and write down which
  channel moves and whether it's 2- or 3-position.
- [ ] Set the RC switch parameters at the bottom of `follower.param`:
  `FLTMODE_CH` on a 3-position switch (low ALT_HOLD / mid LOITER /
  high GUIDED) and `RC<ch>_OPTION,31` (emergency motor stop) on a
  2-position switch. Confirm on the HUD that each switch position shows
  the right mode.
- [ ] Arming Checks screen: untick **GPS Lock only** (indoors there's no
  fix). Leave everything else, including Compass, ticked.
- [ ] Battery voltage on the HUD matches a multimeter; adjust `BATT_VOLT_MULT`.

## 3. Motors (props OFF)

- [ ] Mission Planner Motor Test: each motor spins in the right position
  and direction for Quad X. Swap two motor wires to reverse one.
- [ ] Emergency stop switch stops all motors instantly while armed.

## 4. Sensors

- [ ] MTF-01 bench check (`MTF01_SETUP.md` §4): `opt_qua` > 0,
  rangefinder sensible, flow direction matches attitude (FLOW_ORIENT_YAW).
- [ ] Compass heading on the HUD matches real heading **in the flying
  room**, and doesn't swing when you power the motors (props off) or move
  near furniture. A bad indoor heading makes flow position control drift.
- [ ] EKF healthy with no GPS fix: in LOITER the FC accepts the mode
  (needs the flow-based relative position). If it refuses, stop here.
- [ ] `FLOW_POS_X/Y/Z` measured from the real CG (BOM says the lens is at
  y = −9 mm from frame centre).

## 5. ESP32 + Pixy2 (props OFF)

- [ ] `pio run -e pixy_bench -t upload`, then `pio device monitor -e pixy_bench`:
  `pixy.init() = 0`, blocks print with the beacon in view.
- [ ] Train the beacon (`beacon_training.md`); set `PRIMARY_SIG`.
- [ ] `pio run -t upload` (flight firmware; check the port isn't COM3, the FC).
- [ ] Monitor: disarmed → one `[GATE] Suppressed: FC not armed` per second.
  Armed in ALT_HOLD → `FC not in required mode`. GUIDED → `[GATE] Open`.
- [ ] MAVLink Inspector: `SET_POSITION_TARGET_LOCAL_NED` from compid 158,
  `type_mask 3527`, frame 8. Cover the beacon → zero velocity from the
  first missed frame, `LOST` after ~1 s.

## 6. Failsafes (props OFF, armed on the bench)

- [ ] Transmitter off → radio failsafe → mode **LAND**, then disarm.
- [ ] Unplug ESP32 TX in GUIDED → commands stop; GUIDED stops the vehicle
  within 3 s.
- [ ] Switch GUIDED → LOITER: ESP32 commands stop having effect immediately.

## 7. First flights (props ON, guards on, nobody within reach)

1. **Hover only.** Arm on the sticks in ALT_HOLD, climb to ~1 m, hold
   30 s, land. Check for vibration, oscillation, yaw drift.
2. **LOITER.** Same, then switch to LOITER: it should hold position
   over a textured floor without wandering or circling (circling =
   compass/flow orientation problem → land and fix).
3. **Follow, slow.** Consider halving `VEL_MAX_XY` / `VEL_MAX_Z` in
   `src/config.h` (the file's own advice) and reflashing. In LOITER at
   ~1 m, show the beacon, switch to GUIDED. Abort = switch back to
   LOITER. Then LAND.

**Abort, in order:** switch to LOITER → LAND → disarm on the ground.
Emergency stop only if it's out of control near people (it falls).
**No RTL** on this build (no home indoors; it would also climb to 15 m).
See `OPERATIONS.md`.

## Go / no-go

Fly only when **all** of these are true:

- [ ] Every box in sections 1–6 is ticked.
- [ ] LOITER holds position (section 7.2) before GUIDED is ever tried.
- [ ] You can reach the mode switch and emergency stop without looking.
- [ ] Battery above `BATT_ARM_VOLT` (15.2 V) and balance-checked.
