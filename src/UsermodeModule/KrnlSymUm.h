#pragma once
#include <Windows.h>
#include <winternl.h>
#include <DbgHelp.h>
#include <stdio.h>

#define DBG

#define KM_DEVICE "\\\\.\\KrnlSymDev"

#define INFO_BUFFER_SIZE 32767
//IOCTL
#define IOCTL_RETURN_KR_LENGTH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F30, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RETURN_ROUTINE_NAMES CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F31, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_RESOLVED_RVAS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F32, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_RETURN_SF_LENGTH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F33, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RETURN_FIELDS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F34, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_RESOLVED_OFFSETS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F35, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DONE_RESOLVING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1F36, METHOD_BUFFERED, FILE_ANY_ACCESS)
														

#define UQ_HANDLE (LPVOID)0x1d1d1d1d
#define INVALID_RVA -1
#define INVALID_OFFSET -1
#define STATUS_INFO_LENGTH_MISMATCH      ((NTSTATUS)0xC0000004L)

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

typedef NTSTATUS (*NTAPI t_NtQuerySystemInformation)(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
);

typedef enum
{
	SystemModuleInformation = 11
};

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

enum SymTagEnum {
	SymTagNull,
	SymTagExe,
	SymTagCompiland,
	SymTagCompilandDetails,
	SymTagCompilandEnv,
	SymTagFunction,
	SymTagBlock,
	SymTagData,
	SymTagAnnotation,
	SymTagLabel,
	SymTagPublicSymbol,
	SymTagUDT,
	SymTagEnum,
	SymTagFunctionType,
	SymTagPointerType,
	SymTagArrayType,
	SymTagBaseType,
	SymTagTypedef,
	SymTagBaseClass,
	SymTagFriend,
	SymTagFunctionArgType,
	SymTagFuncDebugStart,
	SymTagFuncDebugEnd,
	SymTagUsingNamespace,
	SymTagVTableShape,
	SymTagVTable,
	SymTagCustom,
	SymTagThunk,
	SymTagCustomType,
	SymTagManagedType,
	SymTagDimension,
	SymTagCallSite,
	SymTagInlineSite,
	SymTagBaseInterface,
	SymTagVectorType,
	SymTagMatrixType,
	SymTagHLSLType
};

DWORD64 InitSymbols(VOID);
VOID DoneResolving(DWORD32 Success);
BOOL ResolveRoutinesForKernelModule(VOID);
BOOL ResolveStructureOffsetsForKernelModule(DWORD64 LoadAddress);
ULONG64 GetKernelRoutineRVA(CHAR* Name);
FIELD_DATA GetKernelStructureFieldOffset(PCHAR StructureField, DWORD64 LoadAddress);
VOID DbgLogMessage(CHAR* Message);
VOID DbgLogLastError(CHAR* Message);
