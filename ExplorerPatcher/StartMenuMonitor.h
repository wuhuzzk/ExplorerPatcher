#pragma once

#include <Windows.h>

#define EP_START_MONITOR_INIT_MESSAGE L"ExplorerPatcher.StartMonitor.Initialize"

typedef struct StartMenuMonitorSearch
{
    DWORD index;
    DWORD current;
    HMONITOR monitor;
} StartMenuMonitorSearch;

static BOOL CALLBACK StartMenu_EnumMonitor(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM param)
{
    StartMenuMonitorSearch* search = (StartMenuMonitorSearch*)param;
    MONITORINFO info = { sizeof(info) };
    if (GetMonitorInfoW(monitor, &info) && !(info.dwFlags & MONITORINFOF_PRIMARY))
    {
        if (search->current++ == search->index)
        {
            search->monitor = monitor;
            return FALSE;
        }
    }
    return TRUE;
}

// Settings number secondary displays from 2, independently of enumeration order.
static inline HMONITOR StartMenu_MonitorForOverride(DWORD setting)
{
    POINT point = { 0, 0 };
    if (!setting && GetCursorPos(&point))
        return MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    if (setting > 1)
    {
        StartMenuMonitorSearch search = { setting, 2, NULL };
        EnumDisplayMonitors(NULL, NULL, StartMenu_EnumMonitor, (LPARAM)&search);
        if (search.monitor)
            return search.monitor;
    }
    point.x = point.y = 0;
    return MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
}

static inline HWND StartMenu_TaskbarForMonitor(HMONITOR monitor)
{
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (taskbar && MonitorFromWindow(taskbar, MONITOR_DEFAULTTONULL) == monitor)
        return taskbar;
    taskbar = NULL;
    while ((taskbar = FindWindowExW(NULL, taskbar, L"Shell_SecondaryTrayWnd", NULL)) != NULL)
    {
        if (MonitorFromWindow(taskbar, MONITOR_DEFAULTTONULL) == monitor)
            return taskbar;
    }
    return NULL;
}

#ifdef __cplusplus
extern "C" {
#endif
BOOL StartMenu_InstallMonitorHook(void);
BOOL StartMenu_IsMonitorHookInstalled(void);
HMONITOR StartMenu_GetConnectedMonitor(void);
#ifdef __cplusplus
}
#endif
