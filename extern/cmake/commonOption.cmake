# disable in-source build
if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed")
endif ()

include(CMakeDependentOption)

# Platform
option(PLATFORM_DESKTOP "build for desktop (Linux, macOS, Windows)" OFF)
option(PLATFORM_IOS "build for iOS" OFF)
option(PLATFORM_ANDROID "build for Android" OFF)
option(PLATFORM_PSV "build for psv" OFF)
option(PLATFORM_SWITCH "build for switch" OFF)

# Windows Only
cmake_dependent_option(WIN32_TERMINAL "Show terminal when run on Windows" ON "WIN32" OFF)

# Linux Only
cmake_dependent_option(INSTALL "Install to system." OFF "UNIX;NOT APPLE" OFF)

# iOS Only (If empty then not sign)
set(IOS_CODE_SIGN_IDENTITY "" CACHE STRING "The code sign identity to use when building the IPA.")
set(IOS_GUI_IDENTIFIER "" CACHE STRING "Package name.")

option(USE_BOOST_FILESYSTEM "Whether to use the boost::filesystem instead of std::filesystem" OFF)

# Do not use it until you know what you are doing.
# You may need this option when packaging a AppImage.
option(CUSTOM_RESOURCES_DIR "Custom resources dir" "")

option(USE_GLFW "using glfw for input and create window" OFF)
option(USE_SDL2 "using sdl2 for input and create window" OFF)

# OpenGL version
option(USE_GL3 "using OpenGL 3.2+" OFF)
option(USE_GL2 "using OpenGL 2.1" OFF)
option(USE_GLES2 "using OpenGL ES 2.0" OFF)
option(USE_GLES3 "using OpenGL ES 3.0" OFF)

# On platforms that are not convenient to load files, such as Android and iOS, they will be forcibly enabled.
# On other platforms, if you do not want to attach necessary resource files when sending the program to others,
# or if you do not want others to modify the resource files, you can also enable this option
option(USE_LIBROMFS "using libromfs to bundle resources" OFF)


if (NOT DEFINED APP_PLATFORM_INCLUDE)
    set(APP_PLATFORM_INCLUDE)
endif ()

if (NOT DEFINED APP_PLATFORM_LIB)
    set(APP_PLATFORM_LIB)
endif ()

if (NOT DEFINED APP_PLATFORM_OPTION)
    set(APP_PLATFORM_OPTION)
endif ()

if (NOT DEFINED APP_PLATFORM_LINK_OPTION)
    set(APP_PLATFORM_LINK_OPTION)
endif ()