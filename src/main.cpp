#include <Arduino.h>
// Servo Libararies
#include <ESP32Servo.h>
// Bluetooth Libraries
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

extern "C" {
  uint8_t temprature_sens_read();  // Access the ESP32 internal temperature sensor
}

// Create multiple servo objects
Servo servo1;
Servo servo2;

// These are all GPIO pins on the ESP32
// Recommended pins include 2,4,12-19,21-23,25-27,32-33
// for the ESP32-S2 the GPIO pins are 1-21,26,33-42
// for the ESP32-S3 the GPIO pins are 1-21,35-45,47-48
// for the ESP32-C3 the GPIO pins are 1-10,18-21
const int servo1Pin = 18;       // The GPIO pin where the servo1 is connected
const int servo2Pin = 21;         // The GPIO pin where the servo2 is connected

// Define constants
const int restingPosition = 135;    // The position (in degrees) where the servo rests between twitches
const int twitchRange = 45;        // The total range of motion for twitches (±22.5 degrees from resting position)
const int interval = 15;           // Main loop interval in milliseconds
const int twitchInterval = 2000;    // Time between twitches in milliseconds

int countTwitch = 0;  // Counter for tracking twitch intervals
bool isTwitching = false;  // Flag to track if the servo is currently in a twitch position

// Define Bluetooth service and characteristic UUIDs
#define SERVICE_UUID "0192be9b-60a3-738d-9601-6822d6161853"
#define CHARACTERISTIC_UUID "0192be9b-60a3-7f4a-ad00-80368ec6b227"
#define CPU_CHARACTERISTIC_UUID "0192be9b-60a3-730d-b326-995e1c434a10"
#define TEMP_CHARACTERISTIC_UUID "0192be9b-60a3-7dfb-89ec-e5d39dfb3cb7"

BLECharacteristic *pCharacteristic;
BLECharacteristic *cpuCharacteristic;
BLECharacteristic *tempCharacteristic;

bool deviceConnected = false;

// Function prototypes
void triggerTwitch();
float readTemperature();
float getCPUUsage();

// Callbacks for BLE server client connection status
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

void setup() {
  Serial2.begin(115200);

  // Initialize the servos
  servo1.attach(servo1Pin, 500, 2400);  // Attach the servo to the specified pin with min/max pulse widths
  servo2.attach(servo2Pin, 500, 2400);  // Attach the servo to the specified pin with min/max pulse widths
  // Move the servos to their initial resting position
  servo1.write(restingPosition);
  servo2.write(180 - restingPosition);

  // Initialize BLE
  BLEDevice::init("Richies3D-LumiFur-Device");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE
                    );

 // CPU utilization characteristic
  cpuCharacteristic = pService->createCharacteristic(
                        CPU_CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ
                      );

  // Temperature characteristic
  tempCharacteristic = pService->createCharacteristic(
                         TEMP_CHARACTERISTIC_UUID,
                         BLECharacteristic::PROPERTY_READ
                       );

  pCharacteristic->setValue("0");  // Initialize characteristic with "LED OFF"
  pService->start();  // Start the service

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
}

void loop() {
  // BLE control: Check if the characteristic has been written to and adjust the servos accordingly
  if (deviceConnected) {
    std::string value = pCharacteristic->getValue();
    if (value == "1") {
      // Example: Activate twitching when receiving "1"
      triggerTwitch();
    } else if (value == "0") {
      // Return servos to resting position on receiving "0"
      servo1.write(restingPosition);
      servo2.write(180 - restingPosition);
      isTwitching = false;  // Ensure it's not twitching
    }
  }

  // Increment the counter
  countTwitch++;

  // Check if it's time for a twitch (only twitch if not controlled by BLE)
  if (!deviceConnected && countTwitch >= (twitchInterval / interval)) {
    countTwitch = 0;  // Reset the counter
    triggerTwitch();  // Trigger twitch as usual
  }

  // Small delay to control the loop interval
  delay(interval);
}

// Function to trigger servo twitch
void triggerTwitch() {
  if (!isTwitching) {
    // Calculate a random position for the twitch
    int randomOffset = random(-twitchRange/2, twitchRange/2 + 1);  // Generate a random offset
    int twitchPosition = restingPosition + randomOffset;  // Calculate the new position

    // Ensure the twitch position is within the valid servo range (0-180 degrees)
    twitchPosition = constrain(twitchPosition, 0, 180);

    // Move the servos to the twitch position
    servo1.write(twitchPosition);
    servo2.write(180 - twitchPosition);
    isTwitching = true;
  } else {
    // Return the servos to the resting position
    servo1.write(restingPosition);
    servo2.write(180 - restingPosition);
    isTwitching = false;
  }
} 

// Function to read ESP32 internal temperature sensor
float readTemperature() {
  return (temprature_sens_read() - 32) / 1.8;  // Convert to Celsius
}

// Dummy function for CPU usage (to be implemented as per your needs)
float getCPUUsage() {
  // Return a dummy value for now (this should be replaced with actual CPU utilization calculation)
  return 10.0;
}