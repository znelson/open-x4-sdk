#include "InputManager.h"
#include <driver/gpio.h>
#include <esp_timer.h>

// Recorded ADC values from real devices
// BACK CONF LEFT RGHT   UP DOWN
// 3597 2760 1530    6 2300    6
// 3470 2666 1480    6 2222    5
// 3470 2655 1470    3 2205    3

// Averages
// BACK CONF LEFT RGHT   UP DOWN
// 3512 2694 1493    5 2242    5

// Setup ranges, if ADC value is between value `i` and `i + 1`, button `i` is being pressed
// These ranges are based on real world values above, and are much more tolerant of different
// devices than a fixed threshold check
// These values are calculated by taking the midpoint of the pairs of averaged values above
const int InputManager::ADC_RANGES_1[] = {ADC_NO_BUTTON, 3100, 2090, 750, INT32_MIN};
const int InputManager::ADC_RANGES_2[] = {ADC_NO_BUTTON, 1120, INT32_MIN};
const char* InputManager::BUTTON_NAMES[] = {"Back", "Confirm", "Left", "Right", "Up", "Down", "Power"};

InputManager::InputManager()
    : currentState(0),
      lastState(0),
      pressedEvents(0),
      releasedEvents(0),
      lastDebounceTime(0),
      buttonPressStart(0),
      buttonPressFinish(0),
      powerButtonPressStart(0),
      powerButtonPressFinish(0) {}

InputManager::~InputManager() {
    if (_adcUnit) {
        adc_oneshot_del_unit(_adcUnit);
    }
}

void InputManager::begin() {
    const adc_oneshot_unit_init_cfg_t unitCfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&unitCfg, &_adcUnit);

    const adc_oneshot_chan_cfg_t chanCfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(_adcUnit, static_cast<adc_channel_t>(BUTTON_ADC_PIN_1), &chanCfg);
    adc_oneshot_config_channel(_adcUnit, static_cast<adc_channel_t>(BUTTON_ADC_PIN_2), &chanCfg);

    const gpio_config_t powerBtnCfg = {
        .pin_bit_mask = (1ULL << POWER_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&powerBtnCfg);
}

int InputManager::getButtonFromADC(const int adcValue, const int ranges[], const int numButtons) {
  for (int i = 0; i < numButtons; i++) {
    if (ranges[i + 1] < adcValue && adcValue <= ranges[i]) {
      return i;
    }
  }

  return -1;
}

uint8_t InputManager::getState() {
  uint8_t state = 0;

  // Read GPIO1 buttons
  int adcValue1 = 0;
  adc_oneshot_read(_adcUnit, static_cast<adc_channel_t>(BUTTON_ADC_PIN_1), &adcValue1);
  const int button1 = getButtonFromADC(adcValue1, ADC_RANGES_1, NUM_BUTTONS_1);
  if (button1 >= 0) {
    state |= (1 << button1);
  }

  // Read GPIO2 buttons
  int adcValue2 = 0;
  adc_oneshot_read(_adcUnit, static_cast<adc_channel_t>(BUTTON_ADC_PIN_2), &adcValue2);
  const int button2 = getButtonFromADC(adcValue2, ADC_RANGES_2, NUM_BUTTONS_2);
  if (button2 >= 0) {
    state |= (1 << (button2 + 4));
  }

  // Read power button (digital, active LOW)
  if (gpio_get_level((gpio_num_t)POWER_BUTTON_PIN) == 0) {
    state |= (1 << BTN_POWER);
  }

  return state;
}

static inline uint32_t now_ms() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void InputManager::update() {
  const uint32_t currentTime = now_ms();
  const uint8_t state = getState();

  // Always clear events first
  pressedEvents = 0;
  releasedEvents = 0;

  // Debounce
  if (state != lastState) {
    lastDebounceTime = currentTime;
    lastState = state;
  }

  if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (state != currentState) {
      // Calculate pressed and released events
      pressedEvents = state & ~currentState;
      releasedEvents = currentState & ~state;

      // If pressing buttons and wasn't before, start recording time
      if (pressedEvents > 0 && currentState == 0) {
        buttonPressStart = currentTime;
      }

      // If releasing a button and no other buttons being pressed, record finish time
      if (releasedEvents > 0 && state == 0) {
        buttonPressFinish = currentTime;
      }

      // Track power button press time separately
      if (pressedEvents & (1 << BTN_POWER)) {
        powerButtonPressStart = currentTime;
      }

      // Track power button release
      if (releasedEvents & (1 << BTN_POWER)) {
        powerButtonPressFinish = currentTime;
      }

      currentState = state;
    }
  }
}

bool InputManager::isPressed(const uint8_t buttonIndex) const {
  return currentState & (1 << buttonIndex);
}

bool InputManager::wasPressed(const uint8_t buttonIndex) const {
  return pressedEvents & (1 << buttonIndex);
}

bool InputManager::wasAnyPressed() const {
  return pressedEvents > 0;
}

bool InputManager::wasReleased(const uint8_t buttonIndex) const {
  return releasedEvents & (1 << buttonIndex);
}

bool InputManager::wasAnyReleased() const {
  return releasedEvents > 0;
}

uint32_t InputManager::getHeldTime() const {
  // Still holding a button
  if (currentState > 0) {
    return now_ms() - buttonPressStart;
  }

  return buttonPressFinish - buttonPressStart;
}

uint32_t InputManager::getPowerButtonHeldTime() const {
  // Power button is currently pressed
  if (isPressed(BTN_POWER)) {
    return now_ms() - powerButtonPressStart;
  }

  // Power button was released
  return powerButtonPressFinish - powerButtonPressStart;
}

const char* InputManager::getButtonName(const uint8_t buttonIndex) {
  if (buttonIndex <= BTN_POWER) {
    return BUTTON_NAMES[buttonIndex];
  }
  return "Unknown";
}

bool InputManager::isPowerButtonPressed() const {
  return isPressed(BTN_POWER);
}
