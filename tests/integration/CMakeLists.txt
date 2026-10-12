function(add_qtm_integration_test target source)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE qtiermaker_core Qt6::Test)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_qtm_integration_test(tst_ProjectSaveOpen tst_ProjectSaveOpen.cpp)
add_qtm_integration_test(tst_ImageImport tst_ImageImport.cpp)
add_qtm_integration_test(tst_ProjectSaveAsFlow tst_ProjectSaveAsFlow.cpp)

# Exercise the production window composition, including the splitter and panel manager.
set(_window_composition_sources)
foreach(source IN LISTS QTM_APP_SOURCES)
    if(source MATCHES "^src/" AND NOT source MATCHES "^src/app/")
        list(APPEND _window_composition_sources "${PROJECT_SOURCE_DIR}/${source}")
    endif()
endforeach()
qt_add_executable(tst_WindowComposition tst_WindowComposition.cpp ${_window_composition_sources})
target_include_directories(tst_WindowComposition PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(tst_WindowComposition PRIVATE qtiermaker_core VkUI::Widgets VkUI::Window
    Qt6::Test Qt6::Network Qt6::Svg)
add_test(NAME tst_WindowComposition COMMAND tst_WindowComposition)
