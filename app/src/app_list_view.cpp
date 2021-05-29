//
//  app_list_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "app_list_view.hpp"
#include "streaming_view.hpp"
#include "GameStreamClient.hpp"
#include "helper.hpp"

AppListView::AppListView(Host host) :
    host(host)
{
    this->inflateFromXMLRes("xml/views/app_list_view.xml");
    
    setTitle(host.hostname);
    updateAppList();
}

void AppListView::updateAppList()
{
    container->clearViews();
    
    ASYNC_RETAIN
    GameStreamClient::instance().connect(host.address, [ASYNC_TOKEN](GSResult<SERVER_DATA> result) {
        ASYNC_RELEASE
        
        if (result.isSuccess())
        {
            ASYNC_RETAIN
            GameStreamClient::instance().applist(host.address, [ASYNC_TOKEN](GSResult<AppInfoList> result) {
                ASYNC_RELEASE
                
                if (result.isSuccess())
                {
                    for (AppInfo app : result.value())
                    {
                        DetailCell* cell = new DetailCell();
                        cell->setText(app.name);
                        
                        cell->registerClickAction([this, app](View* view) {
                            AppletFrame* frame = new AppletFrame(new StreamingView(host, app));
                            frame->setHeaderVisibility(brls::Visibility::GONE);
                            frame->setFooterVisibility(brls::Visibility::GONE);
                            Application::pushActivity(new Activity(frame));
                            return true;
                        });
                        
                        container->addView(cell);
                    }
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
