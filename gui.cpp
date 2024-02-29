#include "stdafx.h"

#include "wlog.h"

void DemoPoc(WLog& log);

HFONT CreateFont()
{
	NONCLIENTMETRICS ncm = { sizeof(ncm) };
	if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
	{
		ncm.lfMenuFont.lfQuality = CLEARTYPE_QUALITY;
		ncm.lfMenuFont.lfPitchAndFamily = FIXED_PITCH|FF_MODERN;
		ncm.lfMenuFont.lfWeight = FW_NORMAL;
		ncm.lfMenuFont.lfHeight = -ncm.iMenuHeight;
		wcscpy(ncm.lfMenuFont.lfFaceName, L"Courier New");

		return CreateFontIndirectW(&ncm.lfMenuFont);
	}

	return 0;
}

void NTAPI ep(void*)
{
#ifndef _WIN64
	PVOID wow_teb;
	if (0 > NtQueryInformationProcess(NtCurrentProcess(), ProcessWow64Information, &wow_teb, sizeof(wow_teb), 0) || !wow_teb)
	{
		MessageBoxW(0, L"ProcessWow64Information", 0, MB_ICONWARNING);
		ExitProcess(0);
	}
#endif

	if (HWND hwnd = CreateWindowExW(0, WC_EDIT, L"Modules", WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0))
	{
		ULONG n = 8;
		SendMessage(hwnd, EM_SETTABSTOPS, 1, (LPARAM)&n);

		HFONT hfont = 0;
		HICON hiS = 0, hiB = 0;

		if (S_OK == LoadIconWithScaleDown((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(1), 
			GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), &hiS))
		{
			SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hiS);
		}

		if (S_OK == LoadIconWithScaleDown((HINSTANCE)&__ImageBase, MAKEINTRESOURCE(1), 
			GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), &hiB))
		{
			SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hiB);
		}

		if (hfont = CreateFont())
		{
			SendMessage(hwnd, WM_SETFONT, (WPARAM)hfont, 0);
		}

		WLog log;
		if (NOERROR == log.Init(0x100000))
		{
			DemoPoc(log);

			log >> hwnd;

			ShowWindow(hwnd, SW_SHOWNORMAL);

			MSG msg;
			while (IsWindow(hwnd) && 0 < GetMessageW(&msg, 0, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
		else
		{
			DestroyWindow(hwnd);
		}

		if (hfont)
		{
			DeleteObject(hfont);
		}

		if (hiB) DestroyIcon(hiB);
		if (hiS) DestroyIcon(hiS);
	}

	ExitProcess(0);
}