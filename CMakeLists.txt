cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(Beatograph VERSION 1.0 LANGUAGES CXX)
# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Add your source files
add_executable(beatograph src/beatograph.cpp)
set_target_properties(beatograph PROPERTIES COMPILE_FLAGS "-g")

# Link gtest and gtest_main
add_executable(beatograph_test src/beatograph_test.cpp)
# Compile tests with debug symbols
set_target_properties(beatograph_test PROPERTIES COMPILE_FLAGS "-g")
target_link_libraries(beatograph_test gtest_main)

include(GoogleTest)
gtest_discover_tests(beatograph_test)

# Find and link libcurl
find_package(CURL REQUIRED)
target_link_libraries(beatograph CURL::libcurl)

# Find and link SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
target_link_libraries(beatograph ${SDL2_LIBRARIES})

