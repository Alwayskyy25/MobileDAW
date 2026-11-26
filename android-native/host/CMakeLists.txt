# Host smoke test: ui_core + stub engine on a desktop machine (no Android).
# Build: cmake -S host -B host/build && cmake --build host/build && ./host/build/host_test
cmake_minimum_required(VERSION 3.18)
project(mobiledaw_native_host CXX)
set(CMAKE_CXX_STANDARD 17)
set(CPP ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp)
add_executable(host_test
    ${CPP}/ui_core.cpp
    ${CPP}/engine_link_stub.cpp
    main.cpp)
target_include_directories(host_test PRIVATE ${CPP})
target_compile_options(host_test PRIVATE -O2 -Wall)
