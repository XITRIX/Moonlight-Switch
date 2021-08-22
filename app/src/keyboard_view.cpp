//
//  keyboard_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 14.06.2021.
//

#include "keyboard_view.hpp"
#include <chrono>
#include <libretro-common/retro_timers.h>

using namespace brls;

short KeyboardCodes[_VK_KEY_MAX]
{
    0x08, 0x1B, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x0D, 0x20,
    0xA3, 0xA5, 0xA1, 0x5B, 0xBE, 0xBC, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x09, 0x2E,
    0xBA, 0xBF, 0xC0, 0xDB, 0xDC, 0xDD, 0xDE,
};

std::string KeyboardLocalization[_VK_KEY_MAX]
{
    "Remove", "Esc", "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", "a", "b", "c", "d", "e", "f", "g", "h",
    "i", "j", "k", "l", "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x", "y", "z", "Return", "Space",
    "Ctrl", "Alt", "Shift", "Win", ".", ",", "F1", "F2", "F3", "F4",
    "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "TAB", "Delete",
    ";", "/", "`", "[", "\\", "]", "'",
};

std::string ShiftKeyboardLocalization[_VK_KEY_MAX]
{
    "Remove", "Esc", ")", "!", "@", "#", "$", "%", "^", "&",
    "*", "(", "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R",
    "S", "T", "U", "V", "W", "X", "Y", "Z", "Return", "Space",
    "Ctrl", "Alt", "Shift", "Win", ">", "<", "F1", "F2", "F3", "F4",
    "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "TAB", "Delete",
    ":", "?", "`", "{", "|", "}", "\"",
};

std::chrono::high_resolution_clock::time_point rumbleLastButtonClicked;
bool rumblingActive = false;

bool keysState[_VK_KEY_MAX];
brls::InputManager* inputManager = nullptr;

void startRumbling()
{
    rumbleLastButtonClicked = std::chrono::high_resolution_clock::now();
    if (!rumblingActive)
    {
        rumblingActive = true;
        inputManager->sendRumble(0, 32512, 32512);
    }
}


ButtonView::ButtonView()
{
    if (inputManager == nullptr)
        inputManager = Application::getPlatform()->getInputManager();
    
    setBackgroundColor(RGB(60, 60, 60));
    setWidth(90);
    setHeight(56);
    
    setAlignItems(AlignItems::CENTER);
    setJustifyContent(JustifyContent::CENTER);
    
    charLabel = new Label();
    charLabel->setHorizontalAlign(HorizontalAlign::CENTER);
    charLabel->setVerticalAlign(VerticalAlign::CENTER);
    charLabel->setTextColor(RGB(255, 255, 255));
    charLabel->setFontSize(27);
    addView(charLabel);
    
    setCornerRadius(8);
    setShadowVisibility(true);
    setShadowType(ShadowType::GENERIC);
    
    shiftSubscription = KeyboardView::shiftUpdated.subscribe([this]{
        applyTitle();
    });
    
    registerCallback();
}

ButtonView::~ButtonView()
{
    KeyboardView::shiftUpdated.unsubscribe(shiftSubscription);
}

void ButtonView::applyTitle()
{
    if (dummy) return;
    
    bool shifted = keysState[VK_RSHIFT];
    charLabel->setText(shifted ? ShiftKeyboardLocalization[key] : KeyboardLocalization[key]);
}

void ButtonView::setKey(KeyboardKeys key)
{
    this->dummy = false;
    this->key = key;
    this->applyTitle();
    
    if (keysState[key])
        this->playClickAnimation(false);
}

void ButtonView::registerCallback()
{
    TapGestureRecognizer* tapRecognizer = new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
        if (!triggerType)
        {
            this->playClickAnimation(status.state != brls::GestureState::START);
            
            switch (status.state) {
                case brls::GestureState::START:
                    startRumbling();

                    if (!dummy)
                        keysState[key] = true;
                    break;
                case brls::GestureState::END:
                case brls::GestureState::FAILED:
                    if (!dummy)
                        keysState[key] = false;
                    
                    if (event != NULL)
                        event();
                    break;
                default:
                    if (!dummy)
                        keysState[key] = false;
                    break;
            }
        }
        else
        {
            switch (status.state) {
                case brls::GestureState::START:
                    startRumbling();
                    if (!keysState[key])
                        this->playClickAnimation(false);
                    break;
                case brls::GestureState::FAILED:
                    if (!keysState[key])
                        this->playClickAnimation(true);
                    break;
                case brls::GestureState::END:
                    keysState[key] = !keysState[key];
                    if (event != NULL)
                        event();
                    
                    if (!keysState[key])
                        this->playClickAnimation(!keysState[key]);
                    break;
                default:
                    break;
            }
        }
    });
    tapRecognizer->setForceRecognision(true);
    addGestureRecognizer(tapRecognizer);
}

KeyboardView::KeyboardView()
    : Box(Axis::COLUMN)
{
    for (int i = 0; i < _VK_KEY_MAX; i++)
        keysState[i] = false;
    
    setBackgroundColor(RGBA(120, 120, 120, 200));
    setAlignItems(AlignItems::CENTER);
    setPaddingTop(24);
    setPaddingBottom(24);
    
    createEnglishLayout();
    
    addGestureRecognizer(new TapGestureRecognizer([](TapGestureStatus status, Sound* sound){}));
    addGestureRecognizer(new PanGestureRecognizer([](PanGestureStatus status, Sound* sound){}, PanAxis::ANY));
}

KeyboardView::~KeyboardView()
{ }

void KeyboardView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) 
{
    Box::draw(vg, x, y, width, height, style, ctx);

    if (rumblingActive) 
    {
        auto timeNow = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - rumbleLastButtonClicked).count();
        if (duration >= 100)
        {
            inputManager->sendRumble(0, 0, 0);
            rumblingActive = false;
        }
    }
}

KeyboardState KeyboardView::getKeyboardState()
{
    KeyboardState state;
    
    for (int i = 0; i < _VK_KEY_MAX; i++)
        state.keys[i] = keysState[i];
    
    return state;
}

short KeyboardView::getKeyCode(KeyboardKeys key)
{
    return KeyboardCodes[key];
}
