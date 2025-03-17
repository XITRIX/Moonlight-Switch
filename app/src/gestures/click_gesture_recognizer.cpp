//
//  click_gesture_recognizer.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.10.2021.
//

#include "click_gesture_recognizer.hpp"

#include <utility>

#define ALLOWED_RANGE 15

using namespace brls;

ClickGestureRecognizer::ClickGestureRecognizer(
    int fingersRequired, ClickGestureEvent::Callback respond)
    : fingers(fingersRequired) {
    event.subscribe(std::move(respond));
}

GestureState ClickGestureRecognizer::recognitionLoop(TouchState touch,
                                                     MouseState mouse,
                                                     View* view,
                                                     Sound* soundToPlay) {
    if (touch.phase == brls::TouchPhase::START) {
        if (fingersStartPoints.empty()) {
            state = GestureState::UNSURE;
        }

        fingersStartPoints[touch.fingerId] = touch.position;

        if (fingersStartPoints.size() > fingers) {
            state = GestureState::FAILED;
        }
    } else if (touch.phase == brls::TouchPhase::STAY) {
        Point position = touch.position;
        Point startPosition = fingersStartPoints[touch.fingerId];
        if (position.x < startPosition.x - ALLOWED_RANGE ||
            position.x > startPosition.x + ALLOWED_RANGE ||
            position.y < startPosition.y - ALLOWED_RANGE ||
            position.y > startPosition.y + ALLOWED_RANGE) {
            this->state = GestureState::FAILED;
            this->event.fire({GestureState::FAILED, position});
        }
    } else if (touch.phase == brls::TouchPhase::END) {
        if (fingersStartPoints.size() == fingers &&
            this->state != GestureState::INTERRUPTED &&
            this->state != GestureState::FAILED) {
            this->state = GestureState::END;
            this->event.fire({GestureState::END, touch.position});
        }
        fingersStartPoints.erase(touch.fingerId);
    }

    return this->state;
}
