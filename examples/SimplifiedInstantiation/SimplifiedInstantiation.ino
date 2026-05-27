/**
 * @file SimplifiedInstantiation.ino
 * @brief This example shows how to initialize the CathoaIOT library 
 *        step-by-step using separate setup functions instead of a large constructor.
 */
#include <CathoaIOT.h>

#if defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
#endif

static const char* WIFI_SSID     = "YourSSID";
static const char* WIFI_PASSWORD = "YourPassword";
static const char* DEVICE_ID     = "YOUR-DEVICE-UUID";

WiFiClientSecure netClient;

// Simplified Constructor - Only requires netClient and DeviceID
CathoaIOT iot(netClient, DEVICE_ID);

void setup() {
    Serial.begin(115200);
    Serial.println(F("\n=== CathoaIOT - Simplified Instantiation ==="));

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println(F("\nWiFi Connected."));

    netClient.setInsecure();

    // -------------------------------------------------------------
    // Step-by-step configuration using setter methods
    // -------------------------------------------------------------
    iot.setMqttHost("broker.hivemq.com");
    iot.setMqttPort(8883);
    
    // Set credentials if required by your broker
    iot.setMqttCredentials("myUsername", "myPassword");
    
    // Custom topic prefix (default is "v1/devices")
    iot.setTelemetryTopicPrefix("custom/topic/prefix");
    
    // Increase buffer size if expecting large JSON commands from Dashboard
    iot.setBufferSize(1024); 

    // Finally connect!
    iot.begin();
}

void loop() {
    iot.loop();
    // Do your work here...
}
