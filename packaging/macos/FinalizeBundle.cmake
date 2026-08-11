cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED QTM_APPLICATION OR QTM_APPLICATION STREQUAL "")
    set(QTM_APPLICATION "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/QTierMaker.app")
endif()
get_filename_component(QTM_APPLICATION "${QTM_APPLICATION}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(_qtm_contents "${QTM_APPLICATION}/Contents")
set(_qtm_frameworks "${_qtm_contents}/Frameworks")
set(_qtm_plugins "${_qtm_contents}/PlugIns")
set(_qtm_executable "${_qtm_contents}/MacOS/QTierMaker")
set(_qtm_helper "${_qtm_contents}/Helpers/QTierMakerMacUpdateHelper")

foreach(_qtm_required_path IN ITEMS
        "${QTM_APPLICATION}"
        "${_qtm_executable}"
        "${_qtm_helper}")
    if(NOT EXISTS "${_qtm_required_path}")
        message(FATAL_ERROR "The macOS bundle is incomplete: ${_qtm_required_path}")
    endif()
endforeach()

# Keep this list explicit. macdeployqt otherwise copies plugins discovered in the local Qt SDK,
# which can silently add QML, Qt Quick, Virtual Keyboard, and OpenGL to a Widgets-only product.
set(_qtm_allowed_plugins
    "imageformats/libqgif.dylib"
    "imageformats/libqjpeg.dylib"
    "imageformats/libqwebp.dylib"
    "platforms/libqcocoa.dylib"
    "styles/libqmacstyle.dylib"
    "tls/libqsecuretransportbackend.dylib"
)
set(_qtm_allowed_frameworks
    "QtConcurrent.framework"
    "QtCore.framework"
    "QtDBus.framework"
    "QtGui.framework"
    "QtNetwork.framework"
    "QtSvg.framework"
    "QtWidgets.framework"
)

function(_qtm_directory_size_kib directory output_variable)
    execute_process(
        COMMAND /usr/bin/du -sk "${directory}"
        RESULT_VARIABLE _qtm_du_result
        OUTPUT_VARIABLE _qtm_du_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _qtm_du_result EQUAL 0)
        set(${output_variable} "unknown" PARENT_SCOPE)
        return()
    endif()
    string(REGEX MATCH "^[0-9]+" _qtm_du_size "${_qtm_du_output}")
    set(${output_variable} "${_qtm_du_size}" PARENT_SCOPE)
endfunction()

_qtm_directory_size_kib("${QTM_APPLICATION}" _qtm_size_before_kib)

if(NOT QTM_AUDIT_ONLY)
    file(GLOB_RECURSE _qtm_deployed_plugin_files LIST_DIRECTORIES FALSE
        "${_qtm_plugins}/*")
    foreach(_qtm_plugin IN LISTS _qtm_deployed_plugin_files)
        if(IS_SYMLINK "${_qtm_plugin}")
            continue()
        endif()
        file(RELATIVE_PATH _qtm_plugin_relative "${_qtm_plugins}" "${_qtm_plugin}")
        list(FIND _qtm_allowed_plugins "${_qtm_plugin_relative}" _qtm_plugin_index)
        if(_qtm_plugin_index EQUAL -1)
            message(STATUS "Removing unused Qt plugin: ${_qtm_plugin_relative}")
            file(REMOVE "${_qtm_plugin}")
        endif()
    endforeach()

    file(GLOB _qtm_deployed_frameworks LIST_DIRECTORIES TRUE
        "${_qtm_frameworks}/Qt*.framework")
    foreach(_qtm_framework IN LISTS _qtm_deployed_frameworks)
        if(NOT IS_DIRECTORY "${_qtm_framework}")
            continue()
        endif()
        get_filename_component(_qtm_framework_name "${_qtm_framework}" NAME)
        list(FIND _qtm_allowed_frameworks "${_qtm_framework_name}" _qtm_framework_index)
        if(_qtm_framework_index EQUAL -1)
            message(STATUS "Removing unused Qt framework: ${_qtm_framework_name}")
            file(REMOVE_RECURSE "${_qtm_framework}")
        endif()
    endforeach()

    file(GLOB _qtm_plugin_directories LIST_DIRECTORIES TRUE "${_qtm_plugins}/*")
    foreach(_qtm_plugin_directory IN LISTS _qtm_plugin_directories)
        if(NOT IS_DIRECTORY "${_qtm_plugin_directory}")
            continue()
        endif()
        file(GLOB _qtm_plugin_directory_entries "${_qtm_plugin_directory}/*")
        if(NOT _qtm_plugin_directory_entries)
            file(REMOVE_RECURSE "${_qtm_plugin_directory}")
        endif()
    endforeach()

    # The public package is arm64-only. Thinning every deployed Mach-O avoids shipping the unused
    # Intel half of Qt's universal SDK while retaining framework symlinks and bundle structure.
    file(GLOB_RECURSE _qtm_bundle_files LIST_DIRECTORIES FALSE "${_qtm_contents}/*")
    foreach(_qtm_file IN LISTS _qtm_bundle_files)
        if(IS_SYMLINK "${_qtm_file}")
            continue()
        endif()
        execute_process(
            COMMAND /usr/bin/lipo -archs "${_qtm_file}"
            RESULT_VARIABLE _qtm_lipo_result
            OUTPUT_VARIABLE _qtm_architectures
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT _qtm_lipo_result EQUAL 0)
            continue()
        endif()
        if(NOT _qtm_architectures MATCHES "(^|[ \t])arm64($|[ \t])")
            message(FATAL_ERROR
                "A deployed Mach-O has no arm64 slice: ${_qtm_file} (${_qtm_architectures})")
        endif()
        if(NOT _qtm_architectures STREQUAL "arm64")
            set(_qtm_thin_file "${_qtm_file}.qtm-arm64")
            execute_process(
                COMMAND /usr/bin/lipo -thin arm64 "${_qtm_file}" -output "${_qtm_thin_file}"
                RESULT_VARIABLE _qtm_thin_result
                ERROR_VARIABLE _qtm_thin_error
            )
            if(NOT _qtm_thin_result EQUAL 0)
                message(FATAL_ERROR "Failed to thin ${_qtm_file}: ${_qtm_thin_error}")
            endif()
            execute_process(COMMAND /bin/chmod 755 "${_qtm_thin_file}")
            file(RENAME "${_qtm_thin_file}" "${_qtm_file}" RESULT _qtm_rename_result)
            if(NOT _qtm_rename_result STREQUAL "0")
                message(FATAL_ERROR "Failed to replace ${_qtm_file}: ${_qtm_rename_result}")
            endif()
        endif()
    endforeach()

    # Deployment and thinning mutate signed code. Seal the final, already-minimized payload once.
    execute_process(
        COMMAND /usr/bin/codesign --force --deep --sign - "${QTM_APPLICATION}"
        RESULT_VARIABLE _qtm_sign_result
        ERROR_VARIABLE _qtm_sign_error
    )
    if(NOT _qtm_sign_result EQUAL 0)
        message(FATAL_ERROR "Failed to ad-hoc sign the minimized app: ${_qtm_sign_error}")
    endif()
endif()

foreach(_qtm_framework_name IN LISTS _qtm_allowed_frameworks)
    if(NOT IS_DIRECTORY "${_qtm_frameworks}/${_qtm_framework_name}")
        message(FATAL_ERROR "Required Qt framework is missing: ${_qtm_framework_name}")
    endif()
endforeach()

foreach(_qtm_plugin_relative IN LISTS _qtm_allowed_plugins)
    if(NOT EXISTS "${_qtm_plugins}/${_qtm_plugin_relative}")
        message(FATAL_ERROR "Required Qt plugin is missing: ${_qtm_plugin_relative}")
    endif()
endforeach()

file(GLOB_RECURSE _qtm_audit_plugin_files LIST_DIRECTORIES FALSE "${_qtm_plugins}/*")
foreach(_qtm_plugin IN LISTS _qtm_audit_plugin_files)
    if(IS_SYMLINK "${_qtm_plugin}")
        continue()
    endif()
    file(RELATIVE_PATH _qtm_plugin_relative "${_qtm_plugins}" "${_qtm_plugin}")
    list(FIND _qtm_allowed_plugins "${_qtm_plugin_relative}" _qtm_plugin_index)
    if(_qtm_plugin_index EQUAL -1)
        message(FATAL_ERROR "Unexpected Qt plugin survived deployment: ${_qtm_plugin_relative}")
    endif()
endforeach()

file(GLOB _qtm_audit_frameworks LIST_DIRECTORIES TRUE "${_qtm_frameworks}/Qt*.framework")
foreach(_qtm_framework IN LISTS _qtm_audit_frameworks)
    get_filename_component(_qtm_framework_name "${_qtm_framework}" NAME)
    list(FIND _qtm_allowed_frameworks "${_qtm_framework_name}" _qtm_framework_index)
    if(_qtm_framework_index EQUAL -1)
        message(FATAL_ERROR "Unexpected Qt framework survived deployment: ${_qtm_framework_name}")
    endif()
endforeach()

# Audit every Mach-O after trimming. Also verify that each embedded @rpath Qt dependency resolves
# inside the app, so a smaller package can never be produced by deleting something still needed.
file(GLOB_RECURSE _qtm_audit_files LIST_DIRECTORIES FALSE "${_qtm_contents}/*")
foreach(_qtm_file IN LISTS _qtm_audit_files)
    if(IS_SYMLINK "${_qtm_file}")
        continue()
    endif()
    execute_process(
        COMMAND /usr/bin/lipo -archs "${_qtm_file}"
        RESULT_VARIABLE _qtm_lipo_result
        OUTPUT_VARIABLE _qtm_architectures
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _qtm_lipo_result EQUAL 0)
        continue()
    endif()
    if(NOT _qtm_architectures STREQUAL "arm64")
        message(FATAL_ERROR
            "The public macOS payload is not arm64-only: ${_qtm_file} (${_qtm_architectures})")
    endif()

    execute_process(
        COMMAND /usr/bin/otool -L "${_qtm_file}"
        RESULT_VARIABLE _qtm_otool_result
        OUTPUT_VARIABLE _qtm_dependencies
        ERROR_QUIET
    )
    if(NOT _qtm_otool_result EQUAL 0)
        message(FATAL_ERROR "Failed to inspect Mach-O dependencies: ${_qtm_file}")
    endif()
    string(REGEX MATCHALL "@rpath/Qt[^/]+\\.framework" _qtm_qt_dependencies
        "${_qtm_dependencies}")
    foreach(_qtm_qt_dependency IN LISTS _qtm_qt_dependencies)
        string(REPLACE "@rpath/" "" _qtm_dependency_framework "${_qtm_qt_dependency}")
        if(NOT IS_DIRECTORY "${_qtm_frameworks}/${_qtm_dependency_framework}")
            message(FATAL_ERROR
                "${_qtm_file} references missing ${_qtm_dependency_framework}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND /usr/bin/codesign --verify --deep --strict "${QTM_APPLICATION}"
    RESULT_VARIABLE _qtm_verify_result
    ERROR_VARIABLE _qtm_verify_error
)
if(NOT _qtm_verify_result EQUAL 0)
    message(FATAL_ERROR "The minimized app signature is invalid: ${_qtm_verify_error}")
endif()

_qtm_directory_size_kib("${QTM_APPLICATION}" _qtm_size_after_kib)
message(STATUS
    "macOS runtime audit passed: before=${_qtm_size_before_kib} KiB, "
    "final=${_qtm_size_after_kib} KiB, frameworks=7, plugins=6, architecture=arm64")
