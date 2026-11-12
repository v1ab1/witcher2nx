cmake_minimum_required(VERSION 3.25)
project(witcher2_deko LANGUAGES C CXX)

get_filename_component(W2_BASE "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(W2_SA2_SOURCE_DIR "" CACHE PATH "SA2-NX checkout at baf523b")
set(W2_SHADER_CACHE_SOURCE "${W2_BASE}/app/generated/deko3d/shader_cache.cpp"
    CACHE FILEPATH "Shader metadata generated locally from the game")
set(W2_DEKO3D_DEVKITPRO "${DEVKITPRO}" CACHE PATH "devkitPro root with deko3d")
set(W2_DEKO3D_INCLUDE_DIR "${W2_DEKO3D_DEVKITPRO}/libnx/include"
    CACHE PATH "Directory containing deko3d.h")
set(THIRDPARTY "${W2_SA2_SOURCE_DIR}/app/thirdparty")
if(NOT W2_SA2_SOURCE_DIR OR NOT EXISTS "${THIRDPARTY}/plume/CMakeLists.txt")
    message(FATAL_ERROR "Set W2_SA2_SOURCE_DIR to the SA2-NX checkout in deko3d/README.md")
endif()
if(NOT EXISTS "${W2_SHADER_CACHE_SOURCE}" OR
   NOT EXISTS "${W2_BASE}/app/generated/default/sources.cmake")
    message(FATAL_ERROR "Generate the guest code and shader cache as described in deko3d/README.md")
endif()
set(REXGLUE_SWITCH_NO_NVK ON CACHE BOOL "" FORCE)
add_subdirectory("${W2_BASE}/sdk" sdk)
target_compile_definitions(rexcore PRIVATE REX_SWITCH_FALLBACK_EXE="/switch/witcher2/witcher2.nro")

set(PLUME_PLATFORM_SWITCH ON CACHE BOOL "" FORCE)
set(PLUME_SWITCH_NO_NVK ON CACHE BOOL "" FORCE)
set(PLUME_DEKO3D ON CACHE BOOL "" FORCE)
set(PLUME_SWITCH_UAM_DIR "${THIRDPARTY}/uam" CACHE PATH "" FORCE)
set(PLUME_SWITCH_DEVKITPRO "${W2_DEKO3D_DEVKITPRO}" CACHE PATH "" FORCE)
set(PLUME_SWITCH_NVK_ROOT "${REXGLUE_SWITCH_NVK_SDK}" CACHE PATH "" FORCE)
set(PLUME_SWITCH_NVK_LIBRARY "${REXGLUE_SWITCH_NVK_SDK}/lib/libvulkan.a" CACHE FILEPATH "" FORCE)
add_subdirectory("${THIRDPARTY}/plume" plume)
target_include_directories(plume SYSTEM PUBLIC "${W2_DEKO3D_INCLUDE_DIR}")

set(XENOS "${THIRDPARTY}/reblue-XenosRecomp")
set(RENUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/renut_engine")
file(GLOB RENUT_BACKEND CONFIGURE_DEPENDS "${RENUT_DIR}/gpu/backend/*.cpp")
file(GLOB RENUT_RENDER CONFIGURE_DEPENDS "${RENUT_DIR}/gpu/render/*.cpp")
file(GLOB RENUT_OVERLAY CONFIGURE_DEPENDS "${RENUT_DIR}/gpu/overlay/*.cpp")
add_library(w2_d3d OBJECT
    ${RENUT_BACKEND} ${RENUT_RENDER} ${RENUT_OVERLAY}
    ${RENUT_DIR}/gpu/banjo_renut_select.cpp
    ${RENUT_DIR}/gpu/banjo_renut_tuning.cpp
    ${RENUT_DIR}/gpu/banjo_renut_checks.cpp
    ${RENUT_DIR}/gpu/banjo_renut_record.cpp
    ${RENUT_DIR}/gpu/banjo_renut_deko3d.cpp
    ${RENUT_DIR}/display_sync.cpp
    ${RENUT_DIR}/perf/frame_stats.cpp
    ${RENUT_DIR}/sa2_compat/frame_latency.cpp
    ${RENUT_DIR}/sa2_compat/sa2_compat.cpp
    ${RENUT_DIR}/gpu/d3d/d3d_native_state.cpp
    ${RENUT_DIR}/gpu/d3d/hooks/device.cpp
    ${RENUT_DIR}/gpu/d3d/hooks/draw.cpp
    ${RENUT_DIR}/gpu/d3d/hooks/resource.cpp
    ${RENUT_DIR}/gpu/d3d/hooks/shader.cpp
    ${RENUT_DIR}/gpu/guest/callbacks.cpp
    ${RENUT_DIR}/gpu/guest/command_buffers.cpp
    ${RENUT_DIR}/gpu/guest/guest_records.cpp
    ${RENUT_DIR}/gpu/guest/sa2_screen.cpp
    "${W2_SHADER_CACHE_SOURCE}"
    "${XENOS}/thirdparty/smol-v/source/smolv.cpp"
    "${XENOS}/thirdparty/xxHash/xxhash.c"
    src/witcher2_d3d_compat.cpp)
target_compile_features(w2_d3d PRIVATE cxx_std_23)
target_compile_definitions(w2_d3d PRIVATE RENUT_RECOMP SA2_RENUT_RENDERER=1 SA2_SWITCH_NO_NVK=1 WITCHER2_LIVE_D3D=1)
target_compile_options(w2_d3d PRIVATE -fno-strict-aliasing)
target_include_directories(w2_d3d BEFORE PRIVATE "${XENOS}/thirdparty/xxHash")
target_include_directories(w2_d3d PRIVATE
    src "${RENUT_DIR}/sa2_compat" "${RENUT_DIR}/gpu/shaders" "${CMAKE_CURRENT_SOURCE_DIR}"
    "${W2_BASE}/sdk/src/core" "${XENOS}/XenosRecomp"
    "${XENOS}/thirdparty/smol-v/source"
    $<TARGET_PROPERTY:imgui,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(w2_d3d SYSTEM BEFORE PRIVATE
    "${W2_BASE}/sdk/switch_compat"
    "${W2_DEKO3D_INCLUDE_DIR}")
target_link_libraries(w2_d3d PRIVATE rex::runtime plume o1heap)

include("${W2_BASE}/app/generated/default/sources.cmake")
list(REMOVE_ITEM GENERATED_SOURCES "${W2_BASE}/app/generated/default/witcher2_register.cpp")
add_executable(witcher2_deko src/graphics_app.cpp "${W2_BASE}/app/src/boot.cpp" ${GENERATED_SOURCES})
target_include_directories(witcher2_deko PRIVATE src "${W2_BASE}/app/src"
    "${W2_BASE}/app/generated/default" "${W2_BASE}/app/generated")
target_include_directories(witcher2_deko SYSTEM BEFORE PRIVATE "${W2_BASE}/sdk/switch_compat")
target_compile_features(witcher2_deko PRIVATE cxx_std_23)
target_precompile_headers(witcher2_deko PRIVATE "${W2_BASE}/app/generated/default/witcher2_pch.h")
target_compile_options(witcher2_deko PRIVATE -fno-strict-aliasing -ffp-contract=off -fno-char8_t -O1 -g0)
rexglue_switch_configure_target(witcher2_deko)
set_property(TARGET witcher2_deko PROPERTY COMPILE_DEFINITIONS "")
set_source_files_properties("${W2_BASE}/app/src/boot.cpp" PROPERTIES
    COMPILE_DEFINITIONS WITCHER2_RENDERER SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties("${REXGLUE_SHARE_DIR}/rex_app.cpp" PROPERTIES
    COMPILE_DEFINITIONS "REXGLUE_BUILD_CONFIG=\"$<CONFIG>\"" SKIP_PRECOMPILE_HEADERS ON)
target_link_libraries(witcher2_deko PRIVATE rex::runtime w2_d3d plume o1heap)
target_link_options(witcher2_deko PRIVATE LINKER:--wrap=nvGpuChannelAppendEntry)
rexglue_switch_add_nro(witcher2_deko NAME "The Witcher 2 deko3d" VERSION 0.1.46)
