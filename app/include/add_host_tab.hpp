//
//  add_host_tab.hpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#pragma once

#include <borealis.hpp>
#include "Settings.hpp"

class AddHostTab : public brls::Box
{
  public:
    AddHostTab();

    static brls::View* create();

  private:
    void findHost();
    void connectHost(std::string address);
    
    BRLS_BIND(brls::InputCell, hostIP, "hostIP");
    BRLS_BIND(brls::DetailCell, connect, "connect");
    BRLS_BIND(brls::Box, searchBox, "search_box");
    BRLS_BIND(brls::Box, loader, "loader");
    BRLS_BIND(brls::Header, searchHeader, "search_header");
};
