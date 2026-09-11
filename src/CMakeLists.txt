set(tern_core_SRCS
    core/urlutil.cpp
    core/matcher.cpp
    core/pipeline.cpp
    core/config.cpp
    core/discovery.cpp
    core/destination.cpp
    core/router.cpp
    core/launcher.cpp
    core/unshorten.cpp
)

add_library(tern-core STATIC ${tern_core_SRCS})
target_include_directories(tern-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tern-core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
)

set(tern_SRCS
    app/main.cpp
    app/Controller.cpp
    app/PickerModel.cpp
    app/TargetModel.cpp
    app/RuleModel.cpp
    app/Autostart.cpp
    app/SourceInfo.cpp
)

qt_add_executable(tern ${tern_SRCS})
target_include_directories(tern PRIVATE ${CMAKE_BINARY_DIR})

qt_add_qml_module(tern
    URI app.tern
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

target_link_libraries(tern
    PRIVATE
        tern-core
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

install(TARGETS tern ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
