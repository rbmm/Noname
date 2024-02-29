#include "stdafx.h"

BOOL IsSysCallZw(PCSTR name)
{
	return 'Z' == name[0] && 'w' == name[1];
}

ULONG GetZwCount(_In_ PVOID ImageBase, _In_ ULONG NumberOfNames, _In_ PULONG AddressOfNames)
{
	ULONG n = 0;

	do 
	{
		PCSTR name = RtlOffsetToPointer(ImageBase, *AddressOfNames++);

		n += IsSysCallZw(name);

	} while (--NumberOfNames);

	return n;
}

struct SSN  
{
	ULONG Address, hash;

	static int __cdecl Compare(void const* pa, void const* pb)
	{
		ULONG a = reinterpret_cast<const SSN*>(pa)->Address;
		ULONG b = reinterpret_cast<const SSN*>(pb)->Address;

		if (a < b) return -1;
		if (a > b) return +1;
		return 0;
	}
};

ULONG HashString(PCSTR lpsz, ULONG hash = 0)
{
	while (char c = *lpsz++) hash = hash * 33 ^ c;
	return hash;
}

BOOL CreateSSNTable(_In_ PVOID ImageBase, 
					_In_ PIMAGE_EXPORT_DIRECTORY pied,
					_Out_ SSN** ppTable, 
					_Out_ ULONG *pN)
{
	if (ULONG NumberOfNames = pied->NumberOfNames)
	{
		PUSHORT AddressOfNameOrdinals = (PUSHORT)RtlOffsetToPointer(ImageBase, pied->AddressOfNameOrdinals);
		PULONG AddressOfNames = (PULONG)RtlOffsetToPointer(ImageBase, pied->AddressOfNames);
		PULONG AddressOfFunctions = (PULONG)RtlOffsetToPointer(ImageBase, pied->AddressOfFunctions);

		if (ULONG n = GetZwCount(ImageBase, NumberOfNames, AddressOfNames))
		{
			if (SSN* p = new SSN[n])
			{
				*pN = n, *ppTable = p;

				do 
				{
					ULONG rva = *AddressOfNames++;
					USHORT o = *AddressOfNameOrdinals++;
					PCSTR name = RtlOffsetToPointer(ImageBase, rva);

					if (IsSysCallZw(name))
					{
						if (!n--)
						{
							break;
						}

						p->hash = HashString(name + 2), p++->Address = AddressOfFunctions[o];
					}

				} while (--NumberOfNames);

				if (!NumberOfNames)
				{
					qsort(*ppTable, *pN, sizeof(SSN), SSN::Compare);

					return TRUE;
				}

				delete [] *ppTable;
			}
		}
	}

	return FALSE;
}

BOOL UnprotectIAT(_In_ void** pIAT, _Out_ PULONG psize, _Out_ PDWORD lpflOldProtect)
{
	PVOID pv = pIAT;
	while (*pIAT) pIAT++;

	ULONG size = RtlPointerToOffset(pv, pIAT);
	*psize = size;

	return VirtualProtect(pv, size, PAGE_READWRITE, lpflOldProtect);
}

SSN* GetSSN(PCSTR Name, SSN* pTable, ULONG N, PULONG pi)
{
	ULONG i = 0;
	ULONG hash = HashString(Name);
	do 
	{
		if (hash == pTable->hash)
		{
			*pi = i;
			return pTable;
		}
	} while (pTable++, i++, --N);

	return 0;
}

PCSTR IsSysCall(PCSTR Name)
{
	switch (*Name++)
	{
	case 'Z':
		if ('w' == *Name++)
		{
			return Name;
		}
		break;

	case 'N':
		if ('t' == *Name++)
		{
			return Name;
		}
		break;
	}

	return 0;
}

ULONG GetSysCallImportCount(const IMAGE_THUNK_DATA* pINT)
{
	ULONG m = 0;
	while (ULONG_PTR Ordinal = pINT++->u1.Ordinal )
	{
		if (!IMAGE_SNAP_BY_ORDINAL(Ordinal))
		{
			PIMAGE_IMPORT_BY_NAME piibn = (PIMAGE_IMPORT_BY_NAME)RtlOffsetToPointer(&__ImageBase, Ordinal);

			if (IsSysCall(piibn->Name))
			{
				m++;
			}

		}
	}

	return m;
}

PSTR __fastcall strnstr(SIZE_T n1, const void* str1, SIZE_T n2, const void* str2);

void InitByPass(PVOID ImageBase, void** pIAT, const IMAGE_THUNK_DATA* pINT, SSN* pTable, ULONG N)
{
	//void* Wow64Transition = GetProcAddress((HMODULE)ImageBase, "Wow64Transition");

	if (ULONG m = GetSysCallImportCount(pINT))
	{
		// allocate executable memory for bypass

		if (PVOID Buffer = VirtualAlloc(0, m *= 24, MEM_COMMIT, PAGE_EXECUTE_READWRITE))
		{
			union {
				PVOID buf;
				PBYTE pb;
				PULONG pu;
				PULONG64 pu64;
				void** pvv;
			};

			for (buf = Buffer; ULONG_PTR Ordinal = pINT++->u1.Ordinal; pIAT++ )
			{
				if (!IMAGE_SNAP_BY_ORDINAL(Ordinal))
				{
					PIMAGE_IMPORT_BY_NAME piibn = (PIMAGE_IMPORT_BY_NAME)RtlOffsetToPointer(&__ImageBase, Ordinal);

					if (PCSTR Name = IsSysCall(piibn->Name))
					{
						PCSTR error = 0;
						ULONG i;
						if (SSN* p = GetSSN(Name, pTable, N, &i))
						{
							PSTR pv = RtlOffsetToPointer(ImageBase, p->Address);
							static const BYTE SysCall_ret[] = { 0x0F, 0x05, 0xC3 };
							if (pv = strnstr(32, pv, sizeof(SysCall_ret), SysCall_ret))
							{
								*pIAT = buf;

								*pu++ = 0xb8d18b4c;		// mov r10,rcx
								*pu++ = i;				// mov eax,i
								*pu64++ = 0x0125ff48;	// jmp [....]
								*pvv++ = pv - sizeof(SysCall_ret);
							}
							else
							{
								error = "syscall";
							}
						}
						else
						{
							error = "export";
						}

						if (error)
						{
							DbgPrint("!! can not find %s for %s\n", error, Name);
						}
					}
				}
			}

			// optional
			VirtualProtect(Buffer, m, PAGE_EXECUTE, &m);
		}
	}
}

void InitSysCall()
{
	ULONG size;
	// walk own import table
	if (PIMAGE_IMPORT_DESCRIPTOR piid = (PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData(
		&__ImageBase, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size))
	{
		if (size /= sizeof(IMAGE_IMPORT_DESCRIPTOR))
		{
			while (ULONG Name = piid->Name)
			{
				if (_stricmp("ntdll.dll", RtlOffsetToPointer(&__ImageBase, Name)))
				{
					if (!size--)
					{
						break;
					}
					piid++;
					continue;
				}

				// found own import from ntdll.dll
				PVOID ImageBase;
				if (RtlPcToFileHeader(NtCurrentTeb()->ProcessEnvironmentBlock->Ldr, &ImageBase))
				{
					ULONG N;
					SSN* pTable;

					// create table { Adddress, hash(name) }
					// SSN of API is equal to index in this table
					if (PIMAGE_EXPORT_DIRECTORY pied = (PIMAGE_EXPORT_DIRECTORY)
						RtlImageDirectoryEntryToData(ImageBase, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &N))
					{
						if (CreateSSNTable(ImageBase, pied, &pTable, &N))
						{
							void** pIAT = (void**)RtlOffsetToPointer(&__ImageBase, piid->FirstThunk);

							ULONG op;

							// make IAT writable
							if (UnprotectIAT(pIAT, &size, &op))
							{
								InitByPass(ImageBase, pIAT, (PIMAGE_THUNK_DATA)
									RtlOffsetToPointer(&__ImageBase, piid->OriginalFirstThunk), pTable, N);

								// restore IAT protection
								if (PAGE_READWRITE != op) VirtualProtect(pIAT, size, op, &op);
							}

							delete [] pTable;
						}
					}
				}

				break;
			}
		}
	}
}

void DemoPoc();

void NTAPI ep(void*)
{
	DemoPoc();
	InitSysCall();

	OBJECT_ATTRIBUTES oa = { sizeof(oa) };
	CLIENT_ID cid = { (HANDLE)(ULONG_PTR)GetCurrentProcessId() };
	if (0 <= ZwOpenProcess(&oa.RootDirectory, PROCESS_QUERY_INFORMATION, &oa, &cid))
	{
		NtClose(oa.RootDirectory);
	}

	ExitProcess(0);
}
