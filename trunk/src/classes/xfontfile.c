
/*
 *@@sourcefile xfontfile.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPFontObject: a subclass of WPDataFile.
 *
 *@@added V0.9.7 (2001-01-12) [umoeller]
 *@@somclass XWPFontFile fonf_
 *@@somclass M_XWPFontFile fonfM_
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M�ller.
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

#ifndef SOM_Module_xfontfile_Source
#define SOM_Module_xfontfile_Source
#endif
#define XWPFontFile_Class_Source
#define M_XWPFontFile_Class_Source

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

#define INCL_WINSHELLDATA
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines

// SOM headers which don't crash with prec. header files
#include "xfontfile.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel

#include "config\fonts.h"               // font folder implementation

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static const char *G_pcszInstanceFilter = "*.OFM,*.FON,*.TTF,*.TTC";

/* ******************************************************************
 *
 *   XWPFontFile instance methods
 *
 ********************************************************************/

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 */

SOM_Scope void  SOMLINK fonf_wpInitData(XWPFontFile *somSelf)
{
    XWPFontFileData *somThis = XWPFontFileGetData(somSelf);
    XWPFontFileMethodDebug("XWPFontFile","fonf_wpInitData");

    XWPFontFile_parent_WPDataFile_wpInitData(somSelf);

    _fFontInstalled = FALSE;
}

/*
 *@@ wpUnInitData:
 *      this WPObject instance method is called when the object
 *      is destroyed as a SOM object, either because it's being
 *      made dormant or being deleted. All allocated resources
 *      should be freed here.
 *      The parent method must always be called last.
 */

SOM_Scope void  SOMLINK fonf_wpUnInitData(XWPFontFile *somSelf)
{
    /* XWPFontFileData *somThis = XWPFontFileGetData(somSelf); */
    XWPFontFileMethodDebug("XWPFontFile","fonf_wpUnInitData");

    XWPFontFile_parent_WPDataFile_wpUnInitData(somSelf);
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
 */

SOM_Scope void  SOMLINK fonf_wpObjectReady(XWPFontFile *somSelf,
                                           ULONG ulCode, WPObject* refObject)
{
    CHAR szFilename[CCHMAXPATH];
    XWPFontFileData *somThis = XWPFontFileGetData(somSelf);
    XWPFontFileMethodDebug("XWPFontFile","fonf_wpObjectReady");

    XWPFontFile_parent_WPDataFile_wpObjectReady(somSelf, ulCode,
                                                refObject);

    // no matter what the reason is that we've been created,
    // we check whether this font file is installed in OS2.INI...
    if (_wpQueryFilename(somSelf,
                         szFilename,
                         FALSE))        // not qualified... we need the key in "PM_Fonts"
    {
        ULONG cb = 0;
        nlsUpper(szFilename, 0);
        if (   (PrfQueryProfileSize(HINI_USER,
                                    (PSZ)PMINIAPP_FONTS, // "PM_Fonts",
                                    szFilename,
                                    &cb))
            && (cb)
           )
        {
            // yes, we exist:
            _fFontInstalled = TRUE;
        }
    }
}

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object (when "Extended help" is
 *      selected from the object's context menu). This should
 *      describe what this object can do in general.
 *      We must return TRUE to report successful completion.
 */

SOM_Scope BOOL  SOMLINK fonf_wpQueryDefaultHelp(XWPFontFile *somSelf,
                                                PULONG pHelpPanelId,
                                                PSZ HelpLibrary)
{
    /* XWPFontFileData *somThis = XWPFontFileGetData(somSelf); */
    XWPFontFileMethodDebug("XWPFontFile","fonf_wpQueryDefaultHelp");

    strcpy(HelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XSH_FONTFILE;
    return (TRUE);
}

/* ******************************************************************
 *
 *   XWPFontFile class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 */

SOM_Scope void  SOMLINK fonfM_wpclsInitData(M_XWPFontFile *somSelf)
{
    /* M_XWPFontFileData *somThis = M_XWPFontFileGetData(somSelf); */
    M_XWPFontFileMethodDebug("M_XWPFontFile","fonfM_wpclsInitData");

    M_XWPFontFile_parent_M_WPDataFile_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXWPFontFile);
}

/*
 *@@ wpclsCreateDefaultTemplates:
 *      this WPObject class method is called by the
 *      Templates folder to allow a class to
 *      create its default templates.
 *
 *      The default WPS behavior is to create new templates
 *      if the class default title is different from the
 *      existing templates.
 *
 *      Since we never want templates for font objects,
 *      we'll have to suppress this behavior.
 */

SOM_Scope BOOL  SOMLINK fonfM_wpclsCreateDefaultTemplates(M_XWPFontFile *somSelf,
                                                          WPObject* Folder)
{
    /* M_XWPFontFileData *somThis = M_XWPFontFileGetData(somSelf); */
    M_XWPFontFileMethodDebug("M_XWPFontFile","fonfM_wpclsCreateDefaultTemplates");

    return (TRUE);
    // means that the Templates folder should _not_ create templates
    // by itself; we pretend that we've done this
}

/*
 *@@ wpclsQueryTitle:
 *      this WPObject class method tells the WPS the clear
 *      name of a class, which is shown in the third column
 *      of a Details view and also used as the default title
 *      for new objects of a class.
 */

SOM_Scope PSZ  SOMLINK fonfM_wpclsQueryTitle(M_XWPFontFile *somSelf)
{
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    /* M_XWPFontFileData *somThis = M_XWPFontFileGetData(somSelf); */
    M_XWPFontFileMethodDebug("M_XWPFontFile","fonfM_wpclsQueryTitle");

    return (cmnGetString(ID_XSSI_FONTFILE)) ; // pszFontFile
}

/*
 *@@ wpclsQueryStyle:
 *
 *@@changed V0.9.16 (2001-11-25) [umoeller]: added nevertemplate
 */

SOM_Scope ULONG  SOMLINK fonfM_wpclsQueryStyle(M_XWPFontFile *somSelf)
{
    /* M_XWPFontFileData *somThis = M_XWPFontFileGetData(somSelf); */
    M_XWPFontFileMethodDebug("M_XWPFontFile","fonfM_wpclsQueryStyle");

    return (CLSSTYLE_NEVERTEMPLATE      // V0.9.16 (2001-11-25) [umoeller]
                | CLSSTYLE_NEVERCOPY
                | CLSSTYLE_NEVERDROPON
                | CLSSTYLE_NEVERPRINT);
}

/*
 *@@ wpclsQueryIconData:
 *      this WPObject class method builds the default
 *      icon for objects of a class (i.e. the icon which
 *      is shown if no instance icon is assigned). This
 *      apparently gets called from some of the other
 *      icon instance methods if no instance icon was
 *      found for an object. The exact mechanism of how
 *      this works is not documented.
 *
 *      We give this class a new standard icon here.
 *
 *@@changed V0.9.16 (2001-11-25) [umoeller]: now using separate icon for font _files_
 */

SOM_Scope ULONG  SOMLINK fonfM_wpclsQueryIconData(M_XWPFontFile *somSelf,
                                                  PICONINFO pIconInfo)
{
    /* M_XWPFontFileData *somThis = M_XWPFontFileGetData(somSelf); */
    M_XWPFontFileMethodDebug("M_XWPFontFile","fonfM_wpclsQueryIconData");

    if (pIconInfo)
    {
        pIconInfo->fFormat = ICON_RESOURCE;
        pIconInfo->resid   = ID_ICONXWPFONTFILE;
                // V0.9.16 (2001-11-25) [umoeller]
        pIconInfo->hmod    = cmnQueryMainResModuleHandle();
    }

    return (sizeof(ICONINFO));
}


/*
 *@@ wpclsQueryInstanceFilter:
 *      this WPDataFile class method determines which file-system
 *      objects will be instances of a certain class according
 *      to a file filter.
 *
 *      We return filters for what we consider font files:
 *
 *      -- *.OFM: Type 1 OS/2 descriptions. Only these can be installed.
 *
 *      -- *.TTF, *.TTC: TrueType fonts.
 *
 *      -- *.FON: bitmap font files (actually, DLLs containing font
 *         resources).
 */

SOM_Scope PSZ  SOMLINK fonfM_wpclsQueryInstanceFilter(M_XWPFontFile *somSelf)
{
    /* M_XWPFontFileData *somThis = M_XWPFontFileGetData(somSelf); */
    M_XWPFontFileMethodDebug("M_XWPFontFile","fonfM_wpclsQueryInstanceFilter");

    // return (M_XWPFontFile_parent_M_WPDataFile_wpclsQueryInstanceFilter(somSelf));

    return ((PSZ)G_pcszInstanceFilter);
}

SOM_Scope PSZ  SOMLINK fonfM_wpclsQueryInstanceType(M_XWPFontFile *somSelf)
{
    /* M_XWPFontFileData *somThis = M_XWPFontFileGetData(somSelf); */
    M_XWPFontFileMethodDebug("M_XWPFontFile","fonfM_wpclsQueryInstanceType");

    return ("Font file");
}

