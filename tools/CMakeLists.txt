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

install(TARGETS egoboo-content-validator
        RUNTIME
        DESTINATION bin
        COMPONENT applications)
