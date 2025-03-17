//
//  two_finger_scroll_recognizer.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 15.11.2021.
//

#include "two_finger_scroll_recognizer.hpp"

#include <utility>

using namespace brls;

TwoFingerScrollGestureRecognizer::TwoFingerScrollGestureRecognizer(
    ScrollGestureEvent::Callback respond) {
    event.subscribe(std::move(respond));
}

GestureState TwoFingerScrollGestureRecognizer::recognitionLoop(
    TouchState touch, MouseState mouse, View* view, Sound* soundToPlay) {
    switch (touch.phase) {
    case TouchPhase::START:
        fingers++;
        fingersStartPoints[touch.fingerId] = touch.position;
        fingersCurrentPoints[touch.fingerId] = touch.position;
        if (fingers == 1) {
            state = brls::GestureState::UNSURE;
        } else if (fingers == 2) {
//            if (state != brls::GestureState::UNSURE)
//                return state;

            // State should be START, but because of conflicts with other gestures in Stream View
            // we will still use UNSURE
            state = brls::GestureState::UNSURE;

            Point start;
            for (auto [key, value] : fingersStartPoints) {
                start = start.lerp(value, 0.5f);
            }
            startPoint = start;

            // We will notify user with event START even if real status UNSURE to allow to reset initial values
            event.fire({brls::GestureState::START, startPoint - startPoint});
        }
        break;
    case TouchPhase::STAY:
        if (fingers == 1) {
            fingersStartPoints[touch.fingerId] = touch.position;
            fingersCurrentPoints[touch.fingerId] = touch.position;
        }

        if (fingers == 2) {
            state = brls::GestureState::STAY;
            Point now;
            fingersCurrentPoints[touch.fingerId] = touch.position;
            for (auto [key, value] : fingersCurrentPoints) {
                now = now.lerp(value, 0.5f);
            }

            event.fire({state, now - startPoint});
        }
        break;
    case TouchPhase::END:
        if (fingers == 2)
            state = brls::GestureState::END;

        if (fingers > 0)
            fingers--;

        fingersStartPoints.erase(touch.fingerId);
        fingersCurrentPoints.erase(touch.fingerId);

        break;
    default:
        break;
    }

    return state;
}

void TwoFingerScrollGestureRecognizer::forceReset() {
    fingers = 0;
    fingersStartPoints.clear();
    fingersCurrentPoints.clear();
    state = brls::GestureState::FAILED;
}
