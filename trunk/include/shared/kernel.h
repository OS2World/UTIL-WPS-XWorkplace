
/*
 *@@sourcefile kernel.h:
 *      header file for kernel.c. See remarks there.
 *
 *@@include #define INCL_DOSSEMAPHORES
 *@@include #define INCL_WINWINDOWMGR
 *@@include #define INCL_WINPOINTERS
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"       // for some features
 *@@include #include "helpers\threads.h"
 *@@include #include <wpobject.h>       // or any other WPS header, for KERNELGLOBALS
 *@@include #include "shared\common.h"
 *@@include #include "shared\kernel.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M�ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef KERNEL_HEADER_INCLUDED
    #define KERNEL_HEADER_INCLUDED

    // required header files
    #include "helpers\threads.h"

    #ifndef INCL_DOSSEMAPHORES
        #error kernel.h requires INCL_DOSSEMAPHORES to be defined.
    #endif

    /* ******************************************************************
     *                                                                  *
     *   Resource protection (thread safety)                            *
     *                                                                  *
     ********************************************************************/

    BOOL krnLock(ULONG ulTimeout);

    VOID krnUnlock(VOID);

    ULONG krnQueryLock(VOID);

    VOID APIENTRY krnOnKillDuringLock(VOID);

    /********************************************************************
     *                                                                  *
     *   KERNELGLOBALS structure                                        *
     *                                                                  *
     ********************************************************************/

    #ifdef SOM_WPObject_h

        // startup flags for KERNELGLOBALS.ulStartupFlags
        #define SUF_SKIPBOOTLOGO            0x0001  // skip boot logo
        #define SUF_SKIPXFLDSTARTUP         0x0002  // skip XFldStartup processing
        #define SUF_SKIPQUICKOPEN           0x0004  // skip "quick open" folder processing

        /*
         *@@ KERNELGLOBALS:
         *      this structure is stored in a static global
         *      variable in kernel.c, whose address can
         *      always be obtained thru t1QueryGlobals.
         *
         *      This structure is used to store information
         *      which is of importance to all parts of XWorkplace.
         *      This includes the old XFolder code as well as
         *      possible new parts.
         *
         *      Here we store information about the classes
         *      which have been successfully initialized and
         *      lots of thread data.
         *
         *      You may extend this structure if you need to
         *      store global data, but please do not modify
         *      the existing fields. Also, for lucidity, use
         *      this structure only if you need to access
         *      data across several code files. Otherwise,
         *      please use global variables.
         */

        typedef struct _KERNELGLOBALS
        {
            // WPS startup date and time (krnInitializeXWorkplace)
            DATETIME        StartupDateTime;

            // PM error windows queried by krnInitializeXWorkplace
            HWND            hwndHardError,
                            hwndSysError;

            ULONG           ulPanicFlags;
                    // flags set by the "panic" dialog if "Shift"
                    // was pressed during startup.
                    // Per default, this field is set to zero,
                    // but if the user disables something in the
                    // "Panic" dialog, this may have any of the
                    // following flags:
                    // -- SUF_SKIPBOOTLOGO: skip boot logo
                    // -- SUF_SKIPXFLDSTARTUP: skip XFldStartup processing
                    // -- SUF_SKIPQUICKOPEN: skip "quick open" folder processing

            /*
             * XWorkplace class objects (new with V0.9.0):
             *      if any of these is FALSE, this means that
             *      the class has not been installed. All
             *      these things are set to TRUE by the respective
             *      wpclsInitData class methods.
             *
             *      If you introduce a new class to the XWorkplace
             *      class setup, please add a BOOL to this list and
             *      set that field to TRUE in the wpclsInitData of
             *      your class.
             */

            // class replacements
            BOOL                fXFldObject,
                                fXFolder,
                                fXFldDisk,
                                fXFldDesktop,
                                fXFldDataFile,
                                fXFldProgramFile,
                                fXWPSound,
                                fXWPMouse,
                                fXWPKeyboard,

            // new classes
                                fXWPSetup,
                                fXFldSystem,
                                fXFldWPS,
                                fXWPScreen,
                                fXFldStartup,
                                fXFldShutdown,
                                fXWPClassList,
                                fXWPTrashCan,
                                fXWPTrashObject,
                                fXWPString,
                                fXWPMedia;

            /*
             * XWorkplace daemon
             */

            HAPP                happDaemon;
                    // != NULLHANDLE if daemon was started

            PVOID               pDaemonShared;
                    // ptr to DAEMONSHARED structure

            HWND                hwndActiveDesktop;
                    // currently active Desktop HWND; this is != NULLHANDLE
                    // after the Desktop has been populated

            /*
             * Thread-1 object window:
             *      additional object window on thread 1.
             *      This is always created.
             */

            // Workplace thread ID (PMSHELL.EXE); should be 1
            TID                 tidWorkplaceThread;

            // XFolder Workplace object window handle
            HWND                hwndThread1Object;

            /*
             * Worker thread:
             *      this thread is always running.
             */

            PTHREADINFO         ptiWorkerThread;
            HWND                hwndWorkerObject;

            ULONG               ulWorkerMsgCount;
            BOOL                WorkerThreadHighPriority;

            // here comes the linked list to remember all objects which
            // have been awakened by the WPS; this list is maintained
            // by the Worker thread and evaluated during XShutdown

            // root of this linked list; this holds plain
            // WPObject* pointers and is created in krnInitializeXWorkplace
            // with lstCreate(FALSE)
            PVOID               pllAwakeObjects;
                    // this has changed with V0.90; this is actually a PLINKLIST,
                    // but since not all source files have #include'd linklist.h,
                    // we declare this as PVOID to avoid compilation errors.

            // mutex semaphore for access to this list
            HMTX                hmtxAwakeObjects;

            // count of currently awake objects
            LONG                lAwakeObjectsCount;

            // address of awake WarpCenter; stored by Worker
            // thread, read by Shutdown thread
            WPObject            *pAwakeWarpCenter;

            /*
             * Speedy thread:
             *      this thread is always running.
             */

            PTHREADINFO         ptiSpeedyThread;
            HWND                hwndSpeedyObject;

            /*
             * File thread:
             *      this thread is always running also,
             *      but with regular priority.
             */

            PTHREADINFO         ptiFileThread;
            HWND                hwndFileObject;

            /*
             * Shutdown threads:
             *      the following threads are only running
             *      while XShutdown is in progress.
             */

            PTHREADINFO         ptiShutdownThread,
                                ptiUpdateThread;

            BOOL                fShutdownRunning;

            // debugging flags
            ULONG               ulShutdownFunc,
                                ulShutdownFunc2;

            // CONFIG.SYS filename (XFldSystem)
            CHAR                szConfigSys[CCHMAXPATH];

        } KERNELGLOBALS, *PKERNELGLOBALS;

        typedef const KERNELGLOBALS* PCKERNELGLOBALS;

        PCKERNELGLOBALS krnQueryGlobals(VOID);

        PKERNELGLOBALS krnLockGlobals(ULONG ulTimeout);

        VOID krnUnlockGlobals(VOID);

    #endif

    /* ******************************************************************
     *                                                                  *
     *   Startup/Daemon interface                                       *
     *                                                                  *
     ********************************************************************/

    VOID krnSetProcessStartupFolder(BOOL fReuse);

    BOOL krnNeed2ProcessStartupFolder(VOID);

    BOOL krnPostDaemonMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *                                                                  *
     *   Thread-1 object window                                         *
     *                                                                  *
     ********************************************************************/

    #define T1M_BEGINSTARTUP            (WM_USER+400)

    #define T1M_POCCALLBACK             (WM_USER+401)

    #define T1M_BEGINQUICKOPEN          (WM_USER+402)
    #define T1M_NEXTQUICKOPEN           (WM_USER+403)

    #define T1M_LIMITREACHED            (WM_USER+404)

    #define T1M_EXCEPTIONCAUGHT         (WM_USER+405)

    #define T1M_EXTERNALSHUTDOWN        (WM_USER+407)

    #define T1M_DESTROYARCHIVESTATUS    (WM_USER+408)    // added V0.9.0

    #define T1M_OPENOBJECTFROMHANDLE    (WM_USER+410)    // added V0.9.0

    #define T1M_DAEMONREADY             (WM_USER+411)    // added V0.9.0

    #define T1M_PAGEMAGECLOSED          (WM_USER+412)    // added V0.9.2 (2000-02-23) [umoeller]

    #define T1M_QUERYXFOLDERVERSION     (WM_USER+413)
                // V0.9.2 (2000-02-26) [umoeller]:
                // msg value changed to break compatibility with V0.8x

    #define T1M_PAGEMAGECONFIGDELAYED   (WM_USER+414)

    MRESULT EXPENTRY krn_fnwpThread1Object(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2);

    BOOL krnPostThread1ObjectMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    MRESULT krnSendThread1ObjectMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *                                                                  *
     *   XWorkplace initialization                                      *
     *                                                                  *
     ********************************************************************/

    VOID krnInitializeXWorkplace(VOID);

#endif
