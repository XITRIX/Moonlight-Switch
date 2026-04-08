//
//  add_host_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "add_host_tab.hpp"
#include "DiscoverManager.hpp"
#include "helper.hpp"
#include "main_tabs_view.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>

#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
extern void darwin_mdns_start(ServerCallback<std::vector<Host>>& callback);
extern void darwin_mdns_stop();
#endif

namespace {
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
        std::vector<Host> hosts = hostsRes.value();
        for (const Host& host : hosts) {
            const auto displayAddress = host.preferred_address();
            if (searchBoxIpExists(displayAddress))
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
    } else {
        loader->setVisibility(brls::Visibility::GONE);
        searchHeader->setTitle("add_host/search"_i18n + " - " +
                               hostsRes.error());
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
    ASYNC_RETAIN
#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS) || defined(PLATFORM_VISIONOS)
    darwin_mdns_start(
#else
    GameStreamClient::find_hosts(
#endif
        [ASYNC_TOKEN](const GSResult<std::vector<Host>>& result) {
            ASYNC_RELEASE

            if (result.isSuccess()) {
                std::vector<Host> hosts = result.value();

                searchBox->clearViews();
                for (const Host& host : hosts) {
                    auto hostButton = new brls::DetailCell();
                    hostButton->setText(host.hostname);
                    hostButton->setDetailText(host.preferred_address());
                    hostButton->setDetailTextColor(
                        brls::Application::getTheme()["brls/text_disabled"]);
                    hostButton->registerClickAction([this, host](View* view) {
                        connectHost(host);
                        return true;
                    });
                    searchBox->addView(hostButton);
                }
//                loader->setVisibility(brls::Visibility::GONE);
            } else {
                showError(result.error(), [] {});
            }
        });
#endif
}

void AddHostTab::stopSearchHost() {
#ifdef PLATFORM_IOS
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

                    auto pin = fmt::format("{}{}{}{}", (int)rand() % 10, (int)rand() % 10,
                            (int)rand() % 10, (int)rand() % 10);

                    brls::Dialog* dialog = createLoadingDialog(
                        "add_host/pair_prefix"_i18n + pin +
                        "add_host/pair_postfix"_i18n);
                    dialog->setCancelable(false);
                    dialog->open();

                    ASYNC_RETAIN
                    GameStreamClient::instance().pair(
                        pairedHost, pin,
                        [ASYNC_TOKEN, pairedHost, dialog](const GSResult<bool>& result) {
                            ASYNC_RELEASE
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
