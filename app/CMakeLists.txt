cmake_minimum_required(VERSION 3.25)
project(witcher2 VERSION 0.1.2 LANGUAGES C CXX)

option(WITCHER2_NATIVE_RENDERER "Build a native graphics comparison tool" OFF)

if(REXGLUE_PLATFORM_SWITCH OR WITCHER2_NATIVE_RENDERER)
    add_subdirectory(../sdk sdk)
else()
    find_package(rexglue 0.10.0 REQUIRED CONFIG)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/sources.cmake")
    message(FATAL_ERROR "Run tools/generate.py before configuring the app")
endif()

include(generated/default/sources.cmake)
if(WITCHER2_NATIVE_RENDERER)
    # ReXApp registers the function map in witcher2_init.cpp.
    list(REMOVE_ITEM GENERATED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/witcher2_register.cpp")
endif()
add_executable(witcher2 src/boot.cpp ${GENERATED_SOURCES})
target_compile_features(witcher2 PRIVATE cxx_std_23)
target_include_directories(witcher2 PRIVATE generated/default generated)
target_precompile_headers(witcher2 PRIVATE generated/default/witcher2_pch.h)
target_compile_options(witcher2 PRIVATE -fno-strict-aliasing -ffp-contract=off -fno-char8_t -O1 -g0)
target_link_libraries(witcher2 PRIVATE rex::runtime)

if(REXGLUE_PLATFORM_SWITCH)
    option(WITCHER2_RENDERER "Enable the Xenos Vulkan renderer" OFF)
    target_include_directories(witcher2 SYSTEM BEFORE PRIVATE ../sdk/switch_compat)
    if(WITCHER2_RENDERER)
        if(REXGLUE_SWITCH_NO_NVK)
            message(FATAL_ERROR "The Xenos renderer needs a Switch Vulkan driver")
        endif()
        rexglue_switch_configure_target(witcher2 GPU_PLUGINS xenos)
        # The build stamp is used only by ReXApp. Keep the generated code's PCH reusable.
        set_property(TARGET witcher2 PROPERTY COMPILE_DEFINITIONS "")
        set_source_files_properties("${REXGLUE_SHARE_DIR}/rex_app.cpp"
            PROPERTIES COMPILE_DEFINITIONS "REXGLUE_BUILD_CONFIG=\"$<CONFIG>\"" SKIP_PRECOMPILE_HEADERS ON)
        target_sources(witcher2 PRIVATE src/graphics_app.cpp)
        set_source_files_properties(src/boot.cpp PROPERTIES COMPILE_DEFINITIONS WITCHER2_RENDERER
            SKIP_PRECOMPILE_HEADERS ON)
        rexglue_switch_add_nro(witcher2 NAME "The Witcher 2 - Hardware Test" VERSION 0.1.9)
    else()
        target_sources(witcher2 PRIVATE src/boot_switch.cpp)
        set_source_files_properties(src/boot_switch.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
        target_link_libraries(witcher2 PRIVATE rexglue_switch_startup rex::nvk)
        target_link_options(witcher2 PRIVATE "LINKER:--whole-archive,$<TARGET_FILE:rexruntime>,--no-whole-archive")
        set_property(TARGET witcher2 APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:rexruntime>")
        rexglue_switch_add_nro(witcher2 NAME "The Witcher 2 - Boot Test" VERSION ${PROJECT_VERSION})
    endif()
elseif(WITCHER2_NATIVE_RENDERER)
    rexglue_configure_target(witcher2 GPU_PLUGINS xenos)
    target_sources(witcher2 PRIVATE src/graphics_app.cpp)
    set_source_files_properties(src/boot.cpp PROPERTIES COMPILE_DEFINITIONS WITCHER2_RENDERER
        SKIP_PRECOMPILE_HEADERS ON)
endif()
