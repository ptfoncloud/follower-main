# Engineering decisions

Why things are built the way they are, and what each choice costs.

## Velocity commands, not position targets

The ESP32 sends `SET_POSITION_TARGET_LOCAL_NED` with a velocity only
type mask (0x0DC7). A position target would mean the ESP32 has to keep
its own estimate of where the drone is relative to the person, which is
more state and more places for drift to build up. With velocity it just
says which way and how fast based on the current frame, every 100 ms,
and nothing gets integrated on the ESP32. The FC's EKF owns position
and velocity, it's the estimator that's actually tuned for that.

Cost: you can't ask for an exact final position ("hover right here").
Fine for following, would need rethinking for station keeping.

## Hold on a missed frame

If a frame comes back with no beacon the ESP32 sends zero velocity
immediately instead of sending nothing. Sending nothing would leave
the FC flying the last command (up to 1.5 m/s) for about a second until
LOST kicks in. The downside is some stop/start if the beacon flickers
at the edge of detection, the fix for that is a better trained
signature not removing the hold.

## Dwell + cooldown instead of reacting to every frame

Discrete events (the tracker state changes and the gesture classifier
in the GCS) need a pose or condition held for a minimum time, then a
cooldown before anything else can fire. Reacting to every frame is
faster but one bad frame (motion blur, occlusion, a random color
match) turns into a real command. A few hundred ms of latency is worth
it for anything that ends up as a flight command.

## The UI waits for the FC

The dashboard's arm/disarm buttons don't flip the display when you
click, they wait for the FC's heartbeat to show the change. Updating
right away feels snappier but if the command silently failed it would
show ARMED while the drone sits disarmed, which is way worse than half
a second of lag.

## Compass yaw from the GPS module, no GPS nav indoors

The MicoAir743-AIO has no compass. The Walksnail WS-M181 has a QMC5883
on I2C, so that's the EKF yaw source (`EK3_SRC1_YAW,1`), same as
ArduPilot's optical flow setup. Before that it was gyro only yaw, which
drifts. Position and velocity still come from the MTF-01
(`EK3_SRC1_POSXY,0`, `VELXY,5`). The GPS is wired to SERIAL3 but has no
fix indoors and isn't used by the EKF.

Cost: steel, rebar and wiring indoors can bend the heading, and a bad
heading makes flow position hold drift. Check it in the actual room
(`FIRST_FLIGHT.md` section 4). Outdoor GPS nav would need a second EKF
source set, not set up.

## UDP for thermal frames

Frames go ESP32 to follower-server over UDP with no retries. For a live
feed a late frame is worse than a missing one, TCP would retry and you'd
end up processing old data thinking it's current.

That only works if a frame fits in one datagram. Raw float32 frames are
3072 bytes and arduino-esp32's WiFiUDP sends a packet every time its
1460 byte buffer fills, so they came out as 3 packets and got rejected.
Frames are now quantized to 8 bits with a min/scale header, 776 bytes,
worst case error about 0.03 °C. Format is in `esp32_thermal/README.md`.

## Boresight error in degrees

`compute_boresight_error()` turns the blob position into an actual angle
using the MLX90640's 55° FOV instead of returning "6 pixels right of
center". Pixels mean nothing downstream without the FOV and resolution,
so it gets converted once at the source.

## No sensor fusion (yet)

The pixy2 tracker (flies the drone) and the thermal tracker (dashboard)
don't talk to each other. Using thermal to reacquire a lost beacon would
be useful but it's not built on purpose. Two trackers disagreeing about
where the target is, reconciled by whichever ran last, is how a
"helpful" feature turns into a safety problem. That needs to be designed
properly, not bolted on.
