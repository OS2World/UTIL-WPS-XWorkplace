
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
 *      Copyright (C) 1997-99 Ulrich M�ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XFolder main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef KERNEL_HEADER_INCLUDED
    #define KERNEL_HEADER_INCLUDED

    // required header files
    #include "helpers\threads.h"

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

        // MMPM/2 status flags in KERNELGLOBALS.ulMMPM2Working;
        // these reflect the status of SOUND.DLL.
        // If this is anything other than MMSTAT_WORKING, sounds
        // are disabled.
        #define MMSTAT_UNKNOWN             0        // initial value
        #define MMSTAT_WORKING             1        // SOUND.DLL is working
        #define MMSTAT_MMDIRNOTFOUND       2        // MMPM/2 directory not found
        #define MMSTAT_SOUNDLLNOTFOUND     3        // SOUND.DLL not found
        #define MMSTAT_SOUNDLLNOTLOADED    4        // SOUND.DLL not loaded (DosLoadModule error)
        #define MMSTAT_SOUNDLLFUNCERROR    5
        #define MMSTAT_CRASHED             6        // SOUND.DLL crashed, sounds disabled

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

            ULONG               ulPanicFlags;
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

            // new classes
                                fXWPSetup,
                                fXFldSystem,
                                fXFldWPS,
                                fXFldStartup,
                                fXFldShutdown,
                                fXWPClassList,
                                fXWPTrashCan,
                                fXWPTrashObject;

            /*
             * XWorkplace daemon
             */

            HAPP                happDaemon;
            PVOID               pDaemonShared;  // ptr to DAEMONSHARED structure

            /*
             * Thread-1 object window:
             *      additional object window on thread 1.
             *      This is always created.
             */

            // Workplace thread ID (PMSHELL.EXE); should be 1
            TID                 tidWorkplaceThread;

            // XFolder Workplace object window handle
            HWND                hwndThread1Object;

            // WPS startup date and time (krnInitializeXWorkplace)
            DATETIME            StartupDateTime;

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
             * Quick thread:
             *      this thread is always running.
             */

            PTHREADINFO         ptiQuickThread;
            HWND                hwndQuickObject;

            // sound data
            ULONG               ulMMPM2Working;      // MMSTAT_* flags above
            USHORT              usDeviceID;

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

    /*
     *@@ T1M_BEGINSTARTUP:
     *      this is an XFolder msg posted by the Worker thread after
     *      the Desktop has been populated; this performs initialization
     *      of the Startup folder process, which will then be further
     *      performed by (again) the Worker thread.
     *
     *      Parameters: none.
     */

    #define T1M_BEGINSTARTUP            WM_USER+1100

    /*
     *@@ T1M_POCCALLBACK:
     *      this msg is posted from the Worker thread whenever
     *      a callback func needs to be called during those
     *      "process ordered content" (POC) functions (initiated by
     *      XFolder::xwpProcessOrderedContent); we need this message to
     *      have the callback func run on the folder's thread.
     *
     *      Parameters:
     *          PPROCESSCONTENTINFO mp1
     *                          structure with all the information;
     *                          this routine must set the hwndView field
     *          MPARAM mp2      always NULL
     */

    #define T1M_POCCALLBACK             WM_USER+1101

    /*
     *@@ T1M_BEGINQUICKOPEN:
     *      this is posted by the Worker thread after the startup
     *      folder has been processed; we will now go for the
     *      "Quick Open" folders by populating them and querying
     *      all their icons. If this is posted for the first time,
     *      mp1 must be NULL; for subsequent postings, mp1 contains the
     *      folder to be populated or (-1) if it's the last.
     */

    #define T1M_BEGINQUICKOPEN          WM_USER+1102
    #define T1M_NEXTQUICKOPEN           WM_USER+1103

    /*
     *@@ T1M_LIMITREACHED:
     *      this is posted by cmnAppendMi2List when too
     *      many menu items are in use, i.e. the user has
     *      opened a zillion folder content menus; we
     *      will display a warning dlg, which will also
     *      destroy the open menu
     */

    #define T1M_LIMITREACHED            WM_USER+1104

    /*
     *@@ T1M_EXCEPTIONCAUGHT:
     *      this is posted from the various XFolder threads
     *      when something trapped; it is assumed that
     *      mp1 is a PSZ to an error msg allocated with
     *      malloc(), and after displaying the error,
     *      (PSZ)mp1 is freed here. If mp2 != NULL, the WPS will
     *      be restarted (this is demanded by XSHutdown traps).
     */

    #define T1M_EXCEPTIONCAUGHT         WM_USER+1105

    /*
     *@@ T1M_QUERYXFOLDERVERSION:
     *      this msg may be send to the XFolder object
     *      window from external programs to query the
     *      XFolder version number which is currently
     *      installed. We will return:
     *          mrc = MPFROM2SHORT(major, minor)
     *      which may be broken down by the external
     *      program using the SHORT1/2FROMMP macros.
     *      This is used by the XShutdown command-line
     *      interface (XSHUTDWN.EXE) to assert that
     *      XFolder is up and running, but can be used
     *      by other software too.
     */

    #define T1M_QUERYXFOLDERVERSION     WM_USER+1106

    /*
     *@@ T1M_EXTERNALSHUTDOWN:
     *      this msg may be posted to the XFolder object
     *      window from external programs to initiate
     *      the eXtended shutdown. mp1 is assumed to
     *      point to a block of shared memory containing
     *      a SHUTDOWNPARAMS structure.
     */
    #define T1M_EXTERNALSHUTDOWN        WM_USER+1107

    /*
     *@@ T1M_DESTROYARCHIVESTATUS:
     *      this gets posted from arcCheckIfBackupNeeded,
     *      which gets called from krnInitializeXWorkplace
     *      with the handle of this object wnd and this message ID.
     */

    #define T1M_DESTROYARCHIVESTATUS    WM_USER+1108    // added V0.9.0

    /*
     *@@ T1M_OPENOBJECTFROMHANDLE:
     *      this can be posted to the thread-1 object
     *      window from anywhere to have an object
     *      opened in its default view. As opposed to
     *      WinOpenObject, which opens the object on
     *      thread 13 (on my system), the thread-1
     *      object window will always open the object
     *      on thread 1, which leads to less problems.
     *
     *      Parameters:
     *      -- HOBJECT mp1: object handle to open.
     *              The following "special objects" exist:
     *              0xFFFF0000: show window list.
     *      -- mp2: not used, always 0.
     */

    #define T1M_OPENOBJECTFROMHANDLE    WM_USER+1110    // added V0.9.0

    /*
     *@@ T1M_DAEMONREADY:
     *      posted by the XWorkplace daemon after it has
     *      successfully created its object window. The
     *      thread-1 object window will then send XDM_HOOKINSTALL
     *      back to the daemon if the global settings have the
     *      hook enabled.
     */

    #define T1M_DAEMONREADY             WM_USER+1111    // added V0.9.0

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
