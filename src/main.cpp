#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// **********************************************
// CONFIGURATION
// **********************************************

// --- Device ---
const uint8_t localId = 1;

// --- MQTT Broker ---
const char *mqttBroker = "broker.hivemq.com";
const int mqttPort = 1883;

// --- MQTT Topics ---
const char *mqttEventTopic = "orbitmorse/project42/events";
const String mqttDisplayTopic = "orbitmorse/project42/display";
const String mqttPlaybackTopic = "orbitmorse/project42/playback/" + String(localId);

// --- Pins ---
const int touchPin = 34;
const int ledRingPin = 16;
const int ledCount = 12;

// **********************************************
// HARDWARE & NETWORK
// **********************************************

// --- Display ---
rgb_lcd lcd;

// --- LED Ring ---
Adafruit_NeoPixel ledRing {
    ledCount,
    ledRingPin,
    NEO_RGB + NEO_KHZ800
};

// --- Network ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// **********************************************
// MORSE TIMING
// **********************************************

// --- Morse Time limits ---
// Standards:
// Dot: 1 TimeUnit, Dash: 3 TimeUnits
// Pause between Characters: 1 TimeUnit, Time between Letters: 3 TimeUnits, Time between Words: 7 TimeUnits
const unsigned long dashThreshold = 400;
const unsigned long characterPauseThreshold = 1000; //Letters
const unsigned long wordPauseThreshold = 3000;
const unsigned long messageEndThreshold = 10000;
const unsigned long longTouchThreshold = 2000;

// **********************************************
// TYPES
// **********************************************

enum class MessageType : uint8_t {
    TouchStart = 1,
    TouchEnd = 2,
    Dot = 3,
    Dash = 4,
    CharacterEnd = 5,
    WordEnd = 6,
    MessageEnd = 7,
    Acknowledge = 8,
    PendingMessage = 9,
    Aborted = 10,
    DeleteLastCharacter = 11
};

enum class PlanetState : uint8_t {
    Default = 1,
    MessageWaiting = 2,
    MessageReceiving = 3,
    MessageSending = 4,
    Acknowledge = 5,
    Aborted = 6
};

// **********************************************
// APPLICATION STATES
// **********************************************

// --- Touch Input ---
bool lastTouchState = LOW;

// --- Timestamps ---
unsigned long touchStartTime = 0;
unsigned long lastTouchEndTime = 0;
unsigned long lastTouchDuration = 0;
unsigned long lastPauseDuration = 0;

// --- Message State ---
bool currentlyMorsing = false;
bool characterEndSent = false;
bool wordEndSent = false;
bool messageEndSent = false;

bool messageWaiting = false;
bool allowedToSend = true;
bool acknowledgedSent = false;

String currentMorse = "";

// --- Output State ---
String displayText = "";
PlanetState ledState = PlanetState::Default;

// **********************************************
// LED ANIMATION / ANIMATION STATES
// **********************************************

// --- Color Wave ---
unsigned long lastColorWaveUpdate = 0;
long colorWaveHue = 21845;

// --- Comet Chase ---
const unsigned long cometInterval = 300;
const int cometBrightness[] = {2, 4, 8, 12, 16, 22, 28, 40, 60, 100, 165, 255};
unsigned long lastCometUpdate = 0;
int cometPhase = 0;
int cometIndex = 0;
int cometLength = ledCount - 1;
int cometPosition = ledCount;

// --- Comet Chase - Display ---
const int displayWidth = 16;
int displayDotPosition = 0;

// **********************************************
// FUNCTIONS
// **********************************************

// --- Default Animation: slowly moves a color gradient around the LED ring ---
void colorWave (int wait, long hueStart, long hueEnd, long hueStep) {
    unsigned long currentTime = millis();

    if (currentTime - lastColorWaveUpdate < wait) {
        return;
    }

    lastColorWaveUpdate = currentTime;

    long hueRange = hueEnd - hueStart;

    for (int i = 0; i < ledRing.numPixels(); i++) {
        int wavePosition = min(i, ledRing.numPixels() - i);

        long hueOffset = wavePosition * hueRange / ledRing.numPixels();

        long pixelHue = hueStart + ((colorWaveHue - hueStart + hueOffset) % hueRange);

        ledRing.setPixelColor(i, ledRing.gamma32(ledRing.ColorHSV(pixelHue)));
    }
    ledRing.show();

    colorWaveHue += hueStep;

    if (colorWaveHue >= hueEnd) {
        colorWaveHue = hueStart;
    }
}

// --- Display Animation when message is waiting: moves three dots across the display ---
void updateWaitingMessageDisplay() {
    String text = "                ";
    text.setCharAt(displayDotPosition, '.');
    text.setCharAt((displayDotPosition + 1) % displayWidth, '.');
    text.setCharAt((displayDotPosition + 2) % displayWidth, '.');

    lcd.setCursor(0, 0);
    lcd.print(text);

    displayDotPosition++;

    if (displayDotPosition >= displayWidth) {
        displayDotPosition = 0;
    }
}

// --- Waiting message animation ---
void cometChase() {
    unsigned long currentTime = millis();

    if (currentTime - lastCometUpdate < cometInterval) {
        return;
    }
    lastCometUpdate = currentTime;

    // --- Update Display ---
    updateWaitingMessageDisplay();

    ledRing.clear();

    // --- PHASE 1 (Build up comet) ---
    if (cometPhase == 0) {
        for (int j = 0; j <= cometIndex; j++) {
            int value = cometBrightness[j];
            int pixel = (cometIndex + j) % ledCount;
            ledRing.setPixelColor(pixel, ledRing.Color(value, value, value));
        }
        ledRing.show();
        cometIndex++;

        if (cometIndex >= ledCount) {
            cometPhase = 1;

            cometLength = ledCount - 1;
            cometPosition = ledCount;
        }
    }

    // --- PHASE 2 (Build back comet) ---
    else if (cometPhase == 1) {

        for (int j = 0; j <= cometLength; j++) {
            int value = cometBrightness[j];

            int pixel =
                (cometPosition + j) % ledCount;

            ledRing.setPixelColor(
                pixel,
                ledRing.Color(value, value, value)
            );
        }

        ledRing.show();

        cometLength--;
        cometPosition += 2;

        if (cometLength < 0) {
            cometPhase = 0;
            cometIndex = 0;
        }
    }
}

// --- Update LED-Ring depending on PlanetState ---
void updateLedRing() {
    switch (ledState) {
        case PlanetState::Default:
            colorWave(50, 21845, 65536, 128);
            break;
        case PlanetState::MessageWaiting:
            cometChase();
            break;
        case PlanetState::MessageReceiving:
            ledRing.fill(ledRing.Color(255, 255, 255));
            ledRing.show();
            break;
        case PlanetState::MessageSending:
            ledRing.fill(ledRing.Color(255, 255, 255));
            ledRing.show();
            break;
        case PlanetState::Acknowledge:
            ledRing.fill(ledRing.Color(255, 255, 255));
            ledRing.show();
            break;
        case PlanetState::Aborted:
            ledRing.fill(ledRing.Color(255, 0, 0));
            ledRing.show();
            break;
    }
}

// --- Encodes a 32-bit duration into four bytes for the binary MQTT payload ---
void writeUint32LE(uint8_t* buffer, uint32_t value) {
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
}

// --- Replays a received message letter by letter ---
void handlePlayback(byte* payload, unsigned int length) {
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.println("Couldn't read JSON");
        return;
    }

    JsonArray letters = doc["playback"].as<JsonArray>();

    if (letters.isNull()) {
        Serial.println("No playback letters found");
        return;
    }

    displayText = "";

    lcd.clear();
    lcd.setCursor(0, 0);

    for (JsonObject letter : letters) {
        const char* character = letter["char"];

        if (character == nullptr) {
            Serial.println("Letter missing");
            continue;
        }

        Serial.print("Playback letter: ");
        Serial.println(character);

        JsonArray sequence = letter["sequence"].as<JsonArray>();

        if (sequence.isNull()) {
            Serial.println("No playback-sequence found");
            return;
        }

        // --- Play light sequence ---
        for (JsonObject step: sequence) {
            const char *type = step["type"];
            unsigned long duration = step["duration"];

            if (type == nullptr) {
                continue;
            }

            if (strcmp(type, "signal") == 0) {
                ledRing.fill(ledRing.Color(255, 255, 255));
                ledRing.show();
                delay(duration);
            } else if (strcmp(type, "pause") == 0) {
                unsigned long startTime = millis();
                while (millis() - startTime < duration) {
                    colorWave(50, 21845, 65536, 128);
                    delay(1);
                }
            } else {
                continue;
            }
        }

        // --- Print text on display ---
        displayText += character;

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print(displayText.substring(0, 16));

        if (displayText.length() > 16) {
            lcd.setCursor(0, 1);
            lcd.print(displayText.substring(16, 32));
        }
    }

    ledState = PlanetState::Default;
    lastColorWaveUpdate = 0;
    updateLedRing();
}

// --- Handles all incoming messages ---
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);

    // --- Update Display ---
    if (strcmp(topic, mqttDisplayTopic.c_str()) == 0) {
        String text = "";
        for (unsigned int i = 0; i < length; i++) {
            text += (char) payload[i];
        }

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(text.substring(0, 16));
        if (text.length() > 16) {
            lcd.setCursor(0, 1);
            lcd.print(text.substring(16, 32));
        }
        return;
    }

    // --- Handle Playback ---
    if (strcmp(topic, mqttPlaybackTopic.c_str()) == 0) {
        handlePlayback(payload, length);
        return;
    }

    // --- Validate incoming message ---
    if (length < 2) {
        Serial.println("Invalid MQTT message");
        return;
    }

    // --- Parse message data ---
    uint8_t senderId = payload[0];
    auto messageType = static_cast<MessageType>(payload[1]);

    // --- Ignore own messages ---
    if (senderId == localId) {
        return;
    }

    Serial.print("Sender: ");
    Serial.println(senderId);
    Serial.print("MessageType: ");
    Serial.println(static_cast<int>(messageType));

    // --- Handle Message ---
    switch (messageType) {
        case MessageType::TouchStart:
            allowedToSend = false;
            ledState = PlanetState::MessageReceiving;
            updateLedRing();
            break;
        case MessageType::TouchEnd:
            ledState = PlanetState::Default;
            updateLedRing();
            break;
        case MessageType::PendingMessage:
            ledState = PlanetState::MessageWaiting;
            messageWaiting = true;
            updateLedRing();

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("...");
            break;
        case MessageType::Aborted:
            ledState = PlanetState::Aborted;
            updateLedRing();
            break;
        case MessageType::MessageEnd:
            ledState = PlanetState::Default;
            allowedToSend = true;
            break;
    }
}

void publishMessage(MessageType messageType, long signalDuration = -1, long pauseDuration = -1) {
    uint8_t message[10];
    message[0] = localId;
    message[1] = static_cast<uint8_t>(messageType);

    size_t length = 2;

    if (signalDuration >= 0 && pauseDuration >= 0) {
        writeUint32LE(message + 2, static_cast<uint32_t>(signalDuration));
        writeUint32LE(message + 6, static_cast<uint32_t>(pauseDuration));
        length = 10;
    }

    mqttClient.publish(mqttEventTopic, message, length);
}

// --- Connects to the MQTT broker and restores all required subscriptions ---
void reconnect() {
    while (!mqttClient.connected()) {
        String clientId = "OrbitMorse-" + String(WiFi.macAddress());
        clientId.replace(":", "");

        Serial.printf("The client %s connects to the public MQTT broker\n",
            clientId.c_str()
            );

        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("MQTT connected");

            mqttClient.subscribe(mqttEventTopic);
            Serial.println("MQTT subscribed to: ");
            Serial.println(mqttEventTopic);
            mqttClient.subscribe(mqttDisplayTopic.c_str());
            mqttClient.subscribe(mqttPlaybackTopic.c_str());

        } else {
            Serial.println("MQTT connection failed with state: ");
            Serial.println(mqttClient.state());

            delay(2000);
        }
    }
}

void setup() {
    Serial.begin(115200);

    // --- Pin Modes ---
    pinMode(touchPin, INPUT);

    // --- Display ---
    Wire.begin();
    lcd.begin(16, 2);

    // --- LED Ring ---
    ledRing.begin();

    // --- Wifi Manager Setup ---
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(180);

    String accessPointName = "OrbitMorse_Setup" + String(localId);
    if (!wifiManager.autoConnect(accessPointName.c_str(), "12345678")) {
        Serial.println("Failed to connect to Wifi -> restart");
        ESP.restart();
    }

    Serial.println("Connected to Wifi: ");
    Serial.println(WiFi.SSID());
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP().toString());

    // --- MQTT-Setup ---
    mqttClient.setServer(mqttBroker, mqttPort);
    mqttClient.setCallback(callback);
    mqttClient.setBufferSize(16384);

    // --- Print Display ---
    lcd.setCursor(0, 0);
    lcd.print("Orbit Morse");

    reconnect();
}

void loop() {
    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();

    updateLedRing();

    // --- When waiting for acknowledge or the other person is morsing -> return ---
    if (!allowedToSend) {
        return;
    }

    // --- Read Touch State ---
    bool currentTouchState = digitalRead(touchPin);

    // --- Publish MQTT Message ---
    // --- TOUCH START ---
    if (currentTouchState == HIGH && lastTouchState == LOW) {
        touchStartTime = millis();

        if (lastTouchEndTime > 0) {
            lastPauseDuration = touchStartTime - lastTouchEndTime;
        }

        ledState = PlanetState::MessageSending;
        updateLedRing();

        publishMessage(MessageType::TouchStart);

        characterEndSent = false;
        wordEndSent = false;
        messageEndSent = false;
        acknowledgedSent = false;
    }

    // --- TOUCH END ---
    else if (currentTouchState == LOW && lastTouchState == HIGH) {
        lastTouchEndTime = millis();
        lastTouchDuration = lastTouchEndTime - touchStartTime;

        if (!messageWaiting) {
            ledState = PlanetState::Default;
        }
        updateLedRing();

        publishMessage(MessageType::TouchEnd);

        if (lastTouchDuration <= dashThreshold && !messageWaiting) {
            publishMessage(MessageType::Dot, lastTouchDuration, lastPauseDuration);
            currentMorse += ".";
        } else if (lastTouchDuration <= longTouchThreshold && !messageWaiting) {
            publishMessage(MessageType::Dash, lastTouchDuration, lastPauseDuration);
            currentMorse += "-";
        } else if (lastTouchDuration > longTouchThreshold && messageWaiting) {
            publishMessage(MessageType::Acknowledge);
            acknowledgedSent = true;
            messageWaiting = false;
            lastTouchEndTime = 0;
        } else if (lastTouchDuration > longTouchThreshold) {
            publishMessage(MessageType::DeleteLastCharacter);
            acknowledgedSent = true;
        }
    }

    // --- CALCULATE PAUSE DURATION ---
    if (currentTouchState == LOW && lastTouchEndTime > 0 && !acknowledgedSent) {
        unsigned long currentPauseDuration = millis() - lastTouchEndTime;

        if (currentPauseDuration > characterPauseThreshold && !characterEndSent) {
            publishMessage(MessageType::CharacterEnd);
            currentMorse += " ";
            characterEndSent = true;
        }
        if (currentPauseDuration > wordPauseThreshold && !wordEndSent) {
            publishMessage(MessageType::WordEnd);
            currentMorse += "  ";
            wordEndSent = true;
        }
        if (currentPauseDuration > messageEndThreshold && !messageEndSent) {
            publishMessage(MessageType::MessageEnd);
            currentlyMorsing = false;
            messageEndSent = true;
            allowedToSend = false;

            lastTouchEndTime = 0;
        }
    }

    lastTouchState = currentTouchState;
}

