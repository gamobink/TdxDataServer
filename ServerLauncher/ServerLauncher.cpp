// ServerLauncher.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include "../Plugin/Plugin.h"

#pragma comment(lib, "ws2_32")

int _tmain(int argc, _TCHAR* argv[])
{
    WSADATA init = {0};
    WSAStartup(MAKEWORD(2,2), &init);

    typedef void (*FF)(PDATAIOFUNC);
    HMODULE h = LoadLibrary("DataServer.dll");
    if (h != INVALID_HANDLE_VALUE) {
        FF func = (FF)GetProcAddress(h, "RegisterDataInterface");
        if (func) {
            func(NULL);
            system("pause");
        }
    }

    WSACleanup();
	return 0;
}

