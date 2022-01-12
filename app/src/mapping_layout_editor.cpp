//
//  mapping_layout_editor.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 09.10.2021.
//

#include "mapping_layout_editor.hpp"
#include "button_selecting_dialog.hpp"

#include <borealis/platforms/switch/swkbd.hpp>

using namespace brls;

MappingLayoutEditor::MappingLayoutEditor(int layoutNumber,
                                         std::function<void(void)> dismissCb)
    : layoutNumber(layoutNumber), dismissCb(dismissCb) {
    std::vector<KeyMappingLayout>* layouts =
        Settings::instance().get_mapping_laouts();

    getAppletFrameItem()->title = "mapping_layout_editor/title"_i18n;
    setJustifyContent(JustifyContent::SPACE_EVENLY);
    setAlignItems(AlignItems::CENTER);

    titleLabel = new brls::Label();
    titleLabel->setText(brls::Hint::getKeyIcon(ControllerButton::BUTTON_RB) +
                        "  " + layouts->at(layoutNumber).title);
    titleLabel->setFontSize(24);
    titleLabel->setMargins(4, 16, 4, 16);

    Box* holder = new Box();
    holder->addView(titleLabel);
    holder->setFocusable(true);
    holder->setCornerRadius(6);
    holder->setMargins(18, 0, 8, 0);
    holder->addGestureRecognizer(new TapGestureRecognizer(holder));

    getAppletFrameItem()->hintView = holder;

    registerAction(
        "_rename", BUTTON_RB,
        [this](View* view) {
            this->renameLayout();
            return true;
        },
        true);

    holder->registerAction(
        "_rename", BUTTON_RB,
        [this](View* view) {
            this->renameLayout();
            return true;
        },
        true);

    holder->registerClickAction([this](View* view) {
        this->renameLayout();
        return true;
    });

    registerAction("mapping_layout_editor/remove"_i18n, BUTTON_BACK,
                   [this](View* view) {
                       this->removeLayout();
                       return true;
                   });

    holder->registerAction("mapping_layout_editor/remove"_i18n, BUTTON_BACK,
                           [this](View* view) {
                               this->removeLayout();
                               return true;
                           });

    Box* containers[4];
    for (int i = 0; i < 4; i++) {
        Box* container = new Box(Axis::COLUMN);
        containers[i] = container;
        addView(container);
    }

    ControllerButton buttons[16] = {
        BUTTON_LT,   BUTTON_LB, BUTTON_BACK,  BUTTON_LSB,
        BUTTON_LEFT, BUTTON_UP, BUTTON_RIGHT, BUTTON_DOWN,
        BUTTON_A,    BUTTON_B,  BUTTON_X,     BUTTON_Y,
        BUTTON_RT,   BUTTON_RB, BUTTON_START, BUTTON_RSB,
    };

    int counter = 0;
    for (ControllerButton button : buttons) {
        Button* buttonView = new Button();
        buttonView->setStyle(&BUTTONSTYLE_BORDERED);

        buttonView->registerAction(
            "mapping_layout_editor/reset"_i18n, BUTTON_Y,
            [this, buttonView, button, layouts](View* view) {
                buttonView->setText(Hint::getKeyIcon(button, true));
                buttonView->setTextColor(Application::getTheme()["brls/text"]);
                buttonView->setActionAvailable(BUTTON_Y, false);

                layouts->at(this->layoutNumber).mapping.erase(button);
                return true;
            });

        if (layouts->at(layoutNumber).mapping.count(button) == 1) {
            buttonView->setText(
                Hint::getKeyIcon(button, true) + " \uE090 " +
                Hint::getKeyIcon((ControllerButton)layouts->at(layoutNumber)
                                     .mapping.at(button),
                                 true));
            buttonView->setTextColor(Application::getTheme()["brls/accent"]);
            buttonView->setActionAvailable(BUTTON_Y, true);
        } else {
            buttonView->setText(Hint::getKeyIcon(button, true));
            buttonView->setTextColor(Application::getTheme()["brls/text"]);
            buttonView->setActionAvailable(BUTTON_Y, false);
        }

        buttonView->setFontSize(24);
        buttonView->setMarginTop(4);
        buttonView->setMarginBottom(4);
        buttonView->setSize(Size(144, 50));

        buttonView->registerClickAction([this, layouts, buttonView,
                                         button](View* view) {
            ButtonSelectingDialog* dialog = ButtonSelectingDialog::create(
                "mapping_layout_editor/selection_dialog_title"_i18n + " " +
                    Hint::getKeyIcon(button, true),
                [this, layouts, buttonView,
                 button](std::vector<ControllerButton> selectedButtons) {
                    if (selectedButtons.size() > 0) {
                        if (selectedButtons[0] == button) {
                            buttonView->setText(Hint::getKeyIcon(button, true));
                            buttonView->setTextColor(
                                Application::getTheme()["brls/text"]);
                            buttonView->setActionAvailable(BUTTON_Y, false);

                            layouts->at(this->layoutNumber)
                                .mapping.erase(button);
                        } else {
                            buttonView->setText(
                                Hint::getKeyIcon(button, true) + " \uE090 " +
                                Hint::getKeyIcon(
                                    (ControllerButton)selectedButtons[0],
                                    true));
                            buttonView->setTextColor(
                                Application::getTheme()["brls/accent"]);
                            buttonView->setActionAvailable(BUTTON_Y, true);

                            layouts->at(this->layoutNumber).mapping[button] =
                                selectedButtons[0];
                        }
                    }
                },
                true);
            dialog->open();
            return true;
        });

        int index = counter++ / 4;
        containers[index]->addView(buttonView);
    }
}

void MappingLayoutEditor::renameLayout() {
    std::string title =
        Settings::instance().get_mapping_laouts()->at(layoutNumber).title;
    Swkbd::openForText(
        [this](std::string text) {
            this->titleLabel->setText(
                brls::Hint::getKeyIcon(ControllerButton::BUTTON_RB, true) +
                "  " + text);
            Settings::instance().get_mapping_laouts()->at(layoutNumber).title =
                text;
        },
        "mapping_layout_editor/rename_title"_i18n, "", 20, title, 0);
}

void MappingLayoutEditor::removeLayout() {
    Dialog* dialog =
        new Dialog("mapping_layout_editor/remove_dialog_title"_i18n);
    dialog->addButton("common/cancel"_i18n, []() {});
    dialog->addButton("common/remove"_i18n, [this]() {
        int current = Settings::instance().get_current_mapping_layout();
        if (this->layoutNumber == current) {
            Settings::instance().set_current_mapping_layout(0);
        } else if (this->layoutNumber < current) {
            Settings::instance().set_current_mapping_layout(current - 1);
        }

        auto layouts = Settings::instance().get_mapping_laouts();
        layouts->erase(layouts->begin() + layoutNumber);

        dismiss();
    });
    dialog->open();
}

void MappingLayoutEditor::dismiss(std::function<void(void)> cb) {
    View::dismiss(cb);
    dismissCb();
}

View* MappingLayoutEditor::getParentNavigationDecision(
    View* from, View* newFocus, FocusDirection direction) {
    if (newFocus && (direction == FocusDirection::LEFT ||
                     direction == FocusDirection::RIGHT)) {
        View* source = Application::getCurrentFocus();
        void* currentparentUserData = source->getParentUserData();
        void* nextParentUserData = newFocus->getParentUserData();

        size_t currentFocusIndex = *((size_t*)currentparentUserData);
        size_t nextFocusIndex = *((size_t*)nextParentUserData);

        if (currentFocusIndex < 0 ||
            currentFocusIndex >= source->getParent()->getChildren().size())
            return Box::getParentNavigationDecision(from, newFocus, direction);

        if (newFocus->getParent()->getChildren().size() <= currentFocusIndex) {
            newFocus = newFocus->getParent()->getChildren()
                           [newFocus->getParent()->getChildren().size() - 1];
            return Box::getParentNavigationDecision(from, newFocus, direction);
        }

        while (newFocus && nextFocusIndex < currentFocusIndex) {
            newFocus = newFocus->getNextFocus(FocusDirection::DOWN, newFocus);
            if (!newFocus)
                break;

            nextParentUserData = newFocus->getParentUserData();
            nextFocusIndex = *((size_t*)nextParentUserData);
        }

        while (newFocus && nextFocusIndex > currentFocusIndex) {
            newFocus = newFocus->getNextFocus(FocusDirection::UP, newFocus);
            if (!newFocus)
                break;

            nextParentUserData = newFocus->getParentUserData();
            nextFocusIndex = *((size_t*)nextParentUserData);
        }
    }
    return Box::getParentNavigationDecision(from, newFocus, direction);
}

MappingLayoutEditor::~MappingLayoutEditor() { Settings::instance().save(); }
