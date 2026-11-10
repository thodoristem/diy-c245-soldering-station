#ifndef Iron_H
#define Iron_H

#include <Arduino.h>
#include <QuickPID.h>
#include <curveFitting.h>

#define OPAMP_ZERO_OUTPUT_V 0.0016f
#define OPAMP_GAIN 214.0f
#define VREF 3.290f

#define THERMISTOR_RES 10000.0f
#define THERMISTOR_FIXED_RES THERMISTOR_RES
#define THERMISTOR_K 298.15f
#define THERMISTOR_BETA 3950.0f

#define ADC_BITS 12
#define ADC_MAX 4095

#define PWM_FREQ 24000
#define PWM_BITS 10
#define PWM_MAX 1023

#define TEMP_MEASURE_SAMPLES_FREQ_MS (40 / 1000)
#define TEMP_MEASURE_SAMPLES 30
#define TEMP_MEASURE_SETTLE_TIME_US 600

#define PID_SAMPLE_TIME_US 100000UL

#define HEATER_CHECK_INTERVAL_SEC 5000
#define HEATER_CHECK_MIN_TEMP_RIZE 2.0f

#define SETTINGS_MAGIC 0xC245A100UL

#define CAL_NPOINTS 9
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
        STATUS_SLEEP,
        STATUS_WARM_UP
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

    struct CalPoint
    {
        double mV;
        double tempC;
    };

private:
    QuickPID *pid;

    uint8_t heaterOutputPin;
    uint8_t heaterTempPin;
    uint8_t coldJunctionTempPin;
    uint8_t buzzerPin;

    bool invertedOutput = true;

    float selectedTemp = 200.0;

    float heaterOutput = 0.0f;
    float minOutput;
    float maxOutput;
    int pwmResolution;

    float heaterTempMV = 0.0f;
    float heaterTemp = 0.0f;
    float minTemp;
    float maxTemp;
    int adcResolution;

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

    struct Settings
    {
        uint32_t magic = SETTINGS_MAGIC;

        // General Settings
        float minTemp = 100.0f;
        float maxTemp = 450.0f;
        float minOutputPCT = 0.0f;
        float maxOutputPCT = 75.0f;
        float tempSelectStep = 10.0f;
        float initTemp = 250.0f;

        // PID Tuning
        float kp = 8.0f;
        float ki = 2.0f;
        float kd = 0.5f;
        QuickPID::pMode pMode = QuickPID::pMode::pOnError;
        QuickPID::dMode dMode = QuickPID::dMode::dOnMeas;
        QuickPID::iAwMode iAwMode = QuickPID::iAwMode::iAwOff;

        // Thermocouple Calibration
        float opAmpGain = OPAMP_GAIN;
        float opAmpZeroOutputV = OPAMP_ZERO_OUTPUT_V;
        float vRef = VREF;
        CalPoint calPoints[CAL_NPOINTS] = {
            {0.120, 50.0f},
            {1.247, 100.0f},
            {2.635, 150.0f},
            {3.904, 200.0f},
            {5.275, 250.0f},
            {6.660, 300.0f},
            {8.098, 350.0f},
            {9.350, 400.0f},
            {10.900, 450.0f}};
        double coeffs[CAL_NCOEFFS];
    };

    Settings settings;

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
        uint8_t buzzerPin,
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
    void setSelectedTemp(float temp);
    void increaseSelectedTemp();
    void increaseSelectedTemp(float step);
    void decreaseSelectedTemp();
    void decreaseSelectedTemp(float step);
    float getHeaterOutput();
    float getHeaterOutputPct();
    float getSelectedTemp();
    float getSelectedTempPct();
    float getHeaterTempMV();
    float getHeaterTemp();
    float getHeaterTempPct();
    float getColdJunctionTemp();
    float getVRef();
    Settings getSettings();
    CalPoint getCalPoint(uint8_t index);
    void setCalPointMv(uint8_t index, double mv);

    static float readVoltage(uint8_t pin, int samples, unsigned int delayTimeUs, float vRef);
};
#endif
