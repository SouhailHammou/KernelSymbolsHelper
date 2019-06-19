#include "DriverCode.h"

extern DRV_CONTEXT_INFO KmDrvCtx;

//Unexported kernel functions type definitions
typedef	PEPROCESS (*t_PsGetNextProcess)(PEPROCESS Process);
typedef	PETHREAD (*t_PsGetNextProcessThread)(PEPROCESS Process, PKTHREAD Thread);

//Kernel routine pointers
t_PsGetNextProcess PsGetNextProcess;
t_PsGetNextProcessThread PsGetNextProcessThread;


//Structure field offsets
FIELD_DATA KTHREAD_Alertable;
FIELD_DATA KTHREAD_WaitMode;
FIELD_DATA KTHREAD_ApcQueueable;

/*
If no routines or structure fields are needed by your driver,
supply empty arrays.
*/
KRNL_ROUTINE KernelRoutines[] =
{
	{"PsGetNextProcess", (PVOID*)&PsGetNextProcess},
	{"PsGetNextProcessThread", (PVOID*)&PsGetNextProcessThread}
};
KRNL_STRUCT_FIELD KernelStructFields[] =
{
	{"_KTHREAD.Alertable", &KTHREAD_Alertable},
	{"_KTHREAD.WaitMode", &KTHREAD_WaitMode},
	{"_KTHREAD.ApcQueueable", &KTHREAD_ApcQueueable}
};

ULONG NumberOfKernelRoutines = sizeof(KernelRoutines) / sizeof(KRNL_ROUTINE);
ULONG NumberOfKernelStructFields = sizeof(KernelStructFields) / sizeof(KRNL_STRUCT_FIELD);


/*
The system thread will be responsible of cleaning up
after the resolving is made.
*/
HANDLE SystemThreadHandle;
PVOID SystemThreadObject;


NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NTSTATUS Status;

	DriverObject->DriverUnload = DriverUnload;

	/*
	Initializes the resolving kernel module and wait for communication from the user-mode module.
	IMPORTANT : The routine uses IRP_MJ_CREATE/CLOSE and IRP_MJ_DEVICE_CONTROL so be careful not to overwrite those in DriverEntry.
	*/
	Status = KmStartRoutine(DriverObject);
	if (!NT_SUCCESS(Status))
		return Status;

	Status = PsCreateSystemThread(&SystemThreadHandle, GENERIC_ALL, NULL, 0, NULL, SystemThread, DriverObject);
	if (!NT_SUCCESS(Status))
		return Status;

	//Get a pointer to thread object
	Status = ObReferenceObjectByHandle(SystemThreadHandle, GENERIC_ALL, *PsThreadType, KernelMode, &SystemThreadObject, NULL);
	if (!NT_SUCCESS(Status))
		return Status;

	return STATUS_SUCCESS;
}

VOID DriverCode(PDRIVER_OBJECT DriverObject)
/*
IMPORTANT : You write code here using the resolved pointers and offsets
The example provided searches for user-mode alertable threads for APC injection (tested on Win7 64-bit and Win10 64-bit)
*/
{
	PEPROCESS i_Process;
	PETHREAD i_Thread;
	//Iterate through all processes
	for (i_Process = PsGetNextProcess(NULL); i_Process != NULL; i_Process = PsGetNextProcess(i_Process))
	{
		//Iterate through each processes threads
		for (	i_Thread = PsGetNextProcessThread(i_Process, NULL); 
				i_Thread != NULL; 
				i_Thread = PsGetNextProcessThread(i_Process, i_Thread))
		{
			if (KmGetFieldValue(i_Thread, KTHREAD_ApcQueueable) == 1 &&
				KmGetFieldValue(i_Thread, KTHREAD_Alertable) == 1 &&
				KmGetFieldValue(i_Thread, KTHREAD_WaitMode) == UserMode)
			{
				DbgPrint("%p is user-mode Alertable\n", i_Thread);
			}
		}
	}
}


VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	//Wait for the system thread to terminate
	KeWaitForSingleObject(SystemThreadObject, Executive, KernelMode, FALSE, NULL);
	
	//Cleanup the references to the thread object
	ObDereferenceObject(SystemThreadObject);
	ZwClose(SystemThreadHandle);
}

void SystemThread(PVOID StartContext)
{
	PDRIVER_OBJECT DriverObject = (PDRIVER_OBJECT)StartContext;

	if (!KmCleanup(DriverObject))
	{
		return;
	}

	//Invoke our code
	DriverCode(DriverObject);
}
