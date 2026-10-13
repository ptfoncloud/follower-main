# First flight

Wiring, setup, bench tests, go/no-go. Go top to bottom and don't put
props on until everything in 1-6 is checked off. If something fails,
stop and fix it.

> This wiring table replaces the "SOLDERING BLUEPRINT" sheet in the BOM.
> That sheet is from an older version of the design (ESP32 on UART2,
> pixy2 on the FC, flow on I2C, GPS on UART4 which collides with the
> MTF-01). None of that matches the firmware or the param file anymore.

## 1. Wiring

| From | To | Notes |
|---|---|---|
| Battery XT60 | FC VBAT / GND | 4S only |
| Motors | FC M1-M4 | order and direction get checked in step 3 |
| ESP32 GPIO17 (TX2) | FC RX1 (UART1 / SERIAL1) | 57600, MAVLink2 |
| ESP32 GPIO16 (RX2) | FC TX1 | |
| Pixy2 ICSP header | ESP32 VSPI: MISO 19, SCK 18, MOSI 23 | no CS wire, PixyMon interface set to "Arduino ICSP SPI" |
| MTF-01 TX / RX | FC RX4 / TX4 (UART4 / SERIAL4) | MAVLink1, 115200, see `MTF01_SETUP.md` |
| WS-M181 GPS TX / RX | FC RX3 / TX3 (UART3 / SERIAL3) | |
| WS-M181 compass SDA / SCL | FC I2C SDA / SCL | QMC5883, keep it away from the ESC and battery leads |
| RP1 TX / RX | FC RX6 / TX6 (UART6 / SERIAL6) | CRSF, needs both wires for telemetry |
| MLX90640 SDA / SCL | ESP32 GPIO21 / GPIO22 | only if thermal streaming is hooked up |

TX goes to RX. All grounds common.

The FC is 36 x 36 mm with 25.5 mm M3 holes (MicoAir743v2 AIO 35A). The FC tray
is 28 mm wide so the board overhangs it, dry fit it and check the
overhang clears the arms, motor wires and the connectors before
soldering anything.

### Power

| Rail | Feeds |
|---|---|
| Pololu 5V 3A (off VBAT) | ESP32 5V/VIN, Pixy2 5V, MLX90640 |
| FC 5V pad (2 A BEC) | RP1, MTF-01, WS-M181 |

Keeps the ESP32's wifi current spikes off the rail the receiver and
nav sensors are on. Make sure the ESP32 board has a diode on USB 5V
before plugging USB in with the Pololu powered (most DevKitC style
boards do).

- [ ] Before the first battery: multimeter, no short between VBAT and GND
- [ ] First power up through a smoke stopper or current limited supply

## 2. FC setup (props off)

- [ ] Flash ArduCopter 4.7.2 (Install Firmware, the quad icon). The
  param file is written for 4.7.
- [ ] Load `params/follower.param`, write, reboot, load it again (some
  params only show up after a reboot), reboot
- [ ] Check `SERIAL8_PROTOCOL` reads -1 (onboard bluetooth off). It's an
  open MAVLink link anyone nearby could arm through, so it stays off.
- [ ] Accel calibration
- [ ] Compass calibration (won't arm without it)
- [ ] Radio calibration. Flip each LiteRadio switch and write down which
  channel moves and if it's 2 or 3 position.
- [ ] Fill in the RC switch params at the bottom of `follower.param`:
  `FLTMODE_CH` on a 3 position switch (low ALT_HOLD / mid LOITER / high
  GUIDED) and `RC<ch>_OPTION,31` (emergency motor stop) on a 2 position
  switch. Check the HUD shows the right mode in each position.
- [ ] Arming Checks screen: untick GPS Lock only (no fix indoors).
  Leave everything else on, compass included.
- [ ] Battery voltage on the HUD matches a multimeter, adjust
  `BATT_VOLT_MULT` if not

## 3. Motors (props off)

- [ ] Check `FRAME_TYPE` reads 12 after loading the params. The firmware
  default on this target is 1, which puts motors on the wrong arms.
- [ ] Mission Planner motor test, props off. `FRAME_TYPE,12` is
  BetaFlightX, so outputs follow the betaflight layout of the AIO's ESC
  pads (from ArduPilot's AP_MotorsMatrix BF_X table):

  | Output | Position | Spin | Motor test button |
  |---|---|---|---|
  | M1 | rear right | CW | B |
  | M2 | front right | CCW | A |
  | M3 | rear left | CCW | C |
  | M4 | front left | CW | D |

  Mission Planner's motor test goes by letter, clockwise from front right
  (A = front right = M2, not M1). Each button has to spin the motor in
  that spot in that direction. Wrong spot = wrong frame type or pads,
  stop. Wrong direction = swap any two of that motor's wires (or reverse
  it in the Bluejay config).
- [ ] Motors are on DShot600 (`MOT_PWM_TYPE,6`, the Bluejay ESC does
  DShot300/600), so no ESC calibration.
  If the motors don't respond at all in the motor test, check that
  first.
- [ ] E-stop switch kills all motors instantly while armed

## 4. Sensors

- [ ] MTF-01 bench check (`MTF01_SETUP.md` section 4): `opt_qua` > 0,
  rangefinder makes sense, flow direction matches attitude
  (FLOW_ORIENT_YAW)
- [ ] Compass heading matches the real heading in the room you'll fly
  in, and doesn't swing when the motors spin up (props off) or near
  furniture. Bad heading indoors = flow position hold drifts.
- [ ] EKF healthy with no GPS fix: FC accepts LOITER (needs the flow
  based relative position). If it won't, stop here.
- [ ] FLOW_POS_X/Y/Z measured from the real CG (BOM has the lens at
  y = -9 mm from frame center)

## 5. ESP32 + Pixy2 (props off)

- [ ] `pio run -e pixy_bench -t upload` then `pio device monitor -e pixy_bench`:
  `pixy.init() = 0` and blocks print with the beacon in view
- [ ] Train the beacon (`beacon_training.md`), set `PRIMARY_SIG`
- [ ] `pio run -t upload` for the flight firmware (make sure it's not
  going to COM3, that's the FC)
- [ ] Monitor: disarmed gives one `[GATE] Suppressed: FC not armed` per
  second. Armed in ALT_HOLD gives `FC not in required mode`. GUIDED
  gives `[GATE] Open`.
- [ ] MAVLink Inspector: `SET_POSITION_TARGET_LOCAL_NED` from compid
  158, type_mask 3527, frame 8. Cover the beacon: zero velocity from the
  first missed frame, LOST after about 1 s.

## 6. Failsafes (props off, armed on the bench)

- [ ] Transmitter off: radio failsafe, mode goes to LAND, then disarms
- [ ] Unplug the ESP32 TX wire in GUIDED: commands stop and GUIDED
  stops the vehicle within 3 s
- [ ] Switch GUIDED to LOITER: ESP32 commands stop mattering right away

## 7. First flights (props on, guards on, nobody in reach)

1. Hover only. Arm on the sticks in ALT_HOLD, go up to about 1 m, hold
   30 s, land. Look for vibration, oscillation, yaw drift.
2. LOITER. Same thing then switch to LOITER. It should hold over a
   textured floor without wandering or circling. Circling usually means
   compass or flow orientation is off, land and fix it.
3. Slow follow. Think about halving `VEL_MAX_XY` / `VEL_MAX_Z` in
   `src/config.h` first. In LOITER at about 1 m, show the beacon,
   switch to GUIDED. To abort switch back to LOITER. Then LAND.

Abort order: LOITER, then LAND, then disarm on the ground. E-stop only
if it's out of control near people (it'll fall). No RTL on this build,
no home indoors and it would climb to 15 m. See `OPERATIONS.md`.

## Go / no-go

Only fly when all of these are true:

- [ ] Everything in sections 1-6 checked off
- [ ] LOITER holds position (7.2) before GUIDED is ever tried
- [ ] You can hit the mode switch and e-stop without looking
- [ ] Battery above `BATT_ARM_VOLT` (15.2 V) and balance checked
