//
//  two_finger_scroll_recognizer.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 15.11.2021.
//

#pragma once

#include <borealis.hpp>

struct TwoFingerScrollState {
    brls::GestureState state;
    brls::Point delta;
};

typedef brls::Event<TwoFingerScrollState> ScrollGestureEvent;
class TwoFingerScrollGestureRecognizer : public brls::GestureRecognizer {
  public:
    TwoFingerScrollGestureRecognizer(ScrollGestureEvent::Callback respond);
    brls::GestureState recognitionLoop(brls::TouchState touch,
                                       brls::MouseState mouse, brls::View* view,
                                       brls::Sound* soundToPlay) override;

    void forceReset();
  private:
    int fingers = 0;
    std::map<int, brls::Point> fingersStartPoints;
    std::map<int, brls::Point> fingersCurrentPoints;
    ScrollGestureEvent event;
    brls::Point startPoint;
};
