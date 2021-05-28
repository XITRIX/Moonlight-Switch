#include "Singleton.hpp"
#include <string>
#include <vector>
#include <functional>
#include <map>
#include "client.h"
#include "errors.h"
#include "Data.hpp"

#pragma once

template <typename T>
struct GSResult {
public:
    static GSResult success(T value) {
        return result(value, "", true);
    }
    
    static GSResult failure(std::string error) {
        return result(T(), error, false);
    }
    
    bool isSuccess() const {
        return _isSuccess;
    }
    
    T value() const {
        return _value;
    }
    
    std::string error() const {
        return _error;
    }
    
private:
    static GSResult result(T value, std::string error, bool isSuccess) {
        GSResult result;
        result._value = value;
        result._error = error;
        result._isSuccess = isSuccess;
        return result;
    }
    
    T _value;
    std::string _error = "";
    bool _isSuccess = false;
};

template<class T> using ServerCallback = const std::function<void(GSResult<T>)>;

struct Host;

struct AppInfo {
    std::string name;
    int app_id;
};

using AppInfoList = std::vector<AppInfo>;

class GameStreamClient: public Singleton<GameStreamClient> {
public:
    SERVER_DATA server_data(const std::string &address) {
        return m_server_data[address];
    }
    
    GameStreamClient();
    
    void start();
    void stop();
    
    std::vector<std::string> host_addresses_for_find();
    
    bool can_find_host();
    void find_host(ServerCallback<Host> callback);
    
    bool can_wake_up_host(const Host &host);
    void wake_up_host(const Host &host, ServerCallback<bool> callback);
    
    void connect(const std::string &address, ServerCallback<SERVER_DATA> callback);
    void pair(const std::string &address, const std::string &pin, ServerCallback<bool> callback);
    void applist(const std::string &address, ServerCallback<AppInfoList> callback);
    void app_boxart(const std::string &address, int app_id, ServerCallback<Data> callback);
    void start(const std::string &address, STREAM_CONFIGURATION config, int app_id, ServerCallback<STREAM_CONFIGURATION> callback);
    void quit(const std::string &address, ServerCallback<bool> callback);
    
private:
    std::map<std::string, SERVER_DATA> m_server_data;
    STREAM_CONFIGURATION m_config;
};
