#!/bin/bash
cd "$( dirname "${BASH_SOURCE[0]}" )/.."

meson build.mac/build
ninja -C build.mac/build

rm -rf ./build.mac/Moonlight.app

mkdir ./build.mac/Moonlight.app
mkdir ./build.mac/Moonlight.app/Contents
mkdir ./build.mac/Moonlight.app/Contents/MacOS
mkdir ./build.mac/Moonlight.app/Contents/Resources

cp ./build.mac/templates/Info.plist ./build.mac/Moonlight.app/Contents/Info.plist
cp ./build.mac/templates/AppIcon.icns ./build.mac/Moonlight.app/Contents/Resources/AppIcon.icns
cp ./build.mac/build/moonlight ./build.mac/Moonlight.app/Contents/MacOS/Moonlight
cp -r ./resources ./build.mac/Moonlight.app/Contents/Resources/
