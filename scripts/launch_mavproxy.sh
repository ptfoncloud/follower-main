#!/usr/bin/env bash
# ============================================================
# FOLLOWER — MAVProxy bridge launcher (Linux / Henri)
#
# WHAT THIS ACTUALLY DOES:
# Takes ONE MAVLink source (either real FC over USB, or SITL) and
# fans it out to multiple UDP outputs simultaneously — your FastAPI
# backend AND a second GCS (Mission Planner/QGC) for monitoring, at
# the same time, from one connection.
#
# HONEST LIMITATION: the "FC over USB" mode below assumes the drone
# is physically tethered to this machine via USB cable (bench testing,
# or a very short-range test hop). For real wireless range outdoors,
# you need a telemetry radio pair (e.g. SiK radio, RFD900) — that's
# not currently in your BOM. Without one, real outdoor flights rely on
# the ELRS link for control (via the RX/RC path) but won't stream
# telemetry back to this laptop/Henri unless you build a WiFi bridge
# on the ESP32 (a real option later, not built yet — see notes at the
# bottom of this file).
#
# Usage:
#   ./launch_mavproxy.sh sitl     — bridge SITL for backend testing
#   ./launch_mavproxy.sh usb      — bridge real FC over USB (bench/tethered)
# ============================================================

set -e

MODE="${1:-sitl}"

GCS_BACKEND_PORT=14550    # matches the FastAPI backend's udpin:127.0.0.1:14550
MONITOR_PORT=14551        # point Mission Planner/QGC here as a UDP client, for
                           # a second pair of eyes during test flights

if [ "$MODE" == "sitl" ]; then
    echo "[launch_mavproxy] Bridging SITL -> backend:$GCS_BACKEND_PORT + monitor:$MONITOR_PORT"
    echo "[launch_mavproxy] Make sure sim_vehicle.py is already running separately:"
    echo "                  sim_vehicle.py -v ArduCopter --out=udp:127.0.0.1:14560"
    mavproxy.py \
        --master=udp:127.0.0.1:14560 \
        --out=udp:127.0.0.1:${GCS_BACKEND_PORT} \
        --out=udp:127.0.0.1:${MONITOR_PORT} \
        --daemon

elif [ "$MODE" == "usb" ]; then
    # CHANGE THIS to your actual FC's USB serial device.
    # Find it with: ls /dev/serial/by-id/   (Linux)
    FC_PORT="/dev/ttyACM0"
    FC_BAUD=115200

    echo "[launch_mavproxy] Bridging real FC ($FC_PORT @ $FC_BAUD) -> backend:$GCS_BACKEND_PORT + monitor:$MONITOR_PORT"
    mavproxy.py \
        --master=${FC_PORT},${FC_BAUD} \
        --out=udp:127.0.0.1:${GCS_BACKEND_PORT} \
        --out=udp:127.0.0.1:${MONITOR_PORT} \
        --daemon

else
    echo "Usage: $0 [sitl|usb]"
    exit 1
fi

echo "[launch_mavproxy] Backend should now be receiving telemetry on :${GCS_BACKEND_PORT}"
echo "[launch_mavproxy] Point Mission Planner/QGC at UDP :${MONITOR_PORT} to watch in parallel"

# ---- Future option, not built yet ----
# If you want true wireless outdoor telemetry without a dedicated radio,
# the ESP32 (already on the airframe, already talking MAVLink to the FC
# on UART1) could ALSO run a WiFi UDP relay — forwarding everything it
# sees on UART1 out over WiFi to this machine. That's a real, buildable
# addition to the ESP32 firmware, but it competes for the ESP32's CPU/
# radio time with the Pixy2 tracking loop, so it needs its own testing
# pass before trusting it in flight. Ask if you want this built.
