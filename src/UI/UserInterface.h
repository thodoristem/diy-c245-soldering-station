#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <Iron/Iron.h>
#include <U8g2lib.h>

#define LCD_INTERVAL_MS 100

class UserInterface
{
public:
    enum View : uint8_t
    {
        DASHBOARD,
        MENU,
        PAGE_GENERAL_SETTINGS,
        PAGE_PID,
        PAGE_OPAMP,
        PAGE_CAL_POINTS,
        PAGE_RESET
    };

    enum SelectLevel : uint8_t
    {
        HOME,
        PAGE_SELECT,
        OPTION_SELECT,
        OPTION_EDIT
    };

private:
    Iron *iron;
    U8G2 *lcd;

    unsigned long lcdStartMS;
    unsigned long lcdEndMS;

    SelectLevel selectedLevel = HOME;
    View selectedView = DASHBOARD;
    bool viewInit = false;
    uint8_t selectedOption = 0;
    uint8_t optionOffset = 0;
    uint8_t maxOptionsVisible = 0;
    uint8_t minOptionCount = 0;
    uint8_t maxOptionCount = 0;

    void prevSelection();
    void nextSelection();

    void drawTitle(String title);
    void drawOption(String label, const uint8_t *font, uint8_t x, uint8_t y, uint8_t h, uint8_t p, bool selected);
    void drawSelect(Iron::SettingType settingType, uint8_t maxOptions, uint8_t maxOptionsVisible, uint8_t x, uint8_t y);
    void drawLoadingBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t percentage);

public:
    UserInterface(Iron *iron, U8G2 *lcd);

    void init();
    void run();

    void onPrev();
    void onNext();
    void onPress();
    void onLongPress();

    void selectView(View view);
    void decOptionOffset();
    void incOptionOffset(uint8_t limit);

    void showIntroMsg();
    void showSaveMsg();
    void showDashboard();
    void showMenu();
    void showGeneralSettingsPage();
    void showCalPointsPage();
    void showPIDPage();
    void showOpAmpPage();
    void showResetPage();
};

#endif