
/*
 *@@sourcefile xwpscreen.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPScreen (WPSystem subclass)
 *
 *      XFldSystem implements the "Screen" settings object
 *      for PM manipulation and XPager configuration.
 *
 *      Installation of this class is optional, but you cannot
 *      configure XPager without it.
 *
 *      This is all new with V0.9.2 (2000-02-23) [umoeller].
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      the SOM interface, i.e. the methods themselves.
 *      The implementation for this class is mostly in config\pager.c.
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 *@@somclass XWPScreen xwpscr_
 *@@somclass M_XWPScreen xwpscrM_
 */

/*
 *      Copyright (C) 2000 Ulrich M�ller.
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

#ifndef SOM_Module_xwpscreen_Source
#define SOM_Module_xwpscreen_Source
#endif
#define XWPScreen_Class_Source
#define M_XWPScreen_Class_Source

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
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#include <os2.h>

// C library headers

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines

// SOM headers which don't crash with prec. header files
#include "xwpscreen.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\hookintf.h"            // daemon/hook interface
#include "config\pager.h"            // XPager interface

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   here come the XWPScreen instance methods
 *
 ********************************************************************/

/*
 *@@ xwpAddXWPScreenPages:
 *      adds the "XPager" pages to the "Screen" notebook.
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 *@@changed V0.9.9 (2001-03-15) [lafaix]: added a new 'pager window' page
 *@@changed V0.9.9 (2001-03-27) [umoeller]: moved "Corners" from XWPMouse to XWPScreen
 *@@changed V0.9.9 (2001-04-04) [lafaix]: renamed to "Screen borders" page
 *@@changed V0.9.19 (2002-05-28) [umoeller]: adjusted for pager rework
 */

SOM_Scope ULONG  SOMLINK xwpscr_xwpAddXWPScreenPages(XWPScreen *somSelf,
                                                     HWND hwndDlg)
{
    ULONG ulrc = 0;

    /* XWPScreenData *somThis = XWPScreenGetData(somSelf); */
    XWPScreenMethodDebug("XWPScreen","xwpscr_xwpAddXWPScreenPages");

    // hook installed?
    if (hifXWPHookReady())
    {
        INSERTNOTEBOOKPAGE  inbp;
        HMODULE             savehmod = cmnQueryNLSModuleHandle(FALSE);

        // moved this here from "Mouse" V0.9.9 (2001-03-27) [umoeller]
        memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
        inbp.somSelf = somSelf;
        inbp.hwndNotebook = hwndDlg;
        inbp.hmod = savehmod;
        inbp.ulDlgID = ID_XSD_MOUSE_CORNERS;
        inbp.usPageStyleFlags = BKA_MAJOR;
        inbp.pcszName = cmnGetString(ID_XSSI_SCREENBORDERSPAGE);  // pszScreenBordersPage
        // inbp.fEnumerate = TRUE;
        inbp.ulDefaultHelpPanel  = ID_XSH_MOUSE_CORNERS;
        inbp.ulPageID = SP_MOUSE_CORNERS;
        inbp.pfncbInitPage    = hifMouseCornersInitPage;
        inbp.pfncbItemChanged = hifMouseCornersItemChanged;
        ulrc = ntbInsertPage(&inbp);

#ifndef __NOPAGER__
        // if (cmnQuerySetting(sfEnableXPager)) always insert, even if
                // pager is disabled V0.9.19 (2002-05-28) [umoeller]
        {
            // moved all this to pager.c
            ulrc = pgmiInsertPagerPages(somSelf, hwndDlg, savehmod);
        }
#endif
    }

    return ulrc;
}

/*
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 *
 *      We remove the "Create another" menu item.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xwpscr_wpFilterPopupMenu(XWPScreen *somSelf,
                                                  ULONG ulFlags,
                                                  HWND hwndCnr,
                                                  BOOL fMultiSelect)
{
    /* XWPScreenData *somThis = XWPScreenGetData(somSelf); */
    XWPScreenMethodDebug("XWPScreen","xwpscr_wpFilterPopupMenu");

    return (XWPScreen_parent_WPSystem_wpFilterPopupMenu(somSelf,
                                                        ulFlags,
                                                        hwndCnr,
                                                        fMultiSelect)
            & ~CTXT_NEW
           );
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook.
 *
 *      In order to to this, unlike the procedure used in
 *      the "Workplace Shell" object, we will explicitly
 *      call the WPSystem methods which insert the
 *      pages we want to see here into the notebook.
 *      As a result, the "Workplace Shell" object
 *      inherits all pages from the "System" object
 *      which might be added by other WPS utils, while
 *      this thing does not.
 *
 */

SOM_Scope BOOL  SOMLINK xwpscr_wpAddSettingsPages(XWPScreen *somSelf,
                                                  HWND hwndNotebook)
{
    /* XWPScreenData *somThis = XWPScreenGetData(somSelf); */
    XWPScreenMethodDebug("XWPScreen","xwpscr_wpAddSettingsPages");

    // do _not_ call the parent, but call the page methods
    // explicitly

    // XFolder "Internals" page bottommost
    // _xwpAddObjectInternalsPage(somSelf, hwndNotebook);

    // "Symbol" page next
    _wpAddObjectGeneralPage(somSelf, hwndNotebook);
        // this inserts the "Internals"/"Object" page now

    // "print screen" page next
    _wpAddSystemPrintScreenPage(somSelf, hwndNotebook);

    // XWorkplace screen pages
    _xwpAddXWPScreenPages(somSelf, hwndNotebook);

    // "Screen" page 2 next; this page may exist on some systems
    // depending on the video driver, and we want this in "Screen"
    // also
    _wpAddDMQSDisplayTypePage(somSelf, hwndNotebook);
    // "Screen" page 1 next
    _wpAddSystemScreenPage(somSelf, hwndNotebook);

    return TRUE;
}

/* ******************************************************************
 *
 *   here come the XWPScreen class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 */

SOM_Scope void  SOMLINK xwpscrM_wpclsInitData(M_XWPScreen *somSelf)
{
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsInitData");

    M_XWPScreen_parent_M_WPSystem_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXWPScreen);
}

/*
 *@@ wpclsQueryTitle:
 *      this WPObject class method tells the WPS the clear
 *      name of a class, which is shown in the third column
 *      of a Details view and also used as the default title
 *      for new objects of a class.
 */

SOM_Scope PSZ  SOMLINK xwpscrM_wpclsQueryTitle(M_XWPScreen *somSelf)
{
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsQueryTitle");

    return (PSZ)cmnGetString(ID_XSSI_XWPSCREENTITLE);
}

/*
 *@@ wpclsQueryDefaultHelp:
 *      this WPObject class method returns the default help
 *      panel for objects of this class. This gets called
 *      from WPObject::wpQueryDefaultHelp if no instance
 *      help settings (HELPLIBRARY, HELPPANEL) have been
 *      set for an individual object. It is thus recommended
 *      to override this method instead of the instance
 *      method to change the default help panel for a class
 *      in order not to break instance help settings (fixed
 *      with 0.9.20).
 *
 *      We return the default help for the "Screen"
 *      object here.
 *
 *@@added V0.9.20 (2002-07-12) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xwpscrM_wpclsQueryDefaultHelp(M_XWPScreen *somSelf,
                                                      PULONG pHelpPanelId,
                                                      PSZ pszHelpLibrary)
{
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsQueryDefaultHelp");

    strcpy(pszHelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XSH_XWPSCREEN;
    return TRUE;
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
 *      We give the "Screen" object a new standard icon here.
 */

SOM_Scope ULONG  SOMLINK xwpscrM_wpclsQueryIconData(M_XWPScreen *somSelf,
                                                    PICONINFO pIconInfo)
{
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsQueryIconData");

    if (pIconInfo)
    {
        pIconInfo->fFormat = ICON_RESOURCE;
        pIconInfo->resid   = ID_ICONXWPSCREEN;
        pIconInfo->hmod    = cmnQueryMainResModuleHandle();
    }

    return sizeof(ICONINFO);
}

/*
 *@@ wpclsQuerySettingsPageSize:
 *      this WPObject class method should return the
 *      size of the largest settings page in dialog
 *      units; if a settings notebook is initially
 *      opened, i.e. no window pos has been stored
 *      yet, the WPS will use this size, to avoid
 *      truncated settings pages.
 *
 *@@added V0.9.5 (2000-08-26) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xwpscrM_wpclsQuerySettingsPageSize(M_XWPScreen *somSelf,
                                                           PSIZEL pSizl)
{
    BOOL brc = FALSE;
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsQuerySettingsPageSize");

    brc = M_XWPScreen_parent_M_WPSystem_wpclsQuerySettingsPageSize(somSelf,
                                                                   pSizl);
    if (brc)
    {
        LONG lCompCY = 160;     // this is the height of the "XPager General" page
                                // which seems to be the largest
        if (G_fIsWarp4)
            // on Warp 4, reduce again, because we're moving
            // the notebook buttons to the bottom
            lCompCY -= WARP4_NOTEBOOK_OFFSET;
        if (pSizl->cy < lCompCY)
            pSizl->cy = lCompCY;
    }

    return brc;
}


