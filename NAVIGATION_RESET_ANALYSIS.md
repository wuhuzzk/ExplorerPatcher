# Explorer navigation reset on device and shell changes

## Symptom and requirements

An existing File Explorer window switches from a local folder to the Desktop
shell namespace when a USB drive is inserted/removed. Opening a file can also
trigger the reported symptom. Switching the command bar back to Windows 11
does not disable the responsible hook. The fix must retain the classic context
menu in the navigation pane, without a "Show more options" entry.

Related reports: https://github.com/valinet/ExplorerPatcher/issues/2550 and
https://github.com/valinet/ExplorerPatcher/issues/4094.

## Observed mechanism

On Windows 11 build 22621.2506 x64 with ExplorerPatcher 26100.9711.71.1,
a live trace of a user-reproduced USB reset captured:

```
CNscTree::_OnNotify (screen-reader selection-change branch)
  CNscTree::DoDefaultActionWithItem
    CShellBrowser::BrowseItem
      CShellBrowser::BrowseObject(empty absolute PIDL, flags 0x80001)
        CShellBrowser::_NavigateToPidl(empty PIDL)
```

The empty absolute PIDL identifies the Desktop shell namespace. Both browse
and navigation returned S_OK. This was an unsolicited successful navigation,
rather than a crash or a filesystem-path fallback.

The matching Microsoft ExplorerFrame.pdb was used to resolve the functions
(PDB identity 4BDA05B87345C88EBC6981787F5ED730, age 1). Disassembly of
CNscTree::_OnNotify maps TVN_SELCHANGEDA (-402) and TVN_SELCHANGEDW (-451)
to the same screen-reader check. A true result invokes the selected item's
default action immediately, including after selection changes from shell
refreshes. EP's SPI_GETSCREENREADER spoof therefore changes navigation policy.

In the same process, the unmodified user32 query returned FALSE while
ExplorerFrame!AccessibilityHelpers::IsScreenReaderEnabled returned TRUE.
Forwarding ExplorerFrame-originated queries to user32 made the helper return
FALSE. The user then repeated reproduction steps and reported no unwanted
navigation; the trace recorded 122 shell changes and zero empty-PIDL
BrowseObject requests after activation, versus one captured Desktop navigation
before activation. However, the user also reported the navigation-pane menu
had gained "Show more options". Therefore removing the entire import hook is
not an acceptable final fix.

## Revised implementation

Keep the original SystemParametersInfoW hook. Use the existing CreateWindowExW
hook to subclass windows of class NamespaceTreeControl. Only while processing
a WM_NOTIFY for TVN_SELCHANGEDA/W, suspend the fake screen-reader response on
that thread. Forward queries to the real Windows API in that scope. Outside
that scope, including NM_RCLICK and WM_CONTEXTMENU, retain the original menu
behavior and settings.

A thread-local nesting counter supports nested selection notifications and
prevents state leaking to another Explorer UI thread. Remove the subclass on
WM_NCDESTROY. No OS-version-specific offsets, stack scans, blocked device
notifications, forced navigation back, registry changes, or fake FALSE result
for a genuine screen reader are involved in the revised implementation.

## Verification and remaining work

An independent native Win32 test includes the production ExplorerNavigation.h
and creates hidden controls in its own process. It passed with both Zig/Clang in Microsoft-extension mode and the installed
MSVC 14.51 compiler, with warnings treated as errors. Coverage:

- Both ANSI and Unicode selection notifications.
- Nested notification dispatch and scope restoration.
- Independent thread retains the original menu query behavior.
- Ordinary/right-click notifications and context-menu messages retain it too.
- Disabled menu setting and unrelated SPI actions are not spoofed.
- Unrelated window classes are not subclassed; destruction cleans up.

This verifies the scope implementation, not the complete Explorer UI. Live validation for this machine is recorded below; the isolated tests do
not establish compatibility with every Windows build or screen-reader UI. Visual Studio 2026 Build Tools and Windows SDK 26100 are now installed; all
pinned submodules and solution NuGet dependencies were restored. The Release
amd64 ExplorerPatcher target and its weather-host dependencies compiled
successfully with MSBuild 18 / MSVC v145. Existing upstream warnings remain.
The patched DLL SHA-256 is
E5CBE01018AFEBC1A7287215052B96B5725F3CB89C8A75CEA4419828A0583F7E (second trial).

A local test package and backup/rollback script are prepared under test/.
Following the user's request for a reversible patch test, an elevated
PowerShell applied the two-DLL test package and restarted Explorer. Both
installed file hashes match the package, and the new Explorer process loaded
the expected EP modules and responds normally. Both original backups were
verified against their recorded hashes. Backup: test/backups/20261102-023402;
its ROLLBACK.ps1 restores the originals from an elevated PowerShell.
The first scoped trial prevented resets, but the user still saw "Show more
options" in the navigation pane. Offline analysis of this machine's matching
ExplorerFrame.dll showed EnsureContextMenuPresenter requests legacy CLSID
86CA1AA0-34AA-4E8B-A509-50C905BAE2A2 and the original presenter IID
706461D1-AC5F-4730-BFE3-CAC6CAD5EF5E. Upstream commit f646262 only
intercepts presenter IID2 in ExplorerFrame_CoCreateInstanceHook. The final
change accepts both IID versions for both known presenter classes, honoring
the existing DisableWindows11ContextMenu setting and leaving unrelated COM
activation alone.

A second native regression test compiles the actual production COM hook
extracted from dllmain.c. It passed all old/new IID and CLSID combinations,
setting-disabled passthrough, unrelated activation, and null output handling.
The Release amd64 target then rebuilt successfully. The second trial was
loaded through the same reversible deployment, preserving the original vendor
backups and the original ROLLBACK.ps1 command.

The user repeated the real reproduction steps after the second trial and
confirmed: classic navigation-pane menu works and the unwanted navigation no
longer occurs. Both installed hashes match the second-trial package; Explorer
is responding normally. Broader Windows-build and actual screen-reader UI
testing are outside the validation performed here.

The first experiment's Frida agent caused an explorer.exe crash during its
teardown (Application Error, fault module frida-agent.dll). Windows restarted
Explorer. That experiment is not being reused; subsequent native tests run
in a separate process. Treat the old trace scripts as historical diagnostics,
not reusable deployment tools.

Diagnostic scripts, symbols, dependencies, and local logs live in ignored
test/. They are not part of the production change. git diff --check passed.


