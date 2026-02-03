#ifndef MATTER_MULTI_SPEED_FAN_H
#define MATTER_MULTI_SPEED_FAN_H

#include <Matter.h>
#include <MatterEndPoint.h>

// Fan Speed Levels
enum FanSpeedLevel_t : uint8_t {
  FAN_SPEED_OFF = 0,
  FAN_SPEED_LOW = 1,
  FAN_SPEED_MEDIUM = 2,
  FAN_SPEED_HIGH = 3
};

// Rock Support Bitmap (Matter Spec 7.4.5.6)
enum RockSupportBitmap : uint8_t {
  ROCK_LEFT_RIGHT = 0x01,   // Bit 0: Supports left-right rocking
  ROCK_UP_DOWN = 0x02,      // Bit 1: Supports up-down rocking
  ROCK_ROUND = 0x04         // Bit 2: Supports circular rocking
};

// Rock Setting Bitmap (Matter Spec 7.4.5.7)
enum RockSettingBitmap : uint8_t {
  ROCK_SETTING_LEFT_RIGHT = 0x01,   // Bit 0: Left-right rocking enabled
  ROCK_SETTING_UP_DOWN = 0x02,      // Bit 1: Up-down rocking enabled
  ROCK_SETTING_ROUND = 0x04         // Bit 2: Circular rocking enabled
};

// FanControl Cluster FeatureMap (Matter Spec 7.4.4)
// This implementation sets FeatureMap = 0x05 (MultiSpeed | Rocking)
enum FanControlFeatureBitmap : uint32_t {
  FEATURE_MULTI_SPEED = 0x01,       // Bit 0: Supports SpeedMax, SpeedSetting, SpeedCurrent
  FEATURE_AUTO = 0x02,              // Bit 1: Supports automatic mode
  FEATURE_ROCKING = 0x04,           // Bit 2: Supports RockSupport, RockSetting
  FEATURE_WIND = 0x08,              // Bit 3: Supports wind feature
  FEATURE_STEP = 0x10,              // Bit 4: Supports step command
  FEATURE_AIRFLOW_DIRECTION = 0x20  // Bit 5: Supports airflow direction
};

// Callback types
typedef std::function<bool(uint8_t speed)> SpeedChangeCallback;
typedef std::function<bool(uint8_t rockSetting)> RockChangeCallback;

class MatterMultiSpeedFan : public MatterEndPoint {
public:
  MatterMultiSpeedFan();
  ~MatterMultiSpeedFan();

  // Initialize the fan with multi-speed support and rock capability
  // speedMax: Maximum speed level (default 3 for Off, Low, Medium, High)
  // rockSupport: Bitmap of supported rock directions (default: left-right)
  bool begin(uint8_t speedMax = 3, uint8_t rockSupport = ROCK_LEFT_RIGHT);

  // Cleanup
  void end();

  // Speed control methods
  bool setSpeed(uint8_t speed, bool performUpdate = true);
  uint8_t getSpeed();
  uint8_t getSpeedMax();

  // On/Off control (convenience methods)
  bool setOnOff(bool onOff, bool performUpdate = true);
  bool getOnOff();
  bool toggle(bool performUpdate = true);

  // Rock/Oscillation control methods
  bool setRockSetting(uint8_t rockSetting, bool performUpdate = true);
  uint8_t getRockSetting();
  uint8_t getRockSupport();
  bool isRocking();

  // Callbacks
  void onChangeSpeed(SpeedChangeCallback cb);
  void onChangeRock(RockChangeCallback cb);

  // Update the accessory state
  void updateAccessory();

  // Matter endpoint callback (called by Matter framework)
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) override;

  // Operator overload to allow direct fan speed setting
  MatterMultiSpeedFan &operator=(uint8_t speed);

  // Decommission warning
  static void __attribute__((deprecated("\n\n'decommission()' is deprecated. \nPlease use 'Matter.decommission()' instead.\n"))) decommission();

protected:
  bool started = false;
  uint8_t currentSpeed = 0;          // Current speed level (0-speedMax)
  uint8_t speedMax = 3;              // Maximum speed level
  uint8_t rockSupport = 0;           // Bitmap of supported rock directions
  uint8_t currentRockSetting = 0;    // Current rock setting bitmap

  SpeedChangeCallback _onChangeSpeedCB = nullptr;
  RockChangeCallback _onChangeRockCB = nullptr;
};

#endif // MATTER_MULTI_SPEED_FAN_H
