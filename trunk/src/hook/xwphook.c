
/*
 *@@sourcefile xwphook.c:
 *      this has the all the code for XWPHOOK.DLL, which implements
 *      all the XWorkplace PM system hooks, as well as a few exported
 *      interface functions which may only be called by the XWorkplace
 *      daemon process (XWPDAEMN.EXE).
 *
 *      There are four hooks at present:
 *      -- hookSendMsgHook
 *      -- hookLockupHook
 *      -- hookInputHook
 *      -- hookPreAccelHook
 *      See the remarks in the respective functions.
 *
 *      Most of the hook features started out with code taken
 *      from ProgramCommander/2 and WarpEnhancer and were then
 *      extended.
 *
 *      The hooks have been designed for greatest possible clarity
 *      and lucidity with respect to interfaces and configuration --
 *      as if that was possible with system hooks at all, since they
 *      are messy by nature.
 *
 *      XWorkplace implementation notes:
 *
 *      --  System hooks are possibly running in the context of each PM
 *          thread which is processing a message -- that is, possibly each
 *          thread on the system which has created a message queue.
 *
 *          As a result, great care must be taken when accessing global
 *          data, and one must keep in mind for each function which thread
 *          it might run in, or otherwise we'd be asking for system hangs.
 *          Don't expect OS/2 to handle exceptions correctly if all PM
 *          programs crash at the same time.
 *
 *          It _is_ possible to access static global variables of
 *          this DLL from every function, because the DLL is linked
 *          with the STATIC SHARED flags (see xwphook.def).
 *          This means that the single data segment of the DLL becomes
 *          part of each process which is using the DLL
 *          (that is, each PM process on the system). We use this
 *          for the HOOKDATA structure, which is a static global structure
 *          and is returned to the daemon when it calls hookInit in the
 *          DLL.
 *
 *          HOOKDATA includes a HOOKCONFIG structure. As a result, to
 *          configure the basic hook flags, the daemon can simply
 *          modify the fields in the HOOKDATA structure to configure
 *          the hook's behavior.
 *
 *          It is however _not_ possible to use malloc() to allocate global
 *          memory and use it between several calls of the hooks, because
 *          that memory will belong to one process only, even if the pointer
 *          is stored in a global DLL variable. The next time the hook gets
 *          called and accesses that memory, some fairly random application
 *          will crash (the one the hook is currently called for), or the
 *          system will hang completely.
 *
 *          For this reason, the hooks use another block of shared memory
 *          internally, which is protected by a mutex semaphore, for storing
 *          the list of object hotkeys (which is variable in size).
 *          This block is (re)allocated in hookSetGlobalHotkeys and
 *          requested in the hook when WM_CHAR comes in. The mutex is
 *          necessary because when hotkeys are changed, the daemon changes
 *          the structure by calling hookSetGlobalHotkeys.
 *
 *      --  The exported hook* functions may only be used by one
 *          single process. It is not possible for one process to
 *          call hookInit and for another to call another hook*
 *          function, because the shared memory which is allocated
 *          here must be "owned" by a certain process and would not be
 *          properly freed if several processes called the hook
 *          interfaces.
 *
 *          So, per definition, it is only the XWorkplace daemon's
 *          (XWPDAEMN.EXE) responsibility to interface and configure
 *          the hook. Theoretically, we could have done this from
 *          XFLDR.DLL in PMSHELL.EXE also, but using the daemon has a
 *          number of advantages to that, since it can be terminated
 *          and restarted independently of the WPS (see xwpdaemn.c
 *          for details). Also, if something goes wrong, it's only
 *          the daemon which crashes, and not the entire WPS.
 *
 *          When any configuration data changes which is of importance
 *          to the hook, XFLDR.DLL writes this data to OS2.INI and
 *          then posts the daemon a message. The daemon then reads
 *          the new data and notifies the hook thru defined interface
 *          functions. All configuration structures are declared in
 *          xwphook.h.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "hook\hook_private.h"
 */

/*
 *      Copyright (C) 1999-2000 Ulrich M�ller.
 *      Copyright (C) 1993-1999 Roman Stangl.
 *      Copyright (C) 1996-1997 Achim Hasenm�ller.
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINSCROLLBARS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINHOOKS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_DOSMODULEMGR

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helpers\undoc.h"

#include "hook\xwphook.h"
#include "hook\hook_private.h"          // private hook and daemon definitions

// PMPRINTF in hooks is a tricky issue;
// avoid this unless this is really needed.
// If enabled, NEVER give PMPRINTF the focus,
// or your system will hang solidly...

#define DONTDEBUGATALL

#include "setup.h"

/******************************************************************
 *                                                                *
 *  Global variables                                              *
 *                                                                *
 ******************************************************************/

/*
 *      We must initialize all these variables to
 *      something, because if we don't, the compiler
 *      won't put them in the DLL's shared data segment
 *      (see introductory notes).
 */

HOOKDATA        G_HookData = {0};
    // this includes HOOKCONFIG; its address is
    // returned from hookInit and stored within the
    // daemon to configure the hook

PGLOBALHOTKEY   G_pGlobalHotkeysMain = NULL;
    // pointer to block of shared memory containing
    // the hotkeys; allocated by hookInit, requested
    // by hooks, changed by hookSetGlobalHotkeys

ULONG           G_cGlobalHotkeys = 0;
    // count of items in that array (_not_ array size!)

HMTX            G_hmtxGlobalHotkeys = NULLHANDLE;
    // mutex for protecting that array

/*
 * Prototypes:
 *
 */

VOID EXPENTRY hookSendMsgHook(HAB hab, PSMHSTRUCT psmh, BOOL fInterTask);
VOID EXPENTRY hookLockupHook(HAB hab, HWND hwndLocalLockupFrame);
BOOL EXPENTRY hookInputHook(HAB hab, PQMSG pqmsg, USHORT fs);
BOOL EXPENTRY hookPreAccelHook(HAB hab, PQMSG pqmsg, ULONG option);

// _CRT_init is the C run-time environment initialization function.
// It will return 0 to indicate success and -1 to indicate failure.
int _CRT_init(void);

// _CRT_term is the C run-time environment termination function.
// It only needs to be called when the C run-time functions are statically
// linked, as is the case with XWorkplace.
void _CRT_term(void);

/******************************************************************
 *                                                                *
 *  Helper functions                                              *
 *                                                                *
 ******************************************************************/

/*
 *@@ _DLL_InitTerm:
 *     this function gets called when the OS/2 DLL loader loads
 *     and frees this DLL. We override this function (which is
 *     normally provided by the runtime library) to intercept
 *     this DLL's module handle.
 *
 *     Since OS/2 calls this function directly, it must have
 *     _System linkage.
 *
 *     Note: You must then link using the /NOE option, because
 *     the VAC++ runtimes also contain a _DLL_Initterm, and the
 *     linker gets in trouble otherwise.
 *     The XWorkplace makefile takes care of this.
 *
 *     This function must return 0 upon errors or 1 otherwise.
 *
 *@@changed V0.9.0 [umoeller]: reworked locale initialization
 */

unsigned long _System _DLL_InitTerm(unsigned long hModule,
                                    unsigned long ulFlag)
{
    switch (ulFlag)
    {
        case 0:
        {
            // store the DLL handle in the global variable
            G_HookData.hmodDLL = hModule;

            // now initialize the C run-time environment before we
            // call any runtime functions
            if (_CRT_init() == -1)
               return (0);  // error

        break; }

        case 1:
            // DLL being freed: cleanup runtime
            _CRT_term();
            break;

        default:
            // other code: beep for error
            DosBeep(100, 100);
            return (0);     // error
    }

    // a non-zero value must be returned to indicate success
    return (1);
}

/*
 *@@ InitializeGlobalsForHooks:
 *      this gets called from hookInit to initialize
 *      the global variables. We query the PM desktop,
 *      the window list, and other stuff we need all
 *      the time.
 */

VOID InitializeGlobalsForHooks(VOID)
{
    HENUM   henum;
    HWND    hwndThis;
    BOOL    fFound;

    // screen dimensions
    G_HookData.lCXScreen = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
    G_HookData.lCYScreen = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

    // PM desktop (the WPS desktop is handled by the daemon)
    G_HookData.hwndPMDesktop = WinQueryDesktopWindow(G_HookData.habDaemonObject,
                                                     NULLHANDLE);

    // enumerate desktop window to find the window list:
    // according to PMTREE, the window list has the following
    // window hierarchy:
    //      FRAME
    //        +--- TITLEBAR
    //        +--- Menu
    //        +--- WindowList
    //        +---
    fFound = FALSE;
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while (     (!fFound)
             && (hwndThis = WinGetNextWindow(henum))
          )
    {
        CHAR    szClass[200];
        if (WinQueryClassName(hwndThis, sizeof(szClass), szClass))
        {
            if (strcmp(szClass, "#1") == 0)
            {
                // frame window: check the children
                HENUM   henumFrame;
                HWND    hwndChild;
                henumFrame = WinBeginEnumWindows(hwndThis);
                while (    (!fFound)
                        && (hwndChild = WinGetNextWindow(henumFrame))
                      )
                {
                    CHAR    szChildClass[200];
                    if (WinQueryClassName(hwndChild, sizeof(szChildClass), szChildClass))
                    {
                        if (strcmp(szChildClass, "WindowList") == 0)
                        {
                            // yup, found:
                            G_HookData.hwndWindowList = hwndThis;
                            fFound = TRUE;
                        }
                    }
                }
                WinEndEnumWindows(henumFrame);
            }
        }
    }
    WinEndEnumWindows(henum);

}

/*
 *@@ GetFrameWindow:
 *      this finds the desktop window (child of
 *      HWND_DESKTOP) to which the specified window
 *      belongs.
 */

HWND GetFrameWindow(HWND hwndTemp)
{
    CHAR    szClass[100];
    HWND    hwndPrevious = NULLHANDLE;
    // climb up the parents tree until we get a frame
    while (    (hwndTemp)
            && (hwndTemp != G_HookData.hwndPMDesktop)
          )
    {
        hwndPrevious = hwndTemp;
        hwndTemp = WinQueryWindow(hwndTemp, QW_PARENT);
    }

    return (hwndPrevious);
}

/*
 *@@ GetScrollBar:
 *      returns the specified scroll bar of hwndOwner.
 *      Returns NULLHANDLE if it doesn't exist or is
 *      disabled.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 *@@changed V0.9.1 (2000-02-13) [umoeller]: fixed disabled scrollbars bug
 */

HWND GetScrollBar(HWND hwndOwner,
                  BOOL fHorizontal) // in: if TRUE, query horizontal;
                                    // if FALSE; query vertical
{
    HWND    hwndReturn = NULLHANDLE;
    HENUM   henum; // enumeration handle for scroll bar seek
    HWND    hwndFound; // handle of found window
    CHAR    szWinClass[3]; // buffer for window class name
    ULONG   ulWinStyle; // style of the found scroll bar

    // begin window enumeration
    henum = WinBeginEnumWindows(hwndOwner);
    while ((hwndFound = WinGetNextWindow(henum)) != NULLHANDLE)
    {
        // query class name of found window
        WinQueryClassName(hwndFound, 3, szWinClass);

        // is it a scroll bar window?
        if (strcmp(szWinClass, "#8") == 0)
        {
            // query style bits of this scroll bar window
            ulWinStyle = WinQueryWindowULong(hwndFound, QWL_STYLE);

            // return window handle if it matches fHorizontal
            if (fHorizontal)
            {
                // query horizonal mode:
                if ((ulWinStyle & SBS_VERT) == 0)
                        // we must check it this way
                        // because SBS_VERT is 1 and SBS_HORZ is 0
                    if (WinIsWindowEnabled(hwndFound))
                    {
                        hwndReturn = hwndFound;
                        break; // while
                    }
            }
            else
                if (ulWinStyle & SBS_VERT)
                    if (WinIsWindowEnabled(hwndFound))
                    {
                        hwndReturn = hwndFound;
                        break; // while
                    }
        } // end if (strcmp(szWinClass, "#8") == 0)
    } // end while ((hwndFound = WinGetNextWindow(henum)) != NULLHANDLE)

    // finish window enumeration
    WinEndEnumWindows(henum);

    return (hwndReturn);
}

/*
 *@@ HiliteMenuItem:
 *      this un-hilites the currently hilited menu
 *      item in hwndMenu and hilites the specified
 *      one instead.
 *
 *      Used during delayed sliding menu processing.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

VOID HiliteMenuItem(HWND hwndMenu,
                    SHORT sItemCount,   // in: item count in hwndMenu
                    SHORT sItemIndex)   // in: item index to hilite
{
    SHORT       sItemIndex2;
    // first, un-hilite the item which currently
    // has hilite state; otherwise, we'd successively
    // hilite _all_ items in the menu... doesn't look
    // too good.
    // We cannot use a global variable for storing
    // the last menu hilite, because there can be more
    // than one open (sub)menu, and we'd get confused
    // otherwise.
    // So go thru all menu items again and un-hilite
    // the first hilited menu item we find. This will
    // be the one either PM has hilited when something
    // was selected, or the one we might have hilited
    // here previously:
    for (sItemIndex2 = 0;
         sItemIndex2 < sItemCount;
         sItemIndex2++)
    {
        SHORT sCurrentItemIdentity2
            = (SHORT)WinSendMsg(hwndMenu,
                                MM_ITEMIDFROMPOSITION,
                                MPFROMSHORT(sItemIndex2),
                                NULL);
        ULONG ulAttr
            = (ULONG)WinSendMsg(hwndMenu,
                                MM_QUERYITEMATTR,
                                MPFROM2SHORT(sCurrentItemIdentity2,
                                             FALSE), // no search submenus
                                MPFROMSHORT(0xFFFF));
        if (ulAttr & MIA_HILITED)
        {
            WinSendMsg(hwndMenu,
                       MM_SETITEMATTR,
                       MPFROM2SHORT(sCurrentItemIdentity2,
                                    FALSE), // no search submenus
                       MPFROM2SHORT(MIA_HILITED,
                                    0));    // unset attribute
            // stop second loop
            // break;
        }
    }

    // now hilite the new item (the one under the mouse)
    WinSendMsg(hwndMenu,
               MM_SETITEMATTR,
               MPFROM2SHORT(sItemIndex,
                            FALSE), // no search submenus
               MPFROM2SHORT(MIA_HILITED, MIA_HILITED));
}

/*
 *@@ ProcessSlidingMenu:
 *      this gets called to implement the "sliding menu" feature.
 *
 *      Based on code from ProgramCommander (C) Roman Stangl.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

VOID SelectMenuItem(HWND hwndMenu,
                    SHORT sItemIndex)
{
    MENUITEM    menuitemCurrent;

    // query the current menuentry's menuitem structure
    if (WinSendMsg(hwndMenu,
                   MM_QUERYITEM,
                   MPFROM2SHORT(sItemIndex,
                                FALSE),
                   MPFROMP(&menuitemCurrent)))
    {
        SHORT   sItemIdentity;
        HWND    hwndSubMenu;

        // select the menuentry just below the mouse pointer. If the
        // item is a submenu, then the first item is selected too
        // (unfortunately!, which displays conditionally cascaded
        // submenus and I found no way to prevent/undo this except
        // for not selecting such submenus at all, which would be somewhat
        // inconsistent for the user)
        WinSendMsg(hwndMenu,
                   MM_SELECTITEM,
                   MPFROM2SHORT(sItemIndex,
                                FALSE), // no search submenus
                   MPFROM2SHORT(0,
                                FALSE));

        // if the menuentry is a submenu, then we may have to select
        // the first menuentry of the submenu too
        while (menuitemCurrent.afStyle & MIS_SUBMENU)
        {
            static ULONG ulQuerySuccess;
            static HWND  hwndLastMenu = NULLHANDLE;

            // get the identity of the first menuentry in the submenu;
            // exit iteration if an error is returned (e.g. empty
            // submenu)
            hwndLastMenu = hwndSubMenu = menuitemCurrent.hwndSubMenu;

            sItemIdentity  = (SHORT)WinSendMsg(hwndSubMenu,
                                               MM_ITEMIDFROMPOSITION,
                                               MPFROMSHORT(0),
                                               NULL);
            if (sItemIdentity == MIT_ERROR)
                break;

            // get the first menuentry of the current submenu, to
            // again select its first menuentry recursively
            ulQuerySuccess = (ULONG) WinSendMsg(hwndSubMenu,
                                                MM_QUERYITEM,
                                                MPFROM2SHORT(sItemIdentity,
                                                             FALSE),
                                                MPFROMP(&menuitemCurrent));
            hwndLastMenu = menuitemCurrent.hwndSubMenu;

            // select the submenu's first menuentry
            WinSendMsg(hwndSubMenu,
                       MM_SELECTITEM,
                       MPFROM2SHORT(sItemIdentity,
                                    FALSE),
                       MPFROM2SHORT(0, FALSE));
            // If the user selected that we should not recursively cascade
            // further into submenus, then simply don't do that. Unfortunately
            // sometimes 2 levels are selected, this is a unexplainable PM
            // behaviour for me
            // if ((!(HookParameters.ulStatusFlag2 & CASCADEMENU)) || (ulQuerySuccess == FALSE))
                break;
        }
    }
}

/******************************************************************
 *                                                                *
 *  Hook interface                                                *
 *                                                                *
 ******************************************************************/

/*
 *@@ hookInit:
 *      registers (sets) all the hooks and initializes data.
 *
 *      In any case, a pointer to the DLL's static HOOKDATA
 *      structure is returned. In this struct, the caller
 *      can examine the two flags for whether the hooks
 *      were successfully installed.
 *
 *      Note: All the exported hook* interface functions must
 *      only be called by the same process, which is the
 *      XWorkplace daemon (XWPDAEMN.EXE).
 *
 *      This gets called by XWPDAEMN.EXE when
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: fixed missing global updates
 *@@changed V0.9.2 (2000-02-21) [umoeller]: added new system hooks
 */

PHOOKDATA EXPENTRY hookInit(HWND hwndDaemonObject) // in: daemon object window (receives notifications)
{
    APIRET arc = NO_ERROR;

    _Pmpf(("Entering hookInit"));

    if (G_hmtxGlobalHotkeys == NULLHANDLE)
        arc = DosCreateMutexSem(NULL,        // unnameed
                                &G_hmtxGlobalHotkeys,
                                DC_SEM_SHARED,
                                FALSE);      // initially unowned

    if (arc == NO_ERROR)
    {
        _Pmpf(("Storing data"));
        G_HookData.hwndDaemonObject = hwndDaemonObject;
        G_HookData.habDaemonObject = WinQueryAnchorBlock(hwndDaemonObject);
        _Pmpf(("  Done storing data"));

        if (G_HookData.hmodDLL)       // initialized by _DLL_InitTerm
        {
            _Pmpf(("hookInit: hwndCaller = 0x%lX", hwndDaemonObject));
            _Pmpf(("  Current HookData.fInputHooked: %d", HookData.fInputHooked));

            // initialize globals needed by the hook
            InitializeGlobalsForHooks();

            // check if shared mem for hotkeys
            // was already acquired
            arc = DosGetNamedSharedMem((PVOID*)(&G_pGlobalHotkeysMain),
                                       IDSHMEM_HOTKEYS,
                                       PAG_READ | PAG_WRITE);
            while (arc == NO_ERROR)
            {
                // exists already:
                arc = DosFreeMem(G_pGlobalHotkeysMain);
                _Pmpf(("DosFreeMem arc: %d", arc));
            };

            // initialize hotkeys
            arc = DosAllocSharedMem((PVOID*)(&G_pGlobalHotkeysMain),
                                    IDSHMEM_HOTKEYS,
                                    sizeof(GLOBALHOTKEY), // rounded up to 4KB
                                    PAG_COMMIT | PAG_READ | PAG_WRITE);
            _Pmpf(("DosAllocSharedMem arc: %d", arc));

            if (arc == NO_ERROR)
            {
                BOOL fSuccess = FALSE;

                // one default hotkey, let's make this better
                G_pGlobalHotkeysMain->usKeyCode = '#';
                G_pGlobalHotkeysMain->usFlags = KC_CTRL | KC_ALT;
                G_pGlobalHotkeysMain->ulHandle = 100;

                G_cGlobalHotkeys = 1;

                // install hooks, but only once...
                if (!G_HookData.fSendMsgHooked)
                    G_HookData.fSendMsgHooked = WinSetHook(G_HookData.habDaemonObject,
                                                         NULLHANDLE, // system hook
                                                         HK_SENDMSG, // send-message hook
                                                         (PFN)hookSendMsgHook,
                                                         G_HookData.hmodDLL);

                if (!G_HookData.fLockupHooked)
                    G_HookData.fLockupHooked = WinSetHook(G_HookData.habDaemonObject,
                                                        NULLHANDLE, // system hook
                                                        HK_LOCKUP,  // lockup hook
                                                        (PFN)hookLockupHook,
                                                        G_HookData.hmodDLL);

                if (!G_HookData.fInputHooked)
                    G_HookData.fInputHooked = WinSetHook(G_HookData.habDaemonObject,
                                                       NULLHANDLE, // system hook
                                                       HK_INPUT,    // input hook
                                                       (PFN)hookInputHook,
                                                       G_HookData.hmodDLL);

                if (!G_HookData.fPreAccelHooked)
                    G_HookData.fPreAccelHooked = WinSetHook(G_HookData.habDaemonObject,
                                                          NULLHANDLE, // system hook
                                                          HK_PREACCEL,  // pre-accelerator table hook (undocumented)
                                                          (PFN)hookPreAccelHook,
                                                          G_HookData.hmodDLL);
            }

            _Pmpf(("  New HookData.fInputHooked: %d", HookData.fInputHooked));
        }

        _Pmpf(("Leaving hookInit"));
    }

    return (&G_HookData);
}

/*
 *@@ hookKill:
 *      deregisters the hook function and frees allocated
 *      resources.
 *
 *      Note: This function must only be called by the same
 *      process which called hookInit (that is, the daemon),
 *      or resources cannot be properly freed.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: fixed missing global updates
 *@@changed V0.9.2 (2000-02-21) [umoeller]: added new system hooks
 */

BOOL EXPENTRY hookKill(void)
{
    BOOL brc = FALSE;

    _Pmpf(("hookKill"));

    if (G_HookData.fInputHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_INPUT,
                       (PFN)hookInputHook,
                       G_HookData.hmodDLL);
        G_HookData.fInputHooked = FALSE;
        brc = TRUE;
    }

    if (G_HookData.fPreAccelHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_PREACCEL,     // pre-accelerator table hook (undocumented)
                       (PFN)hookPreAccelHook,
                       G_HookData.hmodDLL);
        brc = TRUE;
        G_HookData.fPreAccelHooked = FALSE;
    }

    if (G_HookData.fLockupHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_LOCKUP,       // lockup hook
                       (PFN)hookLockupHook,
                       G_HookData.hmodDLL);
        brc = TRUE;
        G_HookData.fLockupHooked = FALSE;
    }

    if (G_HookData.fSendMsgHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_SENDMSG,       // lockup hook
                       (PFN)hookSendMsgHook,
                       G_HookData.hmodDLL);
        brc = TRUE;
        G_HookData.fSendMsgHooked = FALSE;
    }

    if (G_pGlobalHotkeysMain)
        // free shared mem
        DosFreeMem(G_pGlobalHotkeysMain);

    if (G_hmtxGlobalHotkeys)
    {
        DosCloseMutexSem(G_hmtxGlobalHotkeys);
        G_hmtxGlobalHotkeys = NULLHANDLE;
    }

    return (brc);
}

/*
 *@@ hookSetGlobalHotkeys:
 *      this exported function gets called to update the
 *      hook's list of global hotkeys.
 *
 *      This is the only interface to the global hotkeys
 *      list. If we wish to change any of the hotkeys,
 *      we must always pass a complete array of new
 *      hotkeys to this function. This is not terribly
 *      comfortable, but we're dealing with global PM
 *      hooks here, so comfort is not really the main
 *      objective.
 *
 *      XFldObject::xwpSetObjectHotkey can be used for
 *      a more convenient interface. That method will
 *      automatically recompose the complete list when
 *      a single hotkey changes and call this func in turn.
 *
 *      This function copies the given list into a newly
 *      allocated block of shared memory, which is used
 *      by the hook. If a previous such block exists, it
 *      is freed. Access to this block is protected by
 *      a mutex semaphore internally in case WM_CHAR
 *      comes in while the list is being modified
 *      (the hook will not be running on the same thread
 *      as this function, which gets called by the daemon).
 *
 *      Note: This function must only be called by the same
 *      process which called hookInit (that is, the
 *      daemon), because otherwise the shared memory cannot
 *      be properly freed.
 *
 *      This returns the DOS error code of the various
 *      semaphore and shared mem API calls.
 */

APIRET EXPENTRY hookSetGlobalHotkeys(PGLOBALHOTKEY pNewHotkeys, // in: new hotkeys array
                                     ULONG cNewHotkeys)         // in: count of items in array --
                                                              // _not_ array size!!
{
    BOOL    fSemOpened = FALSE,
            fSemOwned = FALSE;
    // request access to the hotkeys mutex
    APIRET arc = DosOpenMutexSem(NULL,       // unnamed
                                 &G_hmtxGlobalHotkeys);

    if (arc == NO_ERROR)
    {
        fSemOpened = TRUE;
        arc = WinRequestMutexSem(G_hmtxGlobalHotkeys,
                                 SEM_TIMEOUT);
    }

    _Pmpf(("hookSetGlobalHotkeys: WinRequestMutexSem arc: %d", arc));

    if (arc == NO_ERROR)
    {
        fSemOwned = TRUE;

        if (G_pGlobalHotkeysMain)
        {
            // hotkeys defined already: free the old ones
            DosFreeMem(G_pGlobalHotkeysMain);
        }

        arc = DosAllocSharedMem((PVOID*)(&G_pGlobalHotkeysMain),
                                IDSHMEM_HOTKEYS,
                                sizeof(GLOBALHOTKEY) * cNewHotkeys, // rounded up to 4KB
                                PAG_COMMIT | PAG_READ | PAG_WRITE);
        _Pmpf(("DosAllocSharedMem arc: %d", arc));

        if (arc == NO_ERROR)
        {
            // copy hotkeys to shared memory
            memcpy(G_pGlobalHotkeysMain,
                   pNewHotkeys,
                   sizeof(GLOBALHOTKEY) * cNewHotkeys);
            G_cGlobalHotkeys = cNewHotkeys;
        }

    }

    if (fSemOwned)
        DosReleaseMutexSem(G_hmtxGlobalHotkeys);
    if (fSemOpened)
        DosCloseMutexSem(G_hmtxGlobalHotkeys);

    return (arc);
}

/******************************************************************
 *                                                                *
 *  System-wide global variables for input hook                   *
 *                                                                *
 ******************************************************************/

HWND    G_hwndUnderMouse = NULLHANDLE;
HWND    G_hwndLastFrameUnderMouse = NULLHANDLE;
POINTS  G_ptsMousePosWin = {0};
POINTL  G_ptlMousePosDesktop = {0};

/******************************************************************
 *                                                                *
 *  Send-Message Hook                                             *
 *                                                                *
 ******************************************************************/

/*
 *@@ ProcessMsgsForPageMage:
 *      message processing which is needed for both
 *      hookInputHook and hookSendMsgHook.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID ProcessMsgsForPageMage(HWND hwnd,
                            ULONG msg,
                            MPARAM mp1,
                            MPARAM mp2)
{
    ULONG       ulRequest;

    // first check, just for speed
    if (    (msg == WM_CREATE)
         || (msg == WM_DESTROY)
         || (msg == WM_ACTIVATE)
         || (msg == WM_WINDOWPOSCHANGED)
       )
    {
        if (WinQueryWindow(hwnd, QW_PARENT) == G_HookData.hwndPMDesktop)
        {
            CHAR    szClass[200];
            if (WinQueryClassName(hwnd, sizeof(szClass), szClass))
            {
                if (    (strcmp(szClass, "#1") == 0)
                     || (strcmp(szClass, "wpFolder window") == 0)
                   )
                {
                    // window creation/destruction:

                    if (    (msg == WM_CREATE)
                         || (msg == WM_DESTROY)
                       )
                    {
                        WinPostMsg(G_HookData.hwndPageMageClient,
                                   PGMG_WNDCHANGE,
                                   MPFROMHWND(hwnd),
                                   MPFROMLONG(msg));
                    }
                    // window move/focus:
                    else if (    (msg == WM_WINDOWPOSCHANGED)
                              || (   (msg == WM_ACTIVATE)    // WM_SETFOCUS) //
                                  && (mp1)             // mp2) // (psmh->
                                 )
                            )
                    {
                        // it's a top-level window:
                        WinPostMsg(G_HookData.hwndPageMageClient,
                                   PGMG_INVALIDATECLIENT,
                                   (MPARAM)FALSE,   // delayed
                                   0);
                    }

                    // someone becomes active, but only call once (via mp1)
                    if (    (!G_HookData.fDisableSwitching)
                         && (msg == WM_ACTIVATE)
                         && (mp1)
                       )
                    {
                        if (hwnd != G_HookData.hwndPageMageFrame)
                        {
                            ulRequest = PGMGQENCODE(PGMGQ_FOCUSCHANGE, 0, 0);
                            WinPostMsg(G_HookData.hwndPageMageClient,
                                       PGMG_PASSTHRU,
                                       MPFROMLONG(ulRequest),
                                       MPVOID);
                        }
                    }
                } // end if (    (strcmp(szClass, ...
            } // end if (WinQueryClassName(hwnd, sizeof(szClass), szClass))
        } // end if (WinQueryWindow(hwnd, QW_PARENT) == HookData.hwndPMDesktop)
    }

    // implement "float on top" for PageMage frame
    if (    (G_HookData.HookConfig.fFloat)
         && (   (msg == WM_SIZE)
             || (msg == WM_MINMAXFRAME)
             || (msg == WM_SETFOCUS)
             || (msg == WM_TRACKFRAME)
            )
       )
    {
        HWND        hwndParent = WinQueryWindow(hwnd, QW_PARENT);
        if (hwndParent == G_HookData.hwndPMDesktop)
            // it's a top-level window:
            WinSetWindowPos(G_HookData.hwndPageMageFrame,
                            HWND_TOP,
                            0, 0, 0, 0,
                            SWP_ZORDER | SWP_SHOW);
    }

    if (    (msg == WM_DESTROY)
         && (hwnd == G_HookData.hwndLockupFrame)
       )
    {
        // current lockup frame being destroyed
        // (system is being unlocked):
        G_HookData.hwndLockupFrame = NULLHANDLE;
        WinPostMsg(G_HookData.hwndPageMageClient,
                   PGMG_LOCKUP,
                   MPFROMLONG(FALSE),
                   MPVOID);
    }
}

/*
 *@@ hookSendMsgHook:
 *      send-message hook.
 *
 *      We must not do any complex processing in here, especially
 *      calling WinSendMsg(). Instead, we post msgs to other places,
 *      because sending messages will recurse into this hook forever.
 *
 *      Be warned that many PM API functions send messages also.
 *      This applies especially to WinSetWindowPos and such.
 *      They are a no-no in here because they would lead to
 *      infinite recursion also.
 *
 *      SMHSTRUCT is defined as follows:
 *
 *          typedef struct _SMHSTRUCT {
 *            MPARAM     mp2;
 *            MPARAM     mp1;
 *            ULONG      msg;
 *            HWND       hwnd;
 *            ULONG      model;  //  Message identity ?!?
 *          } SMHSTRUCT;
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID EXPENTRY hookSendMsgHook(HAB hab,
                              PSMHSTRUCT psmh,
                              BOOL fInterTask)
{
    if (    // PageMage running?
            (G_HookData.hwndPageMageFrame)
            // switching not disabled?
         && (!G_HookData.fDisableSwitching)
                // this flag is set frequently when PageMage
                // is doing tricky stuff; we must not process
                // messages then, or we'll recurse forever
       )
    {
        // OK, go ahead:

        ProcessMsgsForPageMage(psmh->hwnd,
                               psmh->msg,
                               psmh->mp1,
                               psmh->mp2);
    }
}

/*
 *@@ hookLockupHook:
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID EXPENTRY hookLockupHook(HAB hab,
                             HWND hwndLocalLockupFrame)
{
    G_HookData.hwndLockupFrame = hwndLocalLockupFrame;
    WinPostMsg(G_HookData.hwndPageMageClient,
               PGMG_LOCKUP,
               MPFROMLONG(TRUE),
               MPVOID);
}

/******************************************************************
 *                                                                *
 *  Input hook -- WM_MOUSEMOVE processing                         *
 *                                                                *
 ******************************************************************/

/*
 *@@ WMMouseMove_MB3ScrollLineWise:
 *      this gets called from WMMouseMove_MB3OneScrollbar
 *      if "amplified" mb3-scroll is on. This is what
 *      WarpEnhancer did with MB3 scrolls.
 *
 *      This can get called twice for every WM_MOUSEMOVE,
 *      once for the vertical, once for the horizontal
 *      scroll bar of a window.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 */

VOID WMMouseMove_MB3ScrollLineWise(PSCROLLDATA pScrollData,   // in: scroll data structure in HOOKDATA,
                                                              // either for vertical or horizontal scroll bar
                                   LONG lDelta,               // in: X or Y delta that mouse has moved since MB3 was
                                                              // _initially_ depressed
                                   BOOL fHorizontal)          // in: TRUE: process horizontal, otherwise vertical
{
    USHORT  usScrollCode;
    ULONG   ulMsg;

    if (!fHorizontal)
    {
        if (lDelta > 0)
            usScrollCode = SB_LINEDOWN;
        else
            usScrollCode = SB_LINEUP;

        ulMsg = WM_VSCROLL;
    }
    else
    {
        if (lDelta > 0)
            usScrollCode = SB_LINERIGHT;
        else
            usScrollCode = SB_LINELEFT;

        ulMsg = WM_HSCROLL;
    }

    // post up or down scroll message
    WinPostMsg(pScrollData->hwndScrollOwner,
               ulMsg,
               (MPARAM)pScrollData->usScrollBarID,
               MPFROM2SHORT(1, usScrollCode));
    // post end scroll message
    WinPostMsg(pScrollData->hwndScrollOwner,
               ulMsg,
               (MPARAM)pScrollData->usScrollBarID,
               MPFROM2SHORT(1, SB_ENDSCROLL));
}

/*
 *@@ WMMouseMove_MB3FirstAmplified:
 *      this initializes data needed for "amplified"
 *      mode. This gets called for every first
 *      WM_MOUSEMOVE which comes in after MB3 has
 *      been depressed.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

VOID WMMouseMove_MB3FirstAmplified(PSCROLLDATA pScrollData,  // in: scroll data structure in HOOKDATA,
                                                             // either for vertical or horizontal scroll bar
                                   BOOL fHorizontal)         // in: TRUE: process horizontal, otherwise vertical
{
    // for "amplified" mode, we also need the size
    // of the scroll bar and the thumb size to be
    // able to correlate the mouse movement to the
    // scroller position
    SWP     swpScrollBar;
    WNDPARAMS wp;

    // get size of scroll bar
    WinQueryWindowPos(pScrollData->hwndScrollBar, &swpScrollBar);
    if (fHorizontal)
    {
        pScrollData->lScrollBarSize = swpScrollBar.cx
                                      - 2 * WinQuerySysValue(HWND_DESKTOP,
                                                             SV_CXHSCROLLARROW);
    }
    else
    {
        pScrollData->lScrollBarSize = swpScrollBar.cy
                                      - 2 * WinQuerySysValue(HWND_DESKTOP,
                                                             SV_CYVSCROLLARROW);
    }

    // To make the MB3 scrolling more similar to regular scroll
    // bar dragging, we must take the size of the scroll bar
    // thumb into account by subtracting that size from the scroll
    // bar size; otherwise the user would have much more
    // mouse mileage with MB3 compared to the regular
    // scroll bar.

    // Unfortunately, there's no "SBM_QUERYTHUMBSIZE" msg,
    // so we need to be a bit more tricky and extract this
    // from the scroll bar's control data.

    memset(&wp, 0, sizeof(wp));

    // get size of scroll bar control data
    wp.fsStatus = WPM_CBCTLDATA;
    if (WinSendMsg(pScrollData->hwndScrollBar,
                   WM_QUERYWINDOWPARAMS,
                   (MPARAM)&wp,
                   0))
    {
        // success:
        _Pmpf(("    wp.cbCtlData: %d, sizeof SBCDATA: %d",
                    wp.cbCtlData, sizeof(SBCDATA)));
        if (wp.cbCtlData == sizeof(SBCDATA))
        {
            // allocate memory
            SBCDATA sbcd;
            wp.pCtlData = &sbcd;
            // now get control data, finally
            wp.fsStatus = WPM_CTLDATA;
            if (WinSendMsg(pScrollData->hwndScrollBar,
                           WM_QUERYWINDOWPARAMS,
                           (MPARAM)&wp,
                           0))
            {
                // success:
                // cVisible specifies the thumb size in
                // units of cTotal; we now correlate
                // the window dimensions to the dark part
                // of the scroll bar (which is cTotal - cVisible)

                if (    (sbcd.cTotal) // avoid division by zero
                     && (sbcd.cTotal > sbcd.cVisible)
                   )
                    pScrollData->lScrollBarSize = pScrollData->lScrollBarSize
                                * (sbcd.cTotal - sbcd.cVisible)
                                / sbcd.cTotal;
            }
        }
    }
}

/*
 *@@ WMMouseMove_MB3ScrollAmplified:
 *      this gets called from WMMouseMove_MB3OneScrollbar
 *      if "amplified" mb3-scroll is on.
 *      This is new compared to WarpEnhancer.
 *
 *      This can get called twice for every WM_MOUSEMOVE,
 *      once for the vertical, once for the horizontal
 *      scroll bar of a window.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 *@@changed V0.9.2 (2000-02-26) [umoeller]: changed ugly pointer arithmetic
 */

VOID WMMouseMove_MB3ScrollAmplified(PSCROLLDATA pScrollData,  // in: scroll data structure in HOOKDATA,
                                                              // either for vertical or horizontal scroll bar
                                    LONG lDelta,              // in: X or Y delta that mouse has moved since MB3 was
                                                              // _initially_ depressed
                                    BOOL fHorizontal)         // in: TRUE: process horizontal, otherwise vertical
{
    // SWP     swpScrollBar;

    // get scroll bar range
    MRESULT mrThumbRange = WinSendMsg(pScrollData->hwndScrollBar,
                                      SBM_QUERYRANGE, 0, 0);
    SHORT   sThumbLoLimit = SHORT1FROMMR(mrThumbRange),
            sThumbHiLimit = SHORT2FROMMR(mrThumbRange);

    // here come a number of pointers to data we'll
    // need, depending on we're processing the vertical
    // or horizontal scroll bar
    // PLONG   plWinDim;
    ULONG   ulMsg;

    if (!fHorizontal)
    {
        // vertical mode:
        // plWinDim = &swpScrollBar.cy;
        ulMsg = WM_VSCROLL;
    }
    else
    {
        // horizontal mode:
        // plWinDim = &swpScrollBar.cx;
        ulMsg = WM_HSCROLL;
    }

    // We need to calculate a new _absolute_ scroller
    // position based on the _relative_ delta we got
    // as a parameter. This is a bit tricky:

    // 1) Calculate per-mille that mouse has moved
    //    since MB3 was initially depressed, relative
    //    to the window (scroll bar owner) height.
    //    This will be in the range of -1000 to +1000.

    // avoid division by zero
    if (pScrollData->lScrollBarSize)
    {
        SHORT sPerMilleMoved;

        sPerMilleMoved = (lDelta * 1000)
                         / pScrollData->lScrollBarSize;
                // this correlates the y movement to the
                // remaining window height;
                // this is now in the range of -1000 thru +1000

        _Pmpf(("  sPerMilleMoved: %d", sPerMilleMoved));

        // 2) amplification desired? (0 is default for 100%)
        if (G_HookData.HookConfig.sAmplification)
        {
            // yes:
            USHORT usAmpPercent = 100 + (G_HookData.HookConfig.sAmplification * 10);
                // so we get:
                //      0       -->  100%
                //      2       -->  120%
                //     10       -->  200%
                //     -2       -->  80%
                //     -9       -->  10%
            sPerMilleMoved = sPerMilleMoved * usAmpPercent / 100;
        }

        if (sPerMilleMoved)
        {
            // 3) Correlate this to scroll bar units;
            //    this is still a delta, but now in scroll
            //    bar units.
            SHORT sSliderRange = (sThumbHiLimit - sThumbLoLimit);
            LONG lSliderMoved =
                    (LONG)sSliderRange
                    * sPerMilleMoved
                    / 1000;

            SHORT   sNewThumbPos = 0;

            _Pmpf(("  lSliderRange: %d", sSliderRange));
            _Pmpf(("  lSliderOfs: %d", lSliderMoved));

            // 4) Calculate new absolute scroll bar position,
            //    from on what we stored when MB3 was initially
            //    depressed.
            sNewThumbPos = pScrollData->sMB3InitialThumbPos + lSliderMoved;

            _Pmpf(("  New sThumbPos: %d", sNewThumbPos));

            // check against scroll bar limits:
            if (sNewThumbPos < sThumbLoLimit)
                sNewThumbPos = sThumbLoLimit;
            if (sNewThumbPos > sThumbHiLimit)
                sNewThumbPos = sThumbHiLimit;

            // thumb position changed?
            if (sNewThumbPos != pScrollData->sCurrentThumbPos)    // as calculated last time
            {
                // yes:
                // now simulate the message flow that
                // the scroll bar normally produces
                // _while_ the scroll bar thumb is being
                // dragged:
                // a) adjust thumb position in the scroll bar
                WinSendMsg(pScrollData->hwndScrollBar,
                           SBM_SETPOS,
                           (MPARAM)sNewThumbPos,
                           0);
                // b) notify scroll bar owner of the change
                // (normally posted by the scroll bar);
                // this will scroll the window contents
                // depending on the owner's implementation
                WinPostMsg(pScrollData->hwndScrollOwner,
                           ulMsg,                   // WM_xSCROLL
                           (MPARAM)pScrollData->usScrollBarID,
                           MPFROM2SHORT(sNewThumbPos, SB_SLIDERTRACK));

                // set flag to provoke a SB_ENDSCROLL
                // in WM_BUTTON3UP later (hookInputHook)
                pScrollData->fPostSBEndScroll = TRUE;

                // store this thumb position for next time
                pScrollData->sCurrentThumbPos = sNewThumbPos;

                // hoo-yah!!!
            }
        }
    }
}

/*
 *@@ WMMouseMove_MB3OneScrollbar:
 *      this gets called twice from WMMouseMove_MB3Scroll
 *      only when MB3 is currently depressed to do the
 *      scroll bar and scroll bar owner processing and
 *      messaging every time WM_MOUSEMOVE comes in.
 *
 *      Since we pretty much do the same thing twice,
 *      once for the vertical, once for the horizontal
 *      scroll bar, we can put this into a common function
 *      to reduce code size and cache load.
 *
 *      Depending on the configuration and mouse movement,
 *      this calls either WMMouseMove_MB3ScrollLineWise or
 *      WMMouseMove_MB3ScrollAmplified.
 *
 *      Returns TRUE if we were successful and the msg is
 *      to be swallowed.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

BOOL WMMouseMove_MB3OneScrollbar(HWND hwnd,                  // in: window with WM_MOUSEMOVE
                                 PSCROLLDATA pScrollData,
                                 SHORT sCurrentMousePos,     // in: current mouse X or Y (in win coordinates)
                                 BOOL fHorizontal)           // in: TRUE: process horizontal, otherwise vertical
{
    BOOL    brc = FALSE;

    // define initial coordinates if not set yet;
    // this is only -1 if this is the first WM_MOUSEMOVE
    // after MB3 was just depressed
    if (pScrollData->sMB3InitialMousePos == -1)
    {
        /*
         * first call after MB3 was depressed
         *
         */

        // 1) query window handle of vertical scroll bar
        pScrollData->hwndScrollBar = GetScrollBar(hwnd,
                                                  fHorizontal);
        if (pScrollData->hwndScrollBar == NULLHANDLE)
            // if not found, then try if parent has a scroll bar
            pScrollData->hwndScrollBar = GetScrollBar(WinQueryWindow(hwnd,
                                                                     QW_PARENT),
                                                      fHorizontal);

        if (pScrollData->hwndScrollBar)
        {
            if (G_HookData.HookConfig.usScrollMode == SM_AMPLIFIED)
                // initialize data for "amplified" mode;
                // we need the scroller size etc.
                WMMouseMove_MB3FirstAmplified(pScrollData,
                                              fHorizontal);

            // save window ID of scroll bar control
            pScrollData->usScrollBarID = WinQueryWindowUShort(pScrollData->hwndScrollBar,
                                                              QWS_ID);
            // save owner
            pScrollData->hwndScrollOwner = WinQueryWindow(pScrollData->hwndScrollBar,
                                                          QW_OWNER);

            // save initial mouse position
            pScrollData->sMB3InitialMousePos = sCurrentMousePos;
            // save initial scroller position
            pScrollData->sMB3InitialThumbPos
                = (SHORT)WinSendMsg(pScrollData->hwndScrollBar,
                                    SBM_QUERYPOS, 0, 0);
            // cache that
            pScrollData->sCurrentThumbPos = pScrollData->sMB3InitialThumbPos;

            brc = TRUE;         // swallow msg
        }
    } // if (pScrollData->sMB3InitialMousePos == -1)
    else
    {
        /*
         * subsequent calls
         *
         */

        if (pScrollData->hwndScrollOwner)
        {
            // 1) process vertical (Y) scroll bar
            if (pScrollData->hwndScrollBar)
            {
                // calculate difference between initial
                // mouse pos (when MB3 was depressed) and
                // current mouse pos to get the _absolute_
                // delta since MB3 was depressed
                LONG    lDelta = pScrollData->sMB3InitialMousePos - sCurrentMousePos;

                // now check if we need to change the sign of
                // the delta;
                // for a vertical scroll bar,
                // a value of "0" means topmost position
                // (as opposed to screen coordinates...),
                // while for a horizontal scroll bar,
                // "0" means leftmost position (as with
                // screen coordinates...);
                // but when "reverse scrolling" is enabled,
                // we must do this just the other way round
                if (   (    (fHorizontal)
                         && (!G_HookData.HookConfig.fMB3ScrollReverse)
                       )
                        // horizontal scroll bar and _not_ reverse mode:
                    ||
                       (    (!fHorizontal)
                         && (G_HookData.HookConfig.fMB3ScrollReverse)
                       )
                        // vertical scroll bar _and_ reverse mode:
                   )
                    // change sign for all subsequent processing
                    lDelta = -lDelta;

                if (G_HookData.HookConfig.usScrollMode == SM_AMPLIFIED)
                {
                    // amplified mode:
                    if (    (pScrollData->fPostSBEndScroll)
                                // not first call
                         || (abs(lDelta) >= (G_HookData.HookConfig.usMB3ScrollMin + 1))
                                // or movement is large enough:
                       )
                        WMMouseMove_MB3ScrollAmplified(pScrollData,
                                             lDelta,
                                             fHorizontal);
                }
                else
                    // line-wise mode:
                    if (abs(lDelta) >= (G_HookData.HookConfig.usMB3ScrollMin + 1))
                    {
                        // movement is large enough:
                        WMMouseMove_MB3ScrollLineWise(pScrollData,
                                            lDelta,
                                            fHorizontal);
                        pScrollData->sMB3InitialMousePos = sCurrentMousePos;
                    }
            }
        }
    }

    return (brc);
}

/*
 *@@ WMMouseMove_MB3Scroll:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE only if MB3 is currently down to do
 *      the "MB3 scrolling" processing.
 *
 *      This calls WMMouseMove_MB3OneScrollbar twice,
 *      once for the vertical, once for the horizontal
 *      scroll bar.
 *
 *      Returns TRUE if the msg is to be swallowed.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 *@@changed V0.9.2 (2000-02-25) [umoeller]: HScroll not working when VScroll disabled; fixed
 *@@changed V0.9.2 (2000-02-25) [umoeller]: extracted WMMouseMove_MB3OneScrollbar for speed
 */

BOOL WMMouseMove_MB3Scroll(HWND hwnd,       // in: window with WM_MOUSEMOVE
                           MPARAM mp1)      // in: from hookInputHook
{
    BOOL    brc = FALSE;        // no swallow msg
    SHORT   sCurrentMouseX = SHORT1FROMMP(mp1),
            sCurrentMouseY = SHORT2FROMMP(mp1);

    // process vertical scroll bar
    if (WMMouseMove_MB3OneScrollbar(hwnd,
                                    &G_HookData.SDYVert,
                                    sCurrentMouseY,
                                    FALSE))      // vertical
        // msg to be swallowed:
        brc = TRUE;

    // process vertical scroll bar
    if (WMMouseMove_MB3OneScrollbar(hwnd,
                                    &G_HookData.SDXHorz,
                                    sCurrentMouseX,
                                    TRUE))       // horizontal
        // msg to be swallowed:
        brc = TRUE;

    return (brc);
}

/*
 *@@ WMMouseMove_SlidingFocus:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE to do the "sliding focus" processing.
 *
 *      This function evaluates hwnd to find out whether
 *      the mouse has moved over a new frame window.
 *      If so, XDM_SLIDINGFOCUS is posted to the daemon's
 *      object window, which will then do the actual focus
 *      and active window processing (starting a timer, if
 *      delayed focus is active).
 */

VOID WMMouseMove_SlidingFocus(HWND hwnd,        // in: wnd under mouse, from hookInputHook
                              BOOL fMouseMoved, // in: TRUE if mouse has been moved since previous WM_MOUSEMOVE
                                                // (determined by hookInputHook)
                              PSZ pszClassUnderMouse) // in: window class of hwnd (determined
                                                      // by hookInputHook)
{
    // get currently active window; this can only
    // be a frame window (WC_FRAME)
    HWND    hwndActiveNow = WinQueryActiveWindow(HWND_DESKTOP);

    // check 1: check if the active window is still the
    //          the one which was activated by ourselves
    //          previously (either by the hook during WM_BUTTON1DOWN
    //          or by the daemon in sliding focus processing):
    if (hwndActiveNow)
    {
        if (hwndActiveNow != G_HookData.hwndActivatedByUs)
        {
            // active window is not the one we set active:
            // this probably means that some new
            // window popped up which we haven't noticed
            // and was explicitly made active either by the
            // shell or by an application, so we use this
            // for the below checks. Otherwise, sliding focus
            // would be disabled after a new window has popped
            // up until the mouse was moved over a new frame window.
            G_hwndLastFrameUnderMouse = hwndActiveNow;
            G_HookData.hwndActivatedByUs = hwndActiveNow;
        }
    }

    if (hwnd != G_hwndUnderMouse)
        // mouse has moved to a different window:
        G_hwndUnderMouse = hwnd;

    if (   (fMouseMoved)            // has mouse moved?
        && (G_HookData.HookConfig.fSlidingFocus)  // sliding focus enabled?
       )
    {
        // OK:
        HWND    hwndFrame;
        BOOL    fIsSeamless = FALSE;
        // CHAR    szClassUnderMouse[MAXNAMEL+4] = "";
        HWND    hwndFocusNow = WinQueryFocus(HWND_DESKTOP);

        // check 2: make sure mouse is not captured
        if (WinQueryCapture(HWND_DESKTOP) != NULLHANDLE)
            return;

        // check 3: exclude certain window types from
        //          sliding focus, because these don't
        //          work right; we do this by checking
        //          the window classes

        // skip menus with focus
        if (hwndFocusNow)
        {
            CHAR    szFocusClass[MAXNAMEL+4] = "";
            WinQueryClassName(hwndFocusNow,
                              sizeof(szFocusClass),
                              szFocusClass);

            if (strcmp(szFocusClass, "#4") == 0)
                return;
        }

        // skip dropped-down combo boxes under mouse
        if (strcmp(pszClassUnderMouse, "#7") == 0)
            // listbox: check if the parent is the PM desktop
            if (WinQueryWindow(hwnd, QW_PARENT) == G_HookData.hwndPMDesktop)
                // yes: then it's the open list box window of
                // a dropped-down combo box --> ignore
                return;

        // skip seamless Win-OS/2 frames
        // (we must check the actual window under the
        // mouse, because seamless windows have a
        // regular WC_FRAME desktop window, which
        // is apparently invisible though)
        if (strcmp(pszClassUnderMouse, "SeamlessClass") == 0)
            // Win-OS/2 window:
            fIsSeamless = TRUE;

        // OK, enough checks. Now let's do the
        // sliding focus:

        // get the frame window parent of the window
        // under the mouse
        hwndFrame = GetFrameWindow(hwnd);

        if (hwndFrame)
            if (hwndFrame != G_hwndLastFrameUnderMouse)
            {
                // OK, mouse moved to a new desktop window:
                // store that for next time
                G_hwndLastFrameUnderMouse = hwndFrame;

                // notify daemon of the change;
                // it is the daemon which does the rest
                // (timer handling, window activation etc.)
                WinPostMsg(G_HookData.hwndDaemonObject,
                           XDM_SLIDINGFOCUS,
                           (MPARAM)hwndFrame,
                           (MPARAM)fIsSeamless);
            }
    }
}

/*
 *@@ WMMouseMove_SlidingMenus:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE and the window under the mouse is
 *      a menu control to do automatic menu selection.
 *
 *      This implementation is slightly tricky. ;-)
 *      We cannot query the MENUITEM data of a menu
 *      which does not belong to the calling process
 *      because this message needs access to the caller's
 *      storage. I have tried this, this causes every
 *      application to crash as soon as a menu item
 *      gets selected.
 *
 *      Fortunately, Roman Stangl has found a cool way
 *      of doing this, so I could build on that.
 *
 *      So now we start a timer in the daemon which will
 *      post a special WM_MOUSEMOVE message back to us
 *      which can only be received by us
 *      (with the same HWND and MP1 as the original
 *      WM_MOUSEMOVE, but a special flag in MP2). When
 *      that special message comes in, we can select
 *      the menu item.
 *
 *      In addition to Roman's code, we always change the
 *      MIA_HILITE attribute of the current menu item
 *      under the mouse, so the menu hilite follows the
 *      mouse immediately, even though submenus may be
 *      opened delayed.
 *
 *      We return TRUE if the msg is to be swallowed.
 *
 *      Based on code from ProgramCommander (C) Roman Stangl.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

BOOL WMMouseMove_SlidingMenus(HWND hwndCurrentMenu,  // in: menu wnd under mouse, from hookInputHook
                              MPARAM mp1,
                              MPARAM mp2)
{
    BOOL        brc = FALSE;        // per default, don't swallow msg

    // check for special message which was posted from
    // the daemon when the "delayed sliding menu" timer
    // elapsed:
    if (    (mp1 == G_HookData.mpDelayedSlidingMenuMp1)
         && (SHORT1FROMMP(mp2) == HT_DELAYEDSLIDINGMENU)
         && (hwndCurrentMenu == G_HookData.hwndMenuUnderMouse)
       )
    {
        // yes, special message:
        // check if the menu still exists
        // if (WinIsWindow(hwndCurrentMenu))
            // and if it's visible; the menu
            // might have been hidden by now because
            // the user has already proceeded to
            // another submenu
            if (WinIsWindowVisible(hwndCurrentMenu))
                // OK:
                // select menu item under the mouse
                SelectMenuItem(hwndCurrentMenu,
                               G_HookData.sMenuItemUnderMouse);
                                    // stored from last run
                                    // when timer was started...
        // this is our message only,
        // so swallow this
        brc = TRUE;
    }
    else
    {
        // no, regular WM_MOUSEMOVE:
        static SHORT sLastItemIdentity = MIT_NONE;
        SHORT   sCurrentlySelectedItem
            = (SHORT)WinSendMsg(hwndCurrentMenu,
                                MM_QUERYSELITEMID,
                                MPFROMLONG(FALSE),
                                NULL);

        if (sCurrentlySelectedItem != MIT_NONE)
        {
            // loop through all items in the current menu
            // and query the item's rectangle
            SHORT   sItemCount = (SHORT)WinSendMsg(hwndCurrentMenu,
                                                   MM_QUERYITEMCOUNT,
                                                   0, 0),
                    sItemIndex = 0;

            // go thru all menu items
            for (sItemIndex = 0;
                 sItemIndex < sItemCount;
                 sItemIndex++)
            {
                RECTL       rectlItem;

                SHORT sCurrentItemIdentity = (SHORT)WinSendMsg(hwndCurrentMenu,
                                                               MM_ITEMIDFROMPOSITION,
                                                               MPFROMSHORT(sItemIndex),
                                                               NULL);
                // as MIT_ERROR == MIT_END == MIT_NONE == MIT_MEMERROR
                // and may also be used for separators, just ignore this menuentries
                if (sCurrentItemIdentity == MIT_ERROR)
                    continue;

                // get the menuentry's rectangle to test if it covers the
                // current mouse pointer position
                WinSendMsg(hwndCurrentMenu,
                           MM_QUERYITEMRECT,
                           MPFROM2SHORT(sCurrentItemIdentity,
                                        FALSE),
                           MPFROMP(&rectlItem));

                // check if this item's rectangle covers the pointer position
                if (    (G_ptsMousePosWin.x > rectlItem.xLeft)
                     && (G_ptsMousePosWin.x <= rectlItem.xRight)
                     && (G_ptsMousePosWin.y > rectlItem.yBottom)
                     && (G_ptsMousePosWin.y <= rectlItem.yTop)
                   )
                {
                    // has item changed since last time?
                    if (sLastItemIdentity != sCurrentItemIdentity)
                    {
                        // yes:

                        // do we have a submenu delay?
                        if (G_HookData.HookConfig.ulSubmenuDelay)
                        {
                            // delayed:
                            // this is a three-step process:
                            // 1)  If we used MM_SELECTITEM on the item, this
                            //     would immediately open the subwindow (if the
                            //     item represents a submenu).
                            //     So instead, we first need to manually change
                            //     the hilite attribute of the menu item under
                            //     the mouse so that the item under the mouse is
                            //     always immediately hilited (without being
                            //     "selected"; PM doesn't know what we're doing here!)
                            if (G_HookData.HookConfig.fMenuImmediateHilite)
                                HiliteMenuItem(hwndCurrentMenu,
                                               sItemCount,
                                               sCurrentItemIdentity);

                            // 2)  We then post the daemon a message to start
                            //     a timer. Before that, we store the menu item
                            //     data in HOOKDATA so we can re-use it when
                            //     the timer elapses.

                            // prepare data for delayed selection:
                            // when the special WM_MOUSEMOVE comes in,
                            // we check against all these.
                            // a) store mp1 for comparison later
                            G_HookData.mpDelayedSlidingMenuMp1 = mp1;
                            // b) store menu
                            G_HookData.hwndMenuUnderMouse = hwndCurrentMenu;
                            // c) store menu item
                            G_HookData.sMenuItemUnderMouse = sCurrentItemIdentity;
                            // d) notify daemon of the change, which
                            // will start the timer and post WM_MOUSEMOVE
                            // back to us
                            WinPostMsg(G_HookData.hwndDaemonObject,
                                       XDM_SLIDINGMENU,
                                       mp1,
                                       0);

                            // 3)  When the timer elapses, the daemon posts a special
                            //     WM_MOUSEMOVE to the same menu control for which
                            //     the timer was started. See the "special message"
                            //     processing on top. We then immediately select
                            //     the menu item.

                        } // end if (HookData.HookConfig.ulSubmenuDelay)
                        else
                            // no delay, but immediately:
                            SelectMenuItem(hwndCurrentMenu,
                                           sCurrentItemIdentity);
                        // save our the menuentry that is selected within the current
                        // menu now, and break out of the loop (because we've found what
                        // we have been looking for)
                        sLastItemIdentity = sCurrentItemIdentity;
                        break;
                    }
                    else
                        // same as last time: do nothing
                        break;
                }
            } // end for (sItemIndex = 0; ...
        } // end WinSendMsg(hwndCurrentMenu, MM_QUERYSELITEMID, ...
        else
            // reset static storage for the next time we will be using it
            sLastItemIdentity = MIT_NONE;
    }

    return (brc);
}

/*
 *@@ WMMouseMove_AutoHideMouse:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE to automatically hide the mouse
 *      pointer.
 *
 *      We start a timer here which posts WM_TIMER
 *      to fnwpDaemonObject in the daemon. So it's the
 *      daemon which actually hides the mouse.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 */

VOID WMMouseMove_AutoHideMouse(VOID)
{
    // is the timer currently running?
    if (G_HookData.idAutoHideTimer != NULLHANDLE)
    {
        // stop the running async timer
        WinStopTimer(G_HookData.habDaemonObject,
                     G_HookData.hwndDaemonObject,
                     G_HookData.idAutoHideTimer);
        G_HookData.idAutoHideTimer = NULLHANDLE;
    }

    // show the mouse pointer now
    if (G_HookData.fMousePointerHidden)
    {
        WinShowPointer(HWND_DESKTOP, TRUE);
        G_HookData.fMousePointerHidden = FALSE;
    }

    // (re)start timer
    G_HookData.idAutoHideTimer =
        WinStartTimer(G_HookData.habDaemonObject,
                      G_HookData.hwndDaemonObject,
                      TIMERID_AUTOHIDEMOUSE,
                      (G_HookData.HookConfig.ulAutoHideDelay + 1) * 1000);
}

/******************************************************************
 *                                                                *
 *  Input hook -- miscellaneous processing                        *
 *                                                                *
 ******************************************************************/

/*
 *@@ WMButton_SystemMenuContext:
 *      this gets called from hookInputHook upon
 *      MB2 clicks to copy a window's system menu
 *      and display it as a popup menu on the
 *      title bar.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
 */

VOID WMButton_SystemMenuContext(HWND hwnd)     // of WM_BUTTON2CLICK
{
    POINTL      ptlMouse; // mouse coordinates
    HWND        hwndFrame; // handle of the frame window (parent)

    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // query parent of title bar window (frame window)
    hwndFrame = WinQueryWindow(hwnd, QW_PARENT);
    if (hwndFrame)
    {
        // query handle of system menu icon (action bar style)
        HWND hwndSysMenuIcon = WinWindowFromID(hwndFrame, FID_SYSMENU);
        if (hwndSysMenuIcon)
        {
            HWND        hNewMenu; // handle of our copied menu
            HWND        SysMenuHandle;

            // query item id of action bar system menu
            SHORT id = (SHORT)(USHORT)(ULONG)WinSendMsg(hwndSysMenuIcon,
                                                        MM_ITEMIDFROMPOSITION,
                                                        MPFROMSHORT(0), 0);
            // query item id of action bar system menu
            MENUITEM    mi = {0};
            CHAR        szItemText[100]; // buffer for menu text
            WinSendMsg(hwndSysMenuIcon, MM_QUERYITEM,
                       MPFROM2SHORT(id, FALSE),
                       MPFROMP(&mi));
            // submenu is our system menu
            SysMenuHandle = mi.hwndSubMenu;

            // create a new empty menu
            hNewMenu = WinCreateMenu(HWND_OBJECT, NULL);
            if (hNewMenu)
            {
                // query how menu entries the original system menu has
                SHORT SysMenuItems = (SHORT)WinSendMsg(SysMenuHandle,
                                                       MM_QUERYITEMCOUNT,
                                                       0, 0);
                ULONG i;
                // loop through all entries in the original system menu
                for (i = 0; i < SysMenuItems; i++)
                {
                    id = (SHORT)(USHORT)(ULONG)WinSendMsg(SysMenuHandle,
                                                          MM_ITEMIDFROMPOSITION,
                                                          MPFROMSHORT(i),
                                                          0);
                    // get this menu item into mi buffer
                    WinSendMsg(SysMenuHandle,
                               MM_QUERYITEM,
                               MPFROM2SHORT(id, FALSE),
                               MPFROMP(&mi));
                    // query text of this menu entry into our buffer
                    WinSendMsg(SysMenuHandle,
                               MM_QUERYITEMTEXT,
                               MPFROM2SHORT(id, sizeof(szItemText)-1),
                               MPFROMP(szItemText));
                    // add this entry to our new menu
                    WinSendMsg(hNewMenu,
                               MM_INSERTITEM,
                               MPFROMP(&mi),
                               MPFROMP(szItemText));
                }

                // display popup menu
                WinPopupMenu(HWND_DESKTOP, hwndFrame, hNewMenu,
                             ptlMouse.x, ptlMouse.y, 0x8007, PU_HCONSTRAIN |
                             PU_VCONSTRAIN | PU_MOUSEBUTTON1 |
                             PU_MOUSEBUTTON2 | PU_KEYBOARD);
            }
        }
    }
}

/*
 *@@ WMChord_WinList:
 *      this displays the window list at the current
 *      mouse position when WM_CHORD comes in.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
 */

VOID WMChord_WinList(VOID)
{
    POINTL  ptlMouse;       // mouse coordinates
    SWP     WinListPos;     // position of window list window
    LONG WinListX, WinListY; // new ordinates of window list window
    // LONG DesktopCX, DesktopCY; // width and height of screen
    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // get position of window list window
    WinQueryWindowPos(G_HookData.hwndWindowList,
                      &WinListPos);
    // calculate window list position (mouse pointer is center)
    WinListX = ptlMouse.x - (WinListPos.cx / 2);
    if (WinListX < 0)
        WinListX = 0;
    WinListY = ptlMouse.y - (WinListPos.cy / 2);
    if (WinListY < 0)
        WinListY = 0;
    if (WinListX + WinListPos.cx > G_HookData.lCXScreen)
        WinListX = G_HookData.lCXScreen - WinListPos.cx;
    if (WinListY + WinListPos.cy > G_HookData.lCYScreen)
        WinListY = G_HookData.lCYScreen - WinListPos.cy;
    // set window list window to calculated position
    WinSetWindowPos(G_HookData.hwndWindowList, HWND_TOP,
                    WinListX, WinListY, 0, 0,
                    SWP_MOVE | SWP_SHOW | SWP_ZORDER);
    // now make it the active window
    WinSetActiveWindow(HWND_DESKTOP,
                       G_HookData.hwndWindowList);

}

/******************************************************************
 *                                                                *
 *  Input hook (main function)                                    *
 *                                                                *
 ******************************************************************/

/*
 *@@ hookInputHook:
 *      Input queue hook. According to PMREF, this hook gets
 *      called just before the system returns from a WinGetMsg
 *      or WinPeekMsg call -- so this does _not_ get called as
 *      a result of WinPostMsg, but before the msg is _retrieved_.
 *
 *      However, only _posted_ messages go thru this hook. _Sent_
 *      messages never get here; there is a separate hook type
 *      for that.
 *
 *      This implements the XWorkplace mouse features such as
 *      hot corners, sliding focus, mouse-button-3 scrolling etc.
 *
 *      Note: WM_CHAR messages (hotkeys) are not processed here,
 *      but rather in hookPreAccelHook, which gets called even
 *      before this hook. See remarks there.
 *
 *      We return TRUE if the message is to be swallowed, or FALSE
 *      if the current application (or the next hook in the hook
 *      chain) should still receive the message.
 *
 *      I have tried to keep the actual hook function as short as
 *      possible. Several separate subfunctions have been extracted
 *      which only get called if the respective functionality is
 *      actually needed (depending on the message and whether the
 *      respective feature has been enabled), both for clarity and
 *      speed. This is faster because the hook function gets called
 *      thousands of times (for every posted message...), and this
 *      way it is more likely that the hook code can completely remain
 *      in the processor caches all the time when lots of messages are
 *      processed.
 *
 *@@changed V0.9.1 (99-12-03) [umoeller]: added MB3 mouse scroll
 *@@changed V0.9.1 (2000-01-31) [umoeller]: fixed end-scroll bug
 *@@changed V0.9.2 (2000-02-21) [umoeller]: added PageMage processing
 *@@changed V0.9.2 (2000-02-25) [umoeller]: HScroll not working when VScroll disabled; fixed
 *@@changed V0.9.2 (2000-02-25) [umoeller]: added checks for mouse capture
 */

BOOL EXPENTRY hookInputHook(HAB hab,        // in: anchor block of receiver wnd
                            PQMSG pqmsg,    // in/out: msg data
                            USHORT fs)      // in: either PM_REMOVE or PM_NOREMOVE
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    HWND        hwnd;
    ULONG       msg;
    MPARAM      mp1, mp2;

    if (pqmsg == NULL)
        return (FALSE);

    hwnd = pqmsg->hwnd;
    msg = pqmsg->msg;
    mp1 = pqmsg->mp1;
    mp2 = pqmsg->mp2;

    if (    // PageMage running?
            (G_HookData.hwndPageMageFrame)
            // switching not disabled?
         && (!G_HookData.fDisableSwitching)
                // this flag is set frequently when PageMage
                // is doing tricky stuff; we must not process
                // messages then, or we'll recurse forever
       )
    {
        // OK, go ahead:
        ProcessMsgsForPageMage(hwnd, msg, mp1, mp2);
    }

    switch(msg)
    {
        /*
         * WM_BUTTON1DOWN:
         *      if "sliding focus" is on and "bring to top" is off,
         *      we need to explicitly raise the window under the mouse,
         *      because this fails otherwise. Apparently, PM checks
         *      internally when the mouse is clicked whether the window
         *      is active; if so, it thinks it must be on top already.
         *      Since the active and topmost window can now be different,
         *      we need to implement raising the window ourselves.
         *
         *      Parameters:
         *      -- POINTS mp1: ptsPointerPos
         *      -- USHORT SHORT1FROMMP(mp2): fsHitTestResult
         *      -- USHORT SHORT2FROMMP(mp2): fsFlags (KC_* as with WM_CHAR)
         */

        case WM_BUTTON1DOWN:
            if (    (G_HookData.HookConfig.fSlidingFocus)
                 && (!G_HookData.HookConfig.fSlidingBring2Top)
               )
            {
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                    WinSetWindowPos(GetFrameWindow(hwnd),
                                    HWND_TOP,
                                    0,
                                    0,
                                    0,
                                    0,
                                    SWP_ZORDER);
            }
        break;

        /*
         *@@ WM_BUTTON2DOWN:
         *      if mb2 is clicked on titlebar,
         *      show system menu as context menu.
         *
         *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
         */

        case WM_BUTTON2DOWN:
        {
            if (G_HookData.HookConfig.fSysMenuMB2TitleBar)
            {
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                {
                    CHAR szWindowClass[3];
                    // get class name of window being created
                    WinQueryClassName(hwnd,
                                      sizeof(szWindowClass),
                                      szWindowClass);
                    // mouse button 2 was pressed over a title bar control
                    if (strcmp(szWindowClass, "#9") == 0)
                    {
                        // copy system menu and display it as context menu
                        WMButton_SystemMenuContext(hwnd);
                        brc = TRUE; // swallow message
                    }
                }
            }
        break; }

        /*
         * WM_BUTTON3DOWN:
         *      start scrolling. From now on, WM_MOUSEMOVE
         *      (below) will call WMMouseMove_MB3Scroll.
         *
         *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
         */

        case WM_BUTTON3DOWN: // mouse button 3 was pressed down
        {
            // MB3-scroll enabled?
            if (G_HookData.HookConfig.fMB3Scroll)
            {
                // yes:
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                {
                    // set flag that we're currently scrolling
                    // (this enables scroll processing during WM_MOUSEMOVE)
                    G_HookData.fCurrentlyMB3Scrolling = TRUE;
                    // indicate that initial mouse positions have to be recalculated
                    // (checked by first call to WMMouseMove_MB3OneScrollbar)
                    G_HookData.SDXHorz.sMB3InitialMousePos = -1;
                    G_HookData.SDYVert.sMB3InitialMousePos = -1; // V0.9.2 (2000-02-25) [umoeller]
                    // reset flags for WM_BUTTON3UP below; these
                    // will be set to TRUE by WMMouseMove_MB3OneScrollbar
                    G_HookData.SDYVert.fPostSBEndScroll = FALSE;
                    G_HookData.SDXHorz.fPostSBEndScroll = FALSE;

                    // capture messages for window under mouse until
                    // MB3 is released again; this makes sure that scrolling
                    // works even if the mouse pointer is moved out of the
                    // window while MB3 is depressed
                    WinSetCapture(HWND_DESKTOP, hwnd);
                }

                // swallow msg
                brc = TRUE;
            }
        break; }

        /*
         * WM_BUTTON3UP:
         *      stop scrolling.
         *
         *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
         */

        case WM_BUTTON3UP: // mouse button 3 has been released
        {
            if (    (G_HookData.HookConfig.fMB3Scroll)
                 && (G_HookData.fCurrentlyMB3Scrolling)
               )
            {
                // set scrolling mode to off
                G_HookData.fCurrentlyMB3Scrolling = FALSE;

                // release capture
                WinSetCapture(HWND_DESKTOP, NULLHANDLE);

                if (G_HookData.SDYVert.fPostSBEndScroll)
                {
                    // we did move the scroller previously:
                    // send end scroll message
                    WinPostMsg(G_HookData.SDYVert.hwndScrollOwner,
                               WM_VSCROLL,
                               (MPARAM)(G_HookData.SDYVert.usScrollBarID),
                               MPFROM2SHORT(G_HookData.SDYVert.sCurrentThumbPos,
                                            SB_SLIDERPOSITION)); // SB_ENDSCROLL));
                }

                if (G_HookData.SDXHorz.fPostSBEndScroll)
                {
                    // we did move the scroller previously:
                    // send end scroll message
                    WinPostMsg(G_HookData.SDXHorz.hwndScrollOwner,
                               WM_HSCROLL,
                               (MPARAM)(G_HookData.SDXHorz.usScrollBarID),
                               MPFROM2SHORT(G_HookData.SDXHorz.sCurrentThumbPos,
                                            SB_SLIDERPOSITION));
                }

                // swallow msg
                brc = TRUE;
            } // end if (HookData.fCurrentlyMB3Scrolling)
        break; }

        /*
         *@@ WM_BUTTON3CLICK:
         *
         *@@added V0.9.2 (2000-02-26) [umoeller]
         */

        case WM_BUTTON3CLICK:
        case WM_BUTTON3DBLCLK:
        case WM_BUTTON3MOTIONSTART:
        case WM_BUTTON3MOTIONEND:
            if (G_HookData.HookConfig.fMB3Scroll)
                // swallow msg
                brc = TRUE;
        break;

        /*
         * WM_CHORD:
         *      MB 1 and 2 pressed simultaneously:
         *      if enabled, we show the window list at the
         *      current mouse position.
         *
         *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
         */

        case WM_CHORD:
            brc = FALSE;
            // feature enabled?
            if (G_HookData.HookConfig.fChordWinList)
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                {
                    WMChord_WinList();
                    brc = TRUE;         // swallow message
                }
        break;

        /*
         * WM_MOUSEMOVE:
         *      "hot corners", "sliding focus", "MB3 scrolling" support.
         *      This is the most complex part of the hook and calls
         *      several subfunctions in turn.
         *
         *      WM_MOUSEMOVE parameters:
         *      -- SHORT SHORT1FROMMP(mp1): sxMouse (in win coords).
         *      -- SHORT SHORT2FROMMP(mp1): syMouse (in win coords).
         *      -- USHORT SHORT1FROMMP(mp2): uswHitTest: NULL if mouse
         *                  is currently captured; otherwise result
         *                  of WM_HITTEST message.
         *      -- USHORT SHORT2FROMMP(mp2): KC_* flags as with WM_CHAR
         *                  or KC_NONE (no key pressed).
         */

        case WM_MOUSEMOVE:
        {
            BOOL    fMouseMoved = FALSE;

            // store mouse pos in win coords
            G_ptsMousePosWin.x = SHORT1FROMMP(mp1);
            G_ptsMousePosWin.y = SHORT2FROMMP(mp1);

            // has position changed since last WM_MOUSEMOVE?
            // PM keeps posting WM_MOUSEMOVE even if the
            // mouse hasn't moved, so we can drop unnecessary
            // processing...
            if (G_ptlMousePosDesktop.x != pqmsg->ptl.x)
            {
                // store x mouse pos in Desktop coords
                G_ptlMousePosDesktop.x = pqmsg->ptl.x;
                fMouseMoved = TRUE;
            }
            if (G_ptlMousePosDesktop.y != pqmsg->ptl.y)
            {
                // store y mouse pos in Desktop coords
                G_ptlMousePosDesktop.y = pqmsg->ptl.y;
                fMouseMoved = TRUE;
            }

            if (fMouseMoved)
            {
                BYTE    bHotCorner = 0;

                /*
                 * MB3 scroll
                 *
                 */

                // are we currently in scrolling mode
                // (is MB3 currently depressed)?
                if (    (G_HookData.HookConfig.fMB3Scroll)
                     && (G_HookData.fCurrentlyMB3Scrolling)
                   )
                {
                    // note that we do not check for mouse captures
                    // here, because we have captured the mouse ourselves
                    // on WM_BUTTON3DOWN
                    brc = WMMouseMove_MB3Scroll(hwnd, mp1);
                    break;  // skip all the rest
                }

                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                {
                    CHAR    szClassUnderMouse[200];

                    WinQueryClassName(hwnd,
                                      sizeof(szClassUnderMouse),
                                      szClassUnderMouse);

                    /*
                     * sliding focus:
                     *
                     */

                    WMMouseMove_SlidingFocus(hwnd,
                                             fMouseMoved,
                                             szClassUnderMouse);

                    /*
                     * hot corners:
                     *
                     */

                    // check if mouse is in one of the screen
                    // corners
                    if (G_ptlMousePosDesktop.x == 0)
                    {
                        if (G_ptlMousePosDesktop.y == 0)
                            // lower left corner:
                            bHotCorner = 1;
                        else if (G_ptlMousePosDesktop.y == G_HookData.lCYScreen - 1)
                            // top left corner:
                            bHotCorner = 2;
                    }
                    else if (G_ptlMousePosDesktop.x == G_HookData.lCXScreen - 1)
                    {
                        if (G_ptlMousePosDesktop.y == 0)
                            // lower right corner:
                            bHotCorner = 3;
                        else if (G_ptlMousePosDesktop.y == G_HookData.lCYScreen - 1)
                            // top right corner:
                            bHotCorner = 4;
                    }

                    // is mouse in a screen corner?
                    if (bHotCorner != 0)
                        // yes:
                        // notify thread-1 object window, which
                        // will start the user-configured action
                        // (if any)
                        WinPostMsg(G_HookData.hwndDaemonObject,
                                   XDM_HOTCORNER,
                                   (MPARAM)bHotCorner,
                                   (MPARAM)NULL);

                    /*
                     * auto-hide pointer:
                     *
                     */

                    if (G_HookData.HookConfig.fAutoHideMouse)
                        WMMouseMove_AutoHideMouse();

                    /*
                     * sliding menus:
                     *
                     */

                    if (G_HookData.HookConfig.fSlidingMenus)
                        if (strcmp(szClassUnderMouse, "#4") == 0)
                            // window under mouse is a menu:
                            WMMouseMove_SlidingMenus(hwnd,
                                                     mp1,
                                                     mp2);

                } // end if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)

            } // end if (fMouseMoved)

        break; } // WM_MOUSEMOVE
    }

    return (brc);                           // msg not processed if FALSE
}

/******************************************************************
 *                                                                *
 *  Pre-accelerator hook                                          *
 *                                                                *
 ******************************************************************/

/*
 *@@ WMChar_Hotkeys:
 *      this gets called from hookPreAccelHook to
 *      process WM_CHAR messages.
 *
 *      The WM_CHAR message is one of the most complicated
 *      messages which exists. This func only gets called
 *      after hookPreAccelHook has filtered out messages
 *      which we definitely won't need.
 *
 *      As opposed to folder hotkeys (folder.c),
 *      for the global object hotkeys, we need a more
 *      sophisticated processing, because we cannot rely
 *      on the usch field, which is different between
 *      PM and VIO sessions (apparently the translation
 *      is taking place differently for VIO sessions).
 *
 *      As a result, we can only use the scan code to
 *      identify hotkeys. See GLOBALHOTKEY for details.
 *
 *      Since we use a dynamically allocated array of
 *      GLOBALHOTKEY structures, to make this thread-safe,
 *      we have to use shared memory and a global mutex
 *      semaphore. See hookSetGlobalHotkeys.
 */

BOOL WMChar_Hotkeys(USHORT usFlags,  // in: SHORT1FROMMP(mp1) from WM_CHAR
                    UCHAR ucScanCode, // in: CHAR4FROMMP(mp1) from WM_CHAR
                    USHORT usch,     // in: SHORT1FROMMP(mp2) from WM_CHAR
                    USHORT usvk)     // in: SHORT2FROMMP(mp2) from WM_CHAR
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    // request access to the hotkeys mutex:
    // first we need to open it, because this
    // code can be running in any PM thread in
    // any process
    APIRET arc = DosOpenMutexSem(NULL,       // unnamed
                                 &G_hmtxGlobalHotkeys);
    if (arc == NO_ERROR)
    {
        // OK, semaphore opened: request access
        arc = WinRequestMutexSem(G_hmtxGlobalHotkeys,
                                 SEM_TIMEOUT);

        _Pmpf(("WM_CHAR WinRequestMutexSem arc: %d", arc));

        if (arc == NO_ERROR)
        {
            // OK, we got the mutex:
            PGLOBALHOTKEY pGlobalHotkeysShared = NULL;

            // request access to shared memory
            // with hotkey definitions:
            arc = DosGetNamedSharedMem((PVOID*)(&pGlobalHotkeysShared),
                                       IDSHMEM_HOTKEYS,
                                       PAG_READ | PAG_WRITE);

            _Pmpf(("  DosGetNamedSharedMem arc: %d", arc));

            if (arc == NO_ERROR)
            {
                // OK, we got the shared hotkeys:
                USHORT  us;

                PGLOBALHOTKEY pKeyThis = pGlobalHotkeysShared;

                        /*
                   In PM session:
                                                usFlags         usvk usch       ucsk
                        Ctrl alone              VK SC CTRL       0a   0          1d
                        Ctrl-A                     SC CTRL        0  61          1e
                        Ctrl-Alt                VK SC CTRL ALT   0b   0          38
                        Ctrl-Alt-A                 SC CTRL ALT    0  61          1e

                   In VIO session:
                        Ctrl alone                 SC CTRL       07   0          1d
                        Ctrl-A                     SC CTRL        0   1e01       1e
                        Ctrl-Alt                   SC CTRL ALT   07   0          38
                        Ctrl-Alt-A                 SC CTRL ALT   20   1e00       1e

                        Alt-A                      SC      ALT   20   1e00
                        Ctrl-E                     SC CTRL        0   3002

                        */

                #ifdef _PMPRINTF_
                        CHAR    szFlags[2000] = "";
                        if (usFlags & KC_CHAR)                      // 0x0001
                            strcat(szFlags, "KC_CHAR ");
                        if (usFlags & KC_VIRTUALKEY)                // 0x0002
                            strcat(szFlags, "KC_VIRTUALKEY ");
                        if (usFlags & KC_SCANCODE)                  // 0x0004
                            strcat(szFlags, "KC_SCANCODE ");
                        if (usFlags & KC_SHIFT)                     // 0x0008
                            strcat(szFlags, "KC_SHIFT ");
                        if (usFlags & KC_CTRL)                      // 0x0010
                            strcat(szFlags, "KC_CTRL ");
                        if (usFlags & KC_ALT)                       // 0x0020
                            strcat(szFlags, "KC_ALT ");
                        if (usFlags & KC_KEYUP)                     // 0x0040
                            strcat(szFlags, "KC_KEYUP ");
                        if (usFlags & KC_PREVDOWN)                  // 0x0080
                            strcat(szFlags, "KC_PREVDOWN ");
                        if (usFlags & KC_LONEKEY)                   // 0x0100
                            strcat(szFlags, "KC_LONEKEY ");
                        if (usFlags & KC_DEADKEY)                   // 0x0200
                            strcat(szFlags, "KC_DEADKEY ");
                        if (usFlags & KC_COMPOSITE)                 // 0x0400
                            strcat(szFlags, "KC_COMPOSITE ");
                        if (usFlags & KC_INVALIDCOMP)               // 0x0800
                            strcat(szFlags, "KC_COMPOSITE ");

                        _Pmpf(("  usFlags: 0x%lX -->", usFlags));
                        _Pmpf(("    %s", szFlags));
                        _Pmpf(("  ucScanCode: 0x%lX", ucScanCode));
                        _Pmpf(("  usvk: 0x%lX", usvk));
                        _Pmpf(("  usch: 0x%lX", usch));
                #endif

                // filter out unwanted flags
                usFlags &= (KC_VIRTUALKEY | KC_CTRL | KC_ALT | KC_SHIFT);

                _Pmpf(("  Checking %d hotkeys", cGlobalHotkeys));

                // now go through the shared hotkey list and check
                // if the pressed key was assigned an action to
                us = 0;
                for (us = 0;
                     us < G_cGlobalHotkeys;
                     us++)
                {
                    _Pmpf(("    item %d: scan code %lX", us, pKeyThis->ucScanCode));

                    if (   (pKeyThis->usFlags == usFlags)
                        && (pKeyThis->ucScanCode == ucScanCode)
                       )
                    {
                        // hotkey found:
                        // notify daemon
                        WinPostMsg(G_HookData.hwndDaemonObject,
                                   XDM_HOTKEYPRESSED,
                                   (MPARAM)(pKeyThis->ulHandle),
                                   (MPARAM)0);

                        // reset return code: swallow this message
                        brc = TRUE;
                        // get outta here
                        break; // for
                    }

                    // not found: go for next key to check
                    pKeyThis++;
                } // end for

                DosFreeMem(pGlobalHotkeysShared);
            } // end if DosGetNamedSharedMem

            DosReleaseMutexSem(G_hmtxGlobalHotkeys);
        } // end if WinRequestMutexSem

        DosCloseMutexSem(G_hmtxGlobalHotkeys);
    } // end if DosOpenMutexSem

    return (brc);
}

/*
 *@@ hookPreAccelHook:
 *      this is the pre-accelerator-table hook. Like
 *      hookInputHook, this gets called when messages
 *      are coming in from the system input queue, but
 *      as opposed to a "regular" input hook, this hook
 *      gets called even before translation from accelerator
 *      tables occurs.
 *
 *      Pre-accel hooks are not documented in the PM Reference.
 *      I have found this idea (and part of the implementation)
 *      in the source code of ProgramCommander/2, so thanks
 *      go out (once again) to Roman Stangl.
 *
 *      In this hook, we check for the global object hotkeys
 *      so that we can use certain key combinations regardless
 *      of whether they might be currently used in application
 *      accelerator tables. This is especially useful for
 *      "Alt" key combinations, which are usually intercepted
 *      when menus have corresponding shortcuts.
 *
 *      As a result, as opposed to other hotkey software you
 *      might know, XWorkplace does properly react to "Ctrl+Alt"
 *      keyboard combinations, even if a menu would get called
 *      with the "Alt" key. ;-)
 *
 *      We return TRUE if the message is to be swallowed, or FALSE
 *      if the current application (or the next hook in the hook
 *      chain) should still receive the message.
 */

BOOL EXPENTRY hookPreAccelHook(HAB hab, PQMSG pqmsg, ULONG option)
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL        brc = FALSE;

    HWND        hwnd;
    ULONG       msg;
    MPARAM      mp1, mp2;

    if (pqmsg == NULL)
        return (FALSE);

    hwnd = pqmsg->hwnd;
    msg = pqmsg->msg;
    mp1 = pqmsg->mp1;
    mp2 = pqmsg->mp2;

    switch(msg)
    {
        /*
         * WM_CHAR:
         *      keyboard activity. We check for
         *      object hotkeys; if one is found,
         *      we post XWPDAEMN's object window
         *      a notification.
         */

        case WM_CHAR:
            if (G_HookData.HookConfig.fGlobalHotkeys)
            {
                USHORT usFlags    = SHORT1FROMMP(mp1);
                // UCHAR  ucRepeat   = CHAR3FROMMP(mp1);
                UCHAR  ucScanCode = CHAR4FROMMP(mp1);
                USHORT usch       = SHORT1FROMMP(mp2);
                USHORT usvk       = SHORT2FROMMP(mp2);

                if (    // process only key-down messages
                        ((usFlags & KC_KEYUP) == 0)
                        // check flags:
                    &&  (     ((usFlags & KC_VIRTUALKEY) != 0)
                              // Ctrl pressed?
                           || ((usFlags & KC_CTRL) != 0)
                              // Alt pressed?
                           || ((usFlags & KC_ALT) != 0)
                              // or one of the Win95 keys?
                           || (   ((usFlags & KC_VIRTUALKEY) == 0)
                               && (     (usch == 0xEC00)
                                    ||  (usch == 0xED00)
                                    ||  (usch == 0xEE00)
                                  )
                              )
                        )
                        // filter out those ugly composite key (accents etc.)
                   &&   ((usFlags & (KC_DEADKEY | KC_COMPOSITE | KC_INVALIDCOMP)) == 0)
                   )
                {
                    brc = WMChar_Hotkeys(usFlags, ucScanCode, usch, usvk);
                            // returns TRUE (== swallow) if hotkey was found
                }
            }
        break; // WM_CHAR
    } // end switch(msg)

    return (brc);
}

