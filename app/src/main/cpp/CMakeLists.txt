cmake_minimum_required(VERSION 3.22.1)
project(ps5ona LANGUAGES C CXX)

add_library(ps5ona SHARED
    native-lib.cpp
    diagnostics/logger.cpp
    diagnostics/crash_handler.cpp
)

target_compile_features(ps5ona PRIVATE cxx_std_20)
target_include_directories(ps5ona PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Keep frame pointers and unwind tables so crash backtraces stay readable.
target_compile_options(ps5ona PRIVATE -fno-omit-frame-pointer -funwind-tables)

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

target_link_libraries(ps5ona
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
    dl
)
