# Native app library: C++ UI + Oboe host + the repo's shared C++ engine (native/).
# Layout assumption: this project lives at <repo>/android-native/, so the shared
# engine is at ../../../../../native from here - same relative depth the old
# Capacitor app used (android/app/src/main/cpp -> native/).
cmake_minimum_required(VERSION 3.18)
project(mobiledaw_native CXX C)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../native ${CMAKE_CURRENT_BINARY_DIR}/engine)
find_package(oboe REQUIRED CONFIG)

# android_native_app_glue (ships with the NDK)
add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(mobiledaw_native SHARED
    native_main.cpp
    ui_core.cpp
    engine_link.cpp
    gles_renderer.cpp)
target_compile_options(mobiledaw_native PRIVATE -O2 -ffp-contract=off -Wall)
target_link_libraries(mobiledaw_native
    mdaw_engine oboe::oboe native_app_glue android EGL GLESv2 log)
