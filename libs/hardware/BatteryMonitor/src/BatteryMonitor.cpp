#include "BatteryMonitor.h"
#include <esp_adc/adc_cali_scheme.h>
#include <algorithm>
#include <cmath>

BatteryMonitor::BatteryMonitor(adc_oneshot_unit_handle_t adcUnit, uint8_t adcChannel, float dividerMultiplier)
    : _adcUnit(adcUnit), _adcChannel(adcChannel), _dividerMultiplier(dividerMultiplier),
      _caliHandle(nullptr), _caliEnabled(false)
{
    adc_oneshot_chan_cfg_t chanCfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(_adcUnit, static_cast<adc_channel_t>(_adcChannel), &chanCfg);

    adc_cali_curve_fitting_config_t caliCfg = {
        .unit_id = ADC_UNIT_1,
        .chan = static_cast<adc_channel_t>(_adcChannel),
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    _caliEnabled = (adc_cali_create_scheme_curve_fitting(&caliCfg, &_caliHandle) == ESP_OK);
}

BatteryMonitor::~BatteryMonitor()
{
    if (_caliEnabled && _caliHandle) {
        adc_cali_delete_scheme_curve_fitting(_caliHandle);
    }
}

uint16_t BatteryMonitor::readPercentage() const
{
    return percentageFromMillivolts(readMillivolts());
}

uint16_t BatteryMonitor::readMillivolts() const
{
    if (!_adcUnit) {
        return 0;
    }
    int raw = 0;
    adc_oneshot_read(_adcUnit, static_cast<adc_channel_t>(_adcChannel), &raw);

    int mv = 0;
    if (_caliEnabled) {
        adc_cali_raw_to_voltage(_caliHandle, raw, &mv);
    } else {
        mv = (raw * 3100) / 4095;
    }

    return static_cast<uint16_t>(mv * _dividerMultiplier);
}

double BatteryMonitor::readVolts() const
{
    return static_cast<double>(readMillivolts()) / 1000.0;
}

uint16_t BatteryMonitor::percentageFromMillivolts(uint16_t millivolts)
{
    double volts = millivolts / 1000.0;
    // Polynomial derived from LiPo samples
    double y = -144.9390 * volts * volts * volts +
               1655.8629 * volts * volts -
               6158.8520 * volts +
               7501.3202;

    y = std::max(y, 0.0);
    y = std::min(y, 100.0);
    y = round(y);
    return static_cast<uint16_t>(y);
}
