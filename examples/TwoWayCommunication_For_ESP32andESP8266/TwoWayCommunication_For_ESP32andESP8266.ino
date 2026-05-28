/**
 * @file TwoWayCommunication.ino
 * @brief Two-Way Communication Example with CathoaIOT Library
 *
 * Demonstrates:
 *   ► Sending telemetry data every 5 seconds using sendTelemetry()
 *   ► Receiving commands from Cloud using setCommandCallback()
 *     e.g., {"command": "toggleLight", "status": "ON"}
 *     to turn on/off the built-in LED on Pin 2 of the ESP32.
 *
 * MQTT Topics Used:
 *   - Publish:   v1/devices/{deviceId}/telemetry
 *   - Subscribe: v1/devices/{deviceId}/cmd
 *
 * Instructions:
 *   1. Install the CathoaIOT library.
 *   2. Configure your WiFi credentials, MQTT credentials, and Device ID below.
 *   3. Upload to an ESP32 board.
 *   4. Open Serial Monitor at 115200 baud.
 *   5. Send a command JSON to topic v1/devices/{DEVICE_ID}/cmd
 *      e.g.: {"command": "toggleLight", "status": "ON"}
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

// WiFi credentials - Change these to your network settings
static const char* WIFI_SSID     = "YourSSID";           // ← Change WiFi SSID
static const char* WIFI_PASSWORD = "YourPassword";       // ← Change WiFi password

// Device ID - Must match the device registered on your Cathoa Dashboard
static const char* DEVICE_ID     = "YOUR-DEVICE-UUID";   // ← Change Device ID

// MQTT broker settings
static const char* MQTT_HOST     = "broker.hivemq.com";  // ← Change MQTT Host
static const char* MQTT_USER     = "YourUsername";       // ← Change MQTT username
static const char* MQTT_PASS     = "YourPassword";       // ← Change MQTT password

// ===================================================================== //

// Built-in LED Pin on ESP32 (Usually Pin 2)
static constexpr int LED_PIN = 2;

// Pins for LED 2 and Button
static constexpr int LED2_PIN = 23;
static constexpr int BUTTON_PIN = 22;

// Pin for LED 3
static constexpr int LED3_PIN = 21;

// Pin for LED 4 (Gas command)
static constexpr int LED4_PIN = 19;

// Analog Pin (ADC) for Potentiometer
static constexpr int POT_PIN = 34;

// Store state of LED 2 (used for both button and command handling)
bool led2State = false;

// Store state of LED 3
bool led3State = false;

// Create Network Client (Secure for ESP32)
WiFiClientSecure netClient;

// ---- Create CathoaIOT instance using Client Injection ----------- //
// Pass netClient and deviceId to the library,
// then use setter methods to configure MQTT later.
CathoaIOT iot(netClient, DEVICE_ID);

// Publish interval - Send telemetry every 5 seconds
static constexpr unsigned long TELEMETRY_INTERVAL_MS = 5000;
static unsigned long lastSendMs = 0;

// ====================================================================== //
//  Command Callback Function
//  - Function called when a command arrives from the Cloud
// ====================================================================== //

/**
 * @brief Callback function to handle incoming MQTT commands
 *
 * When the Cloud sends a JSON to topic v1/devices/{deviceId}/cmd
 * e.g.: {"command": "toggleLight", "status": "ON"}
 *
 * The library parses the JSON and calls this function with:
 *   - command = "toggleLight"
 *   - payload = "ON"
 *
 * @param command  Command name (key "command" in JSON)
 * @param payload  Command value (key "status" or "payload" in JSON)
 */
void handleCommand(String command, String payload) {
    Serial.println(F("========================================"));
    Serial.print(F("  Command received: "));
    Serial.println(command);
    Serial.print(F("  Payload: "));
    Serial.println(payload);
    Serial.println(F("========================================"));

    // Check command name (ensure Publish Topic in Dashboard matches this)
    if (command == "toggleLight") {
        
        // Convert payload to uppercase for easier comparison
        payload.toUpperCase();

        // Check if command is to turn ON (supports multiple formats)
        if (payload == "ON" || payload == "TRUE" || payload == "1") {
            digitalWrite(LED_PIN, HIGH);
            Serial.println(F("[LED] Turned ON"));
            
            // ⭐ Echo status back to Dashboard to update UI button state
            iot.sendTelemetry(command, String("ON"));
            
        } 
        // Check if command is to turn OFF
        else if (payload == "OFF" || payload == "FALSE" || payload == "0") {
            digitalWrite(LED_PIN, LOW);
            Serial.println(F("[LED] Turned OFF"));
            
            // ⭐ Echo status back to Dashboard to update UI button state
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
        
        // Convert string payload to integer
        int gasValue = payload.toInt();
        
        if (gasValue > 80) {
            digitalWrite(LED4_PIN, HIGH);
            Serial.print(F("[LED4] Gas value > 80 ("));
            Serial.print(gasValue);
            Serial.println(F(") -> LED4 ON"));
            iot.sendTelemetry(command, float(gasValue)); // Echo confirmation
        } else {
            digitalWrite(LED4_PIN, LOW);
            Serial.print(F("[LED4] Gas value <= 80 ("));
            Serial.print(gasValue);
            Serial.println(F(") -> LED4 OFF"));
            iot.sendTelemetry(command, float(gasValue)); // Echo confirmation
        }
        
    } else {
        Serial.print(F("[CMD] Unknown command: "));
        Serial.println(command);
    }
}

// ====================================================================== //
//  Setup - Run once at startup
// ====================================================================== //

void setup() {
    // ---- Initialize Serial Monitor --------------------------------- //
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("=== CathoaIOT - TwoWayCommunication Example ==="));

    // ---- Configure LED pins as OUTPUT ----------------------------- //
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // Start with LED OFF

    pinMode(LED2_PIN, OUTPUT);
    digitalWrite(LED2_PIN, LOW); // Start with LED 2 OFF

    pinMode(LED3_PIN, OUTPUT);
    digitalWrite(LED3_PIN, LOW); // Start with LED 3 OFF

    pinMode(LED4_PIN, OUTPUT);
    digitalWrite(LED4_PIN, LOW); // Start with LED 4 OFF

    // Configure Analog Pin for Potentiometer reading
    pinMode(POT_PIN, INPUT);

    // ---- Configure Button pin as INPUT_PULLUP ----------------------- //
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // ---- Configure MQTT broker (Before calling begin()) ------------- //
    iot.setMqttHost(MQTT_HOST);
    iot.setMqttPort(8883);               // TLS port
    iot.setMqttCredentials(MQTT_USER, MQTT_PASS);

    // ---- Register Command Callback (Before calling begin()) --------- //
    // Must be called before begin() so callback is ready to receive commands
    // as soon as MQTT connects and subscribes successfully.
    iot.setCommandCallback(handleCommand);

    // ---- Connect to WiFi -------------------------------------------- //
    Serial.print(F("Connecting to WiFi"));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println(F("\nWiFi Connected."));

    // Set insecure mode for TLS (for testing purposes)
    netClient.setInsecure();

    // ---- Initialize MQTT ------------------------------------------- //
    // begin() will:
    //   1. Setup internal MQTT callback
    //   2. Connect to MQTT broker
    //   3. Auto-subscribe to v1/devices/{deviceId}/cmd
    iot.begin();
}

// ====================================================================== //
//  Loop - Runs continuously
// ====================================================================== //

void loop() {
    // ---- MUST call iot.loop() every iteration ----------------------- //
    // Responsible for:
    //   - Checking MQTT connection -> auto-reconnect + re-subscribe
    //   - Processing incoming MQTT packets (including command messages)
    iot.loop();

    // ---- Button Debounce Logic -------------------------------------- //
    static int lastButtonState = HIGH;
    static int buttonState = HIGH;
    static unsigned long lastDebounceTime = 0;
    static constexpr unsigned long DEBOUNCE_DELAY = 50;

    int reading = digitalRead(BUTTON_PIN);

    // If button state changed (pressed or released), reset timer
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }

    // If time elapsed exceeds debounce delay, state is stable
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != buttonState) {
            buttonState = reading;

            // Trigger action on press (LOW because INPUT_PULLUP is used)
            if (buttonState == LOW) {
                // Toggle LED 2 state
                led2State = !led2State;
                digitalWrite(LED2_PIN, led2State ? HIGH : LOW);
                
                Serial.print(F("[BUTTON] Pressed -> LED2 is now "));
                Serial.println(led2State ? F("ON") : F("OFF"));
                
                // ⭐ Send latest state to Dashboard immediately
                // String() cast is needed so C++ doesn't evaluate "ON" as bool(true)
                iot.sendTelemetry("led2_switch", led2State ? String("ON") : String("OFF"));
            }
        }
    }

    lastButtonState = reading;

    // ---- Read Potentiometer (Variable Resistor) --------------------- //
    // Reading value range: 0 - 4095
    int lightLevel = analogRead(POT_PIN);

    // ---- LED 3 Control (Send boolean state at extremes) ------------- //
    static bool isAtExtreme = false;

    if (lightLevel >= 4000) {
        // Turned all the way right (max value) -> Keep LED ON + send ON state
        if (!isAtExtreme || led3State != true) {
            isAtExtreme = true;
            led3State = true;
            digitalWrite(LED3_PIN, HIGH);
            Serial.println(F("[LED3] Potentiometer MAX -> LED3 ON"));
        }
    } else if (lightLevel <= 100) {
        // Turned all the way left (min value) -> Keep LED OFF + send OFF state
        if (!isAtExtreme || led3State != false) {
            isAtExtreme = true;
            led3State = false;
            digitalWrite(LED3_PIN, LOW);
            Serial.println(F("[LED3] Potentiometer MIN -> LED3 OFF"));
        }
    }

    // ---- Throttle: Send telemetry every 5 seconds ------------------- //
    const unsigned long now = millis();
    if (now - lastSendMs < TELEMETRY_INTERVAL_MS) return;
    lastSendMs = now;

    // ---- Generate mock sensor data (except light which is real) ----- //
    const float temperature = 20.0f + (random(0, 1500) / 100.0f);
    const float humidity = 40.0f + (random(0, 2000) / 100.0f);

    // ---- Send Multiple Telemetry (C++ Initializer List) ------------- //
    // Send data as batch using { {"key", "value"}, {"key", "value"} } format
    if (iot.sendTelemetry({
        {"temperatureC", String(temperature)},
        {"humidity", String(humidity)},
        {"light", String(lightLevel)}
    })) {
        Serial.println(F("✓ Sent Multiple Telemetry successfully!"));
    }
}