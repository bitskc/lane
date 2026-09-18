cmake_minimum_required(VERSION 3.22)
project(lane VERSION 0.2.0 LANGUAGES CXX)

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

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMFindQmlModule)
include(CTest)

# KDECompilerSettings injects -Werror=...; keep the warnings, drop the errors.
string(REPLACE "-Werror=" "-W" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-Werror=" "-W" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
    DBus
    Network
    Test
    Svg
)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    CoreAddons
    Config
    DBusAddons
    Notifications
    WindowSystem
    IconThemes
    ColorScheme
    StatusNotifierItem
    Crash
)
ecm_find_qmlmodule(org.kde.kirigami REQUIRED)
ecm_find_qmlmodule(org.kde.kirigamiaddons.formcard REQUIRED)
find_package(LayerShellQt REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

ecm_setup_version(PROJECT
    VARIABLE_PREFIX LANE
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lane_version.h"
)

add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(LANE_BIN "${CMAKE_INSTALL_FULL_BINDIR}/lane")
configure_file(data/app.lane.Lane.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.desktop @ONLY)
configure_file(data/dbus/app.lane.Lane.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.service @ONLY)
configure_file(data/systemd/lane.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/lane.service @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES data/app.lane.Lane.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES data/app.lane.Lane.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/app.lane.Lane.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lane.service DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)
install(FILES data/icons/app.lane.Lane.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
