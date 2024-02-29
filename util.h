#pragma once

HRESULT SaveToFile(_In_ PCWSTR lpFileName, _In_ const void* lpBuffer, _In_ ULONG nNumberOfBytesToWrite);

HRESULT PrepareCode(PCWSTR FileName, PULONG64 pb, SIZE_T n);