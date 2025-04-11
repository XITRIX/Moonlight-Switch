#include "WakeOnLanManager.hpp"
#include "Data.hpp"
#include "Settings.hpp"
#include <borealis.hpp>
#include <cerrno>
#include <cstring>

// Inclusion des bibliothèques mbedTLS pour la gestion RSA
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/error.h>
#include <ctime>

#if defined(__linux) || defined(__APPLE__) || defined(__SWITCH__) || defined(__vita__)
#define UNIX_SOCKS
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#elif defined(_WIN32)
#define WIN32_SOCKS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#endif

#if defined(__SWITCH__)
#include <switch.h>
#endif

// Classe pour gérer les clés RSA
class RsaManager {
private:
    mbedtls_pk_context m_key;
    mbedtls_entropy_context m_entropy;
    mbedtls_ctr_drbg_context m_ctr_drbg;
    bool m_initialized;

public:
    RsaManager() : m_initialized(false) {
        mbedtls_pk_init(&m_key);
        mbedtls_entropy_init(&m_entropy);
        mbedtls_ctr_drbg_init(&m_ctr_drbg);
    }

    ~RsaManager() {
        mbedtls_pk_free(&m_key);
        mbedtls_ctr_drbg_free(&m_ctr_drbg);
        mbedtls_entropy_free(&m_entropy);
    }

    // Génère une nouvelle paire de clés RSA
    GSResult<bool> generate_keys(unsigned int key_size = 2048) {
        const char *pers = "wol_key_gen";
        char error_buf[100];
        int ret;

        // Initialiser le générateur aléatoire
        ret = mbedtls_ctr_drbg_seed(&m_ctr_drbg, mbedtls_entropy_func, &m_entropy,
                                  (const unsigned char *)pers, strlen(pers));
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to seed RNG: ") + error_buf);
        }

        // Configurer pour RSA
        ret = mbedtls_pk_setup(&m_key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to setup PK: ") + error_buf);
        }

        // Générer la clé
        ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(m_key), mbedtls_ctr_drbg_random, 
                                &m_ctr_drbg, key_size, 65537);
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to generate key: ") + error_buf);
        }

        m_initialized = true;
        return GSResult<bool>::success(true);
    }

    // Exporte la clé publique au format PEM
    GSResult<std::string> export_public_key() {
        if (!m_initialized) {
            return GSResult<std::string>::failure("RSA not initialized");
        }

        unsigned char output[4096];
        int ret = mbedtls_pk_write_pubkey_pem(&m_key, output, sizeof(output));
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<std::string>::failure(std::string("Failed to export public key: ") + error_buf);
        }

        return GSResult<std::string>::success(std::string((char*)output));
    }

    // Signe un message avec la clé privée
    GSResult<Data> sign_data(const Data& data) {
        if (!m_initialized) {
            return GSResult<Data>::failure("RSA not initialized");
        }

        unsigned char hash[32];
        unsigned char signature[MBEDTLS_MPI_MAX_SIZE];
        size_t sig_len = 0;
        char error_buf[100];

        // Calculer le hash SHA-256 des données
        mbedtls_sha256(data.bytes(), data.size(), hash, 0);

        // Signer le hash
        int ret = mbedtls_pk_sign(&m_key, MBEDTLS_MD_SHA256, hash, sizeof(hash),
                               signature, &sig_len, mbedtls_ctr_drbg_random, &m_ctr_drbg);
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<Data>::failure(std::string("Failed to sign: ") + error_buf);
        }

        return GSResult<Data>::success(Data(signature, sig_len));
    }

    // Sauvegarde les clés dans un fichier
    GSResult<bool> save_keys(const std::string& private_key_file, const std::string& public_key_file) {
        if (!m_initialized) {
            return GSResult<bool>::failure("RSA not initialized");
        }

        // Exporter la clé privée
        unsigned char priv_buf[4096];
        int ret = mbedtls_pk_write_key_pem(&m_key, priv_buf, sizeof(priv_buf));
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to export private key: ") + error_buf);
        }

        // Exporter la clé publique
        unsigned char pub_buf[4096];
        ret = mbedtls_pk_write_pubkey_pem(&m_key, pub_buf, sizeof(pub_buf));
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to export public key: ") + error_buf);
        }

        // Sauvegarder les clés dans des fichiers
        FILE* f_priv = fopen(private_key_file.c_str(), "wb");
        if (!f_priv) {
            return GSResult<bool>::failure("Failed to open private key file");
        }
        fwrite(priv_buf, 1, strlen((char*)priv_buf), f_priv);
        fclose(f_priv);

        FILE* f_pub = fopen(public_key_file.c_str(), "wb");
        if (!f_pub) {
            return GSResult<bool>::failure("Failed to open public key file");
        }
        fwrite(pub_buf, 1, strlen((char*)pub_buf), f_pub);
        fclose(f_pub);

        return GSResult<bool>::success(true);
    }

    // Charge les clés depuis un fichier
    GSResult<bool> load_keys(const std::string& private_key_file) {
        unsigned char key_buf[4096];
        FILE* f = fopen(private_key_file.c_str(), "rb");
        if (!f) {
            return GSResult<bool>::failure("Failed to open key file");
        }

        size_t olen = fread(key_buf, 1, sizeof(key_buf) - 1, f);
        fclose(f);

        key_buf[olen] = 0;

        // Libérer la clé existante si nécessaire
        mbedtls_pk_free(&m_key);
        mbedtls_pk_init(&m_key);

        // Initialiser le générateur aléatoire
        const char *pers = "wol_key_load";
        int ret = mbedtls_ctr_drbg_seed(&m_ctr_drbg, mbedtls_entropy_func, &m_entropy,
                                     (const unsigned char *)pers, strlen(pers));
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to seed RNG: ") + error_buf);
        }

        // Parser la clé
        ret = mbedtls_pk_parse_key(&m_key, key_buf, olen + 1, NULL, 0);
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to parse key: ") + error_buf);
        }

        m_initialized = true;
        return GSResult<bool>::success(true);
    }

    // Envoie la clé publique au serveur relais
    GSResult<bool> register_key_with_relay(const std::string& relay_address, int port) {
        auto public_key_result = export_public_key();
        if (!public_key_result.isSuccess()) {
            return GSResult<bool>::failure(public_key_result.error());
        }

        std::string public_key = public_key_result.value();
        std::string message = "REGISTER_KEY:" + public_key;

        struct sockaddr_in server;
        int sock;

        // Créer socket
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            return GSResult<bool>::failure("Failed to create socket");
        }

        // Configuration de l'adresse du serveur
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(relay_address.c_str());
        server.sin_port = htons(port);

        // Envoyer la clé
        int res = sendto(sock, message.c_str(), message.length(), 0,
                      (struct sockaddr*)&server, sizeof(server));

        if (res < 0) {
            close(sock);
            return GSResult<bool>::failure("Failed to send key to relay");
        }

        // Attendre une réponse avec un timeout
        char buffer[1024];
        struct timeval tv;
        tv.tv_sec = 5;  // 5 secondes de timeout
        tv.tv_usec = 0;

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            close(sock);
            return GSResult<bool>::failure("Failed to set socket timeout");
        }

        // Recevoir la réponse
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        res = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, 
                    (struct sockaddr*)&from, &fromlen);

        close(sock);

        if (res < 0) {
            return GSResult<bool>::failure("No response from relay server");
        }

        buffer[res] = 0;
        if (strncmp(buffer, "OK", 2) == 0) {
            return GSResult<bool>::success(true);
        } else {
            return GSResult<bool>::failure(std::string("Relay error: ") + buffer);
        }
    }
};

#if defined(UNIX_SOCKS) || defined(WIN32_SOCKS)

static Data mac_string_to_bytes(std::string mac) {
    std::string str = mac;
    std::string pattern = ":";

    std::string::size_type i = str.find(pattern);
    while (i != std::string::npos) {
        str.erase(i, pattern.length());
        i = str.find(pattern, i);
    }
    return Data((unsigned char*)str.c_str(), str.length()).hex_to_bytes();
}

static Data create_payload(const Host& host) {
    Data payload;

    // 6 bytes of FF
    uint8_t header = 0xFF;
    for (int i = 0; i < 6; i++) {
        payload = payload.append(Data(&header, 1));
    }

    // 16 repetitions of MAC address
    Data mac_address = mac_string_to_bytes(host.mac);
    for (int i = 0; i < 16; i++) {
        payload = payload.append(mac_address);
    }
    return payload;
}
#endif

#if defined(UNIX_SOCKS)
GSResult<bool> send_packet_unix(const Host& host, const Data& payload) {
    struct sockaddr_in udpClient{}, udpServer{};
    int broadcast = 1;

    int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        brls::Logger::error(
            "WakeOnLanManager: An error was encountered creating "
            "the UDP socket: '{}'",
            strerror(errno));
        return GSResult<bool>::failure(
            "An error was encountered creating the UDP socket: " +
            std::string(strerror(errno)));
    }

    int setsock_result = setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST,
                                    &broadcast, sizeof broadcast);
    if (setsock_result == -1) {
        brls::Logger::error(
            "WakeOnLanManager: Failed to set socket options: '{}'",
            strerror(errno));
        return GSResult<bool>::failure("Failed to set socket options: " +
                                       std::string(strerror(errno)));
    }

    // Set parameters
    udpClient.sin_family = AF_INET;
    udpClient.sin_addr.s_addr = INADDR_ANY;
    udpClient.sin_port = 0;
    // Bind socket
    int bind_result =
        bind(udpSocket, (struct sockaddr*)&udpClient, sizeof(udpClient));
    if (bind_result == -1) {
        brls::Logger::error("WakeOnLanManager: Failed to bind socket: '{}'",
                            strerror(errno));
        return GSResult<bool>::failure("Failed to bind socket: " +
                                       std::string(strerror(errno)));
    }

    // Send to local broadcast address
#if defined(__SWITCH__)
    uint32_t ip, subnet_mask;
    nifmGetCurrentIpConfigInfo(&ip, &subnet_mask, nullptr, nullptr, nullptr);
    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = ip | ~subnet_mask; // Local broadcast address
    brls::Logger::info("WakeOnLanManager: Sending magic packet to local broadcast address: '{}'",
                    inet_ntoa(udpServer.sin_addr));
#else
    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = htonl(INADDR_BROADCAST); // Default broadcast address for other platforms
    brls::Logger::info("WakeOnLanManager: Sending magic packet to default broadcast address");
#endif
    udpServer.sin_port = htons(9);

    // Send the WoL packet to the local broadcast address
    ssize_t result = sendto(udpSocket, payload.bytes(), sizeof(unsigned char) * 102, 0,
                            (struct sockaddr*)&udpServer, sizeof(udpServer));
    if (result == -1) {
        brls::Logger::error(
            "WakeOnLanManager: Failed to send magic packet to socket: '{}'",
            strerror(errno));
        return GSResult<bool>::failure(
            "Failed to send magic packet to socket: " +
            std::string(strerror(errno)));
    }

    // Send to the public IP address
    udpServer.sin_addr.s_addr = inet_addr(host.address.c_str());
    brls::Logger::info("WakeOnLanManager: Sending magic packet to public IP: '{}'",
                    inet_ntoa(udpServer.sin_addr));

    result = sendto(udpSocket, payload.bytes(), sizeof(unsigned char) * 102, 0,
                    (struct sockaddr*)&udpServer, sizeof(udpServer));
    if (result == -1) {
        brls::Logger::error(
            "WakeOnLanManager: Failed to send magic packet to socket: '{}'",
            strerror(errno));
        return GSResult<bool>::failure(
            "Failed to send magic packet to socket: " +
            std::string(strerror(errno)));
    }

    return GSResult<bool>::success(true);
}
#elif defined(_WIN32)
GSResult<bool> send_packet_win32(const Host& host, const Data& payload) {
    struct sockaddr_in udpClient, udpServer;
    int broadcast = 1;

    WSADATA data;
    SOCKET udpSocket;

    // Setup broadcast socket
    WSAStartup(MAKEWORD(2, 2), &data);
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast,
                   sizeof(broadcast)) == -1) {
        return GSResult<bool>::failure("Failed to setup a broadcast socket");
    }

    // Set parameters
    udpClient.sin_family = AF_INET;
    udpClient.sin_addr.s_addr = INADDR_ANY;
    udpClient.sin_port = htons(0);
    // Bind socket
    bind(udpSocket, (struct sockaddr*)&udpClient, sizeof(udpClient));

    // Send to local broadcast address
    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = htonl(INADDR_BROADCAST); // Default broadcast address
    udpServer.sin_port = htons(9);

    brls::Logger::info("WakeOnLanManager: Sending magic packet to local broadcast address: '{}'",
                    inet_ntoa(udpServer.sin_addr));

    // Send the WoL packet to the local broadcast address
    sendto(udpSocket, (const char*)payload.bytes(), sizeof(unsigned char) * 102,
           0, (struct sockaddr*)&udpServer, sizeof(udpServer));

    // Send to the public IP address
    udpServer.sin_addr.s_addr = inet_addr(host.address.c_str());
    brls::Logger::info("WakeOnLanManager: Sending magic packet to public IP: '{}'",
                    inet_ntoa(udpServer.sin_addr));

    sendto(udpSocket, (const char*)payload.bytes(), sizeof(unsigned char) * 102,
           0, (struct sockaddr*)&udpServer, sizeof(udpServer));

    return GSResult<bool>::success(true);
}
#endif

bool WakeOnLanManager::can_wake_up_host(const Host& host) {
#if defined(UNIX_SOCKS) || defined(WIN32_SOCKS)
    return true;
#else
    return false;
#endif
}

GSResult<bool> WakeOnLanManager::wake_up_host(const Host& host) {
    Data payload = create_payload(host);

#if defined(UNIX_SOCKS)
    return send_packet_unix(host, payload);
#elif defined(_WIN32)
    return send_packet_win32(host, payload);
#endif

    return GSResult<bool>::failure("Wake up host not supported...");
}

// Fonction pour envoyer un paquet WOL sécurisé
GSResult<bool> WakeOnLanManager::wake_up_host_secure(const Host& host, 
                                                    const std::string& relay_address, 
                                                    int relay_port,
                                                    RsaManager& rsa_manager) {
    // Créer le paquet WOL standard
    Data payload = create_payload(host);
    
    // Ajouter un timestamp pour éviter les attaques par rejeu
    uint64_t now = static_cast<uint64_t>(time(nullptr));
    Data timestamp_data((unsigned char*)&now, sizeof(uint64_t));
    payload = payload.append(timestamp_data);
    
    // Signer le paquet complet
    auto signature_result = rsa_manager.sign_data(payload);
    if (!signature_result.isSuccess()) {
        return GSResult<bool>::failure(signature_result.error());
    }
    
    Data signature = signature_result.value();
    
    // Ajouter la taille de la signature à la fin du paquet
    uint32_t sig_size = signature.size();
    Data sig_size_data((unsigned char*)&sig_size, sizeof(uint32_t));
    
    // Assembler le paquet final: payload + signature + taille signature
    Data final_payload = payload.append(signature).append(sig_size_data);
    
    // Envoyer le paquet au serveur relais
#if defined(UNIX_SOCKS)
    struct sockaddr_in udpClient{}, udpServer{};
    
    int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        return GSResult<bool>::failure("Failed to create UDP socket");
    }
    
    // Configuration du socket
    udpClient.sin_family = AF_INET;
    udpClient.sin_addr.s_addr = INADDR_ANY;
    udpClient.sin_port = 0;
    
    int bind_result = bind(udpSocket, (struct sockaddr*)&udpClient, sizeof(udpClient));
    if (bind_result == -1) {
        close(udpSocket);
        return GSResult<bool>::failure("Failed to bind socket");
    }
    
    // Configuration du serveur relais
    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = inet_addr(relay_address.c_str());
    udpServer.sin_port = htons(relay_port);
    
    brls::Logger::info("WakeOnLanManager: Sending secured WOL packet to relay at {}:{}",
                    relay_address, relay_port);
    
    // Envoi du paquet au serveur relais
    ssize_t result = sendto(udpSocket, final_payload.bytes(), final_payload.size(), 0,
                          (struct sockaddr*)&udpServer, sizeof(udpServer));
    
    close(udpSocket);
    
    if (result == -1) {
        return GSResult<bool>::failure("Failed to send secured WOL packet");
    }
    
    return GSResult<bool>::success(true);
#else
    return GSResult<bool>::failure("Secure WOL not supported on this platform");
#endif
}

// Fonction pour générer une paire de clés et les enregistrer auprès du serveur relais
GSResult<bool> WakeOnLanManager::setup_secure_wol(const std::string& relay_address, 
                                                 int relay_port,
                                                 const std::string& key_path) {
    RsaManager rsa_manager;
    
    // Générer les clés RSA
    auto gen_result = rsa_manager.generate_keys();
    if (!gen_result.isSuccess()) {
        return GSResult<bool>::failure("Failed to generate RSA keys: " + gen_result.error());
    }
    
    // Sauvegarder la clé privée localement
    std::string private_key_path = key_path + "/wol_private.pem";
    std::string public_key_path = key_path + "/wol_public.pem";
    
    auto save_result = rsa_manager.save_keys(private_key_path, public_key_path);
    if (!save_result.isSuccess()) {
        return GSResult<bool>::failure("Failed to save keys: " + save_result.error());
    }
    
    // Enregistrer la clé publique auprès du serveur relais
    auto reg_result = rsa_manager.register_key_with_relay(relay_address, relay_port + 1);
    if (!reg_result.isSuccess()) {
        return GSResult<bool>::failure("Failed to register key with relay: " + reg_result.error());
    }
    
    return GSResult<bool>::success(true);
}

// Fonction pour charger une clé existante et envoyer un paquet WOL sécurisé
GSResult<bool> WakeOnLanManager::secure_wake(const Host& host, 
                                            const std::string& relay_address,
                                            int relay_port,
                                            const std::string& private_key_path) {
    RsaManager rsa_manager;
    
    // Charger la clé privée
    auto load_result = rsa_manager.load_keys(private_key_path);
    if (!load_result.isSuccess()) {
        return GSResult<bool>::failure("Failed to load private key: " + load_result.error());
    }
    
    // Envoyer le paquet WOL sécurisé
    return wake_up_host_secure(host, relay_address, relay_port, rsa_manager);
}
