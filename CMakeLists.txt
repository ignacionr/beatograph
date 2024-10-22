cmake_minimum_required(VERSION 3.16)

# Set the path to vcpkg toolchain file
set(CMAKE_TOOLCHAIN_FILE "E:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# Enable CMake's find_package to use vcpkg
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")

# Set the project name and version
project(Beatograph VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_testing()

# Add the main executable target
add_executable(beatograph 
  src/pch.cpp
  src/metrics/metric_view.cpp
  src/beatograph.cpp
  src/metrics/metric_view_config_screen.cpp
  src/metrics/metrics_menu.cpp
  src/metrics/metrics_parser.cpp
)

# Precompiled headers for Windows/MSVC
if (MSVC)
    # Specify the precompiled header file for beatograph
    target_precompile_headers(beatograph PRIVATE "src/pch.h")
    
    # Set warnings and other options
    target_compile_options(beatograph PRIVATE /W4 /WX)
    # Set the subsystem to Windows
    target_link_options(beatograph PRIVATE /SUBSYSTEM:WINDOWS)

    # Include the source directory
    target_include_directories(beatograph PRIVATE "src")
endif()

# Add the test executable target
add_executable(beatograph_test 
  src/pch.cpp
  src/beatograph_test.cpp
  src/metrics/metrics_parser.cpp
)

# Precompiled headers for Windows/MSVC for the test executable
if (MSVC)
    # Specify the precompiled header file for beatograph_test
    target_precompile_headers(beatograph_test PRIVATE "src/pch.h")
    
    # Set warnings and other options
    target_compile_options(beatograph_test PRIVATE /W4 /WX)  # Treat warnings as errors

    # Include the source directory
    target_include_directories(beatograph_test PRIVATE "src")
endif()

# Find and link GoogleTest (for tests)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(beatograph_test PRIVATE GTest::gtest GTest::gtest_main)

# Include GoogleTest
include(GoogleTest)

# Find and link libcurl
find_package(CURL REQUIRED)

# Find and link SDL2 and SDL2_image
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# Find and link imgui
find_package(imgui REQUIRED)

# Find and link nlohmann_json
find_package(nlohmann_json CONFIG REQUIRED)

# Find and link GLEW
find_package(GLEW REQUIRED)

# Link external libraries to beatograph
target_link_libraries(beatograph PRIVATE 
  CURL::libcurl 
  SDL2::SDL2 SDL2_image::SDL2_image 
  GLEW::GLEW
  imgui::imgui
  nlohmann_json::nlohmann_json
)

# Link external libraries to beatograph_test
target_link_libraries(beatograph_test PRIVATE 
  CURL::libcurl 
  SDL2::SDL2 SDL2_image::SDL2_image 
  GLEW::GLEW
  imgui::imgui
  nlohmann_json::nlohmann_json
)
