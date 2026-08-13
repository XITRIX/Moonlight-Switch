#include "key_combo_settings.hpp"

#include "button_selecting_dialog.hpp"
#include "helper.hpp"
#include <fmt/format.h>

using namespace brls;
using namespace brls::literals;

namespace {
std::string action_title(KeyComboAction action) {
    switch (action) {
    case KeyComboAction::GUIDE:
        return "settings/guide_key"_i18n;
    case KeyComboAction::OVERLAY:
        return "settings/overlay"_i18n;
    case KeyComboAction::MOUSE_INPUT:
        return "settings/mouse_input"_i18n;
    case KeyComboAction::HOST_CLOSE_APP:
        return "settings/host_close_app"_i18n;
    case KeyComboAction::HOST_SWITCH_WINDOW:
        return "settings/host_switch_window"_i18n;
    }

    return {};
}

std::string setup_message(KeyComboAction action) {
    switch (action) {
    case KeyComboAction::GUIDE:
        return "settings/guide_key_setup_message"_i18n;
    case KeyComboAction::OVERLAY:
        return "settings/overlay_setup_message"_i18n;
    case KeyComboAction::MOUSE_INPUT:
        return "settings/mouse_input_setup_message"_i18n;
    case KeyComboAction::HOST_CLOSE_APP:
    case KeyComboAction::HOST_SWITCH_WINDOW:
        return fmt::format(fmt::runtime(
                               "settings/host_shortcut_setup_message"_i18n),
                           action_title(action));
    }

    return {};
}

bool is_host_shortcut(KeyComboAction action) {
    return action == KeyComboAction::HOST_CLOSE_APP ||
           action == KeyComboAction::HOST_SWITCH_WINDOW;
}
}

void setupKeyComboCell(DetailCell* cell,
                       const std::vector<ControllerButton>& buttons) {
    std::string text;
    for (size_t i = 0; i < buttons.size(); i++) {
        text += Hint::getKeyIcon(buttons[i], true);
        if (i + 1 < buttons.size())
            text += " + ";
    }

    if (text.empty())
        text = "hints/off"_i18n;

    Theme theme = Application::getTheme();
    cell->setDetailText(text);
    cell->setDetailTextColor(
        buttons.empty() ? theme["brls/text_disabled"]
                        : theme["brls/list/listItem_value_color"]);
}

void openKeyComboDialog(KeyComboAction action, DetailCell* cell,
                        bool rejectEmpty) {
    auto* dialog = ButtonSelectingDialog::create(
        setup_message(action),
        [action, cell, rejectEmpty](auto buttons) {
            if (rejectEmpty && buttons.empty())
                return;

            auto conflict =
                Settings::instance().key_combo_conflict(action, buttons);
            if (conflict.has_value()) {
                const std::string message = fmt::format(
                    fmt::runtime("settings/key_combo_conflict"_i18n),
                    action_title(conflict.value()));
                sync([message] { showError(message, [] {}); });
                return;
            }

            auto options = Settings::instance().key_combo_options(action);
            options.buttons = std::move(buttons);
            Settings::instance().set_key_combo_options(action, options);
            setupKeyComboCell(cell, options.buttons);
        },
        false, is_host_shortcut(action));
    dialog->open();
}
