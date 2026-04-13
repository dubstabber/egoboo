# Minimum required CMake version.
cmake_minimum_required(VERSION 3.10)

# Project name and programming language.
project(egolib-library CXX)
message("building Egolib")

add_subdirectory(library)

if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    add_subdirectory(tests)
endif()
