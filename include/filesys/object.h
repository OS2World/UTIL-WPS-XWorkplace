
/*
 *@@sourcefile object.h:
 *      header file for object.c (XFldObject implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>   // or any other WPS SOM header
 *@@include #include "hook\xwphook.h"
 *@@include #include "filesys\object.h"
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

#ifndef OBJECT_HEADER_INCLUDED
    #define OBJECT_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Declarations
     *
     ********************************************************************/

    // Toolkit 4 declarations

    #ifndef OBJSTYLE_LOCKEDINPLACE
        #define OBJSTYLE_LOCKEDINPLACE  0x00020000
    #endif

    /*
     *@@ TRASHDATA:
     *      object deletion information if the object
     *      is in the trash can. This is what _pTrashData
     *      points to if != NULL.
     *
     *      I'd loved to have this declared in xfobj.idl,
     *      but that would have required everyone to include
     *      the container headers.
     *
     *@@added V0.9.20 (2002-07-25) [umoeller]
     */

    typedef struct _TRASHDATA
    {
        // CDATE and CTIME contain the date and time of deletion
        CDATE       cdateDeleted;   // data for xwpQueryDeletion
        CTIME       ctimeDeleted;   // data for xwpQueryDeletion
        WPObject    *pTrashObject;  // corresponding trash object; this is set when
                                    // the trash object is created (upon deletion
                                    // or upon first trash can populate)
    } TRASHDATA, *PTRASHDATA;

    /* ******************************************************************
     *
     *   Object internals
     *
     ********************************************************************/

    #pragma pack(1)                 // SOM packs structures, apparently

    /*
     *@@ IBMOBJECTDATA:
     *      WPObject instance data structure,
     *      as far as I have been able to
     *      decode it. See XFldObject::wpInitData
     *      where we get a pointer to this.
     *
     *      WARNING: This is the result of the
     *      testing done on eComStation, i.e. the
     *      MCP1 code level of the WPS. I have not
     *      tested whether the struct ordering is
     *      the same on all older versions of OS/2,
     *      nor can I guarantee that the ordering will
     *      stay the same in the future (even though
     *      it is unlikely that anyone at IBM is
     *      capable of changing this structure any
     *      more in the first place).
     *
     *      The size of this structure (sizeof(IBMOBJECTDATA))
     *      is 144 bytes, and _somGetInstanceSize(_WPObject)
     *      returns
     *
     *      --  144 bytes on Warp 4 FP 15
     *
     *      --  144 bytes on eComStation (MCP1)
     *
     *@@added V0.9.18 (2002-03-23) [umoeller]
     */

    typedef struct _IBMOBJECTDATA
    {
        WPObject            *pobjNext;
                                // next object in folder content chain;
                                // this is a SOM attribute, so we can safely
                                // get this using the som _get_pobjNext method
                                // (see objGetNextObjPointer)
        PMINIRECORDCORE     pRecord;
                                // pointer to the object record; size is variable
                                // depending on object data
        ULONG               ulUnknown1;
        ULONG               ulUnknown2;
        WPObject            *pFolder;
                                // object's folder
        ULONG               aulUnknown1[13];
        ULONG               cLocks;
                                // current object lock count (wpLockObject,
                                // wpUnlockObject); 0 if not locked
        ULONG               aulUnknown2[4];
        ULONG               ulDefaultView;
                                // object's default view, if explicitly set
                                // by user on the "Menu" page; if 0 (OPEN_DEFAULT),
                                // wpclsQueryDefaultView is used instead
        ULONG               ulHelpPanelID;
                                // object's help panel ID, as returned by
                                // wpQueryDefaultHelp; if 0, wpclsQueryDefaultHelp
                                // is used instead, apparently
        ULONG               ulUnknown3;
        ULONG               flStyle;
                                // object's style, as returned by wpQueryStyle,
                                // if not overridden by subclasses; see wpobject.h
                                // for valid object styles
        ULONG               ulMinWindow;
                                // minimized window behavior, as returned by
                                // wpQueryMinWindow
                                // MINWIN_DEFAULT=0, MINWIN_HIDDEN=1, MINWIN_VIEWER=2, MINWIN_DESKTOP=3
        ULONG               ulConcurrentView;
                                // concurrent views behavior, as returned by
                                // wpQueryConcurrentView; one of
                                // CCVIEW_DEFAULT=0, CCVIEW_ON=1, CCVIEW_OFF=2
        ULONG               ulButtonAppearance;
                                // button appearance, as returned by
                                // wpQueryButtonAppearance; one of
                                // HIDEBUTTON=1, MINBUTTON=2, DEFAULTBUTTON=3
        ULONG               ulMenuStyle;
                                // menu style, as returned by wpQueryMenuStyle (Warp 4 only)
        PSZ                 pszHelpLibrary;
                                // help library, as returned by wpQueryDefaultHelp
        PSZ                 pszObjectID;
                                // object ID, if any, as returned by
                                // wpQueryObjectID
        ULONG               ulUnknown4;
        ULONG               ulUnknown5;
        ULONG               ulUnknown6;
    } IBMOBJECTDATA, *PIBMOBJECTDATA;

    #pragma pack()

    #define OBJFL_WPFILESYSTEM              0x00000001
    #define OBJFL_WPFOLDER                  0x00000002
    #define OBJFL_WPABSTRACT                0x00000004  // V0.9.19 (2002-04-24) [umoeller]
    #define OBJFL_WPSHADOW                  0x00000008

    #define OBJFL_INITIALIZED               0x00001000

    #define OBJFL_GLOBALICON                0x00002000  // V0.9.20 (2002-07-25) [umoeller]
    #define OBJFL_NOLAZYICON                0x00004000  // V0.9.20 (2002-07-25) [umoeller]

    #define OBJLIST_RUNNINGSTORED           0x00010000
    #define OBJLIST_CONFIGFOLDER            0x00020000
#ifndef __NOFOLDERCONTENTS__
    #define OBJLIST_FAVORITEFOLDER          0x00040000
#endif
#ifndef __NOQUICKOPEN__
    #define OBJLIST_QUICKOPENFOLDER         0x00080000
#endif
    #define OBJLIST_HANDLESCACHE            0x00100000 // V0.9.9 (2001-04-02) [umoeller]
    #define OBJLIST_DIRTYLIST               0x00200000 // V0.9.11 (2001-04-18) [umoeller]
    #define OBJLIST_QUERYAWAKEFSOBJECT      0x00400000 // V0.9.16 (2001-10-25) [umoeller]

    WPObject* objResolveIfShadow(WPObject *somSelf);

    ULONG objQueryFlags(WPObject *somSelf);

    BOOL objIsAnAbstract(WPObject *somSelf);

    BOOL objIsAFolder(WPObject *somSelf);

    BOOL objIsObjectInitialized(WPObject *somSelf);

    /*
     *@@ xfTP_get_LastObj:
     *      prototype for WPFolder::_get_LastObj (note the
     *      extra underscore).
     *
     *      See xfTP_get_pobjNext for explanations.
     *
     *@@added V0.9.7 (2001-01-13) [umoeller]
     */

    typedef WPObject** _System xfTP_get_LastObj(WPFolder*);
    typedef xfTP_get_LastObj *xfTD_get_LastObj;

    WPObject** objGetNextObjPointer(WPObject *somSelf);

    /* ******************************************************************
     *
     *   Object setup
     *
     ********************************************************************/

    BOOL objSetup(WPObject *somSelf,
                  PSZ pszSetupString);

    BOOL objQuerySetup(WPObject *somSelf,
                        PVOID pstrSetup);

    /* ******************************************************************
     *
     *   Object scripts
     *
     ********************************************************************/

    #define SCRFL_RECURSE           0x0001

    #ifdef LINKLIST_HEADER_INCLUDED
    #ifdef SOM_WPFolder_h

        APIRET objCreateObjectScript(WPObject *pObject,
                                     PCSZ pcszRexxFile,
                                     WPFolder *pFolderForFiles,
                                     ULONG flCreate);
    #endif
    #endif

    /* ******************************************************************
     *
     *   Object details dialog
     *
     ********************************************************************/

    VOID objShowObjectDetails(HWND hwndOwner,
                              WPObject *pobj);

    /* ******************************************************************
     *
     *   Object creation/destruction
     *
     ********************************************************************/

    VOID objReady(WPObject *somSelf,
                  ULONG ulCode,
                  WPObject* refObject);

    VOID objRefreshUseItems(WPObject *somSelf,
                            PSZ pszNewTitleCopy);

    /* ******************************************************************
     *
     *   Object linked lists
     *
     ********************************************************************/

    #ifdef LINKLIST_HEADER_INCLUDED

        /*
         *@@ OBJECTLIST:
         *      encapsulation for object lists, for
         *      use with the obj* list APIs.
         *
         *      See objAddToList for details.
         *
         *@@added V0.9.9 (2001-03-27) [umoeller]
         */

        typedef struct _OBJECTLIST
        {
            LINKLIST    ll;
            BOOL        fLoaded;
        } OBJECTLIST, *POBJECTLIST;

        extern OBJECTLIST   G_llFavoriteFolders,            // xfldr.c
                            G_llQuickOpenFolders;           // xfldr.c
            // moved declarations here from folder.h
            // V0.9.16 (2001-10-19) [umoeller]

        BOOL objAddToList(WPObject *somSelf,
                          POBJECTLIST pllFolders,
                          BOOL fInsert,
                          PCSZ pcszIniKey,
                          ULONG ulListFlag);

        BOOL objIsOnList(WPObject *somSelf,
                         POBJECTLIST pllFolders);

        WPObject* objEnumList(POBJECTLIST pllFolders,
                              WPObject *pFolder,
                              PCSZ pcszIniKey,
                              ULONG ulListFlag);

    #endif

    /* ******************************************************************
     *
     *   Object handles cache
     *
     ********************************************************************/

    WPObject* objFindObjFromHandle(HOBJECT hobj);

    VOID objRemoveFromHandlesCache(WPObject *somSelf);

    /* ******************************************************************
     *
     *   Dirty objects list
     *
     ********************************************************************/

    BOOL objAddToDirtyList(WPObject *pobj);

    BOOL objRemoveFromDirtyList(WPObject *pobj);

    ULONG objQueryDirtyObjectsCount(VOID);

    typedef BOOL _Optlink FNFORALLDIRTIESCALLBACK(WPObject *pobjThis,
                                                  ULONG ulIndex,
                                                  ULONG cObjects,
                                                  PVOID pvUser);

    ULONG objForAllDirtyObjects(FNFORALLDIRTIESCALLBACK *pCallback,
                                PVOID pvUserForCallback);

    /* ******************************************************************
     *
     *   Object "Internals" page
     *
     ********************************************************************/

    // MRESULT EXPENTRY obj_fnwpSettingsObjDetails(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *
     *   Object hotkeys
     *
     ********************************************************************/

    #ifdef XWPHOOK_HEADER_INCLUDED
    PGLOBALHOTKEY objFindHotkey(PGLOBALHOTKEY pHotkeys,
                                ULONG cHotkeys,
                                HOBJECT hobj);
    #endif

    #ifdef SOM_XFldObject_h
        BOOL objQueryObjectHotkey(WPObject *somSelf,
                                  XFldObject_POBJECTHOTKEY pHotkey);

        BOOL objSetObjectHotkey(WPObject *somSelf,
                                XFldObject_POBJECTHOTKEY pHotkey);
    #endif

    BOOL objRemoveObjectHotkey(HOBJECT hobj);

    /* ******************************************************************
     *
     *   Object menus
     *
     ********************************************************************/

    VOID objModifyPopupMenu(WPObject* somSelf,
                            HWND hwndMenu);

    BOOL objCopyObjectFileName(WPObject *somSelf, HWND hwndCnr, BOOL fCopyFullPath);

#endif


