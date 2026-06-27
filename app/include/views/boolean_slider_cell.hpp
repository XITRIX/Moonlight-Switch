//
//  boolean_slider_cell.hpp
//  Moonlight
//

#pragma once

#include <borealis.hpp>

class BooleanSliderCell : public brls::RecyclerCell {
  public:
    BooleanSliderCell();

    void init(const std::string& title, bool isOn,
              const std::function<void(bool)>& callback);
    void setValueText(const std::string& text);
    void setSliderVisibility(brls::Visibility visibility);
    void setProgress(float progress);
    float getProgress();
    void setStep(float step);
    brls::Event<float>* getProgressEvent();

    static brls::View* create();

  private:
    void updateTitle();

    std::string baseTitle;
    std::string valueText;

    BRLS_BIND(brls::BooleanCell, toggle, "toggle");
    BRLS_BIND(brls::Slider, slider, "slider");
};