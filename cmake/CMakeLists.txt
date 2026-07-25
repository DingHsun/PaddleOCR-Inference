cmake_minimum_required(VERSION 3.20)
project(PaddleOCR-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Prebuilt third-party SDKs. Override with -DOPENCV_DIR=... / -DONNXRUNTIME_DIR=...
# if you don't keep them under <repo>/packages (same layout the vs2022/ build uses).
set(OPENCV_DIR "${REPO_ROOT}/packages/opencv/build" CACHE PATH "OpenCV (prebuilt Windows package) root")
set(ONNXRUNTIME_DIR "${REPO_ROOT}/packages/onnxruntime-win-x64-gpu-1.21.0" CACHE PATH "ONNX Runtime GPU root")

# The top-level OpenCVConfig.cmake tries to auto-detect the MSVC runtime
# folder (vc14/vc15/vc16) from MSVC_VERSION, but this prebuilt package
# predates VS2022 (MSVC 19.3x+) so that detection fails and OpenCV_FOUND
# comes back FALSE. Point straight at the vc15 config instead, which
# doesn't need that detection and works fine with the VS2022 compiler.
set(OpenCV_DIR "${OPENCV_DIR}/x64/vc15/lib")
find_package(OpenCV REQUIRED)

add_library(onnxruntime STATIC IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
)

# --- shared OCR core (text detection + recognition) -------------------------
add_library(ocr_core STATIC
    ${REPO_ROOT}/src/core/text_det.cpp
    ${REPO_ROOT}/src/core/text_rec.cpp
)
target_include_directories(ocr_core PUBLIC ${REPO_ROOT}/src/core)
target_link_libraries(ocr_core PUBLIC ${OpenCV_LIBS} onnxruntime)

function(copy_runtime_assets target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${REPO_ROOT}/weights" "$<TARGET_FILE_DIR:${target}>/weights"
        COMMAND ${CMAKE_COMMAND} -E copy "${OPENCV_DIR}/x64/vc15/bin/opencv_world450$<$<CONFIG:Debug>:d>.dll" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy "${OPENCV_DIR}/x64/vc15/bin/opencv_videoio_ffmpeg450_64.dll" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_cuda.dll" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_tensorrt.dll" "$<TARGET_FILE_DIR:${target}>"
        COMMAND_EXPAND_LISTS
    )
endfunction()

# --- api_server: headless OCR over HTTP (POST /ocr_detect, POST /ocr_recognize) ---
add_executable(api_server ${REPO_ROOT}/src/api_server/main.cpp)
target_include_directories(api_server PRIVATE ${REPO_ROOT}/third_party)
target_link_libraries(api_server PRIVATE ocr_core)
if(WIN32)
    target_link_libraries(api_server PRIVATE ws2_32)
endif()
copy_runtime_assets(api_server)

# Ship the built Vue frontend (frontend/dist) next to api_server, if it's been
# built (`npm run build` in frontend/). api_server serves it via set_mount_point.
if(EXISTS "${REPO_ROOT}/frontend/dist")
    add_custom_command(TARGET api_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${REPO_ROOT}/frontend/dist" "$<TARGET_FILE_DIR:api_server>/frontend/dist"
    )
endif()
