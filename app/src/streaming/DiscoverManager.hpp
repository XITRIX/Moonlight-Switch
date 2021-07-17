//
//  DiscoverManager.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.07.2021.
//

#pragma once

#include <stdio.h>
#include <pthread.h>
#include <borealis.hpp>
#include "Singleton.hpp"
#include "Settings.hpp"
#include "GameStreamClient.hpp"

class DiscoverManager : public Singleton<DiscoverManager>
{
public:
    DiscoverManager();
    ~DiscoverManager();
    
    brls::Event<GSResult<std::vector<Host>>>* getHostsUpdateEvent()
    {
        return &hostsUpdateEvent;
    }
    
    GSResult<std::vector<Host>> getHosts()
    {
        return hosts;
    }
    
    bool isPaused()
    {
        return paused;
    }
    
    void start();
    void pause();
    
private:
    static void* threadLoop(void*);
    
    void loop();
    std::vector<std::string> addresses;
    GSResult<std::vector<Host>> hosts;
    std::vector<Host> _hosts;
    brls::Event<GSResult<std::vector<Host>>> hostsUpdateEvent;
    int counter = 0;
    bool paused = true;
    pthread_t thread;
};
