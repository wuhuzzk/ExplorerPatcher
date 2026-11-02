#pragma once

#include <Windows.h>
#include <commctrl.h>

// CNscTree interprets SPI_GETSCREENREADER as a request to immediately browse
// the selected item on TVN_SELCHANGED. EP also uses this flag to select classic
// context menus, so only suspend that spoof while handling selection changes.
static __declspec(thread) unsigned int s_ExplorerSelectionChangeDepth;

static BOOL Explorer_ShouldSpoofScreenReader(BOOL disableImmersiveMenus, UINT action)
{
    return disableImmersiveMenus && action == SPI_GETSCREENREADER
        && s_ExplorerSelectionChangeDepth == 0;
}

static LRESULT CALLBACK Explorer_NavigationTreeSubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (uMsg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hWnd, Explorer_NavigationTreeSubclassProc, uIdSubclass);
    }

    BOOL selectionChanged = FALSE;
    if (uMsg == WM_NOTIFY && lParam)
    {
        const NMHDR* notification = (const NMHDR*)lParam;
        selectionChanged = notification->code == TVN_SELCHANGEDA
            || notification->code == TVN_SELCHANGEDW;
    }

    if (selectionChanged)
        ++s_ExplorerSelectionChangeDepth;
    LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    if (selectionChanged)
        --s_ExplorerSelectionChangeDepth;
    return result;
}

static void Explorer_SubclassNavigationTree(HWND hWnd)
{
    WCHAR className[64];
    if (GetClassNameW(hWnd, className, ARRAYSIZE(className))
        && !_wcsicmp(className, L"NamespaceTreeControl"))
    {
        SetWindowSubclass(hWnd, Explorer_NavigationTreeSubclassProc,
            (UINT_PTR)Explorer_NavigationTreeSubclassProc, 0);
    }
}
