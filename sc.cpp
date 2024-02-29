#include "stdafx.h"
//#define _PRINT_CPP_NAMES_
#include "../inc/asmfunc.h"

HRESULT __fastcall Code64Call(PVOID arg1, PVOID arg2, PVOID arg3, PVOID arg4, ULONG iApi)ASM_FUNCTION;
HRESULT X64Entry();
SIZE_T SizeOfShellCode();

NTSTATUS DoRemoteQuery(HANDLE hSection, HANDLE hProcess);

#include "util.h"
#include "wlog.h"

typedef struct RTL_PROCESS_MODULE_INFORMATION64
{
	ULONG64 Section;
	ULONG64 MappedBase;
	ULONG64 ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} *PRTL_PROCESS_MODULE_INFORMATION64;

typedef struct RTL_PROCESS_MODULES64
{
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION64 Modules[];
} *PRTL_PROCESS_MODULES64;

void ShowModules(WLog& log, ULONG NumberOfModules, PRTL_PROCESS_MODULE_INFORMATION64 Modules)
{
	ULONG i = 0;
	do 
	{
		PSTR FileName = (PSTR)Modules->FullPathName + Modules->OffsetToFileName;

		if (Modules->OffsetToFileName)
		{
			switch (FileName[-1])
			{
			case '\\':
			case '/':
				FileName[-1] = 0;
			}
		}

		log(L"%03u> %03u %08x %016I64x %08x \"%S\" \"%S\"\r\n", 
			i++, Modules->InitOrderIndex,
			Modules->Flags,
			Modules->ImageBase, 
			Modules->ImageSize, 
			Modules->FullPathName,
			FileName);

	} while (Modules++, --NumberOfModules);
}

NTSTATUS EnumProcModules(WLog& log, ULONG dwProcessId)
{
#ifdef _WIN64
	PrepareCode(L"sc64.asm", (PULONG64)X64Entry, SizeOfShellCode());
#endif

	NTSTATUS status;
	HANDLE hSection, hProcess;
	
	enum { iDoRemoteQuery, SectionSize = 0x100000 };

	LARGE_INTEGER MaximumSize = { SectionSize };

	OBJECT_ATTRIBUTES oa = { sizeof(oa) };
	CLIENT_ID cid = { (HANDLE)(ULONG_PTR)dwProcessId };

	if (0 <= (status = NtCreateSection(&hSection, SECTION_ALL_ACCESS, 0, &MaximumSize, PAGE_READWRITE, SEC_COMMIT, 0)))
	{
		if (0 <= (status = NtOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &oa, &cid)))
		{

#ifdef _WIN64
			status = DoRemoteQuery(hSection, hProcess);
#else
			status = Code64Call(hSection, hProcess, 0, 0, iDoRemoteQuery);
#endif

			NtClose(hProcess);

			union {
				PRTL_PROCESS_MODULES64 ModuleInformation;
				PVOID BaseAddress = 0;
			};

			SIZE_T ViewSize;

			if (0 <= status && 
				0 <= (status = ZwMapViewOfSection(hSection, NtCurrentProcess(), 
				&(BaseAddress = 0), 0, 0, 0, &ViewSize, ViewUnmap, 0, PAGE_READWRITE)))
			{
				status = STATUS_UNSUCCESSFUL;

				ULONG s = *(ULONG*)RtlOffsetToPointer(BaseAddress, ViewSize - sizeof(ULONG));

				if (sizeof(RTL_PROCESS_MODULES64) < s)
				{
					if (ULONG NumberOfModules = ModuleInformation->NumberOfModules)
					{
						s -= sizeof(RTL_PROCESS_MODULES64);

						if (NumberOfModules*sizeof(RTL_PROCESS_MODULE_INFORMATION64) == s)
						{
							status = STATUS_SUCCESS;

							ShowModules(log, NumberOfModules, ModuleInformation->Modules);
						}
					}
				}

				ZwUnmapViewOfSection(NtCurrentProcess(), BaseAddress);
			}
		}
	}

	return status;
}

void DemoPoc(WLog& log)
{
	if (HWND hwnd = GetShellWindow())
	{
		ULONG dwProcessId;
		if (GetWindowThreadProcessId(hwnd, &dwProcessId))
		{
			if (NTSTATUS status = EnumProcModules(log, dwProcessId))
			{
				log(L"error = %x\r\n", status);
			}
		}
	}
}