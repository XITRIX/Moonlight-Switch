#include "GameStreamClient.hpp"
#include "Settings.hpp"
#include "WakeOnLanManager.hpp"
#include <borealis.hpp>
#include <thread>
#include <unistd.h>
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

GameStreamClient::GameStreamClient() { start(); }

void GameStreamClient::start() {}

void GameStreamClient::stop() {}

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

bool GameStreamClient::can_find_host() { return get_my_ip_address() != 0; }

#ifndef MULTICAST_DISABLED
 static std::vector<Host> foundHosts;
 static std::string foundHost;

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
    if (type == MDNS_RECORDTYPE_A) {
        foundHost = get_ip_str(from);
    }
    return 0;
}

void GameStreamClient::find_hosts(ServerCallback<std::vector<Host>>& callback) {
    brls::async([callback] {
        foundHosts.clear();

        size_t capacity = 2048;
        void* buffer = malloc(capacity);
        size_t records;


        int sock = mdns_socket_open_ipv4(nullptr);
        if (sock < 0) {
            return;
        }

        if (mdns_query_send(sock, MDNS_RECORDTYPE_PTR,
                        MDNS_STRING_CONST("_nvstream._tcp.local"),
                        buffer, capacity, 0)) {
            brls::sync([callback] {
                callback(GSResult<std::vector<Host>>::failure(
                        "error/unknown_error"_i18n));
            });
            return;
        }

        int empty_cnt = 0;
        while (true) {
            records = mdns_query_recv(sock, buffer, capacity, mdns_discovery_callback, (void*)(&callback), 0);
            if (records == 0) {
                empty_cnt++;
            } else {
                empty_cnt = 0;

                SERVER_DATA server_data;

                int status = gs_init(&server_data, foundHost);
                if (status == GS_OK) {
                    Host host;
                    host.address = foundHost;
                    host.hostname = server_data.hostname;
                    host.mac = server_data.mac;
                    foundHosts.push_back(host);

                    brls::sync([callback] {
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
    });
}
#endif

bool GameStreamClient::can_wake_up_host(const Host& host) {
    return WakeOnLanManager::can_wake_up_host(host);
}

void GameStreamClient::wake_up_host(const Host& host,
                                    ServerCallback<bool>& callback) {
    brls::async([host, callback] {
        auto result = WakeOnLanManager::wake_up_host(host);

        if (result.isSuccess()) {
            usleep(5'000'000);
            brls::sync([callback, result] { callback(result); });
        } else {
            brls::sync([callback, result] { callback(result); });
        }
    });
}

void GameStreamClient::connect(const std::string& address,
                               ServerCallback<SERVER_DATA>& callback) {
    if (address.empty()) {
        callback(GSResult<SERVER_DATA>::failure("Address is Empty"));
        return;
    }

    SERVER_DATA* data = new SERVER_DATA();
    brls::async([this, address, callback, data] {
        int status = gs_init(data, address);

        brls::sync([this, address, callback, status, data] {
            if (status == GS_OK) {
                m_server_data[address] = *data;
                callback(GSResult<SERVER_DATA>::success(m_server_data[address]));
            } else {
                callback(GSResult<SERVER_DATA>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::pair(const std::string& address, const std::string& pin,
                            ServerCallback<bool>& callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<bool>::failure("Firstly call connect()..."));
        return;
    }

    brls::async([this, address, pin, callback] {
        int status = gs_pair(&m_server_data[address], (char*)pin.c_str());

        brls::sync([callback, status] {
            if (status == GS_OK) {
                callback(GSResult<bool>::success(true));
            } else {
                callback(GSResult<bool>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::applist(const std::string& address,
                               ServerCallback<AppInfoList>& callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<AppInfoList>::failure(
            "Firstly call connect() & pair()..."));
        return;
    }

    brls::async([this, address, callback] {
        PAPP_LIST list;

        int status = gs_applist(&m_server_data[address], &list);
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
                  [](const AppInfo& a, const AppInfo& b) { return a.name < b.name; });

        brls::sync([app_list, callback, status] {
            if (status == GS_OK) {
                callback(GSResult<AppInfoList>::success(app_list));
            } else {
                callback(GSResult<AppInfoList>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::app_boxart(const std::string& address, int app_id,
                                  ServerCallback<Data>& callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<Data>::failure("Firstly call connect() & pair()..."));
        return;
    }

    brls::async([this, address, app_id, callback] {
        Data data;
        int status = gs_app_boxart(&m_server_data[address], app_id, &data);

        brls::sync([callback, data, status] {
            if (status == GS_OK) {
                callback(GSResult<Data>::success(data));
            } else {
                callback(GSResult<Data>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::start(const std::string& address,
                             STREAM_CONFIGURATION config, int app_id,
                             ServerCallback<STREAM_CONFIGURATION>& callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<STREAM_CONFIGURATION>::failure(
            "Firstly call connect() & pair()..."));
        return;
    }

    m_config = config;

    brls::async([this, address, app_id, callback] {
        int status = gs_start_app(&m_server_data[address], &m_config, app_id,
                                  Settings::instance().sops(),
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

void GameStreamClient::quit(const std::string& address,
                            ServerCallback<bool>& callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<bool>::failure("Firstly call connect() & pair()..."));
        return;
    }

    auto server_data = m_server_data[address];

    brls::async([server_data, callback] {
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
