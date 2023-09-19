//
//  click_gesture_recognizer.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.10.2021.
//

#pragma once

#include <borealis.hpp>

typedef brls::Event<brls::TapGestureStatus> ClickGestureEvent;
class ClickGestureRecognizer : public brls::GestureRecognizer {
  public:
    ClickGestureRecognizer(int fingersRequired,
                           ClickGestureEvent::Callback respond);
    brls::GestureState recognitionLoop(brls::TouchState touch,
                                       brls::MouseState mouse, brls::View* view,
                                       brls::Sound* soundToPlay) override;

  private:
    int fingers;
    std::map<int, brls::Point> fingersStartPoints;
    ClickGestureEvent event;
};
