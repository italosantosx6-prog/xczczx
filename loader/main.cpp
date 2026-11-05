// language: C++, file: loader/main.cpp, target: Windows 11 x64, MSVC
// Entry point for the BYOVD driver mapper
// Usage: loader.exe <driver.sys>
// Requires: rtcore64.sys in same directory (or override VULN_DRIVER_PATH)
// Must run as Administrator

#include <windows.h>
#include <iostream>
#include <filesystem>
#include "rtcore.hpp"
#include "kdmapper.hpp"

namespace fs = std::filesystem;

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        std::wcerr << L"usage: loader.exe <path\\to\\driver.sys>\n";
        return 1;
    }

    std::wstring driver_path = argv[1];
    if (!fs::exists(driver_path)) {
        std::wcerr << L"[-] driver file not found: " << driver_path << L"\n";
        return 1;
    }

    // ── 1. Load RTCore64 (the vulnerable driver) ─────────────────────────────
    std::wstring rtcore_path = fs::path(argv[0]).parent_path() / L"rtcore64.sys";
    std::wcout << L"[*] loading RTCore64 from: " << rtcore_path << L"\n";

    if (!rtcore::Load(rtcore_path)) {
        DWORD err = GetLastError();
        std::cerr << "[-] failed to load RTCore64: 0x" << std::hex << err << "\n";
        std::cerr << "    - run as Administrator\n";
        std::cerr << "    - if error 0x20 (sharing violation): driver already loaded\n";
        std::cerr << "    - if error 0x5  (access denied): check seImpersonatePrivilege\n";
        std::cerr << "    - if BSOD on load: driver is on HVCI block list, use different BYOVD\n";
        return 1;
    }
    std::cout << "[+] RTCore64 loaded\n";

    // ── 2. Map our kernel driver ──────────────────────────────────────────────
    std::wcout << L"[*] mapping " << driver_path << L" into kernel...\n";
    bool ok = kdmapper::MapDriver(driver_path);

    // ── 3. Unload RTCore64 (remove evidence) ─────────────────────────────────
    rtcore::Unload();
    std::cout << "[+] RTCore64 unloaded\n";

    if (!ok) {
        std::cerr << "[-] driver mapping failed\n";
        return 1;
    }

    std::cout << "[+] driver mapped and running — launch esp.exe\n";
    return 0;
}
