# Minimum required CMake version.
cmake_minimum_required(VERSION 3.10)

project(egoboo)

enable_testing()

if (CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    set(EGOBOO_ENABLE_RUNNABLE_TESTS OFF)
else()
    set(EGOBOO_ENABLE_RUNNABLE_TESTS ON)
endif()

# Set path to Idlib.
# The superproject provides this explicitly so idlib-game-engine does not need
# to initialize its nested idlib checkout for normal Egoboo builds.
set(IDLIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/idlib)

# Set path to Idlib: Game Engine.
set(IDLIB_GAME_ENGINE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/idlib-game-engine)

# Add Idlib.
set(idlib-with-fetch-googletest OFF CACHE BOOL
    "enable/disable fetching of google test. ON enables the content fetch, OFF disables compilation-time network fetch. Initial value is OFF.")
set(idlib-with-tests ${EGOBOO_ENABLE_RUNNABLE_TESTS} CACHE BOOL
    "enable/disable compilation of tests. ON enables compilation of tests, OFF disables compilation of tests. Initial value is OFF." FORCE)
set(idlib-with-demos OFF CACHE BOOL
    "enable/disable compilation of demos. ON enables compilation of demos, OFF disables compilation of demos. Initial value is OFF." FORCE)
set(idlib-with-documentation OFF CACHE BOOL
    "enable/disable compilation of documentation. ON enables compilation of documentation, OFF disables compilation of documentation. Initial value is OFF." FORCE)
add_subdirectory(${IDLIB_PATH})

# Add Idlib: Game Engine.
set(idlib-game-engine-with-tests ${EGOBOO_ENABLE_RUNNABLE_TESTS} CACHE BOOL
    "enable/disable compilation of tests. ON enables compilation of tests, OFF disables compilation of tests. Initial value is OFF." FORCE)
set(idlib-game-engine-with-demos OFF CACHE BOOL
    "enable/disable compilation of demos. ON enables compilation of demos, OFF disables compilation of demos. Initial value is OFF." FORCE)
add_subdirectory(${IDLIB_GAME_ENGINE_PATH})

include(${IDLIB_PATH}/buildsystem/set_project_default_properties.cmake)
include(${IDLIB_GAME_ENGINE_PATH}/buildsystem/download_and_unpack.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EgobooWindowsSupport.cmake)

# Add Egolib.
add_subdirectory(egolib)

# Add Egoboo.
add_subdirectory(egoboo)

# Add Cartman (legacy map editor; gated by EGOBOO_BUILD_CARTMAN, OFF by default).
add_subdirectory(cartman)

# Add repository tools.
add_subdirectory(tools)

# This must always be last!
include(CPack)
