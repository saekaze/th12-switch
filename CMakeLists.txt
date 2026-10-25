cmake_minimum_required(VERSION 3.16)
project(touhou12 C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Upstream TH12 core (Tweet-META/th12 eagler, ports/th12/src/game + the C ABI
# in src/wasm/Exports.cpp) and the Switch host in src/ (replacing the
# browser front-end in upstream site/ and src/*.mjs).
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/core/game/*.cpp)
list(APPEND CORE_SOURCES ${CMAKE_SOURCE_DIR}/core/wasm/Exports.cpp)
file(GLOB HOST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM HOST_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)

# CP932 -> UTF-16 map for the text rasteriser.
set(CP932_TABLE ${CMAKE_BINARY_DIR}/generated/cp932_table.inc)
add_custom_command(OUTPUT ${CP932_TABLE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/gen_cp932.py ${CP932_TABLE}
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/gen_cp932.py
    COMMENT "Generating CP932 table" VERBATIM)

# Same code generation as upstream's wasm build in the respects that matter
# for determinism: no FMA contraction (GCC contracts by default on AArch64)
# and signed plain char (signed on wasm32 and x86, unsigned on AArch64).
add_library(th12_core OBJECT ${CORE_SOURCES})
target_include_directories(th12_core PUBLIC core)
target_compile_options(th12_core PUBLIC -O2 -ffp-contract=off -fno-strict-aliasing -fsigned-char
    $<$<CXX_COMPILER_ID:GNU>:-Wno-class-memaccess> -Wno-misleading-indentation -Wno-parentheses)

add_library(th12_host OBJECT ${HOST_SOURCES} ${CP932_TABLE})
target_include_directories(th12_host PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(th12_host PUBLIC th12_core)

add_executable(touhou12 src/main.cpp)
target_link_libraries(touhou12 PRIVATE th12_host th12_core)

if(NINTENDO_SWITCH)
    # ---- Nintendo Switch (devkitA64; configure with the devkitPro Switch
    # toolchain file, which defines NINTENDO_SWITCH) ----
    # Layout/naming follows the Saekaze Touhou ports: touhou12.nro +
    # sdmc:/switch/touhou/touhou12/ (th12.exe, th12.dat, thbgm.dat, msgothic.ttc).
    if(NOT DEFINED ENV{DEVKITPRO})
        set(ENV{DEVKITPRO} "/opt/devkitpro")
    endif()
    set(TH_DEVKITPRO "$ENV{DEVKITPRO}")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/platform/switch/icon.jpg")
        message(FATAL_ERROR "platform/switch/icon.jpg (256x256 JPEG) not found.")
    endif()
    foreach(target th12_core th12_host)
        # newlib only declares POSIX extras such as strnlen outside strict ISO mode.
        target_compile_definitions(${target} PUBLIC __SWITCH__ _DEFAULT_SOURCE)
        target_include_directories(${target} PUBLIC
            ${TH_DEVKITPRO}/portlibs/switch/include
            ${TH_DEVKITPRO}/portlibs/switch/include/SDL2
            ${TH_DEVKITPRO}/portlibs/switch/include/freetype2
            ${TH_DEVKITPRO}/libnx/include)
    endforeach()
    target_link_directories(touhou12 PRIVATE
        ${TH_DEVKITPRO}/portlibs/switch/lib
        ${TH_DEVKITPRO}/libnx/lib)
    # Static link order matters: dependents first, dependencies after.
    target_link_libraries(touhou12 PRIVATE
        SDL2 EGL glapi drm_nouveau GLESv2 freetype harfbuzz png bz2 z nx m)
    set_target_properties(touhou12 PROPERTIES OUTPUT_NAME "touhou12")

    # Default (all languages): romanised title. The original Japanese title is
    # patched into the Japanese slot only - same as the other Saekaze ports.
    nx_generate_nacp(OUTPUT "${CMAKE_BINARY_DIR}/touhou12-base.nacp"
        NAME "Touhou 12: Undefined Fantastic Object"
        AUTHOR "Team Shanghai Alice"
        VERSION "1.00b-r2")
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/touhou12.nacp"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/nacp_lang.py"
                "${CMAKE_BINARY_DIR}/touhou12-base.nacp" "${CMAKE_BINARY_DIR}/touhou12.nacp"
                --lang Japanese
                --name "東方星蓮船　～ Undefined Fantastic Object."
                --author "上海アリス幻樂団"
                --fill-empty-name "Touhou 12: Undefined Fantastic Object"
                --fill-empty-author "Team Shanghai Alice"
        DEPENDS "${CMAKE_BINARY_DIR}/touhou12-base.nacp"
                "${CMAKE_SOURCE_DIR}/scripts/nacp_lang.py"
        COMMENT "Patching Japanese title into NACP"
        VERBATIM)
    nx_create_nro(touhou12
        ICON "${CMAKE_SOURCE_DIR}/platform/switch/icon.jpg"
        NACP "${CMAKE_BINARY_DIR}/touhou12.nacp")
else()
    # ---- Linux test build (same sources, SDL2 + GLES3 desktop) ----
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(FT REQUIRED freetype2)
    target_include_directories(th12_host PUBLIC ${SDL2_INCLUDE_DIRS} ${FT_INCLUDE_DIRS})
    target_link_libraries(touhou12 PRIVATE ${SDL2_LIBRARIES} ${FT_LIBRARIES} GLESv2 m)

    # Host tests: ./build/th12_host_tests (run under xvfb-run for the renderer)
    add_executable(th12_host_tests tests/test_host.cpp)
    target_link_libraries(th12_host_tests PRIVATE th12_host th12_core ${SDL2_LIBRARIES} ${FT_LIBRARIES} GLESv2 m)
    # Developer tool: scripted play on your own TH12 1.00b files (not a test).
    add_executable(th12_play_real tests/play_real.cpp)
    target_link_libraries(th12_play_real PRIVATE th12_host th12_core ${SDL2_LIBRARIES} ${FT_LIBRARIES} GLESv2 m)
endif()
