#include "Singleton.hpp"
#include "GameStreamClient.hpp"
#include <stdio.h>

struct Host;

class WakeOnLanManager: public Singleton<WakeOnLanManager> {
private:
    bool can_wake_up_host(const Host &host);
    GSResult<bool> wake_up_host(const Host &host);
    
    friend class GameStreamClient;
};
