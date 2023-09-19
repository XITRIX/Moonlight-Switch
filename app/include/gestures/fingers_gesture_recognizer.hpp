//
//  fingers_gesture_recognizer.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.10.2021.
//

#pragma once

#include <borealis.hpp>

typedef brls::Event<> FingersGestureEvent;
class FingersGestureRecognizer : public brls::GestureRecognizer {
  public:
    FingersGestureRecognizer(int fingers,
                             FingersGestureEvent::Callback respond);
    brls::GestureState recognitionLoop(brls::TouchState touch,
                                       brls::MouseState mouse, brls::View* view,
                                       brls::Sound* soundToPlay) override;

  private:
    int fingers;
    int fingersCounter = 0;
    FingersGestureEvent event;
};
