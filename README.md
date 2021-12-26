# Moonlight-Switch

Moonlight-Switch is a port of [Moonlight Game Streaming Project](https://github.com/moonlight-stream "Moonlight Game Streaming Project") for Nintendo Switch.

Thanks a lot to [Rock88](https://github.com/rock88) and his [Moonlight-NX](https://github.com/rock88/moonlight-nx), lots of streaming code has been lend from it 👍.

# Screenshots
<details>
  <summary>Preview</summary>
  <p float="left">
  <img width="500" src="https://user-images.githubusercontent.com/9553519/135712658-20382345-2da5-4968-9f57-f9f4470ae819.jpg" />
  <img width="500" src="https://user-images.githubusercontent.com/9553519/135712664-bf2481b2-2791-490d-99a9-2f968682db76.jpg" />
  <img width="500" src="https://user-images.githubusercontent.com/9553519/135712669-fd8b2495-e1ea-4357-949f-7fa7312da46f.jpg" />
  <img width="500" src="https://user-images.githubusercontent.com/9553519/135712672-b9ac3785-bd1c-4948-82b2-9b353019feba.jpg" />
  <img width="500" src="https://user-images.githubusercontent.com/9553519/135712676-aaa85bb7-9517-4a6d-bc35-070df092383c.jpg" />
  </p>
</details>

# Installing
1. Download latest Moonlight-Switch [release](https://github.com/XITRIX/Moonlight-Switch/releases).
2. Put Moonlight-Switch.nro to sdcard:/switch/Moonlight-Switch;
3. Launch hbmenu over *Title Redirection* (for FULL RAM access);
4. Launch moonlight.

Or download it from [HB App Store](https://apps.fortheusers.org/switch/Moonlight-Switch)

# Discord
Feel free to join [Moonlight discord server](https://discord.gg/fmtcVPzaG4), you will find me there in "switch-help" channel

# Controls
## Mouse
With touch screen you can move your coursor, tap to left click, scroll 2 fingers to scroll.

While touching screen ZR and ZL buttons will work like left and right mouse buttons.

Also While touching screen L and R sticks will work like scrolling wheel.

USB mouse working as well.

## Keyboard
You can use onscreen keyboard, tap 3 fingers on screen to show it.

USB keyboard working as well.

## Gamepad
By default Switch gamepad configured as X360 gamepad (A/B and X/Y swapped). Key mapping availale in application settings.

Up to 5 gamepads (includes handheld mode) supported. Half of joycons are also supported.

## Ingame overlay
To open overlay, press - and + key simultaneously by default or Hold ESC on keyboard.

Key combination and holding time are configurable in settings.

# Localization
- English (100%)
- Russian (100%)
- German (86%)
- Spanish (72%)
- Japanese (70%)
- Chinese (simplified) (86%)
- Czech (70%) - unsupported yet, as HOS has no such system language

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

## Switch using Docker:
3. `scripts/build-switch.sh`
4. Find `Moonlight-Switch.nro`

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
