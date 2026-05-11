cmake_minimum_required(VERSION 3.20)
project(LaMaQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
#  Build environment notes
#
#  Qt path:
#    DO NOT hardcode Qt6_DIR or CMAKE_PREFIX_PATH here.
#    Qt Creator injects them automatically via the active kit.
#    For command-line builds, pass via:
#      cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.10.2/msvc2022_64 ...
#
#  Compiler:
#    MSVC (Visual Studio 2022 x64) is recommended — required for TRT.
#    MinGW is also supported if MinGW-compatible TRT import libs (.a) exist.
#    To generate them from TRT DLLs with dlltool:
#      gendef nvinfer.dll && dlltool -d nvinfer.def -l libnvinfer.a --dllname nvinfer.dll
#      gendef nvonnxparser.dll && dlltool -d nvonnxparser.def -l libnvonnxparser.a --dllname nvonnxparser.dll
#      gendef cudart64_131.dll && dlltool -d cudart64_131.def -l libcudart.a --dllname cudart64_131.dll
# ============================================================================

# ────────────────────────────────────────────────────────────────────────────
# ↓↓↓  Adjust these three paths to match your installation  ↓↓↓
# ────────────────────────────────────────────────────────────────────────────

set(CUDA_TOOLKIT_ROOT_DIR  "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.1")
set(ONNXRUNTIME_ROOT       "C:/onnxruntime")
set(OpenCV_DIR             "C:/OpenCV")    # directory containing OpenCVConfig.cmake
set(LIBVLC_ROOT            "C:/libVLC")

# ────────────────────────────────────────────────────────────────────────────

set(BUILD_SHARED_LIBS ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Qml Concurrent
)

# OpenCVConfig.cmake 없이 수동 설정
set(OpenCV_INCLUDE_DIRS "${OpenCV_DIR}/include")
set(OpenCV_LIBS "")  # target_link_libraries에서 직접 경로 지정

qt_standard_project_setup(REQUIRES 6.6)

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

# ────────────────────────────────────────────────────────────────────────────
# Sources
# ────────────────────────────────────────────────────────────────────────────

set(APP_SOURCES
    main.cpp
    src/LaMaBridge.cpp
    src/LaMaEngine.cpp
    src/MaskPainter.cpp
    src/VideoSurface.cpp
    src/VideoPlayer.cpp
    src/VLCFrameGrabber.cpp
    src/ResultImageProvider.cpp
    src/OnnxEngine.cpp
    src/LaMa.cpp
    src/SAM2Engine.cpp
    src/SAMBridge.cpp
    src/SegmentCanvas.cpp
    src/ImageUtils.cpp
)

set(APP_HEADERS
    src/LaMaBridge.h
    src/LaMaEngine.h
    src/MaskPainter.h
    src/VideoSurface.h
    src/VideoPlayer.h
    src/VLCFrameGrabber.h
    src/ResultImageProvider.h
    src/OnnxEngine.h
    src/LaMa.h
    src/ISAMEngine.h
    src/SAM2Engine.h
    src/SAMBridge.h
    src/SegmentCanvas.h
    src/ImageUtils.h
)

# ────────────────────────────────────────────────────────────────────────────
# Executable
# ────────────────────────────────────────────────────────────────────────────

qt_add_executable(LaMaQt WIN32 ${APP_SOURCES} ${APP_HEADERS} app.rc)

qt_add_resources(LaMaQt "appicon"
    PREFIX "/" FILES resources/app.ico
)

# ────────────────────────────────────────────────────────────────────────────
# QML module
# ────────────────────────────────────────────────────────────────────────────

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(LaMaQt
    URI CompactLaMaQt
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/SourcePanel.qml
        qml/WorkspacePanel.qml
        qml/StyledButton.qml
        qml/AboutDialog.qml
    SOURCES
        src/VideoSurface.h   src/VideoSurface.cpp
        src/MaskPainter.h    src/MaskPainter.cpp
        src/LaMaBridge.h     src/LaMaBridge.cpp
        src/SAMBridge.h      src/SAMBridge.cpp
        src/SegmentCanvas.h  src/SegmentCanvas.cpp
)

# ────────────────────────────────────────────────────────────────────────────
# Include directories
# ────────────────────────────────────────────────────────────────────────────

target_include_directories(LaMaQt PRIVATE
    src
    "${CUDA_TOOLKIT_ROOT_DIR}/include"
    "${ONNXRUNTIME_ROOT}/include"
    "${LIBVLC_ROOT}/include"
    ${OpenCV_INCLUDE_DIRS}
)

# ────────────────────────────────────────────────────────────────────────────
# External libraries
# ────────────────────────────────────────────────────────────────────────────

set(CUDA_LIB_DIR         "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64")
set(ONNXRUNTIME_LIB_DIR  "${ONNXRUNTIME_ROOT}/lib")

# ── ONNX Runtime ──────────────────────────────────────────────────────────────
find_library(ONNXRUNTIME_LIB
    NAMES onnxruntime
    PATHS ${ONNXRUNTIME_LIB_DIR}
    NO_DEFAULT_PATH
)

if(NOT ONNXRUNTIME_LIB)
    message(FATAL_ERROR
        "onnxruntime.lib not found in: ${ONNXRUNTIME_LIB_DIR}\n"
        "  Expected: C:/onnxruntime-win-x64-gpu-1.24.2/lib/onnxruntime.lib")
endif()

# ── CUDA runtime ──────────────────────────────────────────────────────────────
# Searches for both MSVC (cudart.lib) and MinGW (libcudart.a) import libraries.
find_library(CUDA_RUNTIME_LIB
    NAMES cudart libcudart
    PATHS ${CUDA_LIB_DIR}
    NO_DEFAULT_PATH
)

if(NOT CUDA_RUNTIME_LIB)
    message(FATAL_ERROR
        "CUDA runtime library not found in: ${CUDA_LIB_DIR}\n"
        "  MSVC : cudart.lib\n"
        "  MinGW: libcudart.a (generate with dlltool from cudart64_131.dll)")
endif()

# ── libVLC ────────────────────────────────────────────────────────────────────
find_library(LIBVLC_LIB
    NAMES vlc libvlc
    PATHS "${LIBVLC_ROOT}/bin/X64" "${LIBVLC_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_library(LIBVLCCORE_LIB
    NAMES vlccore libvlccore
    PATHS "${LIBVLC_ROOT}/bin/X64" "${LIBVLC_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(NOT LIBVLC_LIB)
    message(FATAL_ERROR
        "libvlc import library not found in: ${LIBVLC_ROOT}/bin/X64\n"
        "  MinGW: libvlc.a already generated (used by CompactVLMQt)\n"
        "  MSVC : run  gendef libvlc.dll && dlltool -d libvlc.def -l libvlc.lib --dllname libvlc.dll")
endif()

# ────────────────────────────────────────────────────────────────────────────
# Link libraries
# ────────────────────────────────────────────────────────────────────────────

target_link_libraries(LaMaQt PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Qml Qt6::Concurrent
    "$<IF:$<CONFIG:Debug>,${OpenCV_DIR}/lib/opencv_worldd.lib,${OpenCV_DIR}/lib/opencv_world.lib>"
    ${ONNXRUNTIME_LIB}
    ${CUDA_RUNTIME_LIB}
    ${LIBVLC_LIB}
    ${LIBVLCCORE_LIB}
    ws2_32 shlwapi shell32
)

# ────────────────────────────────────────────────────────────────────────────
# Compiler options
# ────────────────────────────────────────────────────────────────────────────

if(MSVC)
    target_compile_options(LaMaQt PRIVATE
        /W3
        /wd4996    # Suppress POSIX name deprecation
        /wd4251    # Suppress DLL-interface warnings from TRT headers
        /DUNICODE /D_UNICODE /DNOMINMAX
    )
else()
    # MinGW / GCC
    target_compile_options(LaMaQt PRIVATE
        -Wall
        -Wno-deprecated-declarations
        -DUNICODE -D_UNICODE -DNOMINMAX
    )
endif()

# ────────────────────────────────────────────────────────────────────────────
# Post-build: copy runtime DLLs
# ────────────────────────────────────────────────────────────────────────────

add_custom_command(TARGET LaMaQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBVLC_ROOT}/bin/X64/libvlc.dll"     $<TARGET_FILE_DIR:LaMaQt>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBVLC_ROOT}/bin/X64/libvlccore.dll" $<TARGET_FILE_DIR:LaMaQt>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIBVLC_ROOT}/bin/X64/plugins"
        "$<TARGET_FILE_DIR:LaMaQt>/plugins"
    COMMENT "Copying VLC runtime..."
)

add_custom_command(TARGET LaMaQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${OpenCV_DIR}/bin/opencv_world4140d.dll,${OpenCV_DIR}/bin/opencv_world4140.dll>"
        $<TARGET_FILE_DIR:LaMaQt>
    COMMENT "Copying OpenCV runtime..."
)

add_custom_command(TARGET LaMaQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT}/bin/onnxruntime.dll"                  $<TARGET_FILE_DIR:LaMaQt>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT}/bin/onnxruntime_providers_cuda.dll"   $<TARGET_FILE_DIR:LaMaQt>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT}/bin/onnxruntime_providers_shared.dll" $<TARGET_FILE_DIR:LaMaQt>
    COMMENT "Copying ONNX Runtime runtime DLLs..."
)

# ── windeployqt ───────────────────────────────────────────────────────────────
# Locate windeployqt6 from whichever Qt kit is active (MSVC or MinGW).
find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt6 windeployqt
    HINTS
        "${Qt6_DIR}/../../../bin"
        "${CMAKE_PREFIX_PATH}/bin"
    NO_DEFAULT_PATH
)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET LaMaQt POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            --no-translations
            $<TARGET_FILE:LaMaQt>
        COMMENT "Running windeployqt..."
    )
endif()
