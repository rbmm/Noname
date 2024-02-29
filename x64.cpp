
#include "stdafx.h"

//#define _PRINT_CPP_NAMES_
#include "../inc/asmfunc.h"

//////////////////////////////////////////////////////////////////////////
//
// CL /cbstring /Oi
//
//////////////////////////////////////////////////////////////////////////

#pragma code_seg(".text$mn$cpp")

PVOID GetNtBase()
{
	return CONTAINING_RECORD(
		NtCurrentTeb()->ProcessEnvironmentBlock->Ldr->InInitializationOrderModuleList.Flink,
		LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks)->DllBase;
}

PVOID __fastcall GetFuncAddress(PIMAGE_DOS_HEADER pidh, PCSTR ProcedureName)
{
	PIMAGE_NT_HEADERS pinth = (PIMAGE_NT_HEADERS)RtlOffsetToPointer(pidh, pidh->e_lfanew);

	PIMAGE_EXPORT_DIRECTORY pied = (PIMAGE_EXPORT_DIRECTORY)RtlOffsetToPointer(pidh, 
		pinth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	PDWORD AddressOfNames = (PDWORD)RtlOffsetToPointer(pidh, pied->AddressOfNames);
	PDWORD AddressOfFunctions = (PDWORD)RtlOffsetToPointer(pidh, pied->AddressOfFunctions);
	PWORD AddressOfNameOrdinals = (PWORD)RtlOffsetToPointer(pidh, pied->AddressOfNameOrdinals);

	DWORD a = 0, b = pied->NumberOfNames, o;

	if (b) 
	{
		do
		{
			int i = strcmp(ProcedureName, RtlOffsetToPointer(pidh, AddressOfNames[o = (a + b) >> 1]));
			if (!i)
			{
				PVOID pv = RtlOffsetToPointer(pidh, AddressOfFunctions[AddressOfNameOrdinals[o]]);

				if ((ULONG_PTR)pv - (ULONG_PTR)pied < pinth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size)
				{
					ANSI_STRING as = { (USHORT)strlen(ProcedureName), as.Length, const_cast<PSTR>(ProcedureName) };
					if (0 > LdrGetProcedureAddress((HMODULE)pidh, &as, 0, &pv)) return 0;
				}

				return pv;
			}

			if (0 > i) b = o; else a = o + 1;

		} while (a < b);
	}

	return 0;
}

PVOID __fastcall GetFuncAddress(PCSTR ProcedureName)
{
	CPP_FUNCTION;
	return GetFuncAddress((PIMAGE_DOS_HEADER)GetNtBase(), ProcedureName);
}

NTSTATUS NtGetProcedureAddress(PVOID ImageBase, PCSTR ProcedureName, _Out_ void** pAddress)
{
	if (PVOID Address = GetFuncAddress((PIMAGE_DOS_HEADER)ImageBase, ProcedureName))
	{
		*pAddress = Address;
		return STATUS_SUCCESS;
	}

	return STATUS_PROCEDURE_NOT_FOUND;
}

EXTERN_C NTSTATUS NTAPI Xyz();

NTSTATUS DoRemoteQuery(HANDLE hSection, HANDLE hProcess)
{
	CPP_FUNCTION;

	HANDLE hThread;
	PVOID BaseAddress = 0;
	SIZE_T ViewSize = 0;
	NTSTATUS status = ZwMapViewOfSection(hSection, hProcess, &BaseAddress, 0, 0, 0, &ViewSize, ViewUnmap, 0, PAGE_READWRITE);

	if (0 <= status)
	{
		union {
			PVOID pv;
			PPS_APC_ROUTINE ApcRoutine;
			PUSER_THREAD_START_ROUTINE StartAddress;
		};

		PVOID hmod = GetNtBase();

		if (0 <= (status = NtGetProcedureAddress(hmod, "RtlExitUserThread", &pv)) &&
			0 <= (status = RtlCreateUserThread(hProcess, 0, TRUE, 0, 0, 0, StartAddress, 0, &hThread, 0)))
		{
			if (0 <= (status = NtGetProcedureAddress(hmod, "LdrQueryProcessModuleInformation", &pv)) &&
				0 <= (status = NtQueueApcThread(hThread, ApcRoutine, 
				BaseAddress, (PVOID)ViewSize, RtlOffsetToPointer(BaseAddress, ViewSize - sizeof(ULONG)))) && 
				0 <= (status = NtResumeThread(hThread, 0)))
			{
				LARGE_INTEGER Timeout;
				Timeout.QuadPart = -20000000; // 2 sec
				status = NtWaitForSingleObject(hThread, FALSE, &Timeout);
			}
			else
			{
				NtTerminateThread(hThread, 0);
			}

			NtClose(hThread);
		}

		ZwUnmapViewOfSection(hProcess, BaseAddress);
	}

	return status;
}

#pragma code_seg(".text$mn$end")

HRESULT X64Entry()ASM_FUNCTION;

SIZE_T SizeOfShellCode()
{
	return (((PBYTE)SizeOfShellCode - (PBYTE)X64Entry) + 7) >> 3;
}

#pragma code_seg()