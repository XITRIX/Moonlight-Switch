//
//  fingers_gesture_recognizer.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.10.2021.
//

#include "fingers_gesture_recognizer.hpp"

using namespace brls;

FingersGestureRecognizer::FingersGestureRecognizer(
    std::function<int(void)> getFingersNum, FingersGestureEvent::Callback respond)
    : getFingersNum(getFingersNum) {
    event.subscribe(respond);
}

GestureState FingersGestureRecognizer::recognitionLoop(TouchState touch,
                                                       MouseState mouse,
                                                       View* view,
                                                       Sound* soundToPlay) {
    if (touch.phase == brls::TouchPhase::START) {
        fingersCounter++;
        auto num = getFingersNum();
        if (num != -1 && fingersCounter >= num) {
            event.fire();
            return brls::GestureState::END;
        }
    } else if (touch.phase == brls::TouchPhase::END) {
        fingersCounter--;
    }

    if (fingersCounter < 0) fingersCounter = 0;

    return brls::GestureState::UNSURE;
}
