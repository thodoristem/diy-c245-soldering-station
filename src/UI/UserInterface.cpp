#include <UI/UserInterface.h>

UserInterface::UserInterface(Iron *iron, U8G2 *lcd)
{
    this->iron = iron;
    this->lcd = lcd;
}

void UserInterface::init()
{
    this->lcd->begin();
    this->lcd->clear();

    this->showIntroMsg();
}

void UserInterface::run()
{
    this->lcdEndMS = millis();

    if (this->lcdEndMS - this->lcdStartMS >= LCD_INTERVAL_MS)
    {
        this->lcdStartMS = this->lcdEndMS;

        this->lcd->clearBuffer();

        switch (this->selectedView)
        {
        case DASHBOARD:
            this->showDashboard();
            break;
        case MENU:
            this->showMenu();
            break;
        case PAGE_GENERAL_SETTINGS:
            this->showGeneralSettingsPage();
            break;
        case PAGE_PID:
            this->showPIDPage();
            break;
        case PAGE_OPAMP:
            this->showOpAmpPage();
            break;
        case PAGE_CAL_POINTS:
            this->showCalPointsPage();
            if (this->iron->getStatus() == Iron::Status::STATUS_FAULT && this->selectedLevel == OPTION_EDIT)
            {
                this->selectedLevel = OPTION_SELECT;
            }
            break;
        case PAGE_RESET:
            this->showResetPage();
            break;
        }

        this->lcd->sendBuffer();
    }
}

void UserInterface::onPrev()
{
    switch (this->selectedLevel)
    {
    case HOME:
        this->iron->decTargetTemp();
        break;
    case OPTION_EDIT:
        switch (this->selectedView)
        {
        case PAGE_GENERAL_SETTINGS:
            this->iron->decSetting(Iron::SettingType::GENERAL_SETTINGS, this->selectedOption - 1);
            break;
        case PAGE_PID:
            this->iron->decSetting(Iron::SettingType::PID_SETTINGS, this->selectedOption - 1);
            break;
        case PAGE_OPAMP:
            this->iron->decSetting(Iron::SettingType::OPAMP_SETTINGS, this->selectedOption - 1);
            break;
        case PAGE_CAL_POINTS:
            this->iron->decTargetTemp(1);
            break;
        default:
            break;
        }
        break;
    default:
        this->prevSelection();
        break;
    }
}

void UserInterface::onNext()
{
    switch (this->selectedLevel)
    {
    case HOME:
        this->iron->incTargetTemp();
        break;
    case OPTION_EDIT:
        switch (this->selectedView)
        {
        case PAGE_GENERAL_SETTINGS:
            this->iron->incSetting(Iron::SettingType::GENERAL_SETTINGS, this->selectedOption - 1);
            break;
        case PAGE_PID:
            this->iron->incSetting(Iron::SettingType::PID_SETTINGS, this->selectedOption - 1);
            break;
        case PAGE_OPAMP:
            this->iron->incSetting(Iron::SettingType::OPAMP_SETTINGS, this->selectedOption - 1);
            break;
        case PAGE_CAL_POINTS:
            this->iron->incTargetTemp(1);
            break;
        default:
            break;
        }
        break;
    default:
        this->nextSelection();
        break;
    }
}

void UserInterface::onPress()
{
    switch (this->selectedLevel)
    {
    case HOME:
        this->iron->toggle();
        break;
    case PAGE_SELECT:
        switch (this->selectedOption)
        {
        case 0:
            this->selectView(PAGE_GENERAL_SETTINGS);
            break;
        case 1:
            this->selectView(PAGE_PID);
            break;
        case 2:
            this->selectView(PAGE_OPAMP);
            break;
        case 3:
            this->selectView(PAGE_CAL_POINTS);
            break;
        case 4:
            this->selectView(PAGE_RESET);
            break;
        }
        break;
    case OPTION_SELECT:
        if (this->selectedOption == 0)
        {
            this->selectedLevel = PAGE_SELECT;
            this->selectView(MENU);
        }
        else
        {
            switch (this->selectedView)
            {
            case PAGE_CAL_POINTS:
                this->selectedLevel = OPTION_EDIT;
                this->iron->initCal();
                this->iron->setTargetTemp(this->iron->getCalPoint(this->selectedOption - 1).tempC);
                this->iron->setStatus(Iron::Status::STATUS_HEATING);
                break;
            default:
                this->selectedLevel = OPTION_EDIT;
                break;
            }
        }
        break;
    case OPTION_EDIT:
        switch (this->selectedView)
        {
        case PAGE_CAL_POINTS:
            this->selectedLevel = OPTION_SELECT;
            this->iron->setCalPointMv(this->selectedOption - 1, this->iron->getHeaterTempMV());
            this->iron->setTargetTemp(this->iron->getSetting(Iron::SettingType::GENERAL_SETTINGS, Iron::GeneralSettings::INITIAL_TEMP_SET).value);
            this->iron->setStatus(Iron::Status::STATUS_OFF);
            this->iron->tempCoefficientCalculation();
            break;
        default:
            this->selectedLevel = OPTION_SELECT;
            break;
        }
        break;
    default:
        break;
    }
}

void UserInterface::onLongPress()
{
    switch (this->selectedView)
    {
    case DASHBOARD:
        this->selectView(View::MENU);
        this->iron->sleep();
        break;
    default:
        this->selectView(View::DASHBOARD);
        this->showSaveMsg();
        this->iron->saveSettings();
        this->iron->init();
        break;
    }
}

void UserInterface::prevSelection()
{
    if (this->selectedOption <= this->minOptionCount)
    {
        this->selectedOption = this->minOptionCount;
    }
    else
    {
        this->selectedOption--;
    }
}

void UserInterface::nextSelection()
{
    if (this->selectedOption >= this->maxOptionCount)
    {
        this->selectedOption = this->maxOptionCount;
    }
    else
    {
        this->selectedOption++;
    }
}

void UserInterface::selectView(View view)
{
    this->viewInit = true;
    this->selectedView = view;
    this->lcd->clear();
}

void UserInterface::decOptionOffset()
{
    if (this->optionOffset > 0)
    {
        this->optionOffset--;
    }
    else
    {
        this->optionOffset = 0;
    }
}

void UserInterface::incOptionOffset(uint8_t limit)
{
    if (this->optionOffset < limit)
    {
        this->optionOffset++;
    }
    else
    {
        this->optionOffset = limit;
    }
}

void UserInterface::showIntroMsg()
{
    String label;

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosTop();

    this->lcd->clearBuffer();

    label = String("DIY");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, 0, label.c_str());
    label = String("C245/T12");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, 16, label.c_str());
    label = String("Soldering Station");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, 16 * 2, label.c_str());
    label = String("By T.Temourtzidis");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, 16 * 3, label.c_str());

    this->lcd->sendBuffer();

    delay(3000);
}

void UserInterface::showSaveMsg()
{
    String label;

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosCenter();

    this->lcd->clearBuffer();

    if (this->iron->getConfig().changed)
    {
        label = "Saving Settings...";
    }
    else
    {
        label = "Nothing Changed!";
        delay(2000);
    }

    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, this->lcd->getHeight() / 2, label.c_str());

    this->lcd->sendBuffer();
}

void UserInterface::showDashboard()
{
    if (this->viewInit)
    {
        this->selectedLevel = HOME;
        this->minOptionCount = 0;
        this->maxOptionCount = 0;
        this->selectedOption = 0;
        this->optionOffset = 0;
        this->viewInit = false;
    }

    this->lcd->setDrawColor(1);

    char ironTemp[10];
    char ironSetTemp[10];
    char ironOutput[10];

    sprintf(ironTemp, "%03d", (int)this->iron->getHeaterTemp());
    sprintf(ironSetTemp, "%03d\u00B0C", (int)this->iron->getTargetTemp());
    sprintf(ironOutput, "%3d%%", (int)this->iron->getHeaterOutputPct());

    this->lcd->setFontPosTop();
    this->lcd->setFont(u8g2_font_spleen8x16_mf);
    this->lcd->drawUTF8(15, 0, ironSetTemp);
    this->lcd->drawUTF8(this->lcd->getWidth() - this->lcd->getStrWidth(ironOutput) - 15, 0, ironOutput);

    // Debug EEPROM
    this->lcd->setCursor(20, 20);
    this->lcd->print(this->iron->getStoredData().saveCount);
    // this->lcd->setCursor(14, 28);
    // this->lcd->print(sizeof(this->iron->getStoredData()));

    this->drawLoadingBar(0, 0, 7, this->lcd->getHeight(), this->iron->getTargetTempPct());
    this->drawLoadingBar(6, 0, 7, this->lcd->getHeight(), this->iron->getHeaterTempPct());
    this->drawLoadingBar(this->lcd->getWidth() - 13, 0, 13, this->lcd->getHeight(), this->iron->getHeaterOutputPct());

    this->lcd->drawRFrame(16, 15, this->lcd->getWidth() - 32, this->lcd->getHeight() - 30, 8);

    this->lcd->drawUTF8((this->lcd->getWidth() / 2 + 24), this->lcd->getHeight() / 2 - 10, "\u00B0C");
    this->lcd->setFont(u8g2_font_spleen16x32_mn);
    this->lcd->setFontPosCenter();
    this->lcd->setCursor((this->lcd->getWidth() - this->lcd->getStrWidth(ironTemp)) / 2, this->lcd->getHeight() / 2);
    this->lcd->print(ironTemp);

    this->drawStatus();
}

void UserInterface::showMenu()
{
    if (this->viewInit)
    {
        this->selectedLevel = PAGE_SELECT;
        this->minOptionCount = 0;
        this->maxOptionCount = 4;
        this->selectedOption = 0;
        this->optionOffset = 0;
        this->viewInit = false;
    }

    switch (this->selectedOption)
    {
    case 0:
        this->showGeneralSettingsPage();
        break;
    case 1:
        this->showPIDPage();
        break;
    case 2:
        this->showOpAmpPage();
        break;
    case 3:
        this->showCalPointsPage();
        break;
    case 4:
        this->showResetPage();
        break;
    }
}

void UserInterface::showGeneralSettingsPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->optionOffset = 0;
        this->maxOptionsVisible = 3;
        this->minOptionCount = 0;
        this->maxOptionCount = GENERAL_SETTINGS_COUNT;
        this->selectedOption = 1;
        this->viewInit = false;
    }

    this->drawTitle("GENERAL SETTINGS", 12);
    this->drawSelect(Iron::SettingType::GENERAL_SETTINGS, GENERAL_SETTINGS_COUNT, this->maxOptionsVisible, 2, 22);
}

void UserInterface::showPIDPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->optionOffset = 0;
        this->maxOptionsVisible = 3;
        this->minOptionCount = 0;
        this->maxOptionCount = PID_SETTINGS_COUNT;
        this->selectedOption = 1;
        this->viewInit = false;
    }

    this->drawTitle("PID TUNING", 12);
    this->drawSelect(Iron::SettingType::PID_SETTINGS, PID_SETTINGS_COUNT, this->maxOptionsVisible, 2, 22);
}

void UserInterface::showOpAmpPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->optionOffset = 0;
        this->maxOptionsVisible = 3;
        this->minOptionCount = 0;
        this->maxOptionCount = OPAMP_SETTINGS_COUNT;
        this->selectedOption = 1;
        this->viewInit = false;
    }

    this->drawTitle("OP AMP SETTINGS", 12);
    this->drawSelect(Iron::SettingType::OPAMP_SETTINGS, OPAMP_SETTINGS_COUNT, this->maxOptionsVisible, 2, 22);
}

void UserInterface::showCalPointsPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->optionOffset = 0;
        this->maxOptionsVisible = 3;
        this->minOptionCount = 0;
        this->maxOptionCount = CAL_NPOINTS;
        this->selectedOption = 1;
        this->viewInit = false;
    }

    this->drawTitle("TIP CALIBRATION", 12);

    bool selected = false;
    String label;
    uint8_t index = 0;
    int xOffset = 2;
    int yOffset = 22;

    for (uint8_t i = this->optionOffset + 1; i <= CAL_NPOINTS; i++)
    {
        index = i - 1;
        selected = (this->selectedLevel == OPTION_SELECT || this->selectedLevel == OPTION_EDIT) && this->selectedOption == i;

        label = String((int)this->iron->getCalPoint(index).tempC) + "C: ";

        if (this->selectedOption == i && this->selectedLevel == OPTION_EDIT)
        {
            label += String((int)(this->iron->getTargetTemp() - this->iron->getCalPoint(index).tempC));
        }
        else
        {
            label += String(this->iron->getCalPoint(index).mV, 3) + "mV";
        }

        if (selected)
        {
            if (this->selectedOption > this->optionOffset + maxOptionsVisible)
            {
                this->incOptionOffset(CAL_NPOINTS - 1);
            }
            else if (this->selectedOption < this->optionOffset + (maxOptionsVisible - 1))
            {
                this->decOptionOffset();
            }
        }

        this->drawOption(label, u8g2_font_spleen6x12_mf, xOffset, yOffset, 12, 2, selected);

        yOffset += 16;
    }

    // this->lcd->setFont(u8g2_font_spleen5x8_mf);

    label = String((int)this->iron->getHeaterTemp()) + "C";
    this->lcd->drawStr(this->lcd->getWidth() - this->lcd->getStrWidth(label.c_str()) - 2, 22, label.c_str());
    label = String(this->iron->getHeaterTempMV(), 2);
    this->lcd->drawStr(this->lcd->getWidth() - this->lcd->getStrWidth(label.c_str()) - 2, 35, label.c_str());
    label = String((int)this->iron->getHeaterOutputPct()) + "%";
    this->lcd->drawStr(this->lcd->getWidth() - this->lcd->getStrWidth(label.c_str()) - 2, 47, label.c_str());
}

void UserInterface::showResetPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->optionOffset = 0;
        this->maxOptionsVisible = 2;
        this->minOptionCount = 0;
        this->maxOptionCount = 2;
        this->selectedOption = 2;
        this->viewInit = false;
    }
    this->drawTitle("RESET SETTINGS", 12);
}

void UserInterface::drawTitle(String title, uint8_t height)
{
    this->lcd->setDrawColor(2);
    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosCenter();
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(title.c_str()) / 2, height / 2 + 1, title.c_str());

    if (this->selectedLevel == OPTION_SELECT || this->selectedLevel == OPTION_EDIT)
    {
        this->drawOption("X", u8g2_font_spleen5x8_mf, 1, height / 2, 8, 1, this->selectedOption == 0);
        this->lcd->drawFrame(0, 0, this->lcd->getWidth(), this->lcd->getHeight());
    }

    this->lcd->drawBox(1, 1, this->lcd->getWidth() - 2, height - 1);
}

void UserInterface::drawOption(String label, const uint8_t *font, uint8_t x, uint8_t y, uint8_t h, uint8_t p, bool selected)
{
    this->lcd->setFont(font);
    this->lcd->setFontPosCenter();

    uint8_t w = this->lcd->getStrWidth(label.c_str());
    w = w + p * 2 + 1;

    uint8_t height = h + p * 2;

    this->lcd->drawUTF8(x + p + 1, y + 1, label.c_str());

    if (selected)
    {
        if (this->selectedLevel == OPTION_EDIT)
        {
            this->lcd->drawBox(x, y - height / 2, w, height);
        }
        else
        {
            this->lcd->drawFrame(x, y - height / 2, w, height);
        }
    }
}

void UserInterface::drawSelect(Iron::SettingType settingType, uint8_t maxOptions, uint8_t maxOptionsVisible, uint8_t xOffset, uint8_t yOffset)
{
    bool selected = false;
    String label;
    uint8_t index = 0;
    uint8_t decPlaces = 0;
    float value = 0.0f;
    String unit;

    for (uint8_t i = this->optionOffset + 1; i <= maxOptions; i++)
    {
        index = i - 1;
        selected = (this->selectedLevel == OPTION_SELECT || this->selectedLevel == OPTION_EDIT) && this->selectedOption == i;
        decPlaces = this->iron->getSetting(settingType, index).decPlaces;
        value = this->iron->getSetting(settingType, index).value;
        label = String(this->iron->getSetting(settingType, index).label) + String(": ");
        unit = String(this->iron->getSetting(settingType, index).unit);

        if (decPlaces == 0)
        {
            label += String((int)(value)) + unit;
        }
        else
        {
            label += String(value, decPlaces);
        }

        if (selected)
        {
            if (this->selectedOption > this->optionOffset + maxOptionsVisible)
            {
                this->incOptionOffset(maxOptions);
            }
            else if (this->selectedOption < this->optionOffset + (maxOptionsVisible - 1))
            {
                this->decOptionOffset();
            }
        }

        this->drawOption(label, u8g2_font_spleen6x12_mf, xOffset, yOffset, 12, 2, selected);

        yOffset += 16;
    }
}

void UserInterface::drawStatus()
{
    String status;

    switch (this->iron->getStatus())
    {
    case Iron::Status::STATUS_HEATING:
        status = "HEATER ON";
        break;
    case Iron::Status::STATUS_FAULT:
        status = "ERROR";
        break;
    case Iron::Status::STATUS_OFF:
        status = "HEATER OFF";
        break;
    case Iron::Status::STATUS_SCREENSAVER:
    case Iron::Status::STATUS_SLEEP:
        status = "SLEEP";
        break;
    }

    // this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFont(u8g2_font_spleen8x16_mf);
    this->lcd->setFontPosBottom();
    this->lcd->setCursor((this->lcd->getWidth() - this->lcd->getStrWidth(status.c_str())) / 2, this->lcd->getHeight());
    this->lcd->print(status);
}

void UserInterface::drawLoadingBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t percentage)
{
    // Draw outer border
    this->lcd->drawFrame(x, y, width, height);

    // Calculate filled height
    uint8_t fillHeight = (height - 4) * percentage / 100;

    // Draw filled area from bottom upwards
    this->lcd->drawBox(
        x + 2,
        y + height - 2 - fillHeight,
        width - 4,
        fillHeight);
}