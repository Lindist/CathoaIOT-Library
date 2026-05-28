/**
 * @file AdvancedTelemetry_MultiValues.ino
 * @brief This example demonstrates how to send multiple telemetry values 
 *        at once using C++ initializer list for the CathoaIOT library.
 */
#include <CathoaIOT.h>

#if defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
#endif

// WiFi & Device Setup
static const char* WIFI_SSID     = "YourSSID";
static const char* WIFI_PASSWORD = "YourPassword";
static const char* DEVICE_ID     = "YOUR-DEVICE-UUID";

// MQTT broker settings
static const char* MQTT_HOST     = "broker.hivemq.com";
static constexpr uint16_t MQTT_PORT = 8883;

WiFiClientSecure netClient;

// Use the full constructor
CathoaIOT iot(
    netClient, DEVICE_ID,
    MQTT_HOST, MQTT_PORT, ""
);

static unsigned long lastSendMs = 0;

void setup() {
    Serial.begin(115200);
    Serial.println(F("\n=== CathoaIOT - Advanced Telemetry Multi-Values ==="));

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println(F("\nWiFi Connected."));

    netClient.setInsecure(); // For testing without certificates
    iot.begin();
}

void loop() {
    iot.loop();

    const unsigned long now = millis();
    if (now - lastSendMs >= 5000) {
        lastSendMs = now;
        
        float temp = 25.0f + random(0, 100)/10.0f;
        float hum = 50.0f + random(0, 100)/10.0f;
        bool motorStatus = random(0, 2) == 1;

        // Send multiple values at once using an initializer list!
        bool success = iot.sendTelemetry({
            {"temperature", String(temp)},
            {"humidity", String(hum)},
            {"motor_on", motorStatus ? "true" : "false"},
            {"status", "running"}
        });

        if (success) {
            Serial.println("Batch telemetry sent successfully!");
        } else {
            Serial.println("Failed to send batch telemetry.");
        }
    }
}
