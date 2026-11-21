#include <Arduino.h>
#include <Iron/Iron.h>
#include <UI/UserInterface.h>
#include <RotaryEncoder.h>
#include <OneButton.h>
#include <U8g2lib.h>

#define HEATER_OUPUT_PIN PA8
#define HEATER_TEMP_PIN PB1
#define BEEPER_PIN PA1

#define ENC_BUTTON_PIN PA0
#define ENC_A_PIN PA3
#define ENC_B_PIN PA2
#define ENC_BTN_DEBOUNCE_MS 0
#define ENC_BTN_CLICK_MS 0
#define ENC_BTN_LONG_PRESS_MS 500

#define HANDLE_SENSE_PIN PB13
#define STANDBY_DEBOUNCE_MS 200
#define STANDBY_LONG_PRESS_MS 1

Iron iron(
    HEATER_OUPUT_PIN,
    HEATER_TEMP_PIN,
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
OneButton encoderButton;
OneButton handleSense;

void encoderISR()
{
    encoder.tick();
}

void onStandbySense()
{
    iron.standBy();
}

void onPickUpSense()
{
    iron.pickUp();
}

void onEncoderLongPress()
{
    ui.onLongPress();
}

void onEncoderPress()
{
    ui.onPress();
}

void setup()
{
    iron.init();
    ui.init();

    handleSense.setup(HANDLE_SENSE_PIN, INPUT, true);
    handleSense.setDebounceMs(STANDBY_DEBOUNCE_MS);
    handleSense.setPressMs(STANDBY_LONG_PRESS_MS);
    handleSense.attachLongPressStart(onStandbySense);
    handleSense.attachLongPressStop(onPickUpSense);

    encoderButton.setup(ENC_BUTTON_PIN, INPUT_PULLUP, true);
    encoderButton.setDebounceMs(ENC_BTN_DEBOUNCE_MS);
    encoderButton.setPressMs(ENC_BTN_LONG_PRESS_MS);
    encoderButton.setClickMs(ENC_BTN_CLICK_MS);
    encoderButton.attachLongPressStart(onEncoderLongPress);
    encoderButton.attachClick(onEncoderPress);

    attachInterrupt(digitalPinToInterrupt(ENC_A_PIN), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_B_PIN), encoderISR, CHANGE);
}

void loop()
{
    handleSense.tick();

    encoderButton.tick();
    switch (encoder.getDirection())
    {
    case RotaryEncoder::Direction::CLOCKWISE:
        ui.onPrev();
        break;
    case RotaryEncoder::Direction::COUNTERCLOCKWISE:
        ui.onNext();
        break;
    case RotaryEncoder::Direction::NOROTATION:
        break;
    }

    iron.run();
    ui.run();
}