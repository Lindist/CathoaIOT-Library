/**
 * @file CathoaIOT.cpp
 * @brief Implementation ของ CathoaIOT SDK v2.0.0
 *
 * รองรับ Two-Way Communication:
 *   ► Telemetry (Device → Cloud): publish ไปที่ {usernameMQTT ถ้ามี}/cathoaiot/telemetry/{deviceId}
 *   ► Commands  (Cloud → Device): subscribe ที่ {usernameMQTT ถ้ามี}/cathoaiot/command/{deviceId}
 *
 * ไฟล์นี้ encapsulate ทุกอย่างเกี่ยวกับ WiFi, MQTT, JSON serialisation
 * ผู้ใช้เพียงแค่เรียก begin(), loop(), sendTelemetry(), setCommandCallback()
 *
 * @version 2.0.0
 * @license MIT
 */

#include "CathoaIOT.h"

// ====================================================================== //
//  Constants – ค่าคงที่ที่ใช้ภายใน
// ====================================================================== //

// ====================================================================== //
//  Static Member Initialization
//  – จำเป็นสำหรับ static pointer ที่ใช้ใน PubSubClient callback
// ====================================================================== //

/**
 * เริ่มต้น static instance pointer เป็น nullptr
 * จะถูก set ใน constructor เพื่อชี้กลับไปยัง instance ปัจจุบัน
 */
CathoaIOT* CathoaIOT::_instance = nullptr;

// ====================================================================== //
//  Constructor: Simplified
// ====================================================================== //

CathoaIOT::CathoaIOT(Client& netClient, const char* deviceId)
    : _deviceId(deviceId)                   // เก็บ Device ID
    , _mqttHost("")                          // ต้อง setMqttHost() ก่อน begin()
    , _mqttPort(1883)                        // Default port
    , _mqttUsername("")                       // Default: anonymous
    , _mqttPassword("")                      // Default: anonymous
    , _telemetryTopicPrefix("v1/devices")    // Default topic prefix
    , _bufferSize(512)                       // Default buffer size
    , _userCommandCallback(nullptr)          // ยังไม่มี callback
    , _netClient(&netClient)                 // เก็บ pointer ไปที่ Client
    , _mqttClient(netClient)                 // ส่ง Client ให้ PubSubClient
{
    // เก็บ pointer ชี้กลับมาที่ instance นี้
    // ใช้สำหรับ static callback trampoline ของ PubSubClient
    _instance = this;
}

// ====================================================================== //
//  Constructor: Full
// ====================================================================== //

CathoaIOT::CathoaIOT(Client& netClient, const char* deviceId,
                     const char* mqttHost, uint16_t mqttPort, const char* telemetryTopicPrefix,
                     const char* mqttUsername, const char* mqttPassword, uint16_t bufferSize)
    : _deviceId(deviceId)                           // เก็บ Device ID
    , _mqttHost(mqttHost)                            // MQTT broker hostname
    , _mqttPort(mqttPort)                            // MQTT broker port
    , _mqttUsername(mqttUsername)                     // MQTT username
    , _mqttPassword(mqttPassword)                    // MQTT password
    , _telemetryTopicPrefix(telemetryTopicPrefix)    // Topic prefix
    , _bufferSize(bufferSize)                        // Buffer size
    , _userCommandCallback(nullptr)                  // ยังไม่มี callback
    , _netClient(&netClient)                         // เก็บ pointer ไปที่ Client
    , _mqttClient(netClient)                         // ส่ง Client ให้ PubSubClient
{
    _instance = this;
}

// ====================================================================== //
//  Configuration Setters – เรียกก่อน begin() เพื่อปรับค่า
// ====================================================================== //

void CathoaIOT::setMqttHost(const char* host)                    { _mqttHost = host; }
void CathoaIOT::setMqttPort(uint16_t port)                       { _mqttPort = port; }
void CathoaIOT::setMqttCredentials(const char* user, const char* pass) {
    _mqttUsername = user;
    _mqttPassword = pass;
}
void CathoaIOT::setTelemetryTopicPrefix(const char* prefix)      { _telemetryTopicPrefix = prefix; }
void CathoaIOT::setBufferSize(uint16_t size)                     { _bufferSize = size; }

// ====================================================================== //
//  Command Callback Registration
//  – ผู้ใช้เรียกเพื่อลงทะเบียน callback สำหรับรับคำสั่ง
// ====================================================================== //

/**
 * @brief ลงทะเบียน callback function สำหรับรับคำสั่งจาก Cloud
 *
 * เมื่อมี message เข้ามาที่ command topic, ไลบรารีจะ parse JSON
 * แล้วเรียก callback นี้พร้อมส่ง command name และ payload ให้
 *
 * @param callback  Function pointer: void callback(String command, String payload)
 */
void CathoaIOT::setCommandCallback(CommandCallback callback) {
    _userCommandCallback = callback;
    Serial.println(F("[CathoaIOT] Command callback registered."));
}

// ====================================================================== //
//  Static MQTT Callback Trampoline
//  – ทำหน้าที่เป็นตัวกลางระหว่าง PubSubClient (C-style) กับ class method
// ====================================================================== //

/**
 * PubSubClient ต้องการ callback เป็น static function (C-style):
 *   void callback(char* topic, byte* payload, unsigned int length)
 *
 * แต่เราต้องการเรียก member function ของ class (เพราะต้องเข้าถึง
 * _userCommandCallback ที่เป็น member variable)
 *
 * วิธีแก้: ใช้ "trampoline pattern"
 *   1. PubSubClient เรียก static function นี้
 *   2. function นี้ใช้ _instance pointer เรียก member function ต่อ
 *
 * ข้อจำกัด: รองรับ CathoaIOT instance เดียวต่อโปรแกรม
 *           (ปกติสำหรับ ESP32 ที่มี device เดียว)
 */
void CathoaIOT::_mqttCallbackTrampoline(char* topic, byte* payload, unsigned int length) {
    // ตรวจสอบว่ามี instance อยู่จริง เพื่อป้องกัน null pointer
    if (_instance != nullptr) {
        _instance->_handleIncomingMessage(topic, payload, length);
    }
}

// ====================================================================== //
//  Internal: Handle Incoming MQTT Message
//  – ประมวลผล message ที่เข้ามาจาก command topic
// ====================================================================== //

/**
 * ขั้นตอนการทำงาน:
 *   1. แปลง byte array payload → String
 *   2. แสดง log: topic + payload ดิบ
 *   3. Parse JSON ด้วย ArduinoJson v7
 *   4. ดึง "command" key และ "status" (fallback "payload") key
 *   5. เรียก user callback (ถ้าลงทะเบียนไว้)
 *
 * รูปแบบ JSON ที่คาดหวัง:
 *   {"command": "toggleLight", "status": "ON"}
 *   หรือ
 *   {"command": "setTemp", "payload": "25"}
 */
void CathoaIOT::_handleIncomingMessage(char* topic, byte* payload, unsigned int length) {
    // ---- Step 1: แปลง byte payload เป็น String -------------------- //
    // PubSubClient ส่ง payload มาเป็น byte array (ไม่มี null terminator)
    // ต้องสร้าง String โดยกำหนดความยาวเอง
    String message;
    message.reserve(length);  // จอง memory ล่วงหน้าเพื่อประสิทธิภาพ
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // ---- Step 2: แสดง debug log ----------------------------------- //
    Serial.print(F("[CathoaIOT] CMD received on ["));
    Serial.print(topic);
    Serial.print(F("] → "));
    Serial.println(message);

    // ---- Step 3: ตรวจสอบว่ามี user callback หรือไม่ ---------------- //
    if (_userCommandCallback == nullptr) {
        Serial.println(F("[CathoaIOT] No command callback registered. Ignoring."));
        return;
    }

    // ---- Step 4: Parse JSON ด้วย ArduinoJson v7 ------------------- //
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        // ถ้า parse ไม่สำเร็จ → ส่ง raw message เป็น payload แทน
        Serial.print(F("[CathoaIOT] JSON parse failed: "));
        Serial.println(error.c_str());
        // Fallback: ส่ง raw message ทั้งก้อนเป็น payload
        _userCommandCallback("raw", message);
        return;
    }

    // ---- Step 5: ดึง command name และ payload จาก JSON ------------- //
    // ดึง "command" key (required)
    String command = doc["command"].as<String>();

    // ดึง "status" key ก่อน, ถ้าไม่มีจะ fallback ไป "payload" key
    // รองรับทั้ง: {"command":"x","status":"ON"}
    //         และ: {"command":"x","payload":"value"}
    String cmdPayload;
    if (doc.containsKey("status")) {
        cmdPayload = doc["status"].as<String>();
    } else if (doc.containsKey("payload")) {
        cmdPayload = doc["payload"].as<String>();
    } else {
        // ถ้าไม่มีทั้ง status และ payload → ส่ง empty string
        cmdPayload = "";
    }

    // ---- Step 6: เรียก user callback ------------------------------ //
    Serial.print(F("[CathoaIOT] → Dispatching: command=\""));
    Serial.print(command);
    Serial.print(F("\", payload=\""));
    Serial.print(cmdPayload);
    Serial.println(F("\""));

    _userCommandCallback(command, cmdPayload);
}

// ====================================================================== //
//  Lifecycle: begin()
//  – เรียกครั้งเดียวใน setup() เพื่อ initialise ทุกอย่าง
// ====================================================================== //

void CathoaIOT::begin() {
    Serial.println();
    Serial.println(F("======================================"));
    Serial.println(F(" CathoaIOT SDK v2.0.0 - Initialising"));
    Serial.println(F("======================================"));

    // ---- (Network connection is managed externally) --------------- //

    // ---- Step 3: ตั้งค่า MQTT broker ------------------------------ //
    _mqttClient.setServer(_mqttHost, _mqttPort);
    _mqttClient.setBufferSize(_bufferSize);

    // ---- Step 4: ลงทะเบียน internal MQTT callback ----------------- //
    // ใช้ static trampoline function เพื่อรับ message จาก PubSubClient
    // แล้วส่งต่อให้ _handleIncomingMessage() (member function)
    _mqttClient.setCallback(_mqttCallbackTrampoline);

    Serial.print(F("[CathoaIOT] MQTT broker: "));
    Serial.print(_mqttHost);
    Serial.print(F(":"));
    Serial.println(_mqttPort);

    // ---- Step 5: เชื่อมต่อ MQTT + auto-subscribe command topic ---- //
    _connectMqtt();

    Serial.println(F("[CathoaIOT] Ready. Two-Way Communication enabled."));
}

// ====================================================================== //
//  Lifecycle: loop()
//  – เรียกทุกรอบใน Arduino loop()
// ====================================================================== //

void CathoaIOT::loop() {

    // ---- MQTT Health Check ---------------------------------------- //
    // ตรวจสอบว่า MQTT ยังเชื่อมต่ออยู่ → reconnect + re-subscribe ถ้าหลุด
    if (!_mqttClient.connected()) {
        Serial.println(F("[CathoaIOT] MQTT lost – reconnecting..."));
        _connectMqtt();  // _connectMqtt() จะ auto-subscribe command topic
    }

    // ---- Process Incoming MQTT Packets ---------------------------- //
    // สำคัญมาก: ต้องเรียก mqttClient.loop() ทุกรอบ
    // เพื่อให้ PubSubClient ประมวลผล incoming message และ keep alive
    _mqttClient.loop();
}

// ====================================================================== //
//  Telemetry: sendTelemetry(key, float)
//  – ส่งค่า numeric (float) ไปยัง Cloud
// ====================================================================== //

bool CathoaIOT::sendTelemetry(String key, float value) {
    // ตรวจสอบว่า MQTT connected ก่อนส่ง
    if (!_mqttClient.connected()) {
        Serial.println(F("[CathoaIOT] sendTelemetry failed: not connected."));
        return false;
    }

    // สร้าง JSON document: { "<key>": <value> }
    JsonDocument doc;
    doc[key] = value;

    // Serialise JSON ลง stack buffer
    char buffer[256];
    const size_t n = serializeJson(doc, buffer, sizeof(buffer));

    // สร้าง topic: v1/devices/{deviceId}/telemetry
    char topic[160];
    _buildTelemetryTopic(topic, sizeof(topic));

    // แสดง debug log
    Serial.print(F("[CathoaIOT] PUB "));
    Serial.print(topic);
    Serial.print(F(" → "));
    Serial.println(buffer);

    // Publish ไปยัง MQTT broker
    return _mqttClient.publish(topic, buffer, n);
}

// ====================================================================== //
//  Telemetry: sendTelemetry(key, String)
//  – ส่งค่า String ไปยัง Cloud
// ====================================================================== //

bool CathoaIOT::sendTelemetry(String key, String value) {
    if (!_mqttClient.connected()) {
        Serial.println(F("[CathoaIOT] sendTelemetry failed: not connected."));
        return false;
    }

    // สร้าง JSON: { "<key>": "<value>" }
    JsonDocument doc;
    doc[key] = value;

    char buffer[256];
    const size_t n = serializeJson(doc, buffer, sizeof(buffer));

    char topic[160];
    _buildTelemetryTopic(topic, sizeof(topic));

    Serial.print(F("[CathoaIOT] PUB "));
    Serial.print(topic);
    Serial.print(F(" → "));
    Serial.println(buffer);

    return _mqttClient.publish(topic, buffer, n);
}

// ====================================================================== //
//  Telemetry: sendTelemetry(key, bool)
//  – ส่งค่า boolean ไปยัง Cloud
// ====================================================================== //

bool CathoaIOT::sendTelemetry(String key, bool value) {
    if (!_mqttClient.connected()) {
        Serial.println(F("[CathoaIOT] sendTelemetry failed: not connected."));
        return false;
    }

    // สร้าง JSON: { "<key>": true/false }
    JsonDocument doc;
    doc[key] = value;

    char buffer[256];
    const size_t n = serializeJson(doc, buffer, sizeof(buffer));

    char topic[160];
    _buildTelemetryTopic(topic, sizeof(topic));

    Serial.print(F("[CathoaIOT] PUB "));
    Serial.print(topic);
    Serial.print(F(" → "));
    Serial.println(buffer);

    return _mqttClient.publish(topic, buffer, n);
}

// ====================================================================== //
//  Status Helpers – ตรวจสอบสถานะ connection
// ====================================================================== //


bool CathoaIOT::isMqttConnected() const {
    return _mqttClient.connected();
}

const char* CathoaIOT::getDeviceId() const {
    return _deviceId;
}


// ====================================================================== //
//  Private: MQTT Connection with Retry + Auto-Subscribe
//  – เชื่อมต่อ MQTT broker + subscribe command topic อัตโนมัติ
// ====================================================================== //

/**
 * เมื่อเชื่อมต่อ MQTT สำเร็จ (ทั้ง connect ครั้งแรก และ reconnect):
 *   1. Subscribe ไปที่ v1/devices/{deviceId}/cmd โดยอัตโนมัติ
 *   2. แสดง log ยืนยัน
 *
 * นี่คือส่วนสำคัญตาม requirement:
 *   "Inside the library's connectMQTT() function, upon a successful
 *    connection or reconnection, it MUST automatically subscribe()
 *    to the Command topic."
 */
void CathoaIOT::_connectMqtt() {
    while (!_mqttClient.connected()) {
        Serial.print(F("[CathoaIOT] MQTT connecting as "));
        Serial.print(_deviceId);
        Serial.print(F("..."));

        bool ok;
        // ถ้ามี credentials → ใช้ authenticated connect
        // ถ้าไม่มี → ใช้ anonymous connect
        if (strlen(_mqttUsername) > 0) {
            ok = _mqttClient.connect(_deviceId, _mqttUsername, _mqttPassword);
        } else {
            ok = _mqttClient.connect(_deviceId);
        }

        if (ok) {
            Serial.println(F(" connected!"));

            // ============================================================ //
            //  AUTO-SUBSCRIBE to Command Topic (ตาม requirement)
            //  เมื่อเชื่อมต่อสำเร็จ ต้อง subscribe command topic ทันที
            //  ทั้งตอน connect ครั้งแรก และตอน reconnect
            // ============================================================ //
            char cmdTopic[160];
            _buildCommandTopic(cmdTopic, sizeof(cmdTopic));

            if (_mqttClient.subscribe(cmdTopic)) {
                Serial.print(F("[CathoaIOT] Subscribed to: "));
                Serial.println(cmdTopic);
            } else {
                Serial.print(F("[CathoaIOT] Failed to subscribe: "));
                Serial.println(cmdTopic);
            }

            return;  // เชื่อมต่อสำเร็จ → ออกจาก loop
        }

        // เชื่อมต่อไม่สำเร็จ → แสดง error code แล้ว retry
        Serial.print(F(" failed (rc="));
        Serial.print(_mqttClient.state());
        Serial.println(F("). Retrying in 2 s..."));
        delay(2000);
    }
}

// ====================================================================== //
//  Private: Topic Builders
//  – สร้าง topic string ตาม MQTT Topic Architecture
// ====================================================================== //

/**
 * สร้าง Telemetry Topic: {usernameMQTT ถ้ามี}/cathoaiot/telemetry/{deviceId}
 */
void CathoaIOT::_buildTelemetryTopic(char* buf, size_t bufLen) const {
    if (_mqttUsername != nullptr && strlen(_mqttUsername) > 0) {
        snprintf(buf, bufLen, "%s/cathoaiot/telemetry/%s", _mqttUsername, _deviceId);
    } else {
        snprintf(buf, bufLen, "cathoaiot/telemetry/%s", _deviceId);
    }
}

/**
 * สร้าง Command Topic: {usernameMQTT ถ้ามี}/cathoaiot/command/{deviceId}
 */
void CathoaIOT::_buildCommandTopic(char* buf, size_t bufLen) const {
    if (_mqttUsername != nullptr && strlen(_mqttUsername) > 0) {
        snprintf(buf, bufLen, "%s/cathoaiot/command/%s", _mqttUsername, _deviceId);
    } else {
        snprintf(buf, bufLen, "cathoaiot/command/%s", _deviceId);
    }
}
