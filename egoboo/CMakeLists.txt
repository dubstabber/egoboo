# Minimum required CMake version.
cmake_minimum_required(VERSION 3.10)

# Project name and programming language.
project(egoboo CXX)
message("building Egoboo Executable")

set_project_default_properties()

set(SOURCE_FILES "")

# Include directories for project.
include_directories(${PROJECT_SOURCE_DIR}/src)

# Enumerate cpp and c files.
file(GLOB_RECURSE CPP_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
list(APPEND SOURCE_FILES ${CPP_FILES})

file(GLOB_RECURSE C_FILES ${PROJECT_SOURCE_DIR}/src/*.c)
SET_SOURCE_FILES_PROPERTIES(${C_FILES} PROPERTIES LANGUAGE CXX)
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
add_executable(egoboo ${SOURCE_FILES})

# Link libraries.
# egolib-gamestates provides egolib-library + lower transitively; egolib-scriptvm (its sibling above
# library) provides the EgoScript VM, which Main.cpp installs via Ego::Script::installDefaultScriptSystem().
target_link_libraries(egoboo egolib-gamestates egolib-scriptvm)

# Copy and install runtime dependencies (e.g. SDL2 DLLs) of idlib-game-engine-library with the executable.
if (WIN32)
  egoboo_stage_windows_runtime_libraries(egoboo idlib-game-engine-library)
endif()

IF(DOXYGEN_FOUND)
    ADD_CUSTOM_TARGET(egoboo-doc ${DOXYGEN_EXECUTABLE} COMMENT "Building documentation")
ENDIF(DOXYGEN_FOUND)

install(TARGETS egoboo
        RUNTIME
        DESTINATION bin
        COMPONENT applications)
