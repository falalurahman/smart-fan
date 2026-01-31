#include <Arduino.h>
#include <Matter.h>
#include <MatterMultiSpeedFan.h>
#if CONFIG_ENABLE_MATTER_OVER_WIFI 
#include <WiFi.h>
#endif

MatterMultiSpeedFan SmartFan;

const uint8_t fanSpeedPin = 4;
const uint8_t buttonPin = BOOT_PIN;

// Input pins to monitor (only when commissioned)
const uint8_t inputPin1 = 5;
const uint8_t inputPin2 = 6;
const uint8_t inputPin3 = 7;

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debouceTime = 250;              // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission
uint8_t lastSpeedLevel = 0;                    // Track last speed level for pulse calculation

#if CONFIG_ENABLE_MATTER_OVER_WIFI
// WiFi is manually set and started
const char *ssid = "Hyperoptic Fibre 91B3";          // Change this to your WiFi SSID
const char *password = "J47MkaB84J4Eju";  // Change this to your WiFi password
#endif

SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

// Function to pulse fanSpeedPin for speed control
void pulseFanSpeed(int pulses) {
  for (int i = 0; i < pulses; i++) {
    digitalWrite(fanSpeedPin, HIGH);
    delay(200);
    digitalWrite(fanSpeedPin, LOW);
    if (i < pulses - 1) delay(100);  // delay between pulses
  }
}

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
    // Calculate pulses needed to reach new speed from last known speed
    int pulses = (newSpeed - lastSpeedLevel + 4) % 4;
    if (pulses > 0) {
      Serial.printf("Pulsing fan %d times to reach speed level %d\r\n", pulses, newSpeed);
      pulseFanSpeed(pulses);
    }

    // Update last known speed
    lastSpeedLevel = newSpeed;
    delay(100); // Small delay to ensure state is stable
    Serial.printf("New speed level set to %d\r\n", lastSpeedLevel);

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

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the FAN GPIO and Matter End Point
  pinMode(fanSpeedPin, OUTPUT);
  digitalWrite(fanSpeedPin, LOW);

  // Initialize input pins for monitoring (with pull-up resistors)
  pinMode(inputPin1, INPUT_PULLUP);
  pinMode(inputPin2, INPUT_PULLUP);
  pinMode(inputPin3, INPUT_PULLUP);

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

    if(xSemaphoreTake(mutex, 0) == pdTRUE) {
      uint8_t currentSpeed = SmartFan.getSpeed();
      lastSpeedLevel = currentSpeed;

      uint8_t newSpeedLevel = 0;
      // Initialize input pin states
      if (digitalRead(inputPin1) == LOW) {
        newSpeedLevel = 1;
      }
      if (digitalRead(inputPin2) == LOW) {
        newSpeedLevel = 2;
      }
      if (digitalRead(inputPin3) == LOW) {
        newSpeedLevel = 3;
      }
      if(lastSpeedLevel != newSpeedLevel) {
        Serial.printf("LED Input Pin: Setting speed level to %d\r\n", newSpeedLevel);
        lastSpeedLevel = newSpeedLevel; // Update last speed level if it was OFF
        SmartFan.setSpeed(newSpeedLevel, false); // Ensure Matter state is consistent
      }
      xSemaphoreGive(mutex);
    }
  }
}

unsigned long lastPrintingTime = 0;

void loop() {
  // Check Matter Fan Commissioning state, which may change during execution of loop()
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("");
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
    // waits for Matter Fan Commissioning.
    uint32_t timeCount = 0;
    while (!Matter.isDeviceCommissioned()) {
      delay(100);
      if ((timeCount++ % 50) == 0) {  // 50*100ms = 5 sec
        Serial.printf("Matter Node not commissioned yet. Waiting for commissioning. Commissioning code: %s\r\n", Matter.getManualPairingCode().c_str());
      }
    }
    Serial.printf("Initial State :: Speed = %d, OnOff = %d, Rock = 0x%02X\r\n",
                  SmartFan.getSpeed(), SmartFan.getOnOff(), SmartFan.getRockSetting());
    SmartFan.updateAccessory();  // configure the Fan based on initial speed

    uint8_t currentSpeed = SmartFan.getSpeed();
    lastSpeedLevel = currentSpeed;

    if(xSemaphoreTake(mutex, 0) == pdTRUE) {
      uint8_t newSpeedLevel = 0;
      // Initialize input pin states
      if (digitalRead(inputPin1) == LOW) {
        newSpeedLevel = 1;
      }
      if (digitalRead(inputPin2) == LOW) {
        newSpeedLevel = 2;
      }
      if (digitalRead(inputPin3) == LOW) {
        newSpeedLevel = 3;
      }
      if(lastSpeedLevel != newSpeedLevel) {
        Serial.printf("LED Input Pin: Setting speed level to %d\r\n", newSpeedLevel);
        lastSpeedLevel = newSpeedLevel; // Update last speed level if it was OFF
        SmartFan.setSpeed(newSpeedLevel, false); // Ensure Matter state is consistent
      }
      xSemaphoreGive(mutex);
    }

    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  if(millis() - lastPrintingTime >= 10000) {
    lastPrintingTime = millis();
    Serial.printf("Matter Fan State :: Speed = %d, OnOff = %d, Rocking = %d (0x%02X)\r\n",
                  SmartFan.getSpeed(), SmartFan.getOnOff(), SmartFan.isRocking(), SmartFan.getRockSetting());
  }


  if(xSemaphoreTake(mutex, 0) == pdTRUE) {
    uint8_t newSpeedLevel = 0;
    // Monitor input pins only when commissioned
    if (digitalRead(inputPin1) == LOW) {
      newSpeedLevel = 1;
    }
    if (digitalRead(inputPin2) == LOW) {
      newSpeedLevel = 2;
    }
    if (digitalRead(inputPin3) == LOW) {
      newSpeedLevel = 3;
    }
    if(lastSpeedLevel != newSpeedLevel) {
      Serial.printf("Loop LED Input Pin: Setting speed level to %d\r\n", newSpeedLevel);
      lastSpeedLevel = newSpeedLevel; // Update last speed level if it was OFF
      SmartFan.setSpeed(newSpeedLevel, false); // Ensure Matter state is consistent
    }
    xSemaphoreGive(mutex);
  }

  // A button is also used to control the fan
  // Check if the button has been pressed
  if (digitalRead(buttonPin) == LOW && !button_state) {
    // deals with button debouncing
    button_time_stamp = millis();  // record the time while the button is pressed.
    button_state = true;           // pressed.
  }

  // Onboard User Button is used as a Fan speed cycle switch or to decommission it
  uint32_t time_diff = millis() - button_time_stamp;

  // Onboard User Button is kept pressed for longer than 5 seconds in order to decommission matter node
  if (button_state && time_diff > decommissioningTimeout) {
    Serial.println("Decommissioning the Fan Matter Accessory. It shall be commissioned again.");
    SmartFan.setSpeed(0);  // Turn off fan
    SmartFan.setRockSetting(0);  // Turn off oscillation
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }

  // Short button press: cycle through fan speeds (OFF -> LOW -> MEDIUM -> HIGH -> OFF)
  if (button_state && digitalRead(buttonPin) == HIGH && time_diff > debouceTime && time_diff < decommissioningTimeout) {
    button_state = false;  // Button released

    uint8_t currentSpeed = SmartFan.getSpeed();
    uint8_t newSpeed;

    // Cycle through discrete speed levels
    switch (currentSpeed) {
      case FAN_SPEED_OFF:
        newSpeed = FAN_SPEED_LOW;
        break;
      case FAN_SPEED_LOW:
        newSpeed = FAN_SPEED_MEDIUM;
        break;
      case FAN_SPEED_MEDIUM:
        newSpeed = FAN_SPEED_HIGH;
        break;
      case FAN_SPEED_HIGH:
      default:
        newSpeed = FAN_SPEED_OFF;
        break;
    }

    Serial.printf("Button pressed: cycling speed from %d to %d\r\n", currentSpeed, newSpeed);

    // Update Matter attributes - this will trigger attribute reports to controllers
    SmartFan.setSpeed(newSpeed, false);

    // Update last known speed
    lastSpeedLevel = newSpeed;
  }

}