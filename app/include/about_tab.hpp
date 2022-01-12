//
//  settings_tab.hpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#pragma once

#include "link_cell.hpp"
#include <borealis.hpp>

class AboutTab : public brls::Box {
  public:
    AboutTab();
    static brls::View* create();

  private:
    BRLS_BIND(brls::Header, versionLabel, "version_label");
    BRLS_BIND(LinkCell, github, "github");
    BRLS_BIND(LinkCell, patreon, "patreon");
    BRLS_BIND(LinkCell, gbatemp, "gbatemp");
};
