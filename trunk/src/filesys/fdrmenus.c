
/*
 *@@sourcefile fdrmenus.c:
 *      this file contains the menu manipulation and selection
 *      logic for most of the XFolder context menu features.
 *
 *      The functions in here are called by the XFolder and
 *      XFldDisk WPS methods for context menus. Since those two
 *      classes share many common menu items, they also share
 *      the routines for handling them.
 *
 *      The two most important entry points into all this are:
 *
 *      -- mnuModifyFolderPopupMenu, which gets called from
 *         XFolder::wpModifyPopupMenu and XFldDisk::wpModifyPopupMenu;
 *         this modifies folder and disk context menu items.
 *
 *      -- mnuMenuItemSelected, which gets called from
 *         XFolder::wpMenuItemSelected and XFldDisk::wpMenuItemSelected;
 *         this reacts to folder and disk context menu items.
 *
 *      This code mainly does three things:
 *
 *      -- Recursively add the contents of the XFolder config
 *         folder to a context menu (on modify) and react to
 *         selections of these menu items.
 *
 *      -- Handle folder content menus, with the help of the
 *         shared content menu functions in src\shared\contentmenus.c.
 *
 *      -- Hack the folder sort and folder view submenus and
 *         allow the context menu to stay open when a menu
 *         item was selected, with the help of the subclassed
 *         folder frame winproc (fdr_fnwpSubclassedFolderFrame).
 *
 *      This code does NOT intercept the menu commands for the
 *      new file operations engine (such as "delete" for deleting
 *      objects into the trash can). See XFolder::xwpProcessObjectCommand
 *      for that.
 *
 *      Function prefix for this file:
 *      --  mnu*
 *
 *      This file was new with XFolder V0.81. This probably
 *      contains some of the oldest code that has been in XFolder
 *      forever, since config folders was what XFolder really
 *      started out with. So don't expect this code to be perfectly
 *      well designed.
 *
 *      All these functions used to be in xfldr.c with early
 *      XFolder versions and have been moved here later. This code
 *      has been completely worked over several times, and parts
 *      have been moved between other files.
 *
 *      With V0.9.7, all menu functions related to folder _content_
 *      menus have been moved to src\shared\contentmenus.c to
 *      allow sharing with other code parts, such as the XCenter.
 *
 *@@header "filesys\fdrmenus.h"
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR        // SC_CLOSE etc.
#define INCL_WININPUT
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINSTATICS
#define INCL_WINSTDCNR
#define INCL_WINMLE
#define INCL_WINCOUNTRY
#define INCL_WINCLIPBOARD
#define INCL_WINSYS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIPRIMITIVES

#define INCL_DEV
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"                     // XFldObject
#include "xwpstring.ih"                 // XWPString
#include "xfdisk.ih"                    // XFldDisk
#include "xfdataf.ih"
#include "xfldr.ih"                     // XFolder

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\contentmenus.h"        // shared menu logic
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

// #include "config\partitions.h"          // WPDrives "Partitions" view

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wppgm.h>                      // WPProgram
#include <wpshadow.h>                   // WPShadow
#include <wprootf.h>                    // WPRootFolder

#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)
#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static BOOL    G_fIsWarp4 = FALSE;

// static HWND    G_hwndTemplateFrame = NULLHANDLE;
// static POINTL  G_ptlTemplateMousePos = {0};

// linked list for config folder content:
static HMTX        G_hmtxConfigContent = NULLHANDLE;   // V0.9.9 (2001-04-04) [umoeller]
static LINKLIST    G_llConfigContent;
static BOOL        G_fConfigCacheValid;                // if FALSE, cache is rebuilt

static POINTL      G_ptlMouseMenu;              // ptr position when menu was opened
                                                // moved this here from XFolder instance
                                                // data V0.9.16 (2001-10-23) [umoeller]

/* ******************************************************************
 *
 *   Various helper funcs
 *
 ********************************************************************/

/*
 *@@ mnuInsertFldrViewItems:
 *      this inserts the folder view menu items
 *      ("Small icons", "Flowed" etc.) into hwndViewSubmenu
 *      if the current cnr view makes sense for this.
 *
 *      This gets called from two places:
 *
 *      -- mnuModifyFolderPopupMenu for regular popup
 *         menus;
 *
 *      -- fdr_fnwpSubclassedFolderFrame upon WM_INITMENU
 *         for the "View" pulldown in folder menu bars.
 *
 *      hwndViewSubmenu contains the submenu to add
 *      items to:
 *      --  on Warp 4, this is the default "View" submenu
 *          (either in the context menu or the "View" pulldown)
 *      --  on Warp 3, mnuModifyFolderPopupMenu creates a new
 *          "View" submenu, which is passed to this func.
 *
 *      Returns TRUE if the menu items were inserted.
 *
 *@@changed V0.9.0 [umoeller]: added "menu bar" item under Warp 4
 *@@changed V0.9.0 [umoeller]: fixed wrong separators
 *@@changed V0.9.0 [umoeller]: now using wpshQueryObjectFromID to get the config folder
 *@@changed V0.9.0 [umoeller]: fixed broken "View" item in menu bar
 */

BOOL mnuInsertFldrViewItems(WPFolder *somSelf,      // in: folder w/ context menu
                            HWND hwndViewSubmenu,   // in: submenu to add items to
                            BOOL fInsertNewMenu,    // in: insert "View" into hwndViewSubmenu?
                            HWND hwndCnr,           // in: cnr hwnd passed to
                                                    //     mnuModifyFolderPopupMenu
                            ULONG ulView)           // in: OPEN_* flag
{
    BOOL        brc = FALSE;
    XFolderData *somThis = XFolderGetData(somSelf);

    G_fIsWarp4 = doshIsWarp4();

    #ifdef DEBUG_MENUS
        _Pmpf(("mnuInsertFldrViewItems, hwndCnr: 0x%X", hwndCnr));
    #endif

    if (hwndCnr)
    {
        // we have a valid open view:
        // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

        ULONG       ulOfs = cmnQuerySetting(sulVarMenuOffset);
        ULONG       ulAttr = 0;
        USHORT      usIconsAttr;
        CNRINFO     CnrInfo;
        cnrhQueryCnrInfo(hwndCnr, &CnrInfo);

        // add "small icons" item for all view types,
        // but disable for Details view
        if (ulView == OPEN_DETAILS)
        {
            // Details view: disable and check
            // "mini icons"
            usIconsAttr = (MIA_DISABLED | MIA_CHECKED);
        }
        else
        {
            // otherwise: set "mini icons" to cnr info data
            usIconsAttr = (CnrInfo.flWindowAttr & CV_MINI)
                                  ? MIA_CHECKED
                                  : 0;
        }

        if (fInsertNewMenu)
        {
            // on Warp 3, we need to add a "View" submenu
            // for the folder view flags, because Warp 3
            // doesn't have one
            hwndViewSubmenu = winhInsertSubmenu(hwndViewSubmenu,
                                                MIT_END,
                                                ulOfs + ID_XFM_OFS_WARP3FLDRVIEW,
                                                cmnGetString(ID_XFSI_FLDRSETTINGS), // pszWarp3FldrView */
                                                    MIS_SUBMENU,
                                                // item
                                                ulOfs + ID_XFMI_OFS_SMALLICONS,
                                                cmnGetString(ID_XFSI_SMALLICONS), // pszSmallIcons */
                                                MIS_TEXT,
                                                usIconsAttr);
        }
        else
            winhInsertMenuItem(hwndViewSubmenu,
                               MIT_END,
                               ulOfs + ID_XFMI_OFS_SMALLICONS,
                               cmnGetString(ID_XFSI_SMALLICONS),  // pszSmallIcons
                               MIS_TEXT,
                               usIconsAttr);

        brc = TRUE; // modified flag

        if (ulView == OPEN_CONTENTS)
        {
            // icon view:
            winhInsertMenuSeparator(hwndViewSubmenu, MIT_END,
                        (ulOfs + ID_XFMI_OFS_SEPARATOR));


            winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                               ulOfs + ID_XFMI_OFS_NOGRID,
                               cmnGetString(ID_XFSI_NOGRID),  MIS_TEXT, // pszNoGrid
                               ((CnrInfo.flWindowAttr & (CV_ICON | CV_TREE)) == CV_ICON)
                                    ? MIA_CHECKED
                                    : 0);
            winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                               ulOfs + ID_XFMI_OFS_FLOWED,
                               cmnGetString(ID_XFSI_FLOWED),  MIS_TEXT, // pszFlowed
                               ((CnrInfo.flWindowAttr & (CV_NAME | CV_FLOW)) == (CV_NAME | CV_FLOW))
                                    ? MIA_CHECKED
                                    : 0);
            winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                               ulOfs + ID_XFMI_OFS_NONFLOWED,
                               cmnGetString(ID_XFSI_NONFLOWED),  MIS_TEXT, // pszNonFlowed
                               ((CnrInfo.flWindowAttr & (CV_NAME | CV_FLOW)) == (CV_NAME))
                                    ? MIA_CHECKED
                                    : 0);
        }

        // for all views: add separator before menu and status bar items
        // if one of these is enabled
        if (    (G_fIsWarp4)
#ifndef __NOCFGSTATUSBARS__
             || (cmnQuerySetting(sfStatusBars))  // added V0.9.0
#endif
           )
            winhInsertMenuSeparator(hwndViewSubmenu, MIT_END,
                                    ulOfs + ID_XFMI_OFS_SEPARATOR);

        // on Warp 4, insert "menu bar" item (V0.9.0)
        if (G_fIsWarp4)
        {
            winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                               ulOfs + ID_XFMI_OFS_WARP4MENUBAR,
                               cmnGetString(ID_XFSI_WARP4MENUBAR),  MIS_TEXT, // pszWarp4MenuBar
                               (_xwpQueryMenuBarVisibility(somSelf))
                                   ? MIA_CHECKED
                                   : 0);
        }

        // insert "status bar" item if status bar feature
        // is enabled in XWPSetup
#ifndef __NOCFGSTATUSBARS__
        if (cmnQuerySetting(sfStatusBars))
#endif
        {
            if (cmnIsADesktop(somSelf))
                // always disable for Desktop
                ulAttr = MIA_DISABLED;
            else if (_somIsA(somSelf, _WPRootFolder))
                // for root folders (WPDisk siblings),
                // check global setting only
                ulAttr = MIA_DISABLED
                            | ((cmnQuerySetting(sfDefaultStatusBarVisibility))
                                ? MIA_CHECKED
                                : 0);
            else
                // for regular folders, check both instance
                // and global status bar setting
                ulAttr = (
                           (_bStatusBarInstance == STATUSBAR_ON)
                        || (    (_bStatusBarInstance == STATUSBAR_DEFAULT)
                             && (cmnQuerySetting(sfDefaultStatusBarVisibility))
                           )
                         )
                            ? MIA_CHECKED
                            : 0;

            // insert status bar item with the above attribute
            winhInsertMenuItem(hwndViewSubmenu,
                               MIT_END,
                               ulOfs + ID_XFMI_OFS_SHOWSTATUSBAR,
                               cmnGetString(ID_XFSI_SHOWSTATUSBAR),  // pszShowStatusBar
                               MIS_TEXT,
                               ulAttr);
        }
    }
    return (brc);
}

/* ******************************************************************
 *
 *   Config folder management/caching
 *
 ********************************************************************/

/*
 *@@ BuildConfigItemsList:
 *      this recursive function gets called from
 *      InsertConfigFolderItems initially to build
 *      the config folder cache (the list of objects
 *      in the configuration folder, CONTENTLISTITEM
 *      structures).
 *
 *      Initially, pllContentThis is set to the global
 *      pllConfigContent variable, and pFolderThis points
 *      to the config folder.
 *
 *      When another folder is found in pFolderThis, we
 *      create another list and recurse with that folder
 *      and the new list.
 *
 *      This way of building the config folder menu items
 *      has been introduced with V0.9.0.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.14 (2001-08-25) [umoeller]: added XWPString support
 */

static BOOL BuildConfigItemsList(PLINKLIST pllContentThis,     // in: CONTENTLISTITEM list to append to
                                 XFolder *pFolderThis)         // in: folder to append from
{
    BOOL        brc = TRUE;
    WPObject    *pObject,
                *pObject2Insert;
    HPOINTER    hptrOld = winhSetWaitPointer();
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    // iterate over the content of *pFolderThis;
    // use the XFolder method which recognizes item order
    ULONG       henum = _xwpBeginEnumContent(pFolderThis);

    if (henum)
    {
        while (pObject = _xwpEnumNext(pFolderThis, henum))
        {
            // if the object is a shadow, we will only de-reference
            // it if it's a template
            if (_somIsA(pObject, _WPShadow))
            {
                pObject2Insert = _wpQueryShadowedObject(pObject, FALSE);
                if (pObject2Insert)
                    if ((_wpQueryStyle(pObject2Insert) & OBJSTYLE_TEMPLATE) == 0)
                        pObject2Insert = pObject;
            }
            else
                pObject2Insert = pObject;

            // eliminate broken shadows
            if (pObject2Insert)
            {
                // create list item
                PCONTENTLISTITEM pcli = malloc(sizeof(CONTENTLISTITEM));
                memset(pcli, 0, sizeof(CONTENTLISTITEM));

                pcli->pObject = pObject2Insert;
                strhncpy0(pcli->szTitle,
                          _wpQueryTitle(pObject2Insert),
                          sizeof(pcli->szTitle));

                // now we check on the type of the class that we found and
                // remember the relevant types in the linked object list:
                // -- template: insert name, mark as QC_TEMPLATE
                // -- WPProgram: insert name into menu and mark as QC_PROGRAM;
                // -- WPFolder: insert submenu and recurse this routine;
                // -- others: insert name, mark as QC_OTHER;
                // we mark WPPrograms separately, since we will perform some
                // tricks on them in mnuMenuItemSelected when selected
                if (_wpQueryStyle(pObject2Insert) & OBJSTYLE_TEMPLATE)
                    pcli->ulObjectType = OC_TEMPLATE;
                else if (_somIsA(pObject2Insert, _WPProgram))
                {
                    // program object:
                    // check if it's to be a menu separator
                    if (strncmp(pcli->szTitle, "---", 3))
                        // no: insert as program object
                        pcli->ulObjectType = OC_PROGRAM;
                    else
                        // title == "---": insert separator
                        pcli->ulObjectType = OC_SEPARATOR;
                }
                else if (_somIsA(pObject2Insert, _WPFolder))
                {
                    // folder:
                    pcli->ulObjectType = OC_FOLDER;
                    // create another list for the contents
                    pcli->pllFolderContent = lstCreate(TRUE);

                    // now call ourselves recursively with the new
                    // folder and the new list
                    brc = BuildConfigItemsList(pcli->pllFolderContent,
                                               pcli->pObject);  // the folder
                }
                else if (    (krnIsClassReady(G_pcszXWPString))   // XWPString installed?
                          && (_somIsA(pObject2Insert, _XWPString))
                        )
                {
                    // V0.9.14 (2001-08-25) [umoeller]
                    pcli->ulObjectType = OC_XWPSTRING;
                }
                else
                    // some other object: mark as OC_OTHER
                    // (will simply be opened)
                    pcli->ulObjectType = OC_OTHER;

                // insert new item in list
                lstAppendItem(pllContentThis,
                              pcli);

                // mark this object as being in the config folder
                _xwpModifyListNotify(pObject2Insert,
                                     OBJLIST_CONFIGFOLDER,
                                     OBJLIST_CONFIGFOLDER);
            } // end if (pObject2Insert)
        } // end while

        _xwpEndEnumContent(pFolderThis, henum);
    }

    WinSetPointer(HWND_DESKTOP, hptrOld);

    return (brc);
}

/*
 *@@ InsertObjectsFromList:
 *      this recursive function gets called from
 *      InsertConfigFolderItems after the list of
 *      config folder items has been built to
 *      insert its list items into the menu.
 *
 *      Initially, pllContentThis points to the
 *      global variable pllConfigContent (the
 *      contents of the config folder). We recurse
 *      if, on that list, we find another folder
 *      item with a list, and call ourselves again
 *      with that list and a new menu.
 *
 *@@changed V0.9.0 [umoeller]: renamed from mnuFillMenuWithObjects; prototype changed; now running with lists
 */

static LONG InsertObjectsFromList(PLINKLIST  pllContentThis, // in: list to take items from (var.)
                                  HWND       hMenuThis,      // in: menu to add items to (var.)
                                  HWND       hwndCnr,        // in: needed for wpInsertPopupMenuItems (const)
                                  ULONG      ulOfs)          // in: cmnQuerySetting(sulVarMenuOffset)
{
    LONG       lDefaultItem = 0;
    LONG       rc = 0,
               lReturnDefaultItem = 0;

    PLISTNODE  pContentNode = lstQueryFirstNode(pllContentThis);

    while (pContentNode)
    {
        PCONTENTLISTITEM pcli = (PCONTENTLISTITEM)pContentNode->pItemData;

        // now we check on the type of the class that we found and
        // remember the relevant types in the linked object list:
        // -- template: insert name, mark as QC_TEMPLATE
        // -- WPProgram: insert name into menu and mark as QC_PROGRAM;
        // -- WPFolder: insert submenu and recurse this routine;
        // -- others: insert name, mark as QC_OTHER;
        // we mark WPPrograms separately, since we will perform some
        // tricks on them in mnuMenuItemSelected when selected
        switch (pcli->ulObjectType)
        {
            case OC_TEMPLATE:
            case OC_PROGRAM:
            case OC_XWPSTRING:      // V0.9.14 (2001-08-25) [umoeller]
            case OC_OTHER:
                rc = cmnuInsertOneObjectMenuItem(hMenuThis,
                                                 MIT_END,
                                                 pcli->szTitle,
                                                 MIS_TEXT,
                                                 pcli->pObject,
                                                 pcli->ulObjectType);

                if (lReturnDefaultItem == 0)
                    lReturnDefaultItem = rc;
            break;

            case OC_SEPARATOR:
                winhInsertMenuSeparator(hMenuThis,
                                        MIT_END,
                                        ulOfs + ID_XFMI_OFS_SEPARATOR);
            break;

            case OC_FOLDER:
            {
                //  if we find another folder, we add a submenu containing
                //  "[Config folder empty]"), which will be removed if more
                //  objects are found in the respective config folder
                HWND hNewMenu = winhInsertSubmenu(hMenuThis,
                                                  MIT_END,
                                                  G_sNextMenuId,
                                                  pcli->szTitle,
                                                  MIS_TEXT,
                                                  ulOfs + ID_XFMI_OFS_BORED,
                                                  cmnGetString(ID_XSSI_BORED), // (cmnQueryNLSStrings())->pszBored,
                                                  MIS_TEXT,
                                                  0);
                cmnuAppendMi2List(pcli->pObject, OC_FOLDER);

                // now call this function recursively with the new
                // list and the new submenu handle
                lDefaultItem = InsertObjectsFromList(pcli->pllFolderContent,
                                                     hNewMenu,
                                                     hwndCnr,
                                                     ulOfs);
                // now we're back: check if error occured; if so, exit
                // immediately to stop recursing
                if (lDefaultItem == -1)
                    return (-1);

                if (lDefaultItem)
                {
                    // items were inserted:
                    // remove static "config folder empty" menu item
                    WinSendMsg(hNewMenu,
                               MM_DELETEITEM,
                               MPFROM2SHORT((ulOfs + ID_XFMI_OFS_BORED),
                                            TRUE),
                               (MPARAM)NULL);

                    if ((cmnQuerySetting(sfMenuCascadeMode)) && (lDefaultItem != 1))
                    {
                        // make the XFolder submenu cascading
                        winhSetMenuCondCascade(hNewMenu,
                                               lDefaultItem);
                    }
                }

                if (lReturnDefaultItem == 0)
                    lReturnDefaultItem = 1;

            }
            break;
        } // end if (pObject2)

        pContentNode = pContentNode->pNext;
    } // end while (pContentNode)
    return (lReturnDefaultItem);
}

/*
 *@@ LockConfigCache:
 *      locks the global config folder content cache.
 *
 *      This has only been added with V0.9.9. Sigh...
 *      I had always thought that mutex protection
 *      for the cache wasn't needed since there was
 *      only ever one menu open in PM. This isn't
 *      quite true... for one, we can't ever trust
 *      PM for such things, secondly, the config
 *      cache gets invalidated behind our back if
 *      an object from the config folders gets
 *      deleted.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

static BOOL LockConfigCache(VOID)
{
    BOOL brc = FALSE;

    if (G_hmtxConfigContent == NULLHANDLE)
    {
        brc = !DosCreateMutexSem(NULL,
                                 &G_hmtxConfigContent,
                                 0,
                                 TRUE);
        lstInit(&G_llConfigContent, FALSE);
        G_fConfigCacheValid = FALSE;        // rebuild
    }
    else
        brc = !WinRequestMutexSem(G_hmtxConfigContent, SEM_INDEFINITE_WAIT);
            // WinRequestMutexSem works even if the thread has no message queue

    return (brc);
}

/*
 *@@ UnlockConfigCache:
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

static VOID UnlockConfigCache(VOID)
{
    DosReleaseMutexSem(G_hmtxConfigContent);
}

/*
 *@@ mnuInvalidateConfigCache:
 *      this gets called from our override of
 *      XFolder::wpStoreIconPosData when the
 *      .ICONPOS EAs for a config folder have
 *      been rewritten. We need to invalidate
 *      our config folder cache then so that
 *      the lists will be rebuilt.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.9 (2001-04-04) [umoeller]: added mutex protection for cache
 */

VOID mnuInvalidateConfigCache(VOID)
{
    if (LockConfigCache())
    {
        lstClear(&G_llConfigContent);
        G_fConfigCacheValid = FALSE;
            // this will enfore a rebuild in InsertConfigFolderItems

        UnlockConfigCache();
    }
}

/*
 *@@ InsertConfigFolderItems:
 *      this gets called from mnuModifyFolderPopupMenu to insert
 *      the config folder items into a folder's context menu.
 *
 *      When this is called for the first time, a list of
 *      CONTENTLISTITEM structures is built in the global
 *      variable pllConfigContent, which holds the contents
 *      of the config folder for subsequent calls ("config
 *      folder caching").
 *
 *      We then insert menu items according to that list,
 *      which saves us from querying the folders' contents
 *      every time the context menu is opened. This feature
 *      is new with V0.9.0.
 *
 *      That list can be invalidated by calling mnuInvalidateConfigCache,
 *      e.g. because the contents of the config folder have
 *      changed. This is done by several XFolder method overrides.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.9 (2001-04-04) [umoeller]: added mutex protection for cache
 *@@changed V0.9.12 (2001-05-22) [umoeller]: added extended close menu
 */

static BOOL InsertConfigFolderItems(XFolder *somSelf,
                                    HWND hwndMenu,
                                    HWND hwndCnr,
                                    ULONG ulOfs)
{
    BOOL brc = FALSE;

    XFolder *pConfigFolder = _xwpclsQueryConfigFolder(_XFolder);

    if (pConfigFolder == NULL)
        // config folder not or no longer found:
        mnuInvalidateConfigCache();
    else
    {
        // config folder found:
        if (LockConfigCache()) // V0.9.9 (2001-04-04) [umoeller]
        {
            // have we built a list of objects yet?
            if (!G_fConfigCacheValid)
            {
                // no: create one
                BuildConfigItemsList(&G_llConfigContent,
                                     pConfigFolder);
                G_fConfigCacheValid = TRUE;
            }

            // do we have any objects?
            if (lstCountItems(&G_llConfigContent) > 0)
            {
                // yes:
                // append another separator to the menu first
                winhInsertMenuSeparator(hwndMenu, MIT_END,
                                        ulOfs + ID_XFMI_OFS_SEPARATOR);

                // now insert items in pConfigFolder into main context menu (hwndMenu);
                // this routine will call itself recursively if it finds subfolders.
                // Since we have registered an exception handler, if errors occur,
                // this will lead to a message box only.
                InsertObjectsFromList(&G_llConfigContent,
                                      hwndMenu,
                                      hwndCnr,
                                      ulOfs);
            }

            UnlockConfigCache();        // V0.9.9 (2001-04-04) [umoeller]
        }
    }

    /* else
    {
        // config folder not found: give message and create it anew
        xthrPostFileMsg(FIM_RECREATECONFIGFOLDER,
                       (MPARAM)RCF_QUERYACTION,
                       MPNULL);
        brc = FALSE;
    } */

    return (brc);
}

/* ******************************************************************
 *
 *   "Modify menu" entry points
 *
 ********************************************************************/

/*
 *@@ mnuModifyFolderPopupMenu:
 *      this is the general menu modifier routine which gets called
 *      by XFolder::wpModifyPopupMenu and XFldDisk::wpModifyPopupMenu.
 *      Since menu items are mostly the same for these two classes,
 *      the shared code has been moved into this function.
 *
 *      Note that when called from XFldDisk, somSelf points to the "root
 *      folder" of the disk object.
 *
 *      First we remove and insert various menu items according to
 *      the Global and instance settings.
 *
 *      We then insert the submenu stubs for folder content menus
 *      by calling mnuPrepareContentSubmenu for the current folder
 *      and all favorite folders.
 *
 *      Finally, we insert the config folder items by calling
 *      InsertConfigFolderItems, which now implements caching for
 *      these items (V0.9.0).
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: introduced config folder menu items cache
 *@@changed V0.9.1 (2000-02-01) [umoeller]: "select some" was added for Tree view also; fixed
 *@@changed V0.9.3 (2000-04-10) [umoeller]: snap2grid feature setting was ignored; fixed
 *@@changed V0.9.12 (2001-05-22) [umoeller]: "refresh now" was added even for non-open-view menus
 *@@changed V0.9.14 (2001-08-07) [pr]: added Run menu item
 */

BOOL mnuModifyFolderPopupMenu(WPFolder *somSelf,  // in: folder or root folder
                              HWND hwndMenu,      // in: main context menu hwnd
                              HWND hwndCnr,       // in: cnr hwnd
                              ULONG iPosition)    // in: dunno
{
    XFolder         *pFavorite;
    BOOL            rc = TRUE;
    MENUITEM        mi;

    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    ULONG           ulVarMenuOfs = cmnQuerySetting(sulVarMenuOffset);

    // set the global variable for whether Warp 4 is running
    G_fIsWarp4 = doshIsWarp4();

    // protect the following with the quiet exception handler (except.c)
    TRY_QUIET(excpt1)
    {
        // some preparations
        XFolderData *somThis = XFolderGetData(somSelf);
        HWND        hwndFrame = NULLHANDLE;
        ULONG       ulView = -1;
        BOOL        bSepAdded = FALSE;

        if (hwndCnr)
            hwndFrame = WinQueryWindow(hwndCnr, QW_PARENT);

        // store mouse pointer position for creating objects from templates
        WinQueryMsgPos(G_habThread1,
                       &G_ptlMouseMenu);   // V0.9.16 (2001-10-23) [umoeller]

        #ifdef DEBUG_MENUS
            _Pmpf(("mnuModifyFolderPopupMenu, hwndCnr: 0x%lX", hwndCnr));
        #endif

        // get view (OPEN_CONTENTS etc.)
        if (hwndFrame)
            ulView = wpshQueryView(somSelf, hwndFrame);

        // in wpFilterPopupMenu, because no codes are provided;
        // we only do this if the Global Settings want it

        /*
         * WPDrives "Open" submenu:
         *
         */

        /* if (_somIsA(somSelf, _WPDrives))
        {
            // get handle to the WPObject's "Help" submenu in the
            // the folder's popup menu
            if (WinSendMsg(hwndMenu,
                           MM_QUERYITEM,
                           MPFROM2SHORT(WPMENUID_OPEN, TRUE),
                           (MPARAM)&mi))
            {
                // mi.hwndSubMenu now contains "Open" submenu handle,
                // which we add items to now
                winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                                   ulVarMenuOfs + ID_XFMI_OFS_XWPVIEW,
                                   cmnGetString(ID_XSSI_OPENPARTITIONS),  // pszOpenPartitions
                                   MIS_TEXT, 0);
            }
        } */

#ifndef __NOFDRDEFAULTDOCS__
        /*
         * Default document in "Open" submenu:
         *
         */

        if (cmnQuerySetting(sfFdrDefaultDoc))
        {
            WPFileSystem *pDefDoc = _xwpQueryDefaultDocument(somSelf);
            if (pDefDoc)
                // we have a default document for this folder:
                if (winhQueryMenuItem(hwndMenu,
                                      WPMENUID_OPEN,
                                      TRUE,
                                      &mi))
                {
                    // mi.hwndSubMenu now contains "Open" submenu handle;

                    CHAR szDefDoc[2*CCHMAXPATH];
                    sprintf(szDefDoc,
                            "%s \"%s\"",
                            cmnGetString(ID_XSSI_FDRDEFAULTDOC),  // pszFdrDefaultDoc
                            _wpQueryTitle(pDefDoc));
                    winhInsertMenuSeparator(mi.hwndSubMenu, MIT_END,
                                            ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);
                    // add "Default document"
                    winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                                       ulVarMenuOfs + ID_XFMI_OFS_FDRDEFAULTDOC,
                                       szDefDoc,
                                       MIS_TEXT, 0);
                }
        }
#endif

        /*
         * "View" submenu:
         *
         */

        if (G_fIsWarp4)
        {
            if (cmnQuerySetting(sfRemoveViewMenu))
            {
                #ifdef DEBUG_MENUS
                    _Pmpf(("  Removing 'View' submenu"));
                #endif
                winhRemoveMenuItem(hwndMenu, ID_WPM_VIEW);
            }

            if (cmnQuerySetting(sfRemovePasteItem))
            {
                #ifdef DEBUG_MENUS
                    _Pmpf(("  Removing 'Paste' menu item"));
                #endif
                winhRemoveMenuItem(hwndMenu, ID_WPMI_PASTE);
            }
        }

        /*
         * product info:
         *
         */

#ifndef __XWPLITE__
        cmnAddProductInfoMenuItem(hwndMenu);
#endif

        // work on the "View" submenu; do the following only
        // if the "View" menu has not been removed (Warp 4)
        // or the "Select" menu has not been removed (Warp 3)
        if (
                (   (G_fIsWarp4)
                 && (cmnQuerySetting(sfRemoveViewMenu) == 0)
                )
             || (   (!G_fIsWarp4)
                 && ((cmnQuerySetting(sflDefaultMenuItems) & CTXT_SELECT) == 0)
                )
           )
        {

            SHORT sPos = MIT_END;

            if (winhQueryMenuItem(hwndMenu,
                                  (G_fIsWarp4)
                                  // in Warp 4, these items are in the "View" submenu;
                                  // in Warp 3, "Select" is  a separate submenu
                                       ? ID_WPM_VIEW           // 0x68
                                       : 0x04,      // WPMENUID_SELECT
                                  TRUE,
                                  &mi))
            {
                // mi.hwndSubMenu now contains "Select"/"View" submenu handle,
                // which we can add items to now

                #ifdef DEBUG_MENUS
                    _Pmpf(("  'View'/'Select' hwnd:0x%X", mi.hwndSubMenu));
                #endif

                // add "Select by name" only if not in Tree view V0.9.1 (2000-02-01) [umoeller]
                if (    (cmnQuerySetting(sfAddSelectSomeItem))
                     && (   (ulView == OPEN_CONTENTS)
                         || (ulView == OPEN_DETAILS)
                        )
                   )
                {

                    if (G_fIsWarp4)
                        // get position of "Refresh now";
                        // we'll add behind that item
                        sPos = (SHORT)WinSendMsg(mi.hwndSubMenu,
                                                 MM_ITEMPOSITIONFROMID,
                                                 MPFROM2SHORT(WPMENUID_REFRESH,
                                                              FALSE),
                                                 MPNULL);
                    // else: using MIT_END (above)

                    #ifdef DEBUG_MENUS
                        _Pmpf(("  Adding 'Select some' @ ofs %d, hwndSubmenu: 0x%lX",
                                    sPos, mi.hwndSubMenu));
                    #endif
                    winhInsertMenuItem(mi.hwndSubMenu,
                                       sPos,
                                       ulVarMenuOfs + ID_XFMI_OFS_SELECTSOME,
                                       cmnGetString(ID_XSSI_SELECTSOME),  // pszSelectSome
                                       MIS_TEXT, 0);
                    if (G_fIsWarp4)
                        winhInsertMenuSeparator(mi.hwndSubMenu,
                                                sPos + 1,
                                                ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);
                }

                // additional "view" items (icon size etc.)
                if (cmnQuerySetting(sfExtendFldrViewMenu))
                {
                    // rule out possible user views
                    // of WPFolder subclasses
                    if (    (ulView == OPEN_TREE)
                         || (ulView == OPEN_CONTENTS)
                         || (ulView == OPEN_DETAILS)
                       )
                    {
                        if (G_fIsWarp4)
                        {
                            // for Warp 4, use the existing "View" submenu,
                            // but add an additional separator after
                            // the exiting "View"/"Refresh now" item
                            winhInsertMenuSeparator(mi.hwndSubMenu, MIT_END,
                                    ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

                            mnuInsertFldrViewItems(somSelf,
                                                   mi.hwndSubMenu,
                                                   FALSE,       // no submenu
                                                   hwndCnr,
                                                   ulView);
                        } else
                            // Warp 3:
                            mnuInsertFldrViewItems(somSelf,
                                                   hwndMenu,
                                                   TRUE, // on Warp 3, insert new submenu
                                                   hwndCnr,
                                                   ulView);
                    }
                }
            } // end if MM_QUERYITEM;
            // else: "View" menu not found; but this can
            // happen with Warp 4 menu bars, which have
            // a "View" menu separate from the "Folder" menu...
        }

        // work on "Sort" menu; we have put this
        // into a subroutine, because this is also
        // needed for folder menu _bars_

        fdrModifySortMenu(somSelf,
                          hwndMenu);

        if (cmnQuerySetting(sfAddCopyFilenameItem))
        {
            winhInsertMenuSeparator(hwndMenu, MIT_END,
                                    ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);
            bSepAdded = TRUE; // V0.9.14

            winhInsertMenuItem(hwndMenu, MIT_END,
                               ulVarMenuOfs + ID_XFMI_OFS_COPYFILENAME_MENU,
                               cmnGetString(ID_XSSI_COPYFILENAME), // (pNLSStrings)->pszCopyFilename,
                               0, 0);
        }

        // V0.9.14
/*      if (cmnQuerySetting(sAddRunItem))
        {
            if (!bSepAdded)
            {
                winhInsertMenuSeparator(hwndMenu, MIT_END,
                                        ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

                bSepAdded = TRUE;
            }

            winhInsertMenuItem(hwndMenu,
                               MIT_END,
                               ulVarMenuOfs + ID_XFMI_OFS_RUN,
                               cmnGetString(ID_XSSI_RUN),
                               MIS_TEXT,
                               0);
        } */

        // insert the "Refresh now" and "Snap to grid" items only
        // if the folder is currently open
        if (ulView != -1)           // fixed V0.9.12 (2001-05-22) [umoeller]
        {
            if (!bSepAdded) // V0.9.14
                winhInsertMenuSeparator(hwndMenu,
                                        MIT_END,
                                        ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);
            bSepAdded = TRUE;

#ifndef __NOMOVEREFRESHNOW__
            // "Refresh now"
            if (cmnQuerySetting(sfMoveRefreshNow))
                winhInsertMenuItem(hwndMenu,
                                   MIT_END,
                                   ulVarMenuOfs + ID_XFMI_OFS_REFRESH,
                                   cmnGetString(ID_XSSI_REFRESHNOW),  // pszRefreshNow
                                   MIS_TEXT,
                                   0);
#endif

            // "Snap to grid" feature enabled? V0.9.3 (2000-04-10) [umoeller]
#ifndef __NOSNAPTOGRID__
            if (    (cmnQuerySetting(sfSnap2Grid))
                 // "Snap to grid" enabled locally or globally?
                 && (    (_bSnapToGridInstance == 1)
                      || (   (_bSnapToGridInstance == 2)
                          && (cmnQuerySetting(sfAddSnapToGridDefault))
                         )
                    )
                 // insert only when sorting is off
                 && (!fdrHasAlwaysSort(somSelf))
                 // and only in icon view
                 && (ulView == OPEN_CONTENTS)
                )
            {
                // insert "Snap to grid" only for open icon views
                winhInsertMenuItem(hwndMenu,
                                   MIT_END,
                                   ulVarMenuOfs + ID_XFMI_OFS_SNAPTOGRID,
                                   cmnGetString(ID_XSSI_SNAPTOGRID),  // pszSnapToGrid
                                   MIS_TEXT,
                                   0);
            }
#endif
        } // end if view open

        // now do necessary preparations for all variable menu items
        // (i.e. folder content and config folder items)
        cmnuInitItemCache(); // pGlobalSettings);

        /*
         * folder content / favorite folders:
         *
         */

#ifndef __NOFOLDERCONTENTS__
        // get first favorite folder; we will only work on
        // this if either folder content for every folder is
        // enabled or at least one favorite folder exists
        pFavorite = _xwpclsQueryFavoriteFolder(_XFolder, NULL);
        if (    (!cmnQuerySetting(sfNoSubclassing))
             && (   (cmnQuerySetting(sfAddFolderContentItem))
                 || (pFavorite)
                )
           )
        {
            winhInsertMenuSeparator(hwndMenu,
                                    MIT_END,
                                    ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

            if (cmnQuerySetting(sfFolderContentShowIcons))
            {
                // before actually inserting the content submenus, we need a real
                // awful cheat, because otherwise the owner draw items won't work
                // right (I don't know why the hell they are not being sent
                // WM_MEASUREITEM msgs if we don't do this); so we insert a
                // content submenu and remove it again right away
                WinSendMsg(hwndMenu,
                           MM_REMOVEITEM,
                           MPFROM2SHORT(cmnuPrepareContentSubmenu(somSelf, hwndMenu,
                                                                  cmnGetString(ID_XSSI_FLDRCONTENT),  // pszFldrContent
                                                                  MIT_END,
                                                                  FALSE), // no owner draw
                                        FALSE),
                           MPNULL);
            }

            if (cmnQuerySetting(sfAddFolderContentItem))
            {
                // add "Folder content" only if somSelf is not a favorite folder,
                // because then we will insert the folder content anyway
                if (!_xwpIsFavoriteFolder(somSelf))
                    // somself is not in favorites list: add "Folder content"
                    cmnuPrepareContentSubmenu(somSelf,
                                              hwndMenu,
                                              cmnGetString(ID_XSSI_FLDRCONTENT),
                                              MIT_END,
                                              FALSE); // no owner draw in main context menu
            }

            // now add favorite folders
            pFavorite = NULL;
            while (pFavorite = _xwpclsQueryFavoriteFolder(_XFolder,
                                                          pFavorite))
            {
                cmnuPrepareContentSubmenu(pFavorite,
                                          hwndMenu,
                                          _wpQueryTitle(pFavorite),
                                          MIT_END,
                                          FALSE); // no owner draw in main context menu
            }
        } // end folder contents
#endif

        /*
         * config folders:
         *
         */

        InsertConfigFolderItems(somSelf,
                                hwndMenu,
                                hwndCnr,
                                ulVarMenuOfs);

    }
    CATCH(excpt1)
    {
        // exception caught:
        PSZ     pszErrMsg = malloc(1000);
        if (pszErrMsg)
        {
            strcpy(pszErrMsg, "An error occured while XFolder was trying to build "
                    "a folder's context menu. This might be due to the fact "
                    "that you have deleted objects from the Configuration folders, "
                    "but you did "
                    "not have these folders opened in the Icon or Details views "
                    "while doing so. "
                    "You should open and close the configuration folder and all "
                    "of its subfolders once. Make sure that all the folders are either "
                    "in Icon or Details view per default.");
            krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT, (MPARAM)pszErrMsg, MPNULL);
            mnuInvalidateConfigCache();
        }
    } END_CATCH();

    return (rc);
}

/*
 *@@ mnuHackFolderClose:
 *      this gets closed from XFolder::wpDisplayMenu
 *      only if "extend close menu" is enabled.
 *
 *      We kick out the standard "close" menu item
 *      and replace it with a submenu. See
 *      XFolder::wpDisplayMenu for remarks why we
 *      can't do it in mnuModifyFolderPopupMenu.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

/* nah, this doesn't work. Try again later. */

/* BOOL mnuHackFolderClose(WPFolder *somSelf,      // in: folder
                        HWND hwndOwner,         // in: from wpDisplayMenu
                        HWND hwndClient,        // in: from wpDisplayMenu
                        PPOINTL pptlPopupPt,    // in: from wpDisplayMenu
                        ULONG ulMenuType,       // in: from wpDisplayMenu
                        HWND hwndMenu)          // in: main context menu created by parent_wpDisplayMenu
{
    BOOL brc = FALSE;

    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    ULONG           ulVarMenuOfs = cmnQuerySetting(sulVarMenuOffset);

    HWND hNewMenu;

    // for some reason, WPS is no longer using
    // WPMENUID_CLOSE but SC_CLOSE directly (0x8004)
    winhRemoveMenuItem(hwndMenu,
                       SC_CLOSE);

    hNewMenu = winhInsertSubmenu(hwndMenu,
                                 MIT_END,
                                 ulVarMenuOfs + ID_XFMI_OFS_CLOSESUBMENU,
                                 cmnGetString(ID_XSSI_CLOSE),   // "close"
                                 MIS_TEXT,
                                 SC_CLOSE,
                                 cmnGetString(ID_XSSI_CLOSETHISVIEW), // "close this view
                                 MIS_SYSCOMMAND | MIS_TEXT,
                                        // note, MIS_SYSCOMMAND
                                 0);

    winhInsertMenuSeparator(hNewMenu,
                            MIT_END,
                            ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

    winhInsertMenuItem(hNewMenu,
                       MIT_END,
                       ulVarMenuOfs + ID_XFMI_OFS_CLOSEALLTHISFDR,
                       cmnGetString(ID_XSSI_CLOSEALLTHISFDR), // "close all of this fdr"
                       0,
                       0);
    winhInsertMenuItem(hNewMenu,
                       MIT_END,
                       ulVarMenuOfs + ID_XFMI_OFS_CLOSEALLSUBFDRS,
                       cmnGetString(ID_XSSI_CLOSEALLSUBFDRS), // "close all subfdrs"
                       0,
                       0);

    // set default menu item
    winhSetMenuCondCascade(hNewMenu,
                           SC_CLOSE);


    if (0 == (ulMenuType & MENU_NODISPLAY))
    {
        // we were supposed to show the menu:
        // show it then
        POINTL ptl;
        HWND hwndConvertSource;
        memcpy(&ptl, pptlPopupPt, sizeof(POINTL));
        if (hwndClient)
            hwndConvertSource = hwndClient;
        else
            hwndConvertSource = hwndOwner;

        WinMapWindowPoints(hwndConvertSource,
                           HWND_DESKTOP,
                           &ptl,
                           1);
        WinSetFocus(HWND_DESKTOP, hwndConvertSource);

        // alright, display the menu
        WinPopupMenu(HWND_DESKTOP,
                     hwndConvertSource,
                     hwndMenu,
                     ptl.x,
                     ptl.y,
                     0,
                     PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1
                        | PU_MOUSEBUTTON2 | PU_KEYBOARD);
    }

    return (TRUE);
} */

/*
 *@@ mnuModifyDataFilePopupMenu:
 *      implementation for XFldDataFile::wpModifyPopupMenu.
 *
 *      This does stuff like "Copy filename", "Attributes"
 *      etc.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.4 (2000-06-09) [umoeller]: added default documents
 *@@changed V0.9.4 (2000-06-09) [umoeller]: fixed separators
 */

BOOL mnuModifyDataFilePopupMenu(WPDataFile *somSelf,
                                HWND hwndMenu,
                                HWND hwndCnr,
                                ULONG iPosition)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    ULONG           ulVarMenuOfs = cmnQuerySetting(sulVarMenuOffset);

    /* if (cmnQuerySetting(sfExtAssocs))
    {
        // THIS HAS BEEN REMOVED FROM HERE;
        // XFldDataFile now handles extended file associations

        /// this doesn't work. VERY FUNNY, IBM. Look what
        // I got when I enumerated the submenu items in the "Open"
        // submenu IN THIS FUNCTION (WPDataFile).

        mnuModifyDataFilePopupMenu: removing 0x70 (Propertie~s)
        mnuModifyDataFilePopupMenu: removing 0x12F (~Icon view)
        mnuModifyDataFilePopupMenu: removing 0x7B (~Tree view)
        mnuModifyDataFilePopupMenu: removing 0x130 (~Details view)
        mnuModifyDataFilePopupMenu: removing 0x84 (~Program)
        mnuModifyDataFilePopupMenu: removing 0x13D (~Palette)
        mnuModifyDataFilePopupMenu: removing 0x67B (Date/~Time)

        // Now you guys explain why the **DATAFILE** "Open" menu
        // initially has "Palette" and "Tree view" menu items, and
        // where this is removed. COME ON.

        // get handle to "Open" submenu

        MENUITEM        mi;
        if (WinSendMsg(hwndMenu,
                       MM_QUERYITEM,
                       MPFROM2SHORT(WPMENUID_OPEN, TRUE),
                       (MPARAM)&mi))
        {
            // found:
            // find first item
            ULONG       ulItemID = 0;

            do
            {
                ulItemID = (ULONG)WinSendMsg(mi.hwndSubMenu,
                                             MM_ITEMIDFROMPOSITION,
                                             0,       // first item
                                             0);      // reserved
                if ((ulItemID) && (ulItemID != MIT_ERROR))
                {
                    PSZ pszItemText = winhQueryMenuItemText(mi.hwndSubMenu, ulItemID);
                    _Pmpf(("mnuModifyDataFilePopupMenu: removing 0x%lX (%s)",
                            ulItemID,
                            pszItemText));
                    free(pszItemText);

                    winhDeleteMenuItem(mi.hwndSubMenu, ulItemID);
                }
                else
                    break;

            } while (TRUE);
        }
    } */

    // insert separator V0.9.4 (2000-06-09) [umoeller]
    if (    (cmnQuerySetting(sfFileAttribs))
         || (cmnQuerySetting(sfAddCopyFilenameItem))
#ifndef __NOFDRDEFAULTDOCS__
         || (cmnQuerySetting(sfFdrDefaultDoc))
#endif
       )
        winhInsertMenuSeparator(hwndMenu,
                                MIT_END,
                                ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

    // insert "Attributes" submenu (for data files
    // only, not for folders
    if (cmnQuerySetting(sfFileAttribs))
    {
        ULONG ulAttr;
        HWND hwndAttrSubmenu;

        // get this file's file-system attributes
        ulAttr = _wpQueryAttr(somSelf);
        // insert submenu
        hwndAttrSubmenu = winhInsertSubmenu(hwndMenu,
                                            MIT_END,
                                            ulVarMenuOfs + ID_XFM_OFS_ATTRIBUTES,
                                            cmnGetString(ID_XFSI_ATTRIBUTES),
                                            0, // pszAttributes
        // "archived" item, checked or not according to file-system attributes
                                            ulVarMenuOfs + ID_XFMI_OFS_ATTR_ARCHIVED,
                                            cmnGetString(ID_XFSI_ATTR_ARCHIVE),
                                            MIS_TEXT,
                                            ((ulAttr & FILE_ARCHIVED) ? MIA_CHECKED : 0));
        // "read-only" item, checked or not according to file-system attributes
        winhInsertMenuItem(hwndAttrSubmenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_ATTR_READONLY,
                           cmnGetString(ID_XFSI_ATTR_READONLY),
                           MIS_TEXT, // pszAttrReadOnly
                           ((ulAttr & FILE_READONLY) ? MIA_CHECKED : 0));
        // "system" item, checked or not according to file-system attributes
        winhInsertMenuItem(hwndAttrSubmenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_ATTR_SYSTEM,
                           cmnGetString(ID_XFSI_ATTR_SYSTEM),
                           MIS_TEXT, // pszAttrSystem
                           ((ulAttr & FILE_SYSTEM) ? MIA_CHECKED : 0));
        // "hidden" item, checked or not according to file-system attributes
        winhInsertMenuItem(hwndAttrSubmenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_ATTR_HIDDEN,
                           cmnGetString(ID_XFSI_ATTR_HIDDEN),
                           MIS_TEXT, // pszAttrHidden
                           ((ulAttr & FILE_HIDDEN) ? MIA_CHECKED : 0));
    }

    // insert "Copy filename" for data files
    // (the XFolder class does this also)
    if (cmnQuerySetting(sfAddCopyFilenameItem))
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_COPYFILENAME_MENU,
                           cmnGetString(ID_XSSI_COPYFILENAME), // (pNLSStrings)->pszCopyFilename,
                           0,
                           0);

    // insert "Default document" if enabled
#ifndef __NOFDRDEFAULTDOCS__
    if (cmnQuerySetting(sfFdrDefaultDoc))
    {
        ULONG flAttr = 0;
        if (_xwpQueryDefaultDocument(_wpQueryFolder(somSelf)) == somSelf)
            // somSelf is default document of its folder:
            flAttr = MIA_CHECKED;

        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_FDRDEFAULTDOC,
                           cmnGetString(ID_XSSI_DATAFILEDEFAULTDOC), // (pNLSStrings)->pszDataFileDefaultDoc,
                           MIS_TEXT,
                           flAttr);
    }
#endif

    return (TRUE);
}

/* ******************************************************************
 *
 *   "Menu selected" entry points
 *
 ********************************************************************/

/*
 *@@  mnuProgramObjectSelected:
 *      this subroutine is called by mnuMenuItemSelected whenever a
 *      program object from the config folders is to be handled;
 *      it does all the necessary fuddling with the program object's
 *      data before opening it, ie. changing the working dir to the
 *      folder's, inserting clipboard data and so on.
 */

BOOL mnuProgramObjectSelected(WPObject *pFolder,        // in: folder or disk object
                              WPProgram *pProgram)
{
    PPROGDETAILS    pDetails;
    ULONG           ulSize;

    CHAR            szRealName[CCHMAXPATH];    // Buffer for wpQueryFilename()

    BOOL            ValidRealName,
                    StartupChanged = FALSE,
                    ParamsChanged = FALSE,
                    TitleChanged = FALSE,
                    brc = FALSE;

    PSZ             pszOldParams = NULL,
                    pszOldTitle = NULL;
    CHAR            szNewTitle[1024] = "";

    HAB             hab;
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // get program object data
    if ((pDetails = progQueryDetails(pProgram)))
    {
        XSTRING         strNewParams;       // V0.9.16 (2001-10-06)
        xstrInit(&strNewParams, 0);

        brc = TRUE;

        // dereference folder/disk shadows
        /* if (_somIsA(pFolder, _WPFolder))
            pFolder = somSelf; */

        // dereference disk objects
        if (_somIsA(pFolder, _WPDisk))
            pFolder = wpshQueryRootFolder(pFolder,      // disk
                                          FALSE,
                                          NULL);

        if (pFolder)
            ValidRealName = (_wpQueryFilename(pFolder, szRealName, TRUE) != NULL);
        // now we have the folder's full path

        // there seems to be a bug in wpQueryFilename for
        // root folders, so we might need to append a "\" */
        if (strlen(szRealName) == 2)
            strcat(szRealName, "\\");

        // *** first trick:
        // if the program object's startup dir has not been
        // set, we will set it to szRealName
        // temporarily; this will start the
        // program object in the directory
        // of the folder whose context menu was selected */
        if (    (ValidRealName)
             && (!pDetails->pszStartupDir))
        {
            StartupChanged = TRUE;
            pDetails->pszStartupDir = szRealName;
        }

        // start playing with the object's parameter list,
        // if the global settings allow it */
        if (cmnQuerySetting(sfAppdParam))
        {
            // CHAR            szNewParams[1024] = "";
            CHAR            szPassRealName[CCHMAXPATH];

            // if the folder's real name contains spaces,
            // we need to enclose it in quotes
            if (strchr(szRealName, ' '))
            {
                strcpy(szPassRealName, "\"");
                strcat(szPassRealName, szRealName);
                strcat(szPassRealName, "\"");
            }
            else
                strcpy(szPassRealName, szRealName);

            // backup prog data for later restore
            if (pszOldParams = pDetails->pszParameters)
            {
                // parameter list not empty:

                // *** second trick:
                // we will append the current folder path to the parameters
                // if the program object's parameter list does not
                // end in "%" ("Netscape support")
                if (    (ValidRealName)
                     && (pDetails->pszParameters[strlen(pDetails->pszParameters)-1] != '%'))
                {
                    ParamsChanged = TRUE;

                    xstrcpy(&strNewParams, pszOldParams, 0);
                    xstrcatc(&strNewParams, ' ');
                    xstrcat(&strNewParams, szPassRealName, 0);
                }

                // *** third trick:
                // replace an existing "%**C" in the parameters
                // with the contents of the clipboard */
                if (strstr(pszOldParams, CLIPBOARDKEY))
                {
                    hab = WinQueryAnchorBlock(HWND_DESKTOP);
                    if (WinOpenClipbrd(hab))
                    {
                        PSZ pszClipText;
                        if (pszClipText = (PSZ)WinQueryClipbrdData(hab, CF_TEXT))
                        {
                            CHAR            szClipBuf[CCHMAXPATH];
                            ULONG           ulOfs = 0;

                            PSZ pszPos = NULL;
                            // copy clipboard text from shared memory,
                            // but limit to 256 chars
                            strncpy(szClipBuf, pszClipText, CCHMAXPATH);
                            szClipBuf[CCHMAXPATH-2] = '\0'; // make sure the string is terminated
                            WinCloseClipbrd(hab);

                            if (!ParamsChanged) // did we copy already?
                                xstrcpy(&strNewParams, pszOldParams, 0);

                            while (xstrFindReplaceC(&strNewParams,
                                                    &ulOfs,
                                                    CLIPBOARDKEY,
                                                    szClipBuf))
                                ;

                            /* pszPos = strstr(szNewParams, CLIPBOARDKEY);
                            Ofs = strlen(szClipBuf);
                            if (Ofs + strlen(szNewParams) > CCHMAXPATH)
                                Ofs -= strlen(szNewParams);
                            strcpy(szClipBuf+Ofs, pszPos+strlen(CLIPBOARDKEY));
                            strcpy(pszPos, szClipBuf); */

                            ParamsChanged = TRUE;
                        }
                        else
                        {
                            // no text data in clipboard:
                            WinCloseClipbrd(hab);
                            cmnSetDlgHelpPanel(ID_XFH_NOTEXTCLIP);
                            if (WinDlgBox(HWND_DESKTOP,         // parent is desktop
                                          HWND_DESKTOP,             // owner is desktop
                                          (PFNWP)cmn_fnwpDlgWithHelp, // dialog procedure (common.c)
                                          cmnQueryNLSModuleHandle(FALSE),  // from resource file
                                          ID_XFD_NOTEXTCLIP,        // dialog resource id
                                          (PVOID)NULL)             // no dialog parameters
                                    == DID_CANCEL)
                                brc = FALSE;
                        }
                    }
                    else
                    {
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Unable to open clipboard.");
                        brc = FALSE;
                    }
                }

                if (ParamsChanged)
                    pDetails->pszParameters = strNewParams.psz;
            }
            else
                // parameter list is empty: simply set params
                if (ValidRealName)
                {
                    ParamsChanged = TRUE;
                    // set parameter list to folder name
                    pDetails->pszParameters = szPassRealName;
                    // since parameter list is empty, we need not
                    // search for the clipboard key ("%**C") */
                }
        } // end if (cmnQuerySetting(sfAppdParam))

        // now remove "~" from title, if allowed
        if (    (pszOldTitle = pDetails->pszTitle)
             && (cmnQuerySetting(sfRemoveX))
           )
        {
            PSZ pszPos;
            if (pszPos = strchr(pszOldTitle, '~'))
            {
                TitleChanged = TRUE;
                strncpy(szNewTitle, pszOldTitle, (pszPos - pszOldTitle));
                strcat(szNewTitle, pszPos+1);
                pDetails->pszTitle = szNewTitle;
            }
        }

        // now apply new settings, if necessary
        if (StartupChanged || ParamsChanged || TitleChanged)
            if (!_wpSetProgDetails(pProgram, pDetails))
            {
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Unable to set new startup directory.");
                brc = FALSE;
            }

        if (brc)
            // open the object with new settings
            if (!_wpViewObject(pProgram, NULLHANDLE, OPEN_DEFAULT, 0))
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "wpViewObject failed.");

        // now restore the old settings, if necessary
        if (StartupChanged)
            pDetails->pszStartupDir = NULL;
        if (ParamsChanged)
            pDetails->pszParameters = pszOldParams;
        if (TitleChanged)
            pDetails->pszTitle = pszOldTitle;
        if (StartupChanged || ParamsChanged || TitleChanged)
            _wpSetProgDetails(pProgram, pDetails);

        free(pDetails);
        xstrClear(&strNewParams);
    }
    else
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "wpQueryProgDetails failed.");

    return (brc);
}

/*
 *@@ CheckForVariableMenuItems:
 *      called from mnuMenuItemSelected for the default
 *      case, i.e. checks if one of the variable menu
 *      items (from config folder or folder content
 *      menus) was selected.
 *
 *      Must return TRUE if the menu item was processed.
 *
 *@@added V0.9.14 (2001-07-14) [umoeller]
 */

static BOOL CheckForVariableMenuItems(WPFolder *somSelf,  // in: folder or root folder
                                      HWND hwndFrame,    // in: as in wpMenuItemSelected
                                      ULONG ulMenuId)    // in: selected menu item
{
    BOOL brc = FALSE;

    PVARMENULISTITEM    pItem;
    WPObject            *pObject = NULL;

    ULONG ulFirstVarMenuId = cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_VARIABLE;
    if (     (ulMenuId >= ulFirstVarMenuId)
          && (ulMenuId <  ulFirstVarMenuId + G_ulVarItemCount)
       )
    {
        // yes, variable menu item selected:
        // get corresponding menu list item from the list that
        // was created by mnuModifyFolderPopupMenu
        pItem = cmnuGetVarItem(ulMenuId - ulFirstVarMenuId);

        if (pItem)
            pObject = pItem->pObject;

        if (pObject)    // defaults to NULL
        {
            // OK, we've found the corresponding object
            switch (pItem->ulObjType)
            {
                // this data has previously been saved by InsertObjectsFromList when
                // the context menu was created; it contains a flag telling us
                // what kind of menu item we're dealing with

                case OC_TEMPLATE:
                {
                    // if the object is a template, we create a new
                    // object from it; do this in the supplementary
                    // object window V0.9.2 (2000-02-26) [umoeller]
                    PSUBCLASSEDFOLDERVIEW psfv = fdrQuerySFV(hwndFrame,
                                                             NULL);
                    if (psfv)
                    {
                        XFolderData     *somThis = XFolderGetData(somSelf);

                        wpshCreateFromTemplate(WinQueryAnchorBlock(hwndFrame),
                                               pObject,  // template
                                               somSelf,    // folder
                                               hwndFrame, // view frame
                                               cmnQuerySetting(sulTemplatesOpenSettings),
                                                        // 0: do nothing after creation
                                                        // 1: open settings notebook
                                                        // 2: make title editable
                                               cmnQuerySetting(sfTemplatesReposition),
                                               &G_ptlMouseMenu); // V0.9.16 (2001-10-23) [umoeller]
                        /* G_ptlTemplateMousePos.x = _MenuMousePosX;
                        G_ptlTemplateMousePos.y = _MenuMousePosY;
                        WinPostMsg(psfv->hwndSupplObject,
                                   SOM_CREATEFROMTEMPLATE,
                                   (MPARAM)pObject, // template
                                   (MPARAM)somSelf); // folder
                                */
                    }
                }
                break;  // end OC_TEMPLATE

                case OC_PROGRAM:
                {
                    // WPPrograms are handled separately, for we will perform
                    // tricks on the startup directory and parameters */
                    mnuProgramObjectSelected(somSelf, pObject);
                }
                break;  // end OC_PROGRAM

                case OC_XWPSTRING:      // V0.9.14 (2001-08-25) [umoeller]
                {
                    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
                    if (    (krnIsClassReady(G_pcszXWPString))
                         && (_somIsA(pObject, _XWPString))
                       )
                        _xwpInvokeString(pObject,       // string object
                                         1,             // one target
                                         &somSelf);      // target: the folder
                }
                break;

                default:
                    // objects other than WPProgram and WPFolder (which is handled by
                    // the OS/2 menu handling) will simply be opened without further
                    // discussion.
                    // This includes folder content menu items,
                    // which are marked as OC_CONTENT; MB2 clicks into
                    // content menus are handled by the subclassed folder wnd proc
                    _wpViewObject(pObject, NULLHANDLE, OPEN_DEFAULT, 0);

            } // end switch
        } //end else (pObject == NULL)
        brc = TRUE;
    } // end if ((ulMenuId >= ID_XFM_VARIABLE) && (ulMenuId < ID_XFM_VARIABLE+varItemCount))
    // else none of our variable menu items: brc still false

    return (brc);
}

/*
 *@@ mnuMenuItemSelected:
 *      this gets called by XFolder::wpMenuItemSelected and
 *      XFldDisk::wpMenuItemSelected. Since both classes have
 *      most menu items in common, the handling of the
 *      selections can be done for both in one routine.
 *
 *      This routine now checks if one of XFolder's menu items
 *      was selected; if so, it executes corresponding action
 *      and returns TRUE, otherwise it does nothing and
 *      returns FALSE, upon which the caller should
 *      call its parent method to process the menu item.
 *
 *      Note that when called from XFldDisk, somSelf points
 *      to the "root folder" of the disk object instead of
 *      the disk object itself (wpQueryRootFolder).
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: "Refresh" item now moved to File thread
 *@@changed V0.9.1 (99-11-29) [umoeller]: "Open parent and close" closed even the Desktop; fixed
 *@@changed V0.9.1 (99-12-01) [umoeller]: "Open parent" crashed for root folders; fixed
 *@@changed V0.9.4 (2000-06-09) [umoeller]: added default document
 *@@changed V0.9.6 (2000-10-16) [umoeller]: fixed "Refresh now"
 *@@changed V0.9.9 (2001-03-27) [umoeller]: removed SOM_CREATEFROMTEMPLATE crap, now calling wpshCreateFromTemplate directly
 *@@changed V0.9.12 (2001-05-03) [umoeller]: removed "Partitions" for WPDrives
 */

BOOL mnuMenuItemSelected(WPFolder *somSelf,  // in: folder or root folder
                         HWND hwndFrame,    // in: as in wpMenuItemSelected
                         ULONG ulMenuId)    // in: selected menu item
{
    BOOL                brc = FALSE;     // "not processed" flag

    TRY_LOUD(excpt1)
    {
        // PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();
        ULONG               ulMenuId2 = ulMenuId - cmnQuerySetting(sulVarMenuOffset);

        if (somSelf)
        {
            BOOL        fDummy;

            /*
             *  "Sort" menu items:
             *
             */

            if (fdrSortMenuItemSelected(somSelf,
                                        hwndFrame,
                                        NULLHANDLE,     // we don't know the menu hwnd
                                        ulMenuId,
                                        &fDummy))
                brc = TRUE;

            // no sort menu item:
            // check other variable IDs
            else switch (ulMenuId2)
            {
                /*
                 * ID_XFMI_OFS_OPENPARTITIONS:
                 *      open WPDrives "Partitions" view
                 *
                 *  V0.9.2 (2000-02-29) [umoeller]
                 */

                /* case ID_XFMI_OFS_XWPVIEW:
                    partCreatePartitionsView(somSelf,
                                             ulMenuId);
                break; */ // disabled V0.9.12 (2001-05-03) [umoeller]

                /*
                 * ID_XFMI_OFS_FDRDEFAULTDOC:
                 *      open folder's default document
                 *
                 *  V0.9.4 (2000-06-09) [umoeller]
                 */

                case ID_XFMI_OFS_FDRDEFAULTDOC:
                {
                    WPFileSystem *pDefaultDoc = _xwpQueryDefaultDocument(somSelf);
                    if (pDefaultDoc)
                        _wpViewObject(pDefaultDoc, NULLHANDLE, OPEN_DEFAULT, 0);
                }
                break;

#ifndef __XWPLITE__
                /*
                 * ID_XFMI_OFS_PRODINFO:
                 *      "Product Information"
                 */

                case ID_XFMI_OFS_PRODINFO:
                    cmnShowProductInfo(NULLHANDLE,      // owner
                                       MMSOUND_SYSTEMSTARTUP);
                    brc = TRUE;
                break;
#endif

                /*
                 * ID_XFMI_OFS_SELECTSOME:
                 *      "Select by name"
                 */

                case ID_XFMI_OFS_SELECTSOME:
                {
                    HWND hwndSelectSome = WinLoadDlg(HWND_DESKTOP,    // parent
                                                     hwndFrame,         // owner
                                                     (PFNWP)fdr_fnwpSelectSome,
                                                     cmnQueryNLSModuleHandle(FALSE),
                                                     ID_XFD_SELECTSOME,
                                                     (PVOID)hwndFrame);    // dlg params
                    // cmnSetHelpPanel();
                    WinShowWindow(hwndSelectSome, TRUE);
                    brc = TRUE;
                }
                break;

                /*
                 * ID_XFMI_OFS_COPYFILENAME_SHORT:
                 * ID_XFMI_OFS_COPYFILENAME_FULL:
                 *      these are no real menu items, but only
                 *      pseudo-commands posted by the corresponding
                 *      folder hotkeys
                 */

                case ID_XFMI_OFS_COPYFILENAME_SHORT:
                case ID_XFMI_OFS_COPYFILENAME_FULL:
                {
                    // if the user presses hotkeys for "copy filename",
                    // we don't want the filename of the folder
                    // (which somSelf points to here...), but of the
                    // selected objects, so we repost the msg to
                    // the first selected object, which will handle
                    // the rest
                    HWND hwndCnr = wpshQueryCnrFromFrame(hwndFrame);
                    if (hwndCnr)
                    {
                        PMINIRECORDCORE pmrc = WinSendMsg(hwndCnr,
                                                          CM_QUERYRECORDEMPHASIS,
                                                          (MPARAM)CMA_FIRST, // query first
                                                          (MPARAM)CRA_SELECTED);
                        if ((pmrc != NULL) && ((ULONG)pmrc != -1))
                        {
                            // get object from record core
                            WPObject *pObject2 = OBJECT_FROM_PREC(pmrc);
                            if (pObject2)
                                wpshCopyObjectFileName(pObject2,
                                                       hwndFrame,
                                                       // full path:
                                                       (ulMenuId2 ==
                                                          ID_XFMI_OFS_COPYFILENAME_FULL));
                        }
                    }
                }
                break;

#ifndef __NOSNAPTOGRID__
                /*
                 * ID_XFMI_OFS_SNAPTOGRID:
                 *      "Snap to grid"
                 */

                case ID_XFMI_OFS_SNAPTOGRID:
                    fdrSnapToGrid(somSelf, TRUE);
                    brc = TRUE;
                break;
#endif

                /*
                 * ID_XFMI_OFS_OPENPARENT:
                 *      "Open parent folder":
                 *      only used by folder hotkeys also
                 */

                case ID_XFMI_OFS_OPENPARENT:
                {
                    WPFolder *pFolder = _wpQueryFolder(somSelf);
                    if (pFolder)
                        _wpViewObject(pFolder, NULLHANDLE, OPEN_DEFAULT, 0);
                    else
                        WinAlarm(HWND_DESKTOP, WA_WARNING);
                    brc = TRUE;
                }
                break;

                /*
                 * ID_XFMI_OFS_OPENPARENTANDCLOSE:
                 *      "open parent, close current"
                 *      only used by folder hotkeys also
                 */

                case ID_XFMI_OFS_OPENPARENTANDCLOSE:
                {
                    WPFolder *pFolder = _wpQueryFolder(somSelf);
                    if (pFolder)
                        _wpViewObject(pFolder, NULLHANDLE, OPEN_DEFAULT, 0);
                    else
                        WinAlarm(HWND_DESKTOP, WA_WARNING);
                    if (somSelf != cmnQueryActiveDesktop())
                        // fixed V0.9.0 (UM 99-11-29); before it was
                        // possible to close the Desktop...
                        _wpClose(somSelf);
                    brc = TRUE;
                }
                break;

                /*
                 * ID_XFMI_OFS_CONTEXTMENU:
                 *      "Show context menu":
                 *      only used by folder hotkeys also
                 */

                case ID_XFMI_OFS_CONTEXTMENU:
                {
                    HWND hwndCnr = wpshQueryCnrFromFrame(hwndFrame);
                    POINTS pts = {0, 0};
                    WinPostMsg(hwndCnr, WM_CONTEXTMENU,
                               (MPARAM)&pts,
                               MPFROM2SHORT(0, TRUE));
                    brc = TRUE;
                }
                break;

                /*
                 * ID_XFMI_OFS_REFRESH:
                 *      "Refresh now":
                 *      pass to File thread (V0.9.0)
                 */

                case ID_XFMI_OFS_REFRESH:
                    // we used to call _wpRefresh ourselves...
                    // apparently this wasn't such a good idea,
                    // because the WPS is doing a lot more things
                    // than just calling "Refresh". We get messed
                    // up container record cores if we just call
                    // _wpRefresh this way, so instead we post the
                    // WPS the command as if the item from the
                    // "View" submenu was selected...

                    WinPostMsg(hwndFrame,
                               WM_COMMAND,
                               MPFROMSHORT(WPMENUID_REFRESH),
                               MPFROM2SHORT(CMDSRC_MENU,
                                            FALSE));     // keyboard

                    brc = TRUE; // V0.9.11 (2001-04-22) [umoeller]
                break;

                /*
                 * ID_XFMI_OFS_CLOSE:
                 *      this is only used for the "close window"
                 *      folder hotkey;
                 *      repost sys command
                 */

                case ID_XFMI_OFS_CLOSE:
                    WinPostMsg(hwndFrame,
                               WM_SYSCOMMAND,
                               (MPARAM)SC_CLOSE,
                               MPFROM2SHORT(CMDSRC_MENU,
                                            FALSE));        // keyboard
                    brc = TRUE;
                break;

                /*
                 * ID_XFMI_OFS_BORED:
                 *      "[Config folder empty]" menu item...
                 *      show a msg box
                 */

                case ID_XFMI_OFS_BORED:
                    // explain how to configure XFolder
                    cmnMessageBoxMsg(HWND_DESKTOP, 116, 135, MB_OK);
                    brc = TRUE;
                break;


                /*
                 * ID_XFMI_OFS_RUN:
                 *      open Run dialog
                 *
                 *  V0.9.14 (2001-08-07) [pr]
                 */

                case ID_XFMI_OFS_RUN:
                    cmnRunCommandLine(NULLHANDLE, NULL);
                    brc = TRUE;
                break;

                /*
                 * default:
                 *      check for variable menu items
                 *      (ie. from config folder or folder
                 *      content menus)
                 */

                default:
                    // anything else: check if it's one of our variable menu items
                    brc = CheckForVariableMenuItems(somSelf,
                                                    hwndFrame,
                                                    ulMenuId);

            } // end switch;
        } // end if (somSelf)
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    return (brc);
    // this flag is FALSE by default; it signals to the caller (which
    // is wpMenuItemSelected of either XFolder or XFldDisk) whether the
    // parent method still needs to be called. If TRUE, we have processed
    // something, if FALSE, we haven't, then call the parent.
}

/*
 *@@ mnuMenuItemHelpSelected:
 *           display help for a context menu item; this routine is
 *           shared by all XFolder classes also, so you'll find
 *           XFldDesktop items in here too.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.4 (2000-08-03) [umoeller]: "View" submenu items never worked; fixed
 */

BOOL mnuMenuItemHelpSelected(WPObject *somSelf, ULONG MenuId)
{
    ULONG   ulFirstVarMenuId;
    ULONG   ulPanel = 0;
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    ULONG   ulMenuId2 = MenuId - cmnQuerySetting(sulVarMenuOffset);

    // first check for variable menu item IDs
    switch(ulMenuId2)
    {
        case ID_XFMI_OFS_RESTARTWPS:
            ulPanel = ID_XMH_RESTARTWPS;
        break;

        case ID_XFMI_OFS_SNAPTOGRID:
            ulPanel = ID_XMH_SNAPTOGRID;
        break;

        case ID_XFMI_OFS_REFRESH:
            ulPanel = ID_XMH_REFRESH;
        break;

        case ID_XFMI_OFS_SELECTSOME:
            ulPanel = ID_XFH_SELECTSOME;
        break;

        // items in "View" submenu
        case ID_XFMI_OFS_SMALLICONS:
        case ID_XFMI_OFS_FLOWED:
        case ID_XFMI_OFS_NONFLOWED:
        case ID_XFMI_OFS_NOGRID:
        case ID_XFMI_OFS_WARP4MENUBAR:
        case ID_XFMI_OFS_SHOWSTATUSBAR:
            ulPanel = ID_XFH_VIEW_MENU_ITEMS;
        break;

        case ID_XFMI_OFS_COPYFILENAME_MENU:
            ulPanel = ID_XMH_COPYFILENAME;
        break;

        case ID_XFMI_OFS_SORTBYEXT:
        case ID_XFMI_OFS_SORTBYCLASS:
        case ID_XFMI_OFS_ALWAYSSORT:
        case ID_XFMI_OFS_SORTFOLDERSFIRST:
            ulPanel = ID_XSH_SETTINGS_FLDRSORT;
        break;

        default:
            // none of the variable item ids:
#ifndef __NOXSHUTDOWN__
            if (    (MenuId == WPMENUID_SHUTDOWN)
                 && (cmnQuerySetting(sfXShutdown))
               )
                ulPanel = ID_XMH_XSHUTDOWN;
            else
#endif
                 if (
#ifndef __ALWAYSEXTSORT__
                        (cmnQuerySetting(sfExtendedSorting))
                     &&
#endif
                        (   (MenuId == ID_WPMI_SORTBYNAME)
                         || (MenuId == ID_WPMI_SORTBYREALNAME)
                         // or one of the details columns:
                         || (    (MenuId >= 6002)
                              && (MenuId <= 6200)
                            )
                        )
                    )
                    ulPanel = ID_XSH_SETTINGS_FLDRSORT;
            else
            {
                // if F1 was pressed over one of the variable menu items,
                // open a help panel with generic help on XFolder
                ulFirstVarMenuId = (cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_VARIABLE);
                if ( (MenuId >= ulFirstVarMenuId)
                        && (MenuId < ulFirstVarMenuId + G_ulVarItemCount)
                     )
                {
                    PVARMENULISTITEM pItem = cmnuGetVarItem(MenuId - ulFirstVarMenuId);

                    if (pItem)
                    {
                        // OK, we've found the corresponding object
                        switch (pItem->ulObjType)
                        {
                            // this data has previously been saved by InsertObjectsFromList when
                            // the context menu was created; it contains a flag telling us
                            // what kind of menu item we're dealing with

                            case OC_CONTENTFOLDER:
                            case OC_CONTENT:
                                ulPanel = ID_XMH_FOLDERCONTENT;
                            break;

                            default:
                                ulPanel = ID_XMH_VARIABLE;
                            break;
                        }
                    }
                }
            }
        break;
    }

    if (ulPanel)
    {
        // now open the help panel we've set above
        cmnDisplayHelp(somSelf,
                       ulPanel);
        return TRUE;
    }
    else
        // none of our items: pass on to parent
        return FALSE;
}

/* ******************************************************************
 *                                                                  *
 *   "Selecting" menu items functions                               *
 *                                                                  *
 ********************************************************************/

/*
 *  The following functions are called on objects even before
 *  wpMenuItemSelected is called, i.e. right after a
 *  menu item has been selected by the user, and before
 *  the menu is dismissed.
 */

/*
 *@@ mnuFileSystemSelectingMenuItem:
 *      this is called for file-system objects (folders and
 *      data files) even before wpMenuItemSelected.
 *
 *      This call is the result of a WM_MENUSELECT intercept
 *      of the subclassed frame window procedure of an open folder
 *      (fdr_fnwpSubclassedFolderFrame, xfldr.c).
 *
 *      We can intercept certain menu item selections here so
 *      that they are not passed to wpMenuItemSelected. This is
 *      the only way we can react to a menu selection and _not_
 *      dismiss the menu (ie. keep it visible after the selection).
 *
 *      Note that somSelf might be a file-system object, but
 *      it might also be a shadow pointing to one, so we might
 *      need to dereference it.
 *
 *      Return value:
 *      -- TRUE          the menu item was handled here; in this case,
 *                       we set *pfDismiss to either TRUE or FALSE.
 *                       If TRUE, the menu will be dismissed and, if
 *                       if (fPostCommand), wpMenuItemSelected will be
 *                       called later.
 *                       If FALSE, the menu will _not_ be dismissed,
 *                       and wpMenuItemSelected will _not_ be called
 *                       later. This is what we return for menu items
 *                       that we have handled here already.
 *      -- FALSE         the menu item was _not_ handled.
 *                       We do _not_ touch *pfDismiss then. In any
 *                       case, wpMenuItemSelected will be called.
 *
 *@@changed V0.9.4 (2000-06-09) [umoeller]: added default documents
 */

BOOL mnuFileSystemSelectingMenuItem(WPObject *somSelf,
                                       // in: file-system object on which the menu was
                                       // opened
                                    USHORT usItem,
                                       // in: selected menu item
                                    BOOL fPostCommand,
                                       // in: this signals whether wpMenuItemSelected
                                       // can be called afterwards
                                    HWND hwndMenu,
                                       // in: current menu control
                                    HWND hwndCnr,
                                       // in: cnr hwnd involved in the operation
                                    ULONG ulSelection,
                                       // one of the following:
                                       // -- SEL_WHITESPACE: the context menu was opened on the
                                       //                   whitespace in an open container view
                                       //                   of somSelf (which is a folder then)
                                       // -- SEL_SINGLESEL: the context menu was opened for a
                                       //                   single selected object: somSelf can
                                       //                   be any object then, including folders
                                       // -- SEL_MULTISEL:   the context menu was opened on one
                                       //                   of a multitude of selected objects.
                                       //                   Again, somSelf can be any object
                                       // -- SEL_SINGLEOTHER: the context menu was opened for a
                                       //                   single object _other_ than the selected
                                       //                   objects
                                    BOOL *pfDismiss)
                                       // out: if TRUE is returned (ie. the menu item was handled
                                       // here), this determines whether the menu should be dismissed
{
    // PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();
    ULONG               ulMenuId2 = usItem - cmnQuerySetting(sulVarMenuOffset);
    BOOL                fHandled = TRUE;
    // BOOL                brc = TRUE;     // "dismiss menu" flag

    WPObject *pObject = somSelf;
    WPFileSystem *pFileSystem = pObject;

    if (_somIsA(pObject, _WPShadow))
        pFileSystem = _wpQueryShadowedObject(pObject, TRUE);

    #ifdef DEBUG_MENUS
        _Pmpf(("mnuFileSystemSelectingMenuItem"));
    #endif

    switch (ulMenuId2)
    {

        /*
         * ID_XFMI_OFS_ATTR_ARCHIVED etc.:
         *      update file attributes
         */

        case ID_XFMI_OFS_ATTR_ARCHIVED:
        case ID_XFMI_OFS_ATTR_SYSTEM:
        case ID_XFMI_OFS_ATTR_HIDDEN:
        case ID_XFMI_OFS_ATTR_READONLY:
        {
            ULONG       ulFileAttr;
            ULONG       ulMenuAttr;
            HPOINTER    hptrOld;

            ulFileAttr = _wpQueryAttr(pFileSystem);
            ulMenuAttr = (ULONG)WinSendMsg(hwndMenu,
                                           MM_QUERYITEMATTR,
                                           MPFROM2SHORT(usItem, FALSE),
                                           (MPARAM)MIA_CHECKED);
            // toggle "checked" flag in menu
            ulMenuAttr ^= MIA_CHECKED;  // XOR checked flag;
            WinSendMsg(hwndMenu,
                       MM_SETITEMATTR,
                       MPFROM2SHORT(usItem, FALSE),
                       MPFROM2SHORT(MIA_CHECKED, ulMenuAttr));

            // toggle file attribute
            ulFileAttr ^= // XOR flag depending on menu item
                      (ulMenuId2 == ID_XFMI_OFS_ATTR_ARCHIVED) ? FILE_ARCHIVED
                    : (ulMenuId2 == ID_XFMI_OFS_ATTR_SYSTEM  ) ? FILE_SYSTEM
                    : (ulMenuId2 == ID_XFMI_OFS_ATTR_HIDDEN  ) ? FILE_HIDDEN
                    : FILE_READONLY;

            // loop thru the selected objects
            // change the mouse pointer to "wait" state
            hptrOld = winhSetWaitPointer();

            while (pObject)
            {
                if (pFileSystem)
                {
                    #ifdef DEBUG_MENUS
                        _Pmpf(("  Settings attrs for %s", _wpQueryTitle(pFileSystem)));
                    #endif

                    _wpSetAttr(pFileSystem, ulFileAttr);

                    // update open "File" notebook pages for this object
                    ntbUpdateVisiblePage(pFileSystem, SP_FILE1);
                }

                if (ulSelection == SEL_MULTISEL)
                    pObject = wpshQueryNextSourceObject(hwndCnr, pObject);
                        // note that we're passing pObject, which might
                        // be the shadow
                else
                    pObject = NULL;

                // dereference shadows again
                pFileSystem = pObject;
                if (pObject)
                    if (_somIsA(pObject, _WPShadow))
                        pFileSystem = _wpQueryShadowedObject(pObject, TRUE);
            }

            WinSetPointer(HWND_DESKTOP, hptrOld);

            // prevent dismissal of menu
            *pfDismiss = FALSE;
        }
        break;  // file attributes

        /*
         * ID_XFMI_OFS_COPYFILENAME_MENU:
         *
         */

        case ID_XFMI_OFS_COPYFILENAME_MENU:
            wpshCopyObjectFileName(pObject,
                                   hwndCnr,
                                   doshQueryShiftState());
                // note again that we're passing pObject instead
                // of pFileSystem, so that this routine can
                // query all selected objects from shadows too

            // dismiss menu
            *pfDismiss = TRUE;
        break;

        /*
         * ID_XFMI_OFS_FDRDEFAULTDOC:
         *      V0.9.4 (2000-06-09) [umoeller]
         */

        case ID_XFMI_OFS_FDRDEFAULTDOC:
        {
            WPFolder *pMyFolder = _wpQueryFolder(somSelf);
            if (_xwpQueryDefaultDocument(pMyFolder) == somSelf)
                // we are already the default document:
                // unset
                _xwpSetDefaultDocument(pMyFolder, NULL);
            else
                _xwpSetDefaultDocument(pMyFolder, somSelf);
        }
        break;

        default:
            fHandled = FALSE;
    }

    return (fHandled);
}

/*
 *@@ mnuFolderSelectingMenuItem:
 *      this is called for folders before wpMenuItemSelected.
 *      See mnuFileSystemSelectingMenuItem for details.
 *
 *      Note that somSelf here will never be a shadow pointing
 *      to a folder. It will always be a folder.
 *
 *@@changed V0.9.0 [umoeller]: added support for "Menu bar" item
 */

BOOL mnuFolderSelectingMenuItem(WPFolder *somSelf,
                                USHORT usItem,
                                BOOL fPostCommand,
                                HWND hwndMenu,
                                HWND hwndCnr,
                                ULONG ulSelection,
                                BOOL *pfDismiss)
{
    // XFolderData *somThis = XFolderGetData(somSelf);

    // PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();
    ULONG               ulMenuId2 = usItem - cmnQuerySetting(sulVarMenuOffset);
    BOOL                fHandled = TRUE;
    // BOOL                brc = TRUE;     // "dismiss menu" flag
    // USHORT              usfAlwaysSort, usDefaultSort;

    #ifdef DEBUG_MENUS
        _Pmpf(("mnuFolderSelectingMenuItem"));
    #endif

    // first check if it's one of the "Sort" menu items
    fHandled = fdrSortMenuItemSelected(somSelf,
                                       WinQueryWindow(hwndCnr,
                                                      QW_PARENT), // frame window
                                       hwndMenu,
                                       usItem,
                                       pfDismiss);   // dismiss flag == return value

    if (!fHandled)
    {
        fHandled = TRUE;

        // no "sort" menu item:
        switch (ulMenuId2)
        {
            /*
             * ID_XFMI_OFS_SMALLICONS:
             *
             */

            case ID_XFMI_OFS_SMALLICONS:
            {
                // toggle small icons for folder; this menu item
                // only exists for open Icon and Tree views
                CNRINFO CnrInfo;
                ULONG ulViewAttr, ulCnrView;
                ULONG ulMenuAttr = (ULONG)WinSendMsg(hwndMenu,
                                                     MM_QUERYITEMATTR,
                                                     MPFROM2SHORT(usItem,
                                                                  FALSE),
                                                     (MPARAM)MIA_CHECKED);
                // toggle "checked" flag in menu
                ulMenuAttr ^= MIA_CHECKED;  // XOR checked flag;
                WinSendMsg(hwndMenu,
                           MM_SETITEMATTR,
                           MPFROM2SHORT(usItem, FALSE),
                           MPFROM2SHORT(MIA_CHECKED, ulMenuAttr));

                // toggle cnr flags
                cnrhQueryCnrInfo(hwndCnr, &CnrInfo);
                ulCnrView = (CnrInfo.flWindowAttr & CV_TREE) ? OPEN_TREE : OPEN_CONTENTS;
                ulViewAttr = _wpQueryFldrAttr(somSelf, ulCnrView);
                ulViewAttr ^= CV_MINI;      // XOR mini-icons flag
                _wpSetFldrAttr(somSelf,
                               ulViewAttr,
                               ulCnrView);

                *pfDismiss = FALSE;
            }
            break;

            case ID_XFMI_OFS_FLOWED:
            case ID_XFMI_OFS_NONFLOWED:
            case ID_XFMI_OFS_NOGRID:
            {
                // these items exist for icon views only
                ULONG ulViewAttr = _wpQueryFldrAttr(somSelf, OPEN_CONTENTS);
                switch (ulMenuId2)
                {
                    case ID_XFMI_OFS_FLOWED:
                        // == CV_NAME | CV_FLOW; not CV_ICON
                        ulViewAttr = (ulViewAttr & ~CV_ICON) | CV_NAME | CV_FLOW;
                    break;

                    case ID_XFMI_OFS_NONFLOWED:
                        // == CV_NAME only; not CV_ICON
                        ulViewAttr = (ulViewAttr & ~(CV_ICON | CV_FLOW)) | CV_NAME;
                    break;

                    case ID_XFMI_OFS_NOGRID:
                        ulViewAttr = (ulViewAttr & ~(CV_NAME | CV_FLOW)) | CV_ICON;
                    break;
                }

                _wpSetFldrAttr(somSelf,
                               ulViewAttr,
                               OPEN_CONTENTS);

                winhSetMenuItemChecked(hwndMenu,
                                       cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_FLOWED,
                                       (ulMenuId2 == ID_XFMI_OFS_FLOWED));
                winhSetMenuItemChecked(hwndMenu,
                                       cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_NONFLOWED,
                                       (ulMenuId2 == ID_XFMI_OFS_NONFLOWED));
                winhSetMenuItemChecked(hwndMenu,
                                       cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_NOGRID,
                                       (ulMenuId2 == ID_XFMI_OFS_NOGRID));

                // do not dismiss menu
                *pfDismiss = FALSE;
            }
            break;

            case ID_XFMI_OFS_SHOWSTATUSBAR:
            {
                // toggle status bar for folder
                ULONG ulMenuAttr = (ULONG)WinSendMsg(hwndMenu,
                                                     MM_QUERYITEMATTR,
                                                     MPFROM2SHORT(usItem,
                                                                  FALSE),
                                                     (MPARAM)MIA_CHECKED);
                _xwpSetStatusBarVisibility(somSelf,
                                           (ulMenuAttr & MIA_CHECKED)
                                              ? STATUSBAR_OFF
                                              : STATUSBAR_ON,
                                           TRUE);  // update open folder views

                // toggle "checked" flag in menu
                ulMenuAttr ^= MIA_CHECKED;  // XOR checked flag;
                WinSendMsg(hwndMenu,
                           MM_SETITEMATTR,
                           MPFROM2SHORT(usItem, FALSE),
                           MPFROM2SHORT(MIA_CHECKED, ulMenuAttr));

                // do not dismiss menu
                *pfDismiss = FALSE;
            }
            break;

            /*
             * ID_XFMI_OFS_WARP4MENUBAR (added V0.9.0):
             *      "Menu bar" item in "View" context submenu.
             *      This is only inserted if Warp 4 is running,
             *      so we do no additional checks here.
             */

            case ID_XFMI_OFS_WARP4MENUBAR:
            {
                BOOL fMenuVisible = _xwpQueryMenuBarVisibility(somSelf);
                // in order to change the menu bar visibility,
                // we need to resolve the method by name, since
                // we don't have the Warp 4 toolkit headers
                xfTD_wpSetMenuBarVisibility pwpSetMenuBarVisibility
                    = (xfTD_wpSetMenuBarVisibility)somResolveByName(
                                                    somSelf,
                                                    "wpSetMenuBarVisibility");

                if (pwpSetMenuBarVisibility)
                    pwpSetMenuBarVisibility(somSelf, !fMenuVisible);
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                        "Unable to resolve wpSetMenuBarVisibility.");

                WinSendMsg(hwndMenu,
                           MM_SETITEMATTR,
                           MPFROM2SHORT(usItem, FALSE),
                           MPFROM2SHORT(MIA_CHECKED,
                                        (fMenuVisible)
                                            ? 0
                                            : MIA_CHECKED));
                // do not dismiss menu
                *pfDismiss = FALSE;
            }
            break;

            default:
                fHandled = FALSE;
        }
    }

    return (fHandled);
}

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c) for XFldWPS "Menu" pages
 *
 ********************************************************************/

static CONTROLDEF
    FileMenusGroup = CONTROLDEF_GROUP(
                            LOAD_STRING, // "File menus",
                            ID_XSDI_FILEMENUS_GROUP,
                            -1,
                            -1),
    AttributesCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Add \"A~ttributes\" menu",
                            ID_XSDI_FILEATTRIBS,
                            -1,
                            -1),
    CopyFilenameCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "~Add \"Copy filename\"",
                            ID_XSDI_COPYFILENAME,
                            -1,
                            -1),

    FolderMenusGroup = CONTROLDEF_GROUP(
                            LOAD_STRING, // "Folder menus",
                            ID_XSDI_FOLDERMENUS_GROUP,
                            -1,
                            -1),
#ifndef __NOMOVEREFRESHNOW__
    RefreshNowCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Add \"~Refresh now\" to main menu",
                            ID_XSDI_MOVE4REFRESH,
                            -1,
                            -1),
#endif
    SelectSomeCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Add ""Se~lect by name""",
                            ID_XSDI_SELECTSOME,
                            -1,
                            -1),
    FolderViewsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Extend ""~View"" menu",
                            ID_XSDI_FLDRVIEWS,
                            -1,
                            -1)
#ifndef __NOFOLDERCONTENTS__
    ,
    FolderContentsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Add f~older content menus",
                            ID_XSDI_FOLDERCONTENT,
                            -1,
                            -1),
    FolderContentsIconsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Show ~icons",
                            ID_XSDI_FC_SHOWICONS,
                            -1,
                            -1)
#endif
    ;

static const DLGHITEM dlgAddMenus[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),       // row 1 in the root table, required
                // create group on top
                START_GROUP_TABLE(&FileMenusGroup),
                    START_ROW(0),
                        CONTROL_DEF(&AttributesCB),
                    START_ROW(0),
                        CONTROL_DEF(&CopyFilenameCB),
                END_TABLE,      // end of group
            START_ROW(0),       // row 2 in the root table
                START_GROUP_TABLE(&FolderMenusGroup),
#ifndef __NOMOVEREFRESHNOW__
                    START_ROW(0),
                        CONTROL_DEF(&RefreshNowCB),
#endif
                    START_ROW(0),
                        CONTROL_DEF(&SelectSomeCB),
                    START_ROW(0),
                        CONTROL_DEF(&FolderViewsCB),
#ifndef __NOFOLDERCONTENTS__
                    START_ROW(0),
                        CONTROL_DEF(&FolderContentsCB),
                    START_ROW(0),
                        CONTROL_DEF(&FolderContentsIconsCB),
#endif
                END_TABLE,      // end of group
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_UndoButton),         // notebook.c
                CONTROL_DEF(&G_DefaultButton),      // notebook.c
                CONTROL_DEF(&G_HelpButton),         // notebook.c
        END_TABLE
    };

static const XWPSETTING G_AddMenusBackup[] =
    {
        sfFileAttribs,
        sfAddCopyFilenameItem,
        sfExtendFldrViewMenu,
#ifndef __NOMOVEREFRESHNOW__
        sfMoveRefreshNow,
#endif
        sfAddSelectSomeItem,
#ifndef __NOFOLDERCONTENTS__
        sfAddFolderContentItem,
        sfFolderContentShowIcons
#endif
    };

/*
 *@@ mnuAddMenusInitPage:
 *      notebook callback function (notebook.c) for the
 *      first "Context Menus" page in "Workplace Shell"
 *      object ("Add menu items").
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.16 (2001-09-29) [umoeller]: now using dialog formatter
 */

VOID mnuAddMenusInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                             ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            /* pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
               */
            pcnbp->pUser = cmnBackupSettings(G_AddMenusBackup,
                                             ARRAYITEMCOUNT(G_AddMenusBackup));
            // insert the controls using the dialog formatter
            // V0.9.16 (2001-09-29) [umoeller]
            ntbFormatPage(pcnbp->hwndDlgPage,
                          dlgAddMenus,
                          ARRAYITEMCOUNT(dlgAddMenus));
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FILEATTRIBS,
                              cmnQuerySetting(sfFileAttribs));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_COPYFILENAME,
                              cmnQuerySetting(sfAddCopyFilenameItem));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FLDRVIEWS,
                              cmnQuerySetting(sfExtendFldrViewMenu));
#ifndef __NOMOVEREFRESHNOW__
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOVE4REFRESH,
                              cmnQuerySetting(sfMoveRefreshNow));
#endif
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SELECTSOME,
                              cmnQuerySetting(sfAddSelectSomeItem));
#ifndef __NOFOLDERCONTENTS__
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FOLDERCONTENT,
                              cmnQuerySetting(sfAddFolderContentItem));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FC_SHOWICONS,
                              cmnQuerySetting(sfFolderContentShowIcons));
#endif

        /* winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_EXTENDCLOSEMENU,
                              cmnQuerySetting(sfExtendCloseMenu));
                // V0.9.12 (2001-05-22) [umoeller]
                */
    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fViewVisible =
        G_fIsWarp4 = doshIsWarp4();
        fViewVisible =
               (    ( (G_fIsWarp4)  && (cmnQuerySetting(sfRemoveViewMenu) == 0) )
                 || ( (!G_fIsWarp4) && ((cmnQuerySetting(sflDefaultMenuItems) & CTXT_SELECT) == 0)
               ));
#ifndef __NOFOLDERCONTENTS__
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FOLDERCONTENT,
                         !cmnQuerySetting(sfNoSubclassing));
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FC_SHOWICONS,
                         !cmnQuerySetting(sfNoSubclassing));
#endif
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_SELECTSOME, fViewVisible);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FLDRVIEWS, fViewVisible);
    }
}

/*
 *@@ mnuAddMenusItemChanged:
 *      notebook callback function (notebook.c) for the
 *      first "Context Menus" page in "Workplace Shell"
 *      object ("Add menu items").
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

MRESULT mnuAddMenusItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG ulItemID,
                               USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MRESULT)0;
    BOOL fSave = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_FILEATTRIBS:
            cmnSetSetting(sfFileAttribs, ulExtra);
        break;

        case ID_XSDI_COPYFILENAME:
            cmnSetSetting(sfAddCopyFilenameItem, ulExtra);
        break;

        case ID_XSDI_FLDRVIEWS:
            cmnSetSetting(sfExtendFldrViewMenu, ulExtra);
        break;

#ifndef __NOMOVEREFRESHNOW__
        case ID_XSDI_MOVE4REFRESH:
            cmnSetSetting(sfMoveRefreshNow, ulExtra);
        break;
#endif

        case ID_XSDI_SELECTSOME:
            cmnSetSetting(sfAddSelectSomeItem, ulExtra);
        break;

#ifndef __NOFOLDERCONTENTS__
        case ID_XSDI_FOLDERCONTENT:
            cmnSetSetting(sfAddFolderContentItem, ulExtra);
        break;

        case ID_XSDI_FC_SHOWICONS:
            cmnSetSetting(sfFolderContentShowIcons, ulExtra);
        break;
#endif

        /* case ID_XSDI_EXTENDCLOSEMENU:
            cmnSetSetting(sfExtendCloseMenu, ulExtra);
                // V0.9.12 (2001-05-22) [umoeller]
        break; */

        case DID_UNDO:
        {
            // "Undo" button: restore the settings for this page
            cmnRestoreSettings(pcnbp->pUser,
                               ARRAYITEMCOUNT(G_AddMenusBackup));

            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        default:
            fSave = FALSE;
    }

    // cmnUnlockGlobalSettings();

    // if (fSave)
        // cmnStoreGlobalSettings();

    return (mrc);
}

static const XWPSETTING G_ConfigFolderMenusBackup[] =
    {
        sfMenuCascadeMode,
        sfRemoveX,
        sfAppdParam,
        sulTemplatesOpenSettings,
        sfTemplatesReposition
    };

/*
 *@@ mnuConfigFolderMenusInitPage:
 *      notebook callback function (notebook.c) for the
 *      second "Context Menus" page in "Workplace Shell"
 *      object ("Config folder items").
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

VOID mnuConfigFolderMenusInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                                      ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = cmnBackupSettings(G_ConfigFolderMenusBackup,
                                             ARRAYITEMCOUNT(G_ConfigFolderMenusBackup));
            /*
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
            */
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_CASCADE,
                              cmnQuerySetting(sfMenuCascadeMode));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_REMOVEX,
                              cmnQuerySetting(sfRemoveX));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_APPDPARAM,
                              cmnQuerySetting(sfAppdParam));

        switch (cmnQuerySetting(sulTemplatesOpenSettings))
        {
            case 0:  winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_TPL_DONOTHING, 1); break;
            case 1:  winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_TPL_OPENSETTINGS, 1); break;
            default:  winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_TPL_EDITTITLE, 1); break;
        }

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_TPL_POSITION,
                              cmnQuerySetting(sfTemplatesReposition));
    }
}

/*
 *@@ mnuConfigFolderMenusItemChanged:
 *      notebook callback function (notebook.c) for the
 *      second "Context Menus" page in "Workplace Shell"
 *      object ("Config folder items").
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

MRESULT mnuConfigFolderMenusItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG ulItemID,
                                        USHORT usNotifyCode,
                                        ULONG ulExtra)      // for checkboxes: contains new state
{
    // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MRESULT)0;
    BOOL fSave = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_CASCADE:
            cmnSetSetting(sfMenuCascadeMode, ulExtra);
        break;

        case ID_XSDI_REMOVEX:
            cmnSetSetting(sfRemoveX, ulExtra);
        break;

        case ID_XSDI_APPDPARAM:
            cmnSetSetting(sfAppdParam, ulExtra);
        break;

        // "create from templates" settings
        case ID_XSDI_TPL_DONOTHING:
            cmnSetSetting(sulTemplatesOpenSettings, 0);
        break;

        case ID_XSDI_TPL_EDITTITLE:
            cmnSetSetting(sulTemplatesOpenSettings, BM_INDETERMINATE);
        break;

        case ID_XSDI_TPL_OPENSETTINGS:
            cmnSetSetting(sulTemplatesOpenSettings, BM_CHECKED);
        break;

        case ID_XSDI_TPL_POSITION:
            cmnSetSetting(sfTemplatesReposition, ulExtra);
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            // PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            cmnRestoreSettings(pcnbp->pUser,
                               ARRAYITEMCOUNT(G_ConfigFolderMenusBackup));
            /*
            cmnSetSetting(sfMenuCascadeMode, pGSBackup->MenuCascadeMode);
            cmnSetSetting(sfRemoveX, pGSBackup->RemoveX);
            cmnSetSetting(sfAppdParam, pGSBackup->AppdParam);
            cmnSetSetting(sulTemplatesOpenSettings, pGSBackup->TemplatesOpenSettings);
            cmnSetSetting(sfTemplatesReposition, pGSBackup->TemplatesReposition);
               */

            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        default:
            fSave = FALSE;
    }

    // cmnUnlockGlobalSettings();

    /* if (fSave)
        cmnStoreGlobalSettings(); */

    return (mrc);
}

static const XWPSETTING G_RemoveMenusBackup[] =
    {
        sflDefaultMenuItems,
        sfRemoveViewMenu,
        sfRemovePasteItem,
        sfRemoveLockInPlaceItem,
        sfFixLockInPlace,
        sfRemoveCheckDiskItem,
        sfRemoveFormatDiskItem
    };

/*
 *@@ mnuRemoveMenusInitPage:
 *      notebook callback function (notebook.c) for the
 *      third "Context Menus" page in "Workplace Shell"
 *      object ("Remove menu items").
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.7 (2000-12-10) [umoeller]: added "fix lock in place"
 */

VOID mnuRemoveMenusInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                                ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            /*
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
            */
            pcnbp->pUser = cmnBackupSettings(G_RemoveMenusBackup,
                                             ARRAYITEMCOUNT(G_RemoveMenusBackup));
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_HELP  ,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_HELP) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_CRANOTHER,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_CRANOTHER) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_COPY  ,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_COPY     ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOVE  ,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_MOVE     ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SHADOW,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_SHADOW   ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DELETE,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_DELETE   ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_PICKUP,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_PICKUP   ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FIND  ,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_FIND     ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SELECT,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_SELECT   ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SORT  ,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_SORT     ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARRANGE,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_ARRANGE ) == 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_PRINT  ,
                              (cmnQuerySetting(sflDefaultMenuItems) & CTXT_PRINT   ) == 0);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_WARP4DISPLAY,
                              !cmnQuerySetting(sfRemoveViewMenu));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_INSERT,
                              !cmnQuerySetting(sfRemovePasteItem));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_LOCKINPLACE,
                              !cmnQuerySetting(sfRemoveLockInPlaceItem));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_LOCKINPLACE_NOSUB,
                              cmnQuerySetting(sfFixLockInPlace));  // V0.9.7 (2000-12-10) [umoeller]
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_CHECKDISK,
                              !cmnQuerySetting(sfRemoveCheckDiskItem));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FORMATDISK,
                              !cmnQuerySetting(sfRemoveFormatDiskItem));
    }

    if (flFlags & CBI_ENABLE)
    {
        // disable items for Warp 3/4
        if (doshIsWarp4())
        {
            winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_SELECT, FALSE);
            winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_LOCKINPLACE_NOSUB,
                             !cmnQuerySetting(sfRemoveLockInPlaceItem));
        }
        else
        {
            winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_LOCKINPLACE, FALSE);
            winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_LOCKINPLACE_NOSUB, FALSE);
            winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_WARP4DISPLAY, FALSE);
            winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_INSERT, FALSE);
        }
    }
}

/*
 *@@ mnuRemoveMenusItemChanged:
 *      notebook callback function (notebook.c) for the
 *      third "Context Menus" page in "Workplace Shell"
 *      object ("Remove menu items").
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.7 (2000-12-10) [umoeller]: added "fix lock in place"
 */

MRESULT mnuRemoveMenusItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                  ULONG ulItemID,
                                  USHORT usNotifyCode,
                                  ULONG ulExtra)      // for checkboxes: contains new state
{
    // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MRESULT)0;
    BOOL fSave = TRUE;

    ULONG flChange = 0;

    switch (ulItemID)
    {
        case ID_XSDI_HELP:
            flChange = CTXT_HELP;
        break;

        case ID_XSDI_CRANOTHER:
            flChange = CTXT_CRANOTHER;
        break;

        case ID_XSDI_COPY:
            flChange = CTXT_COPY;
        break;

        case ID_XSDI_MOVE:
            flChange = CTXT_MOVE;
        break;

        case ID_XSDI_SHADOW:
            flChange = CTXT_SHADOW;
        break;

        case ID_XSDI_DELETE:
            flChange = CTXT_DELETE;
        break;

        case ID_XSDI_PICKUP:
            flChange = CTXT_PICKUP;
        break;

        case ID_XSDI_FIND:
            flChange = CTXT_FIND;
        break;

        case ID_XSDI_SELECT:
            flChange = CTXT_SELECT;
        break;

        case ID_XSDI_SORT:
            flChange = CTXT_SORT;
        break;

        case ID_XSDI_ARRANGE:
            flChange = CTXT_ARRANGE;
        break;

        case ID_XSDI_PRINT:
            flChange = CTXT_PRINT;
        break;

        case ID_XSDI_WARP4DISPLAY:
            cmnSetSetting(sfRemoveViewMenu, 1-ulExtra);
        break;

        case ID_XSDI_INSERT:
            cmnSetSetting(sfRemovePasteItem, 1-ulExtra);
        break;

        case ID_XSDI_LOCKINPLACE:
            cmnSetSetting(sfRemoveLockInPlaceItem, 1-ulExtra);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE); // V0.9.7 (2000-12-10) [umoeller]
        break;

        case ID_XSDI_LOCKINPLACE_NOSUB:  // V0.9.7 (2000-12-10) [umoeller]
            cmnSetSetting(sfFixLockInPlace, ulExtra);
        break;

        case ID_XSDI_CHECKDISK:
            cmnSetSetting(sfRemoveCheckDiskItem, 1-ulExtra);
        break;

        case ID_XSDI_FORMATDISK:
            cmnSetSetting(sfRemoveFormatDiskItem, 1-ulExtra);
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            // PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            cmnRestoreSettings(pcnbp->pUser,
                               ARRAYITEMCOUNT(G_RemoveMenusBackup));
            /*
            cmnSetSetting(sflDefaultMenuItems, pGSBackup->DefaultMenuItems);
            cmnSetSetting(sfRemoveViewMenu, pGSBackup->RemoveViewMenu);
            cmnSetSetting(sfRemovePasteItem, pGSBackup->RemovePasteItem);
            cmnSetSetting(sfRemoveLockInPlaceItem, pGSBackup->RemoveLockInPlaceItem);
            cmnSetSetting(sfFixLockInPlace, pGSBackup->fFixLockInPlace);
            cmnSetSetting(sfRemoveCheckDiskItem, pGSBackup->RemoveCheckDiskItem);
            cmnSetSetting(sfRemoveFormatDiskItem, pGSBackup->RemoveFormatDiskItem);
               */
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        default:
            fSave = FALSE;
    }

    if (flChange)
    {
        ULONG fl = cmnQuerySetting(sflDefaultMenuItems);
        // note, these are reverse
        if (ulExtra)
            fl &= ~flChange;
        else
            fl |= flChange;
        cmnSetSetting(sflDefaultMenuItems, fl);
    }

    // cmnUnlockGlobalSettings();

    /* if (fSave)
        cmnStoreGlobalSettings(); */

    return (mrc);
}


