set(LANE_FIXTURE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

function(lane_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE lane-core Qt6::Test)
    target_compile_definitions(${name} PRIVATE LANE_FIXTURE_ROOT="${LANE_FIXTURE_ROOT}")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

lane_test(test_url)
lane_test(test_matcher)
lane_test(test_pipeline)
lane_test(test_discovery)
lane_test(test_discovery_mac)
lane_test(test_profileroots)
lane_test(test_router)
lane_test(test_config)
lane_test(test_launcher)
lane_test(test_launch_mac)
lane_test(test_destination)
lane_test(test_version)
lane_test(test_updatechecker)
lane_test(test_unshorten)
lane_test(test_linux_handler)

# PickerModel lives in the lane executable, not lane-core, so its test
# compiles the source directly.
add_executable(test_pickermodel test_pickermodel.cpp ${CMAKE_SOURCE_DIR}/src/app/PickerModel.cpp)
target_link_libraries(test_pickermodel PRIVATE lane-core Qt6::Test)
add_test(NAME test_pickermodel COMMAND test_pickermodel)

# ClickGate is app-layer ordering logic with no event loop or window, so
# its test compiles the source directly like PickerModel does.
add_executable(test_clickgate test_clickgate.cpp ${CMAKE_SOURCE_DIR}/src/app/ClickGate.cpp)
target_link_libraries(test_clickgate PRIVATE lane-core Qt6::Test)
add_test(NAME test_clickgate COMMAND test_clickgate)

# DefaultBrowserWatcher lives in the lane executable, not lane-core, so
# its test compiles the source directly.
add_executable(test_watchdog test_watchdog.cpp ${CMAKE_SOURCE_DIR}/src/app/DefaultBrowserWatcher.cpp)
target_link_libraries(test_watchdog PRIVATE lane-core Qt6::Test)
add_test(NAME test_watchdog COMMAND test_watchdog)

# SingleInstance lives in the lane executable, so its test compiles the
# source directly. It is cross-platform (XDG_RUNTIME_DIR on Linux,
# confstr temp dir on macOS).
if(LANE_KDE_DESKTOP OR LANE_MAC_DESKTOP)
    add_executable(test_singleinstance test_singleinstance.cpp ${CMAKE_SOURCE_DIR}/src/app/SingleInstance.cpp)
    target_link_libraries(test_singleinstance PRIVATE lane-core Qt6::Test Qt6::Network)
    add_test(NAME test_singleinstance COMMAND test_singleinstance)

    # UrlEventFilter is a QObject-only filter; the test feeds it fake
    # QFileOpenEvents so it runs on Linux CI too.
    add_executable(test_urlevents test_urlevents.cpp ${CMAKE_SOURCE_DIR}/src/app/UrlEventFilter.cpp)
    target_link_libraries(test_urlevents PRIVATE lane-core Qt6::Test Qt6::Gui)
    add_test(NAME test_urlevents COMMAND test_urlevents)

    add_executable(test_notifymap test_notifymap.cpp)
    target_link_libraries(test_notifymap PRIVATE lane-core Qt6::Test)
    add_test(NAME test_notifymap COMMAND test_notifymap)

    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS QuickTest)
    add_executable(test_qml test_qml.cpp)
    target_link_libraries(test_qml PRIVATE laneuiplugin Qt6::QuickTest)
    target_compile_definitions(test_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME test_qml COMMAND test_qml)
    set_tests_properties(test_qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion")

    # Controller lives in lane-app. This test constructs a real one under
    # offscreen Qt. The app.lane module is compiled into the lane
    # executable, not lane-app, so the test embeds the same QML or
    # Settings never loads (the build-tree qmldir prefers a qrc path
    # this binary does not have).
    add_executable(test_firstrun test_firstrun.cpp)
    target_link_libraries(test_firstrun PRIVATE lane-app laneuiplugin Qt6::Test)
    set(_firstrun_qml
        ../src/qml/Picker.qml
        ../src/qml/Hold.qml
        ../src/qml/Settings.qml
        ../src/qml/pages/OverviewPage.qml
        ../src/qml/pages/TargetsPage.qml
        ../src/qml/pages/RulesPage.qml
        ../src/qml/pages/PreferencesPage.qml
    )
    foreach(_qml ${_firstrun_qml})
        get_filename_component(_alias ${_qml} NAME)
        set_source_files_properties(${_qml} PROPERTIES QT_RESOURCE_ALIAS ${_alias})
    endforeach()
    qt_add_qml_module(test_firstrun
        URI app.lane
        VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        QML_FILES ${_firstrun_qml}
    )
    if(LANE_KDE_DESKTOP)
        target_link_libraries(test_firstrun PRIVATE Qt6::DBus LayerShellQt::Interface)
    endif()
    if(LANE_MAC_DESKTOP)
        target_link_libraries(test_firstrun PRIVATE
            "-framework AppKit"
            "-framework UserNotifications"
            "-framework ServiceManagement"
            "-framework CoreServices")
    endif()
    add_test(NAME test_firstrun COMMAND test_firstrun)
    set_tests_properties(test_firstrun PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion")

    # Same shape as test_firstrun: embed the full app.lane module so
    # loadFromModule can resolve every type, then drive the picker.
    add_executable(test_pickerhints test_pickerhints.cpp)
    target_link_libraries(test_pickerhints PRIVATE lane-app laneuiplugin Qt6::Test)
    set(_pickerhints_qml ${_firstrun_qml})
    foreach(_qml ${_pickerhints_qml})
        get_filename_component(_alias ${_qml} NAME)
        set_source_files_properties(${_qml} PROPERTIES QT_RESOURCE_ALIAS ${_alias})
    endforeach()
    qt_add_qml_module(test_pickerhints
        URI app.lane
        VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        OUTPUT_DIRECTORY pickerhints/app/lane
        QML_FILES ${_pickerhints_qml}
    )
    if(LANE_KDE_DESKTOP)
        target_link_libraries(test_pickerhints PRIVATE Qt6::DBus LayerShellQt::Interface)
    endif()
    if(LANE_MAC_DESKTOP)
        target_link_libraries(test_pickerhints PRIVATE
            "-framework AppKit"
            "-framework UserNotifications"
            "-framework ServiceManagement"
            "-framework CoreServices")
    endif()
    add_test(NAME test_pickerhints COMMAND test_pickerhints)
    set_tests_properties(test_pickerhints PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion")
endif()
