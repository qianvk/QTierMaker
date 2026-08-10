include(GNUInstallDirs)

install(TARGETS QTierMaker
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Keep the sample outside the executable so binary-only updates stay small. The application copies
# it to the user's project directory once and never merges it into an existing project.
if(APPLE)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/samples/Anime Girls v5/"
        DESTINATION "QTierMaker.app/Contents/Resources/samples/Anime Girls v5")
else()
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/samples/Anime Girls v5/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/QTierMaker/samples/Anime Girls v5")
endif()

if(MSVC)
    # Qt and the application import only these ABI-compatible VC runtime libraries. Resolve the
    # active toolchain instead of relying on CMake's compiler-version table, which can lag MSVC.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
        set(_qtm_redist_arch x64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
        set(_qtm_redist_arch arm64)
    else()
        message(FATAL_ERROR "Unsupported Windows package architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(_qtm_redist_root "$ENV{VCToolsRedistDir}")
    if(_qtm_redist_root)
        cmake_path(CONVERT "${_qtm_redist_root}" TO_CMAKE_PATH_LIST _qtm_redist_root NORMALIZE)
    endif()
    if(NOT _qtm_redist_root)
        get_filename_component(_qtm_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_qtm_vc_root
            "${_qtm_compiler_dir}/../../../../../.." ABSOLUTE)
        set(_qtm_redist_root "${_qtm_vc_root}/Redist/MSVC")
    endif()
    file(GLOB _qtm_crt_directories LIST_DIRECTORIES TRUE
        "${_qtm_redist_root}/*/${_qtm_redist_arch}/Microsoft.VC*.CRT"
        "${_qtm_redist_root}/${_qtm_redist_arch}/Microsoft.VC*.CRT")
    list(SORT _qtm_crt_directories COMPARE NATURAL ORDER DESCENDING)
    if(NOT _qtm_crt_directories)
        message(FATAL_ERROR "The active MSVC redistributable directory was not found")
    endif()
    list(GET _qtm_crt_directories 0 _qtm_crt_directory)

    set(_qtm_runtime_names
        msvcp140.dll
        msvcp140_1.dll
        msvcp140_2.dll
        vcruntime140.dll
        vcruntime140_1.dll
    )
    foreach(_qtm_runtime_name IN LISTS _qtm_runtime_names)
        set(_qtm_runtime_path "${_qtm_crt_directory}/${_qtm_runtime_name}")
        if(NOT EXISTS "${_qtm_runtime_path}")
            message(FATAL_ERROR "Required MSVC runtime was not found: ${_qtm_runtime_path}")
        endif()
        install(FILES "${_qtm_runtime_path}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endforeach()
endif()

# Build a self-contained install tree before CPack turns it into a platform installer.
set(_qtm_deploy_tool_options)
set(_qtm_deploy_plugin_options)
if(APPLE)
    # Re-sign after deployment so Qt frameworks and resources belong to the final app seal.
    # Ad-hoc signing needs no private identity; Gatekeeper still requires an explicit user override.
    list(APPEND _qtm_deploy_tool_options "-codesign=-")
endif()
if(WIN32)
    list(APPEND _qtm_deploy_tool_options
        --no-opengl-sw
        --no-system-d3d-compiler
        --no-system-dxc-compiler
        --no-compiler-runtime
        --no-translations
    )
    list(APPEND _qtm_deploy_plugin_options
        EXCLUDE_PLUGINS
            qtuiotouchplugin
            qicns
            qtga
            qwbmp
            qnetworklistmanager
    )
endif()
qt_generate_deploy_app_script(
    TARGET QTierMaker
    OUTPUT_SCRIPT QTM_QT_DEPLOY_SCRIPT
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    NO_UNSUPPORTED_PLATFORM_ERROR
    ${_qtm_deploy_plugin_options}
    DEPLOY_TOOL_OPTIONS ${_qtm_deploy_tool_options}
)
install(SCRIPT "${QTM_QT_DEPLOY_SCRIPT}")

set(CPACK_PACKAGE_NAME "QTierMaker")
set(CPACK_PACKAGE_VENDOR "qianvk")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A native desktop tier-list editor")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/qianvk/QTierMaker")
set(CPACK_PACKAGE_VERSION "${QTM_PACKAGE_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "QTierMaker")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
if(APPLE)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        message(FATAL_ERROR "The public macOS package must be built for arm64")
    endif()
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-macOS-arm64")
    # A DMG with the standard Applications symlink matches the platform's expected drag-install flow.
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "QTierMaker ${CPACK_PACKAGE_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_FILESYSTEM "APFS")
elseif(WIN32)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
        message(FATAL_ERROR "The public Windows package must be built for x64")
    endif()
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Windows-x64-Setup")
    # NSIS provides a familiar machine-wide wizard, Start Menu entry, repair-safe upgrades,
    # Apps & Features metadata, and an optional launch action without modifying PATH.
    file(TO_NATIVE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/app-icon.ico"
        _qtm_nsis_icon)
    file(TO_NATIVE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/installer-welcome.bmp"
        _qtm_nsis_welcome_bitmap)
    file(TO_NATIVE_PATH
        "${CMAKE_INSTALL_BINDIR}/QTierMaker.exe"
        _qtm_nsis_installed_executable)

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_VERBATIM_VARIABLES ON)
    # A machine-wide application belongs under the native 64-bit Program Files directory.
    # CPack's NSIS template already requests elevation and handles an existing installation.
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_DISPLAY_NAME "QTierMaker")
    set(CPACK_NSIS_PACKAGE_NAME "QTierMaker")
    set(CPACK_NSIS_MUI_ICON "${_qtm_nsis_icon}")
    set(CPACK_NSIS_MUI_UNIICON "${_qtm_nsis_icon}")
    set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${_qtm_nsis_welcome_bitmap}")
    set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${_qtm_nsis_welcome_bitmap}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${_qtm_nsis_installed_executable}")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    set(CPACK_PACKAGE_EXECUTABLES "QTierMaker" "QTierMaker")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "QTierMaker.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "WriteRegStr HKLM \"Software\\qianvk\\QTierMaker\" \"RuntimeVersion\" \"${QTM_UPDATE_RUNTIME_VERSION}\"")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/qianvk/QTierMaker")
    set(CPACK_NSIS_HELP_LINK "https://github.com/qianvk/QTierMaker/issues")
    set(CPACK_NSIS_BRANDING_TEXT "QTierMaker")

    set(_qtm_update_arch x64)

    find_program(QTM_MAKENSIS_EXECUTABLE NAMES makensis makensis.exe
        HINTS "C:/Program Files (x86)/NSIS" "C:/Program Files/NSIS")
    if(QTM_MAKENSIS_EXECUTABLE)
        set(QTM_WINDOWS_UPDATE_PACKAGE
            "${CMAKE_BINARY_DIR}/updates/QTierMaker-${QTM_PACKAGE_VERSION}-WinUpdate-${_qtm_update_arch}.exe")
        set(_qtm_update_payload "${CMAKE_BINARY_DIR}/updates/payload/QTierMaker.exe")
        file(TO_NATIVE_PATH "${_qtm_update_payload}" _qtm_update_payload_native)
        add_custom_command(
            OUTPUT "${QTM_WINDOWS_UPDATE_PACKAGE}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/updates/payload"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:QTierMaker>" "${_qtm_update_payload}"
            COMMAND "${QTM_MAKENSIS_EXECUTABLE}"
                "/INPUTCHARSET" "UTF8"
                "/DAPP_EXECUTABLE=${_qtm_update_payload_native}"
                "/DOUTPUT_FILE=${QTM_WINDOWS_UPDATE_PACKAGE}"
                "/DAPP_VERSION=${QTM_PACKAGE_VERSION}"
                "/DNUMERIC_VERSION=${QTM_NUMERIC_VERSION}"
                "/DRUNTIME_VERSION=${QTM_UPDATE_RUNTIME_VERSION}"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/update.nsi"
            DEPENDS QTierMaker "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/update.nsi"
            COMMENT "Creating executable-only Windows update package"
            VERBATIM
        )
        add_custom_target(QTierMakerUpdatePackage DEPENDS "${QTM_WINDOWS_UPDATE_PACKAGE}")
    else()
        message(STATUS "makensis was not found; the Windows update package target is unavailable")
    endif()

    file(WRITE "${CMAKE_BINARY_DIR}/QTierMaker-Windows-runtime-version.txt"
        "${QTM_UPDATE_RUNTIME_VERSION}\n")
endif()

include(CPack)
