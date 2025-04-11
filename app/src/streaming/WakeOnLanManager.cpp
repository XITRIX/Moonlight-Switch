#include "WakeOnLanManager.hpp"
#include "Data.hpp"
#include "Settings.hpp"
#include <borealis.hpp>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

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

    GSResult<bool> generate_keys(unsigned int key_size = 2048) {
        const char *pers = "wol_key_gen";
        char error_buf[100];
        int ret;

        ret = mbedtls_ctr_drbg_seed(&m_ctr_drbg, mbedtls_entropy_func, &m_entropy,
                                  (const unsigned char *)pers, strlen(pers));
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to seed RNG: ") + error_buf);
        }

        ret = mbedtls_pk_setup(&m_key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to setup PK: ") + error_buf);
        }

        ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(m_key), mbedtls_ctr_drbg_random, 
                                &m_ctr_drbg, key_size, 65537);
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to generate key: ") + error_buf);
        }

        m_initialized = true;
        return GSResult<bool>::success(true);
    }

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

    GSResult<Data> sign_data(const Data& data) {
        if (!m_initialized) {
            return GSResult<Data>::failure("RSA not initialized");
        }

        unsigned char hash[32];
        unsigned char signature[MBEDTLS_MPI_MAX_SIZE];
        size_t sig_len = 0;
        char error_buf[100];

        mbedtls_sha256(data.bytes(), data.size(), hash, 0);

        int ret = mbedtls_pk_sign(&m_key, MBEDTLS_MD_SHA256, hash, sizeof(hash),
                               signature, &sig_len, mbedtls_ctr_drbg_random, &m_ctr_drbg);
        if (ret != 0) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<Data>::failure(std::string("Failed to sign: ") + error_buf);
        }

        return GSResult<Data>::success(Data(signature, sig_len));
    }

    GSResult<bool> save_keys(const std::string& private_key_file, const std::string& public_key_file) {
        if (!m_initialized) {
            return GSResult<bool>::failure("RSA not initialized");
        }

        unsigned char priv_buf[4096];
        int ret = mbedtls_pk_write_key_pem(&m_key, priv_buf, sizeof(priv_buf));
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to export private key: ") + error_buf);
        }

        unsigned char pub_buf[4096];
        ret = mbedtls_pk_write_pubkey_pem(&m_key, pub_buf, sizeof(pub_buf));
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to export public key: ") + error_buf);
        }

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

    GSResult<bool> load_keys(const std::string& private_key_file) {
        unsigned char key_buf[4096];
        FILE* f = fopen(private_key_file.c_str(), "rb");
        if (!f) {
            return GSResult<bool>::failure("Failed to open key file");
        }

        size_t olen = fread(key_buf, 1, sizeof(key_buf) - 1, f);
        fclose(f);

        key_buf[olen] = 0;

        mbedtls_pk_free(&m_key);
        mbedtls_pk_init(&m_key);

        const char *pers = "wol_key_load";
        int ret = mbedtls_ctr_drbg_seed(&m_ctr_drbg, mbedtls_entropy_func, &m_entropy,
                                     (const unsigned char *)pers, strlen(pers));
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to seed RNG: ") + error_buf);
        }

        ret = mbedtls_pk_parse_key(&m_key, key_buf, olen + 1, NULL, 0);
        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            return GSResult<bool>::failure(std::string("Failed to parse key: ") + error_buf);
        }

        m_initialized = true;
        return GSResult<bool>::success(true);
    }

    GSResult<bool> register_key_with_relay(const std::string& relay_address, int port) {
        auto public_key_result = export_public_key();
        if (!public_key_result.isSuccess()) {
            return GSResult<bool>::failure(public_key_result.error());
        }

        std::string public_key = public_key_result.value();
        std::string message = "REGISTER_KEY:" + public_key;

        struct sockaddr_in server;
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            return GSResult<bool>::failure("Failed to create socket");
        }

        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(relay_address.c_str());
        server.sin_port = htons(port);

        int res = sendto(sock, message.c_str(), message.length(), 0,
                      (struct sockaddr*)&server, sizeof(server));

        if (res < 0) {
            close(sock);
            return GSResult<bool>::failure("Failed to send key to relay");
        }

        char buffer[1024];
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            close(sock);
            return GSResult<bool>::failure("Failed to set socket timeout");
        }

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

GSResult<bool> WakeOnLanManager::setup_secure_wol(const std::string& relay_address,
                                                  int relay_port) {
    const std::string key_folder = "keys";
    const std::string private_key_path = key_folder + "/wol_private.pem";
    const std::string public_key_path = key_folder + "/wol_public.pem";

    RsaManager rsa_manager;

    struct stat st {};
    if (stat(key_folder.c_str(), &st) != 0) {
        if (mkdir(key_folder.c_str(), 0777) != 0) {
            return GSResult<bool>::failure("Failed to create keys directory");
        }
    }

    if (access(private_key_path.c_str(), F_OK) == 0) {
        auto load_result = rsa_manager.load_keys(private_key_path);
        if (!load_result.isSuccess()) {
            return GSResult<bool>::failure("Failed to load existing key: " + load_result.error());
        }
        brls::Logger::info("WakeOnLanManager: Loaded existing keys");
    } else {
        auto gen_result = rsa_manager.generate_keys();
        if (!gen_result.isSuccess()) {
            return GSResult<bool>::failure("Failed to generate RSA keys: " + gen_result.error());
        }

        auto save_result = rsa_manager.save_keys(private_key_path, public_key_path);
        if (!save_result.isSuccess()) {
            return GSResult<bool>::failure("Failed to save keys: " + save_result.error());
        }
        brls::Logger::info("WakeOnLanManager: Generated and saved new RSA keys");
    }

    auto reg_result = rsa_manager.register_key_with_relay(relay_address, relay_port + 1);
    if (!reg_result.isSuccess()) {
        return GSResult<bool>::failure("Failed to register key with relay: " + reg_result.error());
    }

    return GSResult<bool>::success(true);
}
GSResult<bool> WakeOnLanManager::secure_wake(const Host& host,
                                             const std::string& relay_address,
                                             int relay_port) {
    RsaManager rsa_manager;
    const std::string private_key_path = "keys/wol_private.pem";

    auto load_result = rsa_manager.load_keys(private_key_path);
    if (!load_result.isSuccess()) {
        return GSResult<bool>::failure("Failed to load private key: " + load_result.error());
    }

    return wake_up_host_secure(host, relay_address, relay_port, rsa_manager);
}

// === Ajout des fonctions manquantes ===

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

    // 6 bytes de 0xFF
    uint8_t header = 0xFF;
    for (int i = 0; i < 6; i++) {
        payload = payload.append(Data(&header, 1));
    }

    // 16 répétitions de l'adresse MAC
    Data mac_address = mac_string_to_bytes(host.mac);
    for (int i = 0; i < 16; i++) {
        payload = payload.append(mac_address);
    }

    return payload;
}

#if defined(UNIX_SOCKS)
GSResult<bool> send_packet_unix(const Host& host, const Data& payload) {
    struct sockaddr_in udpClient{}, udpServer{};
    int broadcast = 1;

    int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == -1) {
        return GSResult<bool>::failure("Failed to create UDP socket");
    }

    setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast);

    udpClient.sin_family = AF_INET;
    udpClient.sin_addr.s_addr = INADDR_ANY;
    udpClient.sin_port = 0;

    bind(udpSocket, (struct sockaddr*)&udpClient, sizeof(udpClient));

#if defined(__SWITCH__)
    uint32_t ip, subnet_mask;
    nifmGetCurrentIpConfigInfo(&ip, &subnet_mask, nullptr, nullptr, nullptr);
    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = ip | ~subnet_mask;
#else
    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = htonl(INADDR_BROADCAST);
#endif
    udpServer.sin_port = htons(9);

    sendto(udpSocket, payload.bytes(), 102, 0,
           (struct sockaddr*)&udpServer, sizeof(udpServer));

    udpServer.sin_addr.s_addr = inet_addr(host.address.c_str());
    sendto(udpSocket, payload.bytes(), 102, 0,
           (struct sockaddr*)&udpServer, sizeof(udpServer));

    close(udpSocket);
    return GSResult<bool>::success(true);
}
#elif defined(_WIN32)
GSResult<bool> send_packet_win32(const Host& host, const Data& payload) {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int broadcast = 1;
    setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));

    struct sockaddr_in udpClient{}, udpServer{};
    udpClient.sin_family = AF_INET;
    udpClient.sin_addr.s_addr = INADDR_ANY;
    udpClient.sin_port = 0;
    bind(udpSocket, (struct sockaddr*)&udpClient, sizeof(udpClient));

    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    udpServer.sin_port = htons(9);
    sendto(udpSocket, (const char*)payload.bytes(), 102, 0,
           (struct sockaddr*)&udpServer, sizeof(udpServer));

    udpServer.sin_addr.s_addr = inet_addr(host.address.c_str());
    sendto(udpSocket, (const char*)payload.bytes(), 102, 0,
           (struct sockaddr*)&udpServer, sizeof(udpServer));

    closesocket(udpSocket);
    WSACleanup();
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
#else
    return GSResult<bool>::failure("Wake up host not supported on this platform");
#endif
}

