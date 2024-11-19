cmake_minimum_required(VERSION 3.16)

# Set the path to vcpkg toolchain file
# set(CMAKE_TOOLCHAIN_FILE "E:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
# Enable CMake's find_package to use vcpkg
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
include("/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Set the project name and version
project(Beatograph VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
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
  assets/assets.rc
)

# Precompiled headers for Windows/MSVC
if (MSVC)
    target_precompile_headers(beatograph PRIVATE "src/pch.h")
    target_compile_options(beatograph PRIVATE /W4 /WX /MP /Zi /Od /bigobj)
    target_link_options(beatograph PRIVATE /SUBSYSTEM:WINDOWS /INCREMENTAL)
endif()

# Include directories
target_include_directories(beatograph PRIVATE "src" ${UTF8H_INCLUDE_DIRS} external)

target_link_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\lib\\")

# Link external libraries to beatograph
target_link_libraries(beatograph PRIVATE 
  CURL::libcurl 
  SDL2::SDL2 SDL2_image::SDL2_image 
  GLEW::GLEW
  imgui::imgui
  nlohmann_json::nlohmann_json
  cpr::cpr
  PNG::PNG
  ssh
  ${FFMPEG_LIBRARIES} avformat
  tinyfiledialogs::tinyfiledialogs
  tinyxml2
)

# Enable Unity builds
set_target_properties(beatograph PROPERTIES UNITY_BUILD ON)

# Add the test executable target
# add_executable(beatograph_test 
#   src/pch.cpp
#   src/beatograph_test.cpp
#   src/metrics/metrics_parser.cpp
# )

# target_link_directories(beatograph_test PRIVATE "build\\vcpkg_installed\\x64-windows\\lib\\")

# # Precompiled headers for Windows/MSVC for the test executable
# if (MSVC)
#     target_precompile_headers(beatograph_test PRIVATE "src/pch.h")
#     target_compile_options(beatograph_test PRIVATE /W4 /WX /MP /Zi /Od)
# endif()

# # Include directories for test executable
# target_include_directories(beatograph_test PRIVATE "src" external)

# # Link external libraries to beatograph_test
# target_link_libraries(beatograph_test PRIVATE 
#   CURL::libcurl 
#   SDL2::SDL2 SDL2_image::SDL2_image 
#   GLEW::GLEW
#   imgui::imgui
#   nlohmann_json::nlohmann_json
#   ${FFMPEG_LIBRARIES} avformat
#   GTest::gtest GTest::gtest_main
# )

# Include GoogleTest
include(GoogleTest)

# Find and link required packages
list(APPEND CMAKE_PREFIX_PATH "vcpkg_installed/x64-windows/share")
find_package(GTest CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(imgui REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(libssh CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_path(UTF8H_INCLUDE_DIRS "utf8h/utf8.h")
find_package(TinyXML2 REQUIRED)
