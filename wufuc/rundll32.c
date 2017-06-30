#include <Windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include "service.h"
#include "util.h"

void CALLBACK Rundll32Entry(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
    if (!g_IsWindows7 && !g_IsWindows8Point1) {
        return;
    }

    HANDLE hEvent = OpenEvent(SYNCHRONIZE, FALSE, _T("Global\\wufuc_UnloadEvent"));
    if (hEvent) {
        CloseHandle(hEvent);
        return;
    }
    SC_HANDLE hSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        return;
    }
    TCHAR lpGroupName[256];
    DWORD dwProcessId;
    BOOL result = get_svcpid(hSCManager, _T("wuauserv"), &dwProcessId);
    if (!result && get_svcgname(hSCManager, _T("wuauserv"), lpGroupName, _countof(lpGroupName))) {
        result = get_svcgpid(hSCManager, lpGroupName, &dwProcessId);
    }
    CloseServiceHandle(hSCManager);
    if (!result) {
        return;
    }
    TCHAR lpLibFileName[MAX_PATH + 1];
    GetModuleFileName(HINST_THISCOMPONENT, lpLibFileName, _countof(lpLibFileName));

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
    if (!hProcess) {
        return;
    }
    LPVOID lpBaseAddress = VirtualAllocEx(hProcess, NULL, _countof(lpLibFileName) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (lpBaseAddress && WriteProcessMemory(hProcess, lpBaseAddress, lpLibFileName, _countof(lpLibFileName), NULL)) {

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);
        if (hSnap) {
            MODULEENTRY32 me;
            me.dwSize = sizeof(me);

            if (Module32First(hSnap, &me)) {
                do {
                    if (!_tcsicmp(me.szModule, _T("kernel32.dll"))) {
                        break;
                    }
                } while (Module32Next(hSnap, &me));

                HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(me.hModule, STRINGIZE(LoadLibrary)), lpBaseAddress, 0, NULL);
                CloseHandle(hThread);
            }
            CloseHandle(hSnap);
        }
    }
    CloseHandle(hProcess);
    close_log();
}

void CALLBACK Rundll32Unload(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
    HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, _T("Global\\wufuc_UnloadEvent"));
    if (hEvent) {
        dwprintf(L"Setting unload event...");
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}
