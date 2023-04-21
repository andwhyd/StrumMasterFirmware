/**
 * Bluetooth features based on:
 * https://github.com/loginov-rocks/Web-Bluetooth-Terminal/tree/master/misc/Arduino-Bridge/Arduino-Bridge.ino
 *
 * Wire HM-10 bluetooth module to the Arduino:
 * RX  - 0
 * TX  - 1
 * GND - GND
 * VCC - 7
 *
 * Send: "Connecting" to allow configuration
 **/
#include <Adafruit_NeoPixel.h>
#include <AnalogMultiButton.h>
#include <ArduinoJson.h>

// Serial setup
#define PC_SERIAL_BAUDRATE 115200
#define BT_SERIAL_BAUDRATE 9600

// Mode definition
enum Mode : uint8_t {
    CONFIGURING = 0,
    STANDARD = 1,
    SERIAL_MODE = 2,
    LIVE = 3,
    ERROR = 99
};
Mode currentMode;

// RGB LED setup
uint8_t LED_BRIGHTNESS = 4;               // out of 255
Adafruit_NeoPixel strip(1, 40, NEO_RGB);  // 1 LED, pin 40, RGB

// JSON variables
#define MAX_INPUT_LENGTH 400
#define BUTTON_NUM_KEY "buttonNum"
#define BUTTON_CONFIG_KEY "buttonConfigs"
StaticJsonDocument<MAX_INPUT_LENGTH> doc;
const char *TYPE_KEYS[] = {"Pick", "Strum U", "Strum D", "TBD"};
char bt_message[MAX_INPUT_LENGTH];

// Button definitions
const int BUTTONS_TOTAL = 6;  // number of connected buttons

// Pin definitions
const int BUTTONS_PIN = A5;   // analog pin for reading controller
const int BLUETOOTH_PWR = 7;  // digital pin used to power bluetooth module
const int SOLENOID_PINS[BUTTONS_TOTAL] = {13, 12, 11, 10, 9, 8};

// Button config
// find out what the value of analogRead is when you press each of your buttons
// and put them in this array you can find this out by putting
// Serial.println(analogRead(BUTTONS_PIN)); in your loop() and opening the
// serial monitor to see the values make sure they are in order of smallest to
// largest
const int BUTTONS_VALUES[BUTTONS_TOTAL] = {260, 410, 510, 590, 640, 680};
// make an AnalogMultiButton object, pass in the pin, total and values array
AnalogMultiButton buttons(BUTTONS_PIN, BUTTONS_TOTAL, BUTTONS_VALUES);

// Button variables
int buttonNum;
// TODO: locally save and load config on boot
// Default config:
uint8_t buttons_config[BUTTONS_TOTAL][7] = {
    {1, 0, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 0, 1, 0}};
;

// Delay config
uint8_t PICK_DELAYS[BUTTONS_TOTAL] = {12, 12, 12, 12, 12, 12};  // ms delay during which solenoid is activated
uint16_t STRUM_DELAYS[BUTTONS_TOTAL] = {0, 0, 0, 0, 0, 0};

bool CONNECTED;
bool config_status;

void setup() {
    // Begin serial
    Serial.begin(PC_SERIAL_BAUDRATE);
    Serial1.begin(BT_SERIAL_BAUDRATE);

    // Initialize variables
    currentMode = STANDARD;
    CONNECTED = false;
    buttonNum = 12;
    config_status = false;

    // Set pin modes
    pinMode(BLUETOOTH_PWR, OUTPUT);
    for (int j = 0; j < BUTTONS_TOTAL; j++) {
        pinMode(SOLENOID_PINS[j], OUTPUT);
    }

    // Power cycle bluetooth:
    digitalWrite(BLUETOOTH_PWR, LOW);
    delay(1000);
    digitalWrite(BLUETOOTH_PWR, HIGH);

    delay(1000);
    Serial.println("Setup complete");

    // Initialize RGB LED "strip" (only 1 LED in strip)
    strip.begin();
    strip.setBrightness(LED_BRIGHTNESS);
    strip.setPixelColor(0, 0, 0, 0);
    strip.show();
}

void loop() {
    if (Serial1.available()) {
        Serial1.readBytesUntil('\n', bt_message, MAX_INPUT_LENGTH);
        if (!CONNECTED) {
            if (strstr(bt_message, "CONNECTING") != NULL) {
                bt_message[0] = 0;
                Serial1.write("Connected\n");
                Serial.println("\nConnected");
                CONNECTED = true;
            }
        } else {
            Serial.print("\nReceived: ");
            Serial.println(bt_message);
            if (strstr(bt_message, "OK+LOST") != NULL) {
                CONNECTED = false;
                Serial.println("Disconnected");
            } else if (strstr(bt_message, "MODE:") != NULL) {
                char modeNum[2];
                strncpy(modeNum, bt_message + 5, 2);
                currentMode = (Mode)atoi(modeNum);
                Serial.print("New mode: ");
                Serial.println(currentMode);
            } else if (currentMode == CONFIGURING) {
                Serial1.println("CONFIGURING");
                if (strstr(bt_message, "STRUMDELAY:") != NULL) { // STRUMDELAY:##:###
                    char buttonNum[3];
                    strncpy(buttonNum, bt_message + 11, 2);
                    char fireDelayC[3];
                    strncpy(fireDelayC, bt_message + 14, 3);
                    STRUM_DELAYS[atoi(buttonNum)] = atoi(fireDelayC);
                } else if (strstr(bt_message, "PICKDELAY:") != NULL) { // PICKDELAY:##:##
                    char buttonNum[3];
                    strncpy(buttonNum, bt_message + 10, 2);
                    char pickDelayC[3];
                    strncpy(pickDelayC, bt_message + 13, 2);
                    PICK_DELAYS[atoi(buttonNum)] = atoi(pickDelayC);
                } else {
                    config_status = parseJson();
                    printConfig();
                }
                Serial1.println("CONFIGURED");
            } else if (currentMode == LIVE) {
                if (strstr(bt_message, "LIVE:") != NULL) {
                    char command[2];
                    strncpy(command, bt_message + 5, 2);
                    uint8_t result = keyboard_action(atoi(command));
                    if (result != 0) {
                        Serial.print("Played button #");
                        Serial.println(result);
                        Serial1.write("LIVE:");
                        Serial1.print(result);
                        Serial1.write("\n");
                    }
                }
            }
        }
    }
    if (currentMode == STANDARD) {
        // Parses button stuff and does solenoid stuff
        strip.setPixelColor(0, 255, 0, 0);  // red
        strip.show();
        buttons.update();
        for (int j = 0; j < BUTTONS_TOTAL; j++) {
            if (buttons.onPress(j)) {
                Serial.print(j + 1);
                Serial.println(" has been pressed");
                playAction(j);
            }
        }
    } else if (currentMode == SERIAL_MODE) {
        strip.setPixelColor(0, 0, 255, 0);  // green
        strip.show();
        if (Serial.available()) {
            char command = Serial.read();
            uint8_t result = keyboard_action(command);
            if (result != 0) {
                Serial.print("Played button #");
                Serial.println(result);
            }
        }
    } else if (currentMode == LIVE) {
        strip.setPixelColor(0, 0, 0, 255);  // blue
        strip.show();
    }
}

bool parseJson() {
    DeserializationError error =
        deserializeJson(doc, bt_message, MAX_INPUT_LENGTH);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return false;
    }
    for (int j = 0; j < buttonNum; j++) {
        for (int k = 0; k < 6; k++) {
            buttons_config[j][k] = 0;
        }
    }
    buttonNum = doc[BUTTON_NUM_KEY];
    if (buttonNum > 12 || buttonNum < 1) {
        Serial.println("Button number error, must be within 1 - 12");
        return false;
    }
    JsonArray buttonConfigs = doc[BUTTON_CONFIG_KEY];
    for (int j = 0; j < buttonNum; j++) {
        if (buttonConfigs[j] < 0 || buttonConfigs[j] > 255) {
            Serial.print(F("Config code error"));
            return false;
        }
        parseInts(j, uint8_t(buttonConfigs[j]));
    }
    bt_message[0] = 0;
    return true;
}

// Parse the binary code for each string and save
void parseInts(int b_idx, uint8_t ints) {
    for (int j = 0; j < 6; j++) {
        buttons_config[b_idx][j] = ints >> j & 0b1;
    }
    buttons_config[b_idx][6] = ints >> 6 & 0b11;
}

void printConfig() {
    if (config_status) {
        Serial.println("Button #: E A D G B E Type");
        for (int j = 0; j < buttonNum; j++) {
            Serial.print("Button ");
            Serial.print(j);
            Serial.print(":");
            for (int k = 0; k < 6; k++) {
                Serial.print(" ");
                Serial.print(buttons_config[j][k]);
            }
            Serial.print(" ");
            Serial.println(TYPE_KEYS[buttons_config[j][6]]);
        }
    }
}

void playAction(int buttonIndex) {
    if (buttons_config[buttonIndex][6] == 0) {  // Pick
        pick(buttonIndex);
    } else if (buttons_config[buttonIndex][6] == 1) {  // Strum U
        strumUp(buttonIndex);
    } else if (buttons_config[buttonIndex][6] == 2) {  // Strum D
        strumDown(buttonIndex);
    } else {
        Serial.println("no action identified");
    }
}

void strumDown(int buttonIndex) {
    for (int i = 0; i < 6; i++) {
        digitalWrite(SOLENOID_PINS[i], buttons_config[buttonIndex][i]);
        delay(PICK_DELAYS[buttonIndex]);
        digitalWrite(SOLENOID_PINS[i], LOW);
        delay(STRUM_DELAYS[buttonIndex]);
    }
}

void strumUp(int buttonIndex) {
    for (int i = 5; i >= 0; i--) {
        digitalWrite(SOLENOID_PINS[i], buttons_config[buttonIndex][i]);
        delay(PICK_DELAYS[buttonIndex]);
        digitalWrite(SOLENOID_PINS[i], LOW);
        delay(STRUM_DELAYS[buttonIndex]);
    }
}

void pick(int buttonIndex) {
    for (int i = 0; i < 6; i++) {
        digitalWrite(SOLENOID_PINS[i], buttons_config[buttonIndex][i]);
    }
    delay(PICK_DELAYS[buttonIndex]);
    for (int i = 0; i < 6; i++) {
        digitalWrite(SOLENOID_PINS[i], LOW);
    }
}

uint8_t keyboard_action(uint8_t command) {
    uint8_t played = 0;
    switch (command) {
        case 'a':
        case 00:
            playAction(0);
            played = 1;
            break;
        case 's':
        case 01:
            playAction(1);
            played = 2;
            break;
        case 'd':
        case 02:
            playAction(2);
            played = 3;
            break;
        case 'f':
        case 03:
            playAction(3);
            played = 4;
            break;
        case 'g':
        case 04:
            playAction(4);
            played = 5;
            break;
        case 'h':
        case 05:
            playAction(5);
            played = 6;
            break;
    }
    return played;
}
// git noob over here