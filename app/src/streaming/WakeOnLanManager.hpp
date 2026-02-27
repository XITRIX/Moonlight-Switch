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
                                             RsaManager& rsa_manager, 
                                             int port = 9);

  public:
    static GSResult<bool> setup_secure_wol(const std::string& key_path);
    
    static GSResult<bool> secure_wake(const Host& host, 
                                     const std::string& private_key_path,
                                     int port = 9);

    friend class GameStreamClient;
};
