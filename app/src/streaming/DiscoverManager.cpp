//
//  DiscoverManager.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.07.2021.
//

#include "DiscoverManager.hpp"
#include "GameStreamClient.hpp"

DiscoverManager::DiscoverManager()
{
    this->addresses = GameStreamClient::instance().host_addresses_for_find();
    hosts = hosts.success(std::vector<Host>());
    start();
}

void* DiscoverManager::threadLoop(void*)
{
    DiscoverManager::instance().loop();
    return nullptr;
}

void DiscoverManager::start()
{
    if (addresses.empty()) {
        hosts = hosts.failure("Can't obtain IP address...");
        paused = true;
    }
    else if (paused)
    {
        paused = false;
        pthread_create(&thread, NULL, DiscoverManager::threadLoop, NULL);
    }
    brls::sync([this] { getHostsUpdateEvent()->fire(hosts); });
}

void DiscoverManager::pause()
{
    paused = true;
}

void DiscoverManager::loop()
{
    
    brls::async([this] {
        while (counter < addresses.size() && !paused) {
            SERVER_DATA server_data;

            int status = gs_init(&server_data, addresses[counter], true);
            if (status == GS_OK) {
                Host host;
                host.address = addresses[counter];
                host.hostname = server_data.hostname;
                host.mac = server_data.mac;
                _hosts.push_back(host);
                hosts = hosts.success(_hosts);
                brls::sync([this] { getHostsUpdateEvent()->fire(hosts); });
            }
            
            counter++;
        }
        
        if (counter == addresses.size() && _hosts.empty())
        {
            hosts = hosts.failure("Host PC not found...");
            brls::sync([this] { getHostsUpdateEvent()->fire(hosts); });
        }
        
        paused = true;
    });
}
