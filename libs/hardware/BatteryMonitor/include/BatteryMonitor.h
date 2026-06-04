#pragma once
#include <cstdint>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_oneshot.h>

class BatteryMonitor {
public:
    // adcUnit: shared ADC_UNIT_1 handle (owned by InputManager / HAL init)
    // adcChannel: the ADC channel number for the battery voltage pin
    // dividerMultiplier: voltage divider correction factor (default 2.0 for a 1:1 divider)
    explicit BatteryMonitor(adc_oneshot_unit_handle_t adcUnit, uint8_t adcChannel, float dividerMultiplier = 2.0f);
    ~BatteryMonitor();

    BatteryMonitor(const BatteryMonitor&) = delete;
    BatteryMonitor& operator=(const BatteryMonitor&) = delete;

    // Read voltage and return percentage (0-100)
    uint16_t readPercentage() const;

    // Read the battery voltage in millivolts (accounts for divider)
    uint16_t readMillivolts() const;

    // Read the battery voltage in volts (accounts for divider)
    double readVolts() const;

    // Percentage (0-100) from a millivolt value
    static uint16_t percentageFromMillivolts(uint16_t millivolts);

private:
    adc_oneshot_unit_handle_t _adcUnit;
    uint8_t _adcChannel;
    float _dividerMultiplier;
    adc_cali_handle_t _caliHandle;
    bool _caliEnabled;
};
