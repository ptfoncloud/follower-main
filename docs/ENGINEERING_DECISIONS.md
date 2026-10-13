# Engineering Decisions

Short, honest record of the real tradeoffs behind this design — the
kind of thing worth being able to explain out loud, not just having
work.

## Velocity control, not position control, for the follower logic

The ESP32 sends `SET_POSITION_TARGET_LOCAL_NED` with a velocity-only
type mask, not a position target. A position target would require the
ESP32 to maintain an accurate running estimate of the vehicle's
position relative to the subject — extra state, extra places for drift
to accumulate silently. Velocity control instead just asks "which way
and how fast, right now" every 100ms based on the current camera frame,
recomputed fresh each cycle. Errors don't accumulate because nothing is
integrated locally — the FC's own EKF owns position and velocity state,
which is exactly the estimator that's actually tuned and validated for
that job, so we're not duplicating it worse.

**Tradeoff:** velocity control alone can't specify a precise final
position ("orbit exactly here"). Fine for a follow-me use case, would
need rethinking for a station-keeping mission.

## Dwell + cooldown gating instead of frame-by-frame reaction

Every discrete state transition in the tracking logic (and the planned
gesture classifier) requires a pose or condition to be sustained for a
minimum time, and enforces a minimum gap before the same event can fire
again. The obvious alternative — react instantly to every frame — is
strictly more responsive and strictly worse in practice: a single noisy
frame (motion blur, brief occlusion, a stray color match) becomes a
real command instead of getting filtered out. Trading a few hundred
milliseconds of latency for immunity to single-frame noise is the right
trade for anything that turns into an actual flight command.

## Server-authoritative state, never optimistic UI updates

The dashboard's arm/disarm buttons don't flip their own display the
instant you click — they wait for the FC's own HEARTBEAT to confirm the
state actually changed. The tempting alternative (update the UI
immediately, assume the command worked) makes the interface feel
snappier and is wrong exactly when it matters most: if the command
silently failed, an optimistic UI would show ARMED while the vehicle
sits disarmed, which is a worse failure mode than a half-second of
perceived lag.

## Gyro-only yaw, no compass — a real limitation, not a hidden one

The MicoAir743-AIO has no built-in compass and none is in this build's
BOM. Rather than leave `COMPASS_ENABLE` half-configured with no sensor
behind it, it's explicitly disabled and yaw comes from gyro integration
alone, documented plainly in `follower.param`'s comments. **The actual
cost:** yaw drifts over time with no absolute reference to correct it —
acceptable for short test flights, a real problem for anything long or
requiring precise heading. The fix (an external I2C compass) is a
known, scoped addition, not a redesign — deliberately deferred rather
than solved with a worse workaround now.

## UDP for thermal frames, not TCP

The ESP32 streams raw MLX90640 frames to the Follower Server over UDP,
with no retry or acknowledgment. For a live tracking feed, a frame
that arrives late is worse than a frame that never arrives — TCP's
retry-and-redeliver behavior would mean processing stale thermal data
believing it's current. UDP's drop-and-move-on behavior matches the
actual requirement: latest frame or nothing, never a queued-up stale
one.

## Boresight error computed as real degrees, not raw pixel offset

`compute_boresight_error()` converts a blob's pixel position into an
actual angular offset using the MLX90640's confirmed 55° field of view,
rather than just returning "blob is 6 pixels right of center." A raw
pixel offset is meaningless without also knowing the sensor's FOV and
resolution every time it's consumed downstream — computing the real
angle once, at the source, means every consumer (dashboard display, a
future control loop) gets a physically meaningful number without
re-deriving the conversion each time.

## Where sensor fusion stops — for now

The onboard Pixy2 tracker (primary flight control) and the
Follower Server's thermal tracker (dashboard/monitoring) are two
independent tracking channels that don't currently talk to each other.
Fusing them — using thermal to help re-acquire a lost beacon, say — is
a real, valuable next step, deliberately not built yet. Sensor fusion
done casually is exactly how a "helpful" feature becomes a subtle
safety problem (two disagreeing opinions about where the target is,
silently reconciled by whichever code ran last). That's a design
decision worth making deliberately, not a corner cut for time.
