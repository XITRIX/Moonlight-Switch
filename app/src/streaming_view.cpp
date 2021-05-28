//
//  streaming_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#include "streaming_view.hpp"
#include <nanovg.h>
#include <Limelight.h>

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
    
    Application::blockInputs();
}

void StreamingView::onFocusLost()
{
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
    
    handleInput();
}

void StreamingView::terminate()
{
//    terminated = true;
//    InputController::controller()->stop_rumple();

    // if (m_loader) {
    //     m_loader->dispose();
    //     m_loader = NULL;
    // }

    session->stop(false);
    this->dismiss();
}

void StreamingView::handleInput()
{
    if (!this->focused)
        return;
    
    static ControllerState controller;
    Application::getPlatform()->getInputManager()->updateControllerState(&controller);
    
    
    unsigned char leftTrigger  = 0xFFFF * (controller.buttons[BUTTON_LT] ? 1 : 0);
    unsigned char rightTrigger = 0xFFFF * (controller.buttons[BUTTON_RT] ? 1 : 0);
    short leftStickX           = controller.axes[LEFT_X] * 0x7FFF;
    short leftStickY           = 0xFFFF - controller.axes[LEFT_Y] * 0x7FFF;
    short rightStickX          = controller.axes[RIGHT_X] * 0x7FFF;
    short rightStickY          = 0xFFFF - controller.axes[RIGHT_Y] * 0x7FFF;
    
    short buttonFlags = 0;
#define SET_GAME_PAD_STATE(LIMELIGHT_KEY, GAMEPAD_BUTTON) \
    controller.buttons[GAMEPAD_BUTTON] ? (buttonFlags |= LIMELIGHT_KEY) : (buttonFlags &= ~LIMELIGHT_KEY);
    
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
    
    LiSendControllerEvent(
        buttonFlags,
        leftTrigger,
        rightTrigger,
        leftStickX,
        leftStickY,
        rightStickX,
        rightStickY);
}

StreamingView::~StreamingView()
{
    delete session;
}
