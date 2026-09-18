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
