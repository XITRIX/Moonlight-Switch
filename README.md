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

# How to reach higher bitrate
To be able to use high bitrate setting especially with 1080p - resolution, you need to overclock CPU/GPU of your console.

To learn more about that you can take a look at [Sys-Clk homebrew](https://github.com/retronx-team/sys-clk) or entire [Atmosphere build - 4IFIR](https://github.com/rashevskyv/4IFIR/blob/main/README_ENG.md) which includes everything you need to overclock your console

I DO NOT RESPOSIBLE FOR ANY DAMAGE TO YOUR CONSOLE IF ANYTHING WILL GO WRONG! I am using 4IFIR by myself and not find any issue, but everything possible. So think by you own head and be responsible for what you do with your devices!

# Installing
1. Download latest Moonlight-Switch [release](https://github.com/XITRIX/Moonlight-Switch/releases).
2. Put Moonlight.nro to sdcard:/switch/Moonlight-Switch;
3. Launch hbmenu over *Title Redirection* (for FULL RAM access);
4. Launch moonlight.

Or download it from [HB App Store](https://apps.fortheusers.org/switch/Moonlight-Switch)

# Discord
Feel free to join [Moonlight discord server](https://discord.gg/fmtcVPzaG4), you will find me there in "switch-help" channel

# Controls
## Mouse
With touch screen you can move your coursor, tap to left click, scroll 2 fingers to scroll.

While touching screen ZR and ZL buttons will work like left and right mouse buttons.

Also while touching screen L and R sticks will work like scrolling wheel.

USB mouse working as well.

## Keyboard
You can use onscreen keyboard, tap 3 fingers on screen to show it.

USB keyboard working as well.

## Gamepad
By default, Switch gamepad configured as X360 gamepad (A/B and X/Y swapped). Key mapping available in application settings.

Up to 5 gamepads (includes handheld mode) supported. Half of joycons are also supported.

## SixAxis
You should configure your Sunshine server to recognise controller as DS4 one to be able to use Gyro and Accelerometer. Only works for player 1 controller.

## Ingame overlay
To open overlay, press - and + key simultaneously by default or Hold ESC on keyboard.

Key combination and holding time are configurable in settings.

## NSP forwarder
App supports NSP forwarders to start stream immediately with predefined configuration. Add app you want to launch in Favorites list first.

You'll need to add thees arguments to the forwarder:
- `--host` - Mac address of your PC (you could find it in /switch/Moonlight-Switch/settings.json)
- `--appid` - ID of the app to launch
- `--appname` - The name of the app without any spacings

example:
`--host=a2:34:de:ad:12:3b --appid=1233211234 --appname=Steam`

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

```bash
cd 'folder/to/store/the/sources'

# Clone this repo with submodules
git clone https://github.com/XITRIX/Moonlight-Switch.git --recursive
cd Moonlight-Switch
```

## Switch

To build for Switch, a standard development environment must first be set up. In order to do so, [refer to the Getting Started guide](https://devkitpro.org/wiki/Getting_Started).

```bash
cmake -B build/switch -DPLATFORM_SWITCH=ON
make -C build/switch Moonlight.nro -j$(nproc)
```

## PC (Windows/Linux/MacOS)

To build for PC, the following components are required:

- cmake/make build system
- A C++ compiler supporting the C++17 standard

Please refer to the usual sources of information for your particular operating system. Usually the commands needed to build this project will look like this:

```bash
cmake -B build/pc -DPLATFORM_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release
make -C build/pc -j$(nproc)
```

Also, please note that the `resources` folder must be available in the working directory, otherwise the program will fail to find the shaders.

## iOS:

```shell
# build libromfs generator
./build_libromfs_generator.sh

# prepare vcpkg
./extern/vcpkg/bootstrap-vcpkg.sh

# install packages
./extern/vcpkg/vcpkg install --triplet arm64-ios mbedtls
./extern/vcpkg/vcpkg install --triplet arm64-ios jansson
./extern/vcpkg/vcpkg install --triplet arm64-ios ffmpeg
./extern/vcpkg/vcpkg install --triplet arm64-ios "curl[mbedtls]"
./extern/vcpkg/vcpkg install --triplet arm64-ios libpng
./extern/vcpkg/vcpkg install --triplet arm64-ios opus
```

### 1. Build for arm64 iphoneOS

```shell
# 1. Generate a Xcode project
# IOS_CODE_SIGN_IDENTITY: code is not signed when IOS_CODE_SIGN_IDENTITY is empty
# IOS_GUI_IDENTIFIER: optional, default is com.borealis.demo
cmake -B build/ios -G Xcode -DPLATFORM_IOS=ON

# 2. open project in Xcode
open build/ios/*.xcodeproj

# 3. Set up Team and Bundle Identifiers in Xcode, then connect devices to run.
```

# Credits
Thanks a lot to [Rock88](https://github.com/rock88) and his [Moonlight-NX](https://github.com/rock88/moonlight-nx), lots of streaming code has been lend from it 👍.

[Xfangfang](https://github.com/xfangfang) for maintaining [Borealis](https://github.com/xfangfang/borealis) library. iOS port would not be possible without it. 

Also thanks to [Averne](https://github.com/averne) for NVDEC implementation into [FFmpeg](https://github.com/averne/FFmpeg) and useful guidance of how to enable it 