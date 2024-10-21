cmake_minimum_required(VERSION 3.30)

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

add_executable(beatograph 
  src/metrics/metric_view.cpp
  src/beatograph.cpp
  src/metrics/metric_view_config_screen.cpp
  src/metrics/metrics_menu.cpp
  src/metrics/metrics_parser.cpp
)

set_target_properties(beatograph PROPERTIES CXX_STANDARD 20)
# Enable all possible warnings for cl.exe
if (MSVC)
  target_compile_options(beatograph PRIVATE /W4 /WX)
endif()
# Build to Windows Subsystem on cl.exe
set_target_properties(beatograph PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

# Link gtest and gtest_main
add_executable(beatograph_test 
  src/beatograph_test.cpp
  src/metrics/metrics_parser.cpp
  )

# Compile tests with debug symbols

target_link_libraries(beatograph_test PRIVATE GTest::gtest GTest::gtest_main)

include(GoogleTest)
find_package(GTest CONFIG REQUIRED)

# Find and link libcurl
find_package(CURL REQUIRED)

# Find and link SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

find_package(imgui REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GLEW REQUIRED)

target_link_libraries(beatograph PRIVATE 
  CURL::libcurl 
  SDL2::SDL2 SDL2_image::SDL2_image 
  GLEW::GLEW
  imgui::imgui
  nlohmann_json::nlohmann_json
  )
