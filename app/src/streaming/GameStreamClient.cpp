#include "GameStreamClient.hpp"
#include "Settings.hpp"
#include "WakeOnLanManager.hpp"
#include <borealis.hpp>
#include <algorithm>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <vector>

#include <curl/curl.h>
#include <libretro-common/retro_timers.h>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fmt/core.h>

#ifndef PLATFORM_PSV
#include <net/if.h>
#include <sys/ioctl.h>
#endif

#if defined(__SWITCH__)
#include <switch.h>

// TODO: Remove when presented in LibNX
struct ipv6_mreq {
	struct in6_addr ipv6mr_multiaddr;
	unsigned int    ipv6mr_interface;
};
#endif

#ifndef MULTICAST_DISABLED
extern "C" {
#include <mdns.h>
}
#endif

using namespace brls;

namespace {
void rebind_server_info(SERVER_DATA& server) {
    server.serverInfo.address = server.address.c_str();
    server.serverInfo.serverInfoAppVersion =
        server.serverInfoAppVersion.c_str();
    server.serverInfo.serverInfoGfeVersion =
        server.serverInfoGfeVersion.c_str();
}

std::string host_key(const Host& host) {
    if (!host.mac.empty()) {
        return "mac:" + host.mac;
    }
    if (!host.address.empty()) {
        return "address:" + host.address;
    }
    if (!host.remoteAddress.empty()) {
        return "remote:" + host.remoteAddress;
    }
    return "hostname:" + host.hostname;
}

void merge_discovered_host(std::vector<Host>& hosts, const Host& host) {
    auto it = std::find_if(hosts.begin(), hosts.end(), [host](const Host& existing) {
        return hosts_match(existing, host);
    });

    if (it == hosts.end()) {
        hosts.push_back(host);
        return;
    }

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

bool is_ipv4_address(const std::string& address) {
    if (address.empty()) {
        return false;
    }

    in_addr parsed{};
    return inet_pton(AF_INET, address.c_str(), &parsed) == 1;
}

std::string strip_ipv4_port(const std::string& address) {
    const auto firstColon = address.find(':');
    if (firstColon == std::string::npos) {
        return address;
    }
    if (address.find(':', firstColon + 1) != std::string::npos) {
        return address;
    }
    return address.substr(0, firstColon);
}

std::string ipv4_port_suffix(const std::string& address) {
    const auto firstColon = address.find(':');
    if (firstColon == std::string::npos) {
        return "";
    }
    if (address.find(':', firstColon + 1) != std::string::npos) {
        return "";
    }
    return address.substr(firstColon);
}

bool connect_to_addresses_sync(const std::vector<std::string>& addresses,
                               std::string& connectedAddress,
                               SERVER_DATA& connectedServer,
                               std::string& error) {
    if (std::none_of(addresses.begin(), addresses.end(),
                     [](const std::string& address) {
                         return !address.empty();
                     })) {
        error = "Address is Empty";
        return false;
    }

    error = "Address is Empty";
    connectedAddress.clear();
    connectedServer = SERVER_DATA{};

    for (const auto& address : addresses) {
        if (address.empty()) {
            continue;
        }

        SERVER_DATA serverData{};
        const int status = gs_init(&serverData, address);
        if (status == GS_OK) {
            connectedAddress = address;
            connectedServer = serverData;
            return true;
        }

        error = gs_error();
    }

    return false;
}

constexpr int WAKE_POLL_ATTEMPTS = 20;
constexpr useconds_t WAKE_POLL_INTERVAL_US = 1'000'000;
}

GameStreamClient::GameStreamClient() { start(); }

void GameStreamClient::start() {}

void GameStreamClient::stop() {
#ifndef MULTICAST_DISABLED
    cancel_find_hosts();
#endif
}

static uint32_t get_my_ip_address() {
    uint32_t address = 0;
#if defined(__linux) || defined(__APPLE__)
    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "en0", IFNAMSIZ - 1);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    address = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
#elif defined(__SWITCH__)
    nifmGetCurrentIpAddress(&address);
#endif
    return address;
}

std::vector<std::string> GameStreamClient::host_addresses_for_find() {
    std::vector<std::string> addresses;

    uint32_t address = get_my_ip_address();
    bool isSucceed = address != 0;

    if (isSucceed) {
        uint32_t a = address & 0xFF;
        uint32_t b = (address >> 8) & 0xFF;
        uint32_t c = (address >> 16) & 0xFF;
        uint32_t d = (address >> 24) & 0xFF;

        for (int i = 0; i < 256; i++) {
            if (i == d) {
                continue;
            }
            addresses.push_back(std::to_string(a) + "." + std::to_string(b) +
                                "." + std::to_string(c) + "." +
                                std::to_string(i));
        }
    }
    return addresses;
}

std::string GameStreamClient::external_address_for_mdns(const std::string& address) {
    const auto localAddress = strip_ipv4_port(address);
    if (!localAddress.empty() && !is_ipv4_address(localAddress)) {
        return "";
    }

    unsigned int wanAddress = 0;
    const int err =
        LiFindExternalAddressIP4("stun.moonlight-stream.org", 3478, &wanAddress);
    if (err != 0) {
        Logger::error("Failed to get remote IPv4 address over STUN: {}", err);
        return "";
    }

    in_addr externalAddr{};
    externalAddr.s_addr = wanAddress;

    char addressBuffer[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &externalAddr, addressBuffer, sizeof(addressBuffer)) == nullptr) {
        Logger::error("Failed to format remote IPv4 address returned by STUN");
        return "";
    }

    std::string externalAddress = addressBuffer;
    const auto portSuffix = ipv4_port_suffix(address);
    if (!portSuffix.empty()) {
        externalAddress += portSuffix;
    }

    return externalAddress;
}

bool GameStreamClient::can_find_host() { return get_my_ip_address() != 0; }

#ifndef MULTICAST_DISABLED
static std::atomic_uint64_t findHostsGeneration = 0;

struct MdnsSearchContext {
    std::string foundHost;
};

std::string get_ip_str(const struct sockaddr *addr)
{
    char addrStr[INET6_ADDRSTRLEN];
    if (addr->sa_family == AF_INET) {
        inet_ntop(addr->sa_family, &((struct sockaddr_in*)addr)->sin_addr, addrStr, sizeof(addrStr));
        unsigned short port = htons(((struct sockaddr_in*)addr)->sin_port);
        return fmt::format("{}", addrStr);
    }
    else {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)addr;
        inet_ntop(addr->sa_family, &sin6->sin6_addr, addrStr, sizeof(addrStr));
        unsigned short port = ntohs(((struct sockaddr_in6*)addr)->sin6_port);
        if (sin6->sin6_scope_id != 0) {
            // Link-local addresses with scope IDs are special
            return fmt::format("{}{}", addrStr, sin6->sin6_scope_id);
        }
        else {
            return fmt::format("{}", addrStr);
        }
    }
}

static int mdns_discovery_callback(int sock, const struct sockaddr* from, size_t addrlen,
                                   mdns_entry_type_t entry, uint16_t query_id, uint16_t type,
                                   uint16_t rclass, uint32_t ttl, const void* data, size_t size,
                                   size_t offset, size_t length, size_t record_offset,
                                   size_t record_length, void* user_data)
{
    auto* context = static_cast<MdnsSearchContext*>(user_data);
    if (type == MDNS_RECORDTYPE_A) {
        context->foundHost = get_ip_str(from);
    }
    return 0;
}

void GameStreamClient::find_hosts(ServerCallback<std::vector<Host>>& callback) {
    const uint64_t generation = ++findHostsGeneration;
    brls::async([callback, generation] {
        auto isCancelled = [generation]() {
            return generation != findHostsGeneration.load();
        };

        std::vector<Host> foundHosts;
        MdnsSearchContext searchContext;
        size_t capacity = 2048;
        std::vector<uint8_t> buffer(capacity);
        size_t records;

        int sock = mdns_socket_open_ipv4(nullptr);
        if (sock < 0) {
            return;
        }

        auto closeSocket = [&sock]() {
            if (sock >= 0) {
                mdns_socket_close(sock);
                sock = -1;
            }
        };

        if (isCancelled()) {
            closeSocket();
            return;
        }

        if (mdns_query_send(sock, MDNS_RECORDTYPE_PTR,
                        MDNS_STRING_CONST("_nvstream._tcp.local"),
                        buffer.data(), capacity, 0)) {
            closeSocket();
            brls::sync([callback] {
                callback(GSResult<std::vector<Host>>::failure(
                        "error/unknown_error"_i18n));
            });
            return;
        }

        int empty_cnt = 0;
        while (!isCancelled()) {
            records = mdns_query_recv(sock, buffer.data(), capacity, mdns_discovery_callback, &searchContext, 0);
            if (records == 0) {
                empty_cnt++;
            } else {
                empty_cnt = 0;

                SERVER_DATA server_data;

                int status = gs_init(&server_data, searchContext.foundHost);
                if (status == GS_OK) {
                    Host host;
                    host.address = searchContext.foundHost;
                    host.remoteAddress =
                        GameStreamClient::external_address_for_mdns(host.address);
                    host.hostname = server_data.hostname;
                    host.mac = server_data.mac;
                    merge_discovered_host(foundHosts, host);

                    brls::sync([callback, foundHosts] {
                        callback(GSResult<std::vector<Host>>::success(foundHosts));
                    });
                }
            }
            // wait 30sec after last receive
            if (empty_cnt >= 10) {
                break;
            }

            retro_sleep(500);
        }

        closeSocket();
    });
}

void GameStreamClient::cancel_find_hosts() {
    ++findHostsGeneration;
}
#endif

bool GameStreamClient::can_wake_up_host(const Host& host) {
    return WakeOnLanManager::can_wake_up_host(host);
}

void GameStreamClient::wake_up_host(const Host& host,
                                    ServerCallback<bool>& callback) {
    brls::async([host, callback] {
        auto result = WakeOnLanManager::wake_up_host(host);
        if (!result.isSuccess()) {
            brls::sync([callback, result] { callback(result); });
            return;
        }

        std::string connectedAddress;
        std::string error;
        SERVER_DATA connectedServer{};

        for (int attempt = 0; attempt < WAKE_POLL_ATTEMPTS; attempt++) {
            if (connect_to_addresses_sync(host.connection_addresses(),
                                          connectedAddress, connectedServer,
                                          error)) {
                auto& client = GameStreamClient::instance();
                client.cache_server_data(connectedAddress, connectedServer);
                client.m_active_addresses[host_key(host)] = connectedAddress;

                brls::sync([callback] {
                    callback(GSResult<bool>::success(true));
                });
                return;
            }

            if (attempt + 1 < WAKE_POLL_ATTEMPTS) {
                usleep(WAKE_POLL_INTERVAL_US);
            }
        }

        brls::sync([callback, error] {
            callback(GSResult<bool>::failure(
                error.empty() ? "Host did not come online after wake signal"
                              : error));
        });
    });
}

void GameStreamClient::cache_server_data(const std::string& address,
                                         const SERVER_DATA& data) {
    m_server_data[address] = data;
    rebind_server_info(m_server_data[address]);
    if (!data.mac.empty()) {
        m_active_addresses["mac:" + data.mac] = address;
    }
}

void GameStreamClient::connect_to_addresses(
    const std::vector<std::string>& addresses, const std::string& activeKey,
    ServerCallback<SERVER_DATA>& callback) {
    if (std::none_of(addresses.begin(), addresses.end(),
                     [](const std::string& address) {
                         return !address.empty();
                     })) {
        callback(GSResult<SERVER_DATA>::failure("Address is Empty"));
        return;
    }

    brls::async([this, addresses, activeKey, callback] {
        std::string connectedAddress;
        std::string error;
        SERVER_DATA connectedServer{};
        connect_to_addresses_sync(addresses, connectedAddress, connectedServer,
                                  error);

        brls::sync([this, callback, activeKey, connectedAddress,
                    connectedServer, error] {
            if (connectedAddress.empty()) {
                callback(GSResult<SERVER_DATA>::failure(error));
                return;
            }

            cache_server_data(connectedAddress, connectedServer);
            if (!activeKey.empty()) {
                m_active_addresses[activeKey] = connectedAddress;
            }

            callback(
                GSResult<SERVER_DATA>::success(m_server_data[connectedAddress]));
        });
    });
}

void GameStreamClient::connect(const std::string& address,
                               ServerCallback<SERVER_DATA>& callback) {
    connect_to_addresses({address}, "", callback);
}

std::string GameStreamClient::active_address(const Host& host) const {
    if (const auto it = m_active_addresses.find(host_key(host));
        it != m_active_addresses.end() && !it->second.empty()) {
        return it->second;
    }

    return host.preferred_address();
}

void GameStreamClient::connect(const Host& host,
                               ServerCallback<SERVER_DATA>& callback) {
    connect_to_addresses(host.connection_addresses(), host_key(host), callback);
}

void GameStreamClient::pair(const std::string& address, const std::string& pin,
                            ServerCallback<bool>& callback) {
    with_cached_server_data<bool>(
        address, "Firstly call connect()...", callback,
        [this, pin](const std::string& cachedAddress,
                    ServerCallback<bool>& callback) {
            int status = gs_pair(&m_server_data[cachedAddress], (char*)pin.c_str());

            brls::sync([callback, status] {
                if (status == GS_OK) {
                    callback(GSResult<bool>::success(true));
                } else {
                    callback(GSResult<bool>::failure(gs_error()));
                }
            });
        });
}

void GameStreamClient::pair(const Host& host, const std::string& pin,
                            ServerCallback<bool>& callback) {
    pair(active_address(host), pin, callback);
}

void GameStreamClient::applist(const std::string& address,
                               ServerCallback<AppInfoList>& callback) {
    with_cached_server_data<AppInfoList>(
        address, "Firstly call connect() & pair()...", callback,
        [this](const std::string& cachedAddress,
               ServerCallback<AppInfoList>& callback) {
            PAPP_LIST list;

            int status = gs_applist(&m_server_data[cachedAddress], &list);
            if (status != CURLE_OK) {
                callback(GSResult<AppInfoList>::failure(gs_error()));
                return;
            }

            AppInfoList app_list;

            while (list) {
                std::string name = std::string(list->name);
                int id = list->id;
                AppInfo info;
                info.name = name;
                info.app_id = id;
                app_list.push_back(info);
                list = list->next;
            }

            std::sort(app_list.begin(), app_list.end(),
                      [](const AppInfo& a, const AppInfo& b) {
                          return a.name < b.name;
                      });

            brls::sync([app_list, callback, status] {
                if (status == GS_OK) {
                    callback(GSResult<AppInfoList>::success(app_list));
                } else {
                    callback(GSResult<AppInfoList>::failure(gs_error()));
                }
            });
        });
}

void GameStreamClient::applist(const Host& host,
                               ServerCallback<AppInfoList>& callback) {
    applist(active_address(host), callback);
}

void GameStreamClient::app_boxart(const std::string& address, int app_id,
                                  ServerCallback<Data>& callback) {
    with_cached_server_data<Data>(
        address, "Firstly call connect() & pair()...", callback,
        [this, app_id](const std::string& cachedAddress,
                       ServerCallback<Data>& callback) {
            Data data;
            int status =
                gs_app_boxart(&m_server_data[cachedAddress], app_id, &data);

            brls::sync([callback, data, status] {
                if (status == GS_OK) {
                    callback(GSResult<Data>::success(data));
                } else {
                    callback(GSResult<Data>::failure(gs_error()));
                }
            });
        });
}

void GameStreamClient::app_boxart(const Host& host, int app_id,
                                  ServerCallback<Data>& callback) {
    app_boxart(active_address(host), app_id, callback);
}

void GameStreamClient::start(const std::string& address,
                             STREAM_CONFIGURATION config, int app_id,
                             ServerCallback<STREAM_CONFIGURATION>& callback) {
    m_config = config;

    with_cached_server_data<STREAM_CONFIGURATION>(
        address, "Firstly call connect() & pair()...", callback,
        [this, app_id](const std::string& cachedAddress,
                       ServerCallback<STREAM_CONFIGURATION>& callback) {
            int status = gs_start_app(&m_server_data[cachedAddress], &m_config,
                                      app_id, Settings::instance().sops(),
                                      Settings::instance().play_audio(), 0x1);

            brls::sync([this, callback, status] {
                if (status == GS_OK) {
                    callback(GSResult<STREAM_CONFIGURATION>::success(m_config));
                } else {
                    callback(GSResult<STREAM_CONFIGURATION>::failure(gs_error()));
                }
            });
        });
}

void GameStreamClient::start(const Host& host, STREAM_CONFIGURATION config,
                             int app_id,
                             ServerCallback<STREAM_CONFIGURATION>& callback) {
    start(active_address(host), config, app_id, callback);
}

void GameStreamClient::quit(const std::string& address,
                            ServerCallback<bool>& callback) {
    with_cached_server_data<bool>(
        address, "Firstly call connect() & pair()...", callback,
        [this](const std::string& cachedAddress, ServerCallback<bool>& callback) {
            auto server_data = m_server_data[cachedAddress];

            int status = gs_quit_app((PSERVER_DATA)&server_data);

            brls::sync([callback, status] {
                if (status == GS_OK) {
                    callback(GSResult<bool>::success(true));
                } else {
                    callback(GSResult<bool>::failure(gs_error()));
                }
            });
        });
}

void GameStreamClient::quit(const Host& host,
                            ServerCallback<bool>& callback) {
    quit(active_address(host), callback);
}
