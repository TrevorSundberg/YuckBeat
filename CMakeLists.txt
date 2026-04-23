cmake_minimum_required(VERSION 3.22)

project(YuckBeat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(YUCKBEAT_VST3_COPY_DIR "/home/trevor/.wine/drive_c/Program Files/Common Files/VST3" CACHE PATH "Wine VST3 install folder")
set(VST3_SDK_ROOT "/home/trevor/vst3sdk" CACHE PATH "Steinberg VST3 SDK root")

set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_UTILITIES OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_MODULE_INFO OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_BUNDLE_FOR_WINDOWS OFF CACHE BOOL "" FORCE)

list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")
include(SMTG_VST3_SDK)
smtg_setup_platform_toolset()

set(ROOT "${VST3_SDK_ROOT}")
set(SDK_ROOT "${VST3_SDK_ROOT}")
set(public_sdk_SOURCE_DIR "${SDK_ROOT}/public.sdk")
set(pluginterfaces_SOURCE_DIR "${SDK_ROOT}/pluginterfaces")
set(VST_SDK TRUE)

include_directories("${VST3_SDK_ROOT}" "${SDK_ROOT}")

add_subdirectory("${VST3_SDK_ROOT}/pluginterfaces" vst3sdk/pluginterfaces)
add_subdirectory("${VST3_SDK_ROOT}/base" vst3sdk/base)
add_subdirectory("${VST3_SDK_ROOT}/public.sdk" vst3sdk/public.sdk)

set(_engine_source_path "${CMAKE_BINARY_DIR}/YuckBeatEngine.dll")
string(REPLACE "/" "\\\\" _engine_source_path_escaped "${_engine_source_path}")
set(YUCKBEAT_ENGINE_DLL_PATH_C "Z:${_engine_source_path_escaped}")
configure_file(
    Source/YuckBeatBuildConfig.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/YuckBeatBuildConfig.h"
    @ONLY)

add_library(YuckBeatEngine SHARED
    Source/YuckBeatEngine.cpp
    Source/YuckBeatEngineAPI.h
    Source/YuckBeatIDs.h)

set_target_properties(YuckBeatEngine PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_compile_features(YuckBeatEngine PRIVATE cxx_std_17)
target_include_directories(YuckBeatEngine PRIVATE "${VST3_SDK_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_link_libraries(YuckBeatEngine PRIVATE pluginterfaces)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_options(YuckBeatEngine PRIVATE -static)
endif()

smtg_add_vst3plugin(YuckBeat
    PACKAGE_NAME "YuckBeat"
    SOURCES_LIST
        Source/YuckBeatController.cpp
        Source/YuckBeatController.h
        Source/YuckBeatEditor.cpp
        Source/YuckBeatEditor.h
        Source/YuckBeatEngineAPI.h
        Source/YuckBeatEngineLoader.cpp
        Source/YuckBeatEngineLoader.h
        Source/YuckBeatFactory.cpp
        Source/YuckBeatIDs.h
        Source/YuckBeatProcessor.cpp
        Source/YuckBeatProcessor.h)

add_dependencies(YuckBeat YuckBeatEngine)
target_compile_features(YuckBeat PUBLIC cxx_std_17)
target_include_directories(YuckBeat PRIVATE
    "${VST3_SDK_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(YuckBeat PRIVATE sdk user32 gdi32)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_options(YuckBeat PRIVATE -static)
endif()

add_executable(smoke_load_vst3 tools/smoke_load_vst3.cpp)
target_compile_features(smoke_load_vst3 PRIVATE cxx_std_17)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_options(smoke_load_vst3 PRIVATE -static -municode)
endif()

add_executable(smoke_hot_reload_engine
    tools/smoke_hot_reload_engine.cpp
    Source/YuckBeatEngineAPI.h
    Source/YuckBeatEngineLoader.cpp
    Source/YuckBeatEngineLoader.h)
add_dependencies(smoke_hot_reload_engine YuckBeatEngine)
target_compile_features(smoke_hot_reload_engine PRIVATE cxx_std_17)
target_include_directories(smoke_hot_reload_engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_options(smoke_hot_reload_engine PRIVATE -static)
endif()

add_custom_command(TARGET YuckBeat POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${YUCKBEAT_VST3_COPY_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:YuckBeat>" "${YUCKBEAT_VST3_COPY_DIR}/YuckBeat.vst3"
    COMMENT "Copying YuckBeat.vst3 to Wine VST3 folder")
