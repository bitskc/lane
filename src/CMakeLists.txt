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

# Core app sources shared by every lane executable (Linux daemon and macOS
# app bundle). None of these touch D-Bus or KDE directly; the OS-specific
# bits live in platform_*.cpp via the Platform seam.
set(LANE_APP_SRCS
    app/Controller.cpp
    app/PickerModel.cpp
    app/ClickGate.cpp
    app/TargetModel.cpp
    app/RuleModel.cpp
    app/SourceInfo.cpp
    app/UpdateChecker.cpp
    app/ThemeIconProvider.cpp
    app/ThemeIconProvider.h
    app/SingleInstance.cpp
    app/UrlEventFilter.cpp
    app/CrashLog.cpp
    app/DefaultBrowserWatcher.cpp   # safe on macOS: only enabled when
                                    # Platform::watchesDefaultHandlerByFile()
                                    # is true (Linux only; see Controller)
)
if(LANE_KDE_DESKTOP)
    list(APPEND LANE_APP_SRCS
        app/Autostart.cpp
        app/DBusActivation.cpp
        app/platform/platform_linux.cpp
    )
elseif(LANE_MAC_DESKTOP)
    list(APPEND LANE_APP_SRCS
        app/platform/platform_macos.mm
    )
endif()

add_library(lane-app STATIC ${LANE_APP_SRCS})
target_include_directories(lane-app PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(lane-app PUBLIC lane-core Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Widgets)
if(LANE_KDE_DESKTOP)
    target_link_libraries(lane-app PRIVATE Qt6::DBus)
endif()

if(LANE_KDE_DESKTOP OR LANE_MAC_DESKTOP)

    # UI module: local FormCard/Kirigami replacements, no KDE QML at
    # runtime. Static target so the app links it without a separate
    # plugin; qt_add_qml_module generates the qmldir for URI app.lane.ui.
    set(laneui_SRCS
        qml/ui/Theme.qml
        qml/ui/ScrollableFormPage.qml
        qml/ui/FormCard.qml
        qml/ui/FormHeader.qml
        qml/ui/FormDelegateSeparator.qml
        qml/ui/ThemedShadow.qml
        qml/ui/LaneIcon.qml
        qml/ui/DragHandle.qml
        qml/ui/AbstractFormDelegate.qml
        qml/ui/FormSwitchDelegate.qml
        qml/ui/FormComboBoxDelegate.qml
        qml/ui/FormButtonDelegate.qml
        qml/ui/FormTextDelegate.qml
        qml/ui/FormTextFieldDelegate.qml
        qml/ui/FormSpinBoxDelegate.qml
        qml/ui/FormRadioDelegate.qml
        qml/ui/FormSectionText.qml
    )
    # pragma Singleton alone is not enough: the generated qmldir only
    # exports Theme as a singleton when this source property is set,
    # and it must be set before qt_add_qml_module consumes the file list.
    set_source_files_properties(qml/ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    qt_add_library(laneui STATIC)
    qt_add_qml_module(laneui
        URI app.lane.ui
        VERSION 1.0
        CLASS_NAME LaneUiPlugin
        PLUGIN_TARGET laneuiplugin
        QML_FILES ${laneui_SRCS}
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/app/lane/ui
        RESOURCE_PREFIX /qt/qml
    )
    target_link_libraries(laneui PUBLIC Qt6::Quick Qt6::QuickControls2)
endif()

if(LANE_KDE_DESKTOP)
    set(lane_SRCS
        app/main.cpp
    )

    qt_add_executable(lane ${lane_SRCS})
    target_include_directories(lane PRIVATE ${CMAKE_BINARY_DIR})
    target_link_libraries(lane PRIVATE lane-app laneuiplugin)

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

    # Fusion style defaults: zero KDE QML runtime imports anywhere.
    qt_add_resources(lane "qtquickcontrols2conf"
        PREFIX "/"
        FILES qtquickcontrols2.conf
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
            LayerShellQt::Interface
    )

    if(KF6WindowSystem_FOUND)
        target_link_libraries(lane PRIVATE KF6::WindowSystem)
        target_compile_definitions(lane PRIVATE HAVE_KWAYLANDEXTRAS)
    endif()

    if(PlasmaActivities_FOUND)
        target_link_libraries(lane PRIVATE Plasma::Activities)
        target_compile_definitions(lane PRIVATE HAVE_PLASMA_ACTIVITIES)
    endif()

    install(TARGETS lane ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
elseif(LANE_MAC_DESKTOP)
    set(lane_SRCS
        app/main.cpp
    )

    qt_add_executable(lane MACOSX_BUNDLE ${lane_SRCS})
    target_include_directories(lane PRIVATE ${CMAKE_BINARY_DIR})
    target_link_libraries(lane PRIVATE lane-app laneuiplugin)

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

    qt_add_resources(lane "appresources"
        PREFIX "/"
        FILES qtquickcontrols2.conf
    )
    qt_add_resources(lane "icons"
        PREFIX "/icons"
        BASE "${CMAKE_SOURCE_DIR}/data/icons"
        FILES "${CMAKE_SOURCE_DIR}/data/icons/app.lane.Lane.svg"
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
            Qt6::Network
            Qt6::Svg
            "-framework AppKit"
            "-framework UserNotifications"
            "-framework ServiceManagement"
            "-framework CoreServices"
    )

    set_target_properties(lane PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Lane"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/data/mac/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "app.lane.Lane"
    )
    target_compile_definitions(lane PRIVATE LANE_MACOS_BUNDLE=1)
endif()
