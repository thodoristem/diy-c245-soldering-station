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
            this->iron->init(false);
        }
        else
        {
            this->selectedLevel = OPTION_EDIT;
            switch (this->selectedView)
            {
            case PAGE_RESET:
                this->selectedLevel = HOME;
                this->selectView(View::DASHBOARD);
                if (this->selectedOption == 1)
                {
                    this->iron->resetSettings();
                }
                else if (this->selectedOption == 2)
                {
                    this->selectedLevel = PAGE_SELECT;
                    this->selectView(MENU);
                }
                break;
            case PAGE_CAL_POINTS:
                this->iron->initCal();
                this->iron->setTargetTemp(this->iron->getCalPoint(this->selectedOption - 1).tempC);
                this->iron->setStatus(Iron::Status::STATUS_HEATING);
                break;
            default:
                break;
            }
        }
        break;
    case OPTION_EDIT:
        this->selectedLevel = OPTION_SELECT;
        switch (this->selectedView)
        {
        case PAGE_CAL_POINTS:
            this->iron->setCalPointMv(this->selectedOption - 1, this->iron->getHeaterTempMV());
            this->iron->init(false);
            break;
        default:
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
        this->iron->off();
        this->iron->dontSleep(true);
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
    if (this->selectedOption > this->minOptionCount)
    {
        this->selectedOption--;
    }
    else
    {
        this->selectedOption = this->minOptionCount;
    }
}

void UserInterface::nextSelection()
{
    if (this->selectedOption < this->maxOptionCount)
    {
        this->selectedOption++;
    }
    else
    {
        this->selectedOption = this->maxOptionCount;
    }
}

void UserInterface::selectView(View view)
{
    this->viewInit = true;
    this->selectedView = view;
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

void UserInterface::incOptionOffset(uint8_t max)
{
    if (this->optionOffset < max)
    {
        this->optionOffset++;
    }
    else
    {
        this->optionOffset = max;
    }
}

void UserInterface::showIntroMsg()
{
    String label;
    const uint8_t yOffset = (this->lcd->getHeight() - 13 * 4) / 2;

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosTop();
    this->lcd->setDrawColor(1);

    this->lcd->clearBuffer();

    this->lcd->drawRFrame(0, 0, this->lcd->getWidth(), this->lcd->getHeight(), 8);

    label = String("DIY");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, yOffset, label.c_str());
    label = String("C245/T12");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, 13 + yOffset, label.c_str());
    label = String("SOLDERING STATION");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, 13 * 2 + yOffset, label.c_str());
    label = String("By T.Temourtzidis");
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2, 13 * 3 + yOffset, label.c_str());

    this->lcd->sendBuffer();

    delay(3000);
}

void UserInterface::showSaveMsg()
{
    String label;

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosCenter();
    this->lcd->setDrawColor(1);
    this->lcd->clearBuffer();
    this->lcd->drawRFrame(0, 0, this->lcd->getWidth(), this->lcd->getHeight(), 8);

    if (this->iron->getConfig().changed)
    {
        label = "Saving Settings...";
    }
    else
    {
        label = "No changes!";
    }

    this->lcd->drawStr(
        this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2,
        this->lcd->getHeight() / 2,
        label.c_str());

    this->lcd->sendBuffer();

    delay(3000);
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

    char ironTemp[4];
    char ironSetTemp[10];
    char ironOutput[10];
    char tip[4];

    if (this->iron->getStatus() == Iron::Status::STATUS_FAULT)
    {
        sprintf(ironTemp, "---");
    }
    else
    {
        sprintf(ironTemp, "%03d", (int)this->iron->getHeaterTemp());
    }

    sprintf(ironSetTemp, "%03d\u00B0C", (int)this->iron->getTargetTemp());
    sprintf(ironOutput, "%3d%%", (int)this->iron->getHeaterOutputPct());
    sprintf(tip, "%d", (int)(this->iron->getTip() + 1));

    this->drawLoadingBar(0, 0, 7, this->lcd->getHeight(), this->iron->getTargetTempPct());
    this->drawLoadingBar(6, 0, 7, this->lcd->getHeight(), this->iron->getHeaterTempPct());
    this->drawLoadingBar(this->lcd->getWidth() - 13, 0, 13, this->lcd->getHeight(), this->iron->getHeaterOutputPct());

    this->lcd->setFontPosTop();
    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->drawUTF8(15, 2, ironSetTemp);
    this->lcd->drawUTF8(this->lcd->getWidth() - this->lcd->getStrWidth(ironOutput) - 15, 2, ironOutput);

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->drawStr(18, 22, "TIP");
    this->lcd->drawStr(18 + (18 - this->lcd->getStrWidth(tip)) / 2, 32, tip);
    
    this->lcd->setFont(u8g2_font_spleen16x32_mn);
    this->lcd->setFontPosCenter();
    this->lcd->setCursor((this->lcd->getWidth() - this->lcd->getStrWidth(ironTemp)) / 2, this->lcd->getHeight() / 2);
    this->lcd->print(ironTemp);

    this->lcd->setFontPosTop();
    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->drawUTF8(this->lcd->getWidth() / 2 + 23, this->lcd->getHeight() / 2 - 10, "\u00B0C");

    String status;

    switch (this->iron->getStatus())
    {
    case Iron::Status::STATUS_HEATING:
        status = "HEATING";
        break;
    case Iron::Status::STATUS_FAULT:
        status = "NO TIP";
        break;
    case Iron::Status::STATUS_OFF:
        status = "OFF";
        break;
    case Iron::Status::STATUS_SLEEP:
        status = "SLEEP";
        break;
    }

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosTop();
    this->lcd->setCursor(
        (this->lcd->getWidth() - this->lcd->getStrWidth(status.c_str())) / 2,
        this->lcd->getHeight() - 14);
    this->lcd->print(status);
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

    this->drawTitle("GENERAL");
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

    this->drawTitle("PID TUNING");
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

    this->drawTitle("OP-AMP");
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

    if (this->iron->getStatus() == Iron::Status::STATUS_FAULT && this->selectedLevel == OPTION_EDIT)
    {
        this->selectedLevel = OPTION_SELECT;
    }

    this->drawTitle("TIP-" + String(this->iron->getTip() + 1) + " CALIB.");
    bool selected = false;
    String label;
    uint8_t index = 0;
    int xOffset = 2;
    int yOffset = 22;

    for (uint8_t i = this->optionOffset + 1; i <= CAL_NPOINTS; i++)
    {
        index = i - 1;
        selected = (this->selectedLevel == OPTION_SELECT || this->selectedLevel == OPTION_EDIT) && this->selectedOption == i;

        label = String((int)this->iron->getCalPoint(index).tempC) + "C:";

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
            else if (this->selectedOption < this->optionOffset + maxOptionsVisible - 1)
            {
                this->decOptionOffset();
            }
        }

        this->drawOption(label, u8g2_font_spleen6x12_mf, xOffset, yOffset, 12, 2, selected);

        yOffset += 16;
    }

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosTop();

    yOffset = (this->lcd->getHeight() - 12 * 3 + 12) / 2;

    this->lcd->drawRFrame(this->lcd->getWidth() - 7 * 5 - 5, yOffset, 7 * 5 + 2, 12 * 3, 4);

    label = String((int)this->iron->getHeaterTemp()) + "C";
    this->lcd->drawStr(this->lcd->getWidth() - this->lcd->getStrWidth(label.c_str()) - 6, yOffset + 3, label.c_str());

    label = String(this->iron->getHeaterTempMV(), 2);
    this->lcd->drawStr(this->lcd->getWidth() - this->lcd->getStrWidth(label.c_str()) - 6, yOffset + 13, label.c_str());

    label = String((int)this->iron->getHeaterOutputPct()) + "%";
    this->lcd->drawStr(this->lcd->getWidth() - this->lcd->getStrWidth(label.c_str()) - 6, yOffset + 23, label.c_str());
}

void UserInterface::showResetPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->optionOffset = 0;
        this->maxOptionsVisible = 0;
        this->minOptionCount = 0;
        this->maxOptionCount = 2;
        this->selectedOption = 0;
        this->viewInit = false;
    }

    this->drawTitle("RESET");

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosCenter();

    String label = "Reset all settings";
    this->lcd->drawStr(
        this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2,
        this->lcd->getHeight() / 2 - 8,
        label.c_str());

    label = "to default?";
    this->lcd->drawStr(
        this->lcd->getWidth() / 2 - this->lcd->getStrWidth(label.c_str()) / 2,
        this->lcd->getHeight() / 2 + 4,
        label.c_str());

    label = "YES";
    this->drawOption(
        label, u8g2_font_spleen6x12_mf,
        10,
        this->lcd->getHeight() - 12, 12, 4, this->selectedOption == 1);

    label = "NO";
    this->drawOption(
        label, u8g2_font_spleen6x12_mf,
        this->lcd->getWidth() - this->lcd->getStrWidth(label.c_str()) - 18,
        this->lcd->getHeight() - 12, 12, 4, this->selectedOption == 2);
}

void UserInterface::drawTitle(String title)
{
    uint8_t height = 13;

    this->lcd->setDrawColor(2);
    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->setFontPosCenter();
    this->lcd->drawStr(this->lcd->getWidth() / 2 - this->lcd->getStrWidth(title.c_str()) / 2, height / 2 + 2, title.c_str());

    String options = String(this->selectedOption + 1) + "/" + String(this->maxOptionCount + 1);
    this->lcd->drawStr(3, height / 2 + 2, options.c_str());

    if (this->selectedLevel == OPTION_SELECT || this->selectedLevel == OPTION_EDIT)
    {
        this->drawOption(
            "X",
            u8g2_font_spleen5x8_mf,
            this->lcd->getWidth() - 9,
            height / 2 + 1,
            8,
            1,
            this->selectedOption == 0);
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
            this->lcd->drawRBox(x, y - height / 2, w, height, 0);
        }
        else
        {
            this->lcd->drawRFrame(x, y - height / 2, w, height, 0);
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
        label = String(this->iron->getSetting(settingType, index).label) + String(":");
        unit = String(this->iron->getSetting(settingType, index).unit);

        if (strcmp(unit.c_str(), "tip") == 0)
        {
            label += "TIP-" + String((int)(value + 1));
        }
        else if (strcmp(unit.c_str(), "bool") == 0)
        {
            label += String((value == 1) ? "ON" : "OFF");
        }
        else if (strcmp(unit.c_str(), "pmode") == 0)
        {
            switch ((QuickPID::pMode)value)
            {
            case QuickPID::pMode::pOnError:
                label += "ON ERROR";
                break;
            case QuickPID::pMode::pOnMeas:
                label += "ON MEAS";
                break;
            case QuickPID::pMode::pOnErrorMeas:
                label += "ON ERROR+MEAS";
                break;
            }
        }
        else if (strcmp(unit.c_str(), "dmode") == 0)
        {
            switch ((QuickPID::dMode)value)
            {
            case QuickPID::dMode::dOnError:
                label += "ON ERROR";
                break;
            case QuickPID::dMode::dOnMeas:
                label += "ON MEAS";
                break;
            }
        }
        else if (strcmp(unit.c_str(), "imode") == 0)
        {
            switch ((QuickPID::iAwMode)value)
            {
            case QuickPID::iAwMode::iAwCondition:
                label += "CONDITION";
                break;
            case QuickPID::iAwMode::iAwClamp:
                label += "CLAMP";
                break;
            case QuickPID::iAwMode::iAwOff:
                label += "OFF";
                break;
            }
        }
        else if (decPlaces == 0)
        {
            label += String((int)(value)) + unit;
        }
        else
        {
            label += String(value, decPlaces) + unit;
        }

        if (selected)
        {
            if (this->selectedOption > this->optionOffset + maxOptionsVisible)
            {
                this->incOptionOffset(maxOptions);
            }
            else if (this->selectedOption < this->optionOffset + maxOptionsVisible - 1)
            {
                this->decOptionOffset();
            }
        }

        this->drawOption(label, u8g2_font_spleen6x12_mf, xOffset, yOffset, 12, 2, selected);

        yOffset += 16;
    }
}

void UserInterface::drawLoadingBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t percentage)
{
    this->lcd->drawFrame(x, y, width, height);
    uint8_t fillHeight = (height - 4) * percentage / 100;
    this->lcd->drawBox(x + 2, y + height - 2 - fillHeight, width - 4, fillHeight);
}