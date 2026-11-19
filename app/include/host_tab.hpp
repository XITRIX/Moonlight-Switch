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

    void willAppear(bool resetState) override;
    void willDisappear(bool resetState) override;

    BRLS_BIND(brls::DetailCell, connect, "connect");
    BRLS_BIND(brls::DetailCell, remove, "remove");
    BRLS_BIND(brls::Header, header, "header");

  private:
    void pollHost();
    void showUnavailable();

    Host host;
    HostState state = HostState::FETCHING;
    uint64_t wakeRequestGeneration = 0;
    uint64_t canceledWakeRequestGeneration = 0;

    brls::RepeatingTimer pollTimer;
    bool polling = false;
    uint64_t reloadGeneration = 0;
};
