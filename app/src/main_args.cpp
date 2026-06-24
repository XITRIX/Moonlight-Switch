//
//  main_args.cpp
//  Moonlight
//
//  Created by XITRIX on 22.01.2024.
//

#include "main_args.hpp"
#include <borealis.hpp>
#include "streaming_view.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <optional>
#include <string_view>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#endif

using namespace brls;

bool canStartApp(int argc, char** argv);

namespace {

constexpr std::string_view DEEP_LINK_SCHEME = "moonlightswitch";

struct LaunchRequest {
    std::string mac;
    std::string ip;
    std::string appId;
    std::string appName;
};

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool startsWithNoCase(std::string_view value, std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    for (size_t i = 0; i < prefix.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(value[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }

    return true;
}

bool isDeepLinkUrl(std::string_view value) {
    const size_t schemeEnd = value.find(':');
    return schemeEnd == DEEP_LINK_SCHEME.size() &&
           startsWithNoCase(value.substr(0, schemeEnd), DEEP_LINK_SCHEME);
}

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

std::string urlDecode(std::string_view value, bool plusAsSpace = false) {
    std::string decoded;
    decoded.reserve(value.size());

    for (size_t i = 0; i < value.size(); i++) {
        const char ch = value[i];
        if (ch == '%' && i + 2 < value.size()) {
            const int high = hexValue(value[i + 1]);
            const int low = hexValue(value[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }

        decoded.push_back(ch == '+' && plusAsSpace ? ' ' : ch);
    }

    return decoded;
}

void decodeSwitchAppName(std::string& value) {
    constexpr std::string_view encodedSpace = "&#160;";
    size_t pos = 0;
    while ((pos = value.find(encodedSpace, pos)) != std::string::npos) {
        value.replace(pos, encodedSpace.size(), " ");
        pos++;
    }
}

std::string normalizeKey(std::string key) {
    while (key.starts_with('-')) {
        key.erase(key.begin());
    }

    key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char ch) {
        return ch == '-' || ch == '_';
    }), key.end());

    return toLower(std::move(key));
}

void setLaunchValue(LaunchRequest& request, std::string key, std::string value) {
    key = normalizeKey(std::move(key));

    if (key == "host" || key == "mac") {
        request.mac = std::move(value);
    } else if (key == "ip" || key == "address") {
        request.ip = std::move(value);
    } else if (key == "appid") {
        request.appId = std::move(value);
    } else if (key == "appname" || key == "name") {
        request.appName = std::move(value);
    }
}

LaunchRequest parseArgv(int argc, char** argv) {
    LaunchRequest request;

    for (int i = 1; i < argc; i++) {
        const std::string arg(argv[i]);
        if (!arg.starts_with("--")) {
            continue;
        }

        const size_t separator = arg.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        setLaunchValue(request, arg.substr(2, separator - 2),
                       arg.substr(separator + 1));
    }

    return request;
}

void parseQuery(std::string_view query, LaunchRequest& request) {
    while (!query.empty()) {
        const size_t separator = query.find_first_of("&;");
        const std::string_view pair = query.substr(0, separator);
        query = separator == std::string_view::npos
                    ? std::string_view()
                    : query.substr(separator + 1);

        if (pair.empty()) {
            continue;
        }

        const size_t equals = pair.find('=');
        const auto rawKey = equals == std::string_view::npos
                                ? pair
                                : pair.substr(0, equals);
        const auto rawValue = equals == std::string_view::npos
                                  ? std::string_view()
                                  : pair.substr(equals + 1);

        setLaunchValue(request, urlDecode(rawKey, true),
                       urlDecode(rawValue, true));
    }
}

std::vector<std::string> splitArgumentPayload(std::string_view payload) {
    std::vector<std::string> tokens;
    std::string current;
    char quote = '\0';

    for (const char ch : payload) {
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            } else {
                current.push_back(ch);
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }

    return tokens;
}

void parseArgumentPayload(std::string_view payload, LaunchRequest& request) {
    for (const std::string& token : splitArgumentPayload(payload)) {
        if (!token.starts_with("--")) {
            continue;
        }

        const size_t separator = token.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        setLaunchValue(request, token.substr(2, separator - 2),
                       token.substr(separator + 1));
    }
}

std::optional<int> parseAppId(const std::string& value) {
    int appId = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, appId);
    if (ec != std::errc() || ptr != end) {
        return std::nullopt;
    }
    return appId;
}

LaunchRequest parseDeepLinkUrl(const std::string& url) {
    LaunchRequest request;

    const size_t schemeEnd = url.find(':');
    if (schemeEnd == std::string::npos) {
        return request;
    }

    const size_t queryStart = url.find('?', schemeEnd + 1);
    const size_t fragmentStart = url.find('#', schemeEnd + 1);
    const size_t queryEnd = fragmentStart == std::string::npos
                                ? url.size()
                                : fragmentStart;

    if (queryStart != std::string::npos && queryStart < queryEnd) {
        parseQuery(std::string_view(url).substr(queryStart + 1, queryEnd - queryStart - 1),
                   request);
    }

    const size_t authorityStart = schemeEnd + 1;
    if (url.compare(authorityStart, 2, "//") == 0) {
        const size_t hostStart = authorityStart + 2;
        const size_t hostEnd = std::min({
            url.find('/', hostStart),
            queryStart == std::string::npos ? url.size() : queryStart,
            fragmentStart == std::string::npos ? url.size() : fragmentStart,
        });
        const std::string authority = urlDecode(
            std::string_view(url).substr(hostStart, hostEnd - hostStart));
        if (!authority.empty() && authority != "launch" &&
            request.mac.empty() && request.ip.empty()) {
            request.ip = authority;
        }
    }

    if (request.mac.empty() && request.ip.empty() && request.appId.empty()) {
        const size_t payloadStart = url.compare(schemeEnd + 1, 2, "//") == 0
                                        ? schemeEnd + 3
                                        : schemeEnd + 1;
        const size_t payloadEnd = std::min(queryStart == std::string::npos ? url.size() : queryStart,
                                           fragmentStart == std::string::npos ? url.size() : fragmentStart);
        std::string payload = urlDecode(
            std::string_view(url).substr(payloadStart, payloadEnd - payloadStart),
            true);
        std::replace(payload.begin(), payload.end(), '&', ' ');
        parseArgumentPayload(payload, request);
    }

    return request;
}

bool startFromLaunchRequest(LaunchRequest request, bool resetActivityStack) {
    if (!canStartApp(0, nullptr)) {
        return true;
    }

    decodeSwitchAppName(request.appName);

    Logger::debug("Host {}", request.mac);
    Logger::debug("IP {}", request.ip);
    Logger::debug("Id {}", request.appId);
    Logger::debug("Name {}", request.appName);

    if ((request.mac.empty() && request.ip.empty()) || request.appId.empty()) {
        return false;
    }

    const std::optional<int> appId = parseAppId(request.appId);
    if (!appId.has_value()) {
        Logger::warning("Invalid app id in forwarder launch: {}", request.appId);
        return false;
    }

    auto hosts = Settings::instance().hosts();
    if (auto it = std::find_if(hosts.begin(), hosts.end(), [request](const Host& host) {
            return host.mac == request.mac || host.has_address(request.ip);
        }); it != std::end(hosts)) {
        const Host& host = *it;
        AppInfo info { request.appName, appId.value() };

        if (resetActivityStack) {
            Application::giveFocus(nullptr);
        }

        auto* frame = new AppletFrame(new StreamingView(host, info));
        frame->setBackground(ViewBackground::NONE);
        frame->setHeaderVisibility(brls::Visibility::GONE);
        frame->setFooterVisibility(brls::Visibility::GONE);
        Application::pushActivity(new Activity(frame));

        return true;
    }

    Logger::warning("No paired host matched forwarder launch host={} ip={}",
                    request.mac, request.ip);
    return false;
}

} // namespace

bool canStartApp(int argc, char** argv) {
#ifdef __SWITCH__
    AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication) {
        auto dialog = new Dialog("error/applet_not_supported"_i18n);
        dialog->addButton("common/close"_i18n, [] {});
        dialog->open();
        return false;
    }
#endif
    return true;
}

void registerDeepLinkHandler() {
    Application::registerUrlOpenHandler(std::string(DEEP_LINK_SCHEME), [](const std::string& url) {
        return startFromUrl(url, true);
    });
}

bool startFromArgs(int argc, char** argv) {
    if (!canStartApp(argc, argv)) return true;

    if (argc <= 1) return false;

    Application::enableDebuggingView(true);
    return startFromLaunchRequest(parseArgv(argc, argv), false);
}

bool startFromUrl(const std::string& url, bool resetActivityStack) {
    if (!isDeepLinkUrl(url)) {
        return false;
    }

    Application::enableDebuggingView(true);
    return startFromLaunchRequest(parseDeepLinkUrl(url), resetActivityStack);
}
