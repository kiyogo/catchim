add_executable(catchim_tests
    catchim_tests.cpp
)
target_link_libraries(catchim_tests PRIVATE
    catchim_core
    catchim_editor
    catchim_media
    catchim_subtitles
    catchim_storage
    catchim_render
    catchim_audio
    catchim_export
)
if(HAVE_QT6)
    target_link_libraries(catchim_tests PRIVATE Qt6::Core Qt6::Gui)
endif()
add_test(NAME CatchimCoreTests COMMAND catchim_tests)

add_executable(catchim_empirical_test
    empirical_challenge_test.cpp
)
target_link_libraries(catchim_empirical_test PRIVATE
    catchim_core
    catchim_editor
    catchim_storage
)
if(HAVE_QT6)
    target_link_libraries(catchim_empirical_test PRIVATE Qt6::Core Qt6::Gui)

    add_executable(catchim_ui_grabber
        ui_visual_grabber.cpp
        ../src/app/AppConfig.cpp
    )
    target_link_libraries(catchim_ui_grabber PRIVATE
        catchim_ui
        catchim_storage
        catchim_export
    )
    add_executable(catchim_native_workflow native_workflow_regression.cpp ../src/app/AppConfig.cpp)
    target_link_libraries(catchim_native_workflow PRIVATE catchim_ui catchim_storage catchim_export)
    add_test(NAME CatchimNativeWorkflow COMMAND catchim_native_workflow -platform offscreen)
    add_executable(catchim_playback_regression playback_regression.cpp ../src/app/AppConfig.cpp)
    target_link_libraries(catchim_playback_regression PRIVATE catchim_ui catchim_storage catchim_export)
    add_executable(catchim_preview_selection preview_selection_regression.cpp ../src/app/AppConfig.cpp)
    target_link_libraries(catchim_preview_selection PRIVATE catchim_ui catchim_storage catchim_export)
    add_test(NAME CatchimPreviewSelection COMMAND catchim_preview_selection -platform offscreen)
    add_executable(catchim_ui_finish ui_finish_regression.cpp ../src/app/AppConfig.cpp)
    target_link_libraries(catchim_ui_finish PRIVATE catchim_ui catchim_storage catchim_export)
    add_executable(catchim_dropdown_regression dropdown_regression.cpp)
    target_link_libraries(catchim_dropdown_regression PRIVATE catchim_ui)
    add_test(NAME CatchimDropdownRegression COMMAND catchim_dropdown_regression -platform offscreen)
endif()
add_test(NAME CatchimEmpiricalTests COMMAND catchim_empirical_test)
