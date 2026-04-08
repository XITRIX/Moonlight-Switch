//
//  DiscoverManager.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.07.2021.
//

#include "DiscoverManager.hpp"
#include "GameStreamClient.hpp"
#include <algorithm>

using namespace brls::literals;

DiscoverManager::DiscoverManager() {
    reset();
    start();
}

void DiscoverManager::reset() {
    pause();
    counter = 0;
    addresses.clear();
    _hosts.clear();
    hosts = hosts.success(std::vector<Host>());
}

void DiscoverManager::start() {
    if (addresses.empty()) {
        this->addresses =
            GameStreamClient::host_addresses_for_find();
        if (addresses.empty()) {
            hosts = hosts.failure("discovery_manager/no_ip"_i18n);
            return;
        }

        paused = true;
    }

    if (paused) {
        paused = false;
        brls::async([] { DiscoverManager::instance().loop(); });
    }
    brls::sync([this] { getHostsUpdateEvent()->fire(hosts); });
}

void DiscoverManager::pause() { paused = true; }

void DiscoverManager::loop() {
    brls::async([this] {
        while (counter < addresses.size() && !paused) {
            SERVER_DATA server_data;

            int status = gs_init(&server_data, addresses[counter]);
            if (status == GS_OK) {
                Host host;
                host.address = addresses[counter];
                host.remoteAddress =
                    GameStreamClient::external_address_for_mdns(host.address);
                host.hostname = server_data.hostname;
                host.mac = server_data.mac;
                auto it = std::find_if(_hosts.begin(), _hosts.end(), [host](const Host& existing) {
                    return hosts_match(existing, host);
                });
                if (it == _hosts.end()) {
                    _hosts.push_back(host);
                } else {
                    if (!host.address.empty()) {
                        it->address = host.address;
                    }
                    if (!host.remoteAddress.empty()) {
                        it->remoteAddress = host.remoteAddress;
                    }
                    if (!host.hostname.empty()) {
                        it->hostname = host.hostname;
                    }
                    if (!host.mac.empty()) {
                        it->mac = host.mac;
                    }
                }
                hosts = hosts.success(_hosts);
                brls::sync([this] { getHostsUpdateEvent()->fire(hosts); });
            }

            counter++;
        }

        if (counter == addresses.size() && _hosts.empty()) {
            hosts = hosts.failure("discovery_manager/no_host"_i18n);
            brls::sync([this] { getHostsUpdateEvent()->fire(hosts); });
        }

        paused = true;
    });
}

DiscoverManager::~DiscoverManager() { paused = true; }
