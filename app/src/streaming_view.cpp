//
//  streaming_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#include "streaming_view.hpp"
#include "ingame_overlay_view.hpp"
#include <nanovg.h>
#include <Limelight.h>
#include <chrono>
#include "helper.hpp"
#include "InputManager.hpp"

using namespace brls;

FingersGestureRecognizer::FingersGestureRecognizer(int fingers, FingersGestureEvent::Callback respond)
    : fingers(fingers)
{
    event.subscribe(respond);
}

GestureState FingersGestureRecognizer::recognitionLoop(std::array<TouchState, TOUCHES_MAX> touches, MouseState mouse, View* view, Sound* soundToPlay)
{
    if (touches[fingers - 1].phase == brls::TouchPhase::START)
    {
        event.fire();
        return brls::GestureState::END;
    }
    return brls::GestureState::UNSURE;
}


StreamingView::StreamingView(Host host, AppInfo app) :
    host(host), app(app)
{
    setFocusable(true);
    setHideHighlight(true);
    loader = new LoadingOverlay(this);
    
    keyboardHolder = new Box(Axis::COLUMN);
    keyboardHolder->detach();
    keyboardHolder->setVisibility(brls::Visibility::GONE);
    keyboardHolder->setJustifyContent(JustifyContent::FLEX_END);
    keyboardHolder->setAlignItems(AlignItems::STRETCH);
    addView(keyboardHolder);
    
    keyboard = new KeyboardView();
    keyboardHolder->addView(keyboard);
    
    addGestureRecognizer(new FingersGestureRecognizer(3, [this] {
        keyboardHolder->setVisibility(brls::Visibility::VISIBLE);
    }));
    
    session = new MoonlightSession(host.address, app.app_id);
    
    ASYNC_RETAIN
    session->start([ASYNC_TOKEN](GSResult<bool> result) {
        ASYNC_RELEASE

        loader->setHidden(true);
        if (!result.isSuccess())
        {
            showError(result.error(), [this]() {
                terminate(false);
            });
        }
    });
    
    addGestureRecognizer(new PanGestureRecognizer([this](PanGestureStatus status, Sound* sound) {
        if (status.state == brls::GestureState::START)
            keyboardHolder->setVisibility(brls::Visibility::GONE);
        
        if (status.state == brls::GestureState::STAY)
            MoonlightInputManager::instance().updateTouchScreenPanDelta(status);
    }, PanAxis::ANY));
}

void StreamingView::onFocusGained()
{
    View::onFocusGained();
    if (!blocked)
    {
        blocked = true;
        Application::blockInputs(true);
    }
}

void StreamingView::onFocusLost()
{
    View::onFocusLost();
    if (blocked)
    {
        blocked = false;
        Application::unblockInputs();
    }
}

void StreamingView::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    if (!session->is_active())
    {
        terminate(false);
        return;
    }
    
    session->draw(vg);
    handleInput();
    handleButtonHolding();

    if (session->connection_status_is_poor())
    {
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        nvgFontBlur(vg, 3);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        // nvgFontFace(ctx, "icons");
        // nvgText(ctx, 20, height - 30, utf8(FA_EXCLAMATION_TRIANGLE).data(), NULL);
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "Bad connection...", NULL);

        nvgFontBlur(vg, 0);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
        // nvgFontFace(ctx, "icons");
        // nvgText(ctx, 20, height - 30, utf8(FA_EXCLAMATION_TRIANGLE).data(), NULL);
        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgText(vg, 50, height - 28, "Bad connection...", NULL);
    }
    
    if (draw_stats)
    {
        static char output[1024];
        int offset = 0;
        auto stats = session->session_stats();

        offset += sprintf(&output[offset],
            "Estimated host PC frame rate: %.2f FPS\n"
            "Incoming frame rate from network: %.2f FPS\n"
            "Decoding frame rate: %.2f FPS\n"
            "Rendering frame rate: %.2f FPS\n",
            stats->video_decode_stats.total_fps,
            stats->video_decode_stats.received_fps,
            stats->video_decode_stats.decoded_fps,
            stats->video_render_stats.rendered_fps);

        offset += sprintf(&output[offset],
            "Frames dropped by your network connection: %.2f%% (Total: %u)\n"
            "Average receive time: %.2f ms\n"
            "Average decoding time: %.2f ms\n"
            "Average rendering time: %.2f ms\n",
            (float)stats->video_decode_stats.network_dropped_frames / stats->video_decode_stats.total_frames * 100,
            stats->video_decode_stats.network_dropped_frames,
            (float)stats->video_decode_stats.total_reassembly_time / stats->video_decode_stats.received_frames,
            (float)stats->video_decode_stats.total_decode_time / stats->video_decode_stats.decoded_frames,
            (float)stats->video_render_stats.total_render_time / stats->video_render_stats.rendered_frames);

        nvgFontFaceId(vg, Application::getFont(FONT_REGULAR));
        nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);

        nvgFontBlur(vg, 1);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgTextBox(vg, 20, 30, width, output, NULL);

        nvgFontBlur(vg, 0);
        nvgFillColor(vg, nvgRGBA(0, 255, 0, 255));
        nvgTextBox(vg, 20, 30, width, output, NULL);
    }
    
    Box::draw(vg, x, y, width, height, style, ctx);
}

void StreamingView::terminate(bool terminateApp)
{
//    terminated = true;
//    InputController::controller()->stop_rumple();

    session->stop(terminateApp);
    this->dismiss();
}

void StreamingView::handleInput()
{
    if (!this->focused)
        return;
    
    MoonlightInputManager::instance().handleInput();
    
    static KeyboardState oldKeyboardState;
    KeyboardState keyboardState = keyboard->getKeyboardState();
    
    for (int i = 0; i < _VK_KEY_MAX; i++)
    {
        if (keyboardState.keys[i] != oldKeyboardState.keys[i])
        {
            oldKeyboardState.keys[i] = keyboardState.keys[i];
            LiSendKeyboardEvent(keyboard->getKeyCode((KeyboardKeys)i), keyboardState.keys[i] ? KEY_ACTION_DOWN : KEY_ACTION_UP, 0);
        }
    }
}

void StreamingView::handleButtonHolding()
{
    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateControllerState(&controller);
    
    static std::chrono::high_resolution_clock::time_point clock_counter;
    static bool buttonState = false;
    static bool used = false;
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - clock_counter);
    
    if (!buttonState && controller.buttons[BUTTON_START])
    {
        buttonState = true;
        clock_counter = std::chrono::high_resolution_clock::now();
    }
    else if (buttonState && !controller.buttons[BUTTON_START])
    {
        buttonState = false;
        used = false;
    }
    else if (buttonState && duration.count() > 2 && !used)
    {
        used = true;
        
        IngameOverlay* overlay = new IngameOverlay(this);
        overlay->setTitle(host.hostname + ": " + app.name);
        Application::pushActivity(new Activity(overlay));
    }
}

void StreamingView::onLayout()
{
    Box::onLayout();
    if (loader)
        loader->layout();
    
    if (keyboardHolder)
    {
        keyboardHolder->setWidth(getWidth());
        keyboardHolder->setHeight(getHeight());
    }
}

StreamingView::~StreamingView()
{
    session->stop(false);
    delete session;
}
