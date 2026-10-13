#!/usr/bin/env bash
# ============================================================
# follower - MAVProxy bridge (Linux / Henri)
#
# Takes one MAVLink source (FC over USB or SITL) and sends it out to
# multiple UDP ports at once, the FastAPI backend and a second GCS
# (Mission Planner/QGC) for watching.
#
# usb mode means the drone is plugged into this machine with a cable
# (bench testing or a really short hop). For actual wireless range you'd
# need a telemetry radio pair (SiK, RFD900), not in the BOM yet. Without
# one, flights are controlled over ELRS but no telemetry comes back here
# unless a wifi bridge gets built on the ESP32 (see the bottom of this
# file).
#
# Usage:
#   ./launch_mavproxy.sh sitl     bridge SITL for backend testing
#   ./launch_mavproxy.sh usb      bridge the real FC over USB
# ============================================================

set -e

MODE="${1:-sitl}"

GCS_BACKEND_PORT=14550    # matches the FastAPI backend's udpin:127.0.0.1:14550
MONITOR_PORT=14551        # point Mission Planner/QGC here as a UDP client to
                           # watch during test flights

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
    # change to the FC's actual USB serial device
    # find it with: ls /dev/serial/by-id/
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

# ---- not built yet ----
# For wireless telemetry without a radio, the ESP32 (already on UART1
# talking MAVLink to the FC) could also relay everything it sees on UART1
# over wifi to this machine. Doable, but it competes with the pixy2
# tracking loop for the ESP32's CPU and radio so it needs its own testing
# before it flies.
