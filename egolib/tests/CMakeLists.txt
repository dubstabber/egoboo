# Minimum required CMake version.
cmake_minimum_required (VERSION 3.8)
# Project name and settings.
project(egolib-tests CXX)
message("building Egolib Tests Executable")
set_project_default_properties()

# Add include directory of Google Test.
#include_directories("${IDLIB_PATH}/external/googletest/googletest/include")

# Include directory locations.
include_directories(${PROJECT_SOURCE_DIR}/../library/src)
include_directories(${PROJECT_SOURCE_DIR})

# Build a list of all tests.
file(GLOB_RECURSE test_files CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/egolib/tests/*.cpp)

add_executable(egolib-tests-executable ${test_files})
# egolib-gamestates provides egolib-library + lower transitively; egolib-scriptvm (its sibling above
# library) provides the EgoScript VM — the global ScriptSystemEnvironment installs the adapter and
# ScriptRuntime.cpp calls scr_* directly.
target_link_libraries(egolib-tests-executable egolib-gamestates egolib-scriptvm)
target_link_libraries(egolib-tests-executable GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(egolib-tests-executable)
