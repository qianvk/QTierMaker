cmake_minimum_required(VERSION 3.24)

project(QTierMaker
    VERSION 1.0.2
    DESCRIPTION "A native Qt 6 desktop tier-list editor"
    HOMEPAGE_URL "https://github.com/qianvk/QTierMaker"
    LANGUAGES CXX
)

if(APPLE)
    enable_language(OBJCXX)
endif()

option(QTM_BUILD_TESTS "Build QTierMaker tests" ON)
option(QTM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
# PROJECT_VERSION is the single source of truth for application and package versions. Keep release
# suffixes process-local so tagged or CI builds cannot poison a reused local CMake cache.
if(DEFINED CACHE{QTM_PACKAGE_VERSION})
    unset(QTM_PACKAGE_VERSION CACHE)
endif()
if(DEFINED CACHE{QTM_PACKAGE_VERSION_OVERRIDE})
    unset(QTM_PACKAGE_VERSION_OVERRIDE CACHE)
endif()
set(QTM_PACKAGE_VERSION "${PROJECT_VERSION}")
if(DEFINED ENV{QTM_VERSION_SUFFIX} AND NOT "$ENV{QTM_VERSION_SUFFIX}" STREQUAL "")
    set(_qtm_version_suffix "$ENV{QTM_VERSION_SUFFIX}")
    if(NOT _qtm_version_suffix MATCHES "^[-+][0-9A-Za-z][0-9A-Za-z.+-]*$")
        message(FATAL_ERROR
            "QTM_VERSION_SUFFIX must be an optional semantic-version prerelease or build suffix")
    endif()
    string(APPEND QTM_PACKAGE_VERSION "${_qtm_version_suffix}")
endif()
if(DEFINED ENV{QTM_EXPECTED_VERSION} AND
   NOT "$ENV{QTM_EXPECTED_VERSION}" STREQUAL "" AND
   NOT QTM_PACKAGE_VERSION STREQUAL "$ENV{QTM_EXPECTED_VERSION}")
    message(FATAL_ERROR
        "Release version mismatch: source declares ${QTM_PACKAGE_VERSION}, "
        "but the build expects $ENV{QTM_EXPECTED_VERSION}")
endif()
set(QTM_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(QTM_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(QTM_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(QTM_NUMERIC_VERSION "${PROJECT_VERSION}")
message(STATUS "QTierMaker version: ${QTM_PACKAGE_VERSION}")

set(_qtm_default_update_channel "stable")
if(QTM_PACKAGE_VERSION MATCHES "-")
    set(_qtm_default_update_channel "beta")
endif()
set(QTM_UPDATE_CHANNEL "${_qtm_default_update_channel}" CACHE STRING
    "Update feed channel embedded in the application")
set_property(CACHE QTM_UPDATE_CHANNEL PROPERTY STRINGS stable beta)
set(_qtm_legacy_update_manifest_url
    "https://api.github.com/repos/qianvk/QTierMaker/releases?per_page=20")
if(DEFINED QTM_UPDATE_MANIFEST_URL AND
   QTM_UPDATE_MANIFEST_URL STREQUAL "${_qtm_legacy_update_manifest_url}")
    unset(QTM_UPDATE_MANIFEST_URL CACHE)
endif()
set(QTM_UPDATE_MANIFEST_URL "" CACHE STRING
    "Optional custom HTTPS update manifest URL; empty selects the channel feed")
if(QTM_UPDATE_MANIFEST_URL)
    set(_qtm_effective_update_manifest_url "${QTM_UPDATE_MANIFEST_URL}")
elseif(QTM_UPDATE_CHANNEL STREQUAL "beta")
    set(_qtm_effective_update_manifest_url
        "https://github.com/qianvk/QTierMaker/releases/download/beta-feed/updates.json")
else()
    set(_qtm_effective_update_manifest_url
        "https://github.com/qianvk/QTierMaker/releases/latest/download/updates.json")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectOptions)
include(CompilerWarnings)
include(Dependencies)
include(Version)

# Bump the revision whenever the executable requires a different deployed Qt/plugin payload.
set(QTM_UPDATE_RUNTIME_VERSION "qt-${Qt6_VERSION}-r1" CACHE STRING
    "Runtime generation required by executable-only update packages")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json for clangd" FORCE)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QTM_CORE_SOURCES
    src/assets/AssetManager.cpp
    src/assets/AssetManager.h
    src/assets/BackdropEffect.h
    src/assets/BackdropMaterial.cpp
    src/assets/BackdropMaterial.h
    src/assets/CoverImageCache.cpp
    src/assets/CoverImageCache.h
    src/assets/FocusBackdrop.cpp
    src/assets/FocusBackdrop.h
    src/assets/LiquidGlassBackdrop.cpp
    src/assets/LiquidGlassBackdrop.h
    src/assets/ImageLoader.cpp
    src/assets/ImageLoader.h
    src/assets/ThumbnailCache.cpp
    src/assets/ThumbnailCache.h
    src/export/ExportOptions.cpp
    src/export/ExportOptions.h
    src/export/TierListExporter.cpp
    src/export/TierListExporter.h
    src/i18n/LanguageManager.cpp
    src/i18n/LanguageManager.h
    src/logging/Logger.cpp
    src/logging/Logger.h
    src/logging/UiPerformanceMonitor.cpp
    src/logging/UiPerformanceMonitor.h
    src/persistence/AtomicFileWriter.cpp
    src/persistence/AtomicFileWriter.h
    src/persistence/ProjectFileLayout.cpp
    src/persistence/ProjectFileLayout.h
    src/persistence/ProjectRepository.cpp
    src/persistence/ProjectRepository.h
    src/persistence/ProjectStorage.cpp
    src/persistence/ProjectStorage.h
    src/persistence/ProjectSerializer.cpp
    src/persistence/ProjectSerializer.h
    src/persistence/RecentProjectsStore.cpp
    src/persistence/RecentProjectsStore.h
    src/persistence/SampleProjectSeeder.cpp
    src/persistence/SampleProjectSeeder.h
    src/persistence/TarArchive.cpp
    src/persistence/TarArchive.h
    src/persistence/Result.h
    src/platform/Platform.cpp
    src/platform/Platform.h
    src/settings/AppSettings.cpp
    src/settings/AppSettings.h
    src/settings/SettingsKeys.cpp
    src/settings/SettingsKeys.h
    src/theme/Theme.cpp
    src/theme/Theme.h
    src/theme/ThemeManager.cpp
    src/theme/ThemeManager.h
    src/theme/ThemeTokens.h
    src/tier/TierImage.cpp
    src/tier/TierImage.h
    src/tier/ImagePresentationMode.h
    src/tier/ProjectHistory.cpp
    src/tier/ProjectHistory.h
    src/tier/TierListLayout.cpp
    src/tier/TierListLayout.h
    src/tier/TierProject.cpp
    src/tier/TierProject.h
    src/tier/TierRow.cpp
    src/tier/TierRow.h
)

set(QTM_APP_SOURCES
    src/app/Application.cpp
    src/app/Application.h
    src/app/main.cpp
    src/navigation/SidebarDelegate.cpp
    src/navigation/SidebarDelegate.h
    src/navigation/SidebarModel.cpp
    src/navigation/SidebarModel.h
    src/navigation/SidebarView.cpp
    src/navigation/SidebarView.h
    src/pages/EditPage.cpp
    src/pages/EditPage.h
    src/pages/PreferencesPage.cpp
    src/pages/PreferencesPage.h
    src/pages/ProjectLocationDialog.cpp
    src/pages/ProjectLocationDialog.h
    src/pages/ProjectsPage.cpp
    src/pages/ProjectsPage.h
    src/preview/PreviewOverlay.cpp
    src/preview/PreviewOverlay.h
    src/samples/SampleProjectDownloader.cpp
    src/samples/SampleProjectDownloader.h
    src/tier/ImageGalleryPopover.cpp
    src/tier/ImageGalleryPopover.h
    src/tier/CropEditorWidget.cpp
    src/tier/CropEditorWidget.h
    src/tier/ImageEditDialog.cpp
    src/tier/ImageEditDialog.h
    src/tier/TierRowEditDialog.cpp
    src/tier/TierRowEditDialog.h
    src/tier/TierBoardWidget.cpp
    src/tier/TierBoardWidget.h
    src/tier/TierDragController.cpp
    src/tier/TierDragController.h
    src/tier/TierListDelegate.cpp
    src/tier/TierListDelegate.h
    src/tier/TierListModel.cpp
    src/tier/TierListModel.h
    src/tier/TierListView.cpp
    src/tier/TierListView.h
    src/update/AppUpdater.cpp
    src/update/AppUpdater.h
    src/update/UpdateButton.cpp
    src/update/UpdateButton.h
    src/widgets/DestructiveActionDialog.cpp
    src/widgets/DestructiveActionDialog.h
    src/widgets/EmptyStateWidget.cpp
    src/widgets/EmptyStateWidget.h
    src/widgets/FlowLayout.cpp
    src/widgets/FlowLayout.h
    src/widgets/SectionHeader.cpp
    src/widgets/SectionHeader.h
    src/widgets/TransferProgressWidget.cpp
    src/widgets/TransferProgressWidget.h
    src/widgets/WrappingLabel.cpp
    src/widgets/WrappingLabel.h
    src/window/AppDialog.cpp
    src/window/AppDialog.h
    src/window/AppMessageDialog.h
    src/window/AppTitleBar.cpp
    src/window/AppTitleBar.h
    src/window/CompositionAwareLineEdit.cpp
    src/window/CompositionAwareLineEdit.h
    src/window/MainWindow.cpp
    src/window/MainWindow.h
    src/window/RootWidget.cpp
    src/window/RootWidget.h
    src/window/SidebarToggleButton.cpp
    src/window/SidebarToggleButton.h
    resources/resources.qrc
)

if(APPLE)
    set(QTM_APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/app-icon.icns")
    list(APPEND QTM_APP_SOURCES "${QTM_APP_ICON_MACOS}")
    set_source_files_properties("${QTM_APP_ICON_MACOS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

if(WIN32)
    configure_file(
        resources/windows/QTierMaker.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/generated/QTierMaker.rc"
        @ONLY
    )
    list(APPEND QTM_APP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/generated/QTierMaker.rc")
endif()

if(APPLE)
    list(APPEND QTM_CORE_SOURCES src/platform/PlatformMac.mm)
elseif(WIN32)
    list(APPEND QTM_CORE_SOURCES src/platform/PlatformWindows.cpp)
else()
    list(APPEND QTM_CORE_SOURCES src/platform/PlatformLinux.cpp)
endif()

add_library(qtiermaker_core STATIC ${QTM_CORE_SOURCES})
target_include_directories(qtiermaker_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(qtiermaker_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        VkUI::Core
)
qtm_apply_project_options(qtiermaker_core)
qtm_apply_compiler_warnings(qtiermaker_core)

qt_add_executable(QTierMaker ${QTM_APP_SOURCES})
target_include_directories(QTierMaker PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(QTierMaker
    PRIVATE
        qtiermaker_core
        Qt6::Network
        Qt6::Svg
        VkUI::Widgets
        VkUI::Window
)
if(WIN32)
    target_link_libraries(QTierMaker PRIVATE dwmapi)
endif()
target_compile_definitions(QTierMaker PRIVATE
    QTM_APP_VERSION="${QTM_PACKAGE_VERSION}"
    QTM_GIT_COMMIT="${QTM_GIT_COMMIT}"
    QTM_UPDATE_CHANNEL="${QTM_UPDATE_CHANNEL}"
    QTM_UPDATE_MANIFEST_URL="${_qtm_effective_update_manifest_url}"
    QTM_UPDATE_RUNTIME_VERSION="${QTM_UPDATE_RUNTIME_VERSION}"
)
qt_add_translations(QTierMaker
    TS_FILES
        resources/translations/QTierMaker_en.ts
        resources/translations/QTierMaker_zh_CN.ts
    LUPDATE_OPTIONS
        -no-obsolete
    RESOURCE_PREFIX "/translations"
)
qtm_apply_project_options(QTierMaker)
qtm_apply_compiler_warnings(QTierMaker)

if(APPLE)
    add_executable(QTierMakerMacUpdateHelper src/update/MacUpdateHelper.mm)
    target_link_libraries(QTierMakerMacUpdateHelper PRIVATE "-framework Foundation")
    qtm_apply_project_options(QTierMakerMacUpdateHelper)
    qtm_apply_compiler_warnings(QTierMakerMacUpdateHelper)
endif()

set_target_properties(QTierMaker PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "QTierMaker"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.qtiermaker.app"
    MACOSX_BUNDLE_ICON_FILE "app-icon.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${QTM_NUMERIC_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${QTM_NUMERIC_VERSION}"
)

include(InstallRules)

if(QTM_BUILD_TESTS)
    enable_testing()
    add_test(NAME tst_ApplicationVersion
        COMMAND "${CMAKE_COMMAND}"
            "-DQTM_APPLICATION=$<TARGET_FILE:QTierMaker>"
            "-DQTM_EXPECTED_OUTPUT=QTierMaker ${QTM_PACKAGE_VERSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/VerifyApplicationVersion.cmake"
    )
    add_subdirectory(tests/unit)
    add_subdirectory(tests/integration)
endif()
