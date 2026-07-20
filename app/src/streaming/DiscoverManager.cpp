//
//  DiscoverManager.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.07.2021.
//

#include "DiscoverManager.hpp"
#include "GameStreamClient.hpp"
#include <algorithm>

#if defined(PLATFORM_PSV)
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace brls::literals;

namespace {
#if defined(PLATFORM_PSV)
constexpr uint16_t GAMESTREAM_HTTP_PORT = 47989;
constexpr size_t DISCOVERY_PROBE_BATCH_SIZE = 32;
constexpr suseconds_t DISCOVERY_PROBE_TIMEOUT_US = 750000;

struct PendingProbe {
    int socket = -1;
    std::string address;
};

std::vector<std::string> filter_reachable_gamestream_hosts(
    const std::vector<std::string>& addresses) {
    std::vector<std::string> candidates;

    for (size_t batchStart = 0; batchStart < addresses.size();
         batchStart += DISCOVERY_PROBE_BATCH_SIZE) {
        const size_t batchEnd = std::min(
            batchStart + DISCOVERY_PROBE_BATCH_SIZE, addresses.size());
        std::vector<PendingProbe> pending;
        fd_set writeSet;
        fd_set errorSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        int maxSocket = -1;

        for (size_t i = batchStart; i < batchEnd; i++) {
            sockaddr_in target{};
            target.sin_family = AF_INET;
            target.sin_port = htons(GAMESTREAM_HTTP_PORT);
            if (inet_pton(AF_INET, addresses[i].c_str(), &target.sin_addr) != 1) {
                continue;
            }

            const int probeSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (probeSocket < 0 || probeSocket >= FD_SETSIZE) {
                if (probeSocket >= 0) {
                    close(probeSocket);
                }
                continue;
            }

            int nonBlocking = 1;
            if (setsockopt(probeSocket, SOL_SOCKET, SO_NONBLOCK, &nonBlocking,
                           sizeof(nonBlocking)) != 0) {
                close(probeSocket);
                continue;
            }

            if (connect(probeSocket, reinterpret_cast<sockaddr*>(&target),
                        sizeof(target)) == 0) {
                candidates.push_back(addresses[i]);
                close(probeSocket);
                continue;
            }

            if (errno != EINPROGRESS && errno != EAGAIN &&
                errno != EWOULDBLOCK) {
                close(probeSocket);
                continue;
            }

            FD_SET(probeSocket, &writeSet);
            FD_SET(probeSocket, &errorSet);
            maxSocket = std::max(maxSocket, probeSocket);
            pending.push_back({probeSocket, addresses[i]});
        }

        if (maxSocket >= 0) {
            timeval timeout{};
            timeout.tv_usec = DISCOVERY_PROBE_TIMEOUT_US;
            const int ready =
                select(maxSocket + 1, nullptr, &writeSet, &errorSet, &timeout);
            if (ready > 0) {
                for (const auto& probe : pending) {
                    if (!FD_ISSET(probe.socket, &writeSet) ||
                        FD_ISSET(probe.socket, &errorSet)) {
                        continue;
                    }

                    int socketError = 0;
                    socklen_t errorSize = sizeof(socketError);
                    if (getsockopt(probe.socket, SOL_SOCKET, SO_ERROR,
                                   &socketError, &errorSize) == 0 &&
                        socketError == 0) {
                        candidates.push_back(probe.address);
                    }
                }
            }
        }

        for (const auto& probe : pending) {
            close(probe.socket);
        }
    }

    return candidates;
}
#endif
}

DiscoverManager::DiscoverManager() {
    reset();
    start();
}

void DiscoverManager::reset() {
    pause();
    counter = 0;
    candidatesFiltered = false;
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
#if defined(PLATFORM_PSV)
        if (!candidatesFiltered) {
            brls::Logger::info("Vita host discovery probing {} local addresses",
                               addresses.size());
            addresses = filter_reachable_gamestream_hosts(addresses);
            candidatesFiltered = true;
            brls::Logger::info("Vita host discovery found {} GameStream candidate(s)",
                               addresses.size());
        }
#endif

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
                brls::Logger::info("Host discovery found {} at {}", host.hostname,
                                   host.address);
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
