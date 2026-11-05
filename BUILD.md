# VRD ESP - Build & Run Guide

## Requirements

| Tool | Version |
|---|---|
| Visual Studio 2022 | with C++ workload |
| Windows Driver Kit (WDK) | 10.0.26100 (matches your SDK) |
| CMake | 3.20+ |
| Git | any |
| OS | Windows 10/11 x64 |
| RTCore64.sys | SHA256: f6f11ad2cd2b0cf95ed42324876bee1d83e01775d8dd92daaea85012e1245fd |

---

## Directory Layout

```
valesp/
- driver/        - kernel driver (vread.sys) - WDK build
- loader/        - BYOVD mapper (loader.exe) - MSVC build
- cheat/         - ESP overlay (esp.exe)     - MSVC build
- external/      - ImGui (cloned by get_imgui.bat)
- offset_scanner.py
```

---

## Step 1 - Get ImGui

```bat
cd external
get_imgui.bat
```

---

## Step 2 - Build the kernel driver

Open a **WDK x64 Command Prompt** (from Start - Windows Kits):

```bat
cd driver
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `driver/build/Release/vread.sys`

If WDK cmake doesn't work: open the WDK sample project in VS2022, add entry.cpp, and build manually.
The linker flags in CMakeLists.txt (DRIVER, NODEFAULTLIB, NATIVE) are what matter.

---

## Step 3 - Build the loader

```bat
cd loader
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `loader/build/Release/loader.exe`

**Copy** `rtcore64.sys` next to `loader.exe`.

---

## Step 4 - Build the ESP

```bat
cd cheat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `cheat/build/Release/esp.exe`

---

## Step 5 - Run order

1. **Disable Secure Boot / HVCI** (required for BYOVD on modern Windows):
   - Settings - Recovery - Advanced startup - Troubleshoot - UEFI Firmware
   - Disable Secure Boot
   - OR: test signing mode: `bcdedit /set testsigning on` (not needed for BYOVD, but
     needed if you want to sign-load the driver directly without RTCore64)

2. **Run loader as Admin:**
   ```bat
   loader.exe ..\driver\build\Release\vread.sys
   ```
   Expected output:
   ```
   [+] RTCore64 loaded
   [+] pool @ 0x...
   [+] driver written to kernel pool
   [+] calling DriverEntry @ 0x...
   [+] DriverEntry returned 0x0
   [+] RTCore64 unloaded
   [+] driver mapped and running - launch esp.exe
   ```

3. **Launch Valorant** - wait for the main menu

4. **Run esp.exe** (normal user, no admin needed after driver is mapped):
   ```bat
   esp.exe
   ```

5. **In-game:**
   - `INSERT` - open/close config menu
   - `END` - clean exit

---

## Offset Updates (after Valorant patches)

After a Valorant patch the offsets in `cheat/offsets.hpp` will break.

**To update:**

1. Dump valorant-win64-shipping.exe memory:
   - Use our driver: add a dump IOCTL, or use a trusted kernel-mode dumper
   - Quick path: use Process Hacker 2 (run before Vanguard loads at next reboot)

2. Run the scanner:
   ```bat
   python3 offset_scanner.py --dump valorant.bin --base 0x7FF700000000
   ```
   (Adjust `--base` to the actual loaded base from Process Hacker or x64dbg)

3. Paste the new RVAs into `offsets.hpp`, rebuild esp.exe only (no need to remap driver)

**Manual pattern hunting (x64dbg / IDA):**
- GWorld: search for string "PersistentLevel" - find xref - nearby `mov rax, [rel]` = GWorld
- GObjects: search for `FUObjectArray` string or find `StaticFindObjectFastInternal` - first `mov rax, [rel]`
- Bone array: find `GetBoneTransform` in vtable - first read of `ComponentSpaceTransformsArray`

---

## Vanguard Notes (2025+)

| Vector | Status | Mitigation used |
|---|---|---|
| Unsigned driver | Blocked by DSE | BYOVD maps without registering |
| Handle to valorant.exe | Blocked in usermode | Driver opens via kernel API (not ObOpenObjectByName from ring 3) |
| MmCopyVirtualMemory calls | Monitored | Copy goes through intermediate pool, not directly to usermode buffer |
| Device object name | Scanned | Use short random name; change per build |
| HVCI | Blocks pool exec | On HVCI systems use DMA hardware (PCIe DMA card + PCILeech) instead |
| Vanguard module scan | Checks PsLoadedModuleList | KDMAPPER never adds to that list |

**If Vanguard blocks RTCore64** (shows up on MS HVCI block list):
- Alternative vulnerable drivers: `iqvw64e.sys` (Intel NIC, CVE-2015-2291), `AsrDrv104.sys` (ASRock)
- Or use a **DMA hardware** approach (FPGA/PCIe card + PCILeech firmware) - most UD, no driver needed

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| loader fails "access denied" | Run as Administrator |
| loader BSOD on RTCore load | HVCI active - disable in UEFI or use different BYOVD |
| esp.exe "cannot open driver" | loader didn't finish; check loader output |
| All zeros / no entities | Offsets stale - run offset_scanner.py after the latest patch |
| Overlay not appearing | Check Valorant is in fullscreen windowed mode (not exclusive fullscreen) |
| Box flickers / wrong position | W2S matrix wrong - verify camera manager offset for current patch |
