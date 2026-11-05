#pragma once
// language: C++, file: cheat/memory.hpp, target: Windows 11 x64, MSVC
// Cross-process memory reader via our kernel driver IOCTL
// Opens the driver device, sends COPY_MEMORY_REQUEST per read

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include "../driver/comms.h"  // shared IOCTL structures

namespace mem
{

static HANDLE   g_hDriver  = INVALID_HANDLE_VALUE;
static uint64_t g_GamePid  = 0;
static uint64_t g_GameBase = 0;
static uint64_t g_GameSize = 0;

// ── Open our kernel driver device ───────────────────────────────────────────
bool Init()
{
    // Convert DEVICE_NAME to symbolic link path
    // SYMLINK_NAME = \\DosDevices\\vrd9 → opens as \\.\vrd9
    g_hDriver = CreateFileW(L"\\\\.\\vrd9",
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            nullptr);
    if (g_hDriver == INVALID_HANDLE_VALUE) {
        std::cerr << "[-] cannot open driver device (is loader.exe mapped it?): 0x"
                  << std::hex << GetLastError() << "\n";
        return false;
    }
    std::cout << "[+] driver device opened\n";
    return true;
}

// ── Find game process by name ────────────────────────────────────────────────
bool FindGame(const wchar_t* process_name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, process_name) == 0) {
                g_GamePid = pe.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    if (!found) {
        std::wcerr << L"[-] process not found: " << process_name << L"\n";
        return false;
    }

    std::cout << "[+] found " << std::hex;
    std::wcout << process_name;
    std::cout << " pid=0x" << g_GamePid << "\n";

    // Get module base via IOCTL
    MODULE_BASE_REQUEST req = {};
    req.process_id = g_GamePid;
    wcscpy_s(req.module_name, process_name);

    DWORD returned = 0;
    BOOL ok = DeviceIoControl(g_hDriver,
                              IOCTL_GET_MODULE_BASE,
                              &req, sizeof(req),
                              &req, sizeof(req),
                              &returned, nullptr);
    if (!ok || !req.out_base) {
        std::cerr << "[-] IOCTL_GET_MODULE_BASE failed: 0x" << GetLastError() << "\n";
        return false;
    }

    g_GameBase = req.out_base;
    g_GameSize = req.out_size;
    std::cout << "[+] base=0x" << g_GameBase
              << " size=0x" << g_GameSize << "\n";
    return true;
}

// ── Core read: N bytes from process VA → local buffer ───────────────────────
bool ReadRaw(uint64_t address, void* out_buf, size_t size)
{
    if (!address || !out_buf || !size) return false;

    COPY_MEMORY_REQUEST req = {};
    req.src_pid     = g_GamePid;
    req.src_address = address;
    req.dst_address = (uint64_t)(uintptr_t)out_buf;
    req.size        = (uint64_t)size;

    DWORD returned = 0;
    BOOL ok = DeviceIoControl(g_hDriver,
                              IOCTL_COPY_MEMORY,
                              &req, sizeof(req),
                              &req, sizeof(req),
                              &returned, nullptr);
    return (ok && req.out_bytes_copied == size);
}

// ── Typed reads ─────────────────────────────────────────────────────────────
template<typename T>
T Read(uint64_t addr)
{
    T val = {};
    ReadRaw(addr, &val, sizeof(T));
    return val;
}

template<typename T>
T ReadOffset(uint64_t base, uint32_t offset)
{
    return Read<T>(base + offset);
}

uint64_t ReadPtr(uint64_t addr)
{
    return Read<uint64_t>(addr);
}

uint64_t ReadPtrOffset(uint64_t base, uint32_t offset)
{
    return ReadPtr(base + offset);
}

// ── Chained pointer read: base + offset1 → ptr + offset2 → ptr ... ──────────
template<typename T>
T ReadChain(uint64_t base, std::initializer_list<uint32_t> offsets)
{
    uint64_t ptr = base;
    auto it = offsets.begin();
    while (it != offsets.end()) {
        uint32_t off = *it++;
        if (std::next(it) == offsets.end() || it == offsets.end()) {
            // Last offset: read the value
            return Read<T>(ptr + off);
        }
        ptr = ReadPtr(ptr + off);
        if (!ptr) return T{};
    }
    return T{};
}

// ── Game-relative reads ──────────────────────────────────────────────────────
template<typename T>
T ReadGame(uint32_t rva)
{
    return Read<T>(g_GameBase + rva);
}

void Shutdown()
{
    if (g_hDriver != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDriver);
        g_hDriver = INVALID_HANDLE_VALUE;
    }
}

} // namespace mem
