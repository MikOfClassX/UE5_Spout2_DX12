# =============================================================================
# Spout2_DX12 -- INSPECTION-ONLY CMake project
# =============================================================================
#
# This file does NOT build the plugin. Unreal Engine plugins are compiled by
# UnrealBuildTool (UBT), driven by Source/Spout2_DX12/Spout2_DX12.Build.cs --
# see Build.bat / BUILD_INSTRUCTIONS.md for the real, supported build path.
#
# The only purpose of this CMakeLists.txt is to let Visual Studio open this
# repository (generate a .sln with `cmake -G "Visual Studio 17 2022" ...`,
# or "Open Folder" in the CMake overlay) and show every plugin source file
# in Solution Explorer with the same directory layout as on disk, with
# working IntelliSense include paths, so the code can be browsed and
# navigated without opening a full Unreal project.
#
# The target below is a STATIC library marked EXCLUDE_FROM_ALL:
#  - EXCLUDE_FROM_ALL keeps it out of "Build Solution" / the default
#    ALL_BUILD target, so a normal build of the generated solution does
#    NOT attempt to compile it.
#  - It is a real (non-INTERFACE) library only because Visual Studio's
#    CMake generator does not emit a .vcxproj at all for INTERFACE
#    libraries -- an INTERFACE target would be invisible in Solution
#    Explorer. A STATIC target is the minimum library type that both (a)
#    gets a real project file with proper IntelliSense config and (b) can
#    be fully excluded from the default build.
#  - It WOULD fail to compile if explicitly built (Unreal macros like
#    UCLASS/UPROPERTY need Unreal Header Tool's generated *.generated.h
#    files, which do not exist here), but nothing triggers that build
#    unless you deliberately right-click the project and choose "Build".
#
# IntelliSense will resolve the plugin's own headers and the bundled
# SpoutDX12 third-party headers, but NOT Unreal Engine symbols
# (UCLASS/UPROPERTY/TArray/AActor/...): those live in the Engine's own
# Source tree and in the generated headers mentioned above, neither of
# which are reproduced here. Point UE_ENGINE_SOURCE_DIR below at a local
# Engine/Source folder to also feed those directories to IntelliSense
# (still inspection-only; it will not make this project buildable).
# =============================================================================

cmake_minimum_required(VERSION 3.20)
project(Spout2_DX12_Inspect LANGUAGES CXX)

# Optional: point this at "<UE install>/Engine/Source" to help IntelliSense
# resolve Unreal Engine headers (Core, CoreUObject, Engine, RHI, ...) too.
# Leave empty to only browse/index the plugin's own sources.
set(UE_ENGINE_SOURCE_DIR "C:/Program Files/Epic Games/UE_5.8/Engine/Source" CACHE PATH
    "Path to an Unreal Engine install's Engine/Source folder, for IntelliSense only. Leave empty to skip Engine headers.")

# Optional: point this at a checkout of the upstream Spout2 SDK
# (github.com/leadedge/Spout2) that has the real .cpp implementations of
# spoutFrameCount/spoutDX12/etc (this plugin only ships headers + the
# precompiled SpoutDX12.lib/.dll, so "Go to Definition" on Spout SDK calls
# can't resolve without this). Reference only -- not part of the build.
set(SPOUT2_SDK_REFERENCE_DIR "F:/UNREAL_PROJECT/unreal_poc2/Spout2" CACHE PATH
    "Path to a Spout2 SDK checkout (with SPOUTSDK/SpoutGL and SPOUTSDK/SpoutDirectX/SpoutDX sources), for IntelliSense/Go-to-Definition only. Leave empty to skip.")

# ---------------------------------------------------------------------------
# Collect every plugin source file (headers + .cpp), preserving the on-disk
# folder structure in Solution Explorer via source_group(TREE ...).
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Spout2_DX12/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Spout2_DX12/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ThirdParty/*.h"
)

file(GLOB_RECURSE PLUGIN_BUILD_SCRIPTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Spout2_DX12/*.Build.cs"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ThirdParty/*.Build.cs"
)

set(EXTRA_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Spout2_DX12.uplugin"
)

set(ALL_INSPECTED_FILES
    ${PLUGIN_SOURCES}
    ${PLUGIN_BUILD_SCRIPTS}
    ${EXTRA_FILES}
)

# ---------------------------------------------------------------------------
# Reference-only: the real Spout2 SDK sources (spoutFrameCount, spoutDX,
# spoutDX12, ...), so "Go to Definition" on Spout SDK calls used by the
# plugin (SpoutDX12->frame.CreateAccessMutex(...) etc.) jumps to real code
# instead of failing. Only the library's own top-level files are globbed
# (non-recursive) -- the same folders also contain unrelated tutorial/
# example projects (their own main()/WinMain, D3D12TextureReceiver,
# Tutorial04, ...) that would just add noise and symbol clashes.
# ---------------------------------------------------------------------------
set(SPOUT2_SDK_REFERENCE_SOURCES "")
if(SPOUT2_SDK_REFERENCE_DIR AND EXISTS "${SPOUT2_SDK_REFERENCE_DIR}")
    file(GLOB SPOUT2_SDK_REFERENCE_SOURCES
        "${SPOUT2_SDK_REFERENCE_DIR}/SPOUTSDK/SpoutGL/*.cpp"
        "${SPOUT2_SDK_REFERENCE_DIR}/SPOUTSDK/SpoutGL/*.h"
        "${SPOUT2_SDK_REFERENCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX/*.cpp"
        "${SPOUT2_SDK_REFERENCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX/*.h"
        "${SPOUT2_SDK_REFERENCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX/SpoutDX12/*.cpp"
        "${SPOUT2_SDK_REFERENCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX/SpoutDX12/*.h"
    )
    message(STATUS "Spout2_DX12 (inspect-only): adding ${SPOUT2_SDK_REFERENCE_DIR} Spout2 SDK sources for Go-to-Definition (reference only)")
else()
    message(STATUS "Spout2_DX12 (inspect-only): SPOUT2_SDK_REFERENCE_DIR not set/found - Go to Definition on Spout SDK calls will fail (declaration-only headers)")
endif()

set(ALL_INSPECTED_FILES
    ${ALL_INSPECTED_FILES}
    ${SPOUT2_SDK_REFERENCE_SOURCES}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PLUGIN_SOURCES} ${PLUGIN_BUILD_SCRIPTS} ${EXTRA_FILES})
if(SPOUT2_SDK_REFERENCE_SOURCES)
    source_group(TREE "${SPOUT2_SDK_REFERENCE_DIR}" PREFIX "Spout2 SDK (reference)" FILES ${SPOUT2_SDK_REFERENCE_SOURCES})
endif()

# STATIC + EXCLUDE_FROM_ALL: real project file for Solution Explorer /
# IntelliSense, but never part of the default "Build Solution".
add_library(Spout2_DX12_Browse STATIC EXCLUDE_FROM_ALL
    ${PLUGIN_SOURCES}
    ${PLUGIN_BUILD_SCRIPTS}
    ${SPOUT2_SDK_REFERENCE_SOURCES}
)
# Non-source files (the .uplugin descriptor) are attached separately so
# they show up in Solution Explorer without being treated as C++ sources.
target_sources(Spout2_DX12_Browse PRIVATE ${EXTRA_FILES})
set_source_files_properties(${EXTRA_FILES} ${PLUGIN_BUILD_SCRIPTS}
    PROPERTIES HEADER_FILE_ONLY TRUE)

set_target_properties(Spout2_DX12_Browse PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD TRUE
)

# ---------------------------------------------------------------------------
# IntelliSense include paths (best-effort; irrelevant to building since this
# target is excluded from the default build).
# ---------------------------------------------------------------------------
# NOTE: matches exactly the PublicIncludePaths in Spout2_DX12.Build.cs.
# Source/ThirdParty/Spout2_DX12Library/include is NOT used by the real
# build (it's a leftover/reference copy) and is deliberately not added
# here, to avoid IntelliSense resolving a header the real build doesn't
# actually see.
target_include_directories(Spout2_DX12_Browse PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Spout2_DX12/Public"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Spout2_DX12/Private"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ThirdParty/include"
)

if(UE_ENGINE_SOURCE_DIR AND EXISTS "${UE_ENGINE_SOURCE_DIR}")
    message(STATUS "Spout2_DX12 (inspect-only): adding Engine headers from ${UE_ENGINE_SOURCE_DIR} for IntelliSense")
    # The plugin touches Runtime modules (Core, CoreUObject, Engine, RHI,
    # RenderCore, Slate, SlateCore, ApplicationCore, InputCore, Projects, ...)
    # AND Editor modules (Editor.h, UnrealEd - used by the editor-viewport
    # sender source), so both trees are needed.
    file(GLOB UE_MODULE_PUBLIC_DIRS
        "${UE_ENGINE_SOURCE_DIR}/Runtime/*/Public"
        "${UE_ENGINE_SOURCE_DIR}/Runtime/*/Classes"
        "${UE_ENGINE_SOURCE_DIR}/Runtime/*/*/Public"
        "${UE_ENGINE_SOURCE_DIR}/Editor/*/Public"
        "${UE_ENGINE_SOURCE_DIR}/Editor/*/Classes"
    )
    target_include_directories(Spout2_DX12_Browse PRIVATE ${UE_MODULE_PUBLIC_DIRS})
    list(LENGTH UE_MODULE_PUBLIC_DIRS UE_MODULE_DIR_COUNT)
    message(STATUS "Spout2_DX12 (inspect-only): added ${UE_MODULE_DIR_COUNT} Engine module include directories")
else()
    message(STATUS "Spout2_DX12 (inspect-only): UE_ENGINE_SOURCE_DIR not set/found - Engine symbols (UCLASS, TArray, ...) will show as unresolved in IntelliSense")
endif()

message(STATUS "Spout2_DX12: inspection-only CMake project configured (excluded from default build). See Build.bat for the real build.")
