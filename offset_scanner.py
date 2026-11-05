#!/usr/bin/env python3
# file: offset_scanner.py
# Scan a live Valorant process for engine offsets using byte patterns.
# Run from a kernel-read context (via driver) or with a dumped memory region.
# Usage: python3 offset_scanner.py --dump valorant.bin --base 0x140000000
# Or with live reading (requires ctypes + our driver DeviceIoControl wrapper):
#   python3 offset_scanner.py --live --pid <PID>

import struct
import argparse
import sys
import os

# ── Patterns (sig: bytes with ?? wildcards, offset_in_sig: RIP+rel32 resolution) ──
PATTERNS = {
    "GObjects": {
        "sig": "48 8B 05 ?? ?? ?? ?? 48 8D 1C C8",
        "type": "rip_rel32",
        "instr_offset": 3,   # offset to rel32 within the instruction
        "instr_len": 7,
    },
    "GWorld": {
        "sig": "48 8B 1D ?? ?? ?? ?? 48 85 DB",
        "type": "rip_rel32",
        "instr_offset": 3,
        "instr_len": 7,
    },
    "GNames": {
        "sig": "48 8D 05 ?? ?? ?? ?? 48 89 05 ?? ?? ?? ?? EB",
        "type": "rip_rel32",
        "instr_offset": 3,
        "instr_len": 7,
    },
}

def parse_sig(sig_str):
    """Convert '48 8B 05 ?? ??' to list of (byte|None)."""
    parts = sig_str.strip().split()
    out = []
    for p in parts:
        out.append(None if p == "??" else int(p, 16))
    return out

def find_pattern(data, sig_bytes):
    """Return file offset of first match, or -1."""
    pat = sig_bytes
    plen = len(pat)
    for i in range(len(data) - plen):
        match = True
        for j, b in enumerate(pat):
            if b is not None and data[i+j] != b:
                match = False
                break
        if match:
            return i
    return -1

def resolve_rip_rel32(file_offset, instr_offset, instr_len, image_base, section_va, section_raw):
    """Given a sig match at file_offset, compute the final VA the instruction points to."""
    rel_offset_in_sig = instr_offset
    rel32_file = file_offset + rel_offset_in_sig
    # rel32 is 4 bytes LE
    rel32 = struct.unpack_from("<i", data_buf, rel32_file)[0]
    # RIP = VA of next instruction
    # VA of sig = image_base + (file_offset - section_raw + section_va)
    sig_va = image_base + (file_offset - section_raw + section_va)
    next_instr_va = sig_va + instr_len
    target_va = (next_instr_va + rel32) & 0xFFFFFFFFFFFFFFFF
    return target_va

data_buf = None

def main():
    global data_buf

    parser = argparse.ArgumentParser()
    parser.add_argument("--dump", help="Path to memory dump of valorant-win64-shipping.exe")
    parser.add_argument("--base", default="0x140000000", help="Image base (hex)")
    args = parser.parse_args()

    if not args.dump:
        print("[-] --dump <file> required (dump game memory first)")
        print("    To dump: use Process Hacker or our driver's ReadRaw to dump the .exe region")
        sys.exit(1)

    if not os.path.exists(args.dump):
        print(f"[-] file not found: {args.dump}")
        sys.exit(1)

    image_base = int(args.base, 16)

    with open(args.dump, "rb") as f:
        data_buf = bytearray(f.read())

    print(f"[*] loaded {len(data_buf):#x} bytes, base={image_base:#x}")

    # Parse PE to find .text section
    dos_magic = struct.unpack_from("<H", data_buf, 0)[0]
    if dos_magic != 0x5A4D:
        print("[-] not a valid PE dump (MZ missing)")
        sys.exit(1)

    pe_off   = struct.unpack_from("<I", data_buf, 0x3C)[0]
    num_secs = struct.unpack_from("<H", data_buf, pe_off + 0x6)[0]
    opt_size = struct.unpack_from("<H", data_buf, pe_off + 0x14)[0]
    sec_base = pe_off + 0x18 + opt_size

    sections = []
    for i in range(num_secs):
        off = sec_base + i * 0x28
        name     = data_buf[off:off+8].rstrip(b'\x00').decode("ascii", errors="replace")
        vsize    = struct.unpack_from("<I", data_buf, off + 0x08)[0]
        va       = struct.unpack_from("<I", data_buf, off + 0x0C)[0]
        raw_size = struct.unpack_from("<I", data_buf, off + 0x10)[0]
        raw_off  = struct.unpack_from("<I", data_buf, off + 0x14)[0]
        sections.append((name, va, vsize, raw_off, raw_size))

    print(f"[*] {num_secs} sections found")

    results = {}
    for pat_name, pat_info in PATTERNS.items():
        sig_bytes = parse_sig(pat_info["sig"])
        # Search in .text section primarily
        for sec_name, va, vsize, raw_off, raw_size in sections:
            if sec_name not in (".text", "PAGE", "INIT") and raw_size > 0x1000000:
                continue  # skip huge sections for speed
            if raw_off + raw_size > len(data_buf):
                continue

            section_view = data_buf[raw_off: raw_off + raw_size]

            # Search within section
            plen = len(sig_bytes)
            found = -1
            for i in range(len(section_view) - plen):
                ok = True
                for j, b in enumerate(sig_bytes):
                    if b is not None and section_view[i+j] != b:
                        ok = False
                        break
                if ok:
                    found = i
                    break

            if found >= 0:
                file_off = raw_off + found
                if pat_info["type"] == "rip_rel32":
                    va_result = resolve_rip_rel32(
                        file_off,
                        pat_info["instr_offset"],
                        pat_info["instr_len"],
                        image_base, va, raw_off
                    )
                    # Store as RVA
                    rva = va_result - image_base
                    results[pat_name] = rva
                    print(f"[+] {pat_name}: VA={va_result:#x}  RVA={rva:#x}")
                break

    print("\n── Paste into offsets.hpp ───────────────────────────────────")
    for name, rva in results.items():
        print(f"constexpr uint64_t {name:<20} = {rva:#010x};")

if __name__ == "__main__":
    main()
