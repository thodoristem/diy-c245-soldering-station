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
}

void UserInterface::run()
{
    unsigned long lcdEndMS = millis();

    if (lcdEndMS - lcdStartMS >= LCD_INTERVAL_MS)
    {
        lcdStartMS = lcdEndMS;

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
        case PAGE_CAL_POINTS:
            this->showCalPointsPage();
            break;
        case PAGE_PID:
            this->showPIDPage();
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
        this->iron->decreaseTargetTemp();
        break;
    case OPTION_EDIT:
        switch (this->selectedView)
        {
        case PAGE_CAL_POINTS:
            this->iron->decreaseTargetTemp(1);
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
        this->iron->increaseTargetTemp();
        break;
    case OPTION_EDIT:
        switch (this->selectedView)
        {
        case PAGE_CAL_POINTS:
            this->iron->increaseTargetTemp(1);
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
            this->selectView(PAGE_CAL_POINTS);
            break;
        case 2:
            this->selectView(PAGE_PID);
            break;
        case 3:
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
                this->iron->setTargetTemp(this->iron->getCalPoint(this->selectedOption).tempC);
                this->iron->setStatus(Iron::Status::STATUS_HEATING);
                break;
            default:
                break;
            }
        }
        break;
    case OPTION_EDIT:
        switch (this->selectedView)
        {
        case PAGE_CAL_POINTS:
            this->selectedLevel = OPTION_SELECT;
            this->iron->init();
            this->iron->setCalPointMv(this->selectedOption, this->iron->getHeaterTempMV());
            // this->iron->tempCoefficientCalculation();
            this->iron->setStatus(Iron::Status::STATUS_IDLE);
            this->iron->setTargetTemp(this->iron->getSettings().initTemp);
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
        break;
    default:
        this->selectView(View::DASHBOARD);
        break;
    }
}

void UserInterface::onSleepSense()
{
    this->iron->sleep();
}

void UserInterface::onWakeSense()
{
    this->iron->wakeUp();
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
}

void UserInterface::showDashboard()
{
    if (this->viewInit)
    {
        this->selectedLevel = HOME;
        this->minOptionCount = 0;
        this->maxOptionCount = 0;
        this->selectedOption = 0;
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
    this->lcd->setFont(u8g2_font_spleen6x12_mf);
    this->lcd->drawUTF8(14, 0, ironSetTemp);
    this->lcd->drawUTF8(this->lcd->getWidth() - this->lcd->getStrWidth(ironOutput) - 14, 0, ironOutput);

    this->drawLoadingBar(0, 0, 6, this->lcd->getHeight(), this->iron->getTargetTempPct());
    this->drawLoadingBar(5, 0, 6, this->lcd->getHeight(), this->iron->getHeaterTempPct());
    this->drawLoadingBar(this->lcd->getWidth() - 11, 0, 11, this->lcd->getHeight(), this->iron->getHeaterOutputPct());

    this->lcd->drawUTF8((this->lcd->getWidth() / 2 + 24), this->lcd->getHeight() / 2 - 14, "\u00B0C");
    this->lcd->setFont(u8g2_font_spleen16x32_mu);
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
        this->maxOptionCount = 3;
        this->selectedOption = 0;
        this->viewInit = false;
        this->iron->setStatus(Iron::Status::STATUS_IDLE);
    }

    switch (this->selectedOption)
    {
    case 0:
        this->showGeneralSettingsPage();
        break;
    case 1:
        this->showCalPointsPage();
        break;
    case 2:
        this->showPIDPage();
        break;
    case 3:
        this->showResetPage();
        break;
    }
}

void UserInterface::showGeneralSettingsPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->minOptionCount = 0;
        this->maxOptionCount = 5;
        this->selectedOption = 0;
        this->viewInit = false;
    }

    this->drawTitle("GENERAL SETTINGS", 12);
}

void UserInterface::showCalPointsPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->minOptionCount = 0;
        this->maxOptionCount = CAL_NPOINTS - 1;
        this->selectedOption = 1;
        this->viewInit = false;
    }

    String title = "CALIBRATION (";
    title += String((int)this->iron->getHeaterTemp());
    title += "C)";

    this->drawTitle(title, 12);

    int xOffset = 5;
    int yOffset = 20;
    String label;

    this->lcd->setFont(u8g2_font_spleen5x8_mf);

    for (int i = 1; i < CAL_NPOINTS; i++)
    {
        label = String((int)this->iron->getCalPoint(i).tempC) + "C:";

        if (this->selectedOption == i && this->selectedLevel == OPTION_EDIT)
        {
            label += String(this->iron->getTargetTemp() - this->iron->getCalPoint(i).tempC, 0);
        }
        else
        {
            label += String(this->iron->getSettings().calPoints[i].mV, 3);
        }

        if (i == CAL_NPOINTS / 2 + 1)
        {
            xOffset = this->lcd->getWidth() / 2;
            yOffset = 20;
        }

        this->drawOption(label, xOffset, yOffset, 2, (this->selectedLevel == OPTION_SELECT || this->selectedLevel == OPTION_EDIT) && this->selectedOption == i);

        yOffset += 12;
    }
}

void UserInterface::showPIDPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->minOptionCount = 0;
        this->maxOptionCount = 5;
        this->selectedOption = 1;
        this->viewInit = false;
    }
    this->drawTitle("PID TUNING", 12);
}

void UserInterface::showResetPage()
{
    if (this->viewInit && this->selectedLevel == PAGE_SELECT)
    {
        this->selectedLevel = OPTION_SELECT;
        this->minOptionCount = 0;
        this->maxOptionCount = 5;
        this->selectedOption = 1;
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
        this->drawOption("X", 1, height / 2, 1, this->selectedOption == 0);
        this->lcd->drawFrame(0, 0, this->lcd->getWidth(), this->lcd->getHeight());
    }

    this->lcd->drawBox(1, 1, this->lcd->getWidth() - 2, height - 1);
}

void UserInterface::drawOption(String label, uint8_t x, uint8_t y, uint8_t p, bool selected)
{
    this->lcd->setFont(u8g2_font_spleen5x8_mf);
    this->lcd->setFontPosCenter();

    uint8_t w = this->lcd->getStrWidth(label.c_str());
    w = w + p * 2 + 1;

    uint8_t h = 8 + p * 2;

    if (selected)
    {
        this->lcd->drawFrame(x, y - h / 2, w, h);
    }

    this->lcd->drawStr(x + p + 1, y + 1, label.c_str());
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
    case Iron::Status::STATUS_IDLE:
        status = "IDLE";
        break;
    case Iron::Status::STATUS_SLEEP:
        status = "SLEEP";
        break;
    }

    this->lcd->setFont(u8g2_font_spleen6x12_mf);
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