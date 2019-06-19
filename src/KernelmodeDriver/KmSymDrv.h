#pragma once
#include <ntifs.h>
#include <ntddk.h>

#define INVALID_RVA -1
#define INVALID_OFFSET -1

typedef CHAR BYTE, *PBYTE;

#ifdef _X86_
typedef ULONG TVALUE;
typedef ULONG T_OP_PTR;
#elif AMD64
typedef ULONG64 TVALUE;
typedef ULONG64 T_OP_PTR;
#endif

//Structures and enums
typedef struct _FIELD_DATA
{
	union
	{
		struct
		{
			ULONG64 Offset : 32;
			ULONG64 Size : 24;
			ULONG64 BitPosition : 8;
		};
		ULONG64 Data;
	};
} FIELD_DATA, *PFIELD_DATA;

typedef struct _KRNL_ROUTINE
{
	PCHAR FunctionName;
	PVOID* ppRoutine; //pointer to the variable holding the routine address
} KRNL_ROUTINE, *PKRNL_ROUTINE;

typedef struct _KRNL_STRUCT_FIELD
{
	PCHAR StructureField;
	PFIELD_DATA Offset;
} KRNL_STRUCT_FIELD, *PKRNL_STRUCT_FIELD;

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef struct _DRV_CONTEXT_INFO
{
#ifdef _X86_
	T_OP_PTR KernelBase;
#elif AMD64
	T_OP_PTR KernelBase;
#endif
	ULONG KernelImageSz;
	
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING SymLinkName;

	PCHAR RoutineNames;
	ULONG RoutineNamesLength;
	ULONG NumberOfRoutines;
	ULONG RoutineResolvingFailedAtIndex; //The first index where resolving failed

	PCHAR FieldNames;
	ULONG FieldNamesLength;
	ULONG NumberOfFields;
	ULONG FieldResolvingFailedAtIndex;
	
	PKEVENT ResolvingEvent; //Signaled when the resolving is done
	DWORD32 UserModuleCompletedSuccessfully;
	
	HANDLE MainThreadHandle;
} DRV_CONTEXT_INFO, *PDRV_CONTEXT_INFO;

enum
{
	SystemModuleInformation = 11
};

//Prototypes
extern NTSTATUS ZwQuerySystemInformation(
	LONG SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
);

NTSTATUS KmStartRoutine(PDRIVER_OBJECT DriverObject);
NTSTATUS KmCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS KmDevControlDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID KmWaitForResolvingToCleanup(PDRIVER_OBJECT DriverObject);
BOOLEAN KmHasResolvingFailed(PULONG OutFieldIndex, PULONG OutRoutinesIndex);
BOOLEAN KmCleanup(PDRIVER_OBJECT DriverObject);
TVALUE KmGetFieldValue(PVOID Object, FIELD_DATA FieldData);

//IOCTL
#define IOCTL_RETURN_KR_LENGTH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F30, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RETURN_ROUTINE_NAMES CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F31, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_RESOLVED_RVAS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F32, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_RETURN_SF_LENGTH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F33, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RETURN_FIELDS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F34, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_RESOLVED_OFFSETS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F35, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DONE_RESOLVING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F36, METHOD_BUFFERED, FILE_ANY_ACCESS)
