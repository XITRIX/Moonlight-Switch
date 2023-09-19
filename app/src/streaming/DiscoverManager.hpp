//
//  DiscoverManager.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.07.2021.
//

#pragma once

#include "GameStreamClient.hpp"
#include "Settings.hpp"
#include "Singleton.hpp"
#include <borealis.hpp>
#include <pthread.h>
#include <stdio.h>

class DiscoverManager : public Singleton<DiscoverManager> {
  public:
    DiscoverManager();
    ~DiscoverManager();

    brls::Event<GSResult<std::vector<Host>>>* getHostsUpdateEvent() {
        return &hostsUpdateEvent;
    }

    GSResult<std::vector<Host>> getHosts() { return hosts; }

    bool isPaused() { return paused; }

    void reset();
    void start();
    void pause();

  private:
    void loop();
    std::vector<std::string> addresses;
    GSResult<std::vector<Host>> hosts;
    std::vector<Host> _hosts;
    brls::Event<GSResult<std::vector<Host>>> hostsUpdateEvent;
    int counter = 0;
    bool paused = true;
};
