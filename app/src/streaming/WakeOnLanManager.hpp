#include "GameStreamClient.hpp"
#include "Singleton.hpp"
#include <stdio.h>
#include <string>

struct Host;
class RsaManager;

class WakeOnLanManager : public Singleton<WakeOnLanManager> {
  private:
    static bool can_wake_up_host(const Host& host);
    static GSResult<bool> wake_up_host(const Host& host);

    static GSResult<bool> wake_up_host_secure(const Host& host,
                                              const std::string& relay_address,
                                              int relay_port,
                                              RsaManager& rsa_manager);

    static GSResult<bool> setup_secure_wol(const std::string& relay_address,
                                           int relay_port);

    static GSResult<bool> secure_wake(const Host& host,
                                      const std::string& relay_address,
                                      int relay_port);

    friend class GameStreamClient;
};
