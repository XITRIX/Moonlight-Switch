#include "GameStreamClient.hpp"
#include "Settings.hpp"
#include "WakeOnLanManager.hpp"
#include <borealis.hpp>
#include <thread>
#include <mutex>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <future>
#include <unistd.h>

#if defined(__linux) || defined(__APPLE__)
#include <zeroconf.hpp>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#elif defined(__SWITCH__)
#include <switch.h>
#endif

// namespace
// {
//     void* get_in_addr(sockaddr_storage* sa)
//     {
//         if (sa->ss_family == AF_INET)
//             return &reinterpret_cast<sockaddr_in*>(sa)->sin_addr;

//         if (sa->ss_family == AF_INET6)
//             return &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr;

//         return nullptr;
//     }

//     const std::string SeparatorLine(20, '-');
// }

GameStreamClient::GameStreamClient()
{
    start();
}

void GameStreamClient::start() {
}

void GameStreamClient::stop() {
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
    
    address = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
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
        int a = address & 0xFF;
        int b = (address >> 8) & 0xFF;
        int c = (address >> 16) & 0xFF;
        int d = (address >> 24) & 0xFF;
        
        for (int i = 0; i < 256; i++) {
            if (i == d) {
                continue;
            }
            addresses.push_back(std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." + std::to_string(i));
        }
    }
    return addresses;
}

bool GameStreamClient::can_find_host() {
    return get_my_ip_address() != 0;
}

#ifndef MULTICAST_DISABLED
void LocalPrintLog(Zeroconf::LogLevel level, const std::string& message)
{
    switch (level)
    {
        case Zeroconf::LogLevel::Error:
            brls::Logger::debug("E: {}", message);
            break;
        case Zeroconf::LogLevel::Warning:
            brls::Logger::debug("W: {}", message);
            break;
    }
}
#endif

void GameStreamClient::find_hosts(ServerCallback<std::vector<Host>> callback) {
#ifndef MULTICAST_DISABLED
    brls::async([this, callback] {
        static const std::string MdnsQuery = "_nvstream._tcp.local";
        Zeroconf::SetLogCallback(LocalPrintLog);
        brls::Logger::debug("Search for: {}", MdnsQuery);
        std::vector<Zeroconf::mdns_responce> result;
        bool st = Zeroconf::Resolve(MdnsQuery, /*scanTime*/ 3, &result);
        if (!st)
            brls::sync([callback] { callback(GSResult<std::vector<Host>>::failure("Error has been occured...")); });
        else if (result.empty())
            brls::sync([callback] { callback(GSResult<std::vector<Host>>::failure("Host PC not found...")); });
        else
        {
            std::vector<Host> hosts;
            
            for (size_t i = 0; i < result.size(); i++)
            {
                auto& item = result[i];

                char buffer[INET6_ADDRSTRLEN + 1] = {0};
                inet_ntop(item.peer.ss_family, get_in_addr(&item.peer), buffer, INET6_ADDRSTRLEN);
                brls::Logger::debug("Peer: {}", buffer);
                
                Host host;
                host.address = buffer;
                if (!item.records.empty())
                {
                    for (size_t j = 0; j < item.records.size(); j++)
                    {
                        auto& rr = item.records[j];
                        switch(rr.type)
                        {
                            case  1: host.hostname = rr.name; break;
                            default: break;
                        }
                    }
                }
                hosts.push_back(host);
            }
            
            brls::sync([callback, hosts] { callback(GSResult<std::vector<Host>>::success(hosts)); });
        }
    });
#endif
}
        
       
void GameStreamClient::find_host(ServerCallback<Host> callback) {
    brls::async([this, callback] {
        auto addresses = host_addresses_for_find();

        if (addresses.empty()) {
            brls::sync([callback] { callback(GSResult<Host>::failure("Can't obtain IP address...")); });
        } else {
            bool found = false;

            for (int i = 0; i < addresses.size(); i++) {
                SERVER_DATA server_data;

                int status = gs_init(&server_data, addresses[i], true);
                if (status == GS_OK) {
                    found = true;

                    Host host;
                    host.address = addresses[i];
                    host.hostname = server_data.hostname;
                    host.mac = server_data.mac;
                    brls::sync([callback, host] { callback(GSResult<Host>::success(host)); });
                    break;
                }
            }

            if (!found) {
                brls::sync([callback] { callback(GSResult<Host>::failure("Host PC not found...")); });
            }
        }
    });
}

bool GameStreamClient::can_wake_up_host(const Host &host) {
    return WakeOnLanManager::instance().can_wake_up_host(host);
}

void GameStreamClient::wake_up_host(const Host &host, ServerCallback<bool> callback) {
    brls::async([this, host, callback] {
        auto result = WakeOnLanManager::instance().wake_up_host(host);
        
        if (result.isSuccess()) {
            usleep(5'000'000);
            brls::sync([callback, result] { callback(result); });
        } else {
            brls::sync([callback, result] { callback(result); });
        }
    });
}

void GameStreamClient::connect(const std::string &address, ServerCallback<SERVER_DATA> callback) {
    m_server_data[address] = SERVER_DATA();
    
    brls::async([this, address, callback] {
        int status = gs_init(&m_server_data[address], address);
        
        brls::sync([this, address, callback, status] {
            if (status == GS_OK) {
                Host host;
                host.address = address;
                host.hostname = m_server_data[address].hostname;
                host.mac = m_server_data[address].mac;
                Settings::instance().add_host(host);
                callback(GSResult<SERVER_DATA>::success(m_server_data[address]));
            } else {
                callback(GSResult<SERVER_DATA>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::pair(const std::string &address, const std::string &pin, ServerCallback<bool> callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<bool>::failure("Firstly call connect()..."));
        return;
    }
    
    brls::async([this, address, pin, callback] {
        int status = gs_pair(&m_server_data[address], (char *)pin.c_str());
        
        brls::sync([callback, status] {
            if (status == GS_OK) {
                callback(GSResult<bool>::success(true));
            } else {
                callback(GSResult<bool>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::applist(const std::string &address, ServerCallback<AppInfoList> callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<AppInfoList>::failure("Firstly call connect() & pair()..."));
        return;
    }
    
    brls::async([this, address, callback] {
        PAPP_LIST list;
        
        int status = gs_applist(&m_server_data[address], &list);
        
        AppInfoList app_list;
        
        while (list) {
            app_list.push_back({ .name = list->name, .app_id = list->id });
            list = list->next;
        }
        
        std::sort(app_list.begin(), app_list.end(), [](AppInfo a, AppInfo b) { return a.name < b.name; });
        
        brls::sync([this, app_list, callback, status] {
            if (status == GS_OK) {
                callback(GSResult<AppInfoList>::success(app_list));
            } else {
                callback(GSResult<AppInfoList>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::app_boxart(const std::string &address, int app_id, ServerCallback<Data> callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<Data>::failure("Firstly call connect() & pair()..."));
        return;
    }
    
    brls::async([this, address, app_id, callback] {
        Data data;
        int status = gs_app_boxart(&m_server_data[address], app_id, &data);
        
        brls::sync([this, callback, data, status] {
            if (status == GS_OK) {
                callback(GSResult<Data>::success(data));
            } else {
                callback(GSResult<Data>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::start(const std::string &address, STREAM_CONFIGURATION config, int app_id, ServerCallback<STREAM_CONFIGURATION> callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<STREAM_CONFIGURATION>::failure("Firstly call connect() & pair()..."));
        return;
    }
    
    m_config = config;
    
    brls::async([this, address, app_id, callback] {
        int status = gs_start_app(&m_server_data[address], &m_config, app_id, Settings::instance().sops(), Settings::instance().play_audio(), 0x1);
        
        brls::sync([this, callback, status] {
            if (status == GS_OK) {
                callback(GSResult<STREAM_CONFIGURATION>::success(m_config));
            } else {
                callback(GSResult<STREAM_CONFIGURATION>::failure(gs_error()));
            }
        });
    });
}

void GameStreamClient::quit(const std::string &address, ServerCallback<bool> callback) {
    if (m_server_data.count(address) == 0) {
        callback(GSResult<bool>::failure("Firstly call connect() & pair()..."));
        return;
    }
    
    auto server_data = m_server_data[address];
    
    brls::async([this, server_data, callback] {
        int status = gs_quit_app((PSERVER_DATA)&server_data);
        
        brls::sync([this, callback, status] {
            if (status == GS_OK) {
                callback(GSResult<bool>::success(true));
            } else {
                callback(GSResult<bool>::failure(gs_error()));
            }
        });
    });
}
