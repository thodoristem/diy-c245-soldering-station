#include <Iron/Iron.h>
#include <EEPROM.h>

Iron::Iron(
    uint8_t heaterOutputPin,
    uint8_t heaterTempPin,
    uint8_t coldJunctionTempPin,
    uint8_t buzzerPin,
    bool invertedOutput)
{
    this->heaterOutputPin = heaterOutputPin;
    this->heaterTempPin = heaterTempPin;
    this->coldJunctionTempPin = coldJunctionTempPin;
    this->buzzerPin = buzzerPin;
    this->invertedOutput = invertedOutput;

    pinMode(this->heaterOutputPin, OUTPUT);
    pinMode(this->heaterTempPin, INPUT);
    pinMode(this->coldJunctionTempPin, INPUT);
    pinMode(this->buzzerPin, OUTPUT);
}

void Iron::init()
{
    delay(500);

    analogWriteFrequency(PWM_FREQ);
    analogReadResolution(ADC_BITS);
    analogWriteResolution(PWM_BITS);

    // this->loadSettings();
    this->tempCoefficientCalculation();
    this->setSelectedTemp(this->settings.initTemp);
    this->setOutputRange(this->settings.minOutputPCT, this->settings.maxOutputPCT);
    this->minTemp = this->settings.minTemp;
    this->maxTemp = this->settings.maxTemp;
    this->selectedTemp = this->settings.initTemp;

    this->heaterOutputOff();

    readTemp();

    this->pid = new QuickPID(
        &this->heaterTemp,
        &this->heaterOutput,
        &this->selectedTemp,
        this->settings.kp,
        this->settings.ki,
        this->settings.kd,
        this->settings.pMode,
        this->settings.dMode,
        this->settings.iAwMode,
        QuickPID::Action::direct);

    this->pid->SetMode(QuickPID::Control::automatic);
    this->pid->SetOutputLimits(this->minOutput, this->maxOutput);
    this->pid->SetSampleTimeUs(PID_SAMPLE_TIME_US);

    this->playTone(SoundEvent::SOUND_INIT);
}

void Iron::initCal()
{
    this->setOutputRange(0.0f, 50.0f);
    this->minTemp = 75.0f;
    this->maxTemp = 475.0f;
}

void Iron::setOutputRange(float minPCT, float maxPCT)
{
    this->minOutput = PWM_MAX / 100.0f * minPCT;
    this->maxOutput = PWM_MAX / 100.0f * maxPCT;
}

void Iron::saveSettings()
{
    EEPROM.put(0, this->settings);
}

void Iron::loadSettings()
{
    Settings tmp;
    EEPROM.get(0, tmp);
    if (tmp.magic != SETTINGS_MAGIC)
    {
        this->tempCoefficientCalculation();
        this->saveSettings();
    }
    else
    {
        this->settings = tmp;
    }
}

float Iron::readVoltage(uint8_t pin, int samples, unsigned int settleTimeUs, float vRef)
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

bool Iron::tempCoefficientCalculation()
{
    double mv[CAL_NPOINTS];
    double t[CAL_NPOINTS];

    for (int i = 0; i < CAL_NPOINTS; i++)
    {
        mv[i] = this->settings.calPoints[i].mV;
        t[i] = this->settings.calPoints[i].tempC;
    }

    double coeffs[CAL_NCOEFFS];
    if (fitCurve(CAL_ORDER, CAL_NPOINTS, mv, t, CAL_NCOEFFS, coeffs) != 0)
    {
        return false;
    }

    for (int i = 0; i < CAL_NCOEFFS; i++)
    {
        if (!isfinite(coeffs[i]))
        {
            return false;
        }
    }

    for (int i = 0; i < CAL_NCOEFFS; i++)
    {
        this->settings.coeffs[i] = coeffs[i];
    }

    return true;
}

void Iron::readTemp()
{
    if (!this->tempMeasureActive)
    {
        this->tempMeasureActive = true;
        this->tempMeasureStart = millis();
        return;
    }

    if (millis() - this->tempMeasureStart >= TEMP_MEASURE_SAMPLES_FREQ_MS)
    {
        // Stop heater
        this->heaterOutputOff();

        // Take temp measurement
        float volts = Iron::readVoltage(
            this->heaterTempPin,
            TEMP_MEASURE_SAMPLES,
            TEMP_MEASURE_SETTLE_TIME_US,
            this->settings.vRef);

        // Resume heater
        this->heaterOutputUpdate();

        // Convert amplifier output to thermocouple millivolts.
        this->heaterTempMV = (volts - this->settings.opAmpZeroOutputV) * 1000.0f / this->settings.opAmpGain;

        // 4th Order Polynomial c0 * mV^3 + c1 * mV^2 + c2 * mV + c3
        this->heaterTemp = this->settings.coeffs[0] * pow(this->heaterTempMV, 3);
        this->heaterTemp += this->settings.coeffs[1] * pow(this->heaterTempMV, 2);
        this->heaterTemp += this->settings.coeffs[2] * this->heaterTempMV;
        this->heaterTemp += this->settings.coeffs[3];

        this->tempMeasureActive = false;
    }
}

void Iron::readColdJunctionTemp()
{
    float volts = this->readVoltage(
        this->coldJunctionTempPin,
        TEMP_MEASURE_SAMPLES,
        TEMP_MEASURE_SETTLE_TIME_US,
        this->settings.vRef);

    if (volts <= 0.0f || volts >= this->settings.vRef)
    {
        return;
    }

    float resistance = THERMISTOR_FIXED_RES * volts / (this->settings.vRef - volts);
    float invT = 1.0f / THERMISTOR_K + log(resistance / THERMISTOR_RES) / THERMISTOR_BETA;

    this->coldJunctionTempMV = volts * 1000.0f;
    this->coldJunctionTemp = (1.0f / invT - 273.15f);
}

void Iron::run()
{
    this->readTemp();
    this->heaterCheck();

    if (!this->heaterTempReached && this->getHeaterTemp() >= this->getSelectedTemp())
    {
        this->playTone(SoundEvent::SOUND_TEMP_REACHED);
        this->heaterTempReached = true;
    }
    else if (!this->heaterTempReachedRev && this->getHeaterTemp() <= this->getSelectedTemp())
    {
        this->playTone(SoundEvent::SOUND_TEMP_REACHED);
        this->heaterTempReachedRev = true;
    }

    if (this->getHeaterTemp() < this->getSelectedTemp() - 10)
    {
        this->heaterTempReached = false;
    }
    else if (this->getHeaterTemp() > this->getSelectedTemp() + 10)
    {
        this->heaterTempReachedRev = false;
    }

    this->runPID();
}

void Iron::runPID()
{
    if (this->status != STATUS_FAULT && this->status != STATUS_IDLE)
    {
        if (this->pid->Compute())
        {
            this->heaterOutputUpdate();
        }
    }
    else
    {
        this->heaterOutput = 0;
        this->heaterOutputOff();
    }
}

void Iron::playTone(SoundEvent event)
{
    switch (event)
    {
    case SOUND_INIT:
        tone(this->buzzerPin, 800, 100);
        delay(150);
        tone(this->buzzerPin, 1200, 100);
        delay(150);
        tone(this->buzzerPin, 1600, 150);
        break;
    case SOUND_TEMP_CHANGE:
        tone(this->buzzerPin, 2200, 70);
        break;
    case SOUND_TEMP_REACHED:
        tone(this->buzzerPin, 1200, 100);
        delay(150);
        tone(this->buzzerPin, 1800, 180);
        break;
    case SOUND_ERROR:
        for (uint8_t i = 0; i < 3; i++)
        {
            tone(this->buzzerPin, 500, 180);
            delay(260);
        }
        break;
    case SOUND_HEATER_ON:
        tone(this->buzzerPin, 1600, 70);
        break;
    case SOUND_HEATER_OFF:
        tone(this->buzzerPin, 1000, 100);
        break;
    case SOUND_NONE:
    default:
        noTone(this->buzzerPin);
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

void Iron::toggle()
{
    switch (this->getStatus())
    {
    case STATUS_WARM_UP:
    case STATUS_HEATING:
        this->setStatus(STATUS_IDLE);
        this->playTone(SoundEvent::SOUND_HEATER_OFF);
        break;
    case STATUS_IDLE:
        this->setStatus(STATUS_HEATING);
        this->playTone(SoundEvent::SOUND_HEATER_ON);
        break;
    default:
        this->setStatus(STATUS_HEATING);
        this->playTone(SoundEvent::SOUND_HEATER_ON);
        break;
    }
}

void Iron::heaterCheck()
{
    float heaterOutputThreshhold = this->maxOutput;

    if (this->getStatus() == STATUS_WARM_UP)
    {
        heaterOutputThreshhold = this->maxOutput / 2.0f;
    }

    if (this->getHeaterOutput() < heaterOutputThreshhold)
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

    if (millis() - this->heaterCheckStart >= HEATER_CHECK_INTERVAL_SEC)
    {
        float tempDiff = this->getHeaterTemp() - this->heaterCheckTemp;

        if (tempDiff < HEATER_CHECK_MIN_TEMP_RIZE)
        {
            this->status = STATUS_FAULT;
            this->playTone(SoundEvent::SOUND_ERROR);
        }

        // Start a new observation window.
        this->heaterCheckStart = millis();
        this->heaterCheckTemp = this->getHeaterTemp();
    }
}

void Iron::heaterOutputUpdate()
{
    if (this->invertedOutput)
    {
        analogWrite(this->heaterOutputPin, PWM_RESOLUTION - this->heaterOutput);
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
        analogWrite(this->heaterOutputPin, PWM_RESOLUTION);
    }
    else
    {
        analogWrite(this->heaterOutputPin, 0);
    }
}

void Iron::setSelectedTemp(float tempC)
{
    if (tempC < this->minTemp)
    {
        this->selectedTemp = this->minTemp;
    }
    else if (tempC > this->maxTemp)
    {
        this->selectedTemp = this->maxTemp;
    }
    else
    {
        this->selectedTemp = tempC;
    }
}

void Iron::increaseSelectedTemp()
{
    this->increaseSelectedTemp(this->settings.selectStep);
}

void Iron::increaseSelectedTemp(byte step)
{
    if (this->selectedTemp < this->maxTemp)
    {
        this->selectedTemp += step;
    }
    else
    {
        this->selectedTemp = this->maxTemp;
    }
    this->playTone(SoundEvent::SOUND_TEMP_CHANGE);
}

void Iron::decreaseSelectedTemp()
{
    this->decreaseSelectedTemp(this->settings.selectStep);
}

void Iron::decreaseSelectedTemp(byte step)
{
    if (this->selectedTemp > this->minTemp)
    {
        this->selectedTemp -= step;
    }
    else
    {
        this->selectedTemp = this->minTemp;
    }
    this->playTone(SoundEvent::SOUND_TEMP_CHANGE);
}

float Iron::getSelectedTemp()
{
    return this->selectedTemp;
}

float Iron::getSelectedTempPct()
{
    return this->selectedTemp / this->maxTemp * 100.0f;
}

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

float Iron::getColdJunctionTemp()
{
    return this->coldJunctionTemp;
}

float Iron::getHeaterOutput()
{
    return this->heaterOutput;
}

float Iron::getHeaterOutputPct()
{
    return this->heaterOutput / this->maxOutput * 100.0f;
}

float Iron::getVRef()
{
    return this->settings.vRef;
}

Iron::Settings Iron::getSettings()
{
    return this->settings;
}

Iron::CalPoint Iron::getCalPoint(uint8_t index)
{
    return this->settings.calPoints[index];
}

void Iron::setCalPointMv(uint8_t index, double mV)
{
    this->settings.calPoints[index].mV = mV;
}