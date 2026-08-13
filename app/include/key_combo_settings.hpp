#pragma once

#include "Settings.hpp"
#include <borealis.hpp>

void setupKeyComboCell(brls::DetailCell* cell,
                       const std::vector<brls::ControllerButton>& buttons);
void openKeyComboDialog(KeyComboAction action, brls::DetailCell* cell,
                        bool rejectEmpty = false);
