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

        // Force exit of the app until Borealis supports recreating its static state.
        System.exit(0);
    }

    @Override
    protected String[] getLibraries() {
        // Load SDL3 and Moonlight.
        return new String[] {
                "SDL3",
                "Moonlight"
        };
    }

}
