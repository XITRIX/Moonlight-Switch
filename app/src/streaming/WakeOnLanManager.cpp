#include "WakeOnLanManager.hpp"
#include "Data.hpp"
#include "Settings.hpp"
#include <borealis.hpp>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#if defined(__linux) || defined(__APPLE__) || defined(__SWITCH__) || defined(__vita__)
#define UNIX_SOCKS
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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
namespace {
constexpr unsigned short DEFAULT_SUNSHINE_PORT = 47989;
constexpr std::array<unsigned short, 2> STATIC_WOL_PORTS = {
    9,
    47009,
};
constexpr std::array<unsigned short, 5> DYNAMIC_WOL_PORTS = {
    47998,
    47999,
    48000,
    48002,
    48010,
};

std::string normalize_mac_address(const std::string& mac) {
    std::string normalized;
    normalized.reserve(mac.size());

    for (unsigned char ch : mac) {
        if (std::isxdigit(ch)) {
            normalized.push_back(static_cast<char>(std::toupper(ch)));
        } else if (ch == ':' || ch == '-' || std::isspace(ch)) {
            continue;
        } else {
            return "";
        }
    }

    return normalized.size() == 12 ? normalized : "";
}

bool try_parse_port(const std::string& portText, unsigned short& port) {
    if (portText.empty()) {
        return false;
    }

    if (!std::all_of(portText.begin(), portText.end(),
                     [](unsigned char ch) { return std::isdigit(ch); })) {
        return false;
    }

    const unsigned long parsedPort = std::stoul(portText);
    if (parsedPort == 0 || parsedPort > 65535) {
        return false;
    }

    port = static_cast<unsigned short>(parsedPort);
    return true;
}

bool split_address_and_port(const std::string& addressPort,
                            std::string& address,
                            unsigned short& basePort) {
    if (addressPort.empty()) {
        return false;
    }

    basePort = DEFAULT_SUNSHINE_PORT;

    if (addressPort.front() == '[') {
        const auto closingBracket = addressPort.find(']');
        if (closingBracket == std::string::npos) {
            return false;
        }

        address = addressPort.substr(1, closingBracket - 1);
        if (closingBracket + 1 < addressPort.size()) {
            if (addressPort[closingBracket + 1] != ':') {
                return false;
            }

            return try_parse_port(addressPort.substr(closingBracket + 2),
                                  basePort);
        }

        return !address.empty();
    }

    const auto colonCount =
        std::count(addressPort.begin(), addressPort.end(), ':');

    if (colonCount == 1) {
        const auto portSeparator = addressPort.rfind(':');
        unsigned short parsedPort = 0;
        if (portSeparator != std::string::npos &&
            try_parse_port(addressPort.substr(portSeparator + 1), parsedPort)) {
            address = addressPort.substr(0, portSeparator);
            basePort = parsedPort;
            return !address.empty();
        }
    }

    address = addressPort;
    return true;
}

std::vector<std::string> wake_address_candidates(const Host& host) {
    std::vector<std::string> candidates;

    auto push_unique = [&candidates](const std::string& value) {
        if (!value.empty() &&
            std::find(candidates.begin(), candidates.end(), value) ==
                candidates.end()) {
            candidates.push_back(value);
        }
    };

    for (const auto& address : host.connection_addresses()) {
        push_unique(address);
    }

#if defined(__SWITCH__)
    uint32_t ip = 0;
    uint32_t subnet_mask = 0;
    if (R_SUCCEEDED(
            nifmGetCurrentIpConfigInfo(&ip, &subnet_mask, nullptr, nullptr,
                                       nullptr))) {
        in_addr broadcastAddress{};
        broadcastAddress.s_addr = ip | ~subnet_mask;

        char addressBuffer[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &broadcastAddress, addressBuffer,
                      sizeof(addressBuffer)) != nullptr) {
            push_unique(addressBuffer);
        }
    }
#endif

    push_unique("255.255.255.255");

    return candidates;
}

std::vector<unsigned short> wake_port_candidates(unsigned short basePort) {
    std::vector<unsigned short> ports;

    auto push_unique = [&ports](unsigned short port) {
        if (std::find(ports.begin(), ports.end(), port) == ports.end()) {
            ports.push_back(port);
        }
    };

    for (const auto port : STATIC_WOL_PORTS) {
        push_unique(port);
    }

    for (const auto port : DYNAMIC_WOL_PORTS) {
        push_unique(static_cast<unsigned short>(
            (port - DEFAULT_SUNSHINE_PORT) + basePort));
    }

    return ports;
}

Data create_payload(const std::string& normalizedMac) {
    Data macAddress(reinterpret_cast<unsigned char*>(
                        const_cast<char*>(normalizedMac.data())),
                    normalizedMac.size());
    macAddress = macAddress.hex_to_bytes();
    if (macAddress.size() != 6) {
        return Data();
    }

    Data payload(102);
    memset(payload.bytes(), 0xFF, 6);

    for (int i = 0; i < 16; i++) {
        memcpy(payload.bytes() + 6 + (i * 6), macAddress.bytes(),
               macAddress.size());
    }

    return payload;
}

void populate_port(sockaddr_storage& address, unsigned short port) {
    if (address.ss_family == AF_INET) {
        reinterpret_cast<sockaddr_in*>(&address)->sin_port = htons(port);
    } else if (address.ss_family == AF_INET6) {
        reinterpret_cast<sockaddr_in6*>(&address)->sin6_port = htons(port);
    }
}

#if defined(UNIX_SOCKS)
using SocketHandle = int;

bool is_invalid_socket(SocketHandle socket) { return socket == -1; }

void close_socket(SocketHandle socket) { close(socket); }

std::string last_socket_error_string() { return std::string(strerror(errno)); }
#elif defined(WIN32_SOCKS)
using SocketHandle = SOCKET;

bool is_invalid_socket(SocketHandle socket) { return socket == INVALID_SOCKET; }

void close_socket(SocketHandle socket) { closesocket(socket); }

std::string last_socket_error_string() {
    return "Winsock error " + std::to_string(WSAGetLastError());
}
#endif

GSResult<bool> send_magic_packets(const Host& host, const Data& payload) {
    if (payload.is_empty()) {
        return GSResult<bool>::failure("Magic packet payload is empty");
    }

    std::string lastError = "Failed to resolve any wake address";
    size_t packetsSent = 0;

    for (const auto& candidate : wake_address_candidates(host)) {
        std::string rawAddress;
        unsigned short basePort = DEFAULT_SUNSHINE_PORT;
        if (!split_address_and_port(candidate, rawAddress, basePort) ||
            rawAddress.empty()) {
            brls::Logger::warning(
                "WakeOnLanManager: Skipping invalid wake address '{}'",
                candidate);
            continue;
        }

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
#ifdef AI_ADDRCONFIG
        hints.ai_flags = AI_ADDRCONFIG;
#endif

        addrinfo* result = nullptr;
        const int resolveResult =
            getaddrinfo(rawAddress.c_str(), nullptr, &hints, &result);
        if (resolveResult != 0 || result == nullptr) {
            lastError = "Failed to resolve wake address: " + rawAddress;
            brls::Logger::warning("WakeOnLanManager: {}", lastError);
            continue;
        }

        for (addrinfo* current = result; current != nullptr;
             current = current->ai_next) {
            SocketHandle socketHandle =
                socket(current->ai_family, SOCK_DGRAM, IPPROTO_UDP);
            if (is_invalid_socket(socketHandle)) {
                lastError =
                    "Failed to create wake socket: " + last_socket_error_string();
                brls::Logger::warning("WakeOnLanManager: {}", lastError);
                continue;
            }

            if (current->ai_family == AF_INET) {
                int broadcast = 1;
                if (setsockopt(socketHandle, SOL_SOCKET, SO_BROADCAST,
#if defined(WIN32_SOCKS)
                               reinterpret_cast<const char*>(&broadcast),
#else
                               &broadcast,
#endif
                               sizeof(broadcast)) == -1) {
                    lastError = "Failed to enable broadcast: " +
                                last_socket_error_string();
                    brls::Logger::warning("WakeOnLanManager: {}", lastError);
                    close_socket(socketHandle);
                    continue;
                }
            }

            sockaddr_storage target{};
            memcpy(&target, current->ai_addr, current->ai_addrlen);

            for (const auto port : wake_port_candidates(basePort)) {
                populate_port(target, port);

                const int sendResult = sendto(
                    socketHandle,
#if defined(WIN32_SOCKS)
                    reinterpret_cast<const char*>(payload.bytes()),
#else
                    payload.bytes(),
#endif
                    static_cast<int>(payload.size()), 0,
                    reinterpret_cast<sockaddr*>(&target),
#if defined(WIN32_SOCKS)
                    static_cast<int>(current->ai_addrlen));
#else
                    static_cast<socklen_t>(current->ai_addrlen));
#endif

                if (sendResult >= 0) {
                    packetsSent++;
                    brls::Logger::info(
                        "WakeOnLanManager: Sent magic packet to '{}' on port {}",
                        rawAddress, port);
                } else {
                    lastError = "Failed to send magic packet: " +
                                last_socket_error_string();
                    brls::Logger::warning("WakeOnLanManager: {}", lastError);
                }
            }

            close_socket(socketHandle);
        }

        freeaddrinfo(result);
    }

    if (packetsSent == 0) {
        return GSResult<bool>::failure(lastError);
    }

    return GSResult<bool>::success(true);
}
}
#endif

bool WakeOnLanManager::can_wake_up_host(const Host& host) {
#if defined(UNIX_SOCKS) || defined(WIN32_SOCKS)
    return !normalize_mac_address(host.mac).empty();
#else
    return false;
#endif
}

GSResult<bool> WakeOnLanManager::wake_up_host(const Host& host) {
    const std::string normalizedMac = normalize_mac_address(host.mac);
    if (normalizedMac.empty()) {
        return GSResult<bool>::failure("Missing or invalid host MAC address");
    }

    Data payload = create_payload(normalizedMac);
    if (payload.size() != 102) {
        return GSResult<bool>::failure("Failed to build Wake-on-LAN payload");
    }

#if defined(UNIX_SOCKS)
    return send_magic_packets(host, payload);
#elif defined(_WIN32)
    WSADATA data{};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
    if (startupResult != 0) {
        return GSResult<bool>::failure("Failed to initialize Winsock");
    }

    const auto result = send_magic_packets(host, payload);
    WSACleanup();
    return result;
#endif

    return GSResult<bool>::failure("Wake up host not supported...");
}
