/**
 * @file BasicTelemetry.ino
 * @brief Minimal example: send mock temperature & humidity every 5 seconds
 *        using the CathoaIOT library.
 *
 * Wiring: No external sensors needed – values are simulated.
 *
 * Steps:
 *   1. Install the CathoaIOT library (copy to Arduino/libraries/).
 *   2. Set your WiFi credentials, MQTT credentials, and device ID below.
 *   3. Upload to an ESP32 board.
 *   4. Open Serial Monitor at 115200 baud to observe telemetry output.
 */

#include <CathoaIOT.h>

// ======================= User Configuration ========================== //

// WiFi credentials
static const char* WIFI_SSID     = "Wokwi-GUEST";       // ← change me
static const char* WIFI_PASSWORD = "";                    // ← change me

// Device ID – must match the device registered on your Cathoa Dashboard
static const char* DEVICE_ID     = "YOUR-DEVICE-UUID";   // ← change me

// MQTT broker settings
static const char* MQTT_HOST     = "f13e31dd434f4a099bfe1f13ec6e84a9.s1.eu.hivemq.cloud";
static constexpr uint16_t MQTT_PORT = 8883;
static const char* MQTT_USER     = "iot_user";            // ← change me
static const char* MQTT_PASS     = "YourPassword";        // ← change me
static const char* TOPIC_PREFIX  = "four/dashboardiot/telemetry";
static constexpr uint16_t BUFFER_SIZE = 512;

// ===================================================================== //

// Create the CathoaIOT instance
// Note: MQTT_USER, MQTT_PASS, and BUFFER_SIZE are optional and can be omitted if not needed.
CathoaIOT iot(
    WIFI_SSID, WIFI_PASSWORD, DEVICE_ID,
    MQTT_HOST, MQTT_PORT, TOPIC_PREFIX,
    MQTT_USER, MQTT_PASS, BUFFER_SIZE
);

// Publish interval
static constexpr unsigned long TELEMETRY_INTERVAL_MS = 5000;  // 5 seconds
static unsigned long lastSendMs = 0;

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("=== CathoaIOT – BasicTelemetry Example ==="));

    // Connect to WiFi & MQTT
    iot.begin();
}

void loop() {
    // MUST be called every loop iteration – handles reconnections & MQTT.
    iot.loop();

    // Throttle telemetry to once every TELEMETRY_INTERVAL_MS
    const unsigned long now = millis();
    if (now - lastSendMs < TELEMETRY_INTERVAL_MS) return;
    lastSendMs = now;

    // ---- Generate mock sensor data --------------------------------- //
    //   Temperature : random float 20.0 – 35.0 °C
    //   Humidity    : random float 40.0 – 90.0 %
    const float temperature = 20.0f + (random(0, 1500) / 100.0f);
    const float humidity    = 40.0f + (random(0, 5000) / 100.0f);

    // ---- Publish telemetry ----------------------------------------- //
    if (iot.sendTelemetry("temperatureC", temperature)) {
        Serial.print(F("✓ temperatureC = "));
        Serial.println(temperature);
    }

    if (iot.sendTelemetry("humidity", humidity)) {
        Serial.print(F("✓ humidity = "));
        Serial.println(humidity);
    }

    // You can also send string values:
    // iot.sendTelemetry("status", "OK");
}
