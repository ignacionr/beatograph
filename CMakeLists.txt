cmake_minimum_required(VERSION 3.30)

project(beatograph VERSION 1.0 LANGUAGES CXX C)

# Remove the Windows-specific include and lib paths
# target_include_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\include\\cairo")
# target_link_directories(beatograph PRIVATE "build\\vcpkg_installed\\x64-windows\\lib\\")

# Set the C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find and link Linux libraries (using system's package manager or custom paths)
find_package(FFMPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GLEW REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tinyxml2 REQUIRED)
find_package(libssh REQUIRED)

# Manually specify the paths for imgui if find_package fails
find_path(IMGUI_INCLUDE_DIR imgui.h PATHS /usr/include /usr/local/include /usr/local/include/imgui)
set(IMGUI_LIBRARY /usr/lib/x86_64-linux-gnu/libimgui.a)

if (IMGUI_INCLUDE_DIR AND IMGUI_LIBRARY)
  message(STATUS "Found imgui: ${IMGUI_LIBRARY}")
  add_library(imgui::imgui UNKNOWN IMPORTED)
  set_target_properties(imgui::imgui PROPERTIES
    IMPORTED_LOCATION "${IMGUI_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${IMGUI_INCLUDE_DIR}/imgui"
  )
else()
  if (NOT IMGUI_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find imgui header file (imgui.h). Please install it or specify its location.")
  elseif (NOT IMGUI_LIBRARY)
    message(FATAL_ERROR "Could not find imgui library file (libimgui.a). Please install it or specify its location.")
  endif()
endif()

# Add compile definition for GLEW_NO_GLU
add_compile_definitions(GLEW_NO_GLU)

add_executable(beatograph 
  external/sqlite3.c
  src/cloud/metrics/metric_view.cpp
  src/beatograph.cpp
  src/cloud/metrics/metric_view_config_screen.cpp
  src/cloud/metrics/metrics_menu.cpp
  src/cloud/metrics/metrics_parser.cpp
)

# Adjust linking for system libraries
target_link_libraries(beatograph PRIVATE
  CURL::libcurl 
  SDL2::SDL2
  SDL2_image::SDL2_image
  GLEW::GLEW
  imgui::imgui
  nlohmann_json::nlohmann_json
  ${PNG_LIBRARY}
  ZLIB::ZLIB
  tinyxml2::tinyxml2
  ${FFMPEG_LIBRARIES}
  ssh
)
