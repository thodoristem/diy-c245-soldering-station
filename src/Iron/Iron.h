#ifndef Iron_H
#define Iron_H

#include <Arduino.h>
#include <QuickPID.h>
#include <curveFitting.h>

// #define OPAMP_ZERO_OUTPUT_V 0.0017f
// #define OPAMP_GAIN 214.0f
// #define VREF 3.285f

#define THERMISTOR_RES 10000.0f
#define THERMISTOR_FIXED_RES THERMISTOR_RES
#define THERMISTOR_K 298.15f
#define THERMISTOR_BETA 3950.0f

#define ADC_BITS 12
#define ADC_MAX 4095

#define PWM_FREQ 21000
#define PWM_BITS 10
#define PWM_MAX 1023

#define TEMP_MEASURE_SAMPLES_TIME_MS (40 / 1000) * 1000UL // 40 samples per sec in ms
#define TEMP_MEASURE_SAMPLES 30
#define TEMP_MEASURE_SETTLE_TIME_US 500

#define PID_SAMPLES_TIME_US TEMP_MEASURE_SAMPLES_TIME_MS * 1000UL // 40 samples pers sec for pid in μs

#define HEATER_CHECK_INTERVAL_MS 1000
#define HEATER_CHECK_MIN_TEMP_RIZE 2.0f

#define CONFI_MAGIC 0xC245A100UL
#define GENERAL_SETTINGS_COUNT 7
#define PID_SETTINGS_COUNT 3
#define OPAMP_SETTINGS_COUNT 3

#define CAL_NPOINTS 7
#define CAL_ORDER 3
#define CAL_NCOEFFS CAL_ORDER + 1

class Iron
{
public:
    enum Status : uint8_t
    {
        STATUS_IDLE,
        STATUS_HEATING,
        STATUS_FAULT,
        STATUS_STAND_BY
    };

    enum SoundEvent : uint8_t
    {
        SOUND_NONE,
        SOUND_INIT,
        SOUND_TEMP_CHANGE,
        SOUND_TEMP_REACHED,
        SOUND_ERROR,
        SOUND_HEATER_ON,
        SOUND_HEATER_OFF
    };

    enum SettingType : uint8_t
    {
        GENERAL_SETTINGS,
        PID_SETTINGS,
        OPAMP_SETTINGS
    };

    enum GeneralSettings : uint8_t
    {
        MIN_TEMP,
        MAX_TEMP,
        TEMP_SELECT_STEP,
        MAX_OUTPUT_PCT,
        INITIAL_TEMP,
        STAND_BY_TEMP,
        BEEPER
    };

    enum PidSettings : uint8_t
    {
        PID_KP,
        PID_KI,
        PID_KD
    };

    enum OpAmpSettings : uint8_t
    {
        VREF,
        OPAMP_GAIN,
        OPAMP_ZERO_OUTPUT_V
    };

    struct CalPoint
    {
        double mV;
        double tempC;
    };

    struct Setting
    {
        String label;
        float min;
        float max;
        float value;
        float step;
        uint8_t decPlaces;
        String unit;
    };

private:
    QuickPID *pid;

    uint8_t heaterOutputPin;
    uint8_t heaterTempPin;
    uint8_t coldJunctionTempPin;
    uint8_t beeperPin;

    bool invertedOutput = true;

    float vRef = 3.3f;
    float opAmpGain = 200.0f;
    float opAmpZeroOutputV = 0.0f;

    float targetTemp = 200.0f;
    float pickUpTemp = 200.0f;

    float heaterOutput = 0.0f;
    float minOutput = 0.0f;
    float maxOutput;

    float heaterTempMV = 0.0f;
    float heaterTemp = 0.0f;
    float minTemp;
    float maxTemp;

    float coldJunctionTempMV = 0.0f;
    float coldJunctionTemp = 0.0f;

    bool heaterTempReached = false;
    bool heaterTempReachedRev = true;

    bool tempMeasureActive = true;
    unsigned long tempMeasureStart = 0;

    float heaterCheckTemp = 0;
    unsigned long heaterCheckStart = 0;

    bool heaterCheckActive = false;

    Status status = STATUS_IDLE;

    struct Configuration
    {
        uint32_t magic = CONFI_MAGIC;

        Setting generalSettings[GENERAL_SETTINGS_COUNT] = {
            {"Min Temp", 50.0f, 450.0f, 100.0f, 5.0f, 0, "C"},
            {"Max Temp", 50.0f, 450.0f, 450.0f, 5.0f, 0, "C"},
            {"Temp Step", 1.0f, 50.0f, 10.0f, 1.0f, 0, "C"},
            {"Max Output", 10.0f, 90.0f, 75.0f, 1.0f, 0, "%"},
            {"Initial Temp", 50.0f, 400.0f, 250.0f, 5.0f, 0, "C"},
            {"Stand-By Temp", 50.0f, 200.0f, 150.0f, 5.0f, 0, "C"},
            {"Beeper", 0, 1, 0, 1, 0, ""},
        };

        Setting pidSettings[PID_SETTINGS_COUNT] = {
            {"KP", 0.0f, 20.0f, 10.0f, 0.01f, 2, ""},
            {"KI", 0.0f, 20.0f, 1.0f, 0.01f, 2, ""},
            {"KD", 0.0f, 20.0f, 0.5f, 0.01f, 2, ""},
        };

        Setting opAmpSettings[OPAMP_SETTINGS_COUNT] = {
            {"Vref", 0.0f, 3.3f, 3.285, 0.001f, 3, "V"},
            {"Gain", 100.0f, 300.0f, 214.0f, 0.1f, 1, ""},
            {"Zero V", 0.0f, 5.0f, 0.001f, 0.001f, 3, "V"},
        };

        QuickPID::pMode pMode = QuickPID::pMode::pOnError;
        QuickPID::dMode dMode = QuickPID::dMode::dOnMeas;
        QuickPID::iAwMode iAwMode = QuickPID::iAwMode::iAwCondition;

        // Thermocouple Calibration
        CalPoint calPoints[CAL_NPOINTS] = {
            // {0.120, 50.0f},
            // {1.247, 100.0f},
            {2.635, 150.0f},
            {3.904, 200.0f},
            {5.275, 250.0f},
            {6.660, 300.0f},
            {8.098, 350.0f},
            {9.350, 400.0f},
            {10.900, 450.0f},
        };

        double coeffs[CAL_NCOEFFS];
    };

    Configuration configuration;

    void runPID();
    void readTemp();
    void readColdJunctionTemp();
    void heaterCheck();
    void heaterOutputUpdate();
    void heaterOutputOff();

public:
    Iron(
        uint8_t heaterOutputPin,
        uint8_t heaterTempPin,
        uint8_t coldJunctionTempPin,
        uint8_t beeperPin,
        bool outputReversed);

    void init();
    void initCal();
    void setOutputRange(float min, float max);
    void saveSettings();
    void loadSettings();
    bool tempCoefficientCalculation();
    void run();
    Status getStatus();
    void setStatus(Status status);
    void playTone(SoundEvent event);
    void toggle();
    void standBy();
    void pickUp();
    void setTargetTemp(float temp);
    void incTargetTemp();
    void incTargetTemp(float step);
    void decTargetTemp();
    void decTargetTemp(float step);
    float getHeaterOutput();
    float getHeaterOutputPct();
    float getTargetTemp();
    float getTargetTempPct();
    float getHeaterTempMV();
    float getHeaterTemp();
    float getHeaterTempPct();
    float getColdJunctionTemp();

    Setting getSetting(Iron::SettingType type, uint8_t key);
    void setSetting(Iron::SettingType type, uint8_t key, float value);
    void incSetting(Iron::SettingType type, uint8_t key);
    void decSetting(Iron::SettingType type, uint8_t key);

    CalPoint getCalPoint(uint8_t index);
    void setCalPointMv(uint8_t index, double mv);

    static float readVoltage(uint8_t pin, int samples, unsigned int delayTimeUs, float vRef);
};
#endif
