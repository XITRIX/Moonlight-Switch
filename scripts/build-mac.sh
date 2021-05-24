#!/bin/bash
cd "$( dirname "${BASH_SOURCE[0]}" )/.."

meson build.mac/build
ninja -C build.mac/build

rm -rf ./build.mac/Borealis.app

mkdir ./build.mac/Borealis.app
mkdir ./build.mac/Borealis.app/Contents
mkdir ./build.mac/Borealis.app/Contents/MacOS
mkdir ./build.mac/Borealis.app/Contents/Resources

cp ./build.mac/templates/Info.plist ./build.mac/Borealis.app/Contents/Info.plist
cp ./build.mac/templates/AppIcon.icns ./build.mac/Borealis.app/Contents/Resources/AppIcon.icns
cp ./build.mac/build/borealis_demo ./build.mac/Borealis.app/Contents/MacOS/Borealis
cp -r ./resources ./build.mac/Borealis.app/Contents/Resources/