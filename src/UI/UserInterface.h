#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <Iron/Iron.h>
#include <U8g2lib.h>

#define BTN_DEBOUNCE_MS 10
#define BTN_LONG_PRESS_MS 1000

#define LCD_INTERVAL_MS 60

class UserInterface
{
public:
    enum View : uint8_t
    {
        DASHBOARD,
        MENU,
        PAGE_GENERAL_SETTINGS,
        PAGE_CAL_POINTS,
        PAGE_PID,
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

    SelectLevel selectedLevel = HOME;
    View selectedView = DASHBOARD;
    bool viewInit = false;
    uint8_t selectedOption = 0;
    uint8_t minOptionCount = 0;
    uint8_t maxOptionCount = 0;

    bool btnEventPaused = false;

    void prevSelection();
    void nextSelection();

    void drawTitle(String title, uint8_t height);
    void drawOption(String label, uint8_t x, uint8_t y, uint8_t p, bool selected);
    void drawLoadingBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t percentage);
    void drawStatus();

public:
    UserInterface(Iron *iron, U8G2 *lcd);

    void init();
    void run();

    void onEncPrev();
    void onEncNext();
    void onEncPress();
    void onEncLongPress();

    void selectView(View view);

    void showDashboard();
    void showMenu();
    void showGeneralSettingsPage();
    void showCalPointsPage();
    void showPIDPage();
    void showResetPage();
};

#endif