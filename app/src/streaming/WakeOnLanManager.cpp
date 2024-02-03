#include "WakeOnLanManager.hpp"
#include "Data.hpp"
#include "Settings.hpp"
#include <borealis.hpp>
#include <cerrno>
#include <cstring>

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

    // 16 repitiions of MAC address
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

    // Set server end point (the broadcast addres)
    udpServer.sin_family = AF_INET;
#if defined(__SWITCH__)
    uint32_t ip, subnet_mask;
    // Get the current IP address and subnet mask to calculate subnet broadcast address
    nifmGetCurrentIpConfigInfo(&ip, &subnet_mask, nullptr, nullptr, nullptr);
    udpServer.sin_addr.s_addr = ip | ~subnet_mask;
#else
    udpServer.sin_addr.s_addr = inet_addr(host.address.c_str());
#endif
    udpServer.sin_port = htons(9);

    brls::Logger::info("WakeOnLanManager: Sending magic packet to: '{}'",
                    inet_ntoa(udpServer.sin_addr));

    // Send the packet
    ssize_t result =
        sendto(udpSocket, payload.bytes(), sizeof(unsigned char) * 102, 0,
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

    // Set server end point (the broadcast addres)
    udpServer.sin_family = AF_INET;
#if defined(__SWITCH__)
    uint32_t ip, subnet_mask;
    // Get the current IP address and subnet mask to calculate subnet broadcast address
    nifmGetCurrentIpConfigInfo(&ip, &subnet_mask, nullptr, nullptr, nullptr);
    udpServer.sin_addr.s_addr = ip | ~subnet_mask;
#else
    udpServer.sin_addr.s_addr = inet_addr(host.address.c_str());
#endif
    udpServer.sin_port = htons(9);

    brls::Logger::info("WakeOnLanManager: Sending magic packet to: '{}'",
                    inet_ntoa(udpServer.sin_addr));

    // Send the packet
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
