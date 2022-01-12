//
//  loading_overlay.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 02.06.2021.
//

#pragma once

#include <borealis.hpp>

class LoadingOverlay : brls::Box {
  public:
    LoadingOverlay(Box* holder);

    void layout();
    void setHidden(bool hide);

  private:
    Box* holder = nullptr;
    BRLS_BIND(brls::ProgressSpinner, progress, "progress");
};
