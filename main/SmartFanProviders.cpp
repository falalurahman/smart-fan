#include "SmartFanProviders.h"
#include <Arduino.h>
#include <cstring>

// Static instance (must survive for the lifetime of the process)
static SmartFanDeviceInstanceInfoProvider sDeviceInstanceInfoProvider;

// ============================================================================
// Helper
// ============================================================================
static CHIP_ERROR CopyString(const char * src, char * buf, size_t bufSize)
{
    size_t len = strlen(src);
    if (len >= bufSize) {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    strncpy(buf, src, bufSize);
    buf[bufSize - 1] = '\0';
    return CHIP_NO_ERROR;
}

// ============================================================================
// DeviceInstanceInfoProvider implementation
// ============================================================================

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetVendorName(char * buf, size_t bufSize)
{
    return CopyString(SMART_FAN_VENDOR_NAME, buf, bufSize);
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetVendorId(uint16_t & vendorId)
{
    vendorId = SMART_FAN_VENDOR_ID;
    return CHIP_NO_ERROR;
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetProductName(char * buf, size_t bufSize)
{
    return CopyString(SMART_FAN_PRODUCT_NAME, buf, bufSize);
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetProductId(uint16_t & productId)
{
    productId = SMART_FAN_PRODUCT_ID;
    return CHIP_NO_ERROR;
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetPartNumber(char * buf, size_t bufSize)
{
    return CopyString(SMART_FAN_PART_NUMBER, buf, bufSize);
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetProductURL(char * buf, size_t bufSize)
{
    return CopyString(SMART_FAN_PRODUCT_URL, buf, bufSize);
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetProductLabel(char * buf, size_t bufSize)
{
    return CopyString(SMART_FAN_PRODUCT_LABEL, buf, bufSize);
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetSerialNumber(char * buf, size_t bufSize)
{
    return CopyString(SMART_FAN_SERIAL_NUMBER, buf, bufSize);
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day)
{
    year  = SMART_FAN_MFG_YEAR;
    month = SMART_FAN_MFG_MONTH;
    day   = SMART_FAN_MFG_DAY;
    return CHIP_NO_ERROR;
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetHardwareVersion(uint16_t & hardwareVersion)
{
    hardwareVersion = SMART_FAN_HW_VERSION;
    return CHIP_NO_ERROR;
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetHardwareVersionString(char * buf, size_t bufSize)
{
    return CopyString(SMART_FAN_HW_VERSION_STRING, buf, bufSize);
}

CHIP_ERROR SmartFanDeviceInstanceInfoProvider::GetRotatingDeviceIdUniqueId(chip::MutableByteSpan & uniqueIdSpan)
{
    // 16-byte unique ID - in production this should be truly unique per device
    static const uint8_t kUniqueId[16] = {
        0xFA, 0x1A, 0x10, 0x50, 0xAA, 0x77, 0x08, 0xE0,
        0xFA, 0x4E, 0xD3, 0xD1, 0xC3, 0x00, 0x00, 0x01
    };
    if (uniqueIdSpan.size() < sizeof(kUniqueId)) {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    memcpy(uniqueIdSpan.data(), kUniqueId, sizeof(kUniqueId));
    uniqueIdSpan.reduce_size(sizeof(kUniqueId));
    return CHIP_NO_ERROR;
}

// ============================================================================
// Initialization
// ============================================================================

void initSmartFanProviders()
{
    // Override the DeviceInstanceInfoProvider (vendor name, product name, serial, etc.)
    chip::DeviceLayer::SetDeviceInstanceInfoProvider(&sDeviceInstanceInfoProvider);

    // Keep the ExampleDAC provider for attestation (VID 0xFFF1 works with Apple Home)
    // Keep the default CommissionableDataProvider (passcode 20202021, discriminator 3840)

    Serial.println("Custom device info provider installed.");
    Serial.printf("  Vendor:  %s (0x%04X)\n", SMART_FAN_VENDOR_NAME, SMART_FAN_VENDOR_ID);
    Serial.printf("  Product: %s (0x%04X)\n", SMART_FAN_PRODUCT_NAME, SMART_FAN_PRODUCT_ID);
    Serial.printf("  Serial:  %s\n", SMART_FAN_SERIAL_NUMBER);
}
