
/*
 *@@sourcefile hookintf.c:
 *      daemon/hook interface. This is responsible for
 *      configuring the hook and the daemon.
 *
 *      This has both public interface functions for
 *      configuring the hook (thru the daemon) and
 *      the "Mouse" and "Keyboard" notebook pages.
 *
 *      NEVER call the hook or the daemon directly.
 *      ALWAYS use the functions in this file, which
 *      take care of proper serialization.
 *
 *      The code in here gets called from classes\xwpkeybd.c
 *      (XWPKeyboard), classes\xwpmouse.c (XWPMouse),
 *      classes\xfobj.c (XFldObject).
 *
 *      Function prefix for this file:
 *      --  hif*
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "config\hookintf.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M�ller.
 *      This file is part of the XWorkplace source package.
 *      XWorkplace is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published
 *      by the Free Software Foundation, in version 2 as it comes in the
 *      "COPYING" file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#pragma strings(readonly)

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSSEMAPHORES
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINWINDOWMGR
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSTDSLIDER
#include <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "xwpapi.h"                     // public XWorkplace definitions

#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "config\hookintf.h"            // daemon/hook interface

#pragma hdrstop
#include <wpfolder.h>                   // WPFolder
#include <wpshadow.h>                   // WPShadow

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

static PFNWP G_pfnwpEntryFieldOrig = 0;

/* ******************************************************************
 *                                                                  *
 *   XWorkplace daemon/hook interface                               *
 *                                                                  *
 ********************************************************************/

/*
 *@@ hifLoadHookConfig:
 *      loads the hook configuration data into the
 *      specified buffer.
 *
 *@@added V0.9.3 (2000-05-21) [umoeller]
 */

BOOL hifLoadHookConfig(PHOOKCONFIG phc)
{
    ULONG cb = sizeof(HOOKCONFIG);
    memset(phc, 0, sizeof(HOOKCONFIG));
    // overwrite from INI, if found
    return (PrfQueryProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_CONFIG,
                                phc,
                                &cb));
}

#ifndef __ALWAYSHOOK__

/*
 *@@ hifEnableHook:
 *      installs or deinstalls the hook.
 *
 *      Returns the new hook install status.
 *      If this is the same as fEnable, the
 *      operation was successfull.
 *
 *      This does NOT change the GLOBALSETTINGS.
 *
 *@@added V0.9.2 (2000-02-22) [umoeller]
 */

BOOL hifEnableHook(BOOL fEnable)
{
    BOOL    brc = FALSE;
    PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
    PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;

    _Pmpf(("hifEnableHook (%d)", fEnable));

    // (de)install the hook by notifying the daemon
    if (!pXwpGlobalShared)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "pXwpGlobalShared is NULL.");
    else
    {
        if (!pXwpGlobalShared->hwndDaemonObject)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "pXwpGlobalShared->hwndDaemonObject is NULLHANDLE.");
        else
        {
            _Pmpf(("  Sending XDM_HOOKINSTALL"));
            if (WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                           XDM_HOOKINSTALL,
                           (MPARAM)(fEnable),
                           0))
            {
                HWND hwndActiveDesktop = cmnQueryActiveDesktopHWND();
                _Pmpf(("  Posting XDM_DESKTOPREADY (0x%lX)",
                        hwndActiveDesktop));
                // hook installed:
                brc = TRUE;
                // tell the daemon about the Desktop window
                krnPostDaemonMsg(XDM_DESKTOPREADY,
                                 (MPARAM)hwndActiveDesktop,
                                 (MPARAM)0);
            }
            // else: hook not installed
        }
    }

    return (brc);
}

#endif

/*
 *@@ hifXWPHookReady:
 *      this returns TRUE only if all of the following apply:
 *
 *      --  the XWorkplace daemon is running;
 *
 *      --  the XWorkplace hook is loaded;
 *
 *      --  the XWorkplace hook has been enabled in XWPSetup.
 *
 *      This is used by the various configuration subcomponents
 *      to check whether the hook functionality is currently
 *      available.
 *
 *@@added V0.9.0 [umoeller]
 */

BOOL hifXWPHookReady(VOID)
{
    BOOL brc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;
    // // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    if (pXwpGlobalShared)
        if (pXwpGlobalShared->fAllHooksInstalled)
#ifndef __ALWAYSHOOK__
            if (cmnQuerySetting(sfXWPHook))
#endif
                brc = TRUE;
    return (brc);
}

/*
 *@@ hifEnablePageMage:
 *      enables or disables PageMage by sending
 *      XDM_STARTSTOPPAGEMAGE to the daemon.
 *
 *      This does not change the GLOBALSETTINGS.
 *
 *@@added V0.9.2 (2000-02-22) [umoeller]
 */

BOOL hifEnablePageMage(BOOL fEnable)
{
    BOOL    brc = FALSE;

#ifndef __NOPAGEMAGE__
    PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
    PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;

    _Pmpf(("hifEnablePageMage: %d", fEnable));

    // (de)install the hook by notifying the daemon
    if (!pXwpGlobalShared)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "pXwpGlobalShared is NULL.");
    else
    {
        if (!pXwpGlobalShared->hwndDaemonObject)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "pXwpGlobalShared->hwndDaemonObject is NULLHANDLE. XWPDAEMN.EXE is probably not running.");
        else
        {
            _Pmpf(("hifEnablePageMage: Sending XDM_STARTSTOPPAGEMAGE"));
            if (WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                           XDM_STARTSTOPPAGEMAGE,
                           (MPARAM)(fEnable),
                           0))
                // PageMage installed:
                brc = TRUE;

            // else: hook not installed
            _Pmpf(("  Returning %d", brc));
        }
    }
#endif

    return (brc);
}

/*
 *@@ hifHookConfigChanged:
 *      this writes the HOOKCONFIG structure which
 *      pvdc points to back to OS2.INI and posts
 *      XDM_HOOKCONFIG to the daemon, which then
 *      re-reads that data for both the daemon and
 *      the hook.
 *
 *      Note that the config data does not include
 *      object hotkeys. Use hifSetObjectHotkeys for
 *      that.
 *
 *      pvdc is declared as a PVOID so that xwphook.h
 *      does not have included when including common.h,
 *      but _must_ be a PHOOKCONFIG really.
 *
 *@@added V0.9.0 [umoeller]
 */

BOOL hifHookConfigChanged(PVOID pvdc)
{
    // store HOOKCONFIG back to INI
    // and notify daemon
    BOOL            brc = FALSE;

    if (pvdc)
    {
        PHOOKCONFIG   pdc = (PHOOKCONFIG)pvdc;

        PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
        PXWPGLOBALSHARED   pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;

        brc = PrfWriteProfileData(HINI_USER,
                                  INIAPP_XWPHOOK,
                                  INIKEY_HOOK_CONFIG,
                                  pdc,
                                  sizeof(HOOKCONFIG));

        if (!brc)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "PrfWriteProfileData failed.");
        else
        {
            if (!pXwpGlobalShared)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "pXwpGlobalShared is NULL.");
            else
                if (!pXwpGlobalShared->hwndDaemonObject)
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "pXwpGlobalShared->hwndDaemonObject is NULLHANDLE.");
                else
                    // cross-process send msg: this
                    // does not return until the daemon
                    // has re-read the data
                    brc = (BOOL)WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                                           XDM_HOOKCONFIG,
                                           0, 0);
        }
    }

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Object hotkeys interface                                       *
 *                                                                  *
 ********************************************************************/

#ifndef __ALWAYSOBJHOTKEYS__

/*
 *@@ hifObjectHotkeysEnabled:
 *      returns TRUE if object hotkeys have been
 *      enabled. This does not mean that the
 *      hook is running, but returns the flag
 *      in HOOKCONFIG only. So check hifXWPHookReady()
 *      in addition to this.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL hifObjectHotkeysEnabled(VOID)
{
    BOOL    brc = FALSE;
    HOOKCONFIG  HookConfig;
    ULONG       cb = sizeof(HookConfig);
    if (PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_CONFIG,
                            &HookConfig,
                            &cb))
        if (HookConfig.__fGlobalHotkeys)
            brc = TRUE;

    return (brc);
}

/*
 *@@ hifEnableObjectHotkeys:
 *      enables or disables object hotkeys
 *      altogether. This does not change
 *      the hotkeys list.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

VOID hifEnableObjectHotkeys(BOOL fEnable)
{
    // BOOL    brc = FALSE;
    HOOKCONFIG  HookConfig;
    // ULONG       cb = sizeof(HookConfig);
    hifLoadHookConfig(&HookConfig);
    HookConfig.__fGlobalHotkeys = fEnable;
    // write back to OS2.INI and notify hook
    hifHookConfigChanged(&HookConfig);
}

#endif

/*
 *@@ hifQueryObjectHotkeys:
 *      this returns the array of GLOBALHOTKEY structures
 *      which define _all_ global object hotkeys which are
 *      currently defined.
 *
 *      If _any_ hotkeys exist, a pointer is returned which
 *      points to an array of GLOBALHOTKEY structures, and
 *      *pcHotkeys is set to the number of items in that
 *      array (_not_ the array size!).
 *
 *      If no hotkeys exist, NULL is returned, and *pcHotkeys
 *      is left alone.
 *
 *      Use hifFreeObjectHotkeys to free the hotkeys array.
 *
 *@@added V0.9.0 [umoeller]
 */

PVOID hifQueryObjectHotkeys(PULONG pcHotkeys)   // out: hotkey count (req.)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cbHotkeys = 0;

    if (pHotkeys = (PGLOBALHOTKEY)prfhQueryProfileData(HINI_USER,
                                                       INIAPP_XWPHOOK,
                                                       INIKEY_HOOK_HOTKEYS,
                                                       &cbHotkeys))
        // found: calc no. of items in array
        *pcHotkeys = cbHotkeys / sizeof(GLOBALHOTKEY);

    return (pHotkeys);
}

/*
 *@@ hifFreeObjectHotkeys:
 *      this frees the memory allocated with the return
 *      value of hifQueryObjectHotkeys.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID hifFreeObjectHotkeys(PVOID pvHotkeys)
{
    if (pvHotkeys)
        free(pvHotkeys);
}

/*
 *@@ hifSetObjectHotkeys:
 *      this defines a new set of global object hotkeys
 *      and notifies the XWorkplace daemon (XWPDAEMON.EXE)
 *      of the change, which in turn updates the data of
 *      the XWorkplace hook (XWPHOOK.DLL).
 *
 *      Note that this function redefines all object
 *      hotkeys at once, since the hook is too dumb to
 *      handle single hotkey changes by itself.
 *
 *      Use XFldObject::xwpSetObjectHotkey to change
 *      the hotkey for a single object, which prepares a
 *      new hotkey list and calls this function in turn.
 *
 *@@added V0.9.0 [umoeller]
 */

BOOL hifSetObjectHotkeys(PVOID pvHotkeys,   // in: ptr to array of GLOBALHOTKEY structs
                         ULONG cHotkeys)    // in: no. of items in that array (_not_ array item size!)
{
    BOOL brc = FALSE;

    brc = PrfWriteProfileData(HINI_USER,
                              INIAPP_XWPHOOK,
                              INIKEY_HOOK_HOTKEYS,
                              pvHotkeys,
                              cHotkeys * sizeof(GLOBALHOTKEY));
    if (brc)
    {
        // notify daemon, which in turn notifies the hook
        brc = krnPostDaemonMsg(XDM_HOTKEYSCHANGED,
                               0, 0);
    }

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Function keys interface                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ hifQueryFunctionKeys:
 *      returns a pointer to the shared memory block
 *      containing all current function keys definitions.
 *
 *      Use hifFreeFunctionKeys to free the array returned
 *      here.
 *
 *@@added V0.9.3 (2000-04-19) [umoeller]
 */

PFUNCTIONKEY hifQueryFunctionKeys(PULONG pcFunctionKeys)    // out: function key count (not array size!)
{
    ULONG   cbFunctionKeys = 0;
    PFUNCTIONKEY paFunctionKeys
        = (PFUNCTIONKEY)prfhQueryProfileData(HINI_USER,
                                             INIAPP_XWPHOOK,
                                             INIKEY_HOOK_FUNCTIONKEYS,
                                             &cbFunctionKeys);
    if (paFunctionKeys)
        if (pcFunctionKeys)
            *pcFunctionKeys = cbFunctionKeys / sizeof(FUNCTIONKEY);

    return (paFunctionKeys);
}

/*
 *@@ hifFreeFunctionKeys:
 *      frees an array returned by hifQueryFunctionKeys.
 *
 *@@added V0.9.3 (2000-04-19) [umoeller]
 */

BOOL hifFreeFunctionKeys(PFUNCTIONKEY paFunctionKeys)
{
    if (paFunctionKeys)
    {
        free(paFunctionKeys);
        return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ hifSetFunctionKeys:
 *      writes the specified array of function keys
 *      back to OS2.INI and notifies the hook/daemon
 *      of the change.
 *
 *@@added V0.9.3 (2000-04-19) [umoeller]
 */

BOOL hifSetFunctionKeys(PFUNCTIONKEY paFunctionKeys, // in: function keys array
                        ULONG cFunctionKeys)    // in: array item count (NOT array size!)
{
    BOOL brc = FALSE;

    brc = PrfWriteProfileData(HINI_USER,
                              INIAPP_XWPHOOK,
                              INIKEY_HOOK_FUNCTIONKEYS,
                              (cFunctionKeys)
                                    ? paFunctionKeys
                                    : NULL,     // if none are present
                              cFunctionKeys * sizeof(FUNCTIONKEY));

    if (brc)
    {
        // notify daemon, which in turn notifies the hook
        brc = krnPostDaemonMsg(XDM_HOTKEYSCHANGED,
                               0, 0);
    }

    return (brc);
}

/*
 *@@ hifAppendFunctionKey:
 *      appends a new function key to the end of
 *      the function keys list.
 *
 *      Calls hifSetFunctionKeys in turn.
 *
 *@@added V0.9.3 (2000-04-19) [umoeller]
 */

BOOL hifAppendFunctionKey(PFUNCTIONKEY pNewKey)
{
    BOOL    brc = FALSE;
    ULONG   cbFunctionKeys = 0;
    PFUNCTIONKEY paFunctionKeys
        = (PFUNCTIONKEY) prfhQueryProfileData(HINI_USER,
                                              INIAPP_XWPHOOK,
                                              INIKEY_HOOK_FUNCTIONKEYS,
                                              &cbFunctionKeys),
            paNewKeys = NULL;
    ULONG   cKeys = 0;

    if (paFunctionKeys)
        cKeys = cbFunctionKeys / sizeof(FUNCTIONKEY);

    paNewKeys = malloc(sizeof(FUNCTIONKEY) * (cKeys + 1));
    if (paFunctionKeys)
        // items existed already:
        memcpy(paNewKeys, paFunctionKeys, sizeof(FUNCTIONKEY) * cKeys);
    // append new item
    memcpy(&paNewKeys[cKeys], pNewKey, sizeof(FUNCTIONKEY));

    brc = hifSetFunctionKeys(paNewKeys,
                             cKeys + 1);

    if (paFunctionKeys)
        free(paFunctionKeys);
    if (paNewKeys)
        free(paNewKeys);

    return (brc);
}

/*
 *@@ hifFindFunctionKey:
 *      searches the specified array for the array
 *      item with the specified scan code. If it's
 *      found, the address of the array item is
 *      returned; otherwise we return NULL.
 *
 *@@added V0.9.3 (2000-04-19) [umoeller]
 */

PFUNCTIONKEY hifFindFunctionKey(PFUNCTIONKEY paFunctionKeys, // in: array of function keys
                                ULONG cFunctionKeys,    // in: array item count (NOT array size)
                                UCHAR ucScanCode)       // in: scan code to search
{
    ULONG   ul = 0;

    for (ul = 0;
         ul < cFunctionKeys;
         ul++)
    {
        if (paFunctionKeys[ul].ucScanCode == ucScanCode)
            return (&paFunctionKeys[ul]);
    }

    return (NULL);
}

/*
 *@@ hifDeleteFunctionKey:
 *      deletes the function key with the specified
 *      index from the given function keys array.
 *      The items are copied within the array, but
 *      the array is not re-allocated, only the
 *      array item count is changed.
 *
 *      Calls hifSetFunctionKeys in turn.
 *
 *@@added V0.9.3 (2000-04-20) [umoeller]
 */

BOOL hifDeleteFunctionKey(PFUNCTIONKEY paFunctionKeys,// in: array of function keys
                          PULONG pcFunctionKeys,    // in/out: array item count (NOT array size)
                          ULONG ulDelete)   // in: index of function key to delete
{
    BOOL brc = FALSE;

    if (ulDelete < *pcFunctionKeys)
    {
        if (ulDelete < (*pcFunctionKeys - 1))
            // item to delete is not the last item:
            // copy following items over the item to
            // be deleted
            memcpy(&paFunctionKeys[ulDelete],        // target: item to be deleted
                   &paFunctionKeys[ulDelete + 1],    // source: following items
                   (*pcFunctionKeys - ulDelete - 1)
                        * sizeof(FUNCTIONKEY));
                        // -- if we have 2 items and item 0 is deleted,
                        //    copy 1 item
                        // -- if we have 5 items and item 2 (third) is deleted,
                        //    copy 2 items (item 3 and 4)

        // decrease count
        (*pcFunctionKeys)--;

        brc = hifSetFunctionKeys(paFunctionKeys,
                                 *pcFunctionKeys);
    }

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPKeyboard notebook callbacks (notebook.c)                    *
 *                                                                  *
 ********************************************************************/

/*
 *@@ hifCollectHotkeys:
 *      implementation for FIM_INSERTHOTKEYS.
 *      This runs on the File thread.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-19) [umoeller]: moved this here from xthreads.c
 *@@changed V0.9.3 (2000-04-19) [umoeller]: added XWP function keys support
 */

VOID hifCollectHotkeys(MPARAM mp1,  // in: HWND hwndCnr
                       MPARAM mp2)  // in: PBOOL pfBusy
{
    HWND            hwndCnr = (HWND)mp1;
    PBOOL           pfBusy = (PBOOL)mp2;
    ULONG           cHotkeys;       // set below
    PGLOBALHOTKEY   pHotkeys;

    if ((hwndCnr) && (pfBusy))
    {
        *pfBusy = TRUE;
        pHotkeys = hifQueryObjectHotkeys(&cHotkeys);

        #ifdef DEBUG_KEYS
            _Pmpf(("hifKeybdHotkeysInitPage: got %d hotkeys", cHotkeys));
        #endif

        cnrhRemoveAll(hwndCnr);

        if (pHotkeys)
        {
            ULONG cFuncKeys = 0;
            PFUNCTIONKEY paFuncKeys = hifQueryFunctionKeys(&cFuncKeys);

            ULONG       ulCount = 0;
            PHOTKEYRECORD preccFirst
                        = (PHOTKEYRECORD)cnrhAllocRecords(hwndCnr,
                                                          sizeof(HOTKEYRECORD),
                                                          cHotkeys);
            PHOTKEYRECORD preccThis = preccFirst;
            PGLOBALHOTKEY pHotkeyThis = pHotkeys;

            while (ulCount < cHotkeys)
            {
                PFUNCTIONKEY pFuncKey = 0;

                #ifdef DEBUG_KEYS
                    _Pmpf(("  %d: Getting hotkey for 0x%lX", ulCount, pHotkeyThis->ulHandle));
                #endif

                preccThis->ulIndex = ulCount;

                // copy struct
                memcpy(&preccThis->Hotkey, pHotkeyThis, sizeof(GLOBALHOTKEY));

                // object handle
                sprintf(preccThis->szHandle, "0x%lX", pHotkeyThis->ulHandle);
                preccThis->pszHandle = preccThis->szHandle;

                // describe hotkey
                // check if maybe this is a function key
                // V0.9.3 (2000-04-19) [umoeller]
                if (paFuncKeys)
                    pFuncKey = hifFindFunctionKey(paFuncKeys,
                                                  cFuncKeys,
                                                  pHotkeyThis->ucScanCode);
                if (pFuncKey)
                {
                    // it's a function key:
                    sprintf(preccThis->szHotkey,
                            "\"%s\"",
                            pFuncKey->szDescription);
                }
                else
                    cmnDescribeKey(preccThis->szHotkey,
                                   pHotkeyThis->usFlags,
                                   pHotkeyThis->usKeyCode);
                preccThis->pszHotkey = preccThis->szHotkey;

                // get object for hotkey
                preccThis->pObject = _wpclsQueryObject(_WPObject,
                                                       pHotkeyThis->ulHandle);
                if (preccThis->pObject)
                {
                    WPFolder *pFolder = _wpQueryFolder(preccThis->pObject);

                    preccThis->recc.pszIcon = _wpQueryTitle(preccThis->pObject);
                    preccThis->recc.hptrMiniIcon = _wpQueryIcon(preccThis->pObject);
                    if (pFolder)
                        if (_wpQueryFilename(pFolder, preccThis->szFolderPath, TRUE))
                            preccThis->pszFolderPath = preccThis->szFolderPath;
                }
                else
                    preccThis->recc.pszIcon = cmnGetString(ID_XSSI_INVALID_OBJECT);

                preccThis = (PHOTKEYRECORD)(preccThis->recc.preccNextRecord);
                ulCount++;
                pHotkeyThis++;
            }

            cnrhInsertRecords(hwndCnr,
                              NULL,         // parent
                              (PRECORDCORE)preccFirst,
                              TRUE, // invalidate
                              NULL,         // text
                              CRA_RECORDREADONLY,
                              cHotkeys);

            // clean up
            hifFreeObjectHotkeys(pHotkeys);
            hifFreeFunctionKeys(paFuncKeys);
        }

        *pfBusy = FALSE;
    }
}

/*
 *@@ HOTKEYSPAGEDATA:
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

typedef struct _HOTKEYSPAGEDATA
{
    HWND        hmenuPopup;
} HOTKEYSPAGEDATA, *PHOTKEYSPAGEDATA;

/*
 *@@ hifKeybdHotkeysInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Object hotkeys" page in the "Keyboard" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.4 (2000-06-13) [umoeller]: group title was missing; fixed
 *@@changed V0.9.6 (2000-10-16) [umoeller]: fixed excessive menu creation
 *@@changed V0.9.14 (2001-07-07) [pr]: fixed container font
 */

VOID hifKeybdHotkeysInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                             ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        PHOTKEYSPAGEDATA pPageData = 0;
        XFIELDINFO      xfi[7];
        PFIELDINFO      pfi = NULL;
        int             i = 0;
        HWND            hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        SWP             swpCnr;

        // set group title V0.9.4 (2000-06-13) [umoeller]
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_OBJECTHOTKEYSPAGE)) ; // pszObjectHotkeysPage

        // recreate container at the same position as
        // the container in the dlg resources;
        // this is required because of the undocumented
        // CCS_MINICONS style to support mini-icons in
        // Details view (duh...)
        WinQueryWindowPos(hwndCnr, &swpCnr);
        WinDestroyWindow(hwndCnr);
        hwndCnr = WinCreateWindow(pcnbp->hwndDlgPage,        // parent
                                  WC_CONTAINER,
                                  "",
                                  CCS_MINIICONS | CCS_READONLY | CCS_SINGLESEL,
                                  swpCnr.x, swpCnr.y, swpCnr.cx, swpCnr.cy,
                                  pcnbp->hwndDlgPage,        // owner
                                  HWND_TOP,
                                  ID_XFDI_CNR_CNR,
                                  NULL, NULL);
        if (cmnQuerySetting(sfUse8HelvFont))  // V0.9.14
            winhSetControlsFont(pcnbp->hwndDlgPage,
                                ID_XFDI_CNR_CNR,
                                ID_XFDI_CNR_CNR,
                                "8.Helv");
        WinShowWindow(hwndCnr, TRUE);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, ulIndex);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_RIGHT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, hptrMiniIcon);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_BITMAPORICON;
        xfi[i++].ulOrientation = CFA_CENTER;

        xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, pszIcon);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_HOTKEY_TITLE);  // pszHotkeyTitle
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, pszFolderPath);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_HOTKEY_FOLDER);  // pszHotkeyFolder
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, pszHandle);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_HOTKEY_HANDLE);  // pszHotkeyHandle
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, pszHotkey);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_HOTKEY_HOTKEY);  // pszHotkeyHotkey
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                &xfi[0],
                                i,             // array item count
                                TRUE,          // draw lines
                                4);            // return column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CV_MINI | CA_DETAILSVIEWTITLES | CA_DRAWICON);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(250);
        } END_CNRINFO(hwndCnr);

        pPageData = pcnbp->pUser = malloc(sizeof(HOTKEYSPAGEDATA));
        memset(pPageData, 0, sizeof(HOTKEYSPAGEDATA));

        pPageData->hmenuPopup = WinLoadMenu(HWND_OBJECT,
                                            cmnQueryNLSModuleHandle(FALSE),
                                            ID_XSM_HOTKEYS_SEL);
    }

    if (flFlags & CBI_SET)
    {
        // insert hotkeys into container: this takes
        // several seconds, so have this done by the
        // file thread
        xthrPostFileMsg(FIM_INSERTHOTKEYS,
                        (MPARAM)WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR),
                        (MPARAM)&pcnbp->fShowWaitPointer);
    }

    if (flFlags & CBI_DESTROY)
    {
        PHOTKEYSPAGEDATA pPageData = (PHOTKEYSPAGEDATA)pcnbp->pUser;
        WinDestroyWindow(pPageData->hmenuPopup);
    }
}

/*
 *@@ hifKeybdHotkeysItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Object hotkeys" page in the "Keyboard" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.1 (99-12-06): fixed context-menu-on-whitespace bug
 */

MRESULT hifKeybdHotkeysItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                   ULONG ulItemID, USHORT usNotifyCode,
                                   ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = (MRESULT)0;

    switch (ulItemID)
    {
        case ID_XFDI_CNR_CNR:
            switch (usNotifyCode)
            {
                /*
                 * CN_CONTEXTMENU:
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu = NULLHANDLE; // fixed V0.9.1 (99-12-06)

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        PHOTKEYSPAGEDATA pPageData = (PHOTKEYSPAGEDATA)pcnbp->pUser;
                        // popup menu on container recc:
                        hPopupMenu = pPageData->hmenuPopup;
                    }

                    if (hPopupMenu)
                        cnrhShowContextMenu(pcnbp->hwndControl,     // cnr
                                            (PRECORDCORE)pcnbp->preccSource,
                                            hPopupMenu,
                                            pcnbp->hwndDlgPage);    // owner
                }
                break;
            }
        break;

        /*
         * ID_XSMI_HOTKEYS_PROPERTIES:
         *      "object properties" context menu item
         */

        case ID_XSMI_HOTKEYS_PROPERTIES:
        {
            PHOTKEYRECORD precc = (PHOTKEYRECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (precc)
                if (precc->pObject)
                    _wpViewObject(precc->pObject,
                                  NULLHANDLE,   // hwndCnr (?!?)
                                  OPEN_SETTINGS,
                                  0);
        }
        break;

        /*
         * ID_XSMI_HOTKEYS_OPENFOLDER:
         *      "open folder" context menu item
         */

        case ID_XSMI_HOTKEYS_OPENFOLDER:
        {
            PHOTKEYRECORD precc = (PHOTKEYRECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (precc)
                if (precc->pObject)
                {
                    WPFolder *pFolder = _wpQueryFolder(precc->pObject);
                    if (pFolder)
                        _wpViewObject(pFolder,
                                      NULLHANDLE,   // hwndCnr (?!?)
                                      OPEN_DEFAULT,
                                      0);
                }
        }
        break;

        /*
         * ID_XSMI_HOTKEYS_REMOVE:
         *      "remove hotkey" context menu item
         */

        case ID_XSMI_HOTKEYS_REMOVE:
        {
            PHOTKEYRECORD precc = (PHOTKEYRECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (precc)
            {
                // string replacements
                PSZ apsz[2] = {
                                    precc->szHotkey,        // %1: hotkey
                                    precc->recc.pszIcon     // %2: object title
                              };
                if (cmnMessageBoxMsgExt(pcnbp->hwndFrame, // pcnbp->hwndPage,
                                        148,       // "XWorkplace Setup
                                        apsz, 2,   // two string replacements
                                        162,       // Sure hotkey?
                                        MB_YESNO)
                        == MBID_YES)
                    _xwpclsRemoveObjectHotkey(_XFldObject,
                                              precc->Hotkey.ulHandle);
                            // this updates the notebook in turn
            }
        }
        break;
    }

    return (mrc);
}

/*
 *@@ EDITFUNCTIONKEYDATA:
 *      structure stored in QWL_USER of
 *      "edit function key" dlg (hif_fnwpEditFunctionKeyDlg).
 *
 *@@added V0.9.3 (2000-04-20) [umoeller]
 */

typedef struct _EDITFUNCTIONKEYDATA
{
    WPObject        *somSelf;
    UCHAR           ucScanCode;
} EDITFUNCTIONKEYDATA, *PEDITFUNCTIONKEYDATA;

/*
 *@@ hif_fnwpSubclassedFuncKeyEF:
 *      subclassed entry field procedure which
 *      displays the hardware scan code for
 *      every WM_CHAR.
 *
 *@@added V0.9.3 (2000-04-18) [umoeller]
 */

MRESULT EXPENTRY hif_fnwpSubclassedFuncKeyEF(HWND hwndEdit,
                                             ULONG msg,
                                             MPARAM mp1,
                                             MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CHAR:
        {
            /*
                  #define KC_CHAR                    0x0001
                  #define KC_VIRTUALKEY              0x0002
                  #define KC_SCANCODE                0x0004
                  #define KC_SHIFT                   0x0008
                  #define KC_CTRL                    0x0010
                  #define KC_ALT                     0x0020
                  #define KC_KEYUP                   0x0040
                  #define KC_PREVDOWN                0x0080
                  #define KC_LONEKEY                 0x0100
                  #define KC_DEADKEY                 0x0200
                  #define KC_COMPOSITE               0x0400
                  #define KC_INVALIDCOMP             0x0800
            */

            /*
                Examples:           usFlags  usKeyCode
                    F3                02       22
                    Ctrl+F4           12       23
                    Ctrl+Shift+F4     1a       23
                    Ctrl              12       0a
                    Alt               22       0b
                    Shift             0a       09
            */

            USHORT usFlags    = SHORT1FROMMP(mp1);
            UCHAR  ucScanCode = CHAR4FROMMP(mp1);
            // USHORT usch       = SHORT1FROMMP(mp2);
            USHORT usvk       = SHORT2FROMMP(mp2);

            if (
                    // filter out important virtual keys
                    // which should never be intercepted
                       (    ((usFlags & KC_VIRTUALKEY) == 0)
                         || (   (usvk != VK_ESC)
                             && (usvk != VK_ENTER)
                             && (usvk != VK_TAB)
                            )
                       )
               )
            {

                // process only key-down messages
                if  ((usFlags & KC_KEYUP) == 0)
                {
                    PEDITFUNCTIONKEYDATA pefkd
                        = (PEDITFUNCTIONKEYDATA)WinQueryWindowPtr(WinQueryWindow(hwndEdit,
                                                                                 QW_PARENT),
                                                                  QWL_USER);

                    CHAR    szDescription[100];
                    sprintf(szDescription, "0x%lX (%d)", ucScanCode, ucScanCode);
                    WinSetWindowText(hwndEdit, szDescription);

                    // store scan code in QWL_USER of parent window
                    // so the owner of the dialog can retrieve it
                    pefkd->ucScanCode = ucScanCode;
                }

                mrc = (MPARAM)TRUE; // WM_CHAR processed flag;
            }
            else
                mrc = WinSendMsg(WinQueryWindow(hwndEdit, QW_PARENT),
                                 msg, mp1, mp2);
        }
        break;

        default:
            mrc = G_pfnwpEntryFieldOrig(hwndEdit, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ hif_fnwpEditFunctionKeyDlg:
 *      window procedure for "edit function key" dlg.
 *
 *@@added V0.9.3 (2000-04-18) [umoeller]
 *@@changed V0.9.4 (2000-06-12) [umoeller]: help page was wrong; fixed
 */

MRESULT EXPENTRY hif_fnwpEditFunctionKeyDlg(HWND hwndDlg,
                                            ULONG msg,
                                            MPARAM mp1,
                                            MPARAM mp2)
{
    MRESULT mrc = 0;

    PEDITFUNCTIONKEYDATA pefkd
        = (PEDITFUNCTIONKEYDATA)WinQueryWindowPtr(hwndDlg,
                                                  QWL_USER);

    switch (msg)
    {
        case WM_HELP:
            cmnDisplayHelp(pefkd->somSelf,
                           ID_XSH_SETTINGS_FUNCTIONKEYS + 1);
        break;

        case WM_DESTROY:
            free(pefkd);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ AddFuncKeyRecord:
 *
 *@@added V0.9.3 (2000-04-19) [umoeller]
 */

VOID AddFuncKeyRecord(HWND hwndCnr,             // in: cnr to create record in
                      PFUNCTIONKEY pFuncKey,    // in: new function key definition (one item)
                      ULONG ulIndex)            // in: index of item in FUNCTIONKEY array (must match!)
{
    PFUNCTIONKEYRECORD precc
        = (PFUNCTIONKEYRECORD)cnrhAllocRecords(hwndCnr,
                                               sizeof(FUNCTIONKEYRECORD),
                                               1);
    if (precc)
    {
        memcpy(&precc->FuncKey, pFuncKey, sizeof(FUNCTIONKEY));
        precc->pszDescription = precc->FuncKey.szDescription;
        sprintf(precc->szScanCode,
                "0x%lX (%d)",
                pFuncKey->ucScanCode,
                pFuncKey->ucScanCode);
        precc->pszScanCode = precc->szScanCode;

        precc->ulIndex = ulIndex;

        if (pFuncKey->fModifier)
            precc->pszModifier = "X";

        cnrhInsertRecords(hwndCnr,
                          NULL,
                          (PRECORDCORE)precc,
                          TRUE,
                          NULL,
                          CRA_RECORDREADONLY,
                          1);
    }
}

/*
 *@@ FUNCKEYSPAGEDATA:
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

typedef struct _FUNCKEYSPAGEDATA
{
    HWND        hmenuPopupItem,
                hmenuPopupWhitespace;
} FUNCKEYSPAGEDATA, *PFUNCKEYSPAGEDATA;

/*
 *@@ hifKeybdFunctionKeysInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Function keys" page in the "Keyboard" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.3 (2000-04-17) [umoeller]
 *@@changed V0.9.4 (2000-06-13) [umoeller]: group title was missing; fixed
 *@@changed V0.9.6 (2000-10-16) [umoeller]: fixed excessive menu creation
 *@@changed V0.9.6 (2000-11-12) [umoeller]: fixed memory leak
 *@@changed V0.9.12 (2001-05-19) [umoeller]: removed "Modifier" column for now
 */

VOID hifKeybdFunctionKeysInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                                  ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        PFUNCKEYSPAGEDATA pPageData = 0;

        XFIELDINFO      xfi[4];
        PFIELDINFO      pfi = NULL;
        int             i = 0;
        HWND            hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        // set group title V0.9.4 (2000-06-13) [umoeller]
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_FUNCTIONKEYSPAGE)) ; // pszFunctionKeysPage

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(FUNCTIONKEYRECORD, ulIndex);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_RIGHT;

        xfi[i].ulFieldOffset = FIELDOFFSET(FUNCTIONKEYRECORD, pszDescription);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_KEYDESCRIPTION);  // pszKeyDescription
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(FUNCTIONKEYRECORD, pszScanCode);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_SCANCODE);  // "Hardware scan code"; // pszScanCode
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        /* xfi[i].ulFieldOffset = FIELDOFFSET(FUNCTIONKEYRECORD, pszModifier);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_MODIFIER);  // "Modifier"; // pszModifier
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_CENTER; */
                // removed this column V0.9.12 (2001-05-19) [umoeller]

        pfi = cnrhSetFieldInfos(hwndCnr,
                                &xfi[0],
                                i,             // array item count
                                TRUE,          // draw lines
                                1);            // split bar after this column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CV_MINI | CA_DETAILSVIEWTITLES | CA_DRAWICON);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(250);
        } END_CNRINFO(hwndCnr);

        pPageData = pcnbp->pUser = malloc(sizeof(FUNCKEYSPAGEDATA));

        pPageData->hmenuPopupItem
            = WinLoadMenu(HWND_OBJECT,
                          cmnQueryNLSModuleHandle(FALSE),
                          ID_XSM_FUNCTIONKEYS_SEL);
        pPageData->hmenuPopupWhitespace
            = WinLoadMenu(HWND_OBJECT,
                          cmnQueryNLSModuleHandle(FALSE),
                          ID_XSM_FUNCTIONKEYS_NOSEL);

    }

    if (flFlags & CBI_SET)
    {
        ULONG   cKeys = 0,
                ul = 0;
        PFUNCTIONKEY paFuncKeys = hifQueryFunctionKeys(&cKeys);
        HWND    hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        cnrhRemoveAll(hwndCnr);

        for (ul = 0;
             ul < cKeys;
             ul++)
        {
            AddFuncKeyRecord(hwndCnr,
                             &paFuncKeys[ul],
                             ul);
        }

        hifFreeFunctionKeys(paFuncKeys); // V0.9.6 (2000-11-12) [umoeller]
    }

    if (flFlags & CBI_DESTROY)
    {
        PFUNCKEYSPAGEDATA pPageData = (PFUNCKEYSPAGEDATA)pcnbp->pUser;
        WinDestroyWindow(pPageData->hmenuPopupItem);
        WinDestroyWindow(pPageData->hmenuPopupWhitespace);
    }
}

/*
 *@@ hifKeybdFunctionKeysItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Function keys" page in the "Keyboard" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.3 (2000-04-17) [umoeller]
 *@@changed V0.9.12 (2001-05-17) [umoeller]: removed "modifier" for now
 */

MRESULT hifKeybdFunctionKeysItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG ulItemID, USHORT usNotifyCode,
                                        ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = (MRESULT)0;

    switch (ulItemID)
    {
        case ID_XFDI_CNR_CNR:

            switch (usNotifyCode)
            {
                /*
                 * CN_CONTEXTMENU:
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    PFUNCKEYSPAGEDATA pPageData = (PFUNCKEYSPAGEDATA)pcnbp->pUser;
                    HWND    hPopupMenu = NULLHANDLE; // fixed V0.9.1 (99-12-06)

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        // popup menu on container recc:
                        hPopupMenu = pPageData->hmenuPopupItem;
                    }
                    else
                        // popup menu on whitespace:
                        hPopupMenu = pPageData->hmenuPopupWhitespace;

                    if (hPopupMenu)
                        cnrhShowContextMenu(pcnbp->hwndControl,     // cnr
                                            (PRECORDCORE)pcnbp->preccSource,
                                            hPopupMenu,
                                            pcnbp->hwndDlgPage);    // owner
                }
                break;
            }
        break;

        /*
         * ID_XSMI_FUNCK_NEW:
         *      "New" popup menu item on cnr whitespace
         *
         * ID_XSMI_FUNCK_EDIT:
         *      "Edit" popup menu item on record core
         */

        case ID_XSMI_FUNCK_NEW:
        case ID_XSMI_FUNCK_EDIT:
        {
            // in both cases, we use the same "Edit" dialog
            HWND hwndEditDlg = WinLoadDlg(HWND_DESKTOP,
                                          pcnbp->hwndDlgPage,
                                          hif_fnwpEditFunctionKeyDlg,
                                          cmnQueryNLSModuleHandle(FALSE),
                                          ID_XSD_KEYB_EDITFUNCTIONKEY,
                                          NULL);
            if (hwndEditDlg)
            {
                PEDITFUNCTIONKEYDATA pefkd
                    = (PEDITFUNCTIONKEYDATA)malloc(sizeof(EDITFUNCTIONKEYDATA));

                HWND    hwndEntryField = WinWindowFromID(hwndEditDlg,
                                                         ID_XSDI_FUNCK_SCANCODE_EF);

                // store EDITFUNCTIONKEYDATA
                pefkd->somSelf = pcnbp->somSelf;
                WinSetWindowPtr(hwndEditDlg,
                                QWL_USER,
                                pefkd);

                // subclass entry field
                G_pfnwpEntryFieldOrig = WinSubclassWindow(hwndEntryField,
                                                          hif_fnwpSubclassedFuncKeyEF);

                if (ulItemID == ID_XSMI_FUNCK_EDIT)
                {
                    // if we have "edit" (not "new"), set the dialog items
                    PFUNCTIONKEYRECORD precc = (PFUNCTIONKEYRECORD)pcnbp->preccSource;
                    if (precc)
                    {
                        WinSetWindowText(WinWindowFromID(hwndEditDlg,
                                                         ID_XSDI_FUNCK_DESCRIPTION_EF),
                                         precc->FuncKey.szDescription);
                        WinSetWindowText(hwndEntryField,
                                         precc->szScanCode);
                        // winhSetDlgItemChecked(hwndEditDlg, ID_XSDI_FUNCK_MODIFIER,
                           //                    (precc->FuncKey.fModifier != 0));
                           // V0.9.12 (2001-05-17) [umoeller]
                    }
                }

                winhCenterWindow(hwndEditDlg);
                cmnSetControlsFont(hwndEditDlg, 0, 5000);
                // go!!
                if (WinProcessDlg(hwndEditDlg) == DID_OK)
                {
                    PFUNCTIONKEY paKeys = NULL;
                    ULONG   cKeys = 0;

                    paKeys = hifQueryFunctionKeys(&cKeys);

                    if (ulItemID == ID_XSMI_FUNCK_EDIT)
                    {
                        // if we have "edit" (not "new"), use the
                        // existing record
                        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage,
                                                       ID_XFDI_CNR_CNR);

                        PFUNCTIONKEYRECORD precc = (PFUNCTIONKEYRECORD)pcnbp->preccSource;

                        // update FUNCTIONKEY structure in record core
                        WinQueryWindowText(WinWindowFromID(hwndEditDlg,
                                                           ID_XSDI_FUNCK_DESCRIPTION_EF),
                                           sizeof(precc->FuncKey.szDescription),
                                           precc->FuncKey.szDescription);
                        precc->FuncKey.ucScanCode
                            = pefkd->ucScanCode;
                        precc->FuncKey.fModifier = FALSE;
                           //  = winhIsDlgItemChecked(hwndEditDlg, ID_XSDI_FUNCK_MODIFIER);
                           // V0.9.12 (2001-05-17) [umoeller]

                        // update record
                        sprintf(precc->szScanCode,
                                "0x%lX (%d)",
                                precc->FuncKey.ucScanCode,
                                precc->FuncKey.ucScanCode);
                        precc->pszModifier = (precc->FuncKey.fModifier)
                                                    ? "X"
                                                    : NULL;

                        // update container
                        WinSendMsg(hwndCnr,
                                   CM_INVALIDATERECORD,
                                   (MPARAM)&precc,
                                   MPFROM2SHORT(1,
                                                CMA_TEXTCHANGED));
                        // compose new list and store it
                        if (paKeys)
                        {
                            LONG   lReccIndex = cnrhQueryRecordIndex(hwndCnr,
                                                                     (PRECORDCORE)precc);
                            if (lReccIndex != -1)
                            {
                                // overwrite corresponding entry in
                                // FUNCTIONKEY array
                                memcpy(&paKeys[lReccIndex],
                                       &precc->FuncKey,
                                       sizeof(FUNCTIONKEY));
                                // and store new array
                                hifSetFunctionKeys(paKeys, cKeys);
                            }
                        }
                    }
                    else
                    {
                        // "New" mode:
                        // add new record
                        FUNCTIONKEY FuncKey;
                        WinQueryWindowText(WinWindowFromID(hwndEditDlg,
                                                           ID_XSDI_FUNCK_DESCRIPTION_EF),
                                           sizeof(FuncKey.szDescription),
                                           FuncKey.szDescription);
                        FuncKey.ucScanCode = pefkd->ucScanCode;
                        FuncKey.fModifier = FALSE;
                            // = winhIsDlgItemChecked(hwndEditDlg, ID_XSDI_FUNCK_MODIFIER);
                            // V0.9.12 (2001-05-17) [umoeller]
                        hifAppendFunctionKey(&FuncKey);
                        AddFuncKeyRecord(WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR),
                                         &FuncKey,
                                         cKeys);        // new index == item count
                    }

                    if (paKeys)
                        hifFreeFunctionKeys(paKeys);

                } // end if (WinProcessDlg(hwndEditDlg) == DID_OK)

                WinDestroyWindow(hwndEditDlg);
            } // end if (hwndEditDlg)
        }
        break;

        /*
         * ID_XSMI_FUNCK_DELETE:
         *      "Delete" popup menu item on record core
         */

        case ID_XSMI_FUNCK_DELETE:
        {
            HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XFDI_CNR_CNR);

            PFUNCTIONKEYRECORD precc = (PFUNCTIONKEYRECORD)pcnbp->preccSource;

            LONG   lReccIndex = cnrhQueryRecordIndex(hwndCnr,
                                                     (PRECORDCORE)precc);
            if (lReccIndex != -1)
            {
                PFUNCTIONKEY paKeys = NULL;
                ULONG   cKeys = 0;

                paKeys = hifQueryFunctionKeys(&cKeys);
                if (paKeys)
                {
                    hifDeleteFunctionKey(paKeys, &cKeys, lReccIndex);
                    hifFreeFunctionKeys(paKeys);

                    WinSendMsg(hwndCnr,
                               CM_REMOVERECORD,
                               &precc,
                               MPFROM2SHORT(1, CMA_FREE | CMA_INVALIDATE));
                }
            }
        }
        break;

        default:
        break;
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XWPMouse "Mappings" notebook page
 *
 ********************************************************************/

/*
 *@@ hifMouseMappings2InitPage:
 *      notebook callback function (notebook.c) for the
 *      new second "Mappings" page in the "Mouse" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.1 (99-12-10) [umoeller]
 *@@changed V0.9.4 (2000-06-12) [umoeller]: added mb3 clicks
 *@@changed V0.9.9 (2001-03-20) [lafaix]: added MB3 click mappings
 */

VOID hifMouseMappings2InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                               ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage,
                                        ID_XSDI_MOUSE_MB3CLICK_DROP);
        if (pcnbp->pUser == 0)
        {
            // first call: create HOOKCONFIG
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser)
                hifLoadHookConfig(pcnbp->pUser);

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(HOOKCONFIG));
        }

        // set MB3 mappings combo
        WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_MB3_AUTOSCROLL)) ; // pszMB3AutoScroll
        WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_MB3_DBLCLICK)) ; // pszMB3DblClick
        WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_MB3_NOCONVERSION)) ; // pszMB3NoConversion
        WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_MB3_PUSHTOBOTTOM)) ; // pszMB3PushToBottom

        // set up sliders
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(9, 10), 6);
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(9, 10), 6);
    }

    if (flFlags & CBI_SET)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
        HWND hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage,
                                        ID_XSDI_MOUSE_MB3CLICK_DROP);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_CHORDWINLIST,
                              pdc->fChordWinList);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SYSMENUMB2,
                              pdc->fSysMenuMB2TitleBar);

        // mb3 clicks mappings
        if (pdc->fMB3AutoScroll)
            winhSetLboxSelectedItem(hwndDrop, 0, TRUE);
        else
        if (pdc->fMB3Click2MB1DblClk)
            winhSetLboxSelectedItem(hwndDrop, 1, TRUE);
        else
        if (pdc->fMB3Push2Bottom)
            winhSetLboxSelectedItem(hwndDrop, 3, TRUE);
        else
            winhSetLboxSelectedItem(hwndDrop, 2, TRUE);

        // mb3 scroll
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3SCROLL,
                              pdc->fMB3Scroll);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pdc->usMB3ScrollMin);

        if (pdc->usScrollMode == SM_AMPLIFIED)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMPLIFIED,
                                  TRUE);
        else
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3LINEWISE,
                                  TRUE);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pdc->sAmplification + 9);
            // 0 = 10%, 11 = 100%, 13 = 120%, ...
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3SCROLLREVERSE,
                              pdc->fMB3ScrollReverse);

    }

    if (flFlags & CBI_ENABLE)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
        BOOL        fEnableAmp = (   (pdc->fMB3Scroll)
                                  && (pdc->usScrollMode == SM_AMPLIFIED)
                                 );
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_TXT1,
                          pdc->fMB3Scroll);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_SLIDER,
                          pdc->fMB3Scroll);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_TXT2,
                          pdc->fMB3Scroll);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3LINEWISE,
                          pdc->fMB3Scroll);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMPLIFIED,
                          pdc->fMB3Scroll);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_TXT1,
                          fEnableAmp);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_SLIDER,
                          fEnableAmp);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_TXT2,
                          fEnableAmp);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3SCROLLREVERSE,
                          pdc->fMB3Scroll);
    }
}

/*
 *@@ hifMouseMappings2ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      new second "Mappings" page in the "Mouse" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.1 (99-12-10) [umoeller]
 *@@changed V0.9.4 (2000-06-12) [umoeller]: added mb3 clicks
 *@@changed V0.9.4 (2000-06-12) [umoeller]: fixed "default" and "undo" buttons
 *@@changed V0.9.9 (2001-03-15) [lafaix]: added MB3 click mappings
 *@@changed V0.9.9 (2001-03-25) [lafaix]: fixed "default" and "undo" behavior
 *@@changed V0.9.9 (2001-04-07) [pr]: fixed "default" and "undo" again
 */

MRESULT hifMouseMappings2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID, USHORT usNotifyCode,
                                     ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
    BOOL    fSave = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_MOUSE_CHORDWINLIST:
            hifLoadHookConfig(pdc);
            pdc->fChordWinList = ulExtra;
        break;

        case ID_XSDI_MOUSE_SYSMENUMB2:
            hifLoadHookConfig(pdc);
            pdc->fSysMenuMB2TitleBar = ulExtra;
        break;

        case ID_XSDI_MOUSE_MB3CLICK_DROP:
        {
            // new mappings selected from drop-down box:
            HWND hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage, ulItemID);
            LONG lIndex = winhQueryLboxSelectedItem(hwndDrop, LIT_FIRST);

            hifLoadHookConfig(pdc);
            pdc->fMB3AutoScroll = (lIndex == 0);
            pdc->fMB3Click2MB1DblClk = (lIndex == 1);
            pdc->fMB3Push2Bottom = (lIndex == 3);
        }
        break;

        case ID_XSDI_MOUSE_MB3SCROLL:
            hifLoadHookConfig(pdc);
            pdc->fMB3Scroll = ulExtra;
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_MB3PIXELS_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(
                                            pcnbp->hwndControl,
                                            SMA_INCREMENTVALUE);

            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_XSDI_MOUSE_MB3PIXELS_TXT2,
                               lSliderIndex + 1,
                               FALSE);      // unsigned

            hifLoadHookConfig(pdc);
            pdc->usMB3ScrollMin = lSliderIndex;
        }
        break;

        case ID_XSDI_MOUSE_MB3LINEWISE:
            hifLoadHookConfig(pdc);
            pdc->usScrollMode = SM_LINEWISE;
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_MB3AMPLIFIED:
            hifLoadHookConfig(pdc);
            pdc->usScrollMode = SM_AMPLIFIED;
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_MB3AMP_SLIDER:
        {
            CHAR    szText[20];
            LONG lSliderIndex = winhQuerySliderArmPosition(
                                            pcnbp->hwndControl,
                                            SMA_INCREMENTVALUE);

            sprintf(szText, "%d%%", 100 + ((lSliderIndex - 9) * 10) );
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_MB3AMP_TXT2,
                              szText);

            hifLoadHookConfig(pdc);
            pdc->sAmplification = lSliderIndex - 9;
        }
        break;

        case ID_XSDI_MOUSE_MB3SCROLLREVERSE:
            hifLoadHookConfig(pdc);
            pdc->fMB3ScrollReverse = ulExtra;
        break;

        /*
         * DID_DEFAULT:
         *
         *changed V0.9.9 (2001-03-25) [lafaix]: saving settings here
         */

        case DID_DEFAULT:
            hifLoadHookConfig(pdc);
            pdc->fChordWinList = 0;
            pdc->fSysMenuMB2TitleBar = 0;
            pdc->fMB3AutoScroll = 0; // V0.9.9 (2001-04-07) [pr]
            pdc->fMB3Click2MB1DblClk = 0;
            pdc->fMB3Scroll = 0;
            pdc->fMB3Push2Bottom = 0; // V0.9.9 (2001-04-07) [pr]
            pdc->usMB3ScrollMin = 0;
            pdc->usScrollMode = SM_LINEWISE; // 0
            pdc->sAmplification = 0;
            pdc->fMB3ScrollReverse = 0;

            // saving settings before initializing
            hifHookConfigChanged(pdc);
            fSave = FALSE;

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * DID_UNDO:
         *
         *changed V0.9.9 (2001-03-25) [lafaix]: saving settings here
         */

        case DID_UNDO:
            // restore data which was backed up in INIT callback
            if (pcnbp->pUser2)
            {
                PHOOKCONFIG pBackup = (PHOOKCONFIG)pcnbp->pUser2;
                pdc->fChordWinList = pBackup->fChordWinList;
                pdc->fSysMenuMB2TitleBar = pBackup->fSysMenuMB2TitleBar;
                pdc->fMB3AutoScroll = pBackup->fMB3AutoScroll;  // V0.9.9 (2001-04-07) [pr]
                pdc->fMB3Click2MB1DblClk = pBackup->fMB3Click2MB1DblClk;
                pdc->fMB3Push2Bottom = pBackup->fMB3Push2Bottom;  // V0.9.9 (2001-04-07) [pr]
                pdc->fMB3Scroll = pBackup->fMB3Scroll;
                pdc->usMB3ScrollMin = pBackup->usMB3ScrollMin;
                pdc->usScrollMode = pBackup->usScrollMode;
                pdc->sAmplification = pBackup->sAmplification;
                pdc->fMB3ScrollReverse = pBackup->fMB3ScrollReverse;

                hifHookConfigChanged(pdc);
                fSave = FALSE;
            }
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            fSave = FALSE;
    }

    if (fSave)
        hifHookConfigChanged(pdc);

    return (mrc);
}

/* ******************************************************************
 *
 *   XWPMouse "Movement 1" settings page
 *
 ********************************************************************/

#define STYLE_SLIDERS_WIDTH        150
#define STYLE_SLIDERS_HEIGHT        30
#define STYLE_SLIDERTEXT_WIDTH      30

SLDCDATA
#ifndef __NOSLIDINGFOCUS__
        SlidingFocusDelayCData =
             {
                     sizeof(SLDCDATA),
            // usScale1Increments:
                     31,          // scale 1 increments
                     0,         // scale 1 spacing
                     1,          // scale 2 increments
                     0           // scale 2 spacing
             },
#endif
        SlidingMenusDelayCData =
             {
                     sizeof(SLDCDATA),
            // usScale1Increments:
                     31,          // scale 1 increments
                     0,         // scale 1 spacing
                     1,          // scale 2 increments
                     0           // scale 2 spacing
             };

static CONTROLDEF
#ifndef __NOSLIDINGFOCUS__
    SlidingFocusGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_SLIDINGFOCUS_GRP,
                            -1,
                            -1),
    SlidingFocusCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_SLIDINGFOCUS,
                            -1,
                            -1),
    SlidingFocusDelayTxt1 = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_FOCUSDELAY_TXT1,
                            -1,
                            -1),
    SlidingFocusDelaySlider =
        {
                WC_SLIDER,
                NULL,
                WS_VISIBLE | WS_TABSTOP | WS_GROUP
                    | SLS_HORIZONTAL
                    | SLS_PRIMARYSCALE1
                    | SLS_BUTTONSRIGHT
                    | SLS_SNAPTOINCREMENT,
                ID_XSDI_MOUSE_FOCUSDELAY_SLIDER,
                CTL_COMMON_FONT,
                0,
                { STYLE_SLIDERS_WIDTH, STYLE_SLIDERS_HEIGHT },     // size
                5,               // spacing
                &SlidingFocusDelayCData
        },
    SlidingFocusDelayTxt2 = CONTROLDEF_TEXT(
                            "10000 ms",     // to be replaced
                            ID_XSDI_MOUSE_FOCUSDELAY_TXT2,
                            -1,
                            -1),
    BringToTopCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_BRING2TOP,
                            -1,
                            -1),
    IgnoreSeamlessCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_IGNORESEAMLESS,
                            -1,
                            -1),
    IgnoreDesktopCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_IGNOREDESKTOP,
                            -1,
                            -1),
    IgnorePageMageCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_IGNOREPAGEMAGE,
                            -1,
                            -1),
    IgnoreXCenterCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_IGNOREXCENTER,
                            -1,
                            -1),
#endif
    SlidingMenusGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_SLIDINGMENU_GRP,
                            -1,
                            -1),
    SlidingMenusCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_SLIDINGMENU,
                            -1,
                            -1),
    SlidingMenusDelayTxt1 = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_MENUDELAY_TXT1,
                            -1,
                            -1),
    SlidingMenusDelaySlider =
        {
                WC_SLIDER,
                NULL,
                WS_VISIBLE | WS_TABSTOP | WS_GROUP
                    | SLS_HORIZONTAL
                    | SLS_PRIMARYSCALE1
                    | SLS_BUTTONSRIGHT
                    | SLS_SNAPTOINCREMENT,
                ID_XSDI_MOUSE_MENUDELAY_SLIDER,
                CTL_COMMON_FONT,
                0,
                { STYLE_SLIDERS_WIDTH, STYLE_SLIDERS_HEIGHT },     // size
                5,               // spacing
                &SlidingMenusDelayCData
        },
    SlidingMenusDelayTxt2 = CONTROLDEF_TEXT(
                            "10000 ms",     // to be replaced
                            ID_XSDI_MOUSE_MENUDELAY_TXT2,
                            -1,
                            -1),
    CondCascadeCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_CONDCASCADE,
                            -1,
                            -1),
    MenuHiliteCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_MOUSE_MENUHILITE,
                            -1,
                            -1);

static const DLGHITEM dlgMovement1[] =
    {
        START_TABLE,            // root table, required
#ifndef __NOSLIDINGFOCUS__
            START_ROW(ROW_VALIGN_TOP),       // row 1 in the root table, required
                START_GROUP_TABLE(&SlidingFocusGroup),
                    START_ROW(0),
                        CONTROL_DEF(&SlidingFocusCB),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&SlidingFocusDelayTxt1),
                        CONTROL_DEF(&SlidingFocusDelaySlider),
                        CONTROL_DEF(&SlidingFocusDelayTxt2),
                    START_ROW(0),
                        CONTROL_DEF(&BringToTopCB),
                    START_ROW(0),
                        CONTROL_DEF(&IgnoreSeamlessCB),
                    START_ROW(0),
                        CONTROL_DEF(&IgnoreDesktopCB),
                    START_ROW(0),
                        CONTROL_DEF(&IgnorePageMageCB),
                    START_ROW(0),
                        CONTROL_DEF(&IgnoreXCenterCB),
                END_TABLE,
#endif
            START_ROW(0),
                START_GROUP_TABLE(&SlidingMenusGroup),
                    START_ROW(0),
                        CONTROL_DEF(&SlidingMenusCB),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&SlidingMenusDelayTxt1),
                        CONTROL_DEF(&SlidingMenusDelaySlider),
                        CONTROL_DEF(&SlidingMenusDelayTxt2),
                    START_ROW(0),
                        CONTROL_DEF(&CondCascadeCB),
                    START_ROW(0),
                        CONTROL_DEF(&MenuHiliteCB),
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // notebook.c
                CONTROL_DEF(&G_DefaultButton),      // notebook.c
                CONTROL_DEF(&G_HelpButton),         // notebook.c
        END_TABLE
    };

/*
 *@@ hifMouseMovementInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page in the "Mouse" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.6 (2000-10-27) [umoeller]: added optional NPSWPS-like submenu behavior
 *@@changed V0.9.7 (2000-12-08) [umoeller]: added "ignore XCenter"
 *@@changed V0.9.14 (2001-08-02) [lafaix]: moved the autohide stuff to movement page 2
 *@@changed V0.9.16 (2001-12-06) [umoeller]: now using dialog formatter
 */

VOID hifMouseMovementInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                              ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            // first call: create HOOKCONFIG structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser)
                hifLoadHookConfig(pcnbp->pUser);

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(HOOKCONFIG));

            // insert the controls using the dialog formatter
            // V0.9.16 (2001-12-06) [umoeller]
            ntbFormatPage(pcnbp->hwndDlgPage,
                          dlgMovement1,
                          ARRAYITEMCOUNT(dlgMovement1));
        }

        // setup sliders
#ifndef __NOSLIDINGFOCUS__
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_FOCUSDELAY_SLIDER),
                           MPFROM2SHORT(5, 10), 3,
                           MPFROM2SHORT(0, 10), 6);
#endif
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_MENUDELAY_SLIDER),
                           MPFROM2SHORT(5, 10), 3,
                           MPFROM2SHORT(0, 10), 6);
    }

    if (flFlags & CBI_SET)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;

        // sliding focus
#ifndef __NOSLIDINGFOCUS__
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SLIDINGFOCUS,
                              pdc->__fSlidingFocus);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_BRING2TOP,
                              pdc->__fSlidingBring2Top);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNORESEAMLESS,
                              pdc->__fSlidingIgnoreSeamless);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREDESKTOP,
                              pdc->__fSlidingIgnoreDesktop);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREPAGEMAGE,
                              pdc->__fSlidingIgnorePageMage);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREXCENTER,
                              pdc->__fSlidingIgnoreXCenter);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_XSDI_MOUSE_FOCUSDELAY_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 // slider uses .1 seconds ticks
                                 pdc->__ulSlidingFocusDelay / 100);
#endif

        // sliding menus
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SLIDINGMENU,
                              pdc->fSlidingMenus);
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_XSDI_MOUSE_MENUDELAY_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 // slider uses .1 seconds ticks
                                 pdc->ulSubmenuDelay / 100);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_CONDCASCADE,
                              pdc->fConditionalCascadeSensitive);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MENUHILITE,
                              pdc->fMenuImmediateHilite);
    }

    if (flFlags & CBI_ENABLE)
    {
        // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;

#ifndef __NOSLIDINGFOCUS__
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_BRING2TOP,
                          pdc->__fSlidingFocus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNORESEAMLESS,
                          pdc->__fSlidingFocus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREDESKTOP,
                          pdc->__fSlidingFocus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREPAGEMAGE,
                          (pdc->__fSlidingFocus)
                          && (cmnQuerySetting(sfEnablePageMage))
                         );
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREXCENTER,
                          pdc->__fSlidingFocus);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_FOCUSDELAY_TXT1,
                          pdc->__fSlidingFocus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_FOCUSDELAY_SLIDER,
                          pdc->__fSlidingFocus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_FOCUSDELAY_TXT2,
                          pdc->__fSlidingFocus);
#endif

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MENUDELAY_TXT1,
                          pdc->fSlidingMenus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MENUDELAY_SLIDER,
                          pdc->fSlidingMenus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MENUDELAY_TXT2,
                          pdc->fSlidingMenus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_CONDCASCADE,
                          pdc->fSlidingMenus);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MENUHILITE,
                          (pdc->fSlidingMenus) && (pdc->ulSubmenuDelay > 0));
    }
}

/*
 *@@ hifMouseMovementItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page in the "Mouse" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.4 (2000-06-12) [umoeller]: fixed "default" and "undo" buttons
 *@@changed V0.9.6 (2000-10-27) [umoeller]: added optional NPSWPS-like submenu behavior
 *@@changed V0.9.7 (2000-12-08) [umoeller]: added "ignore XCenter"
 *@@changed V0.9.9 (2001-03-25) [lafaix]: fixed "default" and "undo" behavior
 *@@changed V0.9.9 (2001-04-07) [pr]: fixed "default" and "undo" again
 *@@changed V0.9.14 (2001-08-02) [lafaix]: moved the autohide stuff to movement page 2
 */

MRESULT hifMouseMovementItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    ULONG ulItemID, USHORT usNotifyCode,
                                    ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
    BOOL    fSave = TRUE;

    _Pmpf(("hifMouseMovementItemChanged: usItemID: %d ulExtra: %d", ulItemID, ulExtra));

    switch (ulItemID)
    {
#ifndef __NOSLIDINGFOCUS__
        /*
         * ID_XSDI_MOUSE_SLIDINGFOCUS:
         *      "sliding focus"
         */

        case ID_XSDI_MOUSE_SLIDINGFOCUS:
            hifLoadHookConfig(pdc);
            pdc->__fSlidingFocus = ulExtra;
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_BRING2TOP:
            hifLoadHookConfig(pdc);
            pdc->__fSlidingBring2Top = ulExtra;
        break;

        case ID_XSDI_MOUSE_IGNORESEAMLESS:
            hifLoadHookConfig(pdc);
            pdc->__fSlidingIgnoreSeamless = ulExtra;
        break;

        case ID_XSDI_MOUSE_IGNOREDESKTOP:
            hifLoadHookConfig(pdc);
            pdc->__fSlidingIgnoreDesktop = ulExtra;
        break;

        case ID_XSDI_MOUSE_IGNOREPAGEMAGE:
            hifLoadHookConfig(pdc);
            pdc->__fSlidingIgnorePageMage = ulExtra;
        break;

        case ID_XSDI_MOUSE_IGNOREXCENTER:
            hifLoadHookConfig(pdc);
            pdc->__fSlidingIgnoreXCenter = ulExtra;
        break;

        case ID_XSDI_MOUSE_FOCUSDELAY_SLIDER:
        {
            CHAR szTemp[30];
            // get .1 seconds offset
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            // convert to ms
            hifLoadHookConfig(pdc);
            pdc->__ulSlidingFocusDelay = lSliderIndex * 100;
            sprintf(szTemp, "%d ms", pdc->__ulSlidingFocusDelay);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_FOCUSDELAY_TXT2,
                              szTemp);
        }
        break;
#endif

        case ID_XSDI_MOUSE_SLIDINGMENU:
            hifLoadHookConfig(pdc);
            pdc->fSlidingMenus = ulExtra;
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_MENUDELAY_SLIDER:
        {
            CHAR szTemp[30];
            // get .1 seconds offset
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            // convert to ms
            hifLoadHookConfig(pdc);
            pdc->ulSubmenuDelay = lSliderIndex * 100;
            sprintf(szTemp, "%d ms", pdc->ulSubmenuDelay);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_MENUDELAY_TXT2,
                              szTemp);
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        }
        break;

        case ID_XSDI_MOUSE_CONDCASCADE:
            hifLoadHookConfig(pdc);
            pdc->fConditionalCascadeSensitive = ulExtra;
        break;

        case ID_XSDI_MOUSE_MENUHILITE:
            hifLoadHookConfig(pdc);
            pdc->fMenuImmediateHilite = ulExtra;
        break;

        /*
         * DID_DEFAULT:
         *
         *changed V0.9.9 (2001-03-25) [lafaix]: saving settings here
         */

        case DID_DEFAULT:
            hifLoadHookConfig(pdc);

#ifndef __NOSLIDINGFOCUS__
            pdc->__fSlidingFocus = 0;
            pdc->__fSlidingBring2Top = 0;
            pdc->__fSlidingIgnoreSeamless = 0;
            pdc->__fSlidingIgnoreDesktop = 0;
            pdc->__fSlidingIgnorePageMage = 0;
            pdc->__fSlidingIgnoreXCenter = 0;  // V0.9.9 (2001-04-07) [pr]
            pdc->__ulSlidingFocusDelay = 0;
#endif
            pdc->fSlidingMenus = 0;
            pdc->ulSubmenuDelay = 0;
            pdc->fConditionalCascadeSensitive = 0;
            pdc->fMenuImmediateHilite = 0;

            // saving settings here
            hifHookConfigChanged(pdc);
            fSave = FALSE;

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * DID_UNDO:
         *
         *changed V0.9.9 (2001-03-25) [lafaix]: saving settings here
         */

        case DID_UNDO:
            // restore data which was backed up in INIT callback
            hifLoadHookConfig(pdc);
            if (pcnbp->pUser2)
            {
                PHOOKCONFIG pBackup = (PHOOKCONFIG)pcnbp->pUser2;
#ifndef __NOSLIDINGFOCUS__
                pdc->__fSlidingFocus = pBackup->__fSlidingFocus;
                pdc->__fSlidingBring2Top = pBackup->__fSlidingBring2Top;
                pdc->__fSlidingIgnoreSeamless = pBackup->__fSlidingIgnoreSeamless;
                pdc->__fSlidingIgnoreDesktop = pBackup->__fSlidingIgnoreDesktop;
                pdc->__fSlidingIgnorePageMage = pBackup->__fSlidingIgnorePageMage;
                pdc->__fSlidingIgnoreXCenter = pBackup->__fSlidingIgnoreXCenter;  // V0.9.9 (2001-04-07) [pr]
                pdc->__ulSlidingFocusDelay = pBackup->__ulSlidingFocusDelay;
#endif
                pdc->fSlidingMenus = pBackup->fSlidingMenus;
                pdc->ulSubmenuDelay = pBackup->ulSubmenuDelay;
                pdc->fConditionalCascadeSensitive = pBackup->fConditionalCascadeSensitive;
                pdc->fMenuImmediateHilite = pBackup->fMenuImmediateHilite;

                // saving settings here
                hifHookConfigChanged(pdc);
                fSave = FALSE;
            }
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            fSave = FALSE;
    }

    if (fSave)
        hifHookConfigChanged(pdc);

    return (mrc);
}

/* ******************************************************************
 *
 *   XWPMouse "Movement 2" settings page
 *
 ********************************************************************/

#ifndef __NOMOVEMENT2FEATURES__

/*
 *@@ hifMouseMovement2InitPage:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page 2 in the "Mouse" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.14 (2001-08-02) [lafaix]
 *@@changed V0.9.6 (2000-10-27) [umoeller]: added optional NPSWPS-like submenu behavior
 *@@changed V0.9.7 (2000-12-08) [umoeller]: added "ignore XCenter"
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added delay for auto-move ptr
 */

VOID hifMouseMovement2InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                               ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            // first call: create HOOKCONFIG structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser)
                hifLoadHookConfig(pcnbp->pUser);

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(HOOKCONFIG));
        }

        // setup sliders
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_AUTOHIDE_SLIDER),
                           MPFROM2SHORT(4, 10), 3,
                           MPFROM2SHORT(9, 10), 6);

        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_AUTOMOVE_SLIDER),
                           MPFROM2SHORT(9, 10), 6,
                           (MPARAM)-1, -1);
    }

    if (flFlags & CBI_SET)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;

        // auto-hide mouse pointer
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_CHECK,
                              pdc->__fAutoHideMouse);
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_XSDI_MOUSE_AUTOHIDE_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pdc->__ulAutoHideDelay);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_CHECKMNU,
                              pdc->__ulAutoHideFlags & AHF_IGNOREMENUS);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_CHECKBTN,
                              pdc->__ulAutoHideFlags & AHF_IGNOREBUTTONS);

        // auto-move mouse pointer to default button
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOMOVE_CHECK,
                              pdc->__fAutoMoveMouse);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_XSDI_MOUSE_AUTOMOVE_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pdc->__ulAutoMoveDelay / 100);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOMOVE_ANIMATE,
                              ((pdc->__ulAutoMoveFlags & AMF_ANIMATE) != 0));

    }

    if (flFlags & CBI_ENABLE)
    {
        // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_TXT1,
                          pdc->__fAutoHideMouse);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_SLIDER,
                          pdc->__fAutoHideMouse);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_TXT2,
                          pdc->__fAutoHideMouse);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_CHECKMNU,
                          pdc->__fAutoHideMouse);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_CHECKBTN,
                          pdc->__fAutoHideMouse);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOMOVE_TXT1,
                          pdc->__fAutoMoveMouse);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOMOVE_SLIDER,
                          pdc->__fAutoMoveMouse);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOMOVE_TXT2,
                          pdc->__fAutoMoveMouse);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOMOVE_ANIMATE,
                             (pdc->__fAutoMoveMouse)
                          && (pdc->__ulAutoMoveDelay > 0));
    }
}

/*
 *@@ hifMouseMovement2ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page 2 in the "Mouse" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.14 (2001-08-02) [lafaix]
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added delay for auto-move ptr
 */

MRESULT hifMouseMovement2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID, USHORT usNotifyCode,
                                     ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
    BOOL    fSave = TRUE;
    CHAR    szTemp[30];

    _Pmpf(("hifMouseMovement2ItemChanged: usItemID: %d ulExtra: %d", ulItemID, ulExtra));

    switch (ulItemID)
    {
        case ID_XSDI_MOUSE_AUTOHIDE_CHECK:
            hifLoadHookConfig(pdc);
            pdc->__fAutoHideMouse = ulExtra;
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_AUTOHIDE_SLIDER:
        {
            // get delay
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            // convert to seconds
            hifLoadHookConfig(pdc);
            pdc->__ulAutoHideDelay = lSliderIndex;
            sprintf(szTemp, "%d s", pdc->__ulAutoHideDelay + 1);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_AUTOHIDE_TXT2,
                              szTemp);
        }
        break;

        case ID_XSDI_MOUSE_AUTOHIDE_CHECKMNU:
            hifLoadHookConfig(pdc);
            if (ulExtra)
                pdc->__ulAutoHideFlags |= AHF_IGNOREMENUS;
            else
                pdc->__ulAutoHideFlags &= ~AHF_IGNOREMENUS;
        break;

        case ID_XSDI_MOUSE_AUTOHIDE_CHECKBTN:
            hifLoadHookConfig(pdc);
            if (ulExtra)
                pdc->__ulAutoHideFlags |= AHF_IGNOREBUTTONS;
            else
                pdc->__ulAutoHideFlags &= ~AHF_IGNOREBUTTONS;
        break;

        case ID_XSDI_MOUSE_AUTOMOVE_CHECK:
            hifLoadHookConfig(pdc);
            pdc->__fAutoMoveMouse = ulExtra;
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_AUTOMOVE_SLIDER:
        {
            // get delay
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            // convert to milliseconds
            hifLoadHookConfig(pdc);
            pdc->__ulAutoMoveDelay = lSliderIndex * 100;
            sprintf(szTemp, "%d ms", pdc->__ulAutoMoveDelay);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_AUTOMOVE_TXT2,
                              szTemp);
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
        }
        break;

        case ID_XSDI_MOUSE_AUTOMOVE_ANIMATE:
            hifLoadHookConfig(pdc);
            if (ulExtra)
                pdc->__ulAutoMoveFlags |= AMF_ANIMATE;
            else
                pdc->__ulAutoMoveFlags &= ~AMF_ANIMATE;
        break;

        /*
         * DID_DEFAULT:
         *
         */

        case DID_DEFAULT:
            hifLoadHookConfig(pdc);

            pdc->__fAutoHideMouse = 0;
            pdc->__ulAutoHideDelay = 0;
            pdc->__ulAutoHideFlags = 0;
            pdc->__fAutoMoveMouse = 0;
            pdc->__ulAutoMoveFlags = 0;
            pdc->__ulAutoMoveDelay = 0;

            // saving settings here
            hifHookConfigChanged(pdc);
            fSave = FALSE;

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * DID_UNDO:
         *
         */

        case DID_UNDO:
            // restore data which was backed up in INIT callback
            hifLoadHookConfig(pdc);
            if (pcnbp->pUser2)
            {
                PHOOKCONFIG pBackup = (PHOOKCONFIG)pcnbp->pUser2;
                pdc->__fAutoHideMouse = pBackup->__fAutoHideMouse;
                pdc->__ulAutoHideDelay = pBackup->__ulAutoHideDelay;
                pdc->__ulAutoHideFlags = pBackup->__ulAutoHideFlags;
                pdc->__fAutoMoveMouse = pBackup->__fAutoMoveMouse;
                pdc->__ulAutoMoveFlags = pBackup->__ulAutoMoveFlags;
                pdc->__ulAutoMoveDelay = pBackup->__ulAutoMoveDelay;

                // saving settings here
                hifHookConfigChanged(pdc);
                fSave = FALSE;
            }
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            fSave = FALSE;
    }

    if (fSave)
        hifHookConfigChanged(pdc);

    return (mrc);
}

#endif

/* ******************************************************************
 *
 *   XWPScreen "Screen borders" settings page
 *
 ********************************************************************/

static ULONG   G_ulScreenCornerSelectedID = ID_XSDI_MOUSE_RADIO_TOPLEFT;
static ULONG   G_ulScreenCornerSelectedIndex = 0;
                            // 0 = lower left corner,
                            // 1 = top left corner,
                            // 2 = lower right corner,
                            // 3 = top right corner;
                            //      the following added with V0.9.4 (2000-06-12) [umoeller]:
                            // 4 = top border,
                            // 5 = left border,
                            // 6 = right border,
                            // 7 = bottom border

// screen corner object container d'n'd
static HOBJECT G_hobjBeingDragged = NULLHANDLE;
            // NULLHANDLE means dropping is invalid;
            // in between CN_DRAGOVER and CN_DROP, this
            // contains the object handle being dragged

static BOOL    G_fShutUpSlider = FALSE;

/*
 *@@ UpdateScreenCornerIndex:
 *
 *@@changed V0.9.4 (2000-06-12) [umoeller]: added screen borders
 */

VOID UpdateScreenCornerIndex(USHORT usItemID)
{
    switch (usItemID)
    {
        case ID_XSDI_MOUSE_RADIO_TOPLEFT:
            G_ulScreenCornerSelectedIndex = 1; break;
        case ID_XSDI_MOUSE_RADIO_TOPRIGHT:
            G_ulScreenCornerSelectedIndex = 3; break;
        case ID_XSDI_MOUSE_RADIO_BOTTOMLEFT:
            G_ulScreenCornerSelectedIndex = 0; break;
        case ID_XSDI_MOUSE_RADIO_BOTTOMRIGHT:
            G_ulScreenCornerSelectedIndex = 2; break;

        // V0.9.4 (2000-06-12) [umoeller]
        case ID_XSDI_MOUSE_RADIO_TOP:
            G_ulScreenCornerSelectedIndex = 4; break;
        case ID_XSDI_MOUSE_RADIO_LEFT:
            G_ulScreenCornerSelectedIndex = 5; break;
        case ID_XSDI_MOUSE_RADIO_RIGHT:
            G_ulScreenCornerSelectedIndex = 6; break;
        case ID_XSDI_MOUSE_RADIO_BOTTOM:
            G_ulScreenCornerSelectedIndex = 7; break;
    }
}

/*
 *@@ hifMouseCornersInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page in the "Screen" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.4 (2000-08-08) [umoeller]: added PageMage to special functions
 *@@changed V0.9.9 (2001-01-25) [lafaix]: added more PageMage special functions
 *@@changed V0.9.9 (2001-03-15) [lafaix]: added Corner sensitivity setting
 *@@changed V0.9.9 (2001-03-27) [umoeller]: converted page to use non-auto radio buttons; fixed slider msgs
 */

VOID hifMouseCornersInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                              ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            // first call: create HOOKCONFIG
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser)
                hifLoadHookConfig(pcnbp->pUser);

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(HOOKCONFIG));
        }

        // check top left screen corner
        G_ulScreenCornerSelectedID = ID_XSDI_MOUSE_RADIO_TOPLEFT;
        G_ulScreenCornerSelectedIndex = 1;        // top left
        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              G_ulScreenCornerSelectedID,
                              TRUE);

        // fill drop-down box
        {
            // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
            // ULONG   ul;
            HWND    hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage,
                                               ID_XSDI_MOUSE_SPECIAL_DROP);

            WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_SPECIAL_WINDOWLIST)) ; // pszSpecialWindowList
            WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_SPECIAL_DESKTOPPOPUP)) ; // pszSpecialDesktopPopup

#ifndef __NOPAGEMAGE__
            WinInsertLboxItem(hwndDrop, LIT_END, "PageMage");
            // V0.9.9 (2001-01-25) [lafaix] (clockwise)
            WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_SPECIAL_PAGEMAGEUP)) ; // pszSpecialPageMageUp
            WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_SPECIAL_PAGEMAGERIGHT)) ; // pszSpecialPageMageRight
            WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_SPECIAL_PAGEMAGEDOWN)) ; // pszSpecialPageMageDown
            WinInsertLboxItem(hwndDrop, LIT_END, cmnGetString(ID_XSSI_SPECIAL_PAGEMAGELEFT)) ; // pszSpecialPageMageLeft
#endif
        }

        // set up container
        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_NAME | CV_MINI | CA_DRAWICON);
        } END_CNRINFO(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CNR));

        // set up slider
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_CORNERSIZE_SLIDER),
                           0, 3,
                           MPFROM2SHORT(0, 2), 6);
    }

    if (flFlags & CBI_SET)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
        HWND    hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CNR);
        HWND    hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_DROP);
        ULONG   ulCurrentObj;

        _Pmpf((__FUNCTION__ ": CBI_SET, sel: %d", G_ulScreenCornerSelectedIndex));

        // corner sensitivity
        G_fShutUpSlider = TRUE;
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_XSDI_MOUSE_CORNERSIZE_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 // slider uses 5% seconds ticks
                                 pdc->ulCornerSensitivity * 2 / 10);
        G_fShutUpSlider = FALSE;

        // screen corners objects:
        // set all radio buttons (these are no longer "auto" radio buttons V0.9.9 (2001-03-27) [umoeller])
        ulCurrentObj = (ULONG)pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex];

        _Pmpf(("  current obj: 0x%lX", ulCurrentObj));

        if (ulCurrentObj == 0)
        {
            // "Inactive" corner:
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_INACTIVEOBJ, TRUE);
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_CHECK, FALSE);
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CHECK, FALSE);
            cnrhRemoveAll(hwndCnr);
            winhSetLboxSelectedItem(hwndDrop, LIT_NONE, TRUE);
        }
        else if (ulCurrentObj >= 0xFFFF0000)
        {
            // special function for this corner:
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_INACTIVEOBJ, FALSE);
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_CHECK, TRUE);
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CHECK, FALSE);
            cnrhRemoveAll(hwndCnr);
            winhSetLboxSelectedItem(hwndDrop,
                                    (pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex]
                                         & 0xFFFF),
                                    TRUE);
        }
        else
        {
            // actual object for this corner:
            HOBJECT hobj = (HOBJECT)ulCurrentObj;
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_INACTIVEOBJ, FALSE);
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_CHECK, FALSE);
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CHECK, TRUE);

            winhSetLboxSelectedItem(hwndDrop,
                                    LIT_NONE,
                                    TRUE);

            cnrhRemoveAll(hwndCnr);
            if (hobj != 1)
            {
                WPObject *pobj = _wpclsQueryObject(_WPObject,
                                                   hobj);
                if (pobj)
                {
                    PRECORDCORE precc = cnrhAllocRecords(hwndCnr,
                                                         sizeof(RECORDCORE),
                                                         1);
                    precc->pszName = _wpQueryTitle(pobj);
                    precc->hptrMiniIcon = _wpQueryIcon(pobj);
                    cnrhInsertRecords(hwndCnr,
                                      NULL,         // parent
                                      precc,
                                      TRUE, // invalidate
                                      NULL,
                                      CRA_RECORDREADONLY,
                                      1);
                }
            }
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_DROP,
                          winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                               ID_XSDI_MOUSE_SPECIAL_CHECK));
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CNR,
                          winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                               ID_XSDI_MOUSE_OPEN_CHECK));
    }
}

/*
 *@@ hifMouseCornersItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page in the "Mouse" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.4 (2000-06-12) [umoeller]: added screen borders
 *@@changed V0.9.4 (2000-06-12) [umoeller]: fixed "default" and "undo" buttons
 *@@changed V0.9.9 (2001-03-15) [lafaix]: added corner sensitivity settings
 *@@changed V0.9.9 (2001-03-25) [lafaix]: fixed "default" and "undo" behavior
 *@@changed V0.9.9 (2001-03-27) [umoeller]: converted page to use non-auto radio buttons; fixed slider msgs
 */

MRESULT hifMouseCornersItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                   ULONG ulItemID, USHORT usNotifyCode,
                                   ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
    BOOL    fSave = TRUE;

    if (G_fShutUpSlider)
        return (0);             // V0.9.9 (2001-03-27) [umoeller]

    _Pmpf((__FUNCTION__ ": usItemID: %d ulExtra: %d", ulItemID, ulExtra));
    _Pmpf(("  selected index: %d", G_ulScreenCornerSelectedIndex));

    switch (ulItemID)
    {
        /*
         * ID_XSDI_MOUSE_RADIO_TOPLEFT:
         *      new screen corner selected
         */

        case ID_XSDI_MOUSE_RADIO_TOPLEFT:
        case ID_XSDI_MOUSE_RADIO_TOPRIGHT:
        case ID_XSDI_MOUSE_RADIO_BOTTOMLEFT:
        case ID_XSDI_MOUSE_RADIO_BOTTOMRIGHT:
        case ID_XSDI_MOUSE_RADIO_TOP:
        case ID_XSDI_MOUSE_RADIO_LEFT:
        case ID_XSDI_MOUSE_RADIO_RIGHT:
        case ID_XSDI_MOUSE_RADIO_BOTTOM:
            // check if the old current corner's object
            // is 1 (our "pseudo" object)
            if (pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] == 1)
                // that's an invalid object, so set to 0 (no function)
                pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] = 0;

            // uncheck old radio button
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, G_ulScreenCornerSelectedID, FALSE);

            // update global and then update controls on page
            G_ulScreenCornerSelectedID = ulItemID;
            UpdateScreenCornerIndex(ulItemID);
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, G_ulScreenCornerSelectedID, TRUE);

            /* _Pmpf(("ctrl: %lX, index: %lX",
                   ulScreenCornerSelectedID,
                   ulScreenCornerSelectedIndex)); */

            _Pmpf(("  new selected index: %d", G_ulScreenCornerSelectedIndex));

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            fSave = FALSE;
        break;

        /*
         * ID_XSDI_MOUSE_INACTIVEOBJ:
         *
         */

        case ID_XSDI_MOUSE_INACTIVEOBJ:
            // disable hot corner
            hifLoadHookConfig(pdc);
            pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] = 0;

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * ID_XSDI_MOUSE_OPEN_CHECK:
         *      "open object"
         */

        case ID_XSDI_MOUSE_OPEN_CHECK:
            hifLoadHookConfig(pdc);
            if (    (pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] == 0)
                 ||  (pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] >= 0xFFFF0000)
                )
                // mode changed to object mode: store a pseudo-object
                pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] = 1;
                    // an object handle of 1 does not exist, however
                    // we need this for the INIT callback to work

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_OPEN_CNR:
            hifLoadHookConfig(pdc);
            switch (usNotifyCode)
            {
                case CN_DRAGOVER:
                    mrc = wpshQueryDraggedObjectCnr((PCNRDRAGINFO)ulExtra,
                                                    &G_hobjBeingDragged);
                break; // CN_DRAGOVER

                case CN_DROP:
                    if (G_hobjBeingDragged)
                    {
                        pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex]
                                = G_hobjBeingDragged;
                        G_hobjBeingDragged = NULLHANDLE;

                        pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
                    }
                break;
            }
        break;

        /*
         * ID_XSDI_MOUSE_SPECIAL_CHECK:
         *      "special features"
         */

        case ID_XSDI_MOUSE_SPECIAL_CHECK:
            hifLoadHookConfig(pdc);
            pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] = 0xFFFF0000;

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_SPECIAL_DROP:
        {
            // new special function selected from drop-down box:
            HWND hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage, ulItemID);
            LONG lIndex = winhQueryLboxSelectedItem(hwndDrop, LIT_FIRST);
            hifLoadHookConfig(pdc);

            if (lIndex == LIT_NONE)
                // disable hot corner
                pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex] = 0;
            else
                // store special function, which has the hiword as FFFF
                pdc->ahobjHotCornerObjects[G_ulScreenCornerSelectedIndex]
                    = 0xFFFF0000 | lIndex;
        }
        break;

        /*
         * ID_XSDI_MOUSE_CORNERSIZE_SLIDER:
         *      corner sensitivity
         */

        case ID_XSDI_MOUSE_CORNERSIZE_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);

            // convert to percents
            hifLoadHookConfig(pdc);
            pdc->ulCornerSensitivity = lSliderIndex * 10 / 2;
        }
        break;

        /*
         * DID_DEFAULT:
         *
         *changed V0.9.9 (2001-03-25) [lafaix]: saving settings here
         */

        case DID_DEFAULT:
            hifLoadHookConfig(pdc);
            memset(pdc->ahobjHotCornerObjects, 0, sizeof(pdc->ahobjHotCornerObjects));
            pdc->ulCornerSensitivity = 30;

            hifHookConfigChanged(pdc);
            fSave = FALSE;

            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * DID_UNDO:
         *
         *changed V0.9.9 (2001-03-25) [lafaix]: saving settings here
         */

        case DID_UNDO:
            hifLoadHookConfig(pdc);
            // restore data which was backed up in INIT callback
            if (pcnbp->pUser2)
            {
                PHOOKCONFIG pBackup = (PHOOKCONFIG)pcnbp->pUser2;
                memcpy(pdc->ahobjHotCornerObjects,
                       pBackup->ahobjHotCornerObjects,
                       sizeof(pdc->ahobjHotCornerObjects));
                pdc->ulCornerSensitivity = pBackup->ulCornerSensitivity;
                            // V0.9.9 (2001-03-27) [umoeller]
                hifHookConfigChanged(pdc);
                fSave = FALSE;
            }
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            fSave = FALSE;
    }

    if (fSave)
        hifHookConfigChanged(pdc);

    return (mrc);
}


