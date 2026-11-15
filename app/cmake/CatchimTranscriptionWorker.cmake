set(CATCHIM_APP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(CATCHIM_WEB_NODE_MODULES "${CMAKE_CURRENT_LIST_DIR}/../../web/node_modules")

function(catchim_attach_transcription_worker target)
    if(NOT EMSCRIPTEN OR NOT TARGET "${target}")
        return()
    endif()

    find_program(CATCHIM_NODE_EXECUTABLE NAMES node node.exe REQUIRED)

    set(esbuild "${CATCHIM_WEB_NODE_MODULES}/esbuild/bin/esbuild")
    set(workerEntry "${CATCHIM_APP_SOURCE_DIR}/src/wasm/transcription-worker-entry.js")
    set(workerBridge "${CATCHIM_APP_SOURCE_DIR}/src/wasm/wasm-transcription.js")
    set(workerBundle "${CMAKE_CURRENT_BINARY_DIR}/catchim-transcription-worker.js")
    set(transformersPackage "${CATCHIM_WEB_NODE_MODULES}/@huggingface/transformers/package.json")
    set(onnxPackage "${CATCHIM_WEB_NODE_MODULES}/onnxruntime-web/package.json")
    set(onnxDist "${CATCHIM_WEB_NODE_MODULES}/onnxruntime-web/dist")

    if(NOT EXISTS "${esbuild}" OR NOT EXISTS "${transformersPackage}" OR NOT EXISTS "${onnxPackage}")
        message(FATAL_ERROR "Whisper requires web/node_modules with esbuild, @huggingface/transformers, and onnxruntime-web.")
    endif()

    add_custom_command(
        OUTPUT "${workerBundle}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "NODE_PATH=${CATCHIM_WEB_NODE_MODULES}"
            "${CATCHIM_NODE_EXECUTABLE}" "${esbuild}" "${workerEntry}"
            --bundle --platform=browser --format=esm --target=es2022
            "--outfile=${workerBundle}"
        DEPENDS "${workerEntry}" "${esbuild}" "${transformersPackage}" "${onnxPackage}"
        VERBATIM
        COMMENT "Bundling the local Whisper worker"
    )
    add_custom_target(catchim_transcription_worker_bundle DEPENDS "${workerBundle}")
    add_dependencies("${target}" catchim_transcription_worker_bundle)

    set(onnxRuntimeFiles
        "${onnxDist}/ort-wasm-simd-threaded.mjs"
        "${onnxDist}/ort-wasm-simd-threaded.wasm"
        "${onnxDist}/ort-wasm-simd-threaded.jsep.mjs"
        "${onnxDist}/ort-wasm-simd-threaded.jsep.wasm"
        "${onnxDist}/ort-wasm-simd-threaded.asyncify.mjs"
        "${onnxDist}/ort-wasm-simd-threaded.asyncify.wasm"
    )

    target_link_options("${target}" PRIVATE "SHELL:--pre-js ${workerBridge}")
    set_property(TARGET "${target}" APPEND PROPERTY LINK_DEPENDS
        "${workerBridge}" "${workerBundle}" ${onnxRuntimeFiles})

    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${workerBundle}" "$<TARGET_FILE_DIR:${target}>/catchim-transcription-worker.js"
        VERBATIM
        COMMENT "Installing the local Whisper worker"
    )
    foreach(runtimeFile IN LISTS onnxRuntimeFiles)
        get_filename_component(runtimeName "${runtimeFile}" NAME)
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtimeFile}" "$<TARGET_FILE_DIR:${target}>/${runtimeName}"
            VERBATIM
        )
    endforeach()
endfunction()
