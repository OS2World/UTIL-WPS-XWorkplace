
/*
 *@@sourcefile xwppgm.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPProgram (WPProgram replacement)
 *
 *      XFldProgram is only responsible for overriding the
 *      "Associations" page at this point.
 *
 *      Installation of this class is optional.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 *@@somclass XWPProgram xpg_
 *@@somclass M_XWPProgram xpgM_
 */

/*
 *      Copyright (C) 2001 Ulrich M�ller.
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

#ifndef SOM_Module_xwppgm_Source
#define SOM_Module_xwppgm_Source
#endif
#define XWPProgram_Class_Source
#define M_XWPProgram_Class_Source

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
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSSESMGR          // DosQueryAppType
#define INCL_DOSERRORS

#define INCL_WINPOINTERS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
#include "helpers\dosh.h"
#include "helpers\except.h"             // exception handling
#include "helpers\exeh.h"               // executable helpers
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"
#include "xfdataf.ih"
#include "xwppgm.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filetype.h"           // extended file types implementation
#include "filesys\icons.h"              // icons handling
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros

#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   XWPProgram instance methods
 *
 ********************************************************************/

/*
 *@@ xwpAddAssociationsPage:
 *      this new XWPProgram method adds our replacement
 *      "Associations" page to an executable's settings notebook.
 *
 *      Gets called from our
 *      XWPProgram::wpAddProgramAssociationPage override,
 *      if extended associations are enabled.
 */

SOM_Scope ULONG  SOMLINK xpg_xwpAddAssociationsPage(XWPProgram *somSelf,
                                                     HWND hwndNotebook)
{
    ULONG ulrc = 0;
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xwppgm_xwpAddAssociationsPage");

#ifndef __NEVEREXTASSOCS__
    ulrc = ftypInsertAssociationsPage(somSelf,
                                      hwndNotebook);
#endif
    return (ulrc);
}

/*
 *@@ xwpQuerySetup2:
 *      this XFldObject method is overridden to support
 *      setup strings for program files.
 *
 *      This uses code in filesys\filesys.c which is
 *      shared for WPProgram and WPProgramFile instances.
 *
 *      See XFldObject::xwpQuerySetup2 for details.
 *
 *@@changed V0.9.16 (2001-10-11) [umoeller]: adjusted to new implementation
 */

SOM_Scope BOOL  SOMLINK xpg_xwpQuerySetup2(XWPProgram *somSelf,
                                            PVOID pstrSetup)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xwppgm_xwpQuerySetup2");

    // call implementation
    if (progQuerySetup(somSelf, pstrSetup))
    {
        // manually resolve parent method
        return (wpshParentQuerySetup2(somSelf,
                                      _somGetParent(_XWPProgram),
                                      pstrSetup));
    }

    return (FALSE);
}

/*
 *@@ xwpNukePhysical:
 *      override of XFldObject::xwpNukePhysical, which must
 *      remove the physical representation of an object
 *      when it gets physically deleted.
 *
 *      xwpNukePhysical gets called by name from
 *      XFldObject::wpFree. The default XFldObject::xwpNukePhysical
 *      calls WPObject::wpDestroyObject.
 *
 *      We override this in order to prevent the original
 *      WPProgram::wpDestroyObject to be called, which messes
 *      wrongly with our association data. Instead,
 *      we destroy any association data in the OS2.INI
 *      file ourselves and them jump directly to
 *      WPAbstract::wpDestroyObject.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_xwpNukePhysical(XWPProgram *somSelf)
{
    static xfTD_wpDestroyObject pWPAbstract_wpDestroyObject = NULL;

    BOOL brc = FALSE;
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_xwpNukePhysical");

    if (!pWPAbstract_wpDestroyObject)
    {
        // first call:
        // resolve WPAbstract::wpDestroyObject
        // (skip WPProgram parent call!)
        pWPAbstract_wpDestroyObject
            = (xfTD_wpDestroyObject)wpshResolveFor(somSelf,
                                                   _WPAbstract,
                                                   "wpDestroyObject");
    }

    if (pWPAbstract_wpDestroyObject)
    {
        // clean up program resources in INI file;
        // there's no way to avoid running through
        // the entire handles cache, unfortunately...
        // _wpQueryHandle is safe, since each WPProgram
        // must have one per definition
        ftypAssocObjectDeleted(_wpQueryHandle(somSelf));

        // call WPAbstract::wpDestroyObject explicitly,
        // skipping WPProgram
        brc = pWPAbstract_wpDestroyObject(somSelf);
    }
    else
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Cannot resolve WPAbstract::wpDestroyObject.");

    return (brc);
}

/*
 *@@ xwpQueryExecutable:
 *      this new XWPProgram instance method writes the
 *      executable that this program represents into
 *      the specified buffer, which must be CCHMAXPATH
 *      in size.
 *
 *      This method is handy if you only need the program's
 *      executable and do not want to go thru the overhead
 *      of calling wpQueryProgDetails.
 *
 *      Returns FALSE if the internal executable data is
 *      empty or invalid.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_xwpQueryExecutable(XWPProgram *somSelf,
                                               PSZ pszBuffer)
{
    HOBJECT     hobj;
    WPObject    *pobj;

    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_xwpQueryExecutable");

    if (_pulExecutableHandle)
    {
        if (*_pulExecutableHandle == 0xFFFF)
        {
            // command line
            strcpy(pszBuffer, "*");
            return (TRUE);
        }
        else if (_pszExecutable)
        {
            // executable string present: use that instead
            strhncpy0(pszBuffer, _pszExecutable, CCHMAXPATH);
            return (TRUE);
        }
        else if (   (hobj = *_pulExecutableHandle)
                 && (pobj = _wpclsQueryObject(_WPObject,
                                              hobj | (G_usHiwordFileSystem << 16)))
                 && (_somIsA(pobj, _WPFileSystem))
                 && (_wpQueryFilename(pobj, pszBuffer, TRUE))
                )
            return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope void  SOMLINK xpg_wpInitData(XWPProgram *somSelf)
{
    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpInitData");

    XWPProgram_parent_WPProgram_wpInitData(somSelf);

    _fNeedsSetProgIcon = FALSE;
    _pvpszEnvironment = NULL;
    _pSWPInitial = NULL;
    _pulExecutableHandle = NULL;
    _pulStartupDirHandle = NULL;
    _pProgType = NULL;
    _pszExecutable = NULL;
    _pszParameters = NULL;
}

/*
 *@@ wpUnInitData:
 *      this WPObject instance method is called when the object
 *      is destroyed as a SOM object, either because it's being
 *      made dormant or being deleted. All allocated resources
 *      should be freed here.
 *      The parent method must always be called last.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope void  SOMLINK xpg_wpUnInitData(XWPProgram *somSelf)
{
    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpUnInitData");

    FREE(_pszExecutable);
    FREE(_pszParameters);

    XWPProgram_parent_WPProgram_wpUnInitData(somSelf);
}

/*
 *@@ wpSaveState:
 *      this WPObject instance method saves an object's state
 *      persistently so that it can later be re-initialized
 *      with wpRestoreState. This gets called during wpClose,
 *      wpSaveImmediate or wpSaveDeferred processing.
 *      All persistent instance variables should be stored here.
 *
 *@@added V0.9.12 (2001-05-26) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpSaveState(XWPProgram *somSelf)
{
    BOOL brc;
    // XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpSaveState");

    brc = XWPProgram_parent_WPProgram_wpSaveState(somSelf);

    return (brc);
}

/*
 *@@ wpRestoreState:
 *      this WPObject instance method gets called during object
 *      initialization (after wpInitData) to restore the data
 *      which was stored with wpSaveState.
 *
 *@@added V0.9.12 (2001-05-26) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpRestoreState(XWPProgram *somSelf,
                                            ULONG ulReserved)
{
    BOOL brc;
    // XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpRestoreState");

    brc = XWPProgram_parent_WPProgram_wpRestoreState(somSelf,
                                                     ulReserved);

    return (brc);
}

/*
 *@@ wpRestoreData:
 *      this WPObject instance method restores a chunk
 *      of binary instance data which was previously
 *      saved by WPObject::wpSaveData.
 *
 *      This may only be used while WPObject::wpRestoreState
 *      is being processed.
 *
 *      This method normally isn't designed to be overridden.
 *      However, since this gets called by WPProgram::wpRestoreState,
 *      we override this method to be able to intercept pointers
 *      to the WPProgram instance data, which we cannot access
 *      otherwise. We can store these pointers in the XWPProgram
 *      instance data then and read/write the WPProgram instance
 *      data this way.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpRestoreData(XWPProgram *somSelf,
                                           PSZ pszClass,
                                           ULONG ulKey,
                                           PBYTE pValue,
                                           PULONG pcbValue)
{
    BOOL brc,
         fMakeCopy = FALSE;
    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpRestoreData");

    // intercept pointer to internal WPProgram data
    if (!strcmp(pszClass, "WPProgramRef"))
    {
        switch (ulKey)
        {
            case 6:             // internal WPProgram key for environment
                                // string array
                // note, this comes in twice, first call has
                // pValue == NULL to query the size of the data
                if (pValue)
                    _pvpszEnvironment = pValue;
            break;

            case 7:             // internal WPProgram key for SWP
                _pSWPInitial = pValue;
            break;

            case 10:        // internal WPProgram key for array of strings
                // this is tricky, because in this case we won't have
                // a pointer to WPProgram data... seems like a temporary
                // stack pointer. Call the parent first to get the data
                // and then parse the buffer below.
                fMakeCopy = TRUE;
            break;

            case 11:        // internal WPProgram key for array of LONG values
                // apparently, this points to several LONG values then:
                // 1) executable fsh; even though this is 16-bit, it's a ULONG
                // 2) startup dir fsh; even though this is 16-bit, it's a ULONG
                // 3) next one is unknown to me
                // 4) PROGTYPE structure, which has two ULONGS
                // the following are unknown to me

                _pulExecutableHandle = (PULONG)pValue;
                _pulStartupDirHandle = _pulExecutableHandle + 1;
                // third item unknown
                _pProgType = (PPROGTYPE)(_pulStartupDirHandle + 2);
                                // two ULONGs!
            break;
       }
    }

    brc = XWPProgram_parent_WPProgram_wpRestoreData(somSelf,
                                                    pszClass,
                                                    ulKey,
                                                    pValue,
                                                    pcbValue);

    if ((brc) && (fMakeCopy) && (pcbValue) && (*pcbValue))
    {
        TRY_LOUD(excpt1)
        {
            PSZ pThis = (PSZ)pValue;
            // each string array entry has a USHORT index
            // first; if that is 0xFFFF, this was the last
            // entry
            while (TRUE)
            {
                USHORT usIndex = *(PUSHORT)pThis;
                if (usIndex == 0xFFFF)
                    break;

                // string data comes after the USHORT
                pThis += sizeof(USHORT);
                switch (usIndex)
                {
                    case 0:
                        strhStore(&_pszExecutable,
                                  pThis,
                                  NULL);
                    break;

                    case 1:
                        strhStore(&_pszParameters,
                                  pThis,
                                  NULL);
                    break;
                }

                pThis += strlen(pThis) + 1;
            }
        }
        CATCH(excpt1)
        {
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "program object %s crashed",
                   _wpQueryTitle(somSelf));
        } END_CATCH();
    }

    return (brc);
}

/*
 *@@ wpObjectReady:
 *      this WPObject notification method gets called by the
 *      WPS when object instantiation is complete, for any reason.
 *      ulCode and refObject signify why and where from the
 *      object was created.
 *      The parent method must be called first.
 *
 *      See XFldObject::wpObjectReady for remarks about using
 *      this method as a copy constructor.
 *
 *      In here, we check for whether a non-default icon
 *      was loaded for the object from the INI file. If not,
 *      we call XWPProgram::wpSetProgIcon. See remarks there
 *      for details.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope void  SOMLINK xpg_wpObjectReady(XWPProgram *somSelf,
                                          ULONG ulCode, WPObject* refObject)
{
    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpObjectReady");

    XWPProgram_parent_WPProgram_wpObjectReady(somSelf, ulCode,
                                              refObject);

    if (    (_fNeedsSetProgIcon)        // set by first wpSetProgIcon
         && (!(_wpQueryStyle(somSelf) & OBJSTYLE_NOTDEFAULTICON))
       )
    {
        _fNeedsSetProgIcon = FALSE;
        _wpSetProgIcon(somSelf, NULL);
    }
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
 *@@added V0.9.12 (2001-05-22) [umoeller]
 *@@changed V0.9.16 (2002-01-04) [umoeller]: if ext assocs are enabled, xwp now handles program open too
 */

SOM_Scope HWND  SOMLINK xpg_wpOpen(XWPProgram *somSelf, HWND hwndCnr,
                                    ULONG ulView, ULONG param)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpOpen");

#ifndef __NEVEREXTASSOCS__
    if (cmnQuerySetting(sfExtAssocs))
    {
        if (ulView == OPEN_RUNNING)
        {
            HWND hwnd = NULLHANDLE;
            APIRET arc;
            CHAR szFailing[CCHMAXPATH];

            if (arc = progOpenProgram(somSelf,
                                      NULL,      // no assoc data file
                                      ulView,
                                      &hwnd,
                                      sizeof(szFailing),
                                      szFailing))
            {
                // error opening program: ask user if he wants
                // to change the settings
                if (cmnProgramErrorMsgBox(NULLHANDLE,
                                          somSelf,
                                          szFailing,
                                          arc)
                            == MBID_YES)
                    krnPostThread1ObjectMsg(T1M_OPENOBJECTFROMPTR,
                                            (MPARAM)somSelf,
                                            (MPARAM)OPEN_SETTINGS);
            }

            return (hwnd);
        }
    } // end if (cmnQuerySetting(sfExtAssocs))
#endif

    return (XWPProgram_parent_WPProgram_wpOpen(somSelf, hwndCnr,
                                               ulView, param));
}

/*
 *@@ wpQueryIcon:
 *      this WPObject instance method returns the HPOINTER
 *      with the current icon of the object. For some WPS
 *      classes, icon loading is deferred until the first
 *      call to this method.
 *      See icons.c for an introduction.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope HPOINTER  SOMLINK xpg_wpQueryIcon(XWPProgram *somSelf)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpQueryIcon");

    return (XWPProgram_parent_WPProgram_wpQueryIcon(somSelf));

    // if (!(_wpQueryStyle(somSelf) & OBJSTYLE_NOTDEFAULTICON))

}

/*
 *@@ wpSetProgIcon:
 *      this instance method exists for both WPProgram and
 *      WPProgramFile and is supposed to set the visual icon
 *      for this executable file to the appropriate custom or
 *      default icon.
 *
 *      There are several reasons why we must replace this
 *      if icon replacements are on. Basically, icon management
 *      for abstracts apparently sucks even worse than that of
 *      data files.
 *
 *      The execution flow during object initialization is:
 *
 *      1)  WPProgram::wpRestoreState calls this even
 *          before calling the parent WPAbstract::wpRestoreState.
 *
 *      2)  Only WPAbstract::wpRestoreState looks in
 *          the INI file if the abstract has a non-default
 *          icon.
 *
 *      After long, long debugging sessions I have identified
 *      this method to be responsible for nuking my standard
 *      icons. In the worst case (which isn't all that rare),
 *      the following happens:
 *
 *      1)  WPProgram::wpRestoreState calls this method and
 *          we set a default executable icon, e.g. for a
 *          VIO window.
 *
 *      2)  WPProgram::wpRestoreState calls its parent,
 *          WPAbstract::wpRestoreState.
 *
 *      3)  WPAbstract::wpRestoreState calls WPObject::wpRestoreState
 *          and restores the object style. Even if we set
 *          OBJSTYLE_NOTDEFAULTICON a thousand times here,
 *          that style will be overwritten there.
 *
 *      4)  WPProgram::wpRestoreState then restores the icon
 *          data from OS2.INI if OBJSTYLE_NOTDEFAULTICON was
 *          set in the restored style. In the call to
 *          WPObject::wpSetIcon that follows, the WPS then
 *          nukes our default icon that was set during (1).
 *
 *      Sigh. Yes, the WPS apparently first goes thru the
 *      effort of finding an icon in the entire executable
 *      just to throw it away again right afterwards. In
 *      addition, it kills default icons.
 *
 *      Sooo... if icon replacements are enabled, what we
 *      MUST do is check for whether the object is
 *      initialized; if not, we simply return TRUE. In
 *      our XWPProgram::wpObjectReady override, we then
 *      call this method again if the OBJSTYLE_NOTDEFAULTICON
 *      flag wasn't set during wpRestoreState. This is not
 *      only _way_ faster but doesn't consume as many system
 *      resources.
 *
 *      If the object is initialized, we perform pretty much
 *      the same processing as with XWPProgramFile, i.e. we
 *      either load an icon from the executable or set a
 *      default icon. This method can be called several times
 *      (and will be called, e.g. if the user changes the
 *      executable).
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpSetProgIcon(XWPProgram *somSelf,
                                           PFEA2LIST pfeal)
{
    BOOL            brc = FALSE;
    CHAR            szExecutable[CCHMAXPATH];
    HPOINTER        hptr = NULLHANDLE;
    BOOL        fNotDefaultIcon = FALSE;
    APIRET      arc;
    BOOL        fRunReplacement;

    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpSetProgIcon");

    // turbo folders enabled?
    fRunReplacement = cmnQuerySetting(sfTurboFolders);

#ifndef __NOICONREPLACEMENTS__
    if (!fRunReplacement)
        if (cmnQuerySetting(sfIconReplacements))
            fRunReplacement = TRUE;
#endif

    if (fRunReplacement)
    {
        // here's the important check that prevents our
        // standard icon from being nuked:
        if (!_wpIsObjectInitialized(somSelf))
        {
            // set flag for wpObjectReady so it can call
            // us again after the object has been fully
            // restored
            _fNeedsSetProgIcon = TRUE;
            // AND DO NOTHING YET, we will be called again,
            // but only if wpRestoreState hasn't found
            // an icon in os2.ini
            return TRUE;
        }

        if (    (_xwpQueryExecutable(somSelf, szExecutable))
             && (_pProgType)
           )
        {
            CHAR    szFQExecutable[CCHMAXPATH];
            PSZ     pszExec = NULL;

            if (    (szExecutable[1] != ':')
                 || (!strchr(szExecutable, '\\'))
               )
            {
                // prog is not fully qualified: find it on path
                if (!doshSearchPath("PATH",
                                    szExecutable,
                                    szFQExecutable,
                                    sizeof(szFQExecutable)))
                    // alright, found it:
                    pszExec = szFQExecutable;
            }
            else
                pszExec = szExecutable;

            if (pszExec)
            {
                // first: check if an .ICO file of the same filestem
                // exists in the folder. If so, _always_ use that icon.
                PCSZ pExt;
                if (pExt = doshGetExtension(pszExec))
                {
                    CHAR szIconFile[CCHMAXPATH];
                    memcpy(szIconFile, pszExec, (pExt - pszExec));
                    strcpy(szIconFile + (pExt - pszExec), "ico");
                    // use the new icoLoadICOFile, this is way faster
                    // V0.9.16 (2001-12-08) [umoeller]
                    if (!(arc = icoLoadICOFile(szIconFile,
                                               &hptr,
                                               NULL,
                                               NULL)))
                        fNotDefaultIcon = TRUE;
                    // else: .ICO file doesn't exist, or bad format, so go on
                }

                if (!hptr)
                {
                    PEXECUTABLE pExec = NULL;

                    if (!(arc = exehOpen(pszExec, &pExec)))
                    {
                        ULONG ulProgType;
                        ULONG ulStdIcon = 0;

                        _Pmpf((__FUNCTION__ ": %s, calling _xwpQueryProgType",
                                    pszExec));

                        if (_pProgType->progc)
                            ulProgType = _pProgType->progc;
                        else
                            ulProgType = progQueryProgType(pszExec,
                                                           pExec);

                        progFindIcon(pExec,
                                     ulProgType,
                                     &hptr,
                                     NULL,
                                     &fNotDefaultIcon);

                        exehClose(&pExec);

                    } // end if (!(arc = exehOpen(pszExec, &pExec)))
                }
            }
        }

        if (!hptr)
            cmnGetStandardIcon(STDICON_PROG_UNKNOWN,
                               &hptr,
                               NULL);

        _wpSetIcon(somSelf, hptr);
        _wpModifyStyle(somSelf,
                       OBJSTYLE_NOTDEFAULTICON,
                       (fNotDefaultIcon) ? OBJSTYLE_NOTDEFAULTICON : 0);

        return (TRUE);
    }

    return (XWPProgram_parent_WPProgram_wpSetProgIcon(somSelf, pfeal));
}

/*
 *@@ wpQueryIconData:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xpg_wpQueryIconData(XWPProgram *somSelf,
                                              PICONINFO pIconInfo)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpQueryIconData");

    return (XWPProgram_parent_WPProgram_wpQueryIconData(somSelf,
                                                        pIconInfo));
}

/*
 *@@ wpSetIconData:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpSetIconData(XWPProgram *somSelf,
                                           PICONINFO pIconInfo)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpSetIconData");

    return (XWPProgram_parent_WPProgram_wpSetIconData(somSelf,
                                                      pIconInfo));
}

/*
 *@@ wpSetAssociationType:
 *      this WPProgram(File) method sets the types
 *      this object is associated with.
 *
 *      We must invalidate the caches if the WPS
 *      messes with this.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpSetAssociationType(XWPProgram *somSelf,
                                                  PSZ pszType)
{
    BOOL brc = FALSE;

    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpSetAssociationType");

    brc = XWPProgram_parent_WPProgram_wpSetAssociationType(somSelf,
                                                           pszType);

#ifndef __NEVEREXTASSOCS__
    ftypInvalidateCaches();
#endif

    return (brc);
}

/*
 *@@ wpMoveObject:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpMoveObject(XWPProgram *somSelf,
                                          WPFolder* Folder)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpMoveObject");

    return (XWPProgram_parent_WPProgram_wpMoveObject(somSelf,
                                                     Folder));
}

/*
 *@@ wpCopyObject:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope WPObject*  SOMLINK xpg_wpCopyObject(XWPProgram *somSelf,
                                               WPFolder* Folder,
                                               BOOL fLock)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpCopyObject");

    return (XWPProgram_parent_WPProgram_wpCopyObject(somSelf,
                                                     Folder,
                                                     fLock));
}

/*
 *@@ wpQueryProgDetails:
 *      this instance method exists for both WPProgram
 *      and WPProgramFile and returns information about
 *      the executable program file in the given buffer.
 *      If pProgDetails is NULL, *pulSize receives the
 *      required size of the buffer; otherwise *pulSize
 *      is expected to specify the size of the buffer
 *      on input, which better be large enough.
 *
 *      This is a complete rewrite. We no longer call
 *      the parent because it's so damn slow and messes
 *      with too many things. The shared implementation
 *      for WPProgram and WPProgramFile is in
 *      progFillProgDetails; see remarks there.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpQueryProgDetails(XWPProgram *somSelf,
                                               PPROGDETAILS pProgDetails,
                                               PULONG pulSize)
{
    CHAR        szExecutable[CCHMAXPATH];
    PSZ         pszTitle;
    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpQueryProgDetails");

    if (    (_xwpQueryExecutable(somSelf, szExecutable))
         && (_pProgType)
         && (pszTitle = _wpQueryTitle(somSelf))
         && (pulSize)
       )
    {
#ifdef _PMPRINTF_
        _Pmpf((__FUNCTION__ " for \"%s\": progc is %s",
                pszTitle,
                appDescribeAppType(_pProgType->progc)));
        _Pmpf(("   pcszExecutable: \"%s\"", szExecutable));
        _Pmpf(("   startupdir %lX", *_pulStartupDirHandle));
        _Pmpf(("   params \"%s\"", (_pszParameters) ? _pszParameters : "NULL"));

        _Pmpf(("   _pvpszEnvironment is 0x%lX",
                            _pvpszEnvironment));
        if (_pvpszEnvironment)
        {
            PSZ pszThis = _pvpszEnvironment;
            while (*pszThis != 0)
            {
                _Pmpf(("  \"%s\"", pszThis));
                pszThis += strlen(pszThis) + 1;
            }
        }
#endif

        return (progFillProgDetails(pProgDetails,     // can be NULL
                                    _pProgType->progc,
                                    _pProgType->fbVisible,
                                    _pSWPInitial,
                                    pszTitle,
                                    szExecutable,
                                    *_pulStartupDirHandle,
                                    _pszParameters,
                                    _pvpszEnvironment,
                                    pulSize));
    }

    return (XWPProgram_parent_WPProgram_wpQueryProgDetails(somSelf,
                                                           pProgDetails,
                                                           pulSize));
}

/*
 *@@ wpSetProgDetails:
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xpg_wpSetProgDetails(XWPProgram *somSelf,
                                             PPROGDETAILS pProgDetails)
{
    XWPProgramData *somThis = XWPProgramGetData(somSelf);
    XWPProgramMethodDebug("XWPProgram","xpg_wpSetProgDetails");

    return (XWPProgram_parent_WPProgram_wpSetProgDetails(somSelf,
                                                         pProgDetails));
}

/*
 *@@ wpAddProgramPage:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xpg_wpAddProgramPage(XWPProgram *somSelf,
                                               HWND hwndNotebook)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpAddProgramPage");

    return (XWPProgram_parent_WPProgram_wpAddProgramPage(somSelf,
                                                         hwndNotebook));
}

/*
 *@@ wpAddProgramSessionPage:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xpg_wpAddProgramSessionPage(XWPProgram *somSelf,
                                                      HWND hwndNotebook)
{
    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xpg_wpAddProgramSessionPage");

    return (XWPProgram_parent_WPProgram_wpAddProgramSessionPage(somSelf,
                                                                hwndNotebook));
}

/*
 *@@ wpAddProgramAssociationPage:
 *      this WPProgram method adds the "Associations" page
 *      to an executable's settings notebook.
 *
 *      If extended associations are enabled, we replace the
 *      standard "Associations" page with a new page which
 *      lists the extended file types.
 */

SOM_Scope ULONG  SOMLINK xpg_wpAddProgramAssociationPage(XWPProgram *somSelf,
                                                          HWND hwndNotebook)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    /* XWPProgramData *somThis = XWPProgramGetData(somSelf); */
    XWPProgramMethodDebug("XWPProgram","xwppgm_wpAddProgramAssociationPage");

#ifndef __NEVEREXTASSOCS__
    if (cmnQuerySetting(sfExtAssocs))
        return (_xwpAddAssociationsPage(somSelf, hwndNotebook));
    else
#endif
        return (XWPProgram_parent_WPProgram_wpAddProgramAssociationPage(somSelf,
                                                                        hwndNotebook));
}

/* ******************************************************************
 *
 *   XWPProgram class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 */

SOM_Scope void  SOMLINK xpgM_wpclsInitData(M_XWPProgram *somSelf)
{
    /* M_XWPProgramData *somThis = M_XWPProgramGetData(somSelf); */
    M_XWPProgramMethodDebug("M_XWPProgram","xwppgmM_wpclsInitData");

    M_XWPProgram_parent_M_WPProgram_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXWPProgram);
}



