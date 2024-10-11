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

#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS)
extern void darwin_mdns_start(ServerCallback<std::vector<Host>>& callback);
extern void darwin_mdns_stop();
#endif

AddHostTab::AddHostTab() {
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/add_host.xml");

    hostIP->init("add_host/host_ip"_i18n, "");
    hostIP->setPlaceholder("192.168.1.109");
    hostIP->setHint("192.168.1.109");

    connect->setText("add_host/connect"_i18n);
    connect->registerClickAction([this](View* view) {
        connectHost(hostIP->getValue());
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
            if (searchBoxIpExists(host.address))
                continue;

            auto hostButton = new brls::DetailCell();
            hostButton->setText(host.hostname);
            hostButton->setDetailText(host.address);
            hostButton->setDetailTextColor(
                brls::Application::getTheme()["brls/text_disabled"]);
            hostButton->registerClickAction([this, host](View* view) {
                connectHost(host.address);
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
    ASYNC_RETAIN
#if defined(PLATFORM_IOS) || defined(PLATFORM_TVOS)
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
                    hostButton->setDetailText(host.address);
                    hostButton->setDetailTextColor(
                        brls::Application::getTheme()["brls/text_disabled"]);
                    hostButton->registerClickAction([this, host](View* view) {
                        connectHost(host.address);
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
    
#endif
}

void AddHostTab::connectHost(const std::string& address) {
    pauseSearching();

    Dialog* loaderView = createLoadingDialog("add_host/try_connect"_i18n);
    loaderView->open();

    GameStreamClient::instance().connect(
        address, [this, loaderView](const GSResult<SERVER_DATA>& result) {
                loaderView->close([this, result] {
                if (result.isSuccess()) {
                    Host host{.address = result.value().address,
                              .hostname = result.value().hostname,
                              .mac = result.value().mac};

                    if (result.value().paired) {
                        showAlert("add_host/paired_error"_i18n, [host] {
                            Settings::instance().add_host(host);
                            MainTabs::getInstanse()->refillTabs();
                        });

                        return;
                    }

                    char pin[5];
                    sprintf(pin, "%d%d%d%d", (int)rand() % 10, (int)rand() % 10,
                            (int)rand() % 10, (int)rand() % 10);

                    brls::Dialog* dialog = createLoadingDialog(
                        "add_host/pair_prefix"_i18n + std::string(pin) +
                        "add_host/pair_postfix"_i18n);
                    dialog->setCancelable(false);
                    dialog->open();

                    ASYNC_RETAIN
                    GameStreamClient::instance().pair(
                        result.value().address, pin,
                        [ASYNC_TOKEN, host, dialog](const GSResult<bool>& result) {
                            ASYNC_RELEASE
                            dialog->dismiss([result, host] {
                                if (result.isSuccess()) {
                                    Settings::instance().add_host(host);
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
#ifdef MULTICAST_DISABLED
    DiscoverManager::instance().pause();
    DiscoverManager::instance().getHostsUpdateEvent()->unsubscribe(
        searchSubscription);
#elif defined(PLATFORM_IOS) || defined(PLATFORM_TVOS)
    darwin_mdns_stop();
#endif
}

brls::View* AddHostTab::create() {
    // Called by the XML engine to create a new AddHostTab
    return new AddHostTab();
}
