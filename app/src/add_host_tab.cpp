//
//  add_host_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "add_host_tab.hpp"
#include "CryptoManager.hpp"
#include "DiscoverManager.hpp"
#include "helper.hpp"
#include "http.h"
#include "main_tabs_view.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include <algorithm>
#include <cstring>

#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
extern void darwin_mdns_start(ServerCallback<std::vector<Host>>& callback);
extern void darwin_mdns_stop();
#endif

namespace {
std::string random_pin() {
    // Drop the top of the range so that every PIN is equally likely
    const uint32_t limit = UINT32_MAX - UINT32_MAX % 10000;
    while (true) {
        Data bytes = CryptoManager::random_bytes(sizeof(uint32_t));
        if (bytes.size() != sizeof(uint32_t)) {
            return "";
        }

        uint32_t value;
        memcpy(&value, bytes.bytes(), sizeof(value));
        if (value < limit) {
            return fmt::format("{:04}", value % 10000);
        }
    }
}

std::string pin_expires_text(brls::Time deadline) {
    brls::Time left = std::max<brls::Time>(deadline - brls::getCPUTimeUsec(), 0);
    int seconds = (int)((left + 999999) / 1000000);
    return brls::getStr("add_host/pair_expires",
                        fmt::format("{}:{:02}", seconds / 60, seconds % 60));
}

std::string strip_ipv4_port(const std::string& address) {
    const auto firstColon = address.find(':');
    if (firstColon == std::string::npos) {
        return address;
    }

    if (address.find(':', firstColon + 1) != std::string::npos) {
        return address;
    }

    return address.substr(0, firstColon);
}

bool is_private_ipv4(const in_addr& address) {
    const uint32_t value = ntohl(address.s_addr);
    const uint8_t a = (value >> 24) & 0xFF;
    const uint8_t b = (value >> 16) & 0xFF;

    return a == 10 || a == 127 || (a == 169 && b == 254) ||
           (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31);
}

bool should_store_manual_address_as_remote(const std::string& address) {
    const auto hostPart = strip_ipv4_port(address);
    if (hostPart.empty()) {
        return false;
    }

    in_addr parsed{};
    if (inet_pton(AF_INET, hostPart.c_str(), &parsed) != 1) {
        return false;
    }

    return !is_private_ipv4(parsed);
}
}

AddHostTab::AddHostTab() {
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/add_host.xml");

    hostIP->init("add_host/host_ip"_i18n, "");
    hostIP->setPlaceholder("192.168.1.109:47989");
    hostIP->setHint("192.168.1.109:47989");

    connect->setText("add_host/connect"_i18n);
    connect->registerClickAction([this](View* view) {
        Host host;
        const auto inputAddress = hostIP->getValue();
        if (should_store_manual_address_as_remote(inputAddress)) {
            host.remoteAddress = inputAddress;
        } else {
            host.address = inputAddress;
        }
        connectHost(host);
        return true;
    });

    if (GameStreamClient::can_find_host())
        findHost();
    else {
        searchHeader->setTitle("add_host/search_error"_i18n);
        loader->setVisibility(brls::Visibility::GONE);
    }

    registerAction("add_host/search_refresh"_i18n, ControllerButton::BUTTON_X,
                   [this](View* view) {
#ifdef MULTICAST_DISABLED
                       DiscoverManager::instance().reset();
#endif
                       findHost();
                       return true;
                   });
    setActionAvailable(BUTTON_X, GameStreamClient::can_find_host());
}

void AddHostTab::fillSearchBox(const GSResult<std::vector<Host>>& hostsRes) {
    loader->setVisibility(DiscoverManager::instance().isPaused()
                              ? brls::Visibility::GONE
                              : brls::Visibility::VISIBLE);

    if (hostsRes.isSuccess()) {
        appendSearchHosts(hostsRes.value());
    } else {
        loader->setVisibility(brls::Visibility::GONE);
        searchHeader->setTitle("add_host/search"_i18n + " - " +
                               hostsRes.error());
    }
}

void AddHostTab::appendSearchHosts(const std::vector<Host>& hosts) {
    for (const Host& host : hosts) {
        const auto displayAddress = host.preferred_address();
        if (displayAddress.empty() || searchBoxIpExists(displayAddress))
            continue;

        auto hostButton = new brls::DetailCell();
        hostButton->setText(host.hostname);
        hostButton->setDetailText(displayAddress);
        hostButton->setDetailTextColor(
            brls::Application::getTheme()["brls/text_disabled"]);
        hostButton->registerClickAction([this, host](View* view) {
            connectHost(host);
            return true;
        });
        searchBox->addView(hostButton);
    }
}

bool AddHostTab::searchBoxIpExists(const std::string& ip) {
    return std::any_of(searchBox->getChildren().begin(), searchBox->getChildren().end(), [ip](View* child) {
        auto cell = dynamic_cast<DetailCell*>(child);
        return cell->detail->getFullText() == ip;
    });
}

void AddHostTab::findHost() {
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().start();
    fillSearchBox(DiscoverManager::instance().getHosts());
    DiscoverManager::instance().getHostsUpdateEvent()->unsubscribe(
        searchSubscription);
    searchSubscription =
        DiscoverManager::instance().getHostsUpdateEvent()->subscribe(
            [this](auto result) { fillSearchBox(result); });
#else
    stopSearchHost();
    const uint64_t generation = searchGeneration;
    searchBox->clearViews();
    searchHeader->setTitle("add_host/search"_i18n);
    loader->setVisibility(brls::Visibility::VISIBLE);
    ASYNC_RETAIN
#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
    darwin_mdns_start(
#else
    GameStreamClient::find_hosts(
#endif
        [ASYNC_TOKEN, generation](const GSResult<std::vector<Host>>& result) {
            ASYNC_RELEASE

            if (generation != searchGeneration) {
                return;
            }

            if (result.isSuccess()) {
                appendSearchHosts(result.value());
            } else {
                loader->setVisibility(brls::Visibility::GONE);
                showError(result.error(), [] {});
            }
        });
#endif
}

void AddHostTab::stopSearchHost() {
    searchGeneration++;
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().pause();
#elif defined(PLATFORM_IOS)
#elif defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
#else
    GameStreamClient::cancel_find_hosts();
#endif
}

void AddHostTab::connectHost(const Host& host) {
    pauseSearching();

    Dialog* loaderView = createLoadingDialog("add_host/try_connect"_i18n);
    loaderView->open();

    GameStreamClient::instance().connect(
        host, [this, loaderView, host](const GSResult<SERVER_DATA>& result) {
            loaderView->close([this, result, host] {
                if (result.isSuccess()) {
                    Host pairedHost = host;
                    pairedHost.hostname = result.value().hostname;
                    pairedHost.mac = result.value().mac;

                    if (result.value().paired) {
                        showAlert("add_host/paired_error"_i18n, [pairedHost] {
                            Settings::instance().add_host(pairedHost);
                            MainTabs::getInstanse()->refillTabs();
                        });

                        return;
                    }

                    auto pin = random_pin();
                    if (pin.empty()) {
                        showError("error/unknown_error"_i18n,
                                  [] { AddHostTab::startSearching(); });
                        return;
                    }

                    auto cancelled = std::make_shared<bool>(false);
                    auto countdown = std::make_shared<brls::RepeatingTimer>();
                    brls::Dialog* dialog = createLoadingDialog(
                        "add_host/pair_prefix"_i18n + pin +
                            "add_host/pair_postfix"_i18n,
                        [cancelled, countdown] {
                            *cancelled = true;
                            countdown->stop();
                            GameStreamClient::instance().cancel_pair();
                            AddHostTab::startSearching();
                        });

                    auto* label = (brls::Label*)dialog->getView("loading_text");
                    std::string text = label->getFullText();
                    brls::Time deadline = brls::getCPUTimeUsec() +
                        (brls::Time)HTTPRequestTimeoutPairing * 1000000;
                    label->setText(text + "\n" + pin_expires_text(deadline));
                    countdown->setCallback([label, text, deadline] {
                        label->setText(text + "\n" + pin_expires_text(deadline));
                    });
                    countdown->start(1000);
                    dialog->open();

                    ASYNC_RETAIN
                    GameStreamClient::instance().pair(
                        pairedHost, pin,
                        [ASYNC_TOKEN, pairedHost, dialog, cancelled,
                         countdown](const GSResult<bool>& result) {
                            ASYNC_RELEASE
                            if (*cancelled) {
                                return;
                            }
                            countdown->stop();
                            dialog->dismiss([result, pairedHost] {
                                if (result.isSuccess()) {
                                    Settings::instance().add_host(pairedHost);
                                    MainTabs::getInstanse()->refillTabs();
                                    AddHostTab::startSearching();
                                } else {
                                    showError(result.error(), [] {
                                        AddHostTab::startSearching();
                                    });
                                }
                            });
                        });
                } else {
                    showError(result.error(),
                              [] { AddHostTab::startSearching(); });
                }
            });
        });
}

void AddHostTab::pauseSearching() {
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().pause();
#endif
}

void AddHostTab::startSearching() {
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().start();
#endif
}

AddHostTab::~AddHostTab() {
    stopSearchHost();
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().pause();
    DiscoverManager::instance().getHostsUpdateEvent()->unsubscribe(
        searchSubscription);
#elif defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
    darwin_mdns_stop();
#endif
}

brls::View* AddHostTab::create() {
    // Called by the XML engine to create a new AddHostTab
    return new AddHostTab();
}
