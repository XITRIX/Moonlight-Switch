#pragma once

#include "Settings.hpp"
#include "views/boolean_slider_cell.hpp"
#include <borealis.hpp>
#include <vector>

class StreamProfilesView : public brls::Box {
  public:
    explicit StreamProfilesView(const Host& host);
    ~StreamProfilesView() override;

    BRLS_BIND(brls::DetailCell, localWifi, "local_wifi");
    BRLS_BIND(brls::DetailCell, localEthernet, "local_ethernet");
    BRLS_BIND(brls::DetailCell, remote, "remote");
    BRLS_BIND(brls::Header, summaryHeader, "summary");
    BRLS_BIND(brls::BooleanCell, enabled, "enabled");
    BRLS_BIND(brls::SelectorCell, resolution, "resolution");
    BRLS_BIND(brls::SelectorCell, resolutionScale, "resolution_scale");
    BRLS_BIND(brls::SelectorCell, fps, "fps");
    BRLS_BIND(brls::SelectorCell, codec, "codec");
    BRLS_BIND(brls::BooleanCell, requestHdr, "request_hdr");
    BRLS_BIND(brls::SelectorCell, framePacing, "frame_pacing");
    BRLS_BIND(brls::Header, bitrateHeader, "bitrate_header");
    BRLS_BIND(brls::Slider, bitrateSlider, "bitrate_slider");
    BRLS_BIND(brls::Header, imageAdjustmentsHeader, "image_adjustments_header");
    BRLS_BIND(brls::BooleanCell, upscaling, "upscaling");
    BRLS_BIND(BooleanSliderCell, dithering, "dithering");
    BRLS_BIND(BooleanSliderCell, rcas, "rcas");

  private:
    Host host;
    StreamProfileContext context = StreamProfileContext::LocalWifi;
    StreamProfile profile;
    bool refreshing = false;
    std::vector<VideoCodec> codecs;

    void loadContext(StreamProfileContext nextContext);
    void saveProfile();
    void refreshControls();
    void refreshVisibility();
    void refreshSummary();
    void refreshContextSummaries();
};
