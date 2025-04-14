cmake_minimum_required(VERSION 3.18)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Set the path to vcpkg toolchain file
# set(CMAKE_TOOLCHAIN_FILE "E:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
# Enable CMake's find_package to use vcpkg
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")

# Set the project name and version
project(Beatograph VERSION 1.0 LANGUAGES CXX C)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set default char type to unsigned
add_compile_definitions(CHAR_UNSIGNED)
# add_compile_definitions(IMGUI_USE_WCHAR32)

# Add the main executable target
add_executable(beatograph 
  external/sqlite3.c
  src/pch.cpp
  src/cloud/metrics/metric_view.cpp
  src/beatograph.cpp
  src/cloud/metrics/metric_view_config_screen.cpp
  src/cloud/metrics/metrics_menu.cpp
  src/cloud/metrics/metrics_parser.cpp
  assets/assets.rc
)

# Fetch https://github.com/vmg/sundown.git
include(FetchContent)

FetchContent_Declare(
  sundown
  GIT_REPOSITORY https://github.com/vmg/sundown.git
  GIT_TAG master
)

FetchContent_GetProperties(sundown)
if(NOT sundown_POPULATED)
  FetchContent_Populate(sundown)
endif()

add_library(sundown STATIC
  ${sundown_SOURCE_DIR}/src/autolink.c
  ${sundown_SOURCE_DIR}/src/buffer.c
  ${sundown_SOURCE_DIR}/src/markdown.c
  ${sundown_SOURCE_DIR}/src/stack.c
  ${sundown_SOURCE_DIR}/html/houdini_href_e.c
  ${sundown_SOURCE_DIR}/html/houdini_html_e.c
  ${sundown_SOURCE_DIR}/html/html.c
  ${sundown_SOURCE_DIR}/html/html_smartypants.c
)

target_include_directories(sundown PRIVATE 
  ${sundown_SOURCE_DIR}/src 
  ${sundown_SOURCE_DIR}/html
)

target_include_directories(beatograph PRIVATE ${sundown_SOURCE_DIR}/src ${sundown_SOURCE_DIR}/html)

if (MSVC)
    # target_precompile_headers(beatograph PRIVATE "src/pch.h")
    target_compile_options(beatograph PRIVATE /W4 /WX /MP /Zi /Od /bigobj)
    target_link_options(beatograph PRIVATE /SUBSYSTEM:WINDOWS /INCREMENTAL)
endif()

# Include directories
target_include_directories(beatograph PRIVATE "src" external)
target_include_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\include\\cairo")
target_include_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\include\\gdk-pixbuf-2.0")
target_include_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\include\\glib-2.0")
target_include_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\include\\librsvg-2.0")
target_include_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\lib\\glib-2.0\\include")

target_link_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\lib\\")
target_link_directories(beatograph PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/pkgs/ffmpeg_x64-windows/share/ffmpeg")
find_package(FFMPEG REQUIRED)

# Enable Unity builds
set_target_properties(beatograph PROPERTIES UNITY_BUILD ON)

# Find and link required packages
list(APPEND CMAKE_PREFIX_PATH "vcpkg_installed/x64-windows/share")
find_package(GTest CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/pkgs/zlib_x64-windows/share/zlib")
# Before find_package(ZLIB REQUIRED)
set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
set(ZLIB_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/zlib.lib")
find_package(ZLIB REQUIRED)
# Before find_package(PNG REQUIRED)
set(PNG_PNG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
set(PNG_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/libpng16.lib")
find_package(PNG REQUIRED)
find_package(SDL2 REQUIRED)
# Add these before find_package(SDL2_image REQUIRED)
set(JPEG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
set(JPEG_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/jpeg.lib")
find_package(SDL2_image REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(imgui REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(libssh CONFIG REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(TinyXML2 REQUIRED)

# Link external libraries to beatograph
target_link_libraries(beatograph PRIVATE 
  CURL::libcurl 
  SDL2::SDL2 SDL2_image::SDL2_image 
  GLEW::GLEW
  imgui::imgui
  nlohmann_json::nlohmann_json
  ${PNG_LIBRARY}
  ssh
  ${FFMPEG_LIBRARIES} avformat
  tinyfiledialogs::tinyfiledialogs
  tinyxml2
  sqlite3
  ZLIB::ZLIB
  sundown
)
