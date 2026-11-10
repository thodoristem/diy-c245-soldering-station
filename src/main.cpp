#include <Arduino.h>
#include <Iron/Iron.h>
#include <UI/UserInterface.h>
#include <RotaryEncoder.h>
#include <OneButton.h>
#include <U8g2lib.h>

#define HEATER_OUPUT_PIN PA8
#define COLD_JUNCTION_TEMP_PIN PB0
#define HEATER_TEMP_PIN PB1
#define BEEPER_PIN PA0

#define ENC_BUTTON_PIN PA1
#define ENC_A_PIN PA2
#define ENC_B_PIN PA3

#define BTN_DEBOUNCE_MS 10
#define BTN_LONG_PRESS_MS 1000
#define BTN_CLICK_MS 5
#define BTN_PRESS_MS 100

Iron iron(
    HEATER_OUPUT_PIN,
    HEATER_TEMP_PIN,
    COLD_JUNCTION_TEMP_PIN,
    BEEPER_PIN,
    false);

U8G2_ST7920_128X64_F_SW_SPI lcd(
    U8G2_R0,
    PA5, // Clock / SCLK
    PA7, // Data / SID
    PA4, // Chip select / RS
    U8X8_PIN_NONE);

UserInterface ui(&iron, &lcd);
RotaryEncoder encoder(ENC_A_PIN, ENC_B_PIN, RotaryEncoder::LatchMode::FOUR3);
OneButton button;

void encoderISR()
{
    encoder.tick();
}

void encoderLongPress()
{
    ui.onEncLongPress();
}

void encoderPress()
{
    ui.onEncPress();
}

void setup()
{
    iron.init();
    ui.init();

    button.setup(ENC_BUTTON_PIN, INPUT_PULLUP, true);
    button.setDebounceMs(BTN_DEBOUNCE_MS);
    button.setPressMs(BTN_LONG_PRESS_MS);
    button.setClickMs(BTN_CLICK_MS);
    button.attachLongPressStart(encoderLongPress);
    button.attachClick(encoderPress);

    attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), encoderISR, CHANGE);
}

void loop()
{
    button.tick();

    switch (encoder.getDirection())
    {
    case RotaryEncoder::Direction::CLOCKWISE:
        ui.onEncPrev();
        break;
    case RotaryEncoder::Direction::COUNTERCLOCKWISE:
        ui.onEncNext();
        break;
    case RotaryEncoder::Direction::NOROTATION:
        break;
    }

    iron.run();
    ui.run();
}