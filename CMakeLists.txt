cmake_minimum_required(VERSION 3.22)
project(lane VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(QT_MIN_VERSION "6.6.0")
# 6.19 is required for KWaylandExtras::xdgActivationToken(), used by
# Controller to hand off focus to a launched browser on Wayland (see
# app/Controller.cpp). Below 6.19 only the deprecated serial/signal based
# request exists.
set(KF_MIN_VERSION "6.19.0")

# The full app (Kirigami UI, KF6, LayerShellQt, D-Bus, systemd, XDG) is
# Linux/KDE-only for now. On other platforms lane-core and its tests are
# still built - the routing brain is pure Qt and the tests prove it.
# The app target itself returns per-OS as platform backends land.
set(LANE_KDE_DESKTOP OFF)
if(UNIX AND NOT APPLE)
    set(LANE_KDE_DESKTOP ON)
endif()

if(LANE_KDE_DESKTOP)
    find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
    include(ECMSetupVersion)
    include(ECMFindQmlModule)
endif()
include(CTest)

# KDECompilerSettings injects -Werror=...; keep the warnings, drop the errors.
string(REPLACE "-Werror=" "-W" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-Werror=" "-W" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Network
    Test
)
if(LANE_KDE_DESKTOP)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
        Qml
        Quick
        QuickControls2
        Widgets
        DBus
        Svg
    )
    find_package(KF6WindowSystem ${KF_MIN_VERSION} QUIET)
    ecm_find_qmlmodule(org.kde.kirigami REQUIRED)
    ecm_find_qmlmodule(org.kde.kirigamiaddons.formcard REQUIRED)
    find_package(LayerShellQt REQUIRED)
    # Optional: KDE Plasma Activities integration for per-Activity rules and
    # fallback destinations. Absent on non-Plasma desktops and minimal
    # containers; src/CMakeLists.txt then skips Plasma::Activities and the
    # HAVE_PLASMA_ACTIVITIES define, so the build and runtime degrade to
    # exactly today's behavior.
    find_package(PlasmaActivities QUIET)

    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)

    ecm_setup_version(PROJECT
        VARIABLE_PREFIX LANE
        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lane_version.h"
    )
else()
    # No ECM: emit the same lane_version.h ecm_setup_version would.
    set(LANE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(LANE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(LANE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(LANE_VERSION_STRING "${PROJECT_VERSION}")
    math(EXPR LANE_VERSION
        "(${PROJECT_VERSION_MAJOR} << 16) | (${PROJECT_VERSION_MINOR} << 8) | ${PROJECT_VERSION_PATCH}"
        OUTPUT_FORMAT HEXADECIMAL)
    configure_file(cmake/lane_version.h.in
                   "${CMAKE_CURRENT_BINARY_DIR}/lane_version.h" @ONLY)
endif()

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(LANE_KDE_DESKTOP)
    set(LANE_BIN "${CMAKE_INSTALL_FULL_BINDIR}/lane")
    configure_file(data/app.lane.Lane.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.desktop @ONLY)
    configure_file(data/dbus/app.lane.Lane.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.service @ONLY)
    configure_file(data/systemd/lane.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/lane.service @ONLY)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.desktop DESTINATION ${KDE_INSTALL_APPDIR})
    install(FILES data/app.lane.Lane.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lane.service DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)
    install(FILES data/icons/app.lane.Lane.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
endif()
