/**
 * @file TwoWayCommunication.ino
 * @brief ตัวอย่าง Two-Way Communication ด้วย CathoaIOT Library
 *
 * แสดงการใช้งาน:
 *   ► ส่ง temperature data ทุก 5 วินาที ด้วย sendTelemetry()
 *   ► รับคำสั่งจาก Cloud ด้วย setCommandCallback()
 *     เช่น {"command": "toggleLight", "status": "ON"}
 *     แล้วเปิด/ปิด LED ที่ Pin 2 ของ ESP32
 *
 * MQTT Topic ที่ใช้:
 *   - Publish:   v1/devices/{deviceId}/telemetry
 *   - Subscribe: v1/devices/{deviceId}/cmd
 *
 * วิธีใช้:
 *   1. ติดตั้ง CathoaIOT library
 *   2. แก้ WiFi credentials, MQTT credentials, และ Device ID ด้านล่าง
 *   3. Upload ไปยังบอร์ด ESP32
 *   4. เปิด Serial Monitor ที่ 115200 baud
 *   5. ส่ง command JSON ไปที่ topic v1/devices/{DEVICE_ID}/cmd
 *      เช่น: {"command": "toggleLight", "status": "ON"}
 */

#include <CathoaIOT.h>

// ======================= User Configuration ========================== //

#if defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
#endif

// WiFi credentials – เปลี่ยนเป็นค่าของคุณ
static const char* WIFI_SSID     = "Wokwi-GUEST";       // ← เปลี่ยน WiFi SSID
static const char* WIFI_PASSWORD = "";                    // ← เปลี่ยน WiFi password

// Device ID – ต้องตรงกับที่ลงทะเบียนบน Cathoa Dashboard
static const char* DEVICE_ID     = "YOUR-DEVICE-UUID";   // ← เปลี่ยน Device ID

// MQTT broker settings
static const char* MQTT_HOST     = "f13e31dd434f4a099bfe1f13ec6e84a9.s1.eu.hivemq.cloud";
static const char* MQTT_USER     = "iot_user";            // ← เปลี่ยน MQTT username
static const char* MQTT_PASS     = "YourPassword";        // ← เปลี่ยน MQTT password

// ===================================================================== //

// Pin ของ Built-in LED บน ESP32 (ปกติคือ Pin 2)
static constexpr int LED_PIN = 2;

// สร้าง Network Client (แบบ Secure สำหรับ ESP32)
WiFiClientSecure netClient;

// ---- สร้าง CathoaIOT instance ด้วย Client Injection ----------- //
// ส่ง netClient และ deviceId ให้ไลบรารี
// แล้วใช้ setter methods ตั้งค่า MQTT ทีหลัง
CathoaIOT iot(netClient, DEVICE_ID);

// Publish interval – ส่ง telemetry ทุก 5 วินาที
static constexpr unsigned long TELEMETRY_INTERVAL_MS = 5000;
static unsigned long lastSendMs = 0;

// ====================================================================== //
//  Command Callback Function
//  – ฟังก์ชันที่จะถูกเรียกเมื่อมีคำสั่งเข้ามาจาก Cloud
// ====================================================================== //

/**
 * @brief ฟังก์ชัน callback สำหรับรับคำสั่งจาก MQTT
 *
 * เมื่อ Cloud ส่ง JSON มาที่ topic v1/devices/{deviceId}/cmd
 * เช่น: {"command": "toggleLight", "status": "ON"}
 *
 * ไลบรารีจะ parse JSON แล้วเรียกฟังก์ชันนี้พร้อมส่ง:
 *   - command = "toggleLight"
 *   - payload = "ON"
 *
 * @param command  ชื่อคำสั่ง (ค่า "command" ใน JSON)
 * @param payload  ค่าของคำสั่ง (ค่า "status" หรือ "payload" ใน JSON)
 */
void handleCommand(String command, String payload) {
    Serial.println(F("========================================"));
    Serial.print(F("  Command received: "));
    Serial.println(command);
    Serial.print(F("  Payload: "));
    Serial.println(payload);
    Serial.println(F("========================================"));

    // ---- ตรวจสอบคำสั่ง "toggleLight" ------------------------------ //
    if (command == "toggleLight") {
        if (payload == "ON") {
            // เปิด LED
            digitalWrite(LED_PIN, HIGH);
            Serial.println(F("[LED] Turned ON"));
        } else if (payload == "OFF") {
            // ปิด LED
            digitalWrite(LED_PIN, LOW);
            Serial.println(F("[LED] Turned OFF"));
        } else {
            Serial.print(F("[LED] Unknown status: "));
            Serial.println(payload);
        }
    }
    // ---- เพิ่มคำสั่งอื่นๆ ได้ที่นี่ -------------------------------- //
    // else if (command == "setInterval") { ... }
    // else if (command == "restart")     { ESP.restart(); }
    else {
        Serial.print(F("[CMD] Unknown command: "));
        Serial.println(command);
    }
}

// ====================================================================== //
//  Setup – ทำงานครั้งเดียวตอนเริ่มต้น
// ====================================================================== //

void setup() {
    // ---- เริ่มต้น Serial Monitor ----------------------------------- //
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("=== CathoaIOT – TwoWayCommunication Example ==="));

    // ---- ตั้งค่า LED pin เป็น OUTPUT ------------------------------ //
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // เริ่มต้นปิด LED

    // ---- ตั้งค่า MQTT broker (ก่อนเรียก begin()) ------------------- //
    iot.setMqttHost(MQTT_HOST);
    iot.setMqttPort(8883);               // TLS port
    iot.setMqttCredentials(MQTT_USER, MQTT_PASS);

    // ---- ลงทะเบียน Command Callback (ก่อนเรียก begin()) ------------ //
    // ต้องเรียกก่อน begin() เพื่อให้ callback พร้อมรับคำสั่ง
    // ทันทีที่ MQTT เชื่อมต่อและ subscribe สำเร็จ
    iot.setCommandCallback(handleCommand);

    // ---- เชื่อมต่อ WiFi -------------------------------------------- //
    Serial.print(F("Connecting to WiFi"));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println(F("\nWiFi Connected."));

    // ตั้งค่า insecure mode สำหรับ TLS (สำหรับการทดสอบ)
    netClient.setInsecure();

    // ---- เริ่มต้น MQTT --------------------------------------------- //
    // begin() จะ:
    //   1. ตั้งค่า internal MQTT callback
    //   2. เชื่อมต่อ MQTT broker
    //   3. Auto-subscribe ไปที่ v1/devices/{deviceId}/cmd
    iot.begin();
}

// ====================================================================== //
//  Loop – ทำงานซ้ำไปเรื่อยๆ
// ====================================================================== //

void loop() {
    // ---- ต้องเรียก iot.loop() ทุกรอบ ------------------------------- //
    // ทำหน้าที่:
    //   - ตรวจสอบ MQTT connection → auto-reconnect + re-subscribe
    //   - ประมวลผล incoming MQTT packets (รวมถึง command messages)
    iot.loop();

    // ---- Throttle: ส่ง telemetry ทุก 5 วินาที --------------------- //
    const unsigned long now = millis();
    if (now - lastSendMs < TELEMETRY_INTERVAL_MS) return;
    lastSendMs = now;

    // ---- สร้างข้อมูล sensor จำลอง --------------------------------- //
    //   Temperature: random float 20.0 – 35.0 °C
    const float temperature = 20.0f + (random(0, 1500) / 100.0f);

    // ---- ส่ง Telemetry ไปยัง Cloud -------------------------------- //
    // Publish ไปที่ topic: v1/devices/{deviceId}/telemetry
    // JSON: { "temperatureC": 25.3 }
    if (iot.sendTelemetry("temperatureC", temperature)) {
        Serial.print(F("✓ temperatureC = "));
        Serial.println(temperature);
    }
}
