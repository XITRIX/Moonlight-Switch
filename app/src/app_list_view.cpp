//
//  app_list_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "app_list_view.hpp"
#include "streaming_view.hpp"
#include "GameStreamClient.hpp"
#include "app_cell.hpp"
#include "helper.hpp"

AppListView::AppListView(Host host) :
    host(host)
{
    this->inflateFromXMLRes("xml/views/app_list_view.xml");
    container->setHideHighlight(true);
    setTitle(host.hostname);
    gridView = new GridView();
    container->addView(gridView);
    loader = new LoadingOverlay(this);
    updateAppList();
}

void AppListView::updateAppList()
{
    Application::giveFocus(this);
    gridView->clearViews();
    loader->setHidden(false);
    
    ASYNC_RETAIN
    GameStreamClient::instance().connect(host.address, [ASYNC_TOKEN](GSResult<SERVER_DATA> result) {
        ASYNC_RELEASE
        
        if (result.isSuccess())
        {
            ASYNC_RETAIN
            GameStreamClient::instance().applist(host.address, [ASYNC_TOKEN](GSResult<AppInfoList> result) {
                ASYNC_RELEASE
                
                loader->setHidden(true);
                
                if (result.isSuccess())
                {
                    for (AppInfo app : result.value())
                    {
                        AppCell* cell = new AppCell(host, app);
                        gridView->addView(cell);
                    }
                    Application::giveFocus(this);
                }
                else
                {
                    showError(this, result.error(), [this]
                    {
                        this->dismiss();
                    });
                }
            });
        }
        else
        {
            showError(this, result.error(), [this]
            {
                this->dismiss();
            });
        }
    });
}

void AppListView::onLayout()
{
    Box::onLayout();
    
    if (loader)
        loader->layout();
}
