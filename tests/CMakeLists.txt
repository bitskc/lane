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
lane_test(test_router)
lane_test(test_config)
lane_test(test_launcher)
lane_test(test_destination)
lane_test(test_version)
lane_test(test_updatechecker)
lane_test(test_unshorten)

# PickerModel lives in the lane executable, not lane-core, so its test
# compiles the source directly.
add_executable(test_pickermodel test_pickermodel.cpp ${CMAKE_SOURCE_DIR}/src/app/PickerModel.cpp)
target_link_libraries(test_pickermodel PRIVATE lane-core Qt6::Test)
add_test(NAME test_pickermodel COMMAND test_pickermodel)

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

    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS QuickTest)
    add_executable(test_qml test_qml.cpp)
    target_link_libraries(test_qml PRIVATE laneuiplugin Qt6::QuickTest)
    target_compile_definitions(test_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    add_test(NAME test_qml COMMAND test_qml)
    set_tests_properties(test_qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Fusion")
endif()
