
#include "zmqfilter.h"


#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "ole32")
#pragma comment(lib, "oleaut32")

#ifdef _DEBUG
#pragma comment(lib, "strmbasd")
#else
#pragma comment(lib, "strmbase")
#endif


#include <streams.h>
#include <olectl.h>
#include <initguid.h>
#include <dllsetup.h>




#define CreateComObject(clsid, iid, var) CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER, iid, (void **)&var);

STDAPI AMovieSetupRegisterServer(CLSID   clsServer, LPCWSTR szDescription, LPCWSTR szFileName, LPCWSTR szThreadingModel = L"Both", LPCWSTR szServerType = L"InprocServer32");
STDAPI AMovieSetupUnregisterServer(CLSID clsServer);


// {A841C788-DC34-4829-98FE-27C385789973}
DEFINE_GUID(CLSID_ZMQVirtualCam,
	0xa841c788, 0xdc34, 0x4829, 0x98, 0xfe, 0x27, 0xc3, 0x85, 0x78, 0x99, 0x73);

const AMOVIESETUP_MEDIATYPE AMSMediaTypesVCam =
{
	&MEDIATYPE_Video,
	&MEDIASUBTYPE_NULL
};

const AMOVIESETUP_PIN AMSPinVCam =
{
	L"Output",             // Pin string name
	FALSE,                 // Is it rendered
	TRUE,                  // Is it an output
	FALSE,                 // Can we have none
	FALSE,                 // Can we have many
	&CLSID_NULL,           // Connects to filter
	NULL,                  // Connects to pin
	1,                     // Number of types
	&AMSMediaTypesVCam      // Pin Media types
};

const AMOVIESETUP_FILTER AMSFilterVCam =
{
	&CLSID_ZMQVirtualCam,  // Filter CLSID
	FILTERNAME,     // String name
	MERIT_DO_NOT_USE,      // Filter merit
	1,                     // Number pins
	&AMSPinVCam             // Pin details
};

CFactoryTemplate g_Templates[] =
{
	{
		FILTERNAME,
		&CLSID_ZMQVirtualCam,
		CVCam::CreateInstance,
		NULL,
		&AMSFilterVCam
	},

};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI RegisterFilters(BOOL bRegister)
{
	HRESULT hr = NOERROR;
	WCHAR achFileName[MAX_PATH];
	char achTemp[MAX_PATH];
	ASSERT(g_hInst != 0);

	if (0 == GetModuleFileNameA(g_hInst, achTemp, sizeof(achTemp)))
		return AmHresultFromWin32(GetLastError());

	MultiByteToWideChar(CP_ACP, 0L, achTemp, lstrlenA(achTemp) + 1,
		achFileName, NUMELMS(achFileName));

	hr = CoInitialize(0);
	if (bRegister)
	{
		hr = AMovieSetupRegisterServer(CLSID_ZMQVirtualCam, FILTERNAME, achFileName, L"Both", L"InprocServer32");
	}

	if (SUCCEEDED(hr))
	{
		IFilterMapper2 *fm = 0;
		hr = CreateComObject(CLSID_FilterMapper2, IID_IFilterMapper2, fm);
		if (SUCCEEDED(hr))
		{
			if (bRegister)
			{
				IMoniker *pMoniker = 0;
				REGFILTER2 rf2;
				rf2.dwVersion = 1;
				rf2.dwMerit = MERIT_DO_NOT_USE;
				rf2.cPins = 1;
				rf2.rgPins = &AMSPinVCam;
				hr = fm->RegisterFilter(CLSID_ZMQVirtualCam, FILTERNAME, &pMoniker, &CLSID_VideoInputDeviceCategory, NULL, &rf2);
			}
			else
			{
				hr = fm->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0, CLSID_ZMQVirtualCam);
			}
		}

		// release interface
		//
		if (fm)
			fm->Release();
	}

	if (SUCCEEDED(hr) && !bRegister)
		hr = AMovieSetupUnregisterServer(CLSID_ZMQVirtualCam);

	CoFreeUnusedLibraries();
	CoUninitialize();
	return hr;
}

STDAPI DllRegisterServer()
{
	return RegisterFilters(TRUE);
}

STDAPI DllUnregisterServer()
{
	return RegisterFilters(FALSE);
}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
