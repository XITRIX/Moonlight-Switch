# borealis android demo

borealis 的安卓移植遵循 SDL 的文档：[SDL/docs](https://github.com/libsdl-org/SDL/blob/release-2.28.x/docs/README-android.md)

目前本项目使用 SDL 2.28.* ，在 SDL3 中有一些关于安卓的有趣的特性，预计在其正式发布后再进行迁移

理论上支持 Android API 16 及之后机型 (Android 4.1+)，但我只在 Android 5.0+ 的设备上测试过，因为我实在找不到更老的设备了。

## 构建

```shell
cd android-project
```

在安卓平台，borealis 使用 libromfs 将所有资源文件直接打包到共享库中，我们需要先构建 libromfs-generator。

> libromfs-generator 的作用是将资源文件转换为 cpp 源码，并在后续的编译过程中链接进主程序， 

```shell
./build_libromfs_generator.sh
```

在成功运行后，`app/jni` 目录下会生成: `libromfs-generator`

然后打开 Android Studio 编译即可

```shell
# 在 mac 下我使用这样的命令以从命令行编译和安装
export JAVA_HOME=/Applications/Android\ Studio.app/Contents/jbr/Contents/Home
export ANDROID_SDK_ROOT=~/Library/Android/sdk
# 构建好之后，apk 默认会在 app/build/outputs/apk/debug 目录下
./gradlew assembleDebug
# 直接进行安装 （需要手机或模拟器已经通过 adb 连接）
./gradlew installDebug
```

## 如何让你的 borealis 项目迁移到安卓

1. 将你的项目放置在 app/jni 目录下
2. 修改 app/jni/CMakeLists.txt 把原本 borealis 的部分替换成你的项目
3. 修改 app/build.gradle 中的 applicationId，把它改为你的包名
4. 模仿 app/src/main/java 下的 com.borealis.demo 创建你的 Activity
5. 修改 app/src/main/AndroidMainifest.xml，将 package 改为你的包名，将DemoActivity改为你创建的 Activity 名
6. 修改 app/src/main/res/values/strings.xml 中的软件名
7. 修改 app/src/main/res/mipmap-*/ic_launcher.png 为你的应用图标

你可以查阅这个项目的早期提交记录来获得需要修改的内容大概范围

## 存在的问题

如果删除了某些资源文件，libromfs 可能没有识别出来，导致编译报错，这时候临时的解决方法是：

微调一下 app/jni/CMakeLists.txt （随意加一空行即可），以触发 cmake 重新运行