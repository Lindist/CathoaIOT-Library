/**
 * @file CathoaIOT.h
 * @brief CathoaIOT – Lightweight C++ SDK for connecting ESP32 devices
 *        to the Cathoa IoT Dashboard platform via secure MQTT (TLS).
 *
 * =====================================================================
 *  ✦ Two-Way Communication Support (v2.0.0)
 * =====================================================================
 *  This library encapsulates all WiFi, MQTT, and JSON complexity so that
 *  end-users only need to call begin(), loop(), sendTelemetry(), and
 *  setCommandCallback() to achieve full two-way communication:
 *
 *   ► Telemetry (Device → Cloud):
 *       Publishes JSON payloads to  v1/devices/{deviceId}/telemetry
 *
 *   ► Commands  (Cloud → Device):
 *       Subscribes to  v1/devices/{deviceId}/cmd
 *       Incoming messages are parsed and forwarded to a user-defined callback.
 *
 * MQTT Topic Architecture:
 *   - Publish:   v1/devices/{deviceId}/telemetry
 *   - Subscribe: v1/devices/{deviceId}/cmd
 *
 * Dependencies:
 *   - WiFi.h           (ESP32 Arduino core)
 *   - WiFiClientSecure.h
 *   - PubSubClient.h   (knolleary/pubsubclient)
 *   - ArduinoJson.h    (v7+)
 *
 * @version 2.0.0
 * @license MIT
 */

#ifndef CATHOA_IOT_H
#define CATHOA_IOT_H

#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ====================================================================== //
//  Type alias สำหรับ Command Callback ที่ผู้ใช้กำหนดเอง
//  (User-defined command callback signature)
//
//  พารามิเตอร์:
//    - command : ชื่อคำสั่งที่ได้รับ (e.g., "toggleLight")
//    - payload : ค่าของคำสั่ง (e.g., "ON", "OFF", หรือ JSON string)
// ====================================================================== //
typedef void (*CommandCallback)(String command, String payload);

/**
 * @class CathoaIOT
 * @brief Main SDK class – manages WiFi, MQTT (TLS), telemetry publishing,
 *        and command receiving (two-way communication).
 *
 * Usage:
 * @code
 *   // --- สร้าง instance ด้วย constructor ที่เรียบง่าย ---
 *   CathoaIOT iot("MySSID", "MyPass", "device-uuid-here");
 *
 *   // --- กำหนด callback สำหรับรับคำสั่ง ---
 *   void onCommand(String command, String payload) {
 *       Serial.println("Received: " + command + " = " + payload);
 *   }
 *
 *   void setup() {
 *       iot.setCommandCallback(onCommand);
 *       iot.begin();
 *   }
 *
 *   void loop() {
 *       iot.loop();
 *       iot.sendTelemetry("temp", 25.3f);
 *   }
 * @endcode
 */
class CathoaIOT {
public:
    // ================================================================== //
    //  Construction
    // ================================================================== //

    /**
     * @brief Construct a new CathoaIOT instance (Simplified constructor).
     *
     * @param netClient Network Client object (เช่น WiFiClient, EthernetClient)
     * @param deviceId  Device ID ที่ลงทะเบียนบน Dashboard
     */
    CathoaIOT(Client& netClient, const char* deviceId);

    /**
     * @brief Construct a new CathoaIOT instance (Full constructor).
     *
     * @param netClient             Network Client object
     * @param deviceId              Device ID ที่ลงทะเบียนบน Dashboard
     * @param mqttHost              Hostname ของ MQTT broker
     * @param mqttPort              TCP port ของ MQTT broker
     * @param telemetryTopicPrefix  Topic prefix (default: "v1/devices")
     * @param mqttUsername          MQTT username (default: "")
     * @param mqttPassword          MQTT password (default: "")
     * @param bufferSize            ขนาด MQTT receive buffer (default: 512)
     */
    CathoaIOT(Client& netClient, const char* deviceId,
              const char* mqttHost, uint16_t mqttPort,
              const char* telemetryTopicPrefix,
              const char* mqttUsername = "",
              const char* mqttPassword = "",
              uint16_t bufferSize = 512);

    // ================================================================== //
    //  Lifecycle – ฟังก์ชันหลักที่ต้องเรียกใน setup() และ loop()
    // ================================================================== //

    /**
     * @brief Initialise WiFi + MQTT connections.
     *        เรียกครั้งเดียวใน Arduino setup()
     *
     * ขั้นตอนภายใน:
     *   1. ตั้งค่า MQTT broker, buffer, และ internal callback
     *   2. เชื่อมต่อ MQTT ด้วย _connectMqtt() (auto-subscribe command topic)
     */
    void begin();

    /**
     * @brief Keep connections alive and process incoming MQTT packets.
     *        เรียกทุกรอบใน Arduino loop()
     *
     * ทำหน้าที่:
     *   - ตรวจสอบ MQTT connection → reconnect + re-subscribe ถ้าหลุด
     *   - เรียก mqttClient.loop() เพื่อ process incoming packets
     */
    void loop();

    // ================================================================== //
    //  Command Callback – ระบบรับคำสั่งจาก Cloud
    // ================================================================== //

    /**
     * @brief Register a user-defined callback for incoming MQTT commands.
     *
     * เมื่อมีข้อความเข้ามาที่ topic v1/devices/{deviceId}/cmd
     * ไลบรารีจะ parse JSON และเรียก callback นี้พร้อมส่ง:
     *   - command : ค่าของ key "command" ใน JSON
     *   - payload : ค่าของ key "status" (หรือ "payload") ใน JSON
     *
     * ตัวอย่าง JSON ที่รับ:
     *   {"command": "toggleLight", "status": "ON"}
     *
     * ตัวอย่างการใช้งาน:
     * @code
     *   void handleCommand(String command, String payload) {
     *       if (command == "toggleLight") {
     *           digitalWrite(LED_PIN, payload == "ON" ? HIGH : LOW);
     *       }
     *   }
     *   iot.setCommandCallback(handleCommand);
     * @endcode
     *
     * @param callback  ฟังก์ชัน pointer ที่รับ (String command, String payload)
     */
    void setCommandCallback(CommandCallback callback);

    // ================================================================== //
    //  MQTT Configuration – ตั้งค่าเพิ่มเติม (เรียกก่อน begin())
    // ================================================================== //

    /**
     * @brief Override MQTT broker hostname.
     * @param host  Hostname ของ MQTT broker (e.g., "broker.hivemq.com")
     */
    void setMqttHost(const char* host);

    /**
     * @brief Override MQTT broker port (default: 8883 สำหรับ TLS).
     * @param port  TCP port number
     */
    void setMqttPort(uint16_t port);

    /**
     * @brief Set MQTT credentials (username/password).
     *        จำเป็นสำหรับ HiveMQ Cloud หรือ broker ที่ต้อง authenticate
     * @param username  MQTT username
     * @param password  MQTT password
     */
    void setMqttCredentials(const char* username, const char* password);

    /**
     * @brief Override telemetry topic prefix.
     *        Full topic จะเป็น: <prefix>/<deviceId>/telemetry
     *        Default: "v1/devices"
     * @param prefix  Topic prefix string (ไม่ต้องมี trailing slash)
     */
    void setTelemetryTopicPrefix(const char* prefix);

    /**
     * @brief Set MQTT receive buffer size (default: 512 bytes).
     * @param size  ขนาด buffer เป็น bytes
     */
    void setBufferSize(uint16_t size);

    // ================================================================== //
    //  Telemetry Publishing – ส่งข้อมูล Telemetry ไปยัง Cloud
    // ================================================================== //

    /**
     * @brief Publish a numeric (float) telemetry value.
     *
     * สร้าง JSON: { "<key>": <value> }
     * แล้ว publish ไปที่ v1/devices/{deviceId}/telemetry
     *
     * @param key    ชื่อ telemetry key (e.g., "temperatureC")
     * @param value  ค่า numeric (float)
     * @return true  ถ้า MQTT publish สำเร็จ
     * @return false ถ้า client ไม่ connected หรือ publish ล้มเหลว
     */
    bool sendTelemetry(String key, float value);

    /**
     * @brief Publish a string telemetry value (overloaded).
     *
     * สร้าง JSON: { "<key>": "<value>" }
     * แล้ว publish ไปที่ v1/devices/{deviceId}/telemetry
     *
     * @param key    ชื่อ telemetry key (e.g., "status")
     * @param value  ค่า String
     * @return true  ถ้า MQTT publish สำเร็จ
     * @return false ถ้า client ไม่ connected หรือ publish ล้มเหลว
     */
    bool sendTelemetry(String key, String value);

    /**
     * @brief Publish a boolean telemetry value (overloaded).
     *
     * สร้าง JSON: { "<key>": true/false }
     * แล้ว publish ไปที่ v1/devices/{deviceId}/telemetry
     *
     * @param key    ชื่อ telemetry key (e.g., "switchOn")
     * @param value  ค่า boolean
     * @return true  ถ้า MQTT publish สำเร็จ
     * @return false ถ้า client ไม่ connected หรือ publish ล้มเหลว
     */
    bool sendTelemetry(String key, bool value);

    // ================================================================== //
    //  Status Queries – ตรวจสอบสถานะ
    // ================================================================== //

    /** @brief ตรวจสอบว่า MQTT เชื่อมต่ออยู่หรือไม่ */
    bool isMqttConnected();

    /** @brief ดึง Device ID ที่ส่งเข้า constructor */
    const char* getDeviceId() const;

private:
    // ================================================================== //
    //  Private: Stored Configuration – ค่าที่เก็บภายใน
    // ================================================================== //

    const char* _deviceId;          ///< Device ID ที่ใช้ระบุตัวตนบน platform

    const char* _mqttHost;          ///< MQTT broker hostname
    uint16_t    _mqttPort;          ///< MQTT broker port (default: 8883)
    const char* _mqttUsername;      ///< MQTT authentication username
    const char* _mqttPassword;      ///< MQTT authentication password
    const char* _telemetryTopicPrefix;  ///< Topic prefix สำหรับ telemetry
    uint16_t    _bufferSize;        ///< MQTT receive buffer size

    // ================================================================== //
    //  Private: Command Callback – เก็บ callback ที่ผู้ใช้ลงทะเบียน
    // ================================================================== //

    /**
     * @brief Pointer ไปยังฟังก์ชัน callback ที่ผู้ใช้กำหนด
     *        ค่า default เป็น nullptr (ไม่มี callback)
     *        ถ้าเป็น nullptr จะไม่เรียก callback เมื่อได้รับ command
     */
    CommandCallback _userCommandCallback;

    // ================================================================== //
    //  Private: Static Instance Pointer – สำหรับ PubSubClient callback
    // ================================================================== //

    /**
     * @brief Static pointer ชี้กลับไปยัง instance ปัจจุบัน
     *
     * เหตุผล: PubSubClient ต้องการ callback แบบ C-style (static function)
     * ดังนั้นเราจำเป็นต้องมี static pointer เพื่อให้ static function
     * สามารถเรียก member function ของ instance ได้
     *
     * ข้อจำกัด: รองรับ instance เดียวต่อโปรแกรม (ปกติสำหรับ ESP32)
     */
    static CathoaIOT* _instance;

    // ================================================================== //
    //  Private: Network Stack – WiFi + MQTT client objects
    // ================================================================== //

    Client*          _netClient;    ///< Pointer เก็บ Network Client
    PubSubClient     _mqttClient;   ///< MQTT client

    // ================================================================== //
    //  Private: Internal Helpers – ฟังก์ชันภายในสำหรับจัดการ connection
    // ================================================================== //

    /**
     * @brief เชื่อมต่อ (หรือ reconnect) MQTT broker พร้อม retry loop
     *
     * เมื่อเชื่อมต่อสำเร็จ จะ:
     *   1. Subscribe topic: v1/devices/{deviceId}/cmd โดยอัตโนมัติ
     *   2. แสดง log ผ่าน Serial
     */
    void _connectMqtt();

    /**
     * @brief สร้าง full telemetry topic string ลงใน buffer ที่ให้มา
     *
     * รูปแบบ topic: v1/devices/{deviceId}/telemetry
     *
     * @param buf     buffer ปลายทาง (ต้องมีอย่างน้อย 160 chars)
     * @param bufLen  ขนาดของ buffer
     */
    void _buildTelemetryTopic(char* buf, size_t bufLen) const;

    /**
     * @brief สร้าง full command topic string ลงใน buffer ที่ให้มา
     *
     * รูปแบบ topic: v1/devices/{deviceId}/cmd
     *
     * @param buf     buffer ปลายทาง (ต้องมีอย่างน้อย 160 chars)
     * @param bufLen  ขนาดของ buffer
     */
    void _buildCommandTopic(char* buf, size_t bufLen) const;

    /**
     * @brief Static MQTT callback (trampoline) สำหรับ PubSubClient
     *
     * PubSubClient ต้องการ callback แบบ static C-style function:
     *   void callback(char* topic, byte* payload, unsigned int length)
     *
     * ฟังก์ชันนี้ทำหน้าที่เป็น "trampoline" – รับ callback จาก PubSubClient
     * แล้วส่งต่อให้ member function _handleIncomingMessage() ผ่าน _instance
     *
     * @param topic    ชื่อ topic ที่ข้อความมาจาก
     * @param payload  Byte array ของ message payload
     * @param length   ความยาวของ payload
     */
    static void _mqttCallbackTrampoline(char* topic, byte* payload, unsigned int length);

    /**
     * @brief Member function ที่ประมวลผลข้อความ MQTT ที่เข้ามา
     *
     * ขั้นตอน:
     *   1. แปลง byte payload เป็น String
     *   2. Parse JSON ด้วย ArduinoJson v7
     *   3. ดึงค่า "command" และ "status" (หรือ "payload") จาก JSON
     *   4. เรียก user callback ที่ลงทะเบียนไว้ (ถ้ามี)
     *
     * รูปแบบ JSON ที่คาดหวัง:
     *   {"command": "toggleLight", "status": "ON"}
     *   หรือ
     *   {"command": "setTemp", "payload": "25"}
     *
     * @param topic    ชื่อ topic ที่ข้อความมาจาก
     * @param payload  Byte array ของ message payload
     * @param length   ความยาวของ payload
     */
    void _handleIncomingMessage(char* topic, byte* payload, unsigned int length);
};

#endif // CATHOA_IOT_H
