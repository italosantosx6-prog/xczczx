cmake_minimum_required(VERSION 3.20)
project(vrd_esp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# - Sources -
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/../external/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(esp
    main.cpp
    esp.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(esp PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/../driver  # for comms.h
)

target_compile_definitions(esp PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_options(esp PRIVATE
    /W3
    /MP       # multi-processor compile
    /O2       # optimize for speed (release)
    /GS-      # disable buffer security check (minor; remove if paranoid)
)

target_link_libraries(esp PRIVATE
    d3d11
    dxgi
    dwmapi
    psapi
)

# Strip PDB in release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(esp PRIVATE /DEBUG:NONE /PDB:NONE)
endif()
