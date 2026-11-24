#ifndef Iron_H
#define Iron_H

#include <Arduino.h>
#include <at24c256.h>
#include <QuickPID.h>

#define ADC_BITS 12
#define ADC_MAX 4095

#define PWM_FREQ 21000
#define PWM_BITS 10
#define PWM_MAX 1023

#define TEMP_MEASURE_SAMPLES_PER_SECOND 60
#define TEMP_MEASURE_SAMPLES_TIME_MS 1000 / TEMP_MEASURE_SAMPLES_PER_SECOND
#define TEMP_MEASURE_SAMPLES 30
#define TEMP_MEASURE_SETTLE_TIME_US 500

#define PID_SAMPLES_TIME_US TEMP_MEASURE_SAMPLES_TIME_MS * 1000UL

#define HEATER_CHECK_INTERVAL_MS 2000
#define HEATER_CHECK_MIN_TEMP_RIZE 2.0f

#define GENERAL_SETTINGS_COUNT 6
#define PID_SETTINGS_COUNT 3
#define OPAMP_SETTINGS_COUNT 3

#define TIP_COUNT 5

#define CAL_NPOINTS 6
#define CAL_ORDER 3
#define CAL_NCOEFFS CAL_ORDER + 1

#define EEPROM_MAGIC 0xC245A000UL
#define EEPROM_DATA_TYPE_COUNT 6
#define EEPROM_DATA_SETTINGS_COUNT 10

class Iron
{
public:
    enum Status : uint8_t
    {
        STATUS_OFF,
        STATUS_HEATING,
        STATUS_FAULT,
        STATUS_SLEEP,
        STATUS_SCREENSAVER // @todo Screen after a while
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
        OPAMP_SETTINGS,
        CAL_SETTINGS
    };

    enum GeneralSettings : uint8_t
    {
        TIP_SET,
        TEMP_SELECT_STEP_SET,
        MAX_OUTPUT_PCT_SET,
        INITIAL_TEMP_SET,
        SLEEP_TEMP_SET,
        BEEPER_SET
    };

    enum PidSettings : uint8_t
    {
        PID_KP_SET,
        PID_KI_SET,
        PID_KD_SET
    };

    enum OpAmpSettings : uint8_t
    {
        VREF_SET,
        OPAMP_GAIN_SET,
        OPAMP_ZERO_OUTPUT_V_SET
    };

private:
    AT24C256 *eeprom;
    QuickPID *pid;

    uint8_t heaterOutputPin;
    uint8_t heaterTempPin;
    uint8_t beeperPin;

    struct CalPoint
    {
        float mV;
        float tempC;
    };

    struct Setting
    {
        char label[20];
        float min;
        float max;
        float value;
        float step;
        uint8_t decPlaces;
        char unit[10];
    };

    struct StoredData
    {
        uint32_t magic = EEPROM_MAGIC;
        uint32_t saveCount = 0;
        float data[EEPROM_DATA_TYPE_COUNT + TIP_COUNT][EEPROM_DATA_SETTINGS_COUNT];
    };

    struct Configuration
    {
        bool changed = false;

        Iron::Setting generalSettings[GENERAL_SETTINGS_COUNT] = {
            {"Tip", 0, TIP_COUNT, 0, 1, 0, "tip"},
            {"Temp Step", 1.0f, 50.0f, 10.0f, 1.0f, 0, "C"},
            {"Max Output", 10.0f, 90.0f, 90.0f, 1.0f, 0, "%"},
            {"Initial Temp", 150.0f, 400.0f, 300.0f, 5.0f, 0, "C"},
            {"Sleep Temp", 150.0f, 250.0f, 150.0f, 5.0f, 0, "C"},
            {"Beeper", 0, 1, 0, 1, 0, "bool"},
        };

        Iron::Setting pidSettings[PID_SETTINGS_COUNT] = {
            {"KP", 0.0f, 20.0f, 9.000f, 0.1f, 1, ""},
            {"KI", 0.0f, 20.0f, 9.000f, 0.1f, 1, ""},
            {"KD", 0.0f, 20.0f, 0.500f, 0.1f, 1, ""},
        };

        Iron::Setting opAmpSettings[OPAMP_SETTINGS_COUNT] = {
            {"Vref", 0.0f, 3.3f, 3.285, 0.001f, 3, "V"},
            {"Gain", 100.0f, 300.0f, 214.0f, 0.1f, 1, ""},
            {"ZeroV", 0.0f, 2.0f, 0.001f, 0.001f, 3, "V"},
        };

        Iron::CalPoint calPoints[TIP_COUNT][CAL_NPOINTS] = {
            {
                {3.370f, 150.0f},
                {4.610f, 200.0f},
                {6.070f, 250.0f},
                {7.640f, 300.0f},
                {9.180f, 350.0f},
                {10.580f, 400.0f},
            },
            {
                {1.671f, 150.0f},
                {2.586f, 200.0f},
                {3.583f, 250.0f},
                {4.775f, 300.0f},
                {6.057f, 350.0f},
                {7.669f, 400.0f},
            },
            {
                {1.671f, 150.0f},
                {2.586f, 200.0f},
                {3.583f, 250.0f},
                {4.775f, 300.0f},
                {6.057f, 350.0f},
                {7.669f, 400.0f},
            },
            {
                {1.671f, 150.0f},
                {2.586f, 200.0f},
                {3.583f, 250.0f},
                {4.775f, 300.0f},
                {6.057f, 350.0f},
                {7.669f, 400.0f},
            },
            {
                {1.671f, 150.0f},
                {2.586f, 200.0f},
                {3.583f, 250.0f},
                {4.775f, 300.0f},
                {6.057f, 350.0f},
                {7.669f, 400.0f},
            },
        };
    };

    double coeffs[TIP_COUNT][CAL_NCOEFFS];

    float vRef = 3.3f;
    float opAmpGain = 200.0f;
    float opAmpZeroOutputV = 0.0f;

    uint8_t tip = 0;

    float targetTemp = 200.0f;
    float wakeTemp = 200.0f;
    bool disableSleep = false;

    bool invertedOutput = true;
    float heaterOutput = 0.0f;
    float minOutput = 0.0f;
    float maxOutput;

    float heaterTempMV = 0.0f;
    float heaterTemp = 0.0f;
    float minTemp;
    float maxTemp;

    bool heaterTempReached = false;
    bool heaterTempReachedRev = true;

    bool tempMeasureActive = true;
    unsigned long tempMeasureStart = 0;

    bool heaterCheckActive = false;
    unsigned long heaterCheckStart = 0;
    float heaterCheckTemp = 0;

    Status status = STATUS_OFF;

    Configuration configuration;
    StoredData storedData;

    void runPID();
    void readTemp();
    void heaterCheck();

public:
    Iron(
        uint8_t heaterOutputPin,
        uint8_t heaterTempPin,
        uint8_t beeperPin,
        bool outputReversed);

    static float readVoltage(uint8_t pin, int samples, int settleTimeUs, float vRef);

    bool tcCoefficientCalculation();

    void init();
    void init(bool loadSavedSettings);
    void initCal();
    void run();

    void heaterOutputUpdate();
    void heaterOutputOff();

    void toggle();
    void sleep();
    void wake();

    void setTip(uint8_t tip);
    uint8_t getTip();

    void setOutputRange(float min, float max);

    Status getStatus();
    void setStatus(Status status);

    void playTone(SoundEvent event);

    float getTargetTemp();
    float getTargetTempPct();

    float getHeaterOutput();
    float getHeaterOutputPct();

    float getHeaterTemp();
    float getHeaterTempMV();
    float getHeaterTempPct();

    void setTargetTemp(float temp);
    void incTargetTemp();
    void incTargetTemp(float step);
    void decTargetTemp();
    void decTargetTemp(float step);

    void saveSettings();
    void loadSettings();
    Setting getSetting(Iron::SettingType type, uint8_t key);
    void setSetting(Iron::SettingType type, uint8_t key, float value);
    void incSetting(Iron::SettingType type, uint8_t key);
    void decSetting(Iron::SettingType type, uint8_t key);
    CalPoint getCalPoint(uint8_t index);
    void setCalPointMv(uint8_t index, float mv);
    Configuration getConfig();
    StoredData getStoredData();
    static uint8_t getSettingsCountForType(SettingType type);
};
#endif
