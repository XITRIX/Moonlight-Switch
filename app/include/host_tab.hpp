//
//  host_tab.hpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#pragma once

#include <Settings.hpp>
#include <borealis.hpp>
#include <cstdint>

enum HostState { FETCHING, AVAILABLE, UNAVAILABLE };

class HostTab : public brls::Box {
  public:
    HostTab(const Host& host);
    void reloadHost();

    BRLS_BIND(brls::DetailCell, connect, "connect");
    BRLS_BIND(brls::DetailCell, remove, "remove");
    BRLS_BIND(brls::Header, header, "header");

  private:
    void wakeUpHost();

    Host host;
    HostState state = HostState::FETCHING;
    uint64_t wakeRequestGeneration = 0;
    bool openAppsWhenReady = false;
};
