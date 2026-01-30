#include "MatterFanOscillation.h"

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace chip::app::Clusters;

MatterFanOscillation::MatterFanOscillation() {}

MatterFanOscillation::~MatterFanOscillation() {
  end();
}

bool MatterFanOscillation::begin(uint8_t percent, FanMode_t fanMode, FanModeSequence_t fanModeSeq) {
  // Call parent begin
  if (!MatterFan::begin(percent, fanMode, fanModeSeq)) {
    return false;
  }

  on_off::config_t onoff_config;
  onoff_config.on_off = false;
  cluster_t *onoff_cluster = on_off::create(endpoint::get(getEndPointId()), &onoff_config, CLUSTER_FLAG_SERVER, 0);

  return true;
}

bool MatterFanOscillation::attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) {
  // Call parent callback first
  bool ret = MatterFan::attributeChangeCB(endpoint_id, cluster_id, attribute_id, val);

  if (endpoint_id == getEndPointId() && cluster_id == FanControl::Id) {
    if (attribute_id == FanControl::Attributes::PercentSetting::Id) {
      attribute::report(getEndPointId(), FanControl::Id, FanControl::Attributes::PercentSetting::Id, val);
    }
    if (attribute_id == FanControl::Attributes::PercentCurrent::Id) {
      attribute::report(getEndPointId(), FanControl::Id, FanControl::Attributes::PercentCurrent::Id, val);
    }
  }

  if (endpoint_id == getEndPointId() && cluster_id == FanControl::Id) {
    if (attribute_id == FanControl::Attributes::SpeedSetting::Id) {
      Serial.println("Reporting SpeedSetting");
    }
    if (attribute_id == FanControl::Attributes::SpeedCurrent::Id) {
      Serial.println("Reporting SpeedCurrent");
    }
  }

  if (endpoint_id == getEndPointId() && cluster_id == OnOff::Id) {
    if (attribute_id == OnOff::Attributes::OnOff::Id) {
      log_v("OnOff changed to %d", val->val.u8);
      bool newState = (val->val.b == true);
      if (_onChangeFanStateCB != nullptr) {
        ret &= _onChangeFanStateCB(newState);
      }
      if (ret == true) {
        currentFanState = newState;
      }
    }
  }

  return ret;
}

void MatterFanOscillation::onChangeFanState(std::function<bool(bool)> cb) {
  _onChangeFanStateCB = cb;
}

bool MatterFanOscillation::setFanState(bool newState, bool performUpdate) {
  if (!started) {
    log_w("Matter Fan device has not begun.");
    return false;
  }
  // Avoid processing if there was no change
  if (currentFanState == newState) {
    return true;
  }

  esp_matter_attr_val_t onOffVal = esp_matter_invalid(NULL);
  if (!getAttributeVal(OnOff::Id, OnOff::Attributes::OnOff::Id, &onOffVal)) {
    log_e("Failed to get OnOff attribute.");
    return false;
  }

  if (onOffVal.val.b != newState) {
    onOffVal.val.b = newState;
    bool ret;
    if (performUpdate) {
      ret = updateAttributeVal(OnOff::Id, OnOff::Attributes::OnOff::Id, &onOffVal);
    } else {
      ret = setAttributeVal(OnOff::Id, OnOff::Attributes::OnOff::Id, &onOffVal);
    }
    if (!ret) {
      log_e("Failed to %s OnOff attribute.", performUpdate ? "update" : "set");
      return false;
    }
  }
  currentFanState = newState;
  log_v("Fan state %s to %s", performUpdate ? "updated" : "set", currentFanState ? "ON" : "OFF");
  return true;
}

bool MatterFanOscillation::getFanState() {
  return currentFanState;
}