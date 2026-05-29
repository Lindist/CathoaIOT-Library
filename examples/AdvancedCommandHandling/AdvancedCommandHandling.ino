/**
 * @file AdvancedCommandHandling.ino
 * @brief This example shows how to handle various commands from the dashboard,
 *        including turning on/off a LED, setting PWM, or handling custom strings.
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
static const char* MQTT_USER     = "YourMqttUsername";
static const char* MQTT_PASS     = "YourMqttPassword";

WiFiClientSecure netClient;

// We use the simplified constructor here, setting details in setup()
CathoaIOT iot(netClient, DEVICE_ID);

const int LED_PIN = 2; // Built-in LED on most ESP32/ESP8266 boards

// Command Callback Function
void onCommandReceived(String command, String payload) {
    Serial.print("Command Received: ");
    Serial.print(command);
    Serial.print(" | Payload: ");
    Serial.println(payload);

    if (command == "toggle_light") {
        if (payload == "ON" || payload == "1" || payload == "true") {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("Action: Light turned ON");
            // Optionally echo the status back to the dashboard
            iot.sendTelemetry({
              {"light_status", "ON"},
              {command, payload}
            });
        } else if (payload == "OFF" || payload == "0" || payload == "false") {
            digitalWrite(LED_PIN, LOW);
            Serial.println("Action: Light turned OFF");
            iot.sendTelemetry({
              {"light_status", "OFF"},
              {command, payload}
            });
        }
    } 
    else if (command == "set_speed") {
        int speed = payload.toInt();
        Serial.print("Action: Setting motor speed to ");
        Serial.println(speed);
        // e.g. analogWrite(MOTOR_PIN, speed);
    }
    else {
        Serial.println("Warning: Unknown command.");
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.println(F("\n=== CathoaIOT - Advanced Command Handling ==="));

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println(F("\nWiFi Connected."));

    netClient.setInsecure();
    
    // Set custom settings step-by-step
    iot.setMqttHost("broker.hivemq.com");
    iot.setMqttPort(8883);
    
    // Register the command callback
    iot.setCommandCallback(onCommandReceived);
    
    iot.setMqttCredentials(MQTT_USER, MQTT_PASS);
    
    iot.begin();
}

void loop() {
    // Keep connection alive and process incoming commands
    iot.loop();
}
