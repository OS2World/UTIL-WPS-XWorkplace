
/*
 *@@sourcefile xwpsound.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPSound: a replacement for WPSound, which enhances the
 *                    notebook pages for setting the system sounds.
 *
 *      This class adds proper sound scheme support to the "Sounds"
 *      page. This is fully compatible to the existing sound schemes
 *      in Warp 4, but works on Warp 3 also (!). On Warp 3, you still
 *      won't have support for sound schemes in the Schemes palette though.
 *
 *      Also, we add a proper "Browse..." button for a standard "File
 *      Open" dialog and support drag'n'drop to the file entry field.
 *
 *      Note that when MMPM/2 is installed, WPSound is already
 *      replaced with MMSound. We completely ignore that replacement
 *      and override wpAddSettingsPages to insert _our_ page only.
 *
 *      Installation of this classes is optional. You can also edit
 *      the new XWorkplace system sounds with the default MMSound
 *      replacement class. It's just not that comfortable.
 *
 *      To learn more about how MMPM/2 stores system sound data,
 *      look at the top of helpers\syssound.c.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      the SOM interface, i.e. the methods themselves.
 *      The implementation for this class is in in config\sound.c.
 *
 *@@added V0.9.0 [umoeller]
 *@@somclass XWPSound xtrc_
 *@@somclass M_XWPSound xtrcM_
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
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xwpsound_Source
#define SOM_Module_xwpsound_Source
#endif
#define XWPSound_Class_Source
#define M_XWPSound_Class_Source

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

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIPRIMITIVES
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers

// SOM headers which don't crash with prec. header files
#include "xwpsound.ih"
// #include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\sound.h"               // XWPSound implementation

// other SOM headers

#pragma hdrstop

/* ******************************************************************
 *
 *   XWPSound instance methods
 *
 ********************************************************************/

/*
 *@@ xfAddXWPSoundPages:
 *      this actually adds the new pages into the
 *      "Sound" notebook.
 */

SOM_Scope ULONG  SOMLINK xsnd_xwpAddXWPSoundPages(XWPSound *somSelf,
                                                  HWND hwndNotebook)
{
    PCREATENOTEBOOKPAGE pcnbp;
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    /* XWPSoundData *somThis = XWPSoundGetData(somSelf); */
    XWPSoundMethodDebug("XWPSound","xsnd_xfAddXWPSoundPages");

    // add the "XWorkplace Startup" page on top
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = cmnGetString(ID_XSSI_SOUNDSPAGE);  // pszSoundsPage
    pcnbp->ulDlgID = ID_XSD_XWPSOUND;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_FILETYPES + 2;
    pcnbp->ulPageID = SP_SOUNDS;
    pcnbp->pfncbInitPage    = sndSoundsInitPage;
    pcnbp->pfncbItemChanged = sndSoundsItemChanged;
    ntbInsertPage(pcnbp);

    return 1;
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

SOM_Scope ULONG  SOMLINK xsnd_wpFilterPopupMenu(XWPSound *somSelf,
                                                ULONG ulFlags,
                                                HWND hwndCnr,
                                                BOOL fMultiSelect)
{
    /* XWPSoundData *somThis = XWPSoundGetData(somSelf); */
    XWPSoundMethodDebug("XWPSound","xsnd_wpFilterPopupMenu");

    return (XWPSound_parent_WPSound_wpFilterPopupMenu(somSelf,
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
 *      Since the MMPM/2 replacement of this class, MMSound,
 *      is not documented, we call the settings pages which
 *      are to be inserted here explicitly.
 */

SOM_Scope BOOL  SOMLINK xsnd_wpAddSettingsPages(XWPSound *somSelf,
                                                HWND hwndNotebook)
{
    // PKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    /* XWPSoundData *somThis = XWPSoundGetData(somSelf); */
    XWPSoundMethodDebug("XWPSound","xsnd_wpAddSettingsPages");

    // add the new settings page only if
    // the user wants this _and_ XSound
    // is working
    /* if (    (pKernelGlobals->ulMMPM2Working == MMSTAT_WORKING)
         && (pGlobalSettings->XSystemSounds)
       ) */
    {
        // XFolder "Internals" page bottommost
        /* if (pGlobalSettings->AddObjectPage)
            _xwpAddObjectInternalsPage(somSelf, hwndNotebook); */

        // "Symbol" page next
        _wpAddObjectGeneralPage(somSelf, hwndNotebook);
        // "Beep" page next
        _wpAddSoundWarningBeepPage(somSelf, hwndNotebook);

        // skip "sound schemes" page

        // add our own "Sounds" page
        _xwpAddXWPSoundPages(somSelf, hwndNotebook);

        return (TRUE);
    }
    /* else
        return (XWPSound_parent_WPSound_wpAddSettingsPages(somSelf,
                                                       hwndNotebook)); */

}

/* ******************************************************************
 *
 *   XWPSound class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      initialize XWPSound class data.
 */

SOM_Scope void  SOMLINK xsndM_wpclsInitData(M_XWPSound *somSelf)
{
    /* M_XWPSoundData *somThis = M_XWPSoundGetData(somSelf); */
    M_XWPSoundMethodDebug("M_XWPSound","xsndM_wpclsInitData");

    M_XWPSound_parent_M_WPSound_wpclsInitData(somSelf);

    {
        // store the class object in KERNELGLOBALS
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            pKernelGlobals->fXWPSound = TRUE;
            krnUnlockGlobals();
        }
    }
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
 *      We give the "Sound" object a new standard icon here.
 */

SOM_Scope ULONG  SOMLINK xsndM_wpclsQueryIconData(M_XWPSound *somSelf,
                                                  PICONINFO pIconInfo)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* M_XWPSoundData *somThis = M_XWPSoundGetData(somSelf); */
    M_XWPSoundMethodDebug("M_XWPSound","xsndM_wpclsQueryIconData");

    /* if (pGlobalSettings->fReplaceIcons)
    {
        if (pIconInfo)
        {
            pIconInfo->fFormat = ICON_RESOURCE;
            pIconInfo->resid   = ID_ICONXWPSOUND;
            pIconInfo->hmod    = cmnQueryMainModuleHandle();
        }

        return (sizeof(ICONINFO));
    }
    else */
        return (M_XWPSound_parent_M_WPSound_wpclsQueryIconData(somSelf,
                                                               pIconInfo));
}

