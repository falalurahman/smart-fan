#pragma once

#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>

// ============================================================================
// Device Info Configuration - customize these values
// ============================================================================
#define SMART_FAN_VENDOR_NAME       "Falalu Smart Home"
#define SMART_FAN_VENDOR_ID         0xFFF1          // Test VID (matches ExampleDAC)
#define SMART_FAN_PRODUCT_NAME      "Matter Smart Fan"
#define SMART_FAN_PRODUCT_ID        0x8000          // Matches pre-compiled sdkconfig
#define SMART_FAN_SERIAL_NUMBER     "SF-2026-002"
#define SMART_FAN_HW_VERSION        1
#define SMART_FAN_HW_VERSION_STRING "v1.0"
#define SMART_FAN_MFG_YEAR          2026
#define SMART_FAN_MFG_MONTH         2
#define SMART_FAN_MFG_DAY           1
#define SMART_FAN_PRODUCT_URL       ""
#define SMART_FAN_PRODUCT_LABEL     "Smart Fan"
#define SMART_FAN_PART_NUMBER       ""

// ============================================================================
// Custom Device Instance Info Provider
// ============================================================================
class SmartFanDeviceInstanceInfoProvider : public chip::DeviceLayer::DeviceInstanceInfoProvider
{
public:
    CHIP_ERROR GetVendorName(char * buf, size_t bufSize) override;
    CHIP_ERROR GetVendorId(uint16_t & vendorId) override;
    CHIP_ERROR GetProductName(char * buf, size_t bufSize) override;
    CHIP_ERROR GetProductId(uint16_t & productId) override;
    CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override;
    CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override;
    CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override;
    CHIP_ERROR GetSerialNumber(char * buf, size_t bufSize) override;
    CHIP_ERROR GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day) override;
    CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion) override;
    CHIP_ERROR GetHardwareVersionString(char * buf, size_t bufSize) override;
    CHIP_ERROR GetRotatingDeviceIdUniqueId(chip::MutableByteSpan & uniqueIdSpan) override;
};

// ============================================================================
// Initialization - call AFTER Matter.begin()
// ============================================================================
void initSmartFanProviders();
