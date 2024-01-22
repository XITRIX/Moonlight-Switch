# borealis android demo

[中文](./README_ZH.md)

The Android port of borealis follows SDL's documentation: [SDL/docs](https://github.com/libsdl-org/SDL/blob/release-2.28.x/docs/README-android.md)

Currently, this project uses SDL 2.28.*, but there are some interesting Android-specific features in SDL3. We plan to migrate to SDL3 once it is officially released.

Theoretically, it supports Android API 16 and later (Android 4.1+), but I have only tested it on devices running Android 5.0+ because I couldn't find older devices.

## Building

```shell
cd android-project
```

On the Android platform, borealis uses libromfs to package all resource files directly into the shared library. We need to build the libromfs-generator first.

> The libromfs-generator is responsible for converting resource files into C++ source code and linking them into the main program during the compilation process.

```shell
./build_libromfs_generator.sh
```

After a successful run, `libromfs-generator` will be generated in the `app/jni` directory.

Then open Android Studio and build the project.

```shell
# macOS: compile and install from the command line
export JAVA_HOME=/Applications/Android\ Studio.app/Contents/jbr/Contents/Home
export ANDROID_SDK_ROOT=~/Library/Android/sdk
# Once built, the APK will be located in the app/build/outputs/apk/debug directory by default
./gradlew assembleDebug
# Directly install the APK (requires the device or emulator to be connected via adb)
./gradlew installDebug
```

## Migrating your borealis project to Android

1. Place your project under the `app/jni` directory.
2. Modify `app/jni/CMakeLists.txt` and replace the relevant parts of borealis with your project.
3. Modify the `applicationId` in `app/build.gradle` and change it to your package name.
4. Create your own activity by imitating `com.borealis.demo` in `app/src/main/java`.
5. Modify `app/src/main/AndroidManifest.xml` and change the package to your package name, and change `DemoActivity` to the name of your created activity.
6. Modify the app name in `app/src/main/res/values/strings.xml`.
7. Replace `app/src/main/res/mipmap-*/ic_launcher.png` with your application icon.

You can refer to the early commit history of this project to get an idea of what needs to be modified.

## Known Issues

If you delete some resource files, libromfs may not recognize it, causing a compilation error. In such cases, a temporary solution is to:

Make a minor adjustment in `app/jni/CMakeLists.txt` (add an empty line), to trigger cmake to run again.