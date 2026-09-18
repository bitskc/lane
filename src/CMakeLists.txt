set(lane_core_SRCS
    core/urlutil.cpp
    core/matcher.cpp
    core/pipeline.cpp
    core/config.cpp
    core/discovery.cpp
    core/destination.cpp
    core/router.cpp
    core/launcher.cpp
    core/unshorten.cpp
    core/version.cpp
    core/updatemessages.cpp
    core/updatedecision.cpp
)

add_library(lane-core STATIC ${lane_core_SRCS})
target_include_directories(lane-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
target_link_libraries(lane-core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
)

set(lane_SRCS
    app/main.cpp
    app/Controller.cpp
    app/PickerModel.cpp
    app/TargetModel.cpp
    app/RuleModel.cpp
    app/Autostart.cpp
    app/SourceInfo.cpp
    app/UpdateChecker.cpp
)

qt_add_executable(lane ${lane_SRCS})
target_include_directories(lane PRIVATE ${CMAKE_BINARY_DIR})

qt_add_qml_module(lane
    URI app.lane
    VERSION 1.0
    QML_FILES
        qml/Picker.qml
        qml/Hold.qml
        qml/Settings.qml
        qml/pages/OverviewPage.qml
        qml/pages/TargetsPage.qml
        qml/pages/RulesPage.qml
        qml/pages/PreferencesPage.qml
    RESOURCE_PREFIX /qt/qml
)

target_link_libraries(lane
    PRIVATE
        lane-core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        Qt6::DBus
        Qt6::Network
        Qt6::Svg
        KF6::I18n
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::DBusAddons
        KF6::Notifications
        KF6::WindowSystem
        KF6::IconThemes
        KF6::ColorScheme
        KF6::StatusNotifierItem
        KF6::Crash
        LayerShellQt::Interface
)

install(TARGETS lane ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
