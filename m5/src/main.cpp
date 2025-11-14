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
int myRotation = 0;

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
void updateLED(int state);
void runTallyMode();
void runSettingMode();
void displayNumberOnMatrix(int number);


void setup() {
    M5.begin();
    M5.update(); // Update button state

    if (M5.BtnA.isPressed()) {
        // Enter setting mode if button is pressed on boot
        runSettingMode();
    } else {
        // Normal tally mode
        runTallyMode();
    }
}

void loop() {
    M5.update();
    if (!mqttClient.connected()) {
        reconnectMqtt();
    }
    mqttClient.loop();
}

void runTallyMode() {
    // Initialize GPIO for external LEDs
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(YELLOW_PIN, OUTPUT);
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);

    // Initialize EEPROM and load settings
    EEPROM.begin(EEPROM_SIZE);
    myCameraID = EEPROM.read(0);
    myRotation = EEPROM.read(1);

    // Validate settings
    if (myCameraID < 1 || myCameraID > 10) {
        myCameraID = 1; // Default to 1
    }
    if (myRotation > 3) {
        myRotation = 0; // Default to 0
    }

    // Apply screen rotation
    M5.Display.setRotation(myRotation);

    // Connect to WiFi and MQTT
    setupWifi();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);

    // The rest of the tally logic will be handled by loop()
}

void runSettingMode() {
    EEPROM.begin(EEPROM_SIZE);
    int tempCamID = EEPROM.read(0);
    int tempRotation = EEPROM.read(1);

    // Validate settings
    if (tempCamID < 1 || tempCamID > 10) {
        tempCamID = 1;
    }
    if (tempRotation > 3) {
        tempRotation = 0;
    }

    while (true) {
        M5.update();
        M5.Display.setRotation(tempRotation);
        displayNumberOnMatrix(tempCamID);

        if (M5.BtnA.wasClicked()) {
            // Increment Camera ID (1-10, wraps around)
            tempCamID++;
            if (tempCamID > 10) {
                tempCamID = 1;
            }
        } else if (M5.BtnA.wasDoubleClicked()) {
            // Increment Rotation (0-3, wraps around)
            tempRotation++;
            if (tempRotation > 3) {
                tempRotation = 0;
            }
        } else if (M5.BtnA.pressedFor(2000)) {
            // Save to EEPROM and restart
            EEPROM.write(0, tempCamID);
            EEPROM.write(1, tempRotation);
            EEPROM.commit();

            // Flash green to confirm save
            M5.Display.fillScreen(0x00FF00); // Green
            delay(500);

            ESP.restart();
        }
    }
}

void displayNumberOnMatrix(int number) {
    M5.Display.clear();
    if (number < 1 || number > 10) return; // Basic validation

    // Simple representation: light up 'number' of LEDs
    for (int i = 0; i < number; ++i) {
        // Arrange LEDs in a 5x5 grid fashion
        int x = i % 5;
        int y = i / 5;
        M5.Display.drawPixel(x, y, 0xFFFFFF); // White
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
        M5.Display.drawPixel(0, 0, 0x0000FF); // Blue while connecting
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    M5.Display.drawPixel(0, 0, 0x000000); // Clear LED
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
            M5.Display.drawPixel(0, 0, 0xFF00FF); // Purple for MQTT error
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

void updateLED(int state) {
    // 0: OFF, 1: PVW (Green), 2: PGM (Red), 3: CALL (Yellow)
    uint32_t color = 0x000000;

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
    M5.Display.fillScreen(color);
}
