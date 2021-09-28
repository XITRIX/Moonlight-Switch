# Moonlight-Switch

Moonlight-Switch is a port of [Moonlight Game Streaming Project](https://github.com/moonlight-stream "Moonlight Game Streaming Project") for Nintendo Switch.

Thanks a lot to [Rock88](https://github.com/rock88) and his [Moonlight-NX](https://github.com/rock88/moonlight-nx), lots of streaming code has been lend from it 👍.

# Screenshots
<details>
  <summary>Preview</summary>
  <p float="left">
  <img width="500" src="https://user-images.githubusercontent.com/9553519/122124363-da8b0880-ce37-11eb-9a08-b6f17c794673.jpg" />
  <img width="500" src="https://user-images.githubusercontent.com/9553519/122124394-e7a7f780-ce37-11eb-9a04-f44b380b6a35.jpg" />
  </p>
  <p float="left">
  <img width="500" src="https://user-images.githubusercontent.com/9553519/122124550-1b831d00-ce38-11eb-97e7-ae1f70d02e32.jpg" />
  <img width="500" src="https://user-images.githubusercontent.com/9553519/122124600-2b026600-ce38-11eb-9e18-3da42f8fc33d.jpg" />
  </p>
</details>

# Installing
1. Download latest Moonlight-Switch [release](https://github.com/XITRIX/Moonlight-Switch/releases).
2. Put br_moonlight.nro to sdcard:/switch/moonlight;
3. Launch hbmenu over *Title Redirection* (for FULL RAM access);
4. Launch moonlight.

# Controls
## Mouse
With touch screen you can move your coursor.

While touching screen ZR and ZL buttons will work like left and right mouse buttons.

Also While touching screen L and R sticks will work like scrolling wheel.

USB mouse working as well.

## Keyboard
You can use onscreen keyboard, tap 3 fingers on screen to show it.

USB keyboard working as well.

## Gamepad
By default Switch gamepad configured as X360 gamepad (A/B and X/Y swapped). 

Multiple gamepad currently not supported.

## Ingame overlay
To open overlay, press - and + key simultaneously by default.

Key combination and holding time are configurable in settings.

# Localization
- English (100%)
- Russian (100%)
- Japanese (97%)
- Spanish (100%)
- Czech (98%) - unsupported yet, as HOS has no such system language

## Contribution
If you'd like to improve existing language, or add a new one, follow the instruction:
1. Ask a permission to modify language [here](https://poeditor.com/join/project?hash=9kiCIvN0dc)
2. Notify me by [creating an issue](https://github.com/XITRIX/Moonlight-Switch/issues/new) with title "[Localization] - {Name of language}", in description write your nickname on POEditor
3. After translation is done, notify me in issue created earlier

You have 2 options to add that translation:
1. If you'd like your profile in "contributors" section, you could add that localization by creating a PR
2. If you don't care, I could do that by myself

If you'd like to test your translation, you could follow build instructions, or ask me to create a build with your localization, I'll attach that build in issue.

ATTENTION! Currently there is no way to select language inside of app, it takes from system settings, so it is impossible to add locatization, that HOS doesn't support (that happend with Czech language).

# Build Moonlight-Switch
1. Clone this repo with submodules by `git clone https://github.com/XITRIX/Moonlight-Switch.git --recursive`
2. `cd` into folder

## Switch:
3. Build by `make -j4`
4. If it doesn't work, try to install missing packages
5. If it still doesn't work, god bless you!

## MacOS (XCode):
3. Open .xcodeproj
4. Fix dependency folders if you need inside `Project->Moonlight->Search Paths`
5. Run the build
6. If it doesn't work, try to install missing packages using Homebrew
7. And again, god bless you!

## Linux/Windows/MacOS
3. Install `meson` and `ninja`
4. Run `meson build`
5. Run `ninja -C build`
6. Install every dependency which compiler asks and return to `5`
7. Start app with `./build/moonlight`
