function(add_qtm_unit_test target)
    qt_add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE qtiermaker_core Qt6::Test)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_qtm_unit_test(tst_ProjectSerializer tst_ProjectSerializer.cpp)
add_qtm_unit_test(tst_ProjectFileLayout tst_ProjectFileLayout.cpp)
add_qtm_unit_test(tst_ProjectHistory tst_ProjectHistory.cpp)
add_qtm_unit_test(tst_RecentProjectsStore tst_RecentProjectsStore.cpp)
add_qtm_unit_test(tst_SampleProjectSeeder tst_SampleProjectSeeder.cpp)
add_qtm_unit_test(tst_TarArchive tst_TarArchive.cpp)
add_qtm_unit_test(tst_AssetManager tst_AssetManager.cpp)
add_qtm_unit_test(tst_FocusBackdrop tst_FocusBackdrop.cpp)
add_qtm_unit_test(tst_LiquidGlassBackdrop tst_LiquidGlassBackdrop.cpp)
add_qtm_unit_test(tst_BackdropMaterial tst_BackdropMaterial.cpp)
add_qtm_unit_test(tst_ImageLoader tst_ImageLoader.cpp)
add_qtm_unit_test(tst_ThemeManager tst_ThemeManager.cpp)
add_qtm_unit_test(tst_CompositionAwareLineEdit
    tst_CompositionAwareLineEdit.cpp
    "${PROJECT_SOURCE_DIR}/src/window/CompositionAwareLineEdit.cpp"
    "${PROJECT_SOURCE_DIR}/src/window/CompositionAwareLineEdit.h"
)
add_qtm_unit_test(tst_ThumbnailCache tst_ThumbnailCache.cpp)
add_qtm_unit_test(tst_WrappingLabel
    tst_WrappingLabel.cpp
    "${PROJECT_SOURCE_DIR}/src/widgets/WrappingLabel.cpp"
    "${PROJECT_SOURCE_DIR}/src/widgets/WrappingLabel.h"
)
add_qtm_unit_test(tst_AppUpdater
    tst_AppUpdater.cpp
    "${PROJECT_SOURCE_DIR}/src/update/AppUpdater.cpp"
    "${PROJECT_SOURCE_DIR}/src/update/AppUpdater.h"
)
target_link_libraries(tst_AppUpdater PRIVATE Qt6::Network)
add_qtm_unit_test(tst_UpdateButton
    tst_UpdateButton.cpp
    "${PROJECT_SOURCE_DIR}/src/update/UpdateButton.cpp"
    "${PROJECT_SOURCE_DIR}/src/update/UpdateButton.h"
    "${PROJECT_SOURCE_DIR}/src/widgets/TransferProgressWidget.cpp"
    "${PROJECT_SOURCE_DIR}/src/widgets/TransferProgressWidget.h"
)
target_link_libraries(tst_UpdateButton PRIVATE Qt6::Network)
add_qtm_unit_test(tst_TierListLayout
    tst_TierListLayout.cpp
    "${PROJECT_SOURCE_DIR}/src/tier/TierDragController.cpp"
    "${PROJECT_SOURCE_DIR}/src/tier/TierDragController.h"
    "${PROJECT_SOURCE_DIR}/src/tier/TierListDelegate.cpp"
    "${PROJECT_SOURCE_DIR}/src/tier/TierListDelegate.h"
    "${PROJECT_SOURCE_DIR}/src/tier/TierListModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/tier/TierListModel.h"
    "${PROJECT_SOURCE_DIR}/src/tier/TierListView.cpp"
    "${PROJECT_SOURCE_DIR}/src/tier/TierListView.h"
)

qt_add_executable(tst_WindowLifecycle
    tst_WindowLifecycle.cpp
    "${PROJECT_SOURCE_DIR}/src/pages/ProjectLocationDialog.cpp"
    "${PROJECT_SOURCE_DIR}/src/pages/ProjectLocationDialog.h"
    "${PROJECT_SOURCE_DIR}/src/preview/PreviewOverlay.cpp"
    "${PROJECT_SOURCE_DIR}/src/preview/PreviewOverlay.h"
    "${PROJECT_SOURCE_DIR}/src/window/AppDialog.cpp"
    "${PROJECT_SOURCE_DIR}/src/window/AppDialog.h"
)
target_link_libraries(tst_WindowLifecycle PRIVATE
    qtiermaker_core
    VkUI::Widgets
    VkUI::Window
    Qt6::Test
)
target_include_directories(tst_WindowLifecycle PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME tst_WindowLifecycle COMMAND tst_WindowLifecycle)

qt_add_executable(tst_PreviewOverlay
    tst_PreviewOverlay.cpp
    "${PROJECT_SOURCE_DIR}/src/preview/PreviewOverlay.cpp"
    "${PROJECT_SOURCE_DIR}/src/preview/PreviewOverlay.h"
)
target_link_libraries(tst_PreviewOverlay PRIVATE qtiermaker_core VkUI::Window Qt6::Test)
target_include_directories(tst_PreviewOverlay PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME tst_PreviewOverlay COMMAND tst_PreviewOverlay)

qt_add_executable(tst_ImageGalleryPopover
    tst_ImageGalleryPopover.cpp
    "${PROJECT_SOURCE_DIR}/src/preview/PreviewOverlay.cpp"
    "${PROJECT_SOURCE_DIR}/src/preview/PreviewOverlay.h"
    "${PROJECT_SOURCE_DIR}/src/tier/ImageGalleryPopover.cpp"
    "${PROJECT_SOURCE_DIR}/src/tier/ImageGalleryPopover.h"
    "${PROJECT_SOURCE_DIR}/src/tier/TierDragController.cpp"
    "${PROJECT_SOURCE_DIR}/src/tier/TierDragController.h"
)
target_link_libraries(tst_ImageGalleryPopover PRIVATE qtiermaker_core VkUI::Widgets Qt6::Test)
target_include_directories(tst_ImageGalleryPopover PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_test(NAME tst_ImageGalleryPopover COMMAND tst_ImageGalleryPopover)

qt_add_executable(tst_WindowResizePolicy
    tst_WindowResizePolicy.cpp
    "${PROJECT_SOURCE_DIR}/src/window/AppDialog.cpp"
    "${PROJECT_SOURCE_DIR}/src/window/AppDialog.h"
)
target_link_libraries(tst_WindowResizePolicy PRIVATE VkUI::Window Qt6::Test)
target_include_directories(tst_WindowResizePolicy PRIVATE "${PROJECT_SOURCE_DIR}/src")
if(APPLE)
    set_source_files_properties(tst_WindowResizePolicy.cpp PROPERTIES LANGUAGE OBJCXX)
endif()
add_test(NAME tst_WindowResizePolicy COMMAND tst_WindowResizePolicy)

qt_add_executable(tst_Resources tst_Resources.cpp "${PROJECT_SOURCE_DIR}/resources/resources.qrc")
target_link_libraries(tst_Resources PRIVATE VkUI::Core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME tst_Resources COMMAND tst_Resources)
