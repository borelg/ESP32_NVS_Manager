#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

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

// --- Define your Schema here ---
Parameter schema[] = {
    {"led_pin", "LED GPIO Pin", "int", 0, 20},
    {"red",     "Red Channel",  "int", 0, 255},
    {"green",   "Green Channel", "int", 0, 255},
    {"blue",    "Blue Channel",  "int", 0, 255},
    {"freq",    "Blink Speed (ms)", "int", 10, 5000}
};
const int schema_count = sizeof(schema) / sizeof(Parameter);

Adafruit_NeoPixel* pixels = nullptr;
unsigned long lastMillis = 0;
bool ledState = false;

void initLED(int pin) {
    if (pixels) { pixels->clear(); pixels->show(); delete pixels; }
    pixels = new Adafruit_NeoPixel(1, pin, NEO_GRB + NEO_KHZ800);
    pixels->begin();
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
    initLED(prefs.getInt("led_pin", 8));
}

void loop() {
    // 1. Blink Logic
    int freq = prefs.getInt("freq", 500);
    if (millis() - lastMillis >= freq) {
        lastMillis = millis();
        ledState = !ledState;
        if (ledState && pixels) {
            pixels->setPixelColor(0, pixels->Color(prefs.getInt("red", 255), prefs.getInt("green", 0), prefs.getInt("blue", 0)));
        } else if (pixels) {
            pixels->setPixelColor(0, 0);
        }
        if (pixels) pixels->show();
    }

    // 2. Serial Logic
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        if (input == "GET_SCHEMA") {
            sendSchema();
        } 
        else if (input.startsWith("SET_VAR:")) {
            JsonDocument update;
            deserializeJson(update, input.substring(8));
            const char* key = update["key"];
            int val = update["val"];

            // Server-side Safety Check: Validate against schema before saving
            for (int i = 0; i < schema_count; i++) {
                if (strcmp(schema[i].key, key) == 0) {
                    if (val >= schema[i].min_val && val <= schema[i].max_val) {
                        prefs.putInt(key, val);
                        if (strcmp(key, "led_pin") == 0) initLED(val);
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