if(NOT DEFINED RESET_SCRIPT OR NOT EXISTS "${RESET_SCRIPT}")
    message(FATAL_ERROR "RESET_SCRIPT must name the configured AIO reset policy")
endif()
if(NOT DEFINED INSTALL_SCRIPT OR NOT EXISTS "${INSTALL_SCRIPT}")
    message(FATAL_ERROR "INSTALL_SCRIPT must name the generated install script")
endif()
if(NOT DEFINED TEST_ROOT OR TEST_ROOT STREQUAL "")
    message(FATAL_ERROR "TEST_ROOT is required")
endif()

function(run_reset EXPECTED_PREFIX ACTUAL_PREFIX COMPONENT EXPECTED_RESULT)
    file(MAKE_DIRECTORY "${ACTUAL_PREFIX}")
    set(SENTINEL "${ACTUAL_PREFIX}/preserve-me.txt")
    file(WRITE "${SENTINEL}" "sentinel")

    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            "-DCSX_AIO_EXPECTED_PREFIX=${EXPECTED_PREFIX}"
            "-DCMAKE_INSTALL_PREFIX=${ACTUAL_PREFIX}"
            "-DCMAKE_INSTALL_COMPONENT=${COMPONENT}"
            -P "${RESET_SCRIPT}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR_OUTPUT
    )

    if(EXPECTED_RESULT STREQUAL "reset")
        if(NOT RESULT EQUAL 0 OR EXISTS "${ACTUAL_PREFIX}")
            message(FATAL_ERROR "Full install did not reset staging: ${OUTPUT}${ERROR_OUTPUT}")
        endif()
    elseif(EXPECTED_RESULT STREQUAL "preserve")
        if(NOT RESULT EQUAL 0 OR NOT EXISTS "${SENTINEL}")
            message(FATAL_ERROR "Component install changed staging: ${OUTPUT}${ERROR_OUTPUT}")
        endif()
    elseif(EXPECTED_RESULT STREQUAL "reject")
        if(RESULT EQUAL 0 OR NOT EXISTS "${SENTINEL}")
            message(FATAL_ERROR "Unsafe full install was not rejected safely: ${OUTPUT}${ERROR_OUTPUT}")
        endif()
    else()
        message(FATAL_ERROR "Unknown expected result: ${EXPECTED_RESULT}")
    endif()
endfunction()

file(REMOVE_RECURSE "${TEST_ROOT}")
set(EXPECTED_PREFIX "${TEST_ROOT}/expected staging")
run_reset("${EXPECTED_PREFIX}" "${EXPECTED_PREFIX}" "Unspecified" "preserve")
run_reset("${EXPECTED_PREFIX}" "${EXPECTED_PREFIX}" "SKSE" "preserve")
run_reset("${EXPECTED_PREFIX}" "${TEST_ROOT}/unsafe target" "" "reject")
run_reset("${EXPECTED_PREFIX}" "${EXPECTED_PREFIX}" "" "reset")

# Exercise CMake's generated default-component gate as well as the helper.
file(MAKE_DIRECTORY "${EXPECTED_PREFIX}")
set(GENERATED_SENTINEL "${EXPECTED_PREFIX}/generated-preserve-me.txt")
file(WRITE "${GENERATED_SENTINEL}" "sentinel")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" "-DCMAKE_INSTALL_COMPONENT=Unspecified"
        "-DCMAKE_INSTALL_PREFIX=${EXPECTED_PREFIX}" -P "${INSTALL_SCRIPT}"
    RESULT_VARIABLE GENERATED_RESULT
    OUTPUT_VARIABLE GENERATED_OUTPUT
    ERROR_VARIABLE GENERATED_ERROR
)
if(NOT GENERATED_RESULT EQUAL 0 OR NOT EXISTS "${GENERATED_SENTINEL}")
    message(
        FATAL_ERROR
        "Generated default-component install reset staging: "
        "${GENERATED_OUTPUT}${GENERATED_ERROR}"
    )
endif()
file(REMOVE_RECURSE "${TEST_ROOT}")
