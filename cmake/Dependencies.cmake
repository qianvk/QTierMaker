# VkUI's integrated window module deliberately uses Qt private platform APIs. Installers deploy
# the exact Qt build used here, so suppress the generic warning after making that lock explicit.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 6.10.1 REQUIRED COMPONENTS
    Core CorePrivate
    Gui GuiPrivate
    Widgets WidgetsPrivate
    Svg Network Concurrent Test LinguistTools
)

set(QTM_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(QTM_VKUI_DIR "${QTM_THIRD_PARTY_DIR}/vkui")

if(NOT EXISTS "${QTM_VKUI_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "VkUI is missing. Run: git submodule update --init third_party/vkui")
endif()

# Embed VkUI statically so installers only deploy Qt runtime libraries. VkUI owns the sole native
# window implementation; applications consume its stable facade without a second window dependency.
set(VKUI_BUILD_SHARED OFF CACHE BOOL "Build VkUI statically" FORCE)
set(VKUI_BUILD_EXAMPLES OFF CACHE BOOL "Build VkUI examples" FORCE)
set(VKUI_BUILD_TESTS OFF CACHE BOOL "Build VkUI tests" FORCE)
set(VKUI_BUILD_WINDOW ON CACHE BOOL "Build VkUI native window module" FORCE)
set(VKUI_BUILD_WINDOW_QUICK OFF CACHE BOOL "Build VkUI Qt Quick window module" FORCE)
set(VKUI_INSTALL OFF CACHE BOOL "Install embedded VkUI" FORCE)
set(VKUI_ENABLE_WARNINGS OFF CACHE BOOL "Use parent warning policy" FORCE)
set(VKUI_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export clangd database" FORCE)
set(VKUI_SYNC_COMPILE_COMMANDS OFF CACHE BOOL "Do not copy nested clangd database" FORCE)
add_subdirectory("${QTM_VKUI_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/third_party/vkui"
                 EXCLUDE_FROM_ALL)

qt_standard_project_setup()
