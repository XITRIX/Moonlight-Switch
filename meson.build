project('borealis_demo', ['c', 'cpp'],
    version: '1.0.0',
    default_options: [ 'buildtype=release', 'strip=true', 'b_ndebug=if-release', 'cpp_std=c++1z' ],
)

subdir('extern')

demo_files = files(
    'demo/main.cpp',

    'demo/main_activity.cpp',

    'demo/captioned_image.cpp',

    'demo/components_tab.cpp',
    'demo/settings_tab.cpp',
    'demo/host_tab.cpp',
)

borealis_demo = executable(
    'borealis_demo',
    [ demo_files, borealis_files ],
    dependencies : borealis_dependencies,
    install: true,
    include_directories: [ borealis_include, include_directories('demo')],
    cpp_args: [ '-g', '-O2', '-DBRLS_RESOURCES="./resources/"', ] + borealis_cpp_args
)
