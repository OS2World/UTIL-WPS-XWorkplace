
/*
 *@@sourcefile xfdisk.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XFldDisk (WPDisk replacement)
 *
 *      XFldDisk is needed mainly to modify the popup menus
 *      of Disk objects also. Since these are not instances
 *      of WPFolder, we need an extra subclass.
 *
 *      Installation of this class is now optional (V0.9.0).
 *      However, if it is installed, XFolder must also be
 *      installed.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      i.e. the methods themselves.
 *      The implementation for this class is mostly in filesys\disk.c.
 *
 *@@somclass XFldDisk xfdisk_
 *@@somclass M_XFldDisk xfdiskM_
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

/*
 *@@todo:
 *
 */

/*
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.42
 */

#ifndef SOM_Module_xfdisk_Source
#define SOM_Module_xfdisk_Source
#endif
#define XFldDisk_Class_Source
#define M_XFldDisk_Class_Source

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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include  <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfdisk.ih"
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\disk.h"               // XFldDisk implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\menus.h"              // common XFolder context menu logic
#include "filesys\statbars.h"           // status bar translation logic

// other SOM headers
#pragma hdrstop
#include "helpers\undoc.h"              // some undocumented stuff

// global array of all awake Disk objects
// XFldDisk* apDrives[27];

/* ******************************************************************
 *                                                                  *
 *   here come the XFldDisk instance methods                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpInitData:
 *      this instance method gets called when the object
 *      is being initialized. We initialize our instance
 *      data here.
 */

SOM_Scope void  SOMLINK xfdisk_wpInitData(XFldDisk *somSelf)
{
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpInitData");

    XFldDisk_parent_WPDisk_wpInitData(somSelf);

    // _szFSType[0] = '\0';
}

/*
 *@@ wpObjectReady:
 *      this is called upon an object when its creation
 *      or awakening is complete. This is the last method
 *      which gets called during instantiation of a
 *      WPS object when it has completely initialized
 *      itself. ulCode signifies the cause of object
 *      instantiation.
 *
 *      We will have this object's pointer stored in a
 *      global array to be able to find the Disk object for
 *      a given object (M_XFldDisk::xwpclsQueryDiskObject).
 */

SOM_Scope void  SOMLINK xfdisk_wpObjectReady(XFldDisk *somSelf,
                                             ULONG ulCode, WPObject* refObject)
{
    ULONG ulDisk;
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpObjectReady");

    XFldDisk_parent_WPDisk_wpObjectReady(somSelf, ulCode, refObject);

    // ulDisk = _wpQueryLogicalDrive(somSelf);
    // apDrives[ulDisk] = somSelf;
}

/*
 *@@ wpAddDiskDetailsPage:
 *      this adds the "Details" page to a disk object's
 *      settings notebook.
 *
 *      We override this method to insert our enhanced
 *      page instead.
 *
 *@@added V0.9.0 [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfdisk_wpAddDiskDetailsPage(XFldDisk *somSelf,
                                                     HWND hwndNotebook)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XFldDiskData *somThis = XFldDiskGetData(somSelf); */
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpAddDiskDetailsPage");

    if (pGlobalSettings->fReplaceFilePage)
    {
        PCREATENOTEBOOKPAGE pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
        PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

        memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
        pcnbp->somSelf = somSelf;
        pcnbp->hwndNotebook = hwndNotebook;
        pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
        pcnbp->ulDlgID = ID_XSD_DISK_DETAILS;
        pcnbp->ulPageID = SP_DISK_DETAILS;
        pcnbp->usPageStyleFlags = BKA_MAJOR;
        pcnbp->pszName = pNLSStrings->pszDetailsPage;
        pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_DISKDETAILS;

        pcnbp->pfncbInitPage    = (PFNCBACTION)dskDetailsInitPage;
        pcnbp->pfncbItemChanged = dskDetailsItemChanged;

        return (ntbInsertPage(pcnbp));
    }
    else
        return (XFldDisk_parent_WPDisk_wpAddDiskDetailsPage(somSelf,
                                                            hwndNotebook));
}

/*
 *@@ wpFilterPopupMenu:
 *      remove default entries for disks according to Global Settings
 */

SOM_Scope ULONG  SOMLINK xfdisk_wpFilterPopupMenu(XFldDisk *somSelf,
                                                ULONG ulFlags,
                                                HWND hwndCnr,
                                                BOOL fMultiSelect)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpFilterPopupMenu");

    return (XFldDisk_parent_WPDisk_wpFilterPopupMenu(somSelf,
                                                     ulFlags,
                                                     hwndCnr,
                                                     fMultiSelect)
        & ~(pGlobalSettings->DefaultMenuItems)
        );
}

/*
 *@@ wpModifyPopupMenu:
 *      this method allows an object to modify a context
 *      menu. We add the various XFolder menu entries here
 *      by calling mnuModifyFolderPopupMenu in menus.c,
 *      which is also used by the XFolder class.
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpModifyPopupMenu(XFldDisk *somSelf,
                                               HWND hwndMenu,
                                               HWND hwndCnr,
                                               ULONG iPosition)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    BOOL            rc;

    // get the "root folder" of the WPDisk object; that is the
    // folder which is opened when the disk object is opened
    // (i.e. the folder of the root directory). This returns
    // NULL if the drive is not ready.
    XFolder         *pFolder = _wpQueryRootFolder(somSelf);

    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpModifyPopupMenu");

    rc = XFldDisk_parent_WPDisk_wpModifyPopupMenu(somSelf,hwndMenu,hwndCnr,iPosition);

    if (pGlobalSettings->RemoveFormatDiskItem)
        winhRemoveMenuItem(hwndMenu, ID_WPMI_FORMATDISK);

    if (pGlobalSettings->RemoveCheckDiskItem)
        winhRemoveMenuItem(hwndMenu, ID_WPMI_CHECKDISK);

    if (pFolder)
        // drive ready:
        rc = mnuModifyFolderPopupMenu(pFolder,
                                      hwndMenu,
                                      hwndCnr,    // ### this seems to be NULLHANDLE
                                      iPosition);

    return (rc);
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This is overridden to support the new menu items
 *      we have inserted for our subclass.
 *
 *      We pass the input to mnuMenuItemSelected in menus.c
 *      because disk menu items are mostly shared with XFolder.
 *
 *      Note that the WPS invokes this method upon every
 *      object which has been selected in the container.
 *      That is, if three objects have been selected and
 *      a menu item has been selected for all three of
 *      them, all three objects will receive this method
 *      call. This is true even if FALSE is returned from
 *      this method.
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpMenuItemSelected(XFldDisk *somSelf,
                                                  HWND hwndFrame,
                                                  ULONG ulMenuId)
{
    XFolder         *pFolder = _wpQueryRootFolder(somSelf);
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpMenuItemSelected");

    if (mnuMenuItemSelected(pFolder, hwndFrame, ulMenuId))
        return TRUE;
    else
        return (XFldDisk_parent_WPDisk_wpMenuItemSelected(somSelf, hwndFrame,
                                                   ulMenuId));
}

/*
 *@@ wpMenuItemHelpSelected:
 *      display help for a context menu item;
 *      we pass the input to mnuMenuItemHelpSelected in menus.c.
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpMenuItemHelpSelected(XFldDisk *somSelf,
                                                    ULONG MenuId)
{
    XFolder         *pFolder = _wpQueryRootFolder(somSelf);
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpMenuItemHelpSelected");

    if (mnuMenuItemHelpSelected(pFolder, MenuId))
        return TRUE;
    else
        return (XFldDisk_parent_WPDisk_wpMenuItemHelpSelected(somSelf,
                                                           MenuId));
}

/*
 *@@ wpViewObject:
 *      this either opens a view of the given Disk object
 *      as a root folder (using wpOpen) or resurfaces an
 *      already open view.
 *
 *      This gets called every time the user tries to open
 *      a disk object, e.g. by double-clicking on one in the
 *      "Drives" folder. If a new view is needed, this method
 *      normally calls wpOpen.
 *
 *      The WPS seems to be doing no drive checking in here,
 *      which leads to the annoying "Drive not ready" popups.
 *      So we try to implement this here.
 *
 *@@changed V0.9.0 [umoeller]: added global setting for disabling this feature
 */

SOM_Scope HWND  SOMLINK xfdisk_wpViewObject(XFldDisk *somSelf,
                                            HWND hwndCnr, ULONG ulView,
                                            ULONG param)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND            hwndFrame = NULLHANDLE; // default: error occured

    /* XFldDiskData *somThis = XFldDiskGetData(somSelf); */
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpViewObject");

    // "Drive not ready" replacement enabled?
    if (pGlobalSettings->fReplDriveNotReady)
    {
        // yes: use the safe way of opening the
        // drive (this prompts the user upon errors)
        XFolder*        pRootFolder = NULL;
        pRootFolder = dskCheckDriveReady(somSelf);
        if (pRootFolder)
            // success: call default
            hwndFrame = XFldDisk_parent_WPDisk_wpViewObject(somSelf, hwndCnr,
                                                            ulView, param);
    }
    else
        hwndFrame = XFldDisk_parent_WPDisk_wpViewObject(somSelf, hwndCnr,
                                                        ulView, param);

    return (hwndFrame);
}

/*
 *@@ wpOpen:
 *      this method gets called when wpViewObject thinks a
 *      new view of the object needs to be opened.
 *
 *      We call the parent first and then subclass the
 *      resulting frame window, similar to what we're doing
 *      with folder views (see XFolder::wpOpen).
 *
 *@@changed V0.9.2 (2000-03-06) [umoeller]: drives were checked even if replacement dlg was disabled; fixed
 */

SOM_Scope HWND  SOMLINK xfdisk_wpOpen(XFldDisk *somSelf, HWND hwndCnr,
                                       ULONG ulView, ULONG param)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND            hwndNewFrame = NULLHANDLE; // default: error occured
    APIRET          arc = NO_ERROR;
    XFolder*        pRootFolder = 0;
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpOpen");

    // "Drive not ready" replacement enabled?
    if (pGlobalSettings->fReplDriveNotReady)
    {
        // query root folder (WPRootFolder class, which is a descendant
        // of WPFolder/XFolder); each WPDisk is paired with one of those,
        // and the actual display is done by this class, so we will pass
        // pRootFolder instead of somSelf to most following method calls.
        // We use wpshQueryRootFolder instead of wpQueryRootFolder to
        // avoid "Drive not ready" popups.
        pRootFolder = wpshQueryRootFolder(somSelf, &arc);
        if (pRootFolder)
            // drive ready: call parent to get frame handle
            hwndNewFrame = XFldDisk_parent_WPDisk_wpOpen(somSelf,
                                                         hwndCnr,
                                                         ulView,
                                                         param);
    }
    else
    {
        hwndNewFrame = XFldDisk_parent_WPDisk_wpOpen(somSelf,
                                                     hwndCnr,
                                                     ulView,
                                                     param);
        if (hwndNewFrame)
            pRootFolder = _wpQueryRootFolder(somSelf);
    }

    if (pRootFolder)
    {

        if (hwndNewFrame)
        {
            // open successful:
            if (   (ulView == OPEN_CONTENTS)
                || (ulView == OPEN_TREE)
                || (ulView == OPEN_DETAILS)
               )
            {
                PSUBCLASSEDLISTITEM psli;

                // subclass frame window; this is the same
                // proc which is used for normal folder frames,
                // we just use pRootFolder instead.
                // However, we pass somSelf as the "real" object
                // which will then be stored in *psli.
                psli = fdrSubclassFolderFrame(hwndNewFrame, pRootFolder, somSelf, ulView);

                // add status bar, if allowed: as opposed to
                // XFolder's, for XFldDisk's we only check the
                // global setting, because there's no instance
                // setting for this with XFldDisk's
                if (    (pGlobalSettings->fEnableStatusBars)
                                                // feature enabled?
                     && (pGlobalSettings->fDefaultStatusBarVisibility)
                                                // bars visible per default?
                   )
                    // assert that subclassed list item is valid
                    if (psli)
                        // add status bar only if allowed for the current view type
                        if (    (   (ulView == OPEN_CONTENTS)
                                 && (pGlobalSettings->SBForViews & SBV_ICON)
                                )
                             || (   (ulView == OPEN_TREE)
                                 && (pGlobalSettings->SBForViews & SBV_TREE)
                                )
                             || (   (ulView == OPEN_DETAILS)
                                 && (pGlobalSettings->SBForViews & SBV_DETAILS)
                                )
                            )
                            // this reformats the window with the status bar
                            fdrCreateStatusBar(pRootFolder, psli, TRUE);

                // extended sort functions
                if (pGlobalSettings->ExtFolderSort)
                {
                    hwndCnr = wpshQueryCnrFromFrame(hwndNewFrame);
                    if (hwndCnr)
                        fdrSetFldrCnrSort(pRootFolder, hwndCnr, FALSE);    // xfldr.c
                }
            }
        }
    }

    return (hwndNewFrame);
}

/* ******************************************************************
 *                                                                  *
 *   here come the XFldDisk class methods                           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xfclsQueryDisk:
 *      since I have no idea what the default wpQueryDisk
 *      is for (why the hell does it return a WPFileSystem?),
 *      this is a proper method which returns the disk object
 *      which corresponds to the drive on which the object
 *      resides. This works for both abstract and file-system
 *      objects.
 *
 *      Returns NULL upon errors.
 *
 *      Note: this method only works after the Drives folder
 *      has been populated because it relies on our
 *      wpObjectReady override for XFldDisk. Otherwise you'll
 *      get NULL back.
 *
 *      The WPS normally populates that folder a few seconds
 *      after the Desktop is up.
 */

/* SOM_Scope XFldDisk*  SOMLINK xfdiskM_xwpclsQueryDiskObject(M_XFldDisk *somSelf,
                                                          WPObject* pObject)
{
    ULONG ulLogicalDrive = wpshQueryLogicalDriveNumber(pObject);
    // M_XFldDiskData *somThis = M_XFldDiskGetData(somSelf);
    M_XFldDiskMethodDebug("M_XFldDisk","xfdiskM_xwpclsQueryDiskObject");

    // _Pmpf(("xwpclsQueryDiskObject: ulDrive = %d", ulLogicalDrive));
    return (apDrives[ulLogicalDrive]);
} */

/*
 *@@ wpclsInitData:
 *      initialize XFldDisk class data.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfdiskM_wpclsInitData(M_XFldDisk *somSelf)
{
    // M_XFldDiskData *somThis = M_XFldDiskGetData(somSelf);
    M_XFldDiskMethodDebug("M_XFldDisk","xfdiskM_wpclsInitData");

    M_XFldDisk_parent_M_WPDisk_wpclsInitData(somSelf);
    // M_XFldDisk_parent_M_XFldObject_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXFldDisk = TRUE;
        krnUnlockGlobals();
    }

    // memset(&apDrives, 0, sizeof(apDrives));
}


