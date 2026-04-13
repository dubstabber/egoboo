# Minimum required CMake version.
cmake_minimum_required(VERSION 3.10)

# Project name and programming language.
project(egoboo-tools CXX)
message("building Egoboo Tools")

set_project_default_properties()

add_executable(egoboo-content-validator
    egoboo-content-validator.cpp)

target_link_libraries(egoboo-content-validator
    egolib-library)

if (WIN32)
    egoboo_stage_windows_runtime_libraries(egoboo-content-validator idlib-game-engine-library)
endif()

install(TARGETS egoboo-content-validator
        RUNTIME
        DESTINATION bin
        COMPONENT applications)
