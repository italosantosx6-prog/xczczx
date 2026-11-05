cmake_minimum_required(VERSION 3.20)
project(vread_driver)

# WDK path - adjust to your installed WDK version
set(WDK_PATH "C:/Program Files (x86)/Windows Kits/10")
set(WDK_VER  "10.0.26100.0")  # update to match your installed WDK version

set(CMAKE_SYSTEM_NAME Windows)

add_executable(vread.sys entry.cpp)

target_include_directories(vread.sys PRIVATE
    "${WDK_PATH}/Include/${WDK_VER}/km"
    "${WDK_PATH}/Include/${WDK_VER}/km/crt"
    "${WDK_PATH}/Include/${WDK_VER}/shared"
)

target_link_directories(vread.sys PRIVATE
    "${WDK_PATH}/Lib/${WDK_VER}/km/x64"
)

target_compile_definitions(vread.sys PRIVATE
    _AMD64_=1
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A000007
    POOL_NX_OPTIN=1
)

target_compile_options(vread.sys PRIVATE
    /kernel
    /GS-
    /EHs-c-
    /GR-
    /W4
    /wd4100  # unreferenced param
    /wd4201  # nameless struct/union
)

target_link_options(vread.sys PRIVATE
    /DRIVER
    /ENTRY:DriverEntry
    /NODEFAULTLIB
    /SUBSYSTEM:NATIVE
    /INCREMENTAL:NO
    /PDB:NONE
    /DEBUG:NONE
)

target_link_libraries(vread.sys PRIVATE
    ntoskrnl.lib
    hal.lib
    wdm.lib
    ntstrsafe.lib
)
