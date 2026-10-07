cmake_minimum_required(VERSION 3.16)

project(OpenAULA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Threads REQUIRED)


include_directories(
    core
)


# shared core, linked into daemon + bridge
add_library(openaula-core STATIC

    core/AulaKeyboard.cpp
    core/AulaController.cpp
    core/protocol.cpp
    core/effects.cpp
    core/Color.cpp
    core/KeyboardLayout.cpp
    core/LightingEngine.cpp
    core/AppState.cpp
    core/StateFormat.cpp
    core/ProfileStore.cpp
    core/RemapConfig.cpp
    core/KeyCodes.cpp
    core/CalibrationSession.cpp
    core/SystemMonitor.cpp
)

target_link_libraries(
    openaula-core
    hidapi-hidraw
    Threads::Threads
)


# lighting daemon
add_executable(openaula-daemon

    daemon/main.cpp
    daemon/KeyWatcher.cpp
)

target_link_libraries(
    openaula-daemon
    openaula-core
)


# remap/macro engine, only if libevdev is there
find_package(PkgConfig)

if(PkgConfig_FOUND)
    pkg_check_modules(LIBEVDEV libevdev)
endif()

if(LIBEVDEV_FOUND)
    add_executable(openaula-remapd

        daemon/remapd_main.cpp
        daemon/RemapEngine.cpp
    )

    target_include_directories(openaula-remapd PRIVATE ${LIBEVDEV_INCLUDE_DIRS})
    target_link_libraries(openaula-remapd openaula-core ${LIBEVDEV_LIBRARIES})
    target_compile_options(openaula-remapd PRIVATE ${LIBEVDEV_CFLAGS_OTHER})
else()
    message(STATUS "libevdev not found - skipping openaula-remapd (key remap/macro engine). Lighting control is unaffected.")
endif()


# http bridge + web ui
add_executable(openaula-webd

    bridge/main.cpp
    bridge/HttpServer.cpp
    bridge/Json.cpp
)

target_link_libraries(
    openaula-webd
    openaula-core
    Threads::Threads
)
