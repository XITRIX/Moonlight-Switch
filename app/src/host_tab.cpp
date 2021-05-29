//
//  host_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "host_tab.hpp"
#include "app_list_view.hpp"
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
        }
        else
        {
            header->setTitle("Status: Unavailabe");
        }
        
    });
    
    connect->registerClickAction([this](View* view) {
        AppListView* appList = new AppListView(this->host);
        this->present(appList);
        return true;
    });
}
