cmake_minimum_required(VERSION 3.20)
project(vrd_loader CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(loader main.cpp)

target_include_directories(loader PRIVATE
    ${CMAKE_SOURCE_DIR}/../driver  # for comms.h
)

target_compile_definitions(loader PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(loader PRIVATE psapi)

# Must run as admin - embed manifest
set_target_properties(loader PROPERTIES
    LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
)
