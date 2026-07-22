# Cartman is the legacy in-tree map editor. It is heavily bit-rotted against the
# modern egolib API (last meaningful change 2017-11-29) and is being ported
# incrementally -- see roadmap T3.5 and
# refactoring-documents/73-cartman-build-integration-scouting.md.
#
# It is OFF by default so the standard build is unaffected until the port both
# builds and launches reliably. Enable with -DEGOBOO_BUILD_CARTMAN=ON.
option(EGOBOO_BUILD_CARTMAN "Build the Cartman map editor (legacy, port in progress)" OFF)

if(NOT EGOBOO_BUILD_CARTMAN)
    return()
endif()

# Project name and programming language.
project(cartman CXX)
message("building Cartman map editor (legacy, port in progress)")

set_project_default_properties()

set(SOURCE_FILES "")

# Include directories for project.
include_directories(${PROJECT_SOURCE_DIR}/src)

# Enumerate cpp and c files.
file(GLOB_RECURSE CPP_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
list(APPEND SOURCE_FILES ${CPP_FILES})

file(GLOB_RECURSE C_FILES ${PROJECT_SOURCE_DIR}/src/*.c)
set_source_files_properties(${C_FILES} PROPERTIES LANGUAGE CXX)
list(APPEND SOURCE_FILES ${C_FILES})

# Enumerate hpp and h files.
file(GLOB_RECURSE HPP_FILES ${PROJECT_SOURCE_DIR}/src/*.hpp)
set_source_files_properties(${HPP_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties(${HPP_FILES} PROPERTIES LANGUAGE CXX)
list(APPEND SOURCE_FILES ${HPP_FILES})

file(GLOB_RECURSE H_FILES ${PROJECT_SOURCE_DIR}/src/*.h)
set_source_files_properties(${H_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties(${H_FILES} PROPERTIES LANGUAGE CXX)
list(APPEND SOURCE_FILES ${H_FILES})

# Define product.
add_executable(cartman ${SOURCE_FILES})

# Link libraries. Cartman needs egolib-game-graphics for the Ego::App/AppImpl
# application base (relocated there from egolib-library); it transitively
# provides egolib-library, SDL2, idlib, GLEW, PhysFS and OpenGL.
target_link_libraries(cartman egolib-game-graphics)

# Copy and install runtime dependencies (e.g. SDL2 DLLs) with the executable.
if (WIN32)
    egoboo_stage_windows_runtime_libraries(cartman idlib-game-engine-library)
endif()

install(TARGETS cartman
        RUNTIME
        DESTINATION bin
        COMPONENT applications)
