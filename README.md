## Description
This small project is aimed at Windows kernel researchers and reverse engineers who wish to invoke undocumented/unexported kernel functions from a kernel driver and/or access members within undocumented structures in a stable way and with the least effort.

The project also helps use undocumented kernel functions/structures without worrying too much about which Windows version the code is running on (as long as you know what you're doing).

Uses of this project for means other than research can have serious security implications.

## General Overview

The project contains a user-mode module and a kernel driver that communicate with each other. The driver supplies all the names of the functions and structure fields to resolve to the user-mode module. The latter resolves the functions' RVAs and structure offsets using the symbol server, communicates them to the driver, and then terminates.

To run, you have to manually load the driver first then execute the user-mode module.

## How to use

As an example, the kernel code included with this project looks up all user-mode alertable threads for user-mode APC injection, it has been tested under Windows 7 x64, Windows 10 x64.
As a user of this project, you will only be modifying a single file within the kernel module : `DriverCode.c`. 

Within this file we define the types and pointers to the undocumented kernel routines your code will be calling (`PsGetNextProcess` and `PsGetNextProcessThread` in our example).

```C
//Unexported kernel functions type definitions
typedef PEPROCESS (*t_PsGetNextProcess)(PEPROCESS Process);
typedef PETHREAD (*t_PsGetNextProcessThread)(PEPROCESS Process, PKTHREAD Thread);

//Kernel routine pointers
t_PsGetNextProcess PsGetNextProcess;
t_PsGetNextProcessThread PsGetNextProcessThread;
```

Next, we define storage for each structure offset we're going to need.

```C
//Structure field offsets
FIELD_DATA KTHREAD_Alertable;
FIELD_DATA KTHREAD_WaitMode;
FIELD_DATA KTHREAD_ApcQueueable;
```
The `FIELD_DATA` type encodes an offset to a structure field as shown below.

```C
typedef struct _FIELD_DATA
{
  union
  {
	  struct
	  {
		  ULONG64 Offset : 32; //The offset to the structure field
		  ULONG64 Size : 24; //Member size
		  ULONG64 BitPosition : 8;//The bit-position in case of a bitfield
	  };
	  ULONG64 Data;
  };
} FIELD_DATA, *PFIELD_DATA;
```

Now let's define an array of `KRNL_ROUTINE` elements each containing the name of the routine and a pointer to the corresponding variable we've created previously.

```C
KRNL_ROUTINE KernelRoutines[] =
{
	{"PsGetNextProcess", (PVOID*)&PsGetNextProcess},
	{"PsGetNextProcessThread", (PVOID*)&PsGetNextProcessThread}
};
```

We do the same with the type `KRNL_STRUCT_FIELD` for the structure members.

```C
KRNL_STRUCT_FIELD KernelStructFields[] =
{
	{"_KTHREAD.Alertable", &KTHREAD_Alertable},
	{"_KTHREAD.WaitMode", &KTHREAD_WaitMode},
	{"_KTHREAD.ApcQueueable", &KTHREAD_ApcQueueable}
};
```

The code making use of the resolved pointers/offsets must be written in the function `DriverCode` which is invoked inside a system thread.

To access the structure members we call `KmGetFieldValue`.

```C
VOID DriverCode(PDRIVER_OBJECT DriverObject)
{
	PEPROCESS i_Process;
	PETHREAD i_Thread;
	//Iterate through all processes
	for (i_Process = PsGetNextProcess(NULL); i_Process != NULL; i_Process = PsGetNextProcess(i_Process))
	{
		//Iterate through each processe's threads
		for (i_Thread = PsGetNextProcessThread(i_Process, NULL); i_Thread != NULL; i_Thread = PsGetNextProcessThread(i_Process, i_Thread))
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
```

## Important
Both DLLs `dbghelp.dll` and `symsrv.dll` must be present in the same directory as the user-mode module's executable.

