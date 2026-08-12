foreach(_required QTM_APPLICATION QTM_EXPECTED_OUTPUT)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${QTM_APPLICATION}" --version
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "QTierMaker --version failed (${_result}): ${_error}")
endif()
if(NOT _output STREQUAL QTM_EXPECTED_OUTPUT)
    message(FATAL_ERROR
        "Application version mismatch: expected '${QTM_EXPECTED_OUTPUT}', got '${_output}'")
endif()
