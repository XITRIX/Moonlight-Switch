//
//  boolean_slider_cell.cpp
//  Moonlight
//

#include "views/boolean_slider_cell.hpp"

BooleanSliderCell::BooleanSliderCell() {
    this->inflateFromXMLRes("xml/cells/boolean_slider_cell.xml");

    this->setHeight(brls::View::AUTO);
    toggle->setLineBottom(0);
}

void BooleanSliderCell::init(const std::string& title, bool isOn,
                             const std::function<void(bool)>& callback) {
    baseTitle = title;
    toggle->init(title, isOn, callback);
    updateTitle();
}

void BooleanSliderCell::setValueText(const std::string& text) {
    valueText = text;
    updateTitle();
}

void BooleanSliderCell::setSliderVisibility(brls::Visibility visibility) {
    slider->setVisibility(visibility);
}

void BooleanSliderCell::setProgress(float progress) {
    slider->setProgress(progress);
}

float BooleanSliderCell::getProgress() {
    return slider->getProgress();
}

void BooleanSliderCell::setStep(float step) {
    slider->setStep(step);
}

brls::Event<float>* BooleanSliderCell::getProgressEvent() {
    return slider->getProgressEvent();
}

void BooleanSliderCell::updateTitle() {
    if (valueText.empty()) {
        toggle->setText(baseTitle);
        return;
    }

    toggle->setText(baseTitle + " - " + valueText);
}

brls::View* BooleanSliderCell::create() {
    return new BooleanSliderCell();
}