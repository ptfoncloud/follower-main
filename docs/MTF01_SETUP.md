# MTF-01 setup

The MTF-01 is on SERIAL4 (UART4) and talks MAVLink1. Two things have to
match: the sensor's own settings (MicoAir's MicoAssistant tool) and the
FC params (already in `params/follower.param`).

Source: <https://ardupilot.org/copter/docs/common-mtf-01.html>

## 1. Sensor side (MicoAssistant)

| Setting | Value | Why |
|---|---|---|
| Output protocol | `mav_apm` | ArduPilot version of its MAVLink output |
| `mav_id` | `200` | On ArduPilot 4.5+ the FC might ignore the sensor if it uses sysid 1 (same as the FC). Anything but 1 works, 200 is what the ArduPilot page uses. |

### Without an FTDI adapter (serial passthrough)

The ArduPilot page uses an FTDI adapter for MicoAssistant. You can go
through the FC instead with serial passthrough
(<https://ardupilot.org/copter/docs/common-serial-passthrough.html>) and
leave the sensor wired to SERIAL4:

1. Connect to the FC over USB in Mission Planner (COM3 here).
2. Set:
   - `SERIAL_PASS1,0` (USB)
   - `SERIAL_PASS2,4` (MTF-01 port)
   - `SERIAL_PASSTIMO,60` (seconds of no traffic from the PC before it
     turns off, 0 = until reboot)
   - `SERIAL4_BAUD` has to be the sensor's current baud (115 = 115200),
     the USB side matches it automatically
3. Disconnect Mission Planner but leave the cable in. If it keeps
   reconnecting just close it.
4. Open MicoAssistant on COM3. It should be talking to the MTF-01
   through the FC now. Set `mav_apm` and `mav_id 200`, save.
5. Close MicoAssistant. Passthrough turns off after `SERIAL_PASSTIMO`
   seconds of silence (`SERIAL_PASS2` goes back to -1) or just reboot
   the FC.

> Passthrough is documented ArduPilot stuff, but I haven't found
> anything from MicoAir saying MicoAssistant works over it. If it can't
> find the sensor look for a "show all COM ports" option, otherwise use
> an FTDI adapter.

## 2. FC side (already in `params/follower.param`)

For ArduCopter 4.7.2:

```
SERIAL4_PROTOCOL,1      # MAVLink1
SERIAL4_BAUD,115
MAV3_OPTIONS,2          # don't forward MAVLink to/from SERIAL4
FLOW_TYPE,5             # MAVLink
RNGFND1_TYPE,10         # MAVLink
RNGFND1_ORIENT,25       # down
RNGFND1_MIN,0.01        # metres
RNGFND1_MAX,8
```

It's MAV3 because the number counts MAVLink serial ports from SERIAL0
up: SERIAL0 (USB), SERIAL1 (ESP32), SERIAL4 (MTF-01). If SERIAL2 or
SERIAL3 ever get switched to MAVLink the number shifts.

On 4.6 and older it was `SERIAL4_OPTIONS,1024` and `RNGFND1_MIN_CM,1` /
`RNGFND1_MAX_CM,800` instead.

Reboot after changing `FLOW_TYPE` / `RNGFND1_TYPE`, the related params
only show up after.

## 3. Mounting

- Under the frame, lens pointing straight down.
- ArduPilot's default orientation for this sensor is 180° off from the
  INAV/FMT one. If it's mounted the INAV/FMT way set
  `FLOW_ORIENT_YAW,18000`. Getting this wrong flips the flow feedback
  and can cause a flyaway.
- `FLOW_POS_X/Y/Z` are still 0.0 placeholders. Measure the lens offset
  from the CG once the airframe is built (metres, X forward, Y right,
  Z down).

## 4. Bench check (props off)

1. Mission Planner Status tab: `opt_qua` non zero and `rangefinder1`
   showing a sane distance.
2. Lift the frame slowly, `rangefinder1` should go up and stay in
   1-800 cm.
3. Push the frame forward/right by hand over a textured floor and check
   `opt_m_x` / `opt_m_y` respond. Then roll/pitch it in place and check
   the flow follows attitude the right way (that's the
   `FLOW_ORIENT_YAW` check).
