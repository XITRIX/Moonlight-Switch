//
//  full_layout.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 07.01.2022.
//

#include "Settings.hpp"
#include "keyboard_view.hpp"

using namespace brls;

void KeyboardView::createFullLayout() {
    clearViews();
    keyboardLangLock = -1;

    float menuButtonWidth = 74.0f;
    float baseButtonWidth = 74.0f;
    float tabButtonWidth = 120.0f;
    float returnButtonWidth = 138.0f;
    float shiftButtonWidth = 179.0f;
    float funcMargins = 15.0f;

    // ROW 1
    Box* row1 = new Box(Axis::ROW);
    addView(row1);

    std::vector<KeyboardKeys> row1Keys = { VK_ESCAPE, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12, VK_DELETE };

    for (int i = 0; i < row1Keys.size(); i++) {
        float margin = i % 4 == 0 ? funcMargins : 4;

        ButtonView* button = new ButtonView(this);
        button->setKey(row1Keys[i]);
        button->charLabel->setFontSize(21);
        button->setMargins(4, margin, 4, 4);
        button->setWidth(baseButtonWidth);
        row1->addView(button);
    }

    // ROW 2
    Box* row2 = new Box(Axis::ROW);
    addView(row2);

    std::vector<KeyboardKeys> row2Keys = { VK_OEM_3, VK_KEY_1, VK_KEY_2, VK_KEY_3, VK_KEY_4, VK_KEY_5, VK_KEY_6, VK_KEY_7, VK_KEY_8, VK_KEY_9, VK_KEY_0, VK_OEM_MINUS, VK_OEM_PLUS };

    for (int i = 0; i < row2Keys.size(); i++) {
        ButtonView* button = new ButtonView(this);
        button->setKey(row2Keys[i]);
        button->setMargins(4, 4, 4, 4);
        button->setWidth(baseButtonWidth);
        row2->addView(button);
    }

    ButtonView* removeButton = new ButtonView(this);
    removeButton->setKey(VK_BACK);
    removeButton->charLabel->setFontSize(21);
    removeButton->setMargins(4, 4, 4, 4);
    removeButton->setWidth(tabButtonWidth);
    row2->addView(removeButton);

    // ROW 3
    Box* row3 = new Box(Axis::ROW);
    addView(row3);

    std::vector<KeyboardKeys> row3Keys = { VK_KEY_Q, VK_KEY_W, VK_KEY_E, VK_KEY_R, VK_KEY_T, VK_KEY_Y, VK_KEY_U, VK_KEY_I, VK_KEY_O, VK_KEY_P, VK_OEM_4, VK_OEM_6, VK_OEM_5 };

    ButtonView* tabButton = new ButtonView(this);
    tabButton->setKey(VK_TAB);
    tabButton->charLabel->setFontSize(21);
    tabButton->setMargins(4, 4, 4, 4);
    tabButton->setWidth(tabButtonWidth);
    row3->addView(tabButton);

    for (int i = 0; i < row3Keys.size(); i++) {
        ButtonView* button = new ButtonView(this);
        button->setKey(row3Keys[i]);
        button->setMargins(4, 4, 4, 4);
        button->setWidth(baseButtonWidth);
        row3->addView(button);
    }

    // ROW 4
    Box* row4 = new Box(Axis::ROW);
    addView(row4);

    std::vector<KeyboardKeys> row4Keys = { VK_KEY_A, VK_KEY_S, VK_KEY_D, VK_KEY_F, VK_KEY_G, VK_KEY_H, VK_KEY_J, VK_KEY_K, VK_KEY_L, VK_OEM_1, VK_OEM_7 };

    ButtonView* capsButton = new ButtonView(this);
    capsButton->setKey(VK_CAPITAL);
    capsButton->charLabel->setFontSize(21);
    capsButton->setMargins(4, 4, 4, 4);
    capsButton->setWidth(returnButtonWidth);
    row4->addView(capsButton);

    for (int i = 0; i < row4Keys.size(); i++) {
        ButtonView* button = new ButtonView(this);
        button->setKey(row4Keys[i]);
        button->setMargins(4, 4, 4, 4);
        button->setWidth(baseButtonWidth);
        row4->addView(button);
    }

    ButtonView* returnButton = new ButtonView(this);
    returnButton->setKey(VK_RETURN);
    returnButton->charLabel->setFontSize(21);
    returnButton->setMargins(4, 4, 4, 4);
    returnButton->setWidth(returnButtonWidth);
    row4->addView(returnButton);

    // ROW 5
    Box* row5 = new Box(Axis::ROW);
    addView(row5);

    std::vector<KeyboardKeys> row5Keys = { VK_KEY_Z, VK_KEY_X, VK_KEY_C, VK_KEY_V, VK_KEY_B, VK_KEY_N, VK_KEY_M, VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2 };

    ButtonView* lshiftButton = new ButtonView(this);
    lshiftButton->setKey(VK_RSHIFT);
    lshiftButton->triggerType = true;
    lshiftButton->charLabel->setFontSize(21);
    lshiftButton->setMargins(4, 4, 4, 4);
    lshiftButton->setWidth(shiftButtonWidth);
    lshiftButton->event = [] { KeyboardView::shiftUpdated.fire(); };
    row5->addView(lshiftButton);

    for (int i = 0; i < row5Keys.size(); i++) {
        ButtonView* button = new ButtonView(this);
        button->setKey(row5Keys[i]);
        button->setMargins(4, 4, 4, 4);
        button->setWidth(baseButtonWidth);
        row5->addView(button);
    }

    ButtonView* rshiftButton = new ButtonView(this);
    rshiftButton->setKey(VK_RSHIFT);
    rshiftButton->triggerType = true;
    rshiftButton->charLabel->setFontSize(21);
    rshiftButton->setMargins(4, 4, 4, 4);
    rshiftButton->setWidth(shiftButtonWidth);
    rshiftButton->event = [] { KeyboardView::shiftUpdated.fire(); };
    row5->addView(rshiftButton);

    // ROW 6
    Box* row6 = new Box(Axis::ROW);
    addView(row6);

    ButtonView* langButton = new ButtonView(this);
    langButton->charLabel->setText("\ue01d");
    langButton->charLabel->setFontSize(21);
    langButton->setMargins(4, 4, 4, 4);
    langButton->setWidth(menuButtonWidth);
    langButton->event = [this] {
        auto locales = KeyboardView::getLocales();
        std::vector<std::string> langs;

        std::transform(locales.begin(), locales.end(),
                       std::back_inserter(langs),
                       [](KeyboardLocale locale) { return locale.name; });

        Dropdown* dropdown = new Dropdown(
            "Select language", langs,
            [this](int selected) { this->changeLang(selected); },
            Settings::instance().get_keyboard_locale());
        Application::pushActivity(new Activity(dropdown));
    };
    row6->addView(langButton);

    ButtonView* lctrlButton = new ButtonView(this);
    lctrlButton->setKey(VK_RCONTROL);
    lctrlButton->triggerType = true;
    lctrlButton->charLabel->setFontSize(21);
    lctrlButton->setMargins(4, 4, 4, 4);
    lctrlButton->setWidth(menuButtonWidth);
    row6->addView(lctrlButton);

    ButtonView* lwinButton = new ButtonView(this);
    lwinButton->setKey(VK_LWIN);
    lwinButton->charLabel->setFontSize(21);
    lwinButton->setMargins(4, 4, 4, 4);
    lwinButton->setWidth(menuButtonWidth);
    row6->addView(lwinButton);

    ButtonView* laltButton = new ButtonView(this);
    laltButton->setKey(VK_RMENU);
    laltButton->triggerType = true;
    laltButton->charLabel->setFontSize(21);
    laltButton->setMargins(4, 4, 4, 4);
    laltButton->setWidth(menuButtonWidth);
    row6->addView(laltButton);

    ButtonView* spaceButton = new ButtonView(this);
    spaceButton->setKey(VK_SPACE);
    spaceButton->charLabel->setFontSize(21);
    spaceButton->setMargins(4, 4, 4, 4);
    spaceButton->setWidth(530);
    row6->addView(spaceButton);

    ButtonView* leftButton = new ButtonView(this);
    leftButton->setKey(VK_LEFT);
    leftButton->charLabel->setFontSize(21);
    leftButton->setMargins(4, 4, 4, 4);
    leftButton->setWidth(menuButtonWidth);
    row6->addView(leftButton);

    ButtonView* upButton = new ButtonView(this);
    upButton->setKey(VK_UP);
    upButton->charLabel->setFontSize(21);
    upButton->setMargins(4, 4, 4, 4);
    upButton->setWidth(menuButtonWidth);
    row6->addView(upButton);

    ButtonView* downButton = new ButtonView(this);
    downButton->setKey(VK_DOWN);
    downButton->charLabel->setFontSize(21);
    downButton->setMargins(4, 4, 4, 4);
    downButton->setWidth(menuButtonWidth);
    row6->addView(downButton);

    ButtonView* rightButton = new ButtonView(this);
    rightButton->setKey(VK_RIGHT);
    rightButton->charLabel->setFontSize(21);
    rightButton->setMargins(4, 4, 4, 4);
    rightButton->setWidth(menuButtonWidth);
    row6->addView(rightButton);
}
