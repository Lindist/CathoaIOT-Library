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
static const char* WIFI_SSID     = "STeP._5GHz";           // ← เปลี่ยน WiFi SSID
static const char* WIFI_PASSWORD = "";                    // ← เปลี่ยน WiFi password

// Device ID – ต้องตรงกับที่ลงทะเบียนบน Cathoa Dashboard
static const char* DEVICE_ID     = "d0020718-3442-4680-9319-1f3af3784173";   // ← เปลี่ยน Device ID

// MQTT broker settings
static const char* MQTT_HOST     = "f13e31dd434f4a099bfe1f13ec6e84a9.s1.eu.hivemq.cloud";
static const char* MQTT_USER     = "iot_user";            // ← เปลี่ยน MQTT username
static const char* MQTT_PASS     = "Iamlnw1992";        // ← เปลี่ยน MQTT password

// ===================================================================== //

// Pin ของ Built-in LED บน ESP32 (ปกติคือ Pin 2)
static constexpr int LED_PIN = 2;

// Pin สำหรับ LED ตัวที่ 2 และปุ่มกด
static constexpr int LED2_PIN = 23;
static constexpr int BUTTON_PIN = 22;

// Pin สำหรับ LED ตัวที่ 3
static constexpr int LED3_PIN = 21;

// Pin สำหรับ LED ตัวที่ 4 (รับคำสั่ง Gas)
static constexpr int LED4_PIN = 19;

// Pin สำหรับ Potentiometer (ตัวต้านทานปรับค่าได้) ต้องใช้ขา Analog (ADC)
static constexpr int POT_PIN = 34;

// เก็บสถานะของ LED ตัวที่ 2 (เพื่อใช้ทั้งในปุ่มกดและรับคำสั่ง)
bool led2State = false;

// เก็บสถานะของ LED ตัวที่ 3
bool led3State = false;

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

    // ตรวจสอบชื่อคำสั่ง (ตั้งค่า Publish Topic ใน Dashboard ให้ตรงกับอันนี้)
    // ตัวอย่าง: ถ้าใน Dashboard ตั้ง Publish Topic เป็น led_switch 
    // ตัวแปร command ก็จะมีค่าเป็น "led_switch"
    if (command == "toggleLight") {
        
        // แปลง payload ให้เป็นตัวพิมพ์ใหญ่ทั้งหมดเพื่อเช็คง่ายขึ้น
        payload.toUpperCase();

        // เช็คว่าคำสั่งคือให้เปิดหรือไม่ (รองรับหลายรูปแบบ)
        if (payload == "ON" || payload == "TRUE" || payload == "1") {
            digitalWrite(LED_PIN, HIGH);
            Serial.println(F("[LED] Turned ON"));
            
            // ⭐ ส่งข้อมูลกลับไปยืนยันที่ Dashboard เพื่อให้ปุ่มอัปเดตสถานะเป็นเปิด
            // (เราต้องใช้คำสั่งเดียวกันเป็น Telemetry Key)
            iot.sendTelemetry(command, String("ON"));
            
        } 
        // เช็คว่าคำสั่งคือให้ปิดหรือไม่
        else if (payload == "OFF" || payload == "FALSE" || payload == "0") {
            digitalWrite(LED_PIN, LOW);
            Serial.println(F("[LED] Turned OFF"));
            
            // ⭐ ส่งข้อมูลกลับไปยืนยันที่ Dashboard เพื่อให้ปุ่มอัปเดตสถานะเป็นปิด
            iot.sendTelemetry(command, String("OFF"));
            
        } else {
            Serial.print(F("[LED] Unknown payload: "));
            Serial.println(payload);
        }
    } else if (command == "led2_switch") {
        
        payload.toUpperCase();

        if (payload == "ON" || payload == "TRUE" || payload == "1") {
            led2State = true;
            digitalWrite(LED2_PIN, HIGH);
            Serial.println(F("[LED2] Turned ON by Dashboard"));
            iot.sendTelemetry(command, String("ON"));
            
        } else if (payload == "OFF" || payload == "FALSE" || payload == "0") {
            led2State = false;
            digitalWrite(LED2_PIN, LOW);
            Serial.println(F("[LED2] Turned OFF by Dashboard"));
            iot.sendTelemetry(command, String("OFF"));
            
        } else {
            Serial.print(F("[LED2] Unknown payload: "));
            Serial.println(payload);
        }
    } else if (command == "gas") {
        
        // แปลง payload (ที่เป็น String) ให้กลายเป็นตัวเลข (Integer)
        int gasValue = payload.toInt();
        
        if (gasValue > 80) {
            digitalWrite(LED4_PIN, HIGH);
            Serial.print(F("[LED4] Gas value > 80 ("));
            Serial.print(gasValue);
            Serial.println(F(") -> LED4 ON"));
            iot.sendTelemetry(command, float(gasValue)); // ส่งยืนยัน
        } else {
            digitalWrite(LED4_PIN, LOW);
            Serial.print(F("[LED4] Gas value <= 80 ("));
            Serial.print(gasValue);
            Serial.println(F(") -> LED4 OFF"));
            iot.sendTelemetry(command, float(gasValue)); // ส่งยืนยัน
        }
        
    } else {
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

    pinMode(LED2_PIN, OUTPUT);
    digitalWrite(LED2_PIN, LOW); // เริ่มต้นปิด LED 2

    pinMode(LED3_PIN, OUTPUT);
    digitalWrite(LED3_PIN, LOW); // เริ่มต้นปิด LED 3

    pinMode(LED4_PIN, OUTPUT);
    digitalWrite(LED4_PIN, LOW); // เริ่มต้นปิด LED 4

    // ตั้งค่า Analog Pin สำหรับอ่านค่า Potentiometer
    pinMode(POT_PIN, INPUT);

    // ---- ตั้งค่า Button pin เป็น INPUT_PULLUP ----------------------- //
    pinMode(BUTTON_PIN, INPUT_PULLUP);

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

    // ---- ตรวจสอบการกดปุ่ม (Debounce) ------------------------------ //
    static int lastButtonState = HIGH;
    static int buttonState = HIGH;
    static unsigned long lastDebounceTime = 0;
    static constexpr unsigned long DEBOUNCE_DELAY = 50;

    int reading = digitalRead(BUTTON_PIN);

    // ถ้าย้ายสถานะของปุ่ม (กด หรือ ปล่อย) ให้เริ่มจับเวลาใหม่
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    // ถ้าเวลาผ่านไปเกิน delay แล้ว แปลว่าไม่ใช่สัญญาณรบกวน
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        // ถ้าสถานะปุ่มเปลี่ยนไปจากเดิม
        if (reading != buttonState) {
            buttonState = reading;

            // สนใจเฉพาะตอนกดปุ่ม (ค่าเป็น LOW เพราะใช้ INPUT_PULLUP)
            if (buttonState == LOW) {
                // สลับสถานะ LED 2
                led2State = !led2State;
                digitalWrite(LED2_PIN, led2State ? HIGH : LOW);
                
                Serial.print(F("[BUTTON] Pressed -> LED2 is now "));
                Serial.println(led2State ? F("ON") : F("OFF"));
                
                // ⭐ ส่งสถานะล่าสุดไปยัง Dashboard ทันที
                // ต้องใช้ String() ครอบเพื่อไม่ให้ C++ เผลอแปลง "ON" เป็นค่า bool(true)
                iot.sendTelemetry("led2_switch", led2State ? String("ON") : String("OFF"));
            }
        }
    }

    lastButtonState = reading;

    // ---- อ่านค่าตัวต้านทานปรับค่าได้ (Potentiometer) ---------------- //
    // ค่าที่อ่านได้จะอยู่ในช่วง 0 - 4095
    int lightLevel = analogRead(POT_PIN);

    // ---- สร้างการควบคุม LED 3 (ส่งค่า Boolean เมื่อหมุนสุด) --------- //
    static bool isAtExtreme = false;
    static unsigned long lastLed3BlinkMs = 0;
    static constexpr unsigned long LED3_BLINK_INTERVAL_MS = 2000; 

    unsigned long currentMs = millis();

    if (lightLevel >= 4000) {
        // หมุนสุดขวา (ค่าสูงสุด) -> ไฟเปิดค้าง + ส่งสถานะ ON
        if (!isAtExtreme || led3State != true) {
            isAtExtreme = true;
            led3State = true;
            digitalWrite(LED3_PIN, HIGH);
            Serial.println(F("[LED3] Potentiometer MAX -> LED3 ON"));
        }
    } else if (lightLevel <= 100) {
        // หมุนสุดซ้าย (ค่าต่ำสุด) -> ไฟปิดค้าง + ส่งสถานะ OFF
        if (!isAtExtreme || led3State != false) {
            isAtExtreme = true;
            led3State = false;
            digitalWrite(LED3_PIN, LOW);
            Serial.println(F("[LED3] Potentiometer MIN -> LED3 OFF"));
        }
    }

    // ---- Throttle: ส่ง telemetry ทุก 5 วินาที --------------------- //
    const unsigned long now = millis();
    if (now - lastSendMs < TELEMETRY_INTERVAL_MS) return;
    lastSendMs = now;

    // ---- สร้างข้อมูล sensor จำลอง (ยกเว้น light ที่อ่านจากของจริง) -- //
    const float temperature = 20.0f + (random(0, 1500) / 100.0f);
    const float humidity = 40.0f + (random(0, 2000) / 100.0f);
    // ไม่ใช้ random lightLevel แล้ว เพราะอ่านค่าจาก POT_PIN มาแล้วด้านบน

    // ---- ส่ง Telemetry แบบหลายค่าพร้อมกัน (C++ Initializer List) ---- //
    // ส่งข้อมูลเป็นกลุ่ม โดยใช้รูปแบบ { {"key", "value"}, {"key", "value"} }
    if (iot.sendTelemetry({
        {"temperatureC", String(temperature)},
        {"humidity", String(humidity)},
        {"light", String(lightLevel)}
    })) {
        Serial.println(F("✓ Sent Multiple Telemetry successfully!"));
    }
}