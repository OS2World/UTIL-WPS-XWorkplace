
/*
 *@@sourcefile xtrash.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPFontFolder: a subclass of WPFolder, which implements
 *                  a "font folder".
 *
 *@@added V0.9.7 (2001-01-12) [umoeller]
 *@@somclass XWPFontFolder fon_
 *@@somclass M_XWPFontFolder fonM_
 */

/*
 *      Copyright (C) 2001-2003 Ulrich M�ller.
 *
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

#ifndef SOM_Module_xfont_Source
#define SOM_Module_xfont_Source
#endif
#define XWPFontFolder_Class_Source
#define M_XWPFontFolder_Class_Source

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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling

// SOM headers which don't crash with prec. header files
#include "xfont.ih"
#include "xfontobj.ih"
// #include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\fonts.h"               // font folder implementation

#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// default font folder
// static XWPFontFolder *G_pDefaultFontFolder = NULL;

/* ******************************************************************
 *
 *   XWPFontFolder instance methods
 *
 ********************************************************************/

/*
 *@@ xwpAddFontsPage:
 *
 */

SOM_Scope ULONG  SOMLINK fon_xwpAddFontsPage(XWPFontFolder *somSelf,
                                             HWND hwndDlg)
{
    INSERTNOTEBOOKPAGE inbp;

    // XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_xwpAddFontsPage");

    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = cmnQueryNLSModuleHandle(FALSE);
    inbp.ulDlgID = ID_FND_SAMPLETEXT;
    inbp.ulPageID = SP_FONT_SAMPLETEXT;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_FONTSAMPLEVIEW);  // pszFontSampleView
    inbp.ulDefaultHelpPanel  = ID_XSH_FONTFOLDER_TEXT;
    inbp.pfncbInitPage    = fonSampleTextInitPage;
    inbp.pfncbItemChanged = fonSampleTextItemChanged;
    return ntbInsertPage(&inbp);
}

/*
 *@@ xwpChangeFontsCount:
 *      changes the fonts count and refreshes the status bar.
 *
 *      This gets called when a font object is created, or
 *      later for manual uninstalls, to refresh the count of
 *      installed fonts properly, which wasn't working before
 *      0.9.20.
 *
 *@@added V0.9.20 (2002-07-25) [umoeller]
 */

SOM_Scope BOOL  SOMLINK fon_xwpChangeFontsCount(XWPFontFolder *somSelf,
                                                long i)
{
    BOOL brc = FALSE,
         fLocked = FALSE;
    XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_xwpChangeFontsCount");

    TRY_LOUD(excpt1)
    {
        if (fLocked = !_wpRequestObjectMutexSem(somSelf, SEM_INDEFINITE_WAIT))
        {
            _ulFontsCurrent += i;

            // update _ulFontsMax for deinstalls also, or the status bar will boom
            if (i < 0)
                _ulFontsMax += i;

            stbUpdate(somSelf);

            brc = TRUE;
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (fLocked)
        _wpReleaseObjectMutexSem(somSelf);

    return brc;
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 */

SOM_Scope void  SOMLINK fon_wpInitData(XWPFontFolder *somSelf)
{
    XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_wpInitData");

    XWPFontFolder_parent_XFolder_wpInitData(somSelf);

    _fFilledWithFonts = FALSE;      // attribute

    _ulFontsCurrent = 0;
    _ulFontsMax = 0;

    // tell XFolder to override wpAddToContent...
    _xwpSetDisableCnrAdd(somSelf, TRUE);
}

/*
 *@@ xwpProcessViewCommand:
 *      this XFolder method processes WM_COMMAND messages
 *      for objects in a container. For details refer to
 *      XFolder::xwpProcessViewCommand.
 *
 *      Starting with V1.0.1, we can now really override
 *      this method through our optimized IDL files.
 *
 *@@added V1.0.1 (2002-12-08) [umoeller]
 */

SOM_Scope BOOL  SOMLINK fon_xwpProcessViewCommand(XWPFontFolder *somSelf,
                                                  USHORT usCommand,
                                                  HWND hwndCnr,
                                                  WPObject* pFirstObject,
                                                  ULONG ulSelectionFlags)
{
    XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_xwpProcessViewCommand");

    if (fonProcessViewCommand(somSelf,
                              usCommand,
                              hwndCnr,
                              pFirstObject,
                              ulSelectionFlags))
        return TRUE;

    return XWPFontFolder_parent_XFolder_xwpProcessViewCommand(somSelf,
                                                              usCommand,
                                                              hwndCnr,
                                                              pFirstObject,
                                                              ulSelectionFlags);
}

/*
 *@@ xwpUpdateStatusBar:
 *      this XFolder instance method gets called when the status
 *      bar needs updating.
 *
 *      This always gets called using name-lookup resolution, so
 *      XFolder does not have to be installed for this to work.
 *      However, if it is, this method will be called. See
 *      XFolder::xwpUpdateStatusBar for more on this.
 */

SOM_Scope BOOL  SOMLINK fon_xwpUpdateStatusBar(XWPFontFolder *somSelf,
                                               HWND hwndStatusBar,
                                               HWND hwndCnr)
{
    CHAR szText[200];
    XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_xwpUpdateStatusBar");

    if (_ulFontsCurrent < _ulFontsMax)
    {
        // populating and not done yet:
        sprintf(szText, "Collecting fonts... %u out of %d done",
                _ulFontsCurrent, _ulFontsMax);
            // @@todo localize
    }
    else
        sprintf(szText, "%d fonts installed.", _ulFontsCurrent);

    return WinSetWindowText(hwndStatusBar, szText);

    /* return (XWPFontFolder_parent_XFolder_xwpUpdateStatusBar(somSelf,
                                                            hwndStatusBar,
                                                            hwndCnr));
                                                            */
}

/*
 *@@ wpPopulate:
 *      this instance method populates a folder, in this
 *      case, the font folder.
 *
 *@@changed V0.9.9 (2001-03-11) [umoeller]: fFoldersOnly wasn't respected, fixed.
 *@@changed V0.9.20 (2002-07-12) [umoeller]: finally requesting find sem properly
 */

SOM_Scope BOOL  SOMLINK fon_wpPopulate(XWPFontFolder *somSelf,
                                       ULONG ulReserved, PSZ pszPath,
                                       BOOL fFoldersOnly)
{
    BOOL    brc = TRUE;
    BOOL    fFindLocked = FALSE;

    XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_wpPopulate");

    TRY_LOUD(excpt1)
    {
        // request the find mutex to avoid weird behavior;
        // there can only be one populate at a time
        // V0.9.20 (2002-07-12) [umoeller]
        if (fFindLocked = !_wpRequestFindMutexSem(somSelf, SEM_INDEFINITE_WAIT))
        {
            brc = XWPFontFolder_parent_XFolder_wpPopulate(somSelf,
                                                          ulReserved,
                                                          pszPath,
                                                          fFoldersOnly);

            if (!_fFilledWithFonts)
            {
                // very first call:
                _ulFontsCurrent = 0;
                _ulFontsMax = 0;

                if (!fFoldersOnly)      // V0.9.9 (2001-03-11) [umoeller]
                {
                    // tell XFolder to allow wpAddToContent hacks...
                    _xwpSetDisableCnrAdd(somSelf, TRUE);

                    // now create font objects...
                    fonPopulateFirstTime(somSelf);
                    _fFilledWithFonts = TRUE;
                }
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fFindLocked)
        _wpReleaseFindMutexSem(somSelf);

    return brc;
}

/*
 *@@ wpDeleteContents:
 *      this WPFolder method gets called when a folder is
 *      being deleted to first delete the contents of a
 *      folder before the folder can be deleted. From my
 *      testing, BOTH WPFolder::wpDelete and WPFolder::wpFree
 *      call this method to nuke the folder contents.
 *
 *      Since we might have transients in here, we run
 *      into the same problems as with the trash can
 *      (see XWPTrashCan::wpDeleteContents for more).
 *      So again, we override this method and just invoke
 *      wpFree on all objects in the folder without further
 *      discussion.
 *
 *@@added V0.9.9 (2001-02-08) [umoeller]
 */

SOM_Scope ULONG  SOMLINK fon_wpDeleteContents(XWPFontFolder *somSelf,
                                              ULONG fConfirmations)
{
    ULONG ulrc = NO_DELETE;
    // XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_wpDeleteContents");

    /* return (XWPFontFolder_parent_WPFolder_wpDeleteContents(somSelf,
                                                           fConfirmations)); */

    // note that we do not populate the font folder first... if it
    // hasn't been populated, we shouldn't have a problem in the first
    // place, because there should only be font objects in there...
    // and we don't want to create all objects first and then delete
    // them again.
    if (fdrNukeContents(somSelf))
        ulrc = OK_DELETE;

    return ulrc;
}

/*
 *@@ wpDragOver:
 *      this instance method is called to inform the object
 *      that other objects are being dragged over it.
 *      This corresponds to the DM_DRAGOVER message received by
 *      the object.
 */

SOM_Scope MRESULT  SOMLINK fon_wpDragOver(XWPFontFolder *somSelf,
                                          HWND hwndCnr,
                                          PDRAGINFO pdrgInfo)
{
    // XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_wpDragOver");

    return fonDragOver(somSelf, pdrgInfo);
}

/*
 *@@ wpDrop:
 *      this instance method is called to inform an object that
 *      another object has been dropped on it.
 *      This corresponds to the DM_DROP message received by
 *      the object.
 */

SOM_Scope MRESULT  SOMLINK fon_wpDrop(XWPFontFolder *somSelf,
                                      HWND hwndCnr,
                                      PDRAGINFO pdrgInfo,
                                      PDRAGITEM pdrgItem)
{
    // XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_wpDrop");

    return fonDrop(somSelf, pdrgInfo);
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook. Override
 *      this method to add new settings pages to either the
 *      top or the bottom of notebooks of a given class.
 *
 *      We add the font folder settings pages.
 */

SOM_Scope BOOL  SOMLINK fon_wpAddSettingsPages(XWPFontFolder *somSelf,
                                               HWND hwndNotebook)
{
    BOOL brc = FALSE;

    // XWPFontFolderData *somThis = XWPFontFolderGetData(somSelf);
    XWPFontFolderMethodDebug("XWPFontFolder","fon_wpAddSettingsPages");

    brc = XWPFontFolder_parent_XFolder_wpAddSettingsPages(somSelf,
                                                          hwndNotebook);
    _xwpAddFontsPage(somSelf, hwndNotebook);

    return brc;
}


/* ******************************************************************
 *
 *   XWPFontFolder class methods
 *
 ********************************************************************/

/*
 *@@ xwpclsQueryDefaultFontFolder:
 *      this returns the default font folder (with the object ID
 *      &lt;XWP_FONTFOLDER&gt;).
 */

SOM_Scope XWPFontFolder*  SOMLINK fonM_xwpclsQueryDefaultFontFolder(M_XWPFontFolder *somSelf)
{
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_xwpclsQueryDefaultFontFolder");

    /* Return statement to be customized: */
    return NULL;
}

/*
 *@@ wpclsInitData:
 *      this M_WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 *
 *      We set up some global folder data and also make sure
 *      that the XWPFontObj class gets initialized.
 */

SOM_Scope void  SOMLINK fonM_wpclsInitData(M_XWPFontFolder *somSelf)
{
    SOMClass *pFontObjectClassObject;
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsInitData");

    M_XWPFontFolder_parent_M_XFolder_wpclsInitData(somSelf);

    if (krnClassInitialized(G_pcszXWPFontFolder))
    {
        // first call:

        // enforce initialization of XWPFontObject class
        if (pFontObjectClassObject = XWPFontObjectNewClass(XWPFontObject_MajorVersion,
                                                           XWPFontObject_MinorVersion))
        {
            // now increment the class's usage count by one to
            // ensure that the class is never unloaded; if we
            // didn't do this, we'd get WPS CRASHES in some
            // background class because if no more trash objects
            // exist, the class would get unloaded automatically -- sigh...
            _wpclsIncUsage(pFontObjectClassObject);
        }
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Cannot initialize XWPFontObject class. Is it installed?!?");
    }
}

/*
 *@@ fonM_wpclsUnInitData:
 *
 */

SOM_Scope void  SOMLINK fonM_wpclsUnInitData(M_XWPFontFolder *somSelf)
{
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsUnInitData");

    _wpclsDecUsage(_XWPFontObject);

    M_XWPFontFolder_parent_M_XFolder_wpclsUnInitData(somSelf);
}

/*
 *@@ wpclsQueryTitle:
 *      this M_WPObject class method tells the WPS the clear
 *      name of a class, which is shown in the third column
 *      of a Details view and also used as the default title
 *      for new objects of a class.
 */

SOM_Scope PSZ  SOMLINK fonM_wpclsQueryTitle(M_XWPFontFolder *somSelf)
{
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsQueryTitle");

    // return (M_XWPFontFolder_parent_M_WPFolder_wpclsQueryTitle(somSelf));
    return (PSZ)cmnGetString(ID_XSSI_FONTFOLDER);
}

/*
 *@@ wpclsQueryStyle:
 *
 *@@changed V0.9.16 (2001-11-25) [umoeller]: added nevertemplate
 */

SOM_Scope ULONG  SOMLINK fonM_wpclsQueryStyle(M_XWPFontFolder *somSelf)
{
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsQueryStyle");

    return (CLSSTYLE_NEVERTEMPLATE      // V0.9.16 (2001-11-25) [umoeller]
                | CLSSTYLE_NEVERCOPY    // but allow move
                | CLSSTYLE_NEVERDELETE
                | CLSSTYLE_NEVERPRINT);
}

/*
 *@@ wpclsQueryDefaultHelp:
 *      this M_WPObject class method returns the default help
 *      panel for objects of this class. This gets called
 *      from WPObject::wpQueryDefaultHelp if no instance
 *      help settings (HELPLIBRARY, HELPPANEL) have been
 *      set for an individual object. It is thus recommended
 *      to override this method instead of the instance
 *      method to change the default help panel for a class
 *      in order not to break instance help settings (fixed
 *      with 0.9.20).
 *
 *      We return the font folder default help here.
 *
 *@@added V0.9.20 (2002-07-12) [umoeller]
 */

SOM_Scope BOOL  SOMLINK fonM_wpclsQueryDefaultHelp(M_XWPFontFolder *somSelf,
                                                   PULONG pHelpPanelId,
                                                   PSZ pszHelpLibrary)
{
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsQueryDefaultHelp");

    strcpy(pszHelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XSH_FONTFOLDER;
    return TRUE;
}

/*
 *@@ wpclsCreateDefaultTemplates:
 *      this M_WPObject class method is called by the
 *      Templates folder to allow a class to
 *      create its default templates.
 *
 *      The default WPS behavior is to create new templates
 *      if the class default title is different from the
 *      existing templates.
 */

SOM_Scope BOOL  SOMLINK fonM_wpclsCreateDefaultTemplates(M_XWPFontFolder *somSelf,
                                                         WPObject* Folder)
{
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsCreateDefaultTemplates");

    // pretend we've created the templates
    return TRUE;
}

/*
 *@@ wpclsQueryIconData:
 *      this M_WPObject class method must return information
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
 *      We give this class a new standard icon here.
 */

SOM_Scope ULONG  SOMLINK fonM_wpclsQueryIconData(M_XWPFontFolder *somSelf,
                                                 PICONINFO pIconInfo)
{
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsQueryIconData");

    if (pIconInfo)
    {
        pIconInfo->fFormat = ICON_RESOURCE;
        pIconInfo->resid   = ID_ICONXWPFONTCLOSED;
        pIconInfo->hmod    = cmnQueryMainResModuleHandle();
    }

    return sizeof(ICONINFO);
}

/*
 *@@ wpclsQueryIconDataN:
 *      this should return the class default
 *      "animation" icons (for open folders).
 */

SOM_Scope ULONG  SOMLINK fonM_wpclsQueryIconDataN(M_XWPFontFolder *somSelf,
                                                  ICONINFO* pIconInfo,
                                                  ULONG ulIconIndex)
{
    /* M_XWPFontFolderData *somThis = M_XWPFontFolderGetData(somSelf); */
    M_XWPFontFolderMethodDebug("M_XWPFontFolder","fonM_wpclsQueryIconDataN");

    if (pIconInfo)
    {
        pIconInfo->fFormat = ICON_RESOURCE;
        pIconInfo->resid   = ID_ICONXWPFONTOPEN;
        pIconInfo->hmod    = cmnQueryMainResModuleHandle();
    }

    return sizeof(ICONINFO);
}

