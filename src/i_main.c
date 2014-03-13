/*
====================================================================

DOOM RETRO
A classic, refined DOOM source port. For Windows PC.

Copyright � 1993-1996 id Software LLC, a ZeniMax Media company.
Copyright � 2005-2014 Simon Howard.
Copyright � 2013-2014 Brad Harding.

This file is part of DOOM RETRO.

DOOM RETRO is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DOOM RETRO is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with DOOM RETRO. If not, see http://www.gnu.org/licenses/.

====================================================================
*/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "d_main.h"
#include "m_argv.h"
#include "SDL.h"
#include "SDL_syswm.h"

typedef BOOL (WINAPI *SetAffinityFunc)(HANDLE hProcess, DWORD mask);

static void I_SetAffinityMask(void)
{
    HMODULE             kernel32_dll;
    SetAffinityFunc     SetAffinity;

    // Find the kernel interface DLL.
    kernel32_dll = LoadLibrary("kernel32.dll");

    if (kernel32_dll == NULL)
    {
        fprintf(stderr, "I_SetAffinityMask: Failed to load kernel32.dll\n");
        return;
    }

    SetAffinity = (SetAffinityFunc)GetProcAddress(kernel32_dll, "SetProcessAffinityMask");

    if (SetAffinity != NULL)
    {
        if (!SetAffinity(GetCurrentProcess(), 1))
        {
            fprintf(stderr, "I_SetAffinityMask: Failed to set process affinity (%d)\n",
                    (int)GetLastError());
        }
    }
}

void I_SetProcessPriority(void)
{
    if (!SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS))
        fprintf(stderr, "Failed to set priority for the process (%d)\n", (int)GetLastError());
}

extern int      fullscreen;
extern boolean  window_focused;
HHOOK           g_hKeyboardHook;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    boolean             bEatKeystroke = false;
    KBDLLHOOKSTRUCT     *p = (KBDLLHOOKSTRUCT *)lParam;

    if (nCode < 0 || nCode != HC_ACTION)
        return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);

    switch (wParam)
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            bEatKeystroke = (window_focused && (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN));
            break;
        }
    }

    return (bEatKeystroke ? 1 : CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam));
}

WNDPROC         oldProc;
HICON           icon;
HWND            hwnd;

boolean MouseShouldBeGrabbed(void);
void ToggleFullScreen(void);
void I_InitGamepad(void);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SETCURSOR)
    {
        if (LOWORD(lParam) == HTCLIENT && !MouseShouldBeGrabbed())
        {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return true;
        }
    }
    else if (msg == WM_SYSCOMMAND && (wParam & 0xfff0) == SC_KEYMENU)
        return false;
    else if (msg == WM_SYSKEYDOWN && wParam == VK_RETURN && !(lParam & 0x40000000))
    {
        ToggleFullScreen();
        return true;
    }
    else if (msg == WM_DEVICECHANGE)
        I_InitGamepad();

    return CallWindowProc(oldProc, hwnd, msg, wParam, lParam);
}

void init_win32(LPCTSTR lpIconName)
{
    HINSTANCE           handle = GetModuleHandle(NULL);
    SDL_SysWMinfo       wminfo;

    SDL_VERSION(&wminfo.version)
    SDL_GetWMInfo(&wminfo);
    hwnd = wminfo.window;

    icon = LoadIcon(handle, lpIconName);
    SetClassLong(hwnd, GCL_HICON, (LONG)icon);

    oldProc = (WNDPROC)SetWindowLong(hwnd, GWL_WNDPROC, (LONG)WndProc);
}

void done_win32(void)
{
    DestroyIcon(icon);
    UnhookWindowsHookEx(g_hKeyboardHook);
}

HANDLE hInstanceMutex;

int main(int argc, char **argv)
{
    char        *mutex = "DOOMRETRO-CC4F1071-8B24-4E91-A207-D792F39636CD";

    hInstanceMutex = CreateMutex(NULL, true, mutex);

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hInstanceMutex)
            CloseHandle(hInstanceMutex);
        SetForegroundWindow(FindWindow(mutex, NULL));
        return 1;
    }

    init_win32("0");

    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

    myargc = argc;
    myargv = argv;

    I_SetAffinityMask();

    I_SetProcessPriority();

    D_DoomMain();
}
