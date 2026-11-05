// language: C++, file: driver/entry.cpp, target: Windows 11 x64, WDK 10.0
// Kernel read driver — manually mapped via loader (never via SCM)
// Communication: DeviceIoControl via named device object
// Evasion: randomize DEVICE_NAME each build, strip PDB path in linker flags

#include <ntddk.h>
#include <windef.h>
#include "comms.h"

// ── MmCopyVirtualMemory forward (not in public WDK headers pre-RS) ────────────
extern "C" NTSTATUS MmCopyVirtualMemory(
    PEPROCESS  SourceProcess,
    PVOID      SourceAddress,
    PEPROCESS  TargetProcess,
    PVOID      TargetAddress,
    SIZE_T     BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T    ReturnSize
);

// ── Globals ───────────────────────────────────────────────────────────────────
static PDEVICE_OBJECT  g_Device  = nullptr;
static UNICODE_STRING  g_DevName = {};
static UNICODE_STRING  g_SymLink = {};
static WCHAR           g_DevBuf[64];
static WCHAR           g_SymBuf[64];

// ── Helper: find EPROCESS by PID ─────────────────────────────────────────────
static NTSTATUS GetProcessById(ULONGLONG pid, PEPROCESS* out_process)
{
    return PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, out_process);
}

// ── Helper: safe cross-process copy ──────────────────────────────────────────
static NTSTATUS SafeCopyMemory(
    ULONGLONG src_pid,
    ULONGLONG src_va,
    PVOID     dst_va,     // kernel or usermode buffer
    SIZE_T    size,
    SIZE_T*   out_copied)
{
    PEPROCESS src_proc = nullptr;
    NTSTATUS  status   = GetProcessById(src_pid, &src_proc);
    if (!NT_SUCCESS(status)) return status;

    PEPROCESS cur_proc = PsGetCurrentProcess();

    __try {
        status = MmCopyVirtualMemory(
            src_proc, (PVOID)(ULONG_PTR)src_va,
            cur_proc, dst_va,
            size,
            KernelMode,
            out_copied
        );
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }

    ObDereferenceObject(src_proc);
    return status;
}

// ── Helper: get module base from PEB LDR (walk PsGetProcessWow64Process / LDR) ─
// Walks the target process's LDR in-process — requires attaching to its address space
static NTSTATUS GetModuleBase(
    ULONGLONG   pid,
    const WCHAR* target_name,
    ULONGLONG*  out_base,
    ULONGLONG*  out_size)
{
    PEPROCESS proc = nullptr;
    NTSTATUS status = GetProcessById(pid, &proc);
    if (!NT_SUCCESS(status)) return status;

    KAPC_STATE apc = {};
    KeStackAttachProcess(proc, &apc);

    __try {
        // PEB -> Ldr -> InLoadOrderModuleList
        PPEB peb = PsGetProcessPeb(proc);
        if (!peb || !peb->Ldr) { status = STATUS_NOT_FOUND; __leave; }

        PLIST_ENTRY head = &peb->Ldr->InLoadOrderModuleList;
        PLIST_ENTRY cur  = head->Flink;

        while (cur != head) {
            PLDR_DATA_TABLE_ENTRY entry =
                CONTAINING_RECORD(cur, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (entry->BaseDllName.Buffer) {
                UNICODE_STRING target;
                RtlInitUnicodeString(&target, target_name);

                if (RtlEqualUnicodeString(&entry->BaseDllName, &target, TRUE)) {
                    *out_base = (ULONGLONG)(ULONG_PTR)entry->DllBase;
                    *out_size = entry->SizeOfImage;
                    status = STATUS_SUCCESS;
                    __leave;
                }
            }
            cur = cur->Flink;
        }
        status = STATUS_NOT_FOUND;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }

    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(proc);
    return status;
}

// ── IRP dispatch: CREATE / CLOSE ─────────────────────────────────────────────
static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT, PIRP irp)
{
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// ── IRP dispatch: IOCTL ───────────────────────────────────────────────────────
static NTSTATUS DispatchIoctl(PDEVICE_OBJECT, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code    = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG in_len  = stack->Parameters.DeviceIoControl.InputBufferLength;
    PVOID buf     = irp->AssociatedIrp.SystemBuffer;

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info  = 0;

    switch (code)
    {
    // ── IOCTL_COPY_MEMORY ────────────────────────────────────────────────────
    case IOCTL_COPY_MEMORY:
    {
        if (in_len < sizeof(COPY_MEMORY_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        PCOPY_MEMORY_REQUEST req = (PCOPY_MEMORY_REQUEST)buf;

        // We copy straight into a kernel staging buffer then back to usermode dst
        // This avoids directly passing usermode dst pointer to MmCopyVirtualMemory
        // which can fault if the caller unmaps it between validation and copy
        ULONG  sz    = (ULONG)min(req->size, (ULONGLONG)0x10000); // 64 KB max per call
        PVOID  stage = ExAllocatePool2(POOL_FLAG_NON_PAGED, sz, 'vrdS');
        if (!stage) { status = STATUS_INSUFFICIENT_RESOURCES; break; }

        SIZE_T copied = 0;
        status = SafeCopyMemory(req->src_pid, req->src_address, stage, sz, &copied);

        if (NT_SUCCESS(status)) {
            __try {
                ProbeForWrite((PVOID)(ULONG_PTR)req->dst_address, copied, 1);
                RtlCopyMemory((PVOID)(ULONG_PTR)req->dst_address, stage, copied);
                req->out_bytes_copied = copied;
                info = sizeof(COPY_MEMORY_REQUEST);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                status = GetExceptionCode();
            }
        }

        ExFreePool(stage);
        break;
    }

    // ── IOCTL_GET_MODULE_BASE ────────────────────────────────────────────────
    case IOCTL_GET_MODULE_BASE:
    {
        if (in_len < sizeof(MODULE_BASE_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        PMODULE_BASE_REQUEST req = (PMODULE_BASE_REQUEST)buf;
        status = GetModuleBase(req->process_id,
                               req->module_name,
                               &req->out_base,
                               &req->out_size);
        if (NT_SUCCESS(status))
            info = sizeof(MODULE_BASE_REQUEST);
        break;
    }
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = info;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

// ── Unload ────────────────────────────────────────────────────────────────────
static VOID DriverUnload(PDRIVER_OBJECT driver_obj)
{
    UNREFERENCED_PARAMETER(driver_obj);
    if (g_SymLink.Buffer) IoDeleteSymbolicLink(&g_SymLink);
    if (g_Device)         IoDeleteDevice(g_Device);
}

// ── Entry point (called by KDMAPPER after manual mapping) ─────────────────────
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver_obj, PUNICODE_STRING)
{
    // Build device + symlink names
    RtlStringCbCopyW(g_DevBuf, sizeof(g_DevBuf), DEVICE_NAME);
    RtlStringCbCopyW(g_SymBuf, sizeof(g_SymBuf), SYMLINK_NAME);
    RtlInitUnicodeString(&g_DevName, g_DevBuf);
    RtlInitUnicodeString(&g_SymLink, g_SymBuf);

    NTSTATUS status = IoCreateDevice(
        driver_obj,
        0,
        &g_DevName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_Device
    );
    if (!NT_SUCCESS(status)) return status;

    status = IoCreateSymbolicLink(&g_SymLink, &g_DevName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_Device);
        return status;
    }

    driver_obj->MajorFunction[IRP_MJ_CREATE]         = DispatchCreateClose;
    driver_obj->MajorFunction[IRP_MJ_CLOSE]          = DispatchCreateClose;
    driver_obj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl;
    driver_obj->DriverUnload                          = DriverUnload;

    g_Device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
