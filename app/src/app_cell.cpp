//
//  app_cell.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.06.2021.
//

#include "app_cell.hpp"
#include "BoxArtManager.hpp"
#include "Settings.hpp"
#include "streaming_view.hpp"

static const int BOXART_FETCH_ATTEMPTS = 3;
static const long BOXART_RETRY_DELAY_MS = 2000;

AppCell::AppCell(const Host& host, const AppInfo& app, int currentApp) {
    this->inflateFromXMLRes("xml/cells/app_cell.xml");
    this->setFavorite(false);

    title->setText(app.name);
    title->setTextColor(nvgRGB(255, 255, 255));

    bool isUnactive = currentApp != 0 && currentApp != app.app_id;
    unactiveLayer->setVisibility(isUnactive ? Visibility::VISIBLE
                                            : Visibility::GONE);
    currentAppImage->setVisibility(
        currentApp == app.app_id ? Visibility::VISIBLE : Visibility::GONE);

    this->addGestureRecognizer(new TapGestureRecognizer(this));
    this->registerClickAction([host, app](View* view) {
        auto* frame = new AppletFrame(new StreamingView(host, app));
        frame->setBackground(ViewBackground::NONE);
        frame->setHeaderVisibility(brls::Visibility::GONE);
        frame->setFooterVisibility(brls::Visibility::GONE);
        Application::pushActivity(new Activity(frame));
        return true;
    });
    this->setActionAvailable(BUTTON_A, !isUnactive);

    if (BoxArtManager::instance().has_boxart(app.app_id))
        image->setImageFromFile(
            BoxArtManager::get_texture_path(app.app_id));
    else
        fetchBoxArt(host, app.app_id, 1);
}

void AppCell::fetchBoxArt(const Host& host, int appId, int attempt) {
    ASYNC_RETAIN
    GameStreamClient::instance().app_boxart(
        host, appId, [ASYNC_TOKEN, host, appId, attempt](auto result) {
            ASYNC_RELEASE

            if (result.isSuccess()) {
                BoxArtManager::instance().set_data(result.value(), appId);
                image->setImageFromFile(
                    BoxArtManager::get_texture_path(appId));
            } else if (attempt < BOXART_FETCH_ATTEMPTS) {
                retryBoxArt(host, appId, attempt);
            }
        });
}

void AppCell::retryBoxArt(const Host& host, int appId, int attempt) {
    ASYNC_RETAIN
    brls::delay(BOXART_RETRY_DELAY_MS * attempt,
                [ASYNC_TOKEN, host, appId, attempt] {
                    ASYNC_RELEASE
                    fetchBoxArt(host, appId, attempt + 1);
                });
}

void AppCell::setFavorite(bool favorite) {
    favoriteAppImage->setVisibility(favorite ? Visibility::VISIBLE
                                             : Visibility::GONE);
}
