#include <Arduino.h>
#include <Matter.h>
#include <MatterMultiSpeedFan.h>
#if CONFIG_ENABLE_MATTER_OVER_WIFI 
#include <WiFi.h>
#endif

MatterMultiSpeedFan SmartFan;

// Commissioning state machine
enum CommissioningState {
  COMMISSIONING_NOT_STARTED,
  COMMISSIONING_WAITING,
  COMMISSIONING_JUST_COMPLETED,
  COMMISSIONING_DONE
};

CommissioningState commissioningState = COMMISSIONING_NOT_STARTED;
unsigned long lastCommissioningMessageTime = 0;


#if CONFIG_ENABLE_MATTER_OVER_WIFI
// WiFi is manually set and started
const char *ssid = "Hyperoptic Fibre 91B3";          // Change this to your WiFi SSID
const char *password = "J47MkaB84J4Eju";  // Change this to your WiFi password
#endif

// Non-blocking pulse state machine variables
enum PulseState {
  PULSE_IDLE,
  PULSE_HIGH,      // Pin HIGH for 200ms
  PULSE_LOW        // Pin LOW for 100ms between pulses
};

uint8_t expectedFanSpeed = 0;    // Target speed to reach
uint8_t currentFanSpeed = 0;     // Current physical fan speed
bool isPulsing = false;          // True when actively pulsing
PulseState pulseState = PULSE_IDLE;
unsigned long pulseStartTime = 0;

// Speed tracking and concurrency control
SemaphoreHandle_t mutex = NULL;  // Mutex for thread safety

// Matter Protocol Callback - Speed changed from controller
bool onSpeedChange(uint8_t newSpeed) {
  Serial.printf("Matter Callback :: New Speed Level = %d ", newSpeed);

  // Print speed name for clarity
  switch (newSpeed) {
    case FAN_SPEED_OFF:    Serial.println("(OFF)"); break;
    case FAN_SPEED_LOW:    Serial.println("(LOW)"); break;
    case FAN_SPEED_MEDIUM: Serial.println("(MEDIUM)"); break;
    case FAN_SPEED_HIGH:   Serial.println("(HIGH)"); break;
    default:               Serial.printf("(LEVEL %d)\r\n", newSpeed); break;
  }

  if (xSemaphoreTake(mutex, 5000) == pdTRUE) {
    // Set the expected speed - the state machine will handle pulsing
    expectedFanSpeed = newSpeed;
    Serial.printf("Expected speed set to %d\r\n", expectedFanSpeed);
    if(isPulsing == false && expectedFanSpeed != currentFanSpeed) {
      // Start pulsing process
      isPulsing = true;
    }
    xSemaphoreGive(mutex);
  }

  // Return true to indicate success
  return true;
}

// Matter Protocol Callback - Rock/Oscillation changed from controller
bool onRockChange(uint8_t rockSetting) {
  Serial.printf("Matter Callback :: Rock Setting = 0x%02X ", rockSetting);

  if (rockSetting & ROCK_LEFT_RIGHT) {
    Serial.println("(OSCILLATION ON)");
    // TODO: Turn on oscillation motor/mechanism
  } else {
    Serial.println("(OSCILLATION OFF)");
    // TODO: Turn off oscillation motor/mechanism
  }

  // Return true to indicate success
  return true;
}

/*
  Print Periodically the current status of the Fan Matter Accessory
  Speed, On/Off state, Rock/Oscillation state
*/
unsigned long lastPrintingTime = 0;
void printStatusPeriodically() {
  if (millis() - lastPrintingTime >= 10000) { // Every 10 seconds
    lastPrintingTime = millis();
    Serial.printf("Status :: Speed = %d, OnOff = %d, Rock = %d\r\n",
                  SmartFan.getSpeed(), SmartFan.getOnOff(), SmartFan.getRockSetting());
  }
}

/*
  Handle Decommissioning when the button is kept pressed for a defined time
*/
uint32_t buttonPressTimestamp = 0;                // debouncing control
bool buttonState = false;                     // false = released | true = pressed
const uint32_t debouceTime = 250;              // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission
void handleDecommission() {
  // A button is also used to control the fan
  // Check if the button has been pressed
  if (digitalRead(BOOT_BUTTON_PIN) == LOW && !buttonState) {
    // deals with button debouncing
    buttonPressTimestamp = millis();  // record the time while the button is pressed.
    buttonState = true;           // pressed.
  }

  // Onboard User Button is used as a Fan speed cycle switch or to decommission it
  uint32_t time_diff = millis() - buttonPressTimestamp;

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (buttonState && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Fan Matter Accessory. It shall be commissioned again.");
    SmartFan.setSpeed(0);  // Turn off fan
    Matter.decommission();
    buttonPressTimestamp = millis();  // avoid running decommissioning again, reboot takes a second or so
    ESP.restart();  // restart the device after decommissioning
  }
}

void syncFanSpeedBasedOnExternalInputs() {
  if(xSemaphoreTake(mutex, 0) == pdTRUE) {
    if(!isPulsing) {
      uint8_t newSpeedLevel = 0;
      // Monitor input pins only when commissioned
      // Initialize input pin states
        if (digitalRead(FAN_SPEED_HIGH_INPUT_PIN) == LOW) {
          newSpeedLevel = 3;
        } else if (digitalRead(FAN_SPEED_MEDIUM_INPUT_PIN) == LOW) {
          newSpeedLevel = 2;
        } else if (digitalRead(FAN_SPEED_LOW_INPUT_PIN) == LOW) {
          newSpeedLevel = 1;
        }
      if(currentFanSpeed != newSpeedLevel) {
        Serial.printf("LED Input Pin: Setting speed level to %d\r\n", newSpeedLevel);
        expectedFanSpeed = newSpeedLevel; // Update expected speed for state machine
        currentFanSpeed = newSpeedLevel; // Physical input means fan already at this speed
        SmartFan.setSpeed(newSpeedLevel, false); // Update Matter state without pulsing
      }
    }
    xSemaphoreGive(mutex);
  }
}

void handleCommissioning() {
  switch (commissioningState) {
    case COMMISSIONING_NOT_STARTED:
      // Check if device needs commissioning
      if (!Matter.isDeviceCommissioned()) {
        // Print commissioning info once
        Serial.println("");
        Serial.println("Matter Node is not commissioned yet.");
        Serial.println("Initiate the device discovery in your Matter environment.");
        Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
        Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
        Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());

        commissioningState = COMMISSIONING_WAITING;
        lastCommissioningMessageTime = millis();
      } else {
        // Already commissioned, skip to done state
        commissioningState = COMMISSIONING_DONE;
      }
      break;

    case COMMISSIONING_WAITING:
      // Check if still waiting for commissioning
      if (!Matter.isDeviceCommissioned()) {
        // Print periodic update every 5 seconds
        if (millis() - lastCommissioningMessageTime >= 5000) {
          lastCommissioningMessageTime = millis();
          Serial.printf("Matter Node not commissioned yet. Waiting for commissioning. Commissioning code: %s\r\n",
                        Matter.getManualPairingCode().c_str());
        }
      } else {
        // Commissioning just completed!
        commissioningState = COMMISSIONING_JUST_COMPLETED;
      }
      break;

    case COMMISSIONING_JUST_COMPLETED:
      // Initialize state once after commissioning completes
      Serial.printf("Initial State :: Speed = %d, OnOff = %d, Rock = 0x%02X\r\n",
                    SmartFan.getSpeed(), SmartFan.getOnOff(), SmartFan.getRockSetting());
      SmartFan.updateAccessory();

      if(xSemaphoreTake(mutex, 0) == pdTRUE) {
        if(!isPulsing) {
          uint8_t matterSpeed = SmartFan.getSpeed();
          currentFanSpeed = matterSpeed;

          uint8_t newSpeedLevel = 0;
          // Initialize input pin states
          if (digitalRead(FAN_SPEED_HIGH_INPUT_PIN) == LOW) {
            newSpeedLevel = 3;
          } else if (digitalRead(FAN_SPEED_MEDIUM_INPUT_PIN) == LOW) {
            newSpeedLevel = 2;
          } else if (digitalRead(FAN_SPEED_LOW_INPUT_PIN) == LOW) {
            newSpeedLevel = 1;
          }

          if(currentFanSpeed != newSpeedLevel) {
            Serial.printf("LED Input Pin: Setting speed level to %d\r\n", newSpeedLevel);
            expectedFanSpeed = newSpeedLevel;
            currentFanSpeed = newSpeedLevel;
            SmartFan.setSpeed(newSpeedLevel, false);
          } else {
            expectedFanSpeed = matterSpeed;
            currentFanSpeed = matterSpeed;
          }
        }
        xSemaphoreGive(mutex);
      }

      Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
      commissioningState = COMMISSIONING_DONE;
      break;

    case COMMISSIONING_DONE:
      // Check if device got decommissioned
      if (!Matter.isDeviceCommissioned()) {
        // Device was decommissioned, restart the process
        commissioningState = COMMISSIONING_NOT_STARTED;
      }
      // Otherwise, normal operation - do nothing here
      break;
  }
}

void pulseFanSpeedControl() {
  if(isPulsing){
    // Non-blocking pulse state machine
    switch (pulseState) {
      case PULSE_IDLE:
        // Check if we need to pulse to reach expected speed
        if (expectedFanSpeed != currentFanSpeed) {
            // Calculate pulses needed (cyclic fan: 0->1->2->3->0)
            uint8_t pulsesNeeded = (expectedFanSpeed - currentFanSpeed + 4) % 4;

            if (pulsesNeeded > 0) {
            Serial.printf("Starting pulse sequence: %d pulses to reach speed %d from %d\r\n",
                            pulsesNeeded, expectedFanSpeed, currentFanSpeed);
            pulseState = PULSE_HIGH;
            digitalWrite(FAN_SPEED_CONTROL_PIN, HIGH);
            pulseStartTime = millis();
          }
        }
        break;

      case PULSE_HIGH:
        // Keep pin HIGH for 200ms
        if (millis() - pulseStartTime >= 200) {
          digitalWrite(FAN_SPEED_CONTROL_PIN, LOW);
          pulseState = PULSE_LOW;
          pulseStartTime = millis();
        }
        break;

      case PULSE_LOW:
        // Keep pin LOW for 100ms between pulses
        if (millis() - pulseStartTime >= 100) {
          // Pulsing complete - update current speed
          if (xSemaphoreTake(mutex, 0) == pdTRUE) {
            currentFanSpeed = (currentFanSpeed + 1) % 4; // Cycle through 0-3
            uint8_t pulsesNeeded = (expectedFanSpeed - currentFanSpeed + 4) % 4;

            if (pulsesNeeded > 0) {
              // More pulses needed - go to PULSE_HIGH
              pulseState = PULSE_HIGH;
              digitalWrite(FAN_SPEED_CONTROL_PIN, HIGH);
              pulseStartTime = millis();
            } else {
              isPulsing = false;
              pulseState = PULSE_IDLE;
              Serial.printf("Pulse sequence complete. Current speed now: %d\r\n", currentFanSpeed);
            }
            xSemaphoreGive(mutex);
          }
        }
        break;
    }
  }
}

void setup() {
  // Create mutex for thread safety
  mutex = xSemaphoreCreateMutex();

  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  // Initialize the FAN GPIO and Matter End Point
  pinMode(FAN_SPEED_CONTROL_PIN, OUTPUT);
  digitalWrite(FAN_SPEED_CONTROL_PIN, LOW);

  // Initialize input pins for monitoring (with pull-up resistors)
  pinMode(FAN_SPEED_LOW_INPUT_PIN, INPUT_PULLUP);
  pinMode(FAN_SPEED_MEDIUM_INPUT_PIN, INPUT_PULLUP);
  pinMode(FAN_SPEED_HIGH_INPUT_PIN, INPUT_PULLUP);

  Serial.begin(115200);

  // Print network interface configuration
  #if CONFIG_ENABLE_MATTER_OVER_WIFI
    Serial.println("===========================================");
    Serial.println("Matter Smart Fan - WiFi Configuration");
    Serial.println("Network: WiFi (802.11)");
    Serial.println("Chip: ESP32-C6 (Dual-band WiFi + BLE)");
    Serial.println("===========================================");
    // CONFIG_ENABLE_CHIPOBLE is enabled when BLE is used to commission the Matter Network
    Serial.print("Connecting to ");
    Serial.println(ssid);
    // Manually connect to WiFi
    WiFi.begin(ssid, password);
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\r\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    delay(500);
  #elif CONFIG_ENABLE_MATTER_OVER_THREAD
    Serial.println("===========================================");
    Serial.println("Matter Smart Fan - Thread Configuration");
    Serial.println("Network: Thread (802.15.4)");
    Serial.println("Chip: ESP32-H2 (Thread/Zigbee only)");
    Serial.println("===========================================");
  #else
    Serial.println("===========================================");
    Serial.println("Matter Smart Fan - Default Configuration");
    Serial.println("===========================================");
  #endif

  // Initialize Matter Multi-Speed Fan
  // speedMax = 3 (0=Off, 1=Low, 2=Medium, 3=High)
  // rockSupport = ROCK_LEFT_RIGHT (supports left-right oscillation)
  SmartFan.begin(3, ROCK_LEFT_RIGHT);

  // Register callbacks
  SmartFan.onChangeSpeed(onSpeedChange);
  SmartFan.onChangeRock(onRockChange);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();

  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    Serial.printf("Initial State :: Speed = %d, OnOff = %d, Rock = 0x%02X\r\n",
                  SmartFan.getSpeed(), SmartFan.getOnOff(), SmartFan.getRockSetting());

    // Update accessory to sync local state with Matter
    SmartFan.updateAccessory();

    // Set commissioning state to DONE since already commissioned
    commissioningState = COMMISSIONING_JUST_COMPLETED;
  }
}

void loop() {
  // Non-blocking commissioning handler
  handleCommissioning();

  // Only run these functions when commissioned
  if (commissioningState == COMMISSIONING_DONE) {
    pulseFanSpeedControl();
    syncFanSpeedBasedOnExternalInputs();
    handleDecommission();
    printStatusPeriodically();
  }
}