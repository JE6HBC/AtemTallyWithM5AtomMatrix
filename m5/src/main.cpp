#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// WiFi Configuration
const char* ssid = "Atem_Py";
const char* password = "Tally33";

// MQTT Configuration
// IMPORTANT: Replace with the IP address of the computer running Docker and Mosquitto.
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;
const char* tally_state_topic = "atem/tally/state";
const char* call_topic = "companion/call";

// M5 Atom Matrix specific settings
#define EEPROM_SIZE 16
int myCameraID = 1;

// GPIO pins for external LEDs
#define RED_PIN 21
#define GREEN_PIN 22
#define YELLOW_PIN 23

// Global objects
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Function Prototypes
void setupWifi();
void reconnectMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setCameraIDMode();
void updateLED(int state);

void setup() {
    M5.begin();

    // Initialize GPIO for external LEDs
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(YELLOW_PIN, OUTPUT);
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);

    EEPROM.begin(EEPROM_SIZE);

    // Check if the button is pressed on boot to enter config mode
    M5.update();
    if (M5.Btn.wasPressed()) {
        setCameraIDMode();
    }

    // Load Camera ID from EEPROM
    myCameraID = EEPROM.read(0);
    if (myCameraID == 0 || myCameraID > 10) {
        myCameraID = 1; // Default to 1 if not set or invalid
    }

    setupWifi();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
}

void loop() {
    M5.update();
    if (!mqttClient.connected()) {
        reconnectMqtt();
    }
    mqttClient.loop();

    // Long press to enter config mode
    if (M5.Btn.wasDecided()) {
         setCameraIDMode();
    }
}

void setupWifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        M5.dis.drawpix(0, 0, 0x0000FF); // Blue while connecting
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    M5.dis.drawpix(0, 0, 0x000000); // Clear LED
}

void reconnectMqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "M5StackClient-";
        clientId += String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("connected");
            mqttClient.subscribe(tally_state_topic);
            mqttClient.subscribe(call_topic);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            M5.dis.drawpix(0, 0, 0xFF00FF); // Purple for MQTT error
            delay(5000);
        }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    char message[length + 1];
    for (int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload, length);

    if (strcmp(topic, tally_state_topic) == 0) {
        String myKey = String(myCameraID);
        int state = doc[myKey]; // 0: OFF, 1: PVW, 2: PGM
        updateLED(state);
    } else if (strcmp(topic, call_topic) == 0) {
        int cam = doc["cam"];
        if (cam == myCameraID) {
            String state = doc["state"];
            if (state == "ON") {
                updateLED(3); // 3: CALL
            } else {
                // Revert to tally state by requesting a new one (or just waiting)
                // For now, let's just turn it off, the next tally update will fix it.
                updateLED(0);
            }
        }
    }
}

void setCameraIDMode() {
    int tempID = myCameraID;
    M5.dis.clear();
    M5.dis.drawpix(0, 0, 0xFFFFFF); // White for config mode
    delay(500);

    while (true) {
        M5.update();
        if (M5.Btn.wasPressed()) {
            tempID++;
            if (tempID > 10) tempID = 1;
            // Display number on matrix (simple version)
            M5.dis.clear();
            for(int i=0; i<tempID; i++){
              M5.dis.drawpix(i%5, i/5, 0xFFFFFF);
            }
        }
        if (M5.Btn.wasHold()) {
            myCameraID = tempID;
            EEPROM.write(0, myCameraID);
            EEPROM.commit();
            M5.dis.drawpix(0, 0, 0x00FF00); // Green for saved
            delay(1000);
            ESP.restart();
        }
    }
}

void updateLED(int state) {
    // 0: OFF, 1: PVW (Green), 2: PGM (Red), 3: CALL (Yellow)
    CRGB color = 0x000000;

    // Reset all GPIOs first
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);

    switch (state) {
        case 1: // PVW
            color = 0x00FF00; // Green
            digitalWrite(GREEN_PIN, HIGH);
            break;
        case 2: // PGM
            color = 0xFF0000; // Red
            digitalWrite(RED_PIN, HIGH);
            break;
        case 3: // CALL
            color = 0xFFFF00; // Yellow
            digitalWrite(YELLOW_PIN, HIGH);
            break;
        default: // OFF
            color = 0x000000; // Off
            break;
    }
    M5.dis.fillpix(color);
}
