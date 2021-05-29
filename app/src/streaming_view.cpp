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

// Moonlight ready gamepad
struct GamepadState {
    short buttonFlags;
    unsigned char leftTrigger;
    unsigned char rightTrigger;
    short leftStickX;
    short leftStickY;
    short rightStickX;
    short rightStickY;
    
    bool is_equal(GamepadState other) {
        return buttonFlags == other.buttonFlags &&
        leftTrigger == other.leftTrigger &&
        rightTrigger == other.rightTrigger &&
        leftStickX == other.leftStickX &&
        leftStickY == other.leftStickY &&
        rightStickX == other.rightStickX &&
        rightStickY == other.rightStickY;
    }
};

StreamingView::StreamingView(Host host, AppInfo app) :
    host(host), app(app)
{
    setFocusable(true);
    setHideHighlight(true);
    
    session = new MoonlightSession(host.address, app.app_id);
    
    ASYNC_RETAIN
    session->start([ASYNC_TOKEN](GSResult<bool> result) {
        ASYNC_RELEASE

        if (!result.isSuccess())
        {
            auto alert = new brls::Dialog(result.error());
            alert->addButton("Close", [this](View* view)
            {
                view->dismiss([this]() {
                    terminate();
                });
            });
            alert->setCancelable(true);
            alert->open();
        }
    });
}

void StreamingView::onFocusGained()
{
    View::onFocusGained();
    Application::blockInputs(true);
}

void StreamingView::onFocusLost()
{
    View::onFocusLost();
    Application::unblockInputs();
}

void StreamingView::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    if (!session->is_active())
    {
        brls::Application::notify("Terminate");
        terminate();
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
}

void StreamingView::terminate()
{
//    terminated = true;
//    InputController::controller()->stop_rumple();

    session->stop(false);
    this->dismiss();
}

void StreamingView::handleInput()
{
    if (!this->focused)
        return;
    
    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateControllerState(&controller);
    
    static GamepadState lastState;
    GamepadState state
    {
        .buttonFlags = 0,
        .leftTrigger = static_cast<unsigned char>(0xFFFF * (controller.buttons[BUTTON_LT] ? 1 : 0)),
        .rightTrigger = static_cast<unsigned char>(0xFFFF * (controller.buttons[BUTTON_RT] ? 1 : 0)),
        .leftStickX = static_cast<short>(controller.axes[LEFT_X] * 0x7FFF),
        .leftStickY = static_cast<short>(0xFFFF - controller.axes[LEFT_Y] * 0x7FFF),
        .rightStickX = static_cast<short>(controller.axes[RIGHT_X] * 0x7FFF),
        .rightStickY = static_cast<short>(0xFFFF - controller.axes[RIGHT_Y] * 0x7FFF),
    };
    
#define SET_GAME_PAD_STATE(LIMELIGHT_KEY, GAMEPAD_BUTTON) \
    controller.buttons[GAMEPAD_BUTTON] ? (state.buttonFlags |= LIMELIGHT_KEY) : (state.buttonFlags &= ~LIMELIGHT_KEY); \
    if (controller.buttons[GAMEPAD_BUTTON]) \
        Logger::info("StreamingView: button {} pressed", LIMELIGHT_KEY);
    
    SET_GAME_PAD_STATE(UP_FLAG, BUTTON_UP);
    SET_GAME_PAD_STATE(DOWN_FLAG, BUTTON_DOWN);
    SET_GAME_PAD_STATE(LEFT_FLAG, BUTTON_LEFT);
    SET_GAME_PAD_STATE(RIGHT_FLAG, BUTTON_RIGHT);

    SET_GAME_PAD_STATE(A_FLAG, BUTTON_A);
    SET_GAME_PAD_STATE(B_FLAG, BUTTON_B);
    SET_GAME_PAD_STATE(X_FLAG, BUTTON_Y);
    SET_GAME_PAD_STATE(Y_FLAG, BUTTON_X);

    SET_GAME_PAD_STATE(BACK_FLAG, BUTTON_BACK);
    SET_GAME_PAD_STATE(PLAY_FLAG, BUTTON_START);

    SET_GAME_PAD_STATE(LB_FLAG, BUTTON_LB);
    SET_GAME_PAD_STATE(RB_FLAG, BUTTON_RB);

    SET_GAME_PAD_STATE(LS_CLK_FLAG, BUTTON_LSB);
    SET_GAME_PAD_STATE(RS_CLK_FLAG, BUTTON_RSB);
    
    if (!state.is_equal(lastState))
    {
        lastState = state;
        LiSendControllerEvent(
              state.buttonFlags,
              state.leftTrigger,
              state.rightTrigger,
              state.leftStickX,
              state.leftStickY,
              state.rightStickX,
              state.rightStickY);
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

StreamingView::~StreamingView()
{
    delete session;
}
