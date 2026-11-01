# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)
project(arucocam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARUCOCAM_STATIC "Link the arucocam executable statically" OFF)
option(ARUCOCAM_BUILD_TESTS "Build the marker-image test helper" OFF)

find_package(OpenCV 4.7 REQUIRED
    COMPONENTS core imgproc imgcodecs calib3d objdetect)

add_executable(arucocam
    src/main.cpp
    src/calibration.cpp
    src/detector.cpp
    src/frame_source.cpp
    src/v4l2_source.cpp
    src/output.cpp
)

target_include_directories(arucocam PRIVATE src)
target_link_libraries(arucocam PRIVATE ${OpenCV_LIBS})
target_compile_options(arucocam PRIVATE -Wall -Wextra -Wpedantic -Wno-psabi)
target_compile_definitions(arucocam PRIVATE ARUCOCAM_VERSION="${PROJECT_VERSION}")

if(ARUCOCAM_STATIC)
    target_link_options(arucocam PRIVATE -static -no-pie)
endif()

install(TARGETS arucocam RUNTIME DESTINATION bin)

if(ARUCOCAM_BUILD_TESTS)
    add_executable(gen_marker tests/gen_marker.cpp)
    target_link_libraries(gen_marker PRIVATE ${OpenCV_LIBS})
    target_compile_options(gen_marker PRIVATE -Wall -Wextra -Wpedantic -Wno-psabi)
endif()
