
/*
 *@@sourcefile xwpkeybd.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPKeyboard (WPKeyboard replacement)
 *
 *      This class replaces the WPKeyboard class, which implements the
 *      "Keyboard" settings object, to introduce new settings pages
 *      for hotkeys and such. This is all new with V0.9.0.
 *
 *      Installation of this class is optional, but you won't
 *      be able to influence certain hook keyboard settings without it.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      the SOM interface, i.e. the methods themselves.
 *      The implementation for this class is in in filesys\hookintf.c.
 *
 *@@added V0.9.0 [umoeller]
 *
 *@@somclass XWPKeyboard xkb_
 *@@somclass M_XWPKeyboard xkbM_
 */

/*
 *      Copyright (C) 1999-2000 Ulrich M�ller.
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

#ifndef SOM_Module_xwpkeybd_Source
#define SOM_Module_xwpkeybd_Source
#endif
#define XWPKeyboard_Class_Source
#define M_XWPKeyboard_Class_Source

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
#define INCL_WINWINDOWMGR
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINSTDCNR
#include <os2.h>

// C library headers

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers

// SOM headers which don't crash with prec. header files
#include "xwpkeybd.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\hookintf.h"            // daemon/hook interface

// other SOM headers
#pragma hdrstop

/*
 *@@ xwpAddKeyboardHotkeysPage:
 *      this adds the "Hotkeys" page into the
 *      "Keyboard" notebook.
 */

SOM_Scope ULONG  SOMLINK xkb_xwpAddKeyboardHotkeysPage(XWPKeyboard *somSelf,
                                                       HWND hwndDlg)
{
    ULONG               ulrc = 0;
    PCREATENOTEBOOKPAGE pcnbp;
    HMODULE             savehmod = cmnQueryNLSModuleHandle(FALSE);
    // PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();
    PNLSSTRINGS         pNLSStrings = cmnQueryNLSStrings();

    /* XWPKeyboardData *somThis = XWPKeyboardGetData(somSelf); */
    XWPKeyboardMethodDebug("XWPKeyboard","xkb_xwpAddKeyboardHotkeysPage");

    // insert "Hotkeys" page if the hook has been enabled
    if (    (hifXWPHookReady())
         && (hifObjectHotkeysEnabled())
       )
    {
        pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
        memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
        pcnbp->somSelf = somSelf;
        pcnbp->hwndNotebook = hwndDlg;
        pcnbp->hmod = savehmod;
        pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE; // generic cnr page
        pcnbp->usPageStyleFlags = BKA_MAJOR;
        pcnbp->pszName = pNLSStrings->pszObjectHotkeysPage;
        pcnbp->ulDefaultHelpPanel  = ID_XSH_KEYB_OBJHOTKEYS;
        pcnbp->ulPageID = SP_KEYB_OBJHOTKEYS;
        pcnbp->pampControlFlags = G_pampGenericCnrPage;
        pcnbp->cControlFlags = G_cGenericCnrPage;
        pcnbp->pfncbInitPage    = hifKeybdHotkeysInitPage;
        pcnbp->pfncbItemChanged = hifKeybdHotkeysItemChanged;
        ulrc = ntbInsertPage(pcnbp);
    }

    return (ulrc);
}

/*
 *@@ xwpAddKeyboardFunctionKeysPage:
 *      this adds the "Function keys" page into the
 *      "Keyboard" notebook.
 *
 *@@added V0.9.3 (2000-04-17) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xkb_xwpAddKeyboardFunctionKeysPage(XWPKeyboard *somSelf,
                                                            HWND hwndDlg)
{
    ULONG               ulrc = 0;
    PCREATENOTEBOOKPAGE pcnbp;
    HMODULE             savehmod = cmnQueryNLSModuleHandle(FALSE);
    // PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();
    PNLSSTRINGS         pNLSStrings = cmnQueryNLSStrings();

    /* XWPKeyboardData *somThis = XWPKeyboardGetData(somSelf); */
    XWPKeyboardMethodDebug("XWPKeyboard","xkb_xwpAddKeyboardHotkeysPage");

    // insert "Hotkeys" page if the hook has been enabled
    if (    (hifXWPHookReady())
       )
    {
        pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
        memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
        pcnbp->somSelf = somSelf;
        pcnbp->hwndNotebook = hwndDlg;
        pcnbp->hmod = savehmod;
        pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE; // generic cnr page
        pcnbp->usPageStyleFlags = BKA_MAJOR;
        pcnbp->pszName = pNLSStrings->pszFunctionKeysPage;
        pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_FUNCTIONKEYS;
        pcnbp->ulPageID = SP_KEYB_FUNCTIONKEYS;
        pcnbp->pampControlFlags = G_pampGenericCnrPage;
        pcnbp->cControlFlags = G_cGenericCnrPage;
        pcnbp->pfncbInitPage    = hifKeybdFunctionKeysInitPage;
        pcnbp->pfncbItemChanged = hifKeybdFunctionKeysItemChanged;
        ulrc = ntbInsertPage(pcnbp);
    }

    return (ulrc);
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

SOM_Scope ULONG  SOMLINK xkb_wpFilterPopupMenu(XWPKeyboard *somSelf,
                                               ULONG ulFlags,
                                               HWND hwndCnr,
                                               BOOL fMultiSelect)
{
    /* XWPKeyboardData *somThis = XWPKeyboardGetData(somSelf); */
    XWPKeyboardMethodDebug("XWPKeyboard","xkb_wpFilterPopupMenu");

    return (XWPKeyboard_parent_WPKeyboard_wpFilterPopupMenu(somSelf,
                                                            ulFlags,
                                                            hwndCnr,
                                                            fMultiSelect)
            & ~CTXT_NEW
           );
}

/*
 *@@ wpAddKeyboardSpecialNeedsPage:
 *      this WPKeyboard instance method inserts the "Special
 *      Needs" page into the keyboard object's settings notebook.
 *      We override this to get an opportunity to insert our
 *      own pages behind that page by calling
 *      XWPKeyboard::xwpAddKeyboardHotkeysPage.
 */

SOM_Scope ULONG  SOMLINK xkb_wpAddKeyboardSpecialNeedsPage(XWPKeyboard *somSelf,
                                                           HWND hwndNotebook)
{
    /* XWPKeyboardData *somThis = XWPKeyboardGetData(somSelf); */
    XWPKeyboardMethodDebug("XWPKeyboard","xkb_wpAddKeyboardSpecialNeedsPage");

    _xwpAddKeyboardHotkeysPage(somSelf, hwndNotebook);

    _xwpAddKeyboardFunctionKeysPage(somSelf, hwndNotebook);

    return (XWPKeyboard_parent_WPKeyboard_wpAddKeyboardSpecialNeedsPage(somSelf,
                                                                        hwndNotebook));
}


/*
 *@@ wpclsInitData:
 *      initialize XWPKeyboard class data.
 */

SOM_Scope void  SOMLINK xkbM_wpclsInitData(M_XWPKeyboard *somSelf)
{
    /* M_XWPKeyboardData *somThis = M_XWPKeyboardGetData(somSelf); */
    M_XWPKeyboardMethodDebug("M_XWPKeyboard","xkbM_wpclsInitData");

    M_XWPKeyboard_parent_M_WPKeyboard_wpclsInitData(somSelf);

    {
        // store the class object in KERNELGLOBALS
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            pKernelGlobals->fXWPKeyboard = TRUE;
            krnUnlockGlobals();
        }
    }
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

SOM_Scope BOOL  SOMLINK xkbM_wpclsQuerySettingsPageSize(M_XWPKeyboard *somSelf,
                                                        PSIZEL pSizl)
{
    /* M_XWPKeyboardData *somThis = M_XWPKeyboardGetData(somSelf); */
    M_XWPKeyboardMethodDebug("M_XWPKeyboard","xkbM_wpclsQuerySettingsPageSize");

    return (M_XWPKeyboard_parent_M_WPKeyboard_wpclsQuerySettingsPageSize(somSelf,
                                                                         pSizl));
}

