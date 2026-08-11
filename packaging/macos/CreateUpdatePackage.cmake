foreach(_required QTM_BUILD_DIR QTM_CONFIG QTM_STAGE_DIR QTM_OUTPUT_FILE)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${QTM_STAGE_DIR}")
file(REMOVE "${QTM_OUTPUT_FILE}")
file(MAKE_DIRECTORY "${QTM_STAGE_DIR}")
get_filename_component(_output_directory "${QTM_OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${QTM_BUILD_DIR}"
        --config "${QTM_CONFIG}" --prefix "${QTM_STAGE_DIR}"
    RESULT_VARIABLE _install_result
)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR "Failed to assemble the macOS update application")
endif()

set(_application "${QTM_STAGE_DIR}/QTierMaker.app")
if(NOT EXISTS "${_application}/Contents/MacOS/QTierMaker")
    message(FATAL_ERROR "The staged QTierMaker.app bundle is incomplete")
endif()
if(NOT EXISTS "${_application}/Contents/Helpers/QTierMakerMacUpdateHelper")
    message(FATAL_ERROR "The staged macOS update helper is missing")
endif()

execute_process(
    COMMAND /usr/bin/codesign --verify --deep --strict "${_application}"
    RESULT_VARIABLE _codesign_result
    ERROR_VARIABLE _codesign_error
)
if(NOT _codesign_result EQUAL 0)
    message(FATAL_ERROR "The staged app signature is invalid: ${_codesign_error}")
endif()

# ditto preserves framework symlinks and executable modes. The app has no resource forks, so avoid
# --sequesterRsrc and the redundant __MACOSX AppleDouble tree it would add to every update archive.
execute_process(
    COMMAND /usr/bin/ditto -c -k --keepParent
        "${_application}" "${QTM_OUTPUT_FILE}"
    RESULT_VARIABLE _archive_result
    ERROR_VARIABLE _archive_error
)
if(NOT _archive_result EQUAL 0 OR NOT EXISTS "${QTM_OUTPUT_FILE}")
    message(FATAL_ERROR "Failed to create the macOS update archive: ${_archive_error}")
endif()

message(STATUS "Created macOS update archive: ${QTM_OUTPUT_FILE}")
