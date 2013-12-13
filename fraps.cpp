// This is the main DLL file.

#include "fraps.h"

/*
	Author	: Justin S
	Date	: March 14th, 2010

	Credits :
		Matthew L (Azorbix) @ http://www.game-deception.com
		Plenty of background information and examples on hooking (and I used the add_log function from there).

	Tools used:
		Microsoft Visual Studio .NET 2008
		Microsoft DirectX SDK (February 2010)

	Information:
		This D3D helper was developed for an article on Direct3D 9 hooking at http://spazzarama.wordpress.com
*/
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include <windows.h>
#include <fstream>
#include <stdio.h>
#include <d3d9.h>
#include <d3dx9.h>

using namespace std;


//Globals
ofstream ofile;	
char dlldir[320];
HMODULE dllHandle;

bool dllLoaded;


uintptr_t** g_deviceFunctionAddresses;
extern "C" __declspec(dllexport) uintptr_t* APIENTRY GetD3D9DeviceFunctionAddress(short methodIndex)
{
	// There are 119 functions defined in the IDirect3DDevice9 interface (including our 3 IUnknown methods QueryInterface, AddRef, and Release)
	const int interfaceMethodCount = 119;

	// If we do not yet have the addresses, we need to create our own Direct3D Device and determine the addresses of each of the methods
	// Note: to create a Direct3D device we need a valid HWND - in this case we will create a temporary window ourselves - but it could be a HWND
	//       passed through as a parameter of this function or some other initialisation export.
	if (!g_deviceFunctionAddresses) {
		// Ensure d3d9.dll is loaded
		HMODULE hMod = LoadLibraryA("d3d9.dll");
		LPDIRECT3D9       pD3D       = NULL;
		LPDIRECT3DDEVICE9 pd3dDevice = NULL;


		HWND hWnd = CreateWindowA("BUTTON", "Temporary Window", WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 300, 300, NULL, NULL, dllHandle, NULL);
		__try 
		{
			pD3D = Direct3DCreate9( D3D_SDK_VERSION );
			if(pD3D == NULL )
			{
				// TO DO: Respond to failure of Direct3DCreate9
				return 0;
			}
			__try 
			{
				D3DDISPLAYMODE d3ddm;

				if( FAILED( pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm ) ) )
				{
					// TO DO: Respond to failure of GetAdapterDisplayMode
					return 0;
				}

				//
				// Do we support hardware vertex processing? if so, use it. 
				// If not, downgrade to software.
				//
				D3DCAPS9 d3dCaps;
				if( FAILED(pD3D->GetDeviceCaps( D3DADAPTER_DEFAULT, 
												   D3DDEVTYPE_HAL, &d3dCaps ) ) )
				{
					// TO DO: Respond to failure of GetDeviceCaps
					return 0;
				}

				DWORD dwBehaviorFlags = 0;

				if( d3dCaps.VertexProcessingCaps != 0 )
					dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
				else
					dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

				//
				// Everything checks out - create a simple device.
				//

				D3DPRESENT_PARAMETERS d3dpp;
				memset(&d3dpp, 0, sizeof(d3dpp));

				d3dpp.BackBufferFormat       = d3ddm.Format;
				d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
				d3dpp.Windowed               = TRUE;
				d3dpp.EnableAutoDepthStencil = TRUE;
				d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
				d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;

				if( FAILED( pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
												  dwBehaviorFlags, &d3dpp, &pd3dDevice ) ) )
				{
					// Respond to failure of CreateDevice
					return 0;
				}

				//
				// Device has been created - we can now check for the method addresses
				//

				__try 
				{
					// retrieve a pointer to the VTable
					uintptr_t* pInterfaceVTable = (uintptr_t*)*(uintptr_t*)pd3dDevice;
					g_deviceFunctionAddresses = new uintptr_t*[interfaceMethodCount];

					add_log("d3d9.dll base address: 0x%x", hMod);

					// Retrieve the addresses of each of the methods (note first 3 IUnknown methods)
					// See d3d9.h IDirect3D9Device to see the list of methods, the order they appear there
					// is the order they appear in the VTable, 1st one is index 0 and so on.
					for (int i=0; i<interfaceMethodCount; i++) {
						g_deviceFunctionAddresses[i] = (uintptr_t*)pInterfaceVTable[i];
						
						// Log the address offset
						add_log("Method [%i] offset: 0x%x", i, pInterfaceVTable[i] - (uintptr_t)hMod);
					}
				}
				__finally {
					pd3dDevice->Release();
				}
			}
			__finally
			{
				pD3D->Release();
			}
		}
		__finally {
			DestroyWindow(hWnd);
		}
	}

	// Return the address of the method requested
	if (g_deviceFunctionAddresses) {
		return g_deviceFunctionAddresses[methodIndex];
	} else {
		return 0;
	}
}

bool WINAPI DllMain(HMODULE hDll, DWORD dwReason, PVOID pvReserved)
{
	if(dwReason == DLL_PROCESS_ATTACH)
	{
		// No need to call DllMain for each thread start/stop
		DisableThreadLibraryCalls(hDll);

		dllHandle = hDll;

		// Prepare logging
		//GetModuleFileNameA(hDll, dlldir, 512);


		dllLoaded = true;
		return true;
	}

	else if(dwReason == DLL_PROCESS_DETACH)
	{
		
		// Only free memory allocated on the heap if we are exiting due to a dynamic unload 
		// (application termination can leave the heap in an unstable state and we should leave the OS to clean up)
		// http://msdn.microsoft.com/en-us/library/ms682583(VS.85).aspx
		if (!pvReserved) {
			if(g_deviceFunctionAddresses) { delete g_deviceFunctionAddresses; }
		}
	}

    return false;
}

char *GetDirectoryFile(char *filename)
{
	static char path[320];
	strcpy(path, dlldir);
	strcat(path, filename);
	return path;
}

void __cdecl add_log (const char *fmt, ...)
{
	if(ofile == NULL) {
		GetModuleFileNameA(dllHandle, dlldir, 512);
		for(int i = strlen(dlldir); i > 0; i--) { if(dlldir[i] == '\\') { dlldir[i+1] = 0; break; } }
		ofile.open(GetDirectoryFile("d3dHelperLog.txt"), ios::app);
	}

	if(ofile != NULL)
	{
		if(!fmt) { return; }

		va_list va_alist;
		char logbuf[256] = {0};

		va_start (va_alist, fmt);
		_vsnprintf (logbuf+strlen(logbuf), sizeof(logbuf) - strlen(logbuf), fmt, va_alist);
		va_end (va_alist);

		ofile << logbuf << endl;
	}
}


