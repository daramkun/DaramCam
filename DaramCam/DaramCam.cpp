#include "DaramCam.h"

#pragma comment ( lib, "windowscodecs.lib" )
#pragma comment ( lib, "mf.lib" )
#pragma comment ( lib, "mfplat.lib" )
#pragma comment ( lib, "mfreadwrite.lib" )
#pragma comment ( lib, "mfuuid.lib" )
#pragma comment ( lib, "shlwapi.lib" )
#pragma comment ( lib, "dxgi.lib" )
#pragma comment ( lib, "d3d9.lib" )
#pragma comment ( lib, "OpenGL32.lib" )

#include <Psapi.h>
#pragma comment ( lib, "Kernel32.lib" )
#pragma comment ( lib, "Psapi.lib" )

void DCStartup ()
{
	CoInitializeEx ( NULL, COINIT_APARTMENTTHREADED );
	MFStartup ( MF_VERSION, MFSTARTUP_FULL );
}

void DCShutdown ()
{
	MFShutdown ();
	CoUninitialize ();
}

DARAMCAM_EXPORTS void DCGetProcesses ( DWORD * buffer, unsigned * bufferSize )
{
	EnumProcesses ( buffer, sizeof ( DWORD ) * 4096, ( DWORD* ) bufferSize );
}

DARAMCAM_EXPORTS void DCGetProcessName ( DWORD pId, char * nameBuffer, unsigned nameBufferSize )
{
	HANDLE hProcess = OpenProcess ( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pId );
	if ( hProcess != nullptr )
	{
		HMODULE hModule;
		DWORD cbNeeded;
		if ( EnumProcessModules ( hProcess, &hModule, sizeof ( HMODULE ), &cbNeeded ) )
			GetModuleBaseNameA ( hProcess, hModule, nameBuffer, nameBufferSize );
	}
	CloseHandle ( hProcess );
}

DARAMCAM_EXPORTS HWND DCGetActiveWindowFromProcess ( DWORD pId )
{
	struct ENUMWINDOWPARAM { DWORD process; HWND returnHWND; } lParam = { pId, 0 };
	EnumWindows ( [] ( HWND hWnd, LPARAM lParam )
	{
		DWORD pId;
		ENUMWINDOWPARAM * param = ( ENUMWINDOWPARAM* ) lParam;
		GetWindowThreadProcessId ( hWnd, &pId );
		if ( param->process != pId || !( GetWindow ( hWnd, GW_OWNER ) == 0 && IsWindowVisible ( hWnd ) ) || GetWindowLong(hWnd, WS_CHILD ) )
			return 1;

		RECT cr;
		GetClientRect ( hWnd, &cr );
		if ( cr.right - cr.left == 0 || cr.bottom - cr.top == 0 )
			return 1;

		bool hasChild = false;
		EnumChildWindows ( hWnd, [] ( HWND hWnd, LPARAM lParam )
		{
			if ( GetWindow ( hWnd, GW_OWNER ) == 0 && IsWindowVisible ( hWnd ) )
				return 1;
			RECT cr;
			GetClientRect ( hWnd, &cr );
			if ( cr.right - cr.left == 0 || cr.bottom - cr.top == 0 )
				return 1;

			bool * hasChild = ( bool* ) lParam;
			*hasChild = true;
			return 0;
		}, ( LPARAM ) &hasChild );

		if ( !param->returnHWND )
			param->returnHWND = hWnd;

		if ( hasChild )
			return 1;

		param->returnHWND = hWnd;

		return 0;
	}, ( LPARAM ) &lParam );
	return lParam.returnHWND;
}
