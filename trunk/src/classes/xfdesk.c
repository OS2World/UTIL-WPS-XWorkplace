
/*
 *@@sourcefile xfdesk.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XFldDesktop (WPDesktop replacement)
 *
 *      XFldDesktop provides access to the eXtended shutdown
 *      feature (fntShutdownThread) by modifying popup menus.
 *
 *      Also, the XFldDesktop settings notebook pages offer access
 *      to startup and shutdown configuration data.
 *
 *      Installation of this class is now optional (V0.9.0).
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      i.e. the methods themselves.
 *      The implementation for this class is mostly in filesys\desktop.c,
 *      filesys\shutdown.c, and filesys\archives.c.
 *
 *@@somclass XFldDesktop xfdesk_
 *@@somclass M_XFldDesktop xfdeskM_
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M�ller.
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
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xfdesk_Source
#define SOM_Module_xfdesk_Source
#endif
#define XFldDesktop_Class_Source
#define M_XFldDesktop_Class_Source

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

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMENUS
#define INCL_WINPOINTERS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <io.h>
#include <math.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"
#include "xfdesk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\init.h"                // XWorkplace initialization
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\desktop.h"            // XFldDesktop implementation
#include "filesys\folder.h"             // XFolder implementation
// #include "filesys\object.h"             // XFldObject implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "startshut\archives.h"         // archiving declarations
#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// headers in /hook
#include "hook\xwphook.h"

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise
#include "xfobj.h"
#include "xfldr.h"

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static BOOL    G_DesktopPopulated = FALSE;

/* ******************************************************************
 *
 *   here come the XFldDesktop instance methods
 *
 ********************************************************************/

/*
 *@@ xwpInsertXFldDesktopStartupPage:
 *      this actually adds the new "Startup" page replacement
 *      to the Desktop's settings notebook.
 *
 *      This gets called from XFldDesktop::wpAddSettingsPages.
 *
 *      added V0.9.0
 */

SOM_Scope ULONG  SOMLINK xfdesk_xwpInsertXFldDesktopStartupPage(XFldDesktop *somSelf,
                                                                HWND hwndNotebook)
{
    INSERTNOTEBOOKPAGE inbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpInsertXFldDesktopStartupPage");

    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndNotebook;
    inbp.hmod = savehmod;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_STARTUPPAGE);  // pszStartupPage
    inbp.ulDlgID = ID_XFD_EMPTYDLG; // ID_XSD_DTP_STARTUP; V0.9.16 (2001-10-08) [umoeller]
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_DTP_STARTUP;
    inbp.ulPageID = SP_DTP_STARTUP;
    inbp.pfncbInitPage    = dtpStartupInitPage;
    inbp.pfncbItemChanged = dtpStartupItemChanged;
    return ntbInsertPage(&inbp);
}

/*
 *@@ xwpInsertXFldDesktopArchivesPage:
 *      this actually adds the new "Archives" page replacement
 *      to the Desktop's settings notebook.
 *
 *      This gets called from XFldDesktop::wpAddSettingsPages.
 *
 *      added V0.9.0
 */

SOM_Scope ULONG  SOMLINK xfdesk_xwpInsertXFldDesktopArchivesPage(XFldDesktop *somSelf,
                                                                 HWND hwndNotebook)
{
    INSERTNOTEBOOKPAGE inbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpInsertXFldDesktopArchivesPage");

    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndNotebook;
    inbp.hmod = savehmod;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_ARCHIVESPAGE);  // pszArchivesPage
    inbp.ulDlgID = ID_XFD_EMPTYDLG;       // ID_XSD_DTP_ARCHIVES; V0.9.16 (2001-11-22) [umoeller]
    // inbp.usFirstControlID = ID_SDDI_ARCHIVES;
    // inbp.ulFirstSubpanel = ID_XSH_SETTINGS_DTP_SHUTDOWN_SUB;   // help panel for "System setup"
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_DTP_ARCHIVES;
    inbp.ulPageID = SP_DTP_ARCHIVES;
    inbp.pfncbInitPage    = arcArchivesInitPage;
    inbp.pfncbItemChanged = arcArchivesItemChanged;
    return ntbInsertPage(&inbp);
}

/*
 *@@ xwpInsertXFldDesktopShutdownPage:
 *      this actually adds the new "Shutdown" page replacement
 *      to the Desktop's settings notebook.
 *
 *      This gets called from XFldDesktop::wpAddSettingsPages.
 *
 *      added V0.9.0
 */

SOM_Scope ULONG  SOMLINK xfdesk_xwpInsertXFldDesktopShutdownPage(XFldDesktop *somSelf,
                                                                 HWND hwndNotebook)
{
    ULONG ulrc = 0;

#ifndef __NOXSHUTDOWN__
    INSERTNOTEBOOKPAGE inbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpInsertXFldDesktopShutdownPage");

    // insert "XShutdown" page,
    // if Shutdown has been enabled in XWPConfig
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndNotebook;
    inbp.hmod = savehmod;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_XSHUTDOWNPAGE);  // pszXShutdownPage
    // inbp.ulDlgID = ID_XSD_DTP_SHUTDOWN;
    inbp.ulDlgID = ID_XFD_EMPTYDLG;           // V0.9.16 (2001-09-29) [umoeller]
    // inbp.usFirstControlID = ID_SDDI_REBOOT;
    // inbp.ulFirstSubpanel = ID_XSH_SETTINGS_DTP_SHUTDOWN_SUB;   // help panel for "System setup"
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_DTP_SHUTDOWN;
    inbp.ulPageID = SP_DTP_SHUTDOWN;
    inbp.pfncbInitPage    = xsdShutdownInitPage;
    inbp.pfncbItemChanged = xsdShutdownItemChanged;
    ulrc = ntbInsertPage(&inbp);
#endif

    return ulrc;
}

/*
 *@@ xwpQuerySetup2:
 *      this XFldObject method is overridden to support
 *      setup strings for the Desktop.
 *
 *      See XFldObject::xwpQuerySetup2 for details.
 *
 *@@added V0.9.1 (2000-01-08) [umoeller]
 *@@changed V0.9.16 (2001-10-11) [umoeller]: adjusted to new implementation
 *@@changed V1.0.1 (2002-12-08) [umoeller]: now calling parent methods directly
 */

SOM_Scope BOOL  SOMLINK xfdesk_xwpQuerySetup2(XFldDesktop *somSelf,
                                              PVOID pstrSetup)
{
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpQuerySetup2");

    // call XFldDesktop implementation
    if (dtpQuerySetup(somSelf, pstrSetup))
        return parent_xwpQuerySetup2(somSelf,
                                     pstrSetup);

    return FALSE;
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

SOM_Scope void  SOMLINK xfdesk_wpInitData(XFldDesktop *somSelf)
{
    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpInitData");

    XFldDesktop_parent_WPDesktop_wpInitData(somSelf);

    _fOpened = FALSE;
}

/*
 *@@ wpSetup:
 *      this WPObject instance method is called to allow an
 *      object to set itself up according to setup strings.
 *      As opposed to wpSetupOnce, this gets called any time
 *      a setup string is invoked.
 *
 *      We support new setup strings for XFldDesktop, which
 *      are parsed here. This calls dtpSetup for the implementation.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpSetup(XFldDesktop *somSelf,
                                       PSZ pszSetupString)
{
    BOOL brc = FALSE;
    /* XFldDesktopData *somThis = XFldDesktopGetData(somSelf); */
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpSetup");

    brc = XFldDesktop_parent_WPDesktop_wpSetup(somSelf, pszSetupString);

    dtpSetup(somSelf, pszSetupString);

    return brc;
}

/*
 *@@ wpSetupOnce:
 *      this WPObject method allows special object handling
 *      based on a creation setup string after an object has
 *      been fully created.
 *      As opposed to WPObject::wpSetup, this method _only_
 *      gets called during object creation. The WPObject
 *      implementation calls wpSetup in turn.
 *      If FALSE is returned, object creation is aborted.
 *
 *      We now make sure that ALWAYSSORT=NO is always part
 *      of the setup string.
 *
 *@@added V0.9.20 (2002-07-12) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpSetupOnce(XFldDesktop *somSelf,
                                           PSZ pszSetupString)
{
    BOOL    brc;
    CHAR    szDummy[50];
    ULONG   cbValue;
    PSZ     pszHackedSetupString = NULL;

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpSetupOnce");

    // if a desktop is newly created during eCS installation,
    // MAKE SURE IT HAS ALWAYSSORT=NO because uses might still
    // have a leftover global alwayssort setting from XWorkplace
    // V0.9.20 (2002-07-12) [umoeller]
    cbValue = sizeof(szDummy);
    if (!_wpScanSetupString(somSelf,
                            pszSetupString,
                            "ALWAYSSORT",
                            szDummy,
                            &cbValue))
    {
        const char ADDTHIS[] = "ALWAYSSORT=NO;";
        #define ADDTHISLEN (sizeof(ADDTHIS) - 1)

        if (    (pszSetupString)
             && (*pszSetupString)
           )
        {
            ULONG ulSetupStringLen = strlen(pszSetupString);
            if (pszHackedSetupString = malloc(   ulSetupStringLen
                                               + sizeof(ADDTHIS)))
            {
                memcpy(pszHackedSetupString,
                       ADDTHIS,
                       ADDTHISLEN);
                memcpy(pszHackedSetupString + ADDTHISLEN,
                       pszSetupString,
                       ulSetupStringLen + 1);
                pszSetupString = pszHackedSetupString;
            }
        }
        else
            pszSetupString = (PSZ)ADDTHIS;

    }

    brc = XFldDesktop_parent_WPDesktop_wpSetupOnce(somSelf,
                                                   pszSetupString);

    if (pszHackedSetupString)
        free(pszHackedSetupString);

    return brc;
}

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object. This should hand out a help
 *      panel describing what this object can do in general
 *      and return TRUE.
 *
 *      See XFldObject::wpQueryDefaultHelp for general remarks
 *      for how the method works.
 *
 *      We return a different default help for non-default
 *      desktops.
 *
 *@@added V0.9.20 (2002-07-12) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpQueryDefaultHelp(XFldDesktop *somSelf,
                                                  PULONG pHelpPanelId,
                                                  PSZ HelpLibrary)
{
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpQueryDefaultHelp");

    if (_wpclsQueryActiveDesktop(_WPDesktop) != somSelf)
    {
        strcpy(HelpLibrary, cmnQueryHelpLibrary());
        *pHelpPanelId = ID_XSH_DESKTOP_SECONDARY;
        return TRUE;
    }

    return XFldDesktop_parent_WPDesktop_wpQueryDefaultHelp(somSelf,
                                                           pHelpPanelId,
                                                           HelpLibrary);
}

/*
 *@@ wpSetDefaultView:
 *      this WPObject method changes the default view of
 *      the object.
 *
 *      Since we now use this method to allow the user
 *      to change the default view via shift+click on
 *      a menu item in the "Open" submenu, we better
 *      make sure that the user won't change the Desktop's
 *      default view to anything but icon, details, or
 *      tree. Those three work, but settings or split
 *      view won't.
 *
 *@@added V1.0.0 (2002-09-13) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpSetDefaultView(XFldDesktop *somSelf,
                                                ULONG ulView)
{
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpSetDefaultView");

    switch (ulView)
    {
        case OPEN_SETTINGS:
        case OPEN_DETAILS:
        case OPEN_TREE:
            return XFldDesktop_parent_WPDesktop_wpSetDefaultView(somSelf,
                                                                 ulView);
    }

    return FALSE;
}

/*
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 *
 *      We remove "Create another" for Desktop, because
 *      we don't want to allow creating another Desktop.
 *      For some reason, the "Create another" option
 *      doesn't seem to be working right with XFolder,
 *      so we need to add all this manually (see
 *      XFldObject::wpFilterPopupMenu).
 *
 *@@changed V0.9.19 (2002-04-17) [umoeller]: adjusted for new menu handling
 */

SOM_Scope ULONG  SOMLINK xfdesk_wpFilterPopupMenu(XFldDesktop *somSelf,
                                                  ULONG ulFlags,
                                                  HWND hwndCnr,
                                                  BOOL fMultiSelect)
{
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpFilterPopupMenu");

    return (XFldDesktop_parent_WPDesktop_wpFilterPopupMenu(somSelf,
                                                           ulFlags,
                                                           hwndCnr,
                                                           fMultiSelect)
            & ~(    cmnQuerySetting(sflMenuDesktopWPS)
                  | CTXT_CRANOTHER      // always filter this one out
           ));
}

/*
 *@@ wpModifyPopupMenu:
 *      this WPObject instance methods gets called by the WPS
 *      when a context menu needs to be built for the object
 *      and allows the object to manipulate its context menu.
 *      This gets called _after_ wpFilterPopupMenu.
 *
 *      We play with the Desktop menu entries
 *      (Shutdown and such)
 *
 *@@changed V0.9.0 [umoeller]: reworked context menu items
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpModifyPopupMenu(XFldDesktop *somSelf,
                                                 HWND hwndMenu,
                                                 HWND hwndCnr,
                                                 ULONG iPosition)
{
    BOOL rc;

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpModifyPopupMenu");

    // calling the parent (which is XFolder!) will insert all the
    // variable menu items
    if (rc = XFldDesktop_parent_WPDesktop_wpModifyPopupMenu(somSelf,
                                                            hwndMenu,
                                                            hwndCnr,
                                                            iPosition))
        if (_wpIsCurrentDesktop(somSelf))
            dtpModifyPopupMenu(somSelf,
                               hwndMenu);

    return rc;
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This must be overridden to support new menu
 *      items which have been added in wpModifyPopupMenu.
 *
 *      See XFldObject::wpMenuItemSelected for additional
 *      remarks.
 *
 *      We call dtpMenuItemSelected to process desktop
 *      menu items here.
 *
 *@@changed V0.9.3 (2000-04-26) [umoeller]: now allowing dtpMenuItemSelected to change the menu ID
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpMenuItemSelected(XFldDesktop *somSelf,
                                                  HWND hwndFrame,
                                                  ULONG ulMenuId)
{
    ULONG   ulMenuId2 = ulMenuId;
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpMenuItemSelected");

    if (dtpMenuItemSelected(somSelf, hwndFrame, &ulMenuId2))
        // one of the new items processed:
        return TRUE;

    return XFldDesktop_parent_WPDesktop_wpMenuItemSelected(somSelf,
                                                           hwndFrame,
                                                           ulMenuId2);
}

/*
 *@@ wpOpen:
 *      this WPObject instance method gets called when
 *      a new view needs to be opened. Normally, this
 *      gets called after wpViewObject has scanned the
 *      object's USEITEMs and has determined that a new
 *      view is needed.
 *
 *      This _normally_ runs on thread 1 of the WPS, but
 *      this is not always the case. If this gets called
 *      in response to a menu selection from the "Open"
 *      submenu or a double-click in the folder, this runs
 *      on the thread of the folder (which _normally_ is
 *      thread 1). However, if this results from WinOpenObject
 *      or an OPEN setup string, this will not be on thread 1.
 *
 *      We override this to intercept the opening of the
 *      first desktop of the system so that we can do
 *      pre-populate. This was done in XFolder::wpOpen
 *      with V0.9.18 (where the feature was introduced),
 *      but this broke the pager.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 *@@changed V0.9.19 (2002-04-02) [umoeller]: moved daemon notification here to fix race
 */

SOM_Scope HWND  SOMLINK xfdesk_wpOpen(XFldDesktop *somSelf, HWND hwndCnr,
                                      ULONG ulView, ULONG param)
{
    HWND        hwndReturn;
    static      s_fDesktopOpened = FALSE;
    BOOL        fWasFirstOpen = FALSE;

    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpOpen");

    initLog("Entering " __FUNCTION__"...");

    if (!s_fDesktopOpened)
    {
        // if this is the first desktop ever opened, it
        // must be the default desktop...
        // pre-populate to avoid hangs on startup
        s_fDesktopOpened = TRUE;
        fWasFirstOpen = TRUE;

#ifndef __NEVERCHECKDESKTOP__
        if (cmnQuerySetting(sfPrePopulateDesktop))
        {
            WPObject *pobj;

            HPOINTER hptrOld = winhSetWaitPointer();

            initLog("  pre-populating");

            fdrCheckIfPopulated(somSelf,
                                FALSE);     // not folders only
            for (pobj = _wpQueryContent(somSelf, NULL, (ULONG)QC_FIRST);
                 pobj;
                 pobj = *__get_pobjNext(pobj))
            {
                _wpQueryIcon(pobj);
            }

            WinSetPointer(HWND_DESKTOP, hptrOld);
        }
        else
            initLog("  pre-populate disabled");
#endif
    }

    // store in instance data so wpPopulate can check
    // for pager fix
    _fOpened = TRUE;

    hwndReturn  = XFldDesktop_parent_WPDesktop_wpOpen(somSelf, hwndCnr,
                                               ulView, param);

    initLog("Leaving " __FUNCTION__", returning HWND 0x%lX", hwndReturn);

    if (fWasFirstOpen)
    {
        // V0.9.19 (2002-04-02) [umoeller]
        // moved daemon notification here from
        // startup thread to fix a race
        krnPostDaemonMsg(XDM_DESKTOPREADY,
                         (MPARAM)hwndReturn,
                         (MPARAM)0);
        initLog("  posted desktop HWND 0x%lX to daemon", hwndReturn);
    }

    return hwndReturn;
}

/*
 *@@ wpPopulate:
 *      this instance method populates a folder, in this case, the
 *      Desktop. After the active Desktop has been populated at
 *      Desktop startup, we'll post a message to the Worker thread to
 *      initiate all the XWorkplace startup processing.
 *
 *@@changed V0.9.5 (2000-08-26) [umoeller]: this was previously done in wpOpen
 *@@changed V0.9.19 (2002-04-02) [umoeller]: added startup logging
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpPopulate(XFldDesktop *somSelf,
                                          ULONG ulReserved,
                                          PSZ pszPath,
                                          BOOL fFoldersOnly)
{
    BOOL    brc = FALSE;

    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpPopulate");

    PMPF_STARTUP(("entering"));

    initLog("Entering " __FUNCTION__", calling parent WPDesktop::wpPopulate...");

    brc = XFldDesktop_parent_WPDesktop_wpPopulate(somSelf,
                                                  ulReserved,
                                                  pszPath,
                                                  fFoldersOnly);

    initLog("  parent WPDesktop::wpPopulate returned %d", brc);

    PMPF_STARTUP(("checking whether Worker thread needs notify"));

    if (    (!G_DesktopPopulated)
         && (_fOpened)                      // avoid the pre-populate, wait until it's open!
                                            // V0.9.19 (2002-04-02) [umoeller]
         && (_wpIsCurrentDesktop(somSelf))
       )
    {
        // first call:
        G_DesktopPopulated = TRUE;

        initLog("  first desktop populate after open, posting FIM_DESKTOPPOPULATED");

        PMPF_STARTUP(("posting FIM_DESKTOPPOPULATED"));

        xthrPostFileMsg(FIM_DESKTOPPOPULATED,
                        (MPARAM)somSelf,
                        0);
    }

    PMPF_STARTUP(("leaving"));

    initLog("Leaving " __FUNCTION__);

    return brc;
}

/*
 *@@ wpAddDesktopArcRest1Page:
 *      this instance method inserts the "Archives" page
 *      into the Desktop's settings notebook. If the
 *      extended archiving has been enabled, we return
 *      SETTINGS_PAGE_REMOVED because we want the "Archives"
 *      page at a different position, which gets inserted
 *      from XFldDesktop::wpAddSettingsPages then.
 *
 *@@added V0.9.0 [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfdesk_wpAddDesktopArcRest1Page(XFldDesktop *somSelf,
                                                         HWND hwndNotebook)
{
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpAddDesktopArcRest1Page");

#ifndef __ALWAYSREPLACEARCHIVING__
    if (!cmnQuerySetting(sfReplaceArchiving))
        return XFldDesktop_parent_WPDesktop_wpAddDesktopArcRest1Page(somSelf,
                                                                     hwndNotebook);
#endif

    // remove this, we'll add a new one at a different
    // location in wpAddSettingsPages
    return SETTINGS_PAGE_REMOVED;
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook.
 *
 *      As opposed to the "XFolder" page, which deals with instance
 *      data, we save the Desktop settings with the global
 *      settings, because there should ever be only one active
 *      Desktop.
 *
 *@@changed V0.9.0 [umoeller]: reworked settings pages
 *@@changed V0.9.19 (2002-04-17) [umoeller]: removed menu page, this is now handled by XFldWPS
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpAddSettingsPages(XFldDesktop *somSelf,
                                                  HWND hwndNotebook)
{
    BOOL            rc;

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpAddSettingsPages");

    if (    (rc = XFldDesktop_parent_WPDesktop_wpAddSettingsPages(somSelf,
                                                                 hwndNotebook))
         && (_wpIsCurrentDesktop(somSelf))
       )
    {
        // insert "Menu" page
        // _xwpInsertXFldDesktopMenuItemsPage(somSelf, hwndNotebook);
                // V0.9.19 (2002-04-17) [umoeller]


#ifndef __NOXSHUTDOWN__
        if (cmnQuerySetting(sfXShutdown))
            _xwpInsertXFldDesktopShutdownPage(somSelf, hwndNotebook);
#endif

#ifndef __ALWAYSREPLACEARCHIVING__
        if (cmnQuerySetting(sfReplaceArchiving))
#endif
            // insert new "Archives" page;
            // at the same time, the old archives page method
            // will return SETTINGS_PAGE_REMOVED
            _xwpInsertXFldDesktopArchivesPage(somSelf, hwndNotebook);

        // insert "Startup" page
        _xwpInsertXFldDesktopStartupPage(somSelf, hwndNotebook);
    }

    return rc;
}

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfdeskM_wpclsInitData(M_XFldDesktop *somSelf)
{
    // M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf);
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsInitData");

    M_XFldDesktop_parent_M_WPDesktop_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXFldDesktop);
        // first call: check if the desktop is valid...
        // moved this to WPProgram because that is init'd later
        // V0.9.17 (2002-02-05) [umoeller]
        // initRepairDesktopIfBroken();
}

/*
 *@@ wpclsQuerySettingsPageSize:
 *      this WPObject class method should return the
 *      size of the largest settings page in dialog
 *      units; if a settings notebook is initially
 *      opened, i.e. no window pos has been stored
 *      yet, the WPS will use this size, to avoid
 *      truncated settings pages.
 */

SOM_Scope BOOL  SOMLINK xfdeskM_wpclsQuerySettingsPageSize(M_XFldDesktop *somSelf,
                                                           PSIZEL pSizl)
{
    BOOL brc;
    /* M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf); */
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsQuerySettingsPageSize");

    brc = M_XFldDesktop_parent_M_WPDesktop_wpclsQuerySettingsPageSize(somSelf,
                                                                        pSizl);
    if (brc)
    {
        LONG lCompCY = 153; // this is the height of the "XDesktop" page,
                            // which is pretty large
        if (G_fIsWarp4)
            // on Warp 4, reduce again, because we're moving
            // the notebook buttons to the bottom
            lCompCY -= WARP4_NOTEBOOK_OFFSET;

        if (pSizl->cy < lCompCY)
            pSizl->cy = lCompCY;
        if (pSizl->cx < 260)
            pSizl->cx = 260;    // and the width

    }

    return brc;
}

/*
 *@@ wpclsQueryIconData:
 *      this WPObject class method must return information
 *      about how to build the default icon for objects
 *      of a class. This gets called from various other
 *      methods whenever a class default icon is needed;
 *      most importantly, M_WPObject::wpclsQueryIcon
 *      calls this to build a class default icon, which
 *      is then cached in the class's instance data.
 *      If a subclass wants to change a class default icon,
 *      it should always override _this_ method instead of
 *      wpclsQueryIcon.
 *
 *      Note that the default WPS implementation does not
 *      allow for specifying the ICON_FILE format here,
 *      which is why we have overridden
 *      M_XFldObject::wpclsQueryIcon too. This allows us
 *      to return icon _files_ for theming too. For details
 *      about the WPS's crappy icon management, refer to
 *      src\filesys\icons.c.
 *
 *      We give XFldDesktop's a new default closed icon, if the
 *      global settings allow this.
 */

SOM_Scope ULONG  SOMLINK xfdeskM_wpclsQueryIconData(M_XFldDesktop *somSelf,
                                                    PICONINFO pIconInfo)
{
    // M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf);
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsQueryIconData");

#ifndef __NOICONREPLACEMENTS__
    if (cmnQuerySetting(sfIconReplacements))
    {
        /* hmodIconsDLL = cmnQueryIconsDLL();
        // icon replacements allowed:
        if ((pIconInfo) && (hmodIconsDLL))
        {
            pIconInfo->fFormat = ICON_RESOURCE;
            pIconInfo->hmod = hmodIconsDLL;
            pIconInfo->resid = 110;
        }*/

        // now using cmnGetStandardIcon
        // V0.9.16 (2002-01-13) [umoeller]
        ULONG cb = 0;
        if (!cmnGetStandardIcon(STDICON_DESKTOP_CLOSED,
                                NULL,            // no hpointer
                                &cb,
                                pIconInfo))      // fill icon info
            return cb;

        return 0;

    }
#endif

    // icon replacements not allowed: call default
    return M_XFldDesktop_parent_M_WPDesktop_wpclsQueryIconData(somSelf,
                                                               pIconInfo);
}

/*
 *@@ wpclsQueryIconDataN:
 *      give XFldDesktop's a new open closed icon, if the
 *      global settings allow this.
 *      This is loaded from /ICONS/ICONS.DLL.
 */

SOM_Scope ULONG  SOMLINK xfdeskM_wpclsQueryIconDataN(M_XFldDesktop *somSelf,
                                                     ICONINFO* pIconInfo,
                                                     ULONG ulIconIndex)
{
    // M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf);
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsQueryIconDataN");

#ifndef __NOICONREPLACEMENTS__
    if (cmnQuerySetting(sfIconReplacements))
    {
        /* hmodIconsDLL = cmnQueryIconsDLL();
        // icon replacements allowed:
        if ((pIconInfo) && (hmodIconsDLL))
        {
            pIconInfo->fFormat = ICON_RESOURCE;
            pIconInfo->hmod = hmodIconsDLL;
            pIconInfo->resid = 111;
        }*/

        // now using cmnGetStandardIcon
        // V0.9.16 (2002-01-13) [umoeller]
        ULONG cb = 0;
        if (!cmnGetStandardIcon(STDICON_DESKTOP_OPEN,
                                NULL,            // no hpointer
                                &cb,
                                pIconInfo))      // fill icon info
            return cb;

        return 0;
    }

#endif
    // icon replacements not allowed: call default
    return M_XFldDesktop_parent_M_WPDesktop_wpclsQueryIconDataN(somSelf,
                                                                pIconInfo,
                                                                ulIconIndex);
}

