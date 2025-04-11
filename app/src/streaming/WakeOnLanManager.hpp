#include "GameStreamClient.hpp"
#include "Singleton.hpp"
#include <stdio.h>
#include <string>

struct Host;
class RsaManager; // Déclaration anticipée

class WakeOnLanManager : public Singleton<WakeOnLanManager> {
  private:
    static bool can_wake_up_host(const Host& host);
    static GSResult<bool> wake_up_host(const Host& host);

    // Ajout des déclarations pour les fonctions WOL sécurisées
    static GSResult<bool> wake_up_host_secure(const Host& host,
                                               const std::string& relay_address,
                                               int relay_port,
                                               RsaManager& rsa_manager);
    static GSResult<bool> setup_secure_wol(const std::string& relay_address,
                                           int relay_port,
                                           const std::string& key_path);
    static GSResult<bool> secure_wake(const Host& host,
                                      const std::string& relay_address,
                                      int relay_port,
                                      const std::string& private_key_path);

    friend class GameStreamClient;
};
