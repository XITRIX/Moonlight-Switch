cmake_minimum_required(VERSION 3.6)

project(MY_APP)

find_package(SDL3 REQUIRED CONFIG)

add_library(main SHARED)

target_sources(main PRIVATE YourSourceHere.c)

target_link_libraries(main SDL3::SDL3)

