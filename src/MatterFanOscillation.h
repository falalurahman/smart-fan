#ifndef MATTER_FAN_OSCILLATION_H
#define MATTER_FAN_OSCILLATION_H

#include <MatterEndpoints/MatterFan.h>

class MatterFanOscillation : public MatterFan {
public:
  MatterFanOscillation();
  ~MatterFanOscillation();

  // Override begin to add OnOff cluster for oscillation
  bool begin(uint8_t percent, FanMode_t fanMode, FanModeSequence_t fanModeSeq);

  // Override attributeChangeCB to handle oscillation
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

  // New methods for oscillation
  void onChangeFanState(std::function<bool(bool)> cb);
  bool setFanState(bool newState, bool performUpdate = true);
  bool getFanState();

private:
  bool currentFanState = false;
  std::function<bool(bool)> _onChangeFanStateCB = nullptr;
};

#endif // MATTER_FAN_OSCILLATION_H