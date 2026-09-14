/*  ================================================================
    FOLLOWER DRONE — ESP32 Companion Controller  (v3)
    Hardware: ESP32 + Pixy2 (SPI) + MicoAir H743 AIO (UART1, MAVLink2)

    v3: the armed/mode/link-timeout gating logic now lives in
    safety_gate.h — a pure, hardware-independent function that's
    ALSO compiled and tested standalone on desktop (test/native/).
    This file calls that exact function; it does not reimplement the
    logic inline anymore, so a passing native test is a guarantee
    about this actual flight code, not a parallel copy of it.

    Tracking strategy unchanged:
      PRIMARY   — Pixy2 color blob (subject wears a colored beacon)
      FALLBACK  — Pixy2 line/silhouette tracking (CC mode 2)
      LOST      — hold position, emit warning
    ================================================================ */

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
uint8_t  fcCustomMode     = 0;
uint32_t lastFcHeartbeatMs = 0;
#define ARDUCOPTER_MODE_GUIDED  4
#define FC_LINK_TIMEOUT_MS      3000

mavlink_status_t mavStatus;
mavlink_message_t mavMsg;

// ── Outgoing helpers ─────────────────────────────────────────────
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

/*  Every velocity command goes through evaluate_safety_gate() first -
    the SAME function proven by test/native/test_safety_gate.cpp. */
void sendVelocity(float vx, float vy, float vz) {
    SafetyGateResult gate = evaluate_safety_gate(
        fcArmed, fcCustomMode, ARDUCOPTER_MODE_GUIDED,
        lastFcHeartbeatMs, millis(), FC_LINK_TIMEOUT_MS
    );
    if (!gate.should_command) {
        Serial.printf("[GATE] Suppressed: %s\n", gate.reason);
        return;
    }

    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    vx = constrain(vx, -VEL_MAX_XY, VEL_MAX_XY);
    vy = constrain(vy, -VEL_MAX_XY, VEL_MAX_XY);
    vz = constrain(vz, -VEL_MAX_Z,  VEL_MAX_Z);

    mavlink_msg_set_position_target_local_ned_pack(
        MY_SYSID, MY_COMPID, &msg,
        millis(), FC_SYSID, FC_COMPID,
        MAV_FRAME_BODY_NED,
        0x0FC7,
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

// ── Incoming: parse the FC's own HEARTBEAT to track armed/mode ────
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

// ── Primary tracking: color blob ──────────────────────────────────
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

// ── Fallback tracking: line / silhouette ──────────────────────────
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

// ─────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    FC_SERIAL.begin(FC_BAUD, SERIAL_8N1, 16, 17);
    pinMode(STATUS_LED, OUTPUT);

    Serial.println("[FOLLOWER] Initialising Pixy2...");
    pixy.init();
    pixy.changeProg("ccc");
    Serial.println("[FOLLOWER] Pixy2 ready.");
    Serial.printf("[FOLLOWER] Primary sig: %d  Lost timeout: %d ms\n", PRIMARY_SIG, LOST_TIMEOUT_MS);
    Serial.println("[FOLLOWER] Waiting for FC heartbeat before commanding...");
}

// ─────────────────────────────────────────────────────────────────
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
        Serial.println("[FOLLOWER] FC link timeout — holding");
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
                primaryMissCount++;
                if (primaryMissCount >= FALLBACK_CYCLES) {
                    Serial.println("[FOLLOWER] Primary lost -> FALLBACK");
                    trackMode = MODE_FALLBACK;
                    pixy.changeProg("line");
                }
            }
            break;

        case MODE_FALLBACK:
            detected = runFallback(vx, vy, vz);
            if (detected) {
                lastDetectMs = now;
                sendVelocity(vx, vy, vz);
                digitalWrite(STATUS_LED, (now / 250) % 2);
            } else if (now - lastDetectMs > LOST_TIMEOUT_MS) {
                Serial.println("[FOLLOWER] Fallback lost -> LOST");
                trackMode = MODE_LOST;
            }
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
            break;

        case MODE_LOST:
            sendHold();
            digitalWrite(STATUS_LED, (now / 100) % 2);
            pixy.changeProg("ccc");
            if (runPrimary(vx, vy, vz)) {
                Serial.println("[FOLLOWER] Recovered -> PRIMARY");
                trackMode = MODE_PRIMARY;
                primaryMissCount = 0;
                lastDetectMs = now;
                sendVelocity(vx, vy, vz);
            }
            break;
    }

    if (!detected && trackMode != MODE_LOST) {
        if (now - lastDetectMs > LOST_TIMEOUT_MS) {
            Serial.println("[FOLLOWER] Global timeout -> LOST");
            trackMode = MODE_LOST;
        }
    }
}
