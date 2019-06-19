#include "KrnlSymDrv.h"

#define DEBUG

DRV_CONTEXT_INFO KmDrvCtx;

//Defined in DriverCode.c
extern KRNL_ROUTINE KernelRoutines[];
extern KRNL_STRUCT_FIELD KernelStructFields[];
extern ULONG NumberOfKernelRoutines;
extern ULONG NumberOfKernelStructFields;

VOID KmWaitForResolvingToCleanup(PDRIVER_OBJECT DriverObject)
/*
This routine waits for the event that is signaled
*/
{
	if (KmDrvCtx.ResolvingEvent)
	{
		KeWaitForSingleObject(KmDrvCtx.ResolvingEvent, Executive, KernelMode, FALSE, NULL);
		ExFreePool(KmDrvCtx.ResolvingEvent);
		KmDrvCtx.ResolvingEvent = NULL;
	}
	
	if (DriverObject)
	{
		DriverObject->MajorFunction[IRP_MJ_CREATE] = NULL;
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = NULL;
		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NULL;
	}

	if (KmDrvCtx.DeviceObject)
	{
		IoDeleteDevice(KmDrvCtx.DeviceObject);
		KmDrvCtx.DeviceObject = NULL;
	}
	
	IoDeleteSymbolicLink(&KmDrvCtx.SymLinkName);

	return;
}

BOOLEAN KmHasResolvingFailed(PULONG OutFieldIndex, PULONG OutRoutinesIndex)
/*
Checks if the resolving of either kernel routines or structure fields has failed.
We can get the exact index in which the resolving has failed by supplying OUT variables.
The default index value is -1 which means that no errors were encountered.
*/
{
	if (OutFieldIndex)
		*OutFieldIndex = KmDrvCtx.FieldResolvingFailedAtIndex;
	if (OutRoutinesIndex)
		*OutRoutinesIndex = KmDrvCtx.RoutineResolvingFailedAtIndex;

	if (KmDrvCtx.FieldResolvingFailedAtIndex != -1 || KmDrvCtx.RoutineResolvingFailedAtIndex != -1 || ! KmDrvCtx.UserModuleCompletedSuccessfully )
		return TRUE; //Failed
	
	return FALSE;
}

BOOLEAN KmCleanup(PDRIVER_OBJECT DriverObject)
/*
1. Waits for resolving to end.
2. Cleans up : Deletes the device and symlink and removes the IRP dispatch routines.
3. If the resolving had failed it returns, in the corresponding OUT variable, the index in the array which failed to resolve, otherwise -1. 
*/
{
	ULONG FieldsErrorIndex, RoutinesErrorIndex;

	//Wait synchronously for the resolving to end
	KmWaitForResolvingToCleanup(DriverObject);

	//Check if the resolving failed at a kernel routine or a structure field
	if (KmHasResolvingFailed(&FieldsErrorIndex, &RoutinesErrorIndex))
	{
		//Resolving had failed
#ifdef DEBUG
		if (!KmDrvCtx.UserModuleCompletedSuccessfully)
		{
			DbgPrint("An error occured in the user-mode module !\n");
		}
		if (FieldsErrorIndex != -1)
		{
			DbgPrint("Failed to resolve field at index : %d\n", FieldsErrorIndex);
		}
		if (RoutinesErrorIndex != -1)
		{
			DbgPrint("Failed to resolve kernel routine at index : %d\n", RoutinesErrorIndex);
		}
#endif
		return FALSE;
	}
	//Resolving had succeeded
	return TRUE;
}

NTSTATUS KmStartRoutine(PDRIVER_OBJECT DriverObject)
{
	ULONG ModuleInfoLength = PAGE_SIZE, ReturnLength, i;
	PRTL_PROCESS_MODULES ModuleInfo = NULL;
	NTSTATUS Status;
	UNICODE_STRING DeviceName;
	PCHAR i_RoutineNames, i_FieldNames;

	/*
	Get ntoskrnl.exe image base
	*/
	Status = ZwQuerySystemInformation(SystemModuleInformation, NULL, 0, &ReturnLength);
	if (Status == STATUS_INFO_LENGTH_MISMATCH)
	{
		ModuleInfoLength = ReturnLength;
		ModuleInfo = ExAllocatePool(PagedPool, ReturnLength);
		Status = ZwQuerySystemInformation(SystemModuleInformation, ModuleInfo, ModuleInfoLength, &ReturnLength);
		if (!NT_SUCCESS(Status))
		{
			goto cleanup;
		}
	}
	else if (!NT_SUCCESS(Status))
	{
		goto cleanup;
	}

	if (ModuleInfo->NumberOfModules)
	{
		//ntoskrnl.exe is the first module
		KmDrvCtx.KernelBase = (T_OP_PTR)ModuleInfo->Modules[0].ImageBase;
		KmDrvCtx.KernelImageSz = ModuleInfo->Modules[0].ImageSize;
	}
	else
	{
		Status = STATUS_UNSUCCESSFUL;
		goto cleanup;
	}

	/*
	Calculate the length of the kernel routine names
	*/
	KmDrvCtx.NumberOfRoutines = NumberOfKernelRoutines;
	for (i = 0; i < KmDrvCtx.NumberOfRoutines; i++)
	{
		PCHAR RoutineName = KernelRoutines[i].FunctionName;
		if (!RoutineName)
		{
			Status = STATUS_UNSUCCESSFUL;
			goto cleanup;
		}
		KmDrvCtx.RoutineNamesLength += strlen(RoutineName) + 1;
	}

	/*
	Calculate the length of the structure field strings
	*/
	KmDrvCtx.NumberOfFields = NumberOfKernelStructFields;
	for (i = 0; i < KmDrvCtx.NumberOfFields; i++)
	{
		PCHAR FieldName = KernelStructFields[i].StructureField;
		if (!FieldName)
		{
			Status = STATUS_UNSUCCESSFUL;
			goto cleanup;
		}
		KmDrvCtx.FieldNamesLength += strlen(FieldName) + 1;
	}


	/*
	Concatenate the kernel routine names to prepare them for
	the user-mode module.
	*/
	i_RoutineNames = KmDrvCtx.RoutineNames = ExAllocatePool(PagedPool, KmDrvCtx.RoutineNamesLength);
	RtlZeroMemory(KmDrvCtx.RoutineNames, KmDrvCtx.RoutineNamesLength);
	for (i = 0; i < KmDrvCtx.NumberOfRoutines; i++)
	{
		strcpy(i_RoutineNames, KernelRoutines[i].FunctionName);
		i_RoutineNames += strlen(i_RoutineNames) + 1;
	}

	/*
	Concatenate the structure field strings to prepare them for
	the user-mode module.
	*/
	i_FieldNames = KmDrvCtx.FieldNames = ExAllocatePool(PagedPool, KmDrvCtx.FieldNamesLength);
	RtlZeroMemory(KmDrvCtx.FieldNames, KmDrvCtx.FieldNamesLength);
	for (i = 0; i < KmDrvCtx.NumberOfFields; i++)
	{
		strcpy(i_FieldNames, KernelStructFields[i].StructureField);
		i_FieldNames += strlen(i_FieldNames) + 1;
	}


	/*
	Create the event that is signaled after the functions are resolved
	and create the system thread which contains the start of the real
	driver code.
	*/
	KmDrvCtx.ResolvingEvent = ExAllocatePool(NonPagedPool, sizeof(KEVENT));
	KeInitializeEvent(KmDrvCtx.ResolvingEvent, NotificationEvent, FALSE);


	KmDrvCtx.RoutineResolvingFailedAtIndex = -1;
	KmDrvCtx.FieldResolvingFailedAtIndex = -1;

	/*
	Create a temporary device object that is deleted after the user-mode
	module resolves the addresses of the routines.
	*/
	RtlCreateUnicodeString(&DeviceName, L"\\Device\\KrnlSymDev");
	Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &KmDrvCtx.DeviceObject);
	
	if (!NT_SUCCESS(Status))
		return Status;

	RtlCreateUnicodeString(&KmDrvCtx.SymLinkName, L"\\DosDevices\\KrnlSymDev");
	Status = IoCreateSymbolicLink(&KmDrvCtx.SymLinkName, &DeviceName);
	if (!NT_SUCCESS(Status))
		return Status;
	
	/*
	Setup the device control dispatch routine invoked
	through the user-mode module.
	*/
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KmDevControlDispatch;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = KmCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = KmCreateClose;

	Status = STATUS_SUCCESS;
cleanup:
	if (ModuleInfo)
		ExFreePool(ModuleInfo);
	return Status;
}

NTSTATUS KmCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS KmDevControlDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
/*
	The user-mode module resolves the kernel routines by sending
	the following IOCTLs in order :

	1.	Sends a first IOCTL to get the number of routines and their lengths.

	2.	Sends a second IOCTL to receive the names of the required routines.
		The user-mode modules resolves the offsets using the symbols server
		for the ntoskrnl.

	3.	Sends a third IOCTL with the results. The device is deleted and the
		user-mode module terminates.

	
	The user-mode module also resolves offsets to structure fields including
	bitfields. To accomplish this, it issues these IOCTLs in the following order :

	4.	A first IOCTL to get the number of structure fields and their corresponding length.

	5.	A second IOCTL to fetch the structure field strings.

	6.	A third IOCTL containing the offsets. If the field is a bitfield , the bitposition
		is stored in the high byte of the DWORD containing the offset.

	
	Before the user-mode exits it sends a last IOCTL so that the driver can signal an event
	that allows the kernel resolving module to cleanup.
*/
{
	PIO_STACK_LOCATION StackLocation = IoGetCurrentIrpStackLocation(Irp);
	ULONG IoControlCode = StackLocation->Parameters.DeviceIoControl.IoControlCode;
	ULONG InputBufferLength = StackLocation->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputBufferLength = StackLocation->Parameters.DeviceIoControl.OutputBufferLength;
	PVOID SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG DataLength = 0;

	switch (IoControlCode)
	{

	//*
	//IOCTLs for resolving kernel routines
	//*

	case IOCTL_RETURN_KR_LENGTH: // See 1.
	{
		if (OutputBufferLength >= sizeof(ULONG) * 2)
		{
			PULONG RoutinesInfo = SystemBuffer;
			RoutinesInfo[0] = KmDrvCtx.RoutineNamesLength;
			RoutinesInfo[1] = KmDrvCtx.NumberOfRoutines; //Number of kernel routines
			DataLength = sizeof(ULONG) * 2;
		}
		else
		{
			Status = STATUS_UNSUCCESSFUL;
		}
		break;
	}
	
	case IOCTL_RETURN_ROUTINE_NAMES: //See 2.
	{
		if (OutputBufferLength >= KmDrvCtx.RoutineNamesLength)
		{
			RtlCopyMemory(SystemBuffer, KmDrvCtx.RoutineNames, KmDrvCtx.RoutineNamesLength);
			DataLength = KmDrvCtx.RoutineNamesLength;
		}
		else
		{
			Status = STATUS_UNSUCCESSFUL;
		}
		break;
	}

	case IOCTL_SET_RESOLVED_RVAS: //See 3.
	{
		ULONG i;
		PULONG64 RVAs = SystemBuffer;

		if (InputBufferLength < KmDrvCtx.NumberOfRoutines * sizeof(ULONG64))
		{
			Status = STATUS_UNSUCCESSFUL;
			break;
		}

		for ( i = 0; i < KmDrvCtx.NumberOfRoutines; i++)
		{
			ULONG64 RVA;
			PVOID RoutineAddress, *ppRoutine;
			
			RVA = RVAs[i];
			ppRoutine = KernelRoutines[i].ppRoutine;
			RoutineAddress = (PVOID)(RVA + KmDrvCtx.KernelBase);
			
			if ( 
				!ppRoutine ||
				RVA == INVALID_RVA ||
				RoutineAddress >= (PVOID)(KmDrvCtx.KernelBase + KmDrvCtx.KernelImageSz)
				)
			{
				Status = STATUS_UNSUCCESSFUL;
				KmDrvCtx.RoutineResolvingFailedAtIndex = i;
				break;
			}
			*ppRoutine = RoutineAddress;
		}
	

		if ( i == KmDrvCtx.NumberOfRoutines )
			KmDrvCtx.RoutineResolvingFailedAtIndex = -1;
		
		break;
	}

	//*
	//IOCTLs for resolving structure field offsets
	//*
	case IOCTL_RETURN_SF_LENGTH: //See 4.
	{
		if (OutputBufferLength >= sizeof(ULONG) * 2)
		{
			PULONG FieldsInfo = SystemBuffer;
			FieldsInfo[0] = KmDrvCtx.FieldNamesLength;
			FieldsInfo[1] = KmDrvCtx.NumberOfFields;
			DataLength = sizeof(ULONG) * 2;
		}
		else
		{
			Status = STATUS_UNSUCCESSFUL;
		}
		break;
	}

	case IOCTL_RETURN_FIELDS : //See 5.
	{
		if (OutputBufferLength >= KmDrvCtx.FieldNamesLength)
		{
			RtlCopyMemory(SystemBuffer, KmDrvCtx.FieldNames, KmDrvCtx.FieldNamesLength);
			DataLength = KmDrvCtx.FieldNamesLength;
		}
		else
		{
			Status = STATUS_UNSUCCESSFUL;
		}
		break;
	}
	
	case IOCTL_SET_RESOLVED_OFFSETS: //See 6.
	{
		if (InputBufferLength < KmDrvCtx.NumberOfFields * sizeof(FIELD_DATA))
		{
			Status = STATUS_UNSUCCESSFUL;
			break;
		}
		
		PFIELD_DATA Offsets = (PFIELD_DATA)SystemBuffer;
		ULONG i;
		for (i = 0; i < KmDrvCtx.NumberOfFields; i++)
		{
			FIELD_DATA Offset = Offsets[i];
			PFIELD_DATA pOffset = KernelStructFields[i].Offset;

			if (!pOffset || Offset.Data == INVALID_OFFSET)
			{
				Status = STATUS_UNSUCCESSFUL;
				KmDrvCtx.FieldResolvingFailedAtIndex = i;
				break;
			}
			*pOffset = Offset;
		}

		if (i == KmDrvCtx.NumberOfFields)
			KmDrvCtx.FieldResolvingFailedAtIndex = -1;

		break;
	}

	case IOCTL_DONE_RESOLVING:
	{
		if (InputBufferLength != sizeof(DWORD32))
		{
			Status = STATUS_UNSUCCESSFUL;
			break;
		}

		KmDrvCtx.UserModuleCompletedSuccessfully = *(PDWORD32)SystemBuffer;

		if (KmDrvCtx.ResolvingEvent)
			KeSetEvent(KmDrvCtx.ResolvingEvent, IO_NO_INCREMENT, FALSE);
		else
			Status = STATUS_UNSUCCESSFUL;
		
		break;
	}

	default :
		Status = STATUS_NOT_IMPLEMENTED;
		break;
	}


	StackLocation->Parameters.DeviceIoControl.OutputBufferLength = DataLength;
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = DataLength;
	
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


TVALUE KmGetFieldValue(PVOID Object, FIELD_DATA FieldData)
{	
	ULONG i;
	TVALUE Value, SizeMask;

	for (i = 0, SizeMask = 0; i < FieldData.Size ; i++)
	{
		SizeMask |= 1 << i;
	}
	
	if (FieldData.BitPosition)
	{
		SizeMask <<= FieldData.BitPosition;
	}

	Value = *(TVALUE*)((PBYTE)Object + FieldData.Offset) & SizeMask;
	
	if (FieldData.BitPosition)
	{
		Value >>= FieldData.BitPosition;
	}

	return Value;
}	
