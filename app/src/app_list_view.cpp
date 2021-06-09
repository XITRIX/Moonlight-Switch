//
//  app_list_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "app_list_view.hpp"
#include "streaming_view.hpp"
#include "app_cell.hpp"
#include "helper.hpp"

AppListView::AppListView(Host host) :
    host(host)
{
    this->inflateFromXMLRes("xml/views/app_list_view.xml");
    container->setHideHighlight(true);
    gridView = new GridView();
    container->addView(gridView);
    loader = new LoadingOverlay(this);
    
    registerAction("Reload app list", BUTTON_X, [this](View* view) {
        this->updateAppList();
        return true;
    });
}

void AppListView::terninateApp()
{
    if (loading)
        return;
    
    Dialog* dialog = new Dialog("Terminate " + currentApp->name + "\n\nAll unsaved progress could be lost");
    
    dialog->addButton("Cancel", [] { });
    
    dialog->addButton("Terminate", [dialog, this] {
        if (loading)
            return;
        
        loading = true;
        gridView->clearViews();
        Application::giveFocus(this);
        loader->setHidden(false);
        unregisterAction(terminateIdentifier);
        
        ASYNC_RETAIN
        GameStreamClient::instance().quit(host.address, [ASYNC_TOKEN](GSResult<bool> result) {
            ASYNC_RELEASE
            
            loading = false;
            loader->setHidden(true);
            
            if (!result.isSuccess())
                showError(result.error(), [this] {});
            
            updateAppList();
        });
    });
    
    dialog->open();
}

void AppListView::updateAppList()
{
    if (loading)
        return;
    
    loading = true;
    
    unregisterAction(terminateIdentifier);
    gridView->clearViews();
    Application::giveFocus(this);
    loader->setHidden(false);
    currentApp = std::nullopt;
    
    setTitle(host.hostname);
    
    ASYNC_RETAIN
    GameStreamClient::instance().connect(host.address, [ASYNC_TOKEN](GSResult<SERVER_DATA> result) {
        ASYNC_RELEASE
        
        if (result.isSuccess())
        {
            int currentGame = result.value().currentGame;
            
            ASYNC_RETAIN
            GameStreamClient::instance().applist(host.address, [ASYNC_TOKEN, currentGame](GSResult<AppInfoList> result) {
                ASYNC_RELEASE
                
                loading = false;
                loader->setHidden(true);
                
                if (result.isSuccess())
                {
                    for (AppInfo app : result.value())
                    {
                        if (app.app_id == currentGame)
                            setCurrentApp(app);
                        
                        AppCell* cell = new AppCell(host, app, currentGame);
                        gridView->addView(cell);
                    }
                    Application::giveFocus(this);
                }
                else
                {
                    showError(result.error(), [this]
                    {
                        this->dismiss();
                    });
                }
            });
        }
        else
        {
            showError(result.error(), [this]
            {
                this->dismiss();
            });
        }
    });
}

void AppListView::setCurrentApp(AppInfo app, bool update)
{
    this->currentApp = app;
    setTitle(host.hostname + " - Running " + app.name);
    
    terminateIdentifier = registerAction("Terminate current app", BUTTON_BACK, [this](View* view) {
        this->terninateApp();
        return true;
    });
}

void AppListView::willAppear(bool resetState)
{
    Box::willAppear(resetState);
    updateAppList();
}

void AppListView::onLayout()
{
    Box::onLayout();
    
    if (loader)
        loader->layout();
}
