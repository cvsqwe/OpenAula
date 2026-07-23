cmake_minimum_required(VERSION 3.16)

project(OpenAULA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Threads REQUIRED)


include_directories(
    core
)


# Shared, Qt-free core: HID protocol, layout data, calibration/state
# persistence, and the lighting animation math. Linked into the daemon
# and the web bridge so they can never drift from each other.
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
)

target_link_libraries(
    openaula-core
    hidapi-hidraw
    Threads::Threads
)


# Standalone background daemon - no Qt dependency at all, so it's cheap to
# run permanently (see daemon/openaula-daemon.service).
add_executable(openaula-daemon

    daemon/main.cpp
)

target_link_libraries(
    openaula-daemon
    openaula-core
)


# Optional key-remap/macro engine (daemon/RemapEngine.h explains why it's
# a separate binary from openaula-daemon). Needs libevdev - skip building
# it rather than failing the whole project if that's not installed, since
# lighting control (everything above) doesn't need it at all.
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


# Bridge between openaula-daemon and a browser: a small Qt-free HTTP server
# (bridge/HttpServer.*) that serves web/ and translates its JSON API into
# the same AppState/ProfileStore file writes + SIGUSR1 nudge the daemon
# reads (see bridge/main.cpp). Never touches the HID device itself, so
# it can run alongside the daemon without fighting it for the connection.
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
