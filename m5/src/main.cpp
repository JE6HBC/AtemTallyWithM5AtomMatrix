#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#define FW_VERSION "1.1.0"

// Tally State Definitions
enum TallyState {
    STATE_NONE,   // For override logic
    STATE_OFF,
    STATE_PVW,
    STATE_PGM,
    STATE_CALL
};

// Configuration Structure
struct Config {
    char ssid[32];
    char password[64];
    char mqtt_broker[64];
    int cameraID;
    int rotation;
    bool dhcp;
    char static_ip[16];
    char subnet[16];
    char gateway[16];
};

Config config;

// MQTT Configuration
const int mqtt_port = 1883;
const char* tally_state_topic = "atem/tally/state";
const char* call_topic = "companion/call";

#define EEPROM_SIZE 512

// GPIO pins for external LEDs
#define RED_PIN 21
#define GREEN_PIN 22
#define YELLOW_PIN 23

// Global objects
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Global state variables
TallyState currentTallyState = STATE_OFF;
volatile TallyState manualOverrideState = STATE_NONE;
String serialBuffer = "";

// Function Prototypes
void loadConfiguration();
void saveConfiguration();
void setupWifi();
void reconnectMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void updateLED(TallyState state);
void runTallyMode();
void runSettingMode();
void displayNumberOnMatrix(int number);
void handleSerial();
void parseCommand(String command);

void setup() {
    M5.begin();
    Serial.begin(115200);
    EEPROM.begin(EEPROM_SIZE);

    M5.update();

    if (M5.BtnA.isPressed()) {
        runSettingMode();
    } else {
        runTallyMode();
    }
}

void loop() {
    M5.update();
    handleSerial(); // Check for serial commands

    if (!mqttClient.connected()) {
        reconnectMqtt();
    }
    mqttClient.loop();

    // LED update logic: manual override takes precedence
    if (manualOverrideState != STATE_NONE) {
        updateLED(manualOverrideState);
    } else {
        updateLED(currentTallyState);
    }
}

void loadConfiguration() {
    EEPROM.get(0, config);
    // Simple validation: if cameraID is invalid, assume EEPROM is empty/corrupt
    if (config.cameraID < 1 || config.cameraID > 10) {
        // Load default values
        strcpy(config.ssid, "Atem_Py");
        strcpy(config.password, "Tally33");
        strcpy(config.mqtt_broker, "192.168.1.100");
        config.cameraID = 1;
        config.rotation = 0;
        config.dhcp = true;
        strcpy(config.static_ip, "192.168.1.101");
        strcpy(config.subnet, "255.255.255.0");
        strcpy(config.gateway, "192.168.1.1");
        saveConfiguration(); // Save defaults
    }
}

void saveConfiguration() {
    EEPROM.put(0, config);
    EEPROM.commit();
}

void runTallyMode() {
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(YELLOW_PIN, OUTPUT);
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);

    loadConfiguration();

    M5.Display.setRotation(config.rotation);

    setupWifi();
    mqttClient.setServer(config.mqtt_broker, mqtt_port);
    mqttClient.setCallback(mqttCallback);
}

void runSettingMode() {
    int tempCamID = config.cameraID;
    int tempRotation = config.rotation;

    while (true) {
        M5.update();
        M5.Display.setRotation(tempRotation);
        displayNumberOnMatrix(tempCamID);

        if (M5.BtnA.wasClicked()) {
            tempCamID++;
            if (tempCamID > 10) {
                tempCamID = 1;
            }
        } else if (M5.BtnA.wasDoubleClicked()) {
            tempRotation++;
            if (tempRotation > 3) {
                tempRotation = 0;
            }
        } else if (M5.BtnA.pressedFor(2000)) {
            config.cameraID = tempCamID;
            config.rotation = tempRotation;
            saveConfiguration();

            M5.Display.fillScreen(0x00FF00); // Green
            delay(500);
            ESP.restart();
        }
    }
}

void displayNumberOnMatrix(int number) {
    M5.Display.clear();
    if (number < 1 || number > 10) return;

    for (int i = 0; i < number; ++i) {
        int x = i % 5;
        int y = i / 5;
        M5.Display.drawPixel(x, y, 0xFFFFFF);
    }
}

void setupWifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(config.ssid);

    if (!config.dhcp) {
        IPAddress staticIP, gateway, subnet;
        staticIP.fromString(config.static_ip);
        gateway.fromString(config.gateway);
        subnet.fromString(config.subnet);
        WiFi.config(staticIP, gateway, subnet);
    }

    WiFi.begin(config.ssid, config.password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        M5.Display.drawPixel(0, 0, 0x0000FF);
        attempts++;
    }

    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFailed to connect to WiFi.");
        M5.Display.fillScreen(0xFF0000); // Red for error
        delay(2000);
        // Maybe reboot or enter a safe mode
    } else {
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        M5.Display.drawPixel(0, 0, 0x000000);
    }
}


void reconnectMqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection to ");
        Serial.print(config.mqtt_broker);
        Serial.print("...");

        String clientId = "M5-Tally-";
        clientId += String(random(0xffff), HEX);

        if (mqttClient.connect(clientId.c_str())) {
            Serial.println(" connected");
            mqttClient.subscribe(tally_state_topic);
            mqttClient.subscribe(call_topic);
        } else {
            Serial.print(" failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            M5.Display.drawPixel(0, 0, 0xFF00FF);
            delay(5000);
        }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // A new message arrived, clear any manual override
    manualOverrideState = STATE_NONE;

    // Create a mutable payload buffer
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println(message);


    StaticJsonDocument<256> doc;
    deserializeJson(doc, message);

    if (strcmp(topic, tally_state_topic) == 0) {
        String myKey = String(config.cameraID);
        int state = doc[myKey]; // 0: OFF, 1: PVW, 2: PGM
        switch(state) {
            case 1: currentTallyState = STATE_PVW; break;
            case 2: currentTallyState = STATE_PGM; break;
            default: currentTallyState = STATE_OFF; break;
        }
    } else if (strcmp(topic, call_topic) == 0) {
        if (doc["cam"] == config.cameraID) {
            if (strcmp(doc["state"], "ON") == 0) {
                currentTallyState = STATE_CALL;
            } else {
                // A call ending implies we go back to OFF, waiting for the next update.
                currentTallyState = STATE_OFF;
            }
        }
    }
}

void updateLED(TallyState state) {
    uint32_t color = 0x000000;
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(YELLOW_PIN, LOW);

    switch (state) {
        case STATE_PVW:
            color = 0x00FF00; // Green
            digitalWrite(GREEN_PIN, HIGH);
            break;
        case STATE_PGM:
            color = 0xFF0000; // Red
            digitalWrite(RED_PIN, HIGH);
            break;
        case STATE_CALL:
            color = 0xFFFF00; // Yellow
            digitalWrite(YELLOW_PIN, HIGH);
            break;
        case STATE_OFF:
        default:
            color = 0x000000; // Off
            break;
    }
    M5.Display.fillScreen(color);
}

void handleSerial() {
    while (Serial.available() > 0) {
        char incomingChar = Serial.read();
        if (incomingChar == '\n' || incomingChar == '\r') {
            if (serialBuffer.length() > 0) {
                parseCommand(serialBuffer);
                serialBuffer = "";
            }
        } else {
            serialBuffer += incomingChar;
        }
    }
}

void printHelp() {
    Serial.println("\n--- M5 Tally Serial Console ---");
    Serial.println("Commands:");
    Serial.println("  help | ?          - Shows this help message.");
    Serial.println("  status | show     - Shows current status and configuration.");
    Serial.println("  set ssid [ssid]   - Set WiFi SSID.");
    Serial.println("  set pass [pass]   - Set WiFi Password.");
    Serial.println("  set broker [ip]   - Set MQTT Broker IP address.");
    Serial.println("  set id [1-10]     - Set Camera ID.");
    Serial.println("  set rotation [0-3]- Set screen rotation.");
    Serial.println("  set ip mode [dhcp|static]");
    Serial.println("  set static ip [ip] [subnet] [gateway]");
    Serial.println("  save              - Save changes to EEPROM.");
    Serial.println("  reboot            - Reboot the device.");
    Serial.println("  force pgm         - Manual override to Program (Red).");
    Serial.println("  force pvw         - Manual override to Preview (Green).");
    Serial.println("  force clear       - Clear manual override.");
    Serial.println("---------------------------------");
}

void printStatus() {
    Serial.println("\n--- M5 Tally Status ---");
    Serial.print("Firmware Version: ");
    Serial.println(FW_VERSION);
    Serial.print("Camera ID: ");
    Serial.println(config.cameraID);
    Serial.print("Screen Rotation: ");
    Serial.println(config.rotation);
    Serial.println("--- WiFi ---");
    Serial.print("SSID: ");
    Serial.println(config.ssid);
    Serial.print("IP Mode: ");
    Serial.println(config.dhcp ? "DHCP" : "Static");
    if (!config.dhcp) {
        Serial.print("  IP: ");
        Serial.println(config.static_ip);
        Serial.print("  Subnet: ");
        Serial.println(config.subnet);
        Serial.print("  Gateway: ");
        Serial.println(config.gateway);
    }
    Serial.print("Current IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("--- MQTT ---");
    Serial.print("Broker: ");
    Serial.println(config.mqtt_broker);
    Serial.print("Connection: ");
    Serial.println(mqttClient.connected() ? "Connected" : "Disconnected");
    Serial.println("-------------------------");
}

void parseCommand(String command) {
    command.trim();
    if (command.length() == 0) {
        printHelp();
        return;
    }

    String cmd = command;
    String arg1 = "";
    String arg2 = "";
    String arg3 = "";
    String arg4 = "";

    int spaceIndex = command.indexOf(' ');
    if (spaceIndex != -1) {
        cmd = command.substring(0, spaceIndex);
        String args = command.substring(spaceIndex + 1);
        args.trim();

        // This is a simplified parser. A more robust solution might use strtok or a library.
        // It handles up to 4 arguments for the `set static ip` command.
        int lastSpace = -1;
        int argNum = 0;
        for(int i = 0; i < args.length() && argNum < 4; i++) {
            if (args.charAt(i) == ' ') {
                String* targetArg = (argNum == 0) ? &arg1 : (argNum == 1) ? &arg2 : &arg3;
                *targetArg = args.substring(lastSpace + 1, i);
                lastSpace = i;
                argNum++;
            }
        }
        String* targetArg = (argNum == 0) ? &arg1 : (argNum == 1) ? &arg2 : (argNum == 2) ? &arg3 : &arg4;
        *targetArg = args.substring(lastSpace + 1);

    }

    Serial.print("] ");
    Serial.println(command);

    if (cmd == "help" || cmd == "?") {
        printHelp();
    } else if (cmd == "status" || cmd == "show") {
        printStatus();
    } else if (cmd == "set") {
        if (arg1 == "ssid") {
            arg2.toCharArray(config.ssid, sizeof(config.ssid));
            Serial.print("OK. SSID set to: ");
            Serial.println(config.ssid);
        } else if (arg1 == "pass") {
            arg2.toCharArray(config.password, sizeof(config.password));
            Serial.println("OK. Password set.");
        } else if (arg1 == "broker") {
            arg2.toCharArray(config.mqtt_broker, sizeof(config.mqtt_broker));
            Serial.print("OK. Broker set to: ");
            Serial.println(config.mqtt_broker);
        } else if (arg1 == "id") {
            int id = arg2.toInt();
            if (id >= 1 && id <= 10) {
                config.cameraID = id;
                Serial.print("OK. Camera ID set to: ");
                Serial.println(config.cameraID);
            } else {
                Serial.println("Error: ID must be between 1 and 10.");
            }
        } else if (arg1 == "rotation") {
            int rot = arg2.toInt();
            if (rot >= 0 && rot <= 3) {
                config.rotation = rot;
                Serial.print("OK. Rotation set to: ");
                Serial.println(config.rotation);
            } else {
                Serial.println("Error: Rotation must be between 0 and 3.");
            }
        } else if (arg1 == "ip" && arg2 == "mode") {
            if (arg3 == "dhcp") {
                config.dhcp = true;
                Serial.println("OK. IP mode set to DHCP.");
            } else if (arg3 == "static") {
                config.dhcp = false;
                Serial.println("OK. IP mode set to Static.");
            } else {
                Serial.println("Error: mode must be 'dhcp' or 'static'.");
            }
        } else if (arg1 == "static" && arg2 == "ip") {
            arg3.toCharArray(config.static_ip, sizeof(config.static_ip));
            arg4.toCharArray(config.subnet, sizeof(config.subnet));
            // The last argument is the gateway
            int lastSpaceIndex = command.lastIndexOf(' ');
            command.substring(lastSpaceIndex + 1).toCharArray(config.gateway, sizeof(config.gateway));

            Serial.println("OK. Static IP config set.");
            Serial.print("  IP: "); Serial.println(config.static_ip);
            Serial.print("  Subnet: "); Serial.println(config.subnet);
            Serial.print("  Gateway: "); Serial.println(config.gateway);

        } else {
            Serial.println("Error: Unknown 'set' command.");
        }
    } else if (cmd == "save") {
        Serial.println("Saving configuration to EEPROM...");
        saveConfiguration();
        Serial.println("Done.");
    } else if (cmd == "reboot") {
        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
    } else if (cmd == "force") {
        if (arg1 == "pgm") {
            manualOverrideState = STATE_PGM;
            Serial.println("OK. Forcing PGM state.");
        } else if (arg1 == "pvw") {
            manualOverrideState = STATE_PVW;
            Serial.println("OK. Forcing PVW state.");
        } else if (arg1 == "clear" || arg1 == "auto") {
            manualOverrideState = STATE_NONE;
            Serial.println("OK. Manual override cleared.");
        } else {
            Serial.println("Error: Unknown 'force' command.");
        }
    } else {
        Serial.println("Error: Unknown command. Type 'help' for a list of commands.");
    }
}
