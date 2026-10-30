cmake_minimum_required(VERSION 3.22.1)
project(ps5ona LANGUAGES C CXX)

add_library(ps5ona SHARED
    native-lib.cpp
)

target_compile_features(ps5ona PRIVATE cxx_std_20)

find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)

target_link_libraries(ps5ona
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
)
