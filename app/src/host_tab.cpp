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

using namespace brls::literals;

HostTab::HostTab(Host host) :
    host(host)
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/host.xml");
    
    remove->setText("Remove");
    remove->title->setTextColor(RGB(229, 57, 53));
    
    header->setTitle("Status: Fetching...");
    connect->setText("Wake up");
    
    ASYNC_RETAIN
    GameStreamClient::instance().connect(host.address, [ASYNC_TOKEN](GSResult<SERVER_DATA> result){
        ASYNC_RELEASE
        
        if (result.isSuccess())
        {
            header->setTitle("Status: Ready");
            connect->setText("Connect");
            state = AVAILABLE;
        }
        else
        {
            header->setTitle("Status: Unavailabe");
            state = UNAVAILABLE;
        }
        
    });
    
    connect->registerClickAction([this](View* view) {
        switch (state) {
            case AVAILABLE:
                this->present(new AppListView(this->host));
                break;
            case UNAVAILABLE:
                break;
            case FETCHING:
                break;
        }
        return true;
    });
    
    remove->registerClickAction([this, host](View* view) {
        Dialog* dialog = new Dialog("Are you sure you want to remove this host?");
        dialog->addButton("Cancel", [] {});
        dialog->addButton("Remove", [host]
        {
            Settings::instance().remove_host(host);
            HostsTabs::getInstanse()->refillTabs();
        });
        dialog->open();
        
        return true;
    });
}
