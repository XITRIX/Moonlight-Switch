# Moonlight-Switch

Moonlight-Switch is a port of [Moonlight Game Streaming Project](https://github.com/moonlight-stream "Moonlight Game Streaming Project") for Nintendo Switch.

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

# If you cannot use bitrate higher than 10 Mbps - read this!
To be able to use any streaming setting higher than 720p - resolution, 10MBbps - Bitrate, you need to overclock CPU of your console.

This leads from lack of access to GPU decoder and because of that all decoding performs on CPU, while it is not powerfull enough to handle it.

To learn more about that you can take a look at [Sys-Clk homebrew](https://github.com/retronx-team/sys-clk) or entire [Atmosphere build - 4IFIR](https://github.com/rashevskyv/4IFIR/blob/main/README_ENG.md) which includes everything you need to overclock your console

I DO NOT RESPOSIBLE FOR ANY DAMAGE TO YOUR CONSOLE IF ANYTHING WILL GO WRONG! I am using 4IFIR by myself and not find any issue, but everything possible. So think by you own head and be responsible for what you do with your devices!

# Attention for developers
If you know how to debug Nintendo Switch Homebrew applications please let me know! I have no idea how to do this, I'm just an iOS developer who wants "click-click, UI debugger here we go" and not this GDB and Coredump stuff...

Jokes aside, I seriously couldn't find any usefull information about it, all crashes shows ?? instead of function names and I cannot understand what I do wrong.

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
By default Switch gamepad configured as X360 gamepad (A/B and X/Y swapped). Key mapping available in application settings.

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
3. Install core Switch packages `dkp-pacman -Suy switch-dev`
4. Install other packages `dkp-pacman -Suy switch-ffmpeg switch-mbedtls switch-opusfile switch-sdl2 switch-curl switch-libexpat switch-jansson switch-glfw switch-glm switch-libvpx switch-glad`
5. Configure by `cmake -B build_switch -DPLATFORM_SWITCH=ON`
6. Build by `make -C build_switch borealis_demo.nro -j$(nproc)`
7. Moonlight-Switch.nro should be created. If it doesn't work, try to install missing packages
8. If it still doesn't work, god bless you!

## iOS:

```shell
# build libromfs generator
./build_libromfs_generator.sh

# prepare vcpkg
./extern/vcpkg/bootstrap-vcpkg.sh

# install packages
./extern/vcpkg/vcpkg --triplet arm64-ios mbedtls
./extern/vcpkg/vcpkg --triplet arm64-ios jansson
./extern/vcpkg/vcpkg --triplet arm64-ios ffmpeg
./extern/vcpkg/vcpkg --triplet arm64-ios curl
./extern/vcpkg/vcpkg --triplet arm64-ios libpng
./extern/vcpkg/vcpkg --triplet arm64-ios opus
```

### 1. Build for arm64 iphoneOS

```shell
# 1. Generate a Xcode project
# IOS_CODE_SIGN_IDENTITY: code is not signed when IOS_CODE_SIGN_IDENTITY is empty
# IOS_GUI_IDENTIFIER: optional, default is com.borealis.demo
cmake -B build-ios -G Xcode -DPLATFORM_IOS=ON -DPLATFORM=OS64 -DDEPLOYMENT_TARGET=13.0 \
  -DIOS_CODE_SIGN_IDENTITY="Your identity" \
  -DIOS_GUI_IDENTIFIER="com.xitrix.moonlight"

# 2. open project in Xcode
open build-ios/*.xcodeproj

# 3. Set up Team and Bundle Identifiers in Xcode, then connect devices to run.
```

# Credits
Thanks a lot to [Rock88](https://github.com/rock88) and his [Moonlight-NX](https://github.com/rock88/moonlight-nx), lots of streaming code has been lend from it 👍.

Also thanks to [Averne](https://github.com/averne) for NVDEC implementation into [FFmpeg](https://github.com/averne/FFmpeg) and useful guidance of how to enable it 