/*  follower - ESP32 companion firmware
    ESP32 + Pixy2 (SPI) + MicoAir H743 AIO (UART1, MAVLink2)

    The armed/mode/link timeout check is in safety_gate.h so it can be
    tested on desktop (test/native/). This file calls that same
    function instead of having its own copy.

    Tracking:
      PRIMARY   pixy2 color blob (beacon)
      FALLBACK  pixy2 "line" program, off by default (ENABLE_LINE_FALLBACK
                in config.h) since it tracks lines not people. When off,
                PRIMARY goes straight to LOST.
      LOST      hold position */

#include <Arduino.h>
#include <Pixy2.h>
#include <MAVLink.h>
#include "config.h"
#include "safety_gate.h"

Pixy2 pixy;

enum TrackMode { MODE_PRIMARY, MODE_FALLBACK, MODE_LOST };
TrackMode trackMode = MODE_LOST;

uint32_t lastDetectMs     = 0;
int      primaryMissCount = 0;

bool     fcArmed          = false;
uint32_t fcCustomMode     = 0;   // HEARTBEAT.custom_mode is uint32
uint32_t lastFcHeartbeatMs = 0;

mavlink_status_t mavStatus;
mavlink_message_t mavMsg;

// -- outgoing --------------------------------------------------------
void sendBytes(uint8_t* buf, uint16_t len) { FC_SERIAL.write(buf, len); }

void sendHeartbeat() {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_heartbeat_pack(MY_SYSID, MY_COMPID, &msg,
        MAV_TYPE_GENERIC, MAV_AUTOPILOT_INVALID,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0, MAV_STATE_ACTIVE);
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    sendBytes(buf, len);
}

// Every velocity command goes through evaluate_safety_gate() first (same
// function test_safety_gate.cpp tests).
void sendVelocity(float vx, float vy, float vz) {
    SafetyGateResult gate = evaluate_safety_gate(
        fcArmed, fcCustomMode, ARDUCOPTER_MODE_GUIDED,
        lastFcHeartbeatMs, millis(), FC_LINK_TIMEOUT_MS
    );
    // Only log when the reason changes, or once per GATE_LOG_INTERVAL_MS.
    // This runs every 100 ms so logging every call drowned out everything
    // else while disarmed.
    static const char* lastGateReason = nullptr;   // nullptr = gate was open
    static uint32_t    lastGateLogMs  = 0;
    if (!gate.should_command) {
        uint32_t now = millis();
        bool changed = !lastGateReason || strcmp(lastGateReason, gate.reason) != 0;
        if (changed || now - lastGateLogMs >= GATE_LOG_INTERVAL_MS) {
            Serial.printf("[GATE] Suppressed: %s\n", gate.reason);
            lastGateLogMs = now;
        }
        lastGateReason = gate.reason;
        return;
    }
    if (lastGateReason) {
        Serial.println("[GATE] Open: commanding velocity");
        lastGateReason = nullptr;
    }

    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    vx = constrain(vx, -VEL_MAX_XY, VEL_MAX_XY);
    vy = constrain(vy, -VEL_MAX_XY, VEL_MAX_XY);
    vz = constrain(vz, -VEL_MAX_Z,  VEL_MAX_Z);

    mavlink_msg_set_position_target_local_ned_pack(
        MY_SYSID, MY_COMPID, &msg,
        millis(), FC_SYSID, FC_COMPID,
        // BODY_NED: ArduCopter rotates the velocity by current heading
        // (handle_message_set_position_target_local_ned)
        MAV_FRAME_BODY_NED,
        // 0x0DC7 (3527) = ArduPilot's documented velocity only mask (ignore
        // pos, accel, yaw, yaw rate). Was 0x0FC7 which also set bit 9
        // FORCE_SET, only worked because accel was ignored too.
        0x0DC7,
        0, 0, 0,
        vx, vy, vz,
        0, 0, 0,
        0, 0
    );
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    sendBytes(buf, len);
}

void sendHold() { sendVelocity(0, 0, 0); }

float pxToVel(int err, int deadZone, float kP) {
    if (abs(err) < deadZone) return 0.0f;
    int sign = (err > 0) ? 1 : -1;
    return kP * (float)(abs(err) - deadZone) * sign;
}

// -- incoming: FC heartbeat for armed/mode ---------------------------
void pollFcHeartbeat() {
    while (FC_SERIAL.available()) {
        uint8_t c = FC_SERIAL.read();
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &mavMsg, &mavStatus)) {
            if (mavMsg.msgid == MAVLINK_MSG_ID_HEARTBEAT &&
                mavMsg.sysid == FC_SYSID && mavMsg.compid == FC_COMPID) {

                mavlink_heartbeat_t hb;
                mavlink_msg_heartbeat_decode(&mavMsg, &hb);

                bool wasArmed = fcArmed;
                fcArmed       = (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
                fcCustomMode  = hb.custom_mode;
                lastFcHeartbeatMs = millis();

                if (wasArmed != fcArmed) {
                    Serial.printf("[FC] Armed state changed: %s\n", fcArmed ? "ARMED" : "DISARMED");
                }
            }
        }
    }
}

// -- primary tracking: color blob ------------------------------------
bool runPrimary(float &vx, float &vy, float &vz) {
    int n = pixy.ccc.getBlocks();
    if (n <= 0) return false;

    Block* best = nullptr;
    for (int i = 0; i < n; i++) {
        if (pixy.ccc.blocks[i].m_signature == PRIMARY_SIG) {
            if (!best || pixy.ccc.blocks[i].m_width * pixy.ccc.blocks[i].m_height
                       > best->m_width * best->m_height) {
                best = &pixy.ccc.blocks[i];
            }
        }
    }
    if (!best) return false;

    int cx = best->m_x, cy = best->m_y;
    int area = best->m_width * best->m_height;
    int errX = cx - FRAME_W / 2;
    int errY = cy - FRAME_H / 2;

    float velFwd = 0;
    if (area < TARGET_AREA_FAR)       velFwd =  0.8f;
    else if (area > TARGET_AREA_NEAR) velFwd = -0.6f;

    vx = velFwd;
    vy = pxToVel(errX, DEAD_X, 0.004f);
    vz = pxToVel(errY, DEAD_Y, 0.002f);
    return true;
}

// -- fallback tracking: line (off by default) ------------------------
bool runFallback(float &vx, float &vy, float &vz) {
    pixy.line.getMainFeatures();
    if (!(pixy.line.numVectors)) return false;

    Vector* v = &pixy.line.vectors[0];
    int cx = ((int)v->m_x0 + (int)v->m_x1) / 2;
    int cy = ((int)v->m_y0 + (int)v->m_y1) / 2;
    int errX = cx - FRAME_W / 2;
    int errY = cy - FRAME_H / 2;

    vx = 0.4f;
    vy = pxToVel(errX, DEAD_X + 10, 0.003f);
    vz = pxToVel(errY, DEAD_Y + 10, 0.001f);
    return true;
}

// --------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    FC_SERIAL.begin(FC_BAUD, SERIAL_8N1, 16, 17);
    pinMode(STATUS_LED, OUTPUT);

    Serial.println("[FOLLOWER] Initialising Pixy2...");
    int8_t pixyRes = pixy.init();   // waits up to ~5 s for the pixy to answer
    if (pixyRes < 0) {
        // not fatal, with no blocks it just stays LOST (hold)
        Serial.printf("[FOLLOWER] ERROR: Pixy2 init failed (%d, -4 = timeout). "
                      "Check SPI wiring (MISO 19, SCK 18, MOSI 23, no CS) and that "
                      "PixyMon's interface is \"Arduino ICSP SPI\". Tracking will stay LOST.\n",
                      pixyRes);
    } else {
        pixyRes = pixy.changeProg("ccc");
        if (pixyRes < 0) {
            Serial.printf("[FOLLOWER] ERROR: Pixy2 changeProg(\"ccc\") failed (%d)\n", pixyRes);
        } else {
            Serial.println("[FOLLOWER] Pixy2 ready.");
        }
    }
    Serial.printf("[FOLLOWER] Primary sig: %d  Lost timeout: %d ms\n", PRIMARY_SIG, LOST_TIMEOUT_MS);
    Serial.println("[FOLLOWER] Waiting for FC heartbeat before commanding...");
}

// --------------------------------------------------------------------
void loop() {
    static uint32_t lastHeartbeatMs = 0;
    static uint32_t lastCmdMs       = 0;
    uint32_t now = millis();

    pollFcHeartbeat();

    if (now - lastHeartbeatMs >= 1000) {
        lastHeartbeatMs = now;
        sendHeartbeat();
    }

    if (now - lastCmdMs < 100) return;
    lastCmdMs = now;

    if (lastFcHeartbeatMs == 0) {
        digitalWrite(STATUS_LED, (now / 500) % 2);
        return;
    }
    if (now - lastFcHeartbeatMs > FC_LINK_TIMEOUT_MS) {
        // nothing gets sent while the link is stale, the FC's guided
        // timeout stops it. log at most once per interval
        static uint32_t lastLinkLogMs = 0;
        if (lastLinkLogMs == 0 || now - lastLinkLogMs >= GATE_LOG_INTERVAL_MS) {
            Serial.println("[FOLLOWER] FC link timeout - not commanding");
            lastLinkLogMs = now;
        }
        digitalWrite(STATUS_LED, (now / 100) % 2);
        return;
    }

    float vx = 0, vy = 0, vz = 0;
    bool detected = false;

    switch (trackMode) {
        case MODE_PRIMARY:
            detected = runPrimary(vx, vy, vz);
            if (detected) {
                primaryMissCount = 0;
                lastDetectMs = now;
                sendVelocity(vx, vy, vz);
                digitalWrite(STATUS_LED, HIGH);
            } else {
                // hold on every missed frame. sending nothing would leave the
                // FC flying the last velocity (up to VEL_MAX_XY) until LOST
                // kicks in ~FALLBACK_CYCLES x 100 ms later
                sendHold();
                primaryMissCount++;
                if (primaryMissCount >= FALLBACK_CYCLES) {
#if ENABLE_LINE_FALLBACK
                    Serial.println("[FOLLOWER] Primary lost -> FALLBACK");
                    trackMode = MODE_FALLBACK;
                    pixy.changeProg("line");
#else
                    // line features aren't people (see config.h), hold instead
                    Serial.println("[FOLLOWER] Primary lost -> LOST (line fallback disabled)");
                    trackMode = MODE_LOST;
#endif
                }
            }
            break;

        case MODE_FALLBACK: {
            detected = runFallback(vx, vy, vz);
            if (detected) {
                lastDetectMs = now;
                sendVelocity(vx, vy, vz);
                digitalWrite(STATUS_LED, (now / 250) % 2);
            } else {
                sendHold();   // same as PRIMARY, no target = zero velocity
                if (now - lastDetectMs > LOST_TIMEOUT_MS) {
                    Serial.println("[FOLLOWER] Fallback lost -> LOST");
                    trackMode = MODE_LOST;
                }
            }

            // Check if PRIMARY came back every 5th cycle (~2x a second)
            // instead of every loop. changeProg() swaps pixy's whole
            // pipeline, so doing line->ccc->line every 100 ms would keep
            // interrupting the fallback tracker for no real benefit.
            static uint8_t recoveryCheckCounter = 0;
            if (++recoveryCheckCounter >= 5) {
                recoveryCheckCounter = 0;
                pixy.changeProg("ccc");
                if (runPrimary(vx, vy, vz)) {
                    Serial.println("[FOLLOWER] Primary recovered -> PRIMARY");
                    trackMode = MODE_PRIMARY;
                    primaryMissCount = 0;
                    lastDetectMs = now;
                    sendVelocity(vx, vy, vz);
                    break;
                }
                pixy.changeProg("line");
            }
            break;
        }

        case MODE_LOST: {
            sendHold();
            digitalWrite(STATUS_LED, (now / 100) % 2);

            // same as FALLBACK, a couple checks a second is plenty while
            // holding
            static uint8_t lostRecoveryCounter = 0;
            if (++lostRecoveryCounter >= 5) {
                lostRecoveryCounter = 0;
                pixy.changeProg("ccc");
                if (runPrimary(vx, vy, vz)) {
                    Serial.println("[FOLLOWER] Recovered -> PRIMARY");
                    trackMode = MODE_PRIMARY;
                    primaryMissCount = 0;
                    lastDetectMs = now;
                    sendVelocity(vx, vy, vz);
                }
            }
            break;
        }
    }

    if (!detected && trackMode != MODE_LOST) {
        if (now - lastDetectMs > LOST_TIMEOUT_MS) {
            Serial.println("[FOLLOWER] Global timeout -> LOST");
            trackMode = MODE_LOST;
        }
    }
}
