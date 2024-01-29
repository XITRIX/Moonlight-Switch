#include "about_tab.hpp"
#include <fmt/core.h>

#include <utility>

#ifdef __SWITCH__
#include <switch.h>
#endif

using namespace brls;

void openWebpage(std::string url) {
    Application::getPlatform()->openBrowser(std::move(url));
}

AboutTab::AboutTab() {
    this->inflateFromXMLRes("xml/tabs/about.xml");

    ThemeVariant variant = Application::getThemeVariant();
    std::string themePart =
        variant == brls::ThemeVariant::DARK ? "_dark" : "_light";

    std::string subtitle = fmt::format("about/version"_i18n, APP_VERSION);
    versionLabel->setSubtitle(subtitle);

    std::string githubLink = "https://github.com/XITRIX/Moonlight-Switch";
    github->addGestureRecognizer(new TapGestureRecognizer(github));
    github->title->setText("about/link_github"_i18n);
    github->subtitle->setText(githubLink);
    github->image->setImageFromRes("img/links/github" + themePart + ".png");
    github->registerClickAction([githubLink](View* view) {
        openWebpage(githubLink);
        return true;
    });

    std::string patreonLink = "https://www.patreon.com/xitrix";
    patreon->addGestureRecognizer(new TapGestureRecognizer(patreon));
    patreon->title->setText("about/link_patreon"_i18n);
    patreon->subtitle->setText(patreonLink);
    patreon->image->setImageFromRes("img/links/patreon.png");
    patreon->registerClickAction([patreonLink](View* view) {
        openWebpage(patreonLink);
        return true;
    });

    std::string gbatempLink =
        "https://gbatemp.net/threads/"
        "moonlight-switch-nvidia-game-stream-client.591408/";
    gbatemp->addGestureRecognizer(new TapGestureRecognizer(gbatemp));
    gbatemp->title->setText("about/link_gbatemp"_i18n);
    gbatemp->subtitle->setText(gbatempLink);
    gbatemp->image->setImageFromRes("img/links/gbatemp.png");
    gbatemp->registerClickAction([gbatempLink](View* view) {
        openWebpage(gbatempLink);
        return true;
    });
}

brls::View* AboutTab::create() { return new AboutTab(); }
