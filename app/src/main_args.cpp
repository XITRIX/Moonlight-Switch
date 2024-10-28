//
//  main_args.cpp
//  Moonlight
//
//  Created by XITRIX on 22.01.2024.
//

#include "main_args.hpp"
#include <borealis.hpp>
#include "streaming_view.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

using namespace brls;

bool canStartApp(int argc, char** argv) {
#ifdef __SWITCH__
    AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication) {
        auto dialog = new Dialog("error/applet_not_supported"_i18n);
        dialog->addButton("common/close"_i18n, [] {});
        dialog->open();
        return false;
    }
#endif
    return true;
}

bool startFromArgs(int argc, char** argv) {
    if (!canStartApp(argc, argv)) return true;

    if (argc <= 1) return false;

    std::string args_pref[3] = {"--host=", "--appid=", "--appname="};
    std::string args[3];

    for (int i = 1; i < argc; i++) {
        for (int j = 0; j < 3; j++) {
            auto arg = std::string(argv[i]);

            if (arg.rfind(args_pref[j], 0) == 0) {
                args[j] = arg.substr(args_pref[j].length());
            }
        }
    }

    Application::enableDebuggingView(true);

    Logger::debug("Host {}", args[0]);
    Logger::debug("Id {}", args[1]);
    Logger::debug("Name {}", args[2]);

    if (args_pref[0].empty() || args_pref[1].empty()) { return false; }

    auto hosts = Settings::instance().hosts();
    if (auto it = std::find_if(hosts.begin(), hosts.end(), [args](const Host& host) {
        return host.mac == args[0];
    }); it != std::end(hosts)) {
        Host host = *it;
        AppInfo info { args[2], stoi(args[1]) };

        auto* frame = new AppletFrame(new StreamingView(host, info));
        frame->setBackground(ViewBackground::NONE);
        frame->setHeaderVisibility(brls::Visibility::GONE);
        frame->setFooterVisibility(brls::Visibility::GONE);
        Application::pushActivity(new Activity(frame));

        return true;
    }

    return false;
}
