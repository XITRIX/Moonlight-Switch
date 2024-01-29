#include "GameStreamClient.hpp"
#include "Singleton.hpp"
#include <stdio.h>

struct Host;

class WakeOnLanManager : public Singleton<WakeOnLanManager> {
  private:
    static bool can_wake_up_host(const Host& host);
    static GSResult<bool> wake_up_host(const Host& host);

    friend class GameStreamClient;
};
