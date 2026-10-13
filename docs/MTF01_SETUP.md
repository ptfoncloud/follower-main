# MTF-01 Optical Flow + Rangefinder Setup

The MicoAir MTF-01 sits on the FC's **SERIAL4 (UART4)** and talks
MAVLink1. Two halves have to agree: the sensor's own settings (done in
MicoAir's MicoAssistant tool) and the FC parameters (already in
`params/follower.param`).

Primary source: <https://ardupilot.org/copter/docs/common-mtf-01.html>

## 1. Sensor side (MicoAssistant)

Set these on the MTF-01 itself:

| Setting | Value | Why |
|---|---|---|
| Output protocol | `mav_apm` | The ArduPilot flavour of its MAVLink output |
| `mav_id` | `200` | On ArduPilot 4.5 and later the FC may not recognise the sensor while it uses MAVLink system ID 1 (the FC's own). Any value other than 1 works; 200 is what the ArduPilot page suggests. |

### Configuring it without an FTDI adapter (serial passthrough)

ArduPilot's docs describe MicoAssistant over a USB-serial (FTDI)
adapter. You can instead tunnel through the flight controller with
ArduPilot's serial passthrough
(<https://ardupilot.org/copter/docs/common-serial-passthrough.html>),
leaving the sensor wired to SERIAL4:

1. Connect the FC over USB (COM3 on this machine) in Mission Planner.
2. Set:
   - `SERIAL_PASS1,0` (USB)
   - `SERIAL_PASS2,4` (the MTF-01's port)
   - `SERIAL_PASSTIMO,60` (seconds of PC silence before it reverts; 0 = until reboot)
   - `SERIAL4_BAUD` must be the sensor's current baud (115 = 115200). The USB side follows it automatically.
3. **Disconnect** Mission Planner (leave the cable plugged in). If it
   insists on reconnecting, close it.
4. Open MicoAssistant and connect to COM3. It should now be talking to
   the MTF-01 through the FC. Set `mav_apm` and `mav_id 200`, save.
5. Close MicoAssistant. Passthrough ends after `SERIAL_PASSTIMO`
   seconds of silence (`SERIAL_PASS2` reverts to -1), or reboot the FC.

> Passthrough itself is documented ArduPilot behaviour. That
> MicoAssistant in particular works over it has not been verified
> against MicoAir's documentation — if MicoAssistant can't find the
> sensor, look for a "show all COM ports" option, then fall back to an
> FTDI adapter.

## 2. FC side (already in `params/follower.param`)

For **ArduCopter 4.6.x** (what the param file targets):

```
SERIAL4_PROTOCOL,1      # MAVLink1
SERIAL4_BAUD,115
SERIAL4_OPTIONS,1024    # don't forward MAVLink to/from this port
FLOW_TYPE,5             # MAVLink
RNGFND1_TYPE,10         # MAVLink
RNGFND1_ORIENT,25       # down
RNGFND1_MIN_CM,1
RNGFND1_MAX_CM,800
```

For **ArduCopter 4.7+**, two things moved:

- "Don't forward MAVLink" is `MAVx_OPTIONS,2`, where `x` counts
  MAVLink-protocol ports (not SERIAL numbers). After setting
  `SERIAL4_PROTOCOL,1` and rebooting, find which `MAVx` belongs to
  SERIAL4 (most likely `MAV3`: SERIAL0, SERIAL1, SERIAL4).
- Range limits are in metres: `RNGFND1_MIN,0.01`, `RNGFND1_MAX,8`.

Reboot after changing `FLOW_TYPE` / `RNGFND1_TYPE` — the related
parameters only appear afterwards.

## 3. Mounting and orientation

- Underneath the frame, lens pointing straight down.
- ArduPilot's default orientation for this sensor is rotated **180°**
  from the INAV/FMT convention. If you mount it the INAV/FMT way, set
  `FLOW_ORIENT_YAW,18000`. A wrong value inverts flow feedback and can
  cause a flyaway.
- `FLOW_POS_X/Y/Z` are still placeholders (0.0) — measure the lens's
  offset from the centre of gravity (metres; X forward, Y right, Z down)
  once the airframe is built.

## 4. Bench check (props off)

1. Mission Planner → Status tab: `opt_qua` should be non-zero and
   `rangefinder1` should show a sensible distance.
2. Lift the frame slowly: `rangefinder1` should rise and stay within
   1–800 cm.
3. Move the frame forward/right by hand over a textured floor and
   confirm `opt_m_x` / `opt_m_y` respond. Then roll/pitch it in place
   and confirm flow tracks attitude in the right direction (this is the
   `FLOW_ORIENT_YAW` check).
