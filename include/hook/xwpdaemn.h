
/*
 * xwpdaemn.h:
 *      XPager and daemon declarations.
 *      These are not visible to the hook nor
 *      to XFLDR.DLL.
 *
 *      Requires xwphook.h to be included first.
 *
 *@@include #define INCL_DOSSEMAPHORES
 *@@include #include <os2.h>
 *@@include #include "hook\xwphook.h"
 *@@include #include "hook\xwpdaemn.h"
 */

/*
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *      Copyright (C) 2000 Ulrich M�ller.
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

#ifndef PAGER_HEADER_INCLUDED
    #define PAGER_HEADER_INCLUDED

#ifndef __NOPAGER__

    /* ******************************************************************
     *
     *   Pager interface
     *
     ********************************************************************/

    VOID dmnKillXPager(BOOL fNotifyKernel);

    BOOL dmnLoadPagerSettings(ULONG flConfig);

    /* ******************************************************************
     *
     *   Pager definitions
     *
     ********************************************************************/

    #define TIMEOUT_HMTX_WINLIST    20*1000
                // raised V0.9.12 (2001-05-31) [umoeller]

    // move hotkey flags
    #define MOVE_LEFT           1
    #define MOVE_RIGHT          2
    #define MOVE_UP             4
    #define MOVE_DOWN           8

    // font ID to use for the XPager window titles;
    // we always use the same one, because there's only one
    // in the daemon process
    #define LCID_PAGER_FONT  ((ULONG)1)

    #ifdef DEBUG_WINDOWLIST
        typedef struct WINLISTRECORD
        {
            RECORDCORE      recc;

            PSZ             pszSwtitle;

            PSZ             pszWindowType;

            PSZ             pszPID;
            CHAR            szPID[20];

            PVOID           pWinInfo;       // reverse linkage

        } WINLISTRECORD, *PWINLISTRECORD;
    #endif

    #pragma pack(1)

    /*
     *@@ PAGERWININFO:
     *      one of these exists for every window
     *      which is currently handled by XPager.
     *
     *      This was completely revamped with V0.9.7.
     *      XPager no longer uses a global fixed
     *      array of these items, but maintains a
     *      linked list now. I believe some of the
     *      XPager hangs we had previously were
     *      due to an overflow of the global array.
     *
     *@@added V0.9.7 (2001-01-21) [umoeller]
     */

    typedef struct _PAGERWININFO
    {
        HSWITCH     hsw;                // switch entry or NULLHANDLE

        SWCNTRL     swctl;              // switch list entry
                        // HWND hwnd
                        // HWND hwndIcon
                        // HPROGRAM hprog
                        // PID idProcess
                        // ULONG idSession
                        // ULONG uchVisibility
                        // ULONG fbJump
                        // CHAR szSwTitle[MAXNAMEL+4]
                        // ULONG bProgType

        HPOINTER    hptrFrame;          // frame icon (WM_QUERYICON)

        BYTE        bWindowType;
            // the following types are treated as "normal"
            // windows and moved around by the pager
            #define WINDOW_NORMAL       1
            #define WINDOW_MAXIMIZE     2   // window is maximized
            // the following styles are treated as special
            // and are left alone by the pager (always sticky)
            #define WINDOW_MINIMIZE     3   // window is minimized, treat as sticky
            #define WINDOW_XWPDAEMON    4   // probably XPager or scroll window,
                                            // ignore (sticky)
            #define WINDOW_WPSDESKTOP   5   // WPS desktop, always sticky
            #define WINDOW_STICKY       6   // window is on sticky list
            #define WINDOW_NIL          7   // "not in list" == not in switch list
            #define WINDOW_HIDDEN       8

        CHAR        szClassName[30];
        ULONG       tid;

        SWP         swp;                // last known window pos

        #ifdef DEBUG_WINDOWLIST
            PWINLISTRECORD prec;
        #endif

    } PAGERWININFO, *PPAGERWININFO;

    #pragma pack()

    /* ******************************************************************
     *
     *   Pager window list
     *
     ********************************************************************/

    APIRET pgrInit(VOID);

    BOOL pgrLockWinlist(VOID);

    VOID pgrUnlockWinlist(VOID);

    PPAGERWININFO pgrFindWinInfo(HWND hwndThis,
                                 PVOID *ppListNode);

    BOOL pgrGetWinInfo(PPAGERWININFO pWinInfo);

    BOOL pgrCreateWinInfo(HWND hwnd);

    VOID pgrBuildWinlist(VOID);

    VOID pgrFreeWinInfo(HWND hwnd);

    BOOL pgrRefresh(HWND hwnd);

    BOOL pgrIsSticky(HWND hwnd,
                     PCSZ pcszSwtitle);

    BOOL pgrIconChange(HWND hwnd,
                       HPOINTER hptr);

    #ifdef INCL_WINSWITCHLIST
    PSWBLOCK pgrQueryWinList(ULONG pid);
    #endif

    #ifdef THREADS_HEADER_INCLUDED
        VOID _Optlink fntWinlistThread(PTHREADINFO pti);
    #endif

    /* ******************************************************************
     *
     *   Pager control window
     *
     ********************************************************************/

    BOOL pgrLockHook(PCSZ pcszFile, ULONG ulLine, PCSZ pcszFunction);

    VOID pgrUnlockHook(VOID);

    LONG pgrCalcClientCY(LONG cx);

    BOOL pgrIsShowing(PSWP pswp);

    VOID pgrRecoverWindows(HAB hab);

    BOOL pgrCreatePager(VOID);

    /* ******************************************************************
     *
     *   Pager window movement
     *
     ********************************************************************/

    #ifdef THREADS_HEADER_INCLUDED
        VOID _Optlink fntMoveThread(PTHREADINFO pti);
    #endif

#endif

    /* ******************************************************************
     *
     *   Drive monitors
     *
     ********************************************************************/

    BOOL dmnAddDiskfreeMonitor(ULONG ulLogicalDrive,
                               HWND hwndNotify,
                               ULONG ulMessage);

    #ifdef THREADS_HEADER_INCLUDED
        void _Optlink fntDiskWatch(PTHREADINFO ptiMyself);
    #endif

    BOOL dmnQueryDisks(ULONG ulLogicalDrive,
                       MPARAM mpDiskInfos);

    /* ******************************************************************
     *
     *   Global variables in xwpdaemn.c
     *
     ********************************************************************/

    #ifdef HOOK_PRIVATE_HEADER_INCLUDED
        extern PHOOKDATA    G_pHookData;
    #endif

    extern PXWPGLOBALSHARED G_pXwpGlobalShared;

    extern HPOINTER     G_hptrDaemon;

    #ifdef LINKLIST_HEADER_INCLUDED
        extern LINKLIST     G_llWinInfos;
    #endif

#endif

