
/*
 *@@sourcefile xwpfsys.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPFileSystem (WPFileSystem replacement)
 *
 *      This class replaces the WPFileSystem class.
 *      This is all new with V0.9.5.
 *
 *      Installation of this class is optional. This class
 *      is not very useful at this point and NOT installed
 *      with the default XWorkplace install by WarpIN.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      the SOM interface, i.e. the methods themselves.
 *      The implementation for this class is in in filesys\filesys.c.
 *
 *@@added V0.9.5 [umoeller]
 *
 *@@somclass XWPFileSystem xfs_
 *@@somclass M_XWPFileSystem xfsM_
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

#ifndef SOM_Module_xwpfsys_Source
#define SOM_Module_xwpfsys_Source
#endif
#define XWPFileSystem_Class_Source
#define M_XWPFileSystem_Class_Source

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

#include <os2.h>

// C library headers

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers

// SOM headers which don't crash with prec. header files
#include "xwpfsys.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\fhandles.h"           // replacement file-system handles management
#include "filesys\filesys.h"            // various file-system object implementation code

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *                                                                  *
 *  Global variables                                                *
 *                                                                  *
 ********************************************************************/

static BOOL    G_fReplaceHandles = FALSE;

/* ******************************************************************
 *                                                                  *
 *  XWPFileSystem instance methods                                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xwpQueryHandle:
 *
 */

SOM_Scope HOBJECT  SOMLINK xfs_xwpQueryHandle(XWPFileSystem *somSelf,
                                              BOOL fCreate)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_xwpQueryHandle");

    /* Return statement to be customized: */
    return (0);
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

    XWPFileSystem_parent_WPFileSystem_wpUnInitData(somSelf);
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

SOM_Scope void  SOMLINK xfs_wpObjectReady(XWPFileSystem *somSelf,
                                          ULONG ulCode, WPObject* refObject)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpObjectReady");

    XWPFileSystem_parent_WPFileSystem_wpObjectReady(somSelf,
                                                    ulCode, refObject);
}

/* ******************************************************************
 *                                                                  *
 *  M_XWPFileSystem class methods                                   *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *
 */

SOM_Scope void  SOMLINK xfsM_wpclsInitData(M_XWPFileSystem *somSelf)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsInitData");

#ifdef __REPLHANDLES__
    // query once at system startup whether handles
    // management has been replaced
    G_fReplaceHandles = pGlobalSettings->fReplaceHandles;
    if (G_fReplaceHandles)
        // enabled:
        // initialize handles
        fhdlLoadHandles();
#endif

    M_XWPFileSystem_parent_M_WPFileSystem_wpclsInitData(somSelf);
}


