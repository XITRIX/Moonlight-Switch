#include "Data.hpp"
#include "Singleton.hpp"
#include "Settings.hpp"
#include "client.h"
#include "errors.h"
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#pragma once

template <typename T> struct GSResult {
  public:
    static GSResult success(T value) { return result(value, "", true); }

    static GSResult failure(std::string error) {
        return result(T(), error, false);
    }

    [[nodiscard]] bool isSuccess() const { return _isSuccess; }

    T value() const { return _value; }

    [[nodiscard]] std::string error() const { return _error; }

  private:
    static GSResult result(T value, const std::string& error, bool isSuccess) {
        GSResult result;
        result._value = value;
        result._error = error;
        result._isSuccess = isSuccess;
        return result;
    }

    T _value;
    std::string _error;
    bool _isSuccess = false;
};

template <class T>
using ServerCallback = const std::function<void(GSResult<T>)>;

//struct Host;

struct AppInfo {
    std::string name;
    int app_id;
};

using AppInfoList = std::vector<AppInfo>;

class GameStreamClient : public Singleton<GameStreamClient> {
  public:
    SERVER_DATA server_data(const std::string& address) {
        return m_server_data[address];
    }
    SERVER_DATA server_data(const Host& host) {
        return server_data(active_address(host));
    }

    [[nodiscard]] std::string active_address(const Host& host) const;

    GameStreamClient();

    void start();
    void stop();

    static std::vector<std::string> host_addresses_for_find();
    static std::string external_address_for_mdns(const std::string& address = "");

    static bool can_find_host();
    static void find_hosts(ServerCallback<std::vector<Host>>& callback);
    static void cancel_find_hosts();

    static bool can_wake_up_host(const Host& host);
    static void wake_up_host(const Host& host, ServerCallback<bool>& callback);

    void connect(const std::string& address,
                 ServerCallback<SERVER_DATA>& callback);
    void connect(const Host& host, ServerCallback<SERVER_DATA>& callback);
    void pair(const std::string& address, const std::string& pin,
              ServerCallback<bool>& callback);
    void pair(const Host& host, const std::string& pin,
              ServerCallback<bool>& callback);
    void applist(const std::string& address,
                 ServerCallback<AppInfoList>& callback);
    void applist(const Host& host, ServerCallback<AppInfoList>& callback);
    void app_boxart(const std::string& address, int app_id,
                    ServerCallback<Data>& callback);
    void app_boxart(const Host& host, int app_id,
                    ServerCallback<Data>& callback);
    void start(const std::string& address, STREAM_CONFIGURATION config,
               int app_id, ServerCallback<STREAM_CONFIGURATION>& callback);
    void start(const Host& host, STREAM_CONFIGURATION config,
               int app_id, ServerCallback<STREAM_CONFIGURATION>& callback);
    void quit(const std::string& address, ServerCallback<bool>& callback);
    void quit(const Host& host, ServerCallback<bool>& callback);

  private:
    template <typename T, typename Worker>
    void with_cached_server_data(const std::string& address,
                                 const std::string& missingError,
                                 ServerCallback<T>& callback,
                                 Worker&& worker) {
        if (m_server_data.count(address) == 0) {
            callback(GSResult<T>::failure(missingError));
            return;
        }

        brls::async([this, address, callback,
                     worker = std::forward<Worker>(worker)]() mutable {
            worker(address, callback);
        });
    }

    void cache_server_data(const std::string& address, const SERVER_DATA& data);
    void connect_to_addresses(const std::vector<std::string>& addresses,
                              const std::string& activeKey,
                              ServerCallback<SERVER_DATA>& callback);

    std::map<std::string, SERVER_DATA> m_server_data;
    std::map<std::string, std::string> m_active_addresses;
    STREAM_CONFIGURATION m_config;
};
