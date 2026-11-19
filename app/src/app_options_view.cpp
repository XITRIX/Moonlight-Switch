//
//  app_options_view.cpp
//  Moonlight
//

#include "app_options_view.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace brls;
using namespace brls::literals;

namespace {
std::string bitrateName(int bitrate) {
    if (bitrate == 0)
        return "settings/bitrate_auto"_i18n;

    std::stringstream stream;
    stream << std::fixed << std::setprecision(1) << bitrate / 1000.0f;
    return stream.str() + " Mbps";
}

std::vector<int> bitrates() {
    std::vector<int> values = {0};
    for (int mbps : {5, 10, 20, 30, 50, 80, 100, 150}) {
        if (mbps * 1000 <= Settings::max_bitrate())
            values.push_back(mbps * 1000);
    }
    return values;
}

template <typename T>
int selectionOf(const std::vector<T>& values, const std::optional<T>& value) {
    if (!value)
        return 0;
    auto it = std::find(values.begin(), values.end(), *value);
    return it == values.end() ? 0 : int(it - values.begin()) + 1;
}

template <typename T>
std::optional<T> valueAt(const std::vector<T>& values, int index) {
    if (index < 0)
        return std::nullopt;
    return values[index];
}
}

AppOptionsView::AppOptionsView(const Host& host, const AppInfo& app)
    : host(host), appId(app.app_id),
      options(Settings::instance().app_options(host, app.app_id)) {
    getAppletFrameItem()->title = "app_list/options"_i18n;
    setAxis(Axis::COLUMN);
    setPaddingLeft(40);
    setPaddingRight(40);

    auto* header = new Header();
    header->setTitle(brls::getStr("app_list/options_note", app.name));
    addView(header);

    const auto& settings = Settings::instance();

    const auto resolutions = Settings::resolutions();
    std::vector<std::string> resolutionNames;
    for (int value : resolutions)
        resolutionNames.push_back(getResolutionName(value));
    addRow("settings/resolution"_i18n, getResolutionName(settings.resolution()),
           resolutionNames, selectionOf(resolutions, options.resolution),
           [this, resolutions](int index) { options.resolution = valueAt(resolutions, index); });

    const auto fpsValues = Settings::fps_values();
    std::vector<std::string> fpsNames;
    for (int value : fpsValues)
        fpsNames.push_back(std::to_string(value));
    addRow("settings/fps"_i18n, std::to_string(settings.fps()),
           fpsNames, selectionOf(fpsValues, options.fps),
           [this, fpsValues](int index) { options.fps = valueAt(fpsValues, index); });

    const auto bitrateValues = bitrates();
    std::vector<std::string> bitrateNames;
    for (int value : bitrateValues)
        bitrateNames.push_back(bitrateName(value));
    addRow("settings/video_bitrate"_i18n,
           bitrateName(settings.bitrate_auto() ? 0 : settings.bitrate()),
           bitrateNames, selectionOf(bitrateValues, options.bitrate),
           [this, bitrateValues](int index) { options.bitrate = valueAt(bitrateValues, index); });

    const auto codecs = Settings::video_codecs();
    std::vector<std::string> codecNames;
    for (VideoCodec codec : codecs)
        codecNames.push_back(getVideoCodecName(codec));
    addRow("settings/video_codec"_i18n, getVideoCodecName(settings.video_codec()),
           codecNames, selectionOf(codecs, options.codec),
           [this, codecs](int index) { options.codec = valueAt(codecs, index); });

    registerAction("app_list/options_reset"_i18n, BUTTON_Y, [this](View*) {
        reset();
        return true;
    });
}

void AppOptionsView::addRow(const std::string& title, const std::string& global,
                            std::vector<std::string> names, int selected,
                            std::function<void(int)> apply) {
    names.insert(names.begin(), brls::getStr("app_list/options_default", global));

    auto* cell = new SelectorCell();
    auto mark = [cell](int selected) {
        cell->setDetailTextColor(Application::getTheme()[selected == 0
            ? "brls/text_disabled" : "brls/list/listItem_value_color"]);
    };
    cell->init(title, names, selected, [this, apply, mark](int selected) {
        apply(selected - 1);
        mark(selected);
        Settings::instance().set_app_options(host, appId, options);
    });
    mark(selected);
    addView(cell);
    cells.push_back(cell);
}

void AppOptionsView::reset() {
    options = {};
    Settings::instance().set_app_options(host, appId, options);
    for (auto* cell : cells) {
        cell->setSelection(0, true);
        cell->setDetailTextColor(Application::getTheme()["brls/text_disabled"]);
    }
}
