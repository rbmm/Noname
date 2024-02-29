#include "stdafx.h"

#include "wlog.h"

void WLog::operator >> (HWND hwnd)
{
	PVOID pv = (PVOID)SendMessage(hwnd, EM_GETHANDLE, 0, 0);
	SendMessage(hwnd, EM_SETHANDLE, (WPARAM)_BaseAddress, 0);
	_BaseAddress = 0;
	if (pv)
	{
		LocalFree(pv);
	}
}

ULONG WLog::Init(SIZE_T RegionSize)
{
	if (_BaseAddress = LocalAlloc(0, RegionSize))
	{
		_RegionSize = (ULONG)RegionSize, _Ptr = 0;
		return NOERROR;
	}
	return GetLastError();
}

WLog& WLog::operator ()(PCWSTR format, ...)
{
	va_list args;
	va_start(args, format);

	int len = vswprintf_s(_buf(), _cch(), format, args);

	if (0 < len)
	{
		_Ptr += len * sizeof(WCHAR);
	}

	va_end(args);

	return *this;
}
