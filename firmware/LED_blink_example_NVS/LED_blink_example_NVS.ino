#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "nvs_flash.h"
#include "nvs.h"

// --- Global Objects ---
Preferences prefs;
const char* NVS_NAMESPACE = "config";

// --- Parameter Metadata Structure ---
struct Parameter {
    const char* key;
    const char* label;
    const char* type; // "int" or "str"
    int min_val;
    int max_val;
};

// --- Updated Schema: GPIO and Timeout only ---
Parameter schema[] = {
    {"led_pin", "LED GPIO Pin",      "int", 0,  48},   // Adjust max for your ESP32 model
    {"timeout", "Blink Timeout (ms)", "int", 50, 5000}
};
const int schema_count = sizeof(schema) / sizeof(Parameter);

// --- Runtime Variables ---
unsigned long lastMillis = 0;
bool ledState = false;

// Function to safely switch GPIO pins
void updateGPIOPin(int oldPin, int newPin) {
    pinMode(oldPin, INPUT);     // Reset old pin to high impedance
    pinMode(newPin, OUTPUT);    // Set new pin as output
}

void sendSchema() {
    JsonDocument doc;
    JsonArray params = doc.to<JsonArray>();
    
    for (int i = 0; i < schema_count; i++) {
        JsonObject p = params.add<JsonObject>();
        p["key"] = schema[i].key;
        p["label"] = schema[i].label;
        p["type"] = schema[i].type;
        p["min"] = schema[i].min_val;
        p["max"] = schema[i].max_val;
        
        // Fetch current value from NVS
        if (strcmp(schema[i].type, "int") == 0) {
            p["val"] = prefs.getInt(schema[i].key, schema[i].min_val);
        } else {
            p["val"] = prefs.getString(schema[i].key, "");
        }
    }
    serializeJson(doc, Serial);
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    prefs.begin(NVS_NAMESPACE, false);

    // Initial default values
    if (!prefs.isKey("led_pin")) prefs.putInt("led_pin", 8); // Default for C3 SuperMini LED
    if (!prefs.isKey("timeout")) prefs.putInt("timeout", 500);

    // Initialize the GPIO defined in NVS
    pinMode(prefs.getInt("led_pin", 8), OUTPUT);
}

void loop() {
    // 1. Blink Logic (Simple LED)
    int currentPin = prefs.getInt("led_pin", 8);
    int currentTimeout = prefs.getInt("timeout", 500);

    if (millis() - lastMillis >= currentTimeout) {
        lastMillis = millis();
        ledState = !ledState;
        digitalWrite(currentPin, ledState ? HIGH : LOW);
    }

    // 2. Serial Communication
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        
        if (input == "GET_SCHEMA") {
            sendSchema();
        } 
        else if (input.startsWith("SET_VAR:")) {
            JsonDocument update;
            DeserializationError error = deserializeJson(update, input.substring(8));
            
            if (!error) {
                const char* key = update["key"];
                int val = update["val"];

                // Server-side Safety Check
                for (int i = 0; i < schema_count; i++) {
                    if (strcmp(schema[i].key, key) == 0) {
                        if (val >= schema[i].min_val && val <= schema[i].max_val) {
                            
                            // If we are changing the pin, update the hardware mode
                            if (strcmp(key, "led_pin") == 0) {
                                int oldPin = prefs.getInt("led_pin", 8);
                                updateGPIOPin(oldPin, val);
                            }

                            prefs.putInt(key, val);
                            Serial.println("OK");
                        } else {
                            Serial.println("ERROR: Out of bounds");
                        }
                        break;
                    }
                }
            }
        }
    }
}