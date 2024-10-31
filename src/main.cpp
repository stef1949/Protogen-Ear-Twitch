#include <Arduino.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

extern "C" {
  uint8_t temperature_sens_read();
}

Servo servo1;
Servo servo2;

const int servo1Pin = 18;
const int servo2Pin = 21;

// Enhanced movement parameters
const int restingPosition = 135;
const int maxTwitchRange = 45;      // Maximum range for full twitches
const int microTwitchRange = 15;    // Range for subtle movements
const int interval = 15;            // Main loop interval
const int baseInterval = 5000;      // Base time between major twitches
const float smoothingFactor = 0.15; // Controls movement smoothing (0.0 to 1.0)

// Movement state variables
struct ServoState {
    float currentPosition;
    float targetPosition;
    float velocity;
    unsigned long lastMoveTime;
    bool isMoving;
};

ServoState servo1State = {restingPosition, restingPosition, 0, 0, false};
ServoState servo2State = {180 - restingPosition, 180 - restingPosition, 0, 0, false};

// Timing variables
unsigned long lastTwitchTime = 0;
unsigned long lastMicroTwitchTime = 0;
int twitchInterval;  // Will be randomized

// BLE definitions (unchanged)
#define SERVICE_UUID "0192be9b-60a3-738d-9601-6822d6161853"
#define CHARACTERISTIC_UUID "0192be9b-60a3-7f4a-ad00-80368ec6b227"
#define CPU_CHARACTERISTIC_UUID "0192be9b-60a3-730d-b326-995e1c434a10"
#define TEMP_CHARACTERISTIC_UUID "0192be9b-60a3-7dfb-89ec-e5d39dfb3cb7"

BLECharacteristic *pCharacteristic;
BLECharacteristic *cpuCharacteristic;
BLECharacteristic *tempCharacteristic;
bool deviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

// New function for smooth movement
void updateServoPosition(ServoState &state, Servo &servo, bool invert = false) {
    if (state.currentPosition != state.targetPosition) {
        float diff = state.targetPosition - state.currentPosition;
        
        // Apply smoothing
        float movement = diff * smoothingFactor;
        
        // Add some natural variance to the movement speed
        float speedVariance = random(80, 120) / 100.0;
        movement *= speedVariance;
        
        state.currentPosition += movement;
        
        // Update servo position
        int finalPosition = round(state.currentPosition);
        if (invert) {
            finalPosition = 180 - finalPosition;
        }
        servo.write(finalPosition);
        
        // Update movement state
        state.isMoving = abs(diff) > 0.5;
    }
}

// Enhanced twitch function with different types of movements
void triggerTwitch(bool isMicroTwitch = false) {
    float range = isMicroTwitch ? microTwitchRange : maxTwitchRange;
    
    // Calculate random positions with natural bias towards smaller movements
    float randomFactor = pow(random(0, 1000) / 1000.0, 1.5); // Bias towards smaller movements
    float offset = range * randomFactor * (random(2) == 0 ? -1 : 1);
    
    // Add slight asymmetry between ears
    float asymmetry = random(-5, 6);
    
    // Set new target positions
    servo1State.targetPosition = constrain(restingPosition + offset, 0, 180);
    servo2State.targetPosition = constrain(180 - (restingPosition + offset + asymmetry), 0, 180);
    
    // Randomize next twitch interval
    twitchInterval = baseInterval + random(-1000, 1001);
}

void setup() {
    Serial2.begin(115200);
    
    // Initialize servos with slightly different min/max pulses for natural asymmetry
    servo1.attach(servo1Pin, 500, 2400);
    servo2.attach(servo2Pin, 544, 2400); // Slightly different calibration
    
    servo1.write(restingPosition);
    servo2.write(180 - restingPosition);
    
    // Initialize BLE
    BLEDevice::init("Richies3D-LumiFur-Device");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    
    cpuCharacteristic = pService->createCharacteristic(
        CPU_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    
    tempCharacteristic = pService->createCharacteristic(
        TEMP_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    
    pCharacteristic->setValue("0");
    pService->start();
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->start();
    
    // Initialize random seed
    randomSeed(analogRead(0));
    twitchInterval = baseInterval;
}

void loop() {
    unsigned long currentTime = millis();
    
    // Handle BLE control
    if (deviceConnected) {
        std::string value = pCharacteristic->getValue();
        if (value == "1") {
            triggerTwitch(false);
        } else if (value == "0") {
            servo1State.targetPosition = restingPosition;
            servo2State.targetPosition = 180 - restingPosition;
        }
    }
    
    // Autonomous movement when not BLE controlled
    if (!deviceConnected) {
        // Major twitches
        if (currentTime - lastTwitchTime >= twitchInterval) {
            triggerTwitch(false);
            lastTwitchTime = currentTime;
        }
        
        // Micro twitches (subtle movements)
        if (currentTime - lastMicroTwitchTime >= 1000 && random(10) == 0) {
            triggerTwitch(true);
            lastMicroTwitchTime = currentTime;
        }
    }
    
    // Update servo positions with smooth movement
    updateServoPosition(servo1State, servo1, false);
    updateServoPosition(servo2State, servo2, true);
    
    delay(interval);
}

float readTemperature() {
    return (temperature_sens_read() - 32) / 1.8;
}

float getCPUUsage() {
    return 10.0;
}