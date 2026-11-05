#pragma once
// language: C, file: driver/comms.h, target: Windows 11 x64 (shared usermode + kernel)
// Shared IOCTL request structures between kernel driver and usermode client

#define DEVICE_NAME     L"\\Device\\vrd9"       // change per session to avoid static sig
#define SYMLINK_NAME    L"\\DosDevices\\vrd9"

#define IOCTL_COPY_MEMORY      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_GET_MODULE_BASE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_PROTECT_PROCESS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

#pragma pack(push, 1)

typedef struct _COPY_MEMORY_REQUEST {
    ULONGLONG src_pid;          // source process id (valorant)
    ULONGLONG src_address;      // virtual address to read from
    ULONGLONG dst_address;      // usermode buffer to write into
    ULONGLONG size;             // bytes to copy
    ULONGLONG out_bytes_copied; // filled by driver on return
} COPY_MEMORY_REQUEST, *PCOPY_MEMORY_REQUEST;

typedef struct _MODULE_BASE_REQUEST {
    ULONGLONG  process_id;
    WCHAR      module_name[260]; // e.g. L"valorant-win64-shipping.exe"
    ULONGLONG  out_base;         // filled by driver
    ULONGLONG  out_size;
} MODULE_BASE_REQUEST, *PMODULE_BASE_REQUEST;

#pragma pack(pop)
