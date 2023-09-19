//
//  fingers_gesture_recognizer.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.10.2021.
//

#include "fingers_gesture_recognizer.hpp"

using namespace brls;

FingersGestureRecognizer::FingersGestureRecognizer(
    int fingers, FingersGestureEvent::Callback respond)
    : fingers(fingers) {
    event.subscribe(respond);
}

GestureState FingersGestureRecognizer::recognitionLoop(TouchState touch,
                                                       MouseState mouse,
                                                       View* view,
                                                       Sound* soundToPlay) {
    if (touch.phase == brls::TouchPhase::START) {
        fingersCounter++;
        if (fingersCounter == fingers) {
            event.fire();
            return brls::GestureState::END;
        }
    } else if (touch.phase == brls::TouchPhase::END) {
        fingersCounter--;
    }

    return brls::GestureState::UNSURE;
}
