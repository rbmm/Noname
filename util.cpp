#include "stdafx.h"
#include "util.h"

HRESULT SaveToFile(_In_ PCWSTR lpFileName, _In_ const void* lpBuffer, _In_ ULONG nNumberOfBytesToWrite)
{
	UNICODE_STRING ObjectName;

	NTSTATUS status = RtlDosPathNameToNtPathName_U_WithStatus(lpFileName, &ObjectName, 0, 0);

	if (0 <= status)
	{
		HANDLE hFile;
		IO_STATUS_BLOCK iosb;
		OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName, OBJ_CASE_INSENSITIVE };

		LARGE_INTEGER AllocationSize = { nNumberOfBytesToWrite };

		status = NtCreateFile(&hFile, FILE_APPEND_DATA|SYNCHRONIZE, &oa, &iosb, &AllocationSize,
			0, 0, FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE, 0, 0);

		RtlFreeUnicodeString(&ObjectName);

		if (0 <= status)
		{
			status = NtWriteFile(hFile, 0, 0, 0, &iosb, const_cast<void*>(lpBuffer), nNumberOfBytesToWrite, 0, 0);
			NtClose(hFile);
		}
	}

	return status ? HRESULT_FROM_NT(status) : S_OK;
}

HRESULT PrepareCode(PCWSTR FileName, PULONG64 pb, SIZE_T n)
{
	HRESULT hr = E_OUTOFMEMORY;

	SIZE_T cch = n * (7 + 16) + 1;

	if (PSTR buf = new char[cch])
	{
		hr = ERROR_INTERNAL_ERROR;

		int len;

		PSTR psz = buf;

		do 
		{
			if (0 >= (len = sprintf_s(psz, cch, "DQ 0%016I64xh\r\n", *pb++)))
			{
				break;
			}

		} while (psz += len, cch -= len, --n);

		if (!n)
		{
			hr = SaveToFile(FileName, buf, RtlPointerToOffset(buf, psz));
		}

		delete [] buf;
	}

	return hr;
}