#include <Iron/Iron.h>
#include <Wire.h>
#include <at24c256.h>
#include <curveFitting.h>

#define FLASH_DEBUG 0
#define USING_FLASH_SECTOR_NUMBER 62

Iron::Iron(
    uint8_t heaterOutputPin,
    uint8_t heaterTempPin,
    uint8_t beeperPin,
    bool invertedOutput)
{
    this->heaterOutputPin = heaterOutputPin;
    this->heaterTempPin = heaterTempPin;
    this->beeperPin = beeperPin;
    this->invertedOutput = invertedOutput;

    analogWriteFrequency(PWM_FREQ);
    analogReadResolution(ADC_BITS);
    analogWriteResolution(PWM_BITS);

    pinMode(this->heaterTempPin, INPUT);
    pinMode(this->heaterOutputPin, OUTPUT);
    pinMode(this->beeperPin, OUTPUT);
}

float Iron::readVoltage(uint8_t pin, int samples, int settleTimeUs, float vRef)
{
    delayMicroseconds(settleTimeUs);

    int total = 0;

    for (int i = 0; i < samples; i++)
    {
        delayMicroseconds(2);
        total += analogRead(pin);
    }

    return (float)(total / samples) * vRef / ADC_MAX;
}

void Iron::readTemp()
{
    if (!this->tempMeasureActive)
    {
        this->tempMeasureActive = true;
        this->tempMeasureStart = millis();
        return;
    }

    if (millis() - this->tempMeasureStart >= TEMP_MEASURE_SAMPLES_TIME_MS)
    {
        this->heaterOutputOff();

        float volts = Iron::readVoltage(
            this->heaterTempPin,
            TEMP_MEASURE_SAMPLES,
            TEMP_MEASURE_SETTLE_TIME_US,
            this->vRef);

        this->heaterOutputUpdate();

        this->heaterTempMV = (volts - this->opAmpZeroOutputV) * 1000.0f / this->opAmpGain;

        this->heaterTemp = this->coeffs[0] * pow(this->heaterTempMV, 3);
        this->heaterTemp += this->coeffs[1] * pow(this->heaterTempMV, 2);
        this->heaterTemp += this->coeffs[2] * this->heaterTempMV;
        this->heaterTemp += this->coeffs[3];

        this->tempMeasureActive = false;
    }
}

void Iron::runPID()
{
    if (this->status != STATUS_FAULT && this->status != STATUS_OFF)
    {
        if (this->pid->Compute())
        {
            this->heaterOutputUpdate();
        }

        if (this->getHeaterTemp() > this->getTargetTemp() + 1)
        {
            this->pid->Reset();
        }
    }
    else
    {
        this->heaterOutput = 0;
        this->heaterOutputOff();
    }
}

void Iron::heaterCheck()
{
    if (this->getHeaterTemp() > this->maxTemp + 100)
    {
        this->status = STATUS_FAULT;
        this->playTone(SoundEvent::SOUND_ERROR);
        return;
    }

    if (this->getHeaterOutput() < this->maxOutput * 0.25f)
    {
        this->heaterCheckActive = false;
        return;
    }

    if (!this->heaterCheckActive)
    {
        this->heaterCheckActive = true;
        this->heaterCheckStart = millis();
        this->heaterCheckTemp = this->getHeaterTemp();
        return;
    }

    if (millis() - this->heaterCheckStart >= HEATER_CHECK_INTERVAL_MS)
    {
        float tempDiff = this->getHeaterTemp() - this->heaterCheckTemp;

        if (tempDiff < HEATER_CHECK_MIN_TEMP_RIZE)
        {
            this->setStatus(Status::STATUS_FAULT);
            this->playTone(SoundEvent::SOUND_ERROR);
        }

        // Start a new observation window.
        this->heaterCheckStart = millis();
        this->heaterCheckTemp = this->getHeaterTemp();
    }
}

bool Iron::tcCoefficientCalculation()
{
    double mv[CAL_NPOINTS];
    double t[CAL_NPOINTS];

    for (int i = 0; i < CAL_NPOINTS; i++)
    {
        mv[i] = this->configuration.calPoints[this->tip][i].mV;
        t[i] = this->configuration.calPoints[this->tip][i].tempC;
    }

    if (fitCurve(CAL_ORDER, CAL_NPOINTS, mv, t, CAL_NCOEFFS, this->coeffs) != 0)
    {
        return false;
    }

    for (int i = 0; i < CAL_NCOEFFS; i++)
    {
        if (!isfinite(this->coeffs[i]))
        {
            return false;
        }
    }

    return true;
}

void Iron::init()
{
    this->init(true);
}

void Iron::init(bool loadSavedSettings)
{
    delay(500);

    Wire.begin();
    this->eeprom = new AT24C256(AT24C_ADDRESS_0, Wire);

    if (loadSavedSettings)
    {
        this->loadSettings();
    }

    this->tcCoefficientCalculation();

    this->tip = (uint8_t)this->getSetting(SettingType::GENERAL_SETTINGS, GeneralSettings::TIP_SET).value;
    this->vRef = this->getSetting(SettingType::OPAMP_SETTINGS, OpAmpSettings::VREF_SET).value;
    this->opAmpGain = this->getSetting(SettingType::OPAMP_SETTINGS, OpAmpSettings::OPAMP_GAIN_SET).value;
    this->opAmpZeroOutputV = this->getSetting(SettingType::OPAMP_SETTINGS, OpAmpSettings::OPAMP_ZERO_OUTPUT_V_SET).value;
    this->minTemp = this->getCalPoint(0).tempC;
    this->maxTemp = this->getCalPoint(CAL_NPOINTS - 1).tempC;
    this->setTargetTemp(this->getSetting(SettingType::GENERAL_SETTINGS, GeneralSettings::INITIAL_TEMP_SET).value);
    this->setOutputRange(0.0f, this->getSetting(SettingType::GENERAL_SETTINGS, GeneralSettings::MAX_OUTPUT_PCT_SET).value);

    this->readTemp();

    this->pid = new QuickPID(
        &this->heaterTemp,
        &this->heaterOutput,
        &this->targetTemp,
        this->getSetting(SettingType::PID_SETTINGS, PidSettings::PID_KP_SET).value,
        this->getSetting(SettingType::PID_SETTINGS, PidSettings::PID_KI_SET).value,
        this->getSetting(SettingType::PID_SETTINGS, PidSettings::PID_KD_SET).value,
        QuickPID::pMode::pOnError,
        QuickPID::dMode::dOnError,
        QuickPID::iAwMode::iAwCondition,
        QuickPID::Action::direct);

    this->pid->SetMode(QuickPID::Control::automatic);
    this->pid->SetOutputLimits(this->minOutput, this->maxOutput);
    this->pid->SetSampleTimeUs(PID_SAMPLES_TIME_US);

    this->disableSleep = !loadSavedSettings;
    this->setStatus(Status::STATUS_OFF);
    this->sleep();
    this->playTone(SoundEvent::SOUND_INIT);
}

void Iron::initCal()
{
    this->minTemp -= 100;
    this->maxTemp += 100;
    this->disableSleep = true;
}

void Iron::run()
{
    this->readTemp();
    this->heaterCheck();
    this->runPID();
}

void Iron::heaterOutputUpdate()
{
    if (this->invertedOutput)
    {
        analogWrite(this->heaterOutputPin, (int)(PWM_MAX - this->heaterOutput));
    }
    else
    {
        analogWrite(this->heaterOutputPin, (int)this->heaterOutput);
    }
}

void Iron::heaterOutputOff()
{
    if (this->invertedOutput)
    {
        analogWrite(this->heaterOutputPin, PWM_MAX);
    }
    else
    {
        analogWrite(this->heaterOutputPin, 0);
    }
}

void Iron::toggle()
{
    switch (this->getStatus())
    {
    case STATUS_SLEEP:
        return;
    case STATUS_FAULT:
    case STATUS_HEATING:
        this->setStatus(STATUS_OFF);
        this->playTone(SoundEvent::SOUND_HEATER_OFF);
        break;
    default:
        this->setStatus(STATUS_HEATING);
        this->playTone(SoundEvent::SOUND_HEATER_ON);
        break;
    }
}

void Iron::sleep()
{
    if (this->disableSleep || this->status == Status::STATUS_FAULT)
    {
        return;
    }

    if (this->getStatus() != Iron::Status::STATUS_SLEEP)
    {
        this->wakeTemp = this->getTargetTemp();
        this->setTargetTemp(this->getSetting(SettingType::GENERAL_SETTINGS, GeneralSettings::SLEEP_TEMP_SET).value);
        this->setStatus(Iron::Status::STATUS_SLEEP);
        this->playTone(SoundEvent::SOUND_HEATER_OFF);
    }
}

void Iron::wake()
{
    if (this->getStatus() == Iron::Status::STATUS_SLEEP)
    {
        this->setTargetTemp(this->wakeTemp);
        this->setStatus(Iron::Status::STATUS_HEATING);
        this->playTone(SoundEvent::SOUND_HEATER_ON);
    }
}

void Iron::setTip(uint8_t tip)
{
    this->tip = tip;
}

uint8_t Iron::getTip()
{
    return this->tip;
}

void Iron::setOutputRange(float minPCT, float maxPCT)
{
    this->minOutput = PWM_MAX / 100.0f * minPCT;
    this->maxOutput = PWM_MAX / 100.0f * maxPCT;
}

void Iron::playTone(SoundEvent event)
{
    if (this->getSetting(SettingType::GENERAL_SETTINGS, GeneralSettings::BEEPER_SET).value == 0)
    {
        return;
    }
    switch (event)
    {
    case SOUND_INIT:
        tone(this->beeperPin, 800, 100);
        delay(150);
        tone(this->beeperPin, 1200, 100);
        delay(150);
        tone(this->beeperPin, 1600, 150);
        break;
    case SOUND_TEMP_CHANGE:
        tone(this->beeperPin, 2200, 70);
        break;
    case SOUND_TEMP_REACHED:
        tone(this->beeperPin, 1200, 100);
        delay(150);
        tone(this->beeperPin, 1800, 180);
        break;
    case SOUND_ERROR:
        for (uint8_t i = 0; i < 3; i++)
        {
            tone(this->beeperPin, 500, 180);
            delay(260);
        }
        break;
    case SOUND_HEATER_ON:
        tone(this->beeperPin, 1600, 70);
        break;
    case SOUND_HEATER_OFF:
        tone(this->beeperPin, 1000, 100);
        break;
    case SOUND_NONE:
    default:
        noTone(this->beeperPin);
        break;
    }
}

Iron::Status Iron::getStatus()
{
    return this->status;
}

void Iron::setStatus(Status status)
{
    this->status = status;
}

// ##############################################################################################
// ## TARGET TEMPRATURE #########################################################################
// ##############################################################################################

void Iron::setTargetTemp(float temp)
{
    if (temp <= this->minTemp)
    {
        this->targetTemp = this->minTemp;
    }
    else if (temp >= this->maxTemp)
    {
        this->targetTemp = this->maxTemp;
    }
    else
    {
        this->targetTemp = temp;
    }
}

float Iron::getTargetTemp()
{
    return this->targetTemp;
}

float Iron::getTargetTempPct()
{
    return this->targetTemp / this->maxTemp * 100.0f;
}

void Iron::incTargetTemp()
{
    this->incTargetTemp(this->getSetting(SettingType::GENERAL_SETTINGS, GeneralSettings::TEMP_SELECT_STEP_SET).value);
}

void Iron::incTargetTemp(float step)
{
    if (this->status == Status::STATUS_SLEEP)
    {
        return;
    }

    if (this->targetTemp < this->maxTemp)
    {
        this->targetTemp += step;
    }
    else
    {
        this->targetTemp = this->maxTemp;
    }
    this->playTone(SoundEvent::SOUND_TEMP_CHANGE);
}

void Iron::decTargetTemp()
{
    this->decTargetTemp(this->getSetting(SettingType::GENERAL_SETTINGS, GeneralSettings::TEMP_SELECT_STEP_SET).value);
}

void Iron::decTargetTemp(float step)
{
    if (this->status == Status::STATUS_SLEEP)
    {
        return;
    }

    if (this->targetTemp > this->minTemp)
    {
        this->targetTemp -= step;
    }
    else
    {
        this->targetTemp = this->minTemp;
    }
    this->playTone(SoundEvent::SOUND_TEMP_CHANGE);
}

// ##############################################################################################
// ## TIP TEMPRATURE ################################################################################
// ##############################################################################################

float Iron::getHeaterTemp()
{
    return this->heaterTemp;
}

float Iron::getHeaterTempPct()
{
    return this->heaterTemp / this->maxTemp * 100.0f;
}

float Iron::getHeaterTempMV()
{
    return this->heaterTempMV;
}

// ##############################################################################################
// ## HEATER OUTPUT #############################################################################
// ##############################################################################################

float Iron::getHeaterOutput()
{
    return this->heaterOutput;
}

float Iron::getHeaterOutputPct()
{
    return this->heaterOutput / this->maxOutput * 100.0f;
}

// ##############################################################################################
// ## SETTINGS ##################################################################################
// ##############################################################################################

void Iron::saveSettings()
{
    if (!this->configuration.changed)
    {
        return;
    }

    for (uint8_t typeIndex = 0; typeIndex < EEPROM_DATA_TYPE_COUNT; typeIndex++)
    {
        for (uint8_t settingIndex = 0; settingIndex < Iron::getSettingsCountForType((SettingType)typeIndex); settingIndex++)
        {
            switch (typeIndex)
            {
            case SettingType::GENERAL_SETTINGS:
                this->storedData.data[typeIndex][settingIndex] = (float)this->configuration.generalSettings[settingIndex].value;
                break;
            case SettingType::PID_SETTINGS:
                this->storedData.data[typeIndex][settingIndex] = (float)this->configuration.pidSettings[settingIndex].value;
                break;
            case SettingType::OPAMP_SETTINGS:
                this->storedData.data[typeIndex][settingIndex] = (float)this->configuration.opAmpSettings[settingIndex].value;
                break;
            case SettingType::CAL_SETTINGS:
                for (uint8_t tipIndex = 0; tipIndex < TIP_COUNT; tipIndex++)
                {
                    this->storedData.data[typeIndex + tipIndex][settingIndex] = (float)this->configuration.calPoints[tipIndex][settingIndex].mV;
                }
                break;
            }
        }
    }

    this->storedData.saveCount++;
    this->storedData.magic = EEPROM_MAGIC;

    this->eeprom->put(0, this->storedData);

    this->configuration.changed = false;
}

void Iron::loadSettings()
{
    StoredData tempStoredData;

    this->eeprom->get(0, tempStoredData);

    if (tempStoredData.magic != EEPROM_MAGIC)
    {
        this->configuration.changed = true;
        this->saveSettings();
        return;
    }

    this->storedData = tempStoredData;

    for (uint8_t typeIndex = 0; typeIndex < EEPROM_DATA_TYPE_COUNT; typeIndex++)
    {
        for (uint8_t settingIndex = 0; settingIndex < Iron::getSettingsCountForType((SettingType)typeIndex); settingIndex++)
        {
            switch (typeIndex)
            {
            case SettingType::GENERAL_SETTINGS:
                this->configuration.generalSettings[settingIndex].value = this->storedData.data[typeIndex][settingIndex];
                break;
            case SettingType::PID_SETTINGS:
                this->configuration.pidSettings[settingIndex].value = this->storedData.data[typeIndex][settingIndex];
                break;
            case SettingType::OPAMP_SETTINGS:
                this->configuration.opAmpSettings[settingIndex].value = this->storedData.data[typeIndex][settingIndex];
                break;
            case SettingType::CAL_SETTINGS:
                for (uint8_t tipIndex = 0; tipIndex < TIP_COUNT; tipIndex++)
                {
                    this->configuration.calPoints[tipIndex][settingIndex].mV = this->storedData.data[typeIndex + tipIndex][settingIndex];
                }
                break;
            }
        }
    }
}

Iron::Setting Iron::getSetting(Iron::SettingType type, uint8_t key)
{
    switch (type)
    {
    case GENERAL_SETTINGS:
        return this->configuration.generalSettings[key];
    case PID_SETTINGS:
        return this->configuration.pidSettings[key];
    case OPAMP_SETTINGS:
        return this->configuration.opAmpSettings[key];
    default:
        return this->configuration.generalSettings[key];
    }
}

void Iron::setSetting(Iron::SettingType type, uint8_t key, float value)
{
    this->configuration.changed = true;

    switch (type)
    {
    case GENERAL_SETTINGS:
        this->configuration.generalSettings[key].value = value;
        break;
    case PID_SETTINGS:
        this->configuration.pidSettings[key].value = value;
        break;
    case OPAMP_SETTINGS:
        this->configuration.opAmpSettings[key].value = value;
        break;
    default:
        return;
    }
}

void Iron::incSetting(Iron::SettingType type, uint8_t key)
{
    float value = this->getSetting(type, key).value;
    float step = this->getSetting(type, key).step;
    float max = this->getSetting(type, key).max;

    if (value < max)
    {
        value += step;
    }
    else
    {
        value = max;
    }

    this->setSetting(type, key, value);
}

void Iron::decSetting(Iron::SettingType type, uint8_t key)
{
    float value = this->getSetting(type, key).value;
    float step = this->getSetting(type, key).step;
    float min = this->getSetting(type, key).min;

    if (value > min)
    {
        value -= step;
    }
    else
    {
        value = min;
    }

    this->setSetting(type, key, value);
}

Iron::CalPoint Iron::getCalPoint(uint8_t index)
{
    return this->configuration.calPoints[this->tip][index];
}

void Iron::setCalPointMv(uint8_t index, float mV)
{
    this->configuration.changed = true;
    this->configuration.calPoints[this->tip][index].mV = mV;
}

Iron::Configuration Iron::getConfig()
{
    return this->configuration;
}

Iron::StoredData Iron::getStoredData()
{
    return this->storedData;
}

uint8_t Iron::getSettingsCountForType(SettingType type)
{
    switch (type)
    {
    case SettingType::GENERAL_SETTINGS:
        return GENERAL_SETTINGS_COUNT;
    case SettingType::PID_SETTINGS:
        return PID_SETTINGS_COUNT;
    case SettingType::OPAMP_SETTINGS:
        return OPAMP_SETTINGS_COUNT;
    case SettingType::CAL_SETTINGS:
        return CAL_NPOINTS;
    default:
        return 0;
    }
}
