#include <Arduino.h>
#include <Matter.h>

MatterFan SmartFan;

const uint8_t fanPin = 4;
const uint8_t buttonPin = BOOT_PIN;

// Button control
uint32_t button_time_stamp = 0;                // debouncing control
bool button_state = false;                     // false = released | true = pressed
const uint32_t debouceTime = 250;              // button debouncing time (ms)
const uint32_t decommissioningTimeout = 5000;  // keep the button pressed for 5s, or longer, to decommission

// Function to pulse fanPin for speed control
void pulseFan(int pulses) {
  for (int i = 0; i < pulses; i++) {
    digitalWrite(fanPin, HIGH);
    delay(200);
    digitalWrite(fanPin, LOW);
    if (i < pulses - 1) delay(100);  // delay between pulses
  }
}

// Matter Protocol Endpoint Callback
bool setFanState(MatterFan::FanMode_t newMode, uint8_t newSpeedPercent) {
  Serial.printf("User Callback :: New Mode = %d , New Speed = %d\r\n", newMode, newSpeedPercent);

  uint8_t currentOnOff = SmartFan.getOnOff();
  uint8_t currentMode = SmartFan.getMode();
  uint8_t currentSpeed = SmartFan.getSpeedPercent();
  Serial.printf("Current State :: On/Off = %d, Mode = %d, Speed = %d\r\n", currentOnOff, currentMode, currentSpeed);

  if (newMode == currentMode && newSpeedPercent == currentSpeed) {
    return true;
  }

  if (newMode == currentMode 
      && newSpeedPercent != currentSpeed
      && newSpeedPercent != 0
      && newSpeedPercent != 33
      && newSpeedPercent != 66
      && newSpeedPercent != 100) {
    newSpeedPercent = newSpeedPercent > 66 ? 100 : newSpeedPercent > 33 ? 66 : newSpeedPercent > 0 ? 33 : 0;
  }

  if (newMode == currentMode 
      && newSpeedPercent != currentSpeed) {
    uint8_t newModeInt = newSpeedPercent / 33; // Map 0-100% to 0-3
    newMode = static_cast<MatterFan::FanMode_t>(newModeInt);
  }

  newMode > 0 ? SmartFan.setOnOff(true, false) : SmartFan.setOnOff(false, false);
  SmartFan.setMode(newMode, false);
  uint8_t speedPercent = newMode == MatterFan::FAN_MODE_OFF ? 0 : newMode == MatterFan::FAN_MODE_HIGH ? 100 : newMode * 33;
  SmartFan.setSpeedPercent(speedPercent, false);

  int pulses = (newMode - currentMode + 4) % 4;
  if (pulses > 0) {
    pulseFan(pulses);
  }

  Serial.printf("New State :: On/Off = %d, Mode = %d, Speed = %d\r\n", SmartFan.getOnOff(), SmartFan.getMode(), SmartFan.getSpeedPercent());
  // This callback must return the success state to Matter core
  return true;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) GPIO that will act as a toggle switch
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize the FAN GPIO and Matter End Point
  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, LOW);

  Serial.begin(115200);

  // Initialize Matter EndPoint
  SmartFan.begin(0, MatterFan::FAN_MODE_OFF, MatterFan::FAN_MODE_SEQ_OFF_LOW_MED_HIGH);
  SmartFan.onChange(setFanState);

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  // This may be a restart of a already commissioned Matter accessory
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
    Serial.printf("Initial State :: On/Off = %d, Mode = %d, Speed = %d\r\n", SmartFan.getOnOff(), SmartFan.getMode(), SmartFan.getSpeedPercent());
    SmartFan.updateAccessory();  // configure the Fan based on initial speed
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
    Serial.printf("Initial State :: On/Off = %d, Mode = %d, Speed = %d\r\n", SmartFan.getOnOff(), SmartFan.getMode(), SmartFan.getSpeedPercent());
    SmartFan.updateAccessory();  // configure the Fan based on initial speed
    Serial.println("Matter Node is commissioned and connected to the network. Ready for use.");
  }

  uint8_t currentSpeedPercent = SmartFan.getSpeedPercent();
  if (currentSpeedPercent != 0
      && currentSpeedPercent != 33
      && currentSpeedPercent != 66
      && currentSpeedPercent != 100) {
    uint8_t newSpeedPercent = currentSpeedPercent > 66 ? 100 : currentSpeedPercent > 33 ? 66 : currentSpeedPercent > 0 ? 33 : 0;
    SmartFan.setSpeedPercent(newSpeedPercent);
  }

  if(millis() - lastPrintingTime >= 10000) {
    lastPrintingTime = millis();
    Serial.printf("Matter Fan Accessory State :: On/Off = %d, Mode = %d, Speed = %d\r\n", SmartFan.getOnOff(), SmartFan.getMode(), SmartFan.getSpeedPercent());
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
    SmartFan.setMode(MatterFan::FAN_MODE_OFF);  // turn the fan off
    Matter.decommission();
    button_time_stamp = millis();  // avoid running decommissioning again, reboot takes a second or so
  }


}