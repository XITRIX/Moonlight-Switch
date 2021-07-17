//
//  host_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "host_tab.hpp"
#include "app_list_view.hpp"
#include "hosts_tabs_view.hpp"
#include "GameStreamClient.hpp"
#include "helper.hpp"

using namespace brls::literals;

HostTab::HostTab(Host host) :
    host(host)
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/host.xml");
    
    remove->setText("main/host/remove"_i18n);
    remove->title->setTextColor(RGB(229, 57, 53));
    
    reloadHost();
    
    connect->registerClickAction([this](View* view) {
        switch (state) {
            case AVAILABLE:
                this->present(new AppListView(this->host));
                break;
            case UNAVAILABLE:
                if (GameStreamClient::instance().can_wake_up_host(this->host))
                {
                    Dialog* loader = createLoadingDialog("main/host/wake_up_message"_i18n);
                    loader->open();
                    
                    GameStreamClient::instance().wake_up_host(this->host, [this, loader](GSResult<bool> result) {
                        loader->close([this, result] {
                            if (result.isSuccess()) {
                                reloadHost();
                            }
                            else
                            {
                                showError("main/host/wake_up_error"_i18n);
                            }
                        });
                    });
                }
                break;
            case FETCHING:
                break;
        }
        return true;
    });
    
    remove->registerClickAction([this, host](View* view) {
        Dialog* dialog = new Dialog("main/host/remove_message"_i18n);
        dialog->addButton("main/host/cancel"_i18n, [] {});
        dialog->addButton("main/host/remove"_i18n, [host]
        {
            Settings::instance().remove_host(host);
            HostsTabs::getInstanse()->refillTabs();
        });
        dialog->open();
        
        return true;
    });
}

void HostTab::reloadHost() {
    state = FETCHING;
    header->setTitle("main/host/status"_i18n + ": " + "main/host/fetching"_i18n);
    connect->setText("main/host/wait"_i18n);
    
    ASYNC_RETAIN
    GameStreamClient::instance().connect(host.address, [ASYNC_TOKEN](GSResult<SERVER_DATA> result){
        ASYNC_RELEASE
        
        if (result.isSuccess())
        {
            header->setTitle("main/host/status"_i18n + ": " + "main/host/ready"_i18n);
            connect->setText("main/host/connect"_i18n);
            state = AVAILABLE;
        }
        else
        {
            header->setTitle("main/host/status"_i18n + ": " + "main/host/unable"_i18n);
            connect->setText("main/host/wake_up"_i18n);
            state = UNAVAILABLE;
        }
        
    });
}
