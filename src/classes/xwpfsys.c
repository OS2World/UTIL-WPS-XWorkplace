
/*
 *@@sourcefile xwpfsys.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPFileSystem (WPFileSystem replacement)
 *
 *      This class replaces the WPFileSystem class.
 *      This is all new with V0.9.5 but was only experimental
 *      at that point.
 *
 *      Starting with V0.9.16, installation of this class is
 *      required for the new folder content hacks to work.
 *
 *@@added V0.9.5 [umoeller]
 *
 *@@somclass XWPFileSystem xfs_
 *@@somclass M_XWPFileSystem xfsM_
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M�ller.
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

#ifndef SOM_Module_xwpfsys_Source
#define SOM_Module_xwpfsys_Source
#endif
#define XWPFileSystem_Class_Source
#define M_XWPFileSystem_Class_Source

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
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_WINPOINTERS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines

// SOM headers which don't crash with prec. header files
#include "xwpfsys.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\fhandles.h"           // replacement file-system handles management
#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\filetype.h"           // extended file types implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\icons.h"              // icons handling
// #include "filesys\object.h"             // XFldObject implementation

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *
 *  Global variables
 *
 ********************************************************************/

static BOOL     G_fReplaceHandles = FALSE;

/* ******************************************************************
 *
 *  XWPFileSystem instance methods
 *
 ********************************************************************/

/*
 *@@ xwpQueryUpperRealName:
 *      returns the current real name of the object in
 *      upper case, which can then be used for fast
 *      string comparisons, e.g. in the new folder
 *      content trees (V0.9.16).
 *
 *      Precondition:
 *
 *      --  somSelf must be fully initialized.
 *
 *      --  The caller must hold the folder mutex sem of
 *          somSelf's folder while calling this.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

SOM_Scope PSZ  SOMLINK xfs_xwpQueryUpperRealName(XWPFileSystem *somSelf)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_xwpQueryUpperRealName");

    if (!_pWszUpperRealName)
    {
        // not queried yet:
        // create a copy
        CHAR sz[CCHMAXPATH];
        if (_wpQueryFilename(somSelf, sz, FALSE))
        {
            ULONG ulLength;
            wpshStore(somSelf, &_pWszUpperRealName, sz, &ulLength);
            nlsUpper(_pWszUpperRealName);
        }
    }

    return _pWszUpperRealName;
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 */

SOM_Scope void  SOMLINK xfs_wpInitData(XWPFileSystem *somSelf)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpInitData");

    XWPFileSystem_parent_WPFileSystem_wpInitData(somSelf);

    _ulHandle = 0;
    _pWszUpperRealName = NULL;
    _ulCnrRefresh = -1;
}

/*
 *@@ UnInitData:
 *      this WPObject instance method is called when the object
 *      is destroyed as a SOM object, either because it's being
 *      made dormant or being deleted. All allocated resources
 *      should be freed here.
 *      The parent method must always be called last.
 */

SOM_Scope void  SOMLINK xfs_wpUnInitData(XWPFileSystem *somSelf)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpUnInitData");

    wpshStore(somSelf, &_pWszUpperRealName, NULL, NULL);

    XWPFileSystem_parent_WPFileSystem_wpUnInitData(somSelf);
}

/*
 *@@ wpQueryHandle:
 *      overridden for debugging.
 *
 *@@added V0.9.20 (2002-08-04) [umoeller]
 */

SOM_Scope HOBJECT  SOMLINK xfs_wpQueryHandle(XWPFileSystem *somSelf)
{
#ifdef __DEBUG__
    CHAR    szFilename[CCHMAXPATH];
#endif

    // XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpQueryHandle");

#ifdef __DEBUG__
    if (_wpQueryFilename(somSelf, szFilename, TRUE))
        PMPF_PROGRAMSTART(("[%s]", szFilename));
#endif

    return XWPFileSystem_parent_WPFileSystem_wpQueryHandle(somSelf);
}

/*
 *@@ wpQueryIcon:
 *      this WPObject instance method returns the HPOINTER
 *      with the current icon of the object. For some WPS
 *      classes, icon loading is deferred until the first
 *      call to this method.
 *      See icons.c for an introduction.
 *
 *      WPDataFile overrides this method to defer icon loading
 *      for data files until the icon is first needed.
 *      See XFldDataFile::wpQueryIcon for more about that.
 *
 *      However, WPFolder does _not_ override this method, so
 *      for folders, the WPFileSystem implementation gets called.
 *      Unfortunately, that implementation is very expensive.
 *      Even though (as with all file-system objects) the icon
 *      data from the .ICON EA is passed to
 *      WPFolder::wpRestoreState, the WPS apparently doesn't
 *      even check the buffer but checks the icon EAs _again_
 *      in this method, which isn't exactly speedy. What we
 *      can do here safely is check if an icon was set in
 *      our XFolder::wpRestoreState override (which does parse
 *      the FEA2LIST) and if not, load a default icon here.
 *
 *      I'd love to have shared this implementation with
 *      XFldDataFile, but since WPDataFile overrides this
 *      method, we have to override that override again
 *      for XFldDataFile.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope HPOINTER  SOMLINK xfs_wpQueryIcon(XWPFileSystem *somSelf)
{
    // XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpQueryIcon");

#ifndef __NOTURBOFOLDERS__
    if (cmnQuerySetting(sfTurboFolders))
    {
        HPOINTER hptrReturn = NULLHANDLE;
        PMINIRECORDCORE prec = _wpQueryCoreRecord(somSelf);
        if (!(hptrReturn = prec->hptrIcon))
        {
            // first call, and icon wasn't set in wpRestoreState:
            // use class default icon
            // (again, note that we override this for XFldDataFile
            // so this really only affects folders)
            if (hptrReturn = _wpclsQueryIcon(_somGetClass(somSelf)))
            {
                _wpSetIcon(somSelf, hptrReturn);
                _wpModifyStyle(somSelf,
                               OBJSTYLE_NOTDEFAULTICON,
                               0);
            }
        }

        // V0.9.18 (2002-03-24) [umoeller]
        // fixed to never call the parent any more,
        // which nukes our shared icons sometimes
        return hptrReturn;
    }

#endif

    return XWPFileSystem_parent_WPFileSystem_wpQueryIcon(somSelf);
}

/*
 *@@ wpSetIconData:
 *      this WPObject method sets a new persistent icon for
 *      the object and stores the new icon permanently.
 *
 *      We need to override this for our icon replacements
 *      because the WPS will do evil things to our standard
 *      icons again.
 *
 *      Note that XFldProgramFile and XWPProgramFile override
 *      this too to support their better ICON_CLEAR case.
 *
 *@@added V0.9.18 (2002-03-19) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfs_wpSetIconData(XWPFileSystem *somSelf,
                                          PICONINFO pIconInfo)
{
    CHAR        szFilename[CCHMAXPATH];
    HPOINTER    hptrNew = NULLHANDLE;

    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpSetIconData");

    if (icomRunReplacement())
    {
        BOOL    brc = FALSE;
        BOOL    fNotDefaultIcon = FALSE;

        if (    (pIconInfo)
             && (_wpQueryFilename(somSelf, szFilename, TRUE))
           )
        {
            switch (pIconInfo->fFormat)
            {
                case ICON_DATA:
                case ICON_RESOURCE:
                case ICON_FILE:
                    // WinSetFileIcon supports this
                    if (WinSetFileIcon(szFilename, pIconInfo))
                    {
                        // change the visible icon too
                        if (hptrNew = WinLoadFileIcon(szFilename, FALSE))
                            fNotDefaultIcon = TRUE;

                        brc = TRUE;
                    }
                break;

                case ICON_CLEAR:
                    // WinSetFileIcon(ICON_CLEAR) clears the .ICON
                    // EA for us, so no need to worry...
                    // note, this case is now also overridden by
                    // XFldDataFile and XWPProgramFile, so effectively
                    // this code only ever gets used by folders
                    if (WinSetFileIcon(szFilename, pIconInfo))
                    {
                        // use class default icon
                        hptrNew = _wpclsQueryIcon(_somGetClass(somSelf));
                        brc = TRUE;
                    }
                break;
            }
        }

        if (hptrNew)
        {
            _wpSetIcon(somSelf, hptrNew);
            _wpModifyStyle(somSelf,
                           OBJSTYLE_NOTDEFAULTICON,
                           (fNotDefaultIcon) ? OBJSTYLE_NOTDEFAULTICON : 0);
        }

        return brc;
    }

    return XWPFileSystem_parent_WPFileSystem_wpSetIconData(somSelf,
                                                           pIconInfo);
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
 *      We check for OPEN_SETTINGS here because
 *      WPFileSystem::wpOpen evilly refreshes the file-system
 *      data under our back and keeps nuking our replacement
 *      icons. As a result, we call our refresh replacement
 *      instead.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 *@@changed V0.9.19 (2002-04-14) [umoeller]: if turbo was off, opening settings could take ages on folder; fixed
 */

SOM_Scope HWND  SOMLINK xfs_wpOpen(XWPFileSystem *somSelf, HWND hwndCnr,
                                   ULONG ulView, ULONG param)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpOpen");

#ifndef __NOTURBOFOLDERS__
    if (    (ulView == OPEN_SETTINGS)
         // do this only if turbo folders are on!
         // V0.9.19 (2002-04-14) [umoeller]
         && (cmnQuerySetting(sfTurboFolders))
       )
    {
        somTD_WPObject_wpOpen pwpOpen;

        // now call the WPObject method by skipping the WPFileSystem
        // implementation
        if (pwpOpen = (somTD_WPObject_wpOpen)wpshResolveFor(
                                     somSelf,
                                     _WPObject,     // class to resolve for
                                     "wpOpen"))
        {
            // this allows us to replace the method, dammit
            // _wpRefresh(somSelf,
            _wpRefreshFSInfo(somSelf,       // V1.0.1 (2002-12-15) [umoeller]
                             NULLHANDLE,      // view
                             NULL,           // reserved
                             0);

            return pwpOpen(somSelf, hwndCnr, ulView, param);
        }
    }
#endif

    return XWPFileSystem_parent_WPFileSystem_wpOpen(somSelf,
                                                    hwndCnr,
                                                    ulView,
                                                    param);
}

/*
 *@@ wpSetTitleAndRenameFile:
 *      this WPFileSystem method is responsible for setting
 *      the real name _and_ the title of a file-system object.
 *
 *      WPFileSystem::wpSetTitle only calls this method and
 *      does nothing else. In other words, this method is
 *      the implementation for WPFileSystem::wpSetTitle;
 *      fConfirmations is then set to what _wpQueryConfirmations
 *      returns.
 *      Apparently, is the only place in the WPS which
 *      actually renames a file-system object on disk.
 *
 *      If "turbo folders" are enabled, we must update the
 *      content tree of our folder.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 *@@changed V0.9.19 (2002-04-17) [umoeller]: moved code to XWPFileSystem::wpSetRealName to fix rename cases
 */

SOM_Scope BOOL  SOMLINK xfs_wpSetTitleAndRenameFile(XWPFileSystem *somSelf,
                                                    PSZ pszNewTitle,
                                                    ULONG fConfirmations)
{
    BOOL        brc;
    BOOL        fFolderLocked = FALSE;
    WPFolder    *pMyFolder;
    CHAR        szFolder[CCHMAXPATH];

    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpSetTitleAndRenameFile");

    PMPF_TURBOFOLDERS(("new title is \"%s\"", STRINGORNULL(pszNewTitle)));

/*
#ifndef __NOTURBOFOLDERS__
    if (    (cmnQuerySetting(sfTurboFolders))
            // we only need to handle the case where the object's name
            // is being changed, so skip if we aren't even initialized yet
         && (objIsObjectInitialized(somSelf))
            // we can't handle UNC yet
         && (pMyFolder = _wpQueryFolder(somSelf))
         && (_wpQueryFilename(pMyFolder, szFolder, TRUE))
         && (szFolder[0] != '\\')
         && (szFolder[1] != '\\')
       )
    {
        TRY_LOUD(excpt1)
        {
            if (    (brc = XWPFileSystem_parent_WPFileSystem_wpSetTitleAndRenameFile(somSelf,
                                                                                     pszNewTitle,
                                                                                     fConfirmations))
                 && (fFolderLocked = !fdrRequestFolderWriteMutexSem(pMyFolder))
               )
            {
                // update the folder's contents tree
                // (this also changes our instance data!)
                fdrRealNameChanged(pMyFolder,
                                   somSelf);
            }
        }
        CATCH(excpt1)
        {
            brc = FALSE;
        } END_CATCH();

        if (fFolderLocked)
            fdrReleaseFolderWriteMutexSem(pMyFolder);

        return brc;
    } // end if (cmnQuerySetting(sfTurboFolders))
#endif
*/

    brc = XWPFileSystem_parent_WPFileSystem_wpSetTitleAndRenameFile(somSelf,
                                                                    pszNewTitle,
                                                                    fConfirmations);

    PMPF_TURBOFOLDERS(("exiting, rc = %d", brc));

    return brc;
}

/*
 *@@ wpSetRealName:
 *      this WPFileSystem instance method sets the real name
 *      for the object.
 *
 *      This code normally only gets called by WPS-internal
 *      implementations to update the internal representation
 *      of the file. When this gets called, the file has
 *      already been renamed on disk, I think.
 *
 *      In detail, I think the following code calls this:
 *
 *      --  WPFileSystem::wpRestoreState (during initialization);
 *
 *      --  WPFileSystem::wpMoveObject if wpConfirmObjectTitle
 *          returned with NAMECLASH_RENAME during a move
 *          operation;
 *
 *      --  WPFileSystem::wpSetTitleAndRenameFile to set the
 *          new real name for the object on rename.
 *
 *      Be warned, WPDataFile also overrides this to reset
 *      the association icon, so we have added
 *      XFldDataFile::wpSetRealName as well.
 *
 *      If "turbo folders" are enabled, we must update the
 *      content tree of our folder which is based on real
 *      names.
 *
 *      Before V0.9.19, we used to override wpSetTitleAndRenameFile
 *      which worked for 95% of the cases, but not when a file
 *      conflict occured because in that case, this method got
 *      called behind our back, rendering our tree all wrong and
 *      leading to duplicate entries sometimes.
 *      WPFileSystem::wpSetTitleAndRenameFile calls this in turn,
 *      so apparently we're safe here too.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfs_wpSetRealName(XWPFileSystem *somSelf,
                                          PSZ pszName)
{
    BOOL        brc;
    BOOL        fFolderLocked = FALSE;
    WPFolder    *pMyFolder;
    CHAR        szFolder[CCHMAXPATH];

    // XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpSetRealName");

    PMPF_TURBOFOLDERS(("new real name is \"%s\"", STRINGORNULL(pszName)));

#ifndef __NOTURBOFOLDERS__
    if (    (cmnQuerySetting(sfTurboFolders))
            // we only need to handle the case where the object's name
            // is being changed, so skip if we aren't even initialized yet
            // V0.9.20 (2002-08-08) [umoeller]
            // not quite true, this method gets called when objects are
            // moved and renamed because of a title clash, so then we
            // need to set or real name too
         // && (objIsObjectInitialized(somSelf))
            // we can't handle UNC yet
         && (pMyFolder = _wpQueryFolder(somSelf))
         && (_wpQueryFilename(pMyFolder, szFolder, TRUE))
         && (szFolder[0] != '\\')
         && (szFolder[1] != '\\')
       )
    {
        PMPF_TURBOFOLDERS(("    obj is inited, calling fdrRealNameChanged"));

        TRY_LOUD(excpt1)
        {
            if (    (brc = XWPFileSystem_parent_WPFileSystem_wpSetRealName(somSelf,
                                                                           pszName))
                 && (fFolderLocked = !fdrRequestFolderWriteMutexSem(pMyFolder))
               )
            {
                // update the folder's contents tree
                // (this also changes our instance data!)
                fdrRealNameChanged(pMyFolder,
                                   somSelf);
            }
        }
        CATCH(excpt1)
        {
            brc = FALSE;
        } END_CATCH();

        if (fFolderLocked)
            fdrReleaseFolderWriteMutexSem(pMyFolder);

        return brc;
    } // end if (cmnQuerySetting(sfTurboFolders))
#endif

    brc = XWPFileSystem_parent_WPFileSystem_wpSetRealName(somSelf,
                                                          pszName);

    PMPF_TURBOFOLDERS(("exiting, rc = %d", brc));

    return brc;
}

/*
 *@@ wpRefreshFSInfo:
 *      this unpublished WPFileSystem instance method is supposed
 *      to refresh the object with new file-system information.
 *
 *      Note: This method is not in the public IBM wpfsys.idl file.
 *      To override this, we have our special wpfsys.idl file in
 *      our own idl\wps directory, which is put on SMINCLUDE first.
 *
 *      From my testing, WPFileSystem::wpRefresh does nothing but
 *      calling this method.
 *
 *      Unfortunately, the WPFileSystem implementation keeps killing
 *      our executable icons, so we need to override this behavior.
 *
 *@@added V0.9.20 (2002-07-25) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfs_wpRefreshFSInfo(XWPFileSystem *somSelf,
                                            ULONG ulView,
                                            PVOID pReserved,
                                            ULONG ulUnknown)
{
    // XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpRefreshFSInfo");

#ifndef __NOTURBOFOLDERS__
    if (cmnQuerySetting(sfTurboFolders))
        return !fsysRefresh(somSelf, pReserved);
#endif

    return XWPFileSystem_parent_WPFileSystem_wpRefreshFSInfo(somSelf,
                                                             ulView,
                                                             pReserved,
                                                             ulUnknown);
}

/* ******************************************************************
 *
 *  M_XWPFileSystem class methods
 *
 ********************************************************************/

VOID cmnEnableTurboFolders(VOID);

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 */

SOM_Scope void  SOMLINK xfsM_wpclsInitData(M_XWPFileSystem *somSelf)
{
    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsInitData");

    if (krnClassInitialized(G_pcszXWPFileSystem))
    {
#ifndef __NOTURBOFOLDERS__
        // now enable the real turbo folders setting,
        // which _requires_ this class; after this
        // call cmnQuerySetting(sfTurboFolders) may
        // return TRUE if the user also enabled the
        // setting
        cmnEnableTurboFolders();
#endif
    }

#ifdef __REPLHANDLES__
    // query once at system startup whether handles
    // management has been replaced
    if (G_fReplaceHandles = cmnQuerySetting(sfReplaceHandles))
        // enabled:
        // initialize handles
        fhdlLoadHandles();
#endif

    M_XWPFileSystem_parent_M_WPFileSystem_wpclsInitData(somSelf);

    // go register instance types and filters
    if (ftypRegisterInstanceTypesAndFilters(somSelf))
        // got any:
        // do not allow this class to be unloaded!!
        _wpclsIncUsage(somSelf);
}

/*
 *@@ wpclsQueryAwakeObject:
 *      this WPFileSystem class method determines whether a
 *      a file-system object is already awake.
 *
 *      From my testing, this method is never actually
 *      called in the WPS, but my own code calls it in
 *      some places. If "turbo folders" are on, we use
 *      our own high-speed functions instead of calling
 *      the parent.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

SOM_Scope WPObject*  SOMLINK xfsM_wpclsQueryAwakeObject(M_XWPFileSystem *somSelf,
                                                        PSZ pszInputPath)
{
    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsQueryAwakeObject");

#ifndef __NOTURBOFOLDERS__
    if (    (cmnQuerySetting(sfTurboFolders))
            // we can't handle UNC yet
         && (pszInputPath[0] != '\\')
         && (pszInputPath[1] != '\\')
       )
        return fdrQueryAwakeFSObject(pszInputPath);
#endif

    return M_XWPFileSystem_parent_M_WPFileSystem_wpclsQueryAwakeObject(somSelf,
                                                                       pszInputPath);
}


/*
 *@@ wpclsFileSysExists:
 *      tests "Folder" whether a file-system object with
 *      the real name pszFilename is already awake in the
 *      folder. If so, it is returned; otherwise, we
 *      return NULL.
 *
 *      We replace this method also if "turbo folders"
 *      are on.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope WPObject*  SOMLINK xfsM_wpclsFileSysExists(M_XWPFileSystem *somSelf,
                                                     WPFolder* Folder,
                                                     PSZ pszFilename,
                                                     ULONG attrFile)
{
    WPObject *pAwake;
    CHAR    szFolder[CCHMAXPATH];
    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsFileSysExists");

#ifndef __NOTURBOFOLDERS__
    if (    (cmnQuerySetting(sfTurboFolders))
            // we can't handle UNC yet
         && (_wpQueryFilename(Folder, szFolder, TRUE))
         && (szFolder[0] != '\\')
         && (szFolder[1] != '\\')
       )
    {
        // alright, apparently we got something:
        // check if it is already awake (using the
        // fast content tree functions)
        if (pAwake = fdrSafeFindFSFromName(Folder,
                                           pszFilename))
        {
            if ((_wpQueryAttr(pAwake) & FILE_DIRECTORY) == (attrFile & FILE_DIRECTORY))
                _wpSetRefreshFlags(pAwake,
                                    (_wpQueryRefreshFlags(pAwake)
                                        & ~DIRTYBIT)
                                        | FOUNDBIT);
            else
                pAwake = NULL;
        }
    }
    else
#endif
        pAwake = M_XWPFileSystem_parent_M_WPFileSystem_wpclsFileSysExists(somSelf,
                                                                          Folder,
                                                                          pszFilename,
                                                                          attrFile);

    return pAwake;
}


/*
 *@@ wpclsObjectFromHandle:
 *      this class method returns the object that corresponds
 *      to the given handle or NULL if the handle is invalid.
 *
 *      WPSREF doesn't say so, but this is really an implementation
 *      method and should not be called directly. Instead, one
 *      should always use M_WPObject::wpclsQueryObject, which
 *      figures out which base class the handle belongs to and
 *      then invokes _this_ method on the proper base class.
 *
 *      So WPFileSystem::wpclsObjectFromHandle, which we override
 *      here, only ever gets called for file-system handles.
 *
 *      @@todo
 *      Eventually we must override this method once we can
 *      replace the WPS file-handles engine. For now, THIS METHOD
 *      IS NOT OPERATING CORRECTLY because if the object for the
 *      given handle is not awake yet, it is made awake here,
 *      which bypasses or new populate code. I believe this method
 *      is the reason that sometimes objects awakened independently
 *      from populate are of the wrong class and sometimes are not
 *      recorded in our folder trees either.
 *
 *@@added V0.9.19 (2002-06-12) [umoeller]
 */

SOM_Scope WPObject*  SOMLINK xfsM_wpclsObjectFromHandle(M_XWPFileSystem *somSelf,
                                                        HOBJECT hObject)
{
    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsObjectFromHandle");

    // _PmpfF(("HOBJECT 0x%lX", hObject));
    return M_XWPFileSystem_parent_M_WPFileSystem_wpclsObjectFromHandle(somSelf,
                                                                       hObject);
}

