//
//  two_finger_scroll_recognizer.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 15.11.2021.
//

#include "two_finger_scroll_recognizer.hpp"

using namespace brls;

TwoFingerScrollGestureRecognizer::TwoFingerScrollGestureRecognizer(ScrollGestureEvent::Callback respond)
{
    event.subscribe(respond);
}

GestureState TwoFingerScrollGestureRecognizer::recognitionLoop(TouchState touch, MouseState mouse, View* view, Sound* soundToPlay)
{
    switch (touch.phase) {
        case TouchPhase::START:
            fingers++;
            fingersStartPoints[touch.fingerId] = touch.position;
            fingersCurrentPoints[touch.fingerId] = touch.position;
            if (fingers == 1) {
                state = brls::GestureState::UNSURE;
            }
            else if (fingers == 2) {
                if (state != brls::GestureState::UNSURE)
                    return state;

                state = brls::GestureState::START;

                Point start;
                for (auto [key, value] : fingersStartPoints) {
                    start = start.lerp(value, 0.5f);
                }
                startPoint = start;
                event.fire({ state, startPoint - startPoint });
            }
            break;
        case TouchPhase::STAY:
            if (fingers == 2) {
                state = brls::GestureState::STAY;
                Point now;
                fingersCurrentPoints[touch.fingerId] = touch.position;
                for (auto [key, value] : fingersCurrentPoints) {
                    now = now.lerp(value, 0.5f);
                }

                event.fire({ state, now - startPoint });
            }
            break;
        case TouchPhase::END:
            if (fingers == 2)
                state = brls::GestureState::END;
            fingers--;
            break;
        default:
            break;
    }

    return state;
}
