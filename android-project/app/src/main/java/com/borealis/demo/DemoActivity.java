package com.borealis.demo;
import org.libsdl.app.SDLActivity;

public class DemoActivity extends SDLActivity
{

    @Override
    protected void onDestroy() {
        super.onDestroy();

        // Android does not recommend using exit(0) directly,
        // but borealis heavily uses static variables,
        // which can cause some problems when reloading the program.

        // In SDL3, we can use SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY to control the behavior

        // In SDL2, Force exit of the app.
        System.exit(0);
    }

    @Override
    protected String[] getLibraries() {
        // Load SDL2 and borealis demo app
        return new String[] {
                "SDL2",
                "borealis_demo"
        };
    }

}