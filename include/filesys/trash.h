
/*
 *@@sourcefile trash.h:
 *      header file for trash.c (trash can).
 *
 *      This file is ALL new with V0.9.1.
 *
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>
 *@@include #include "helpers\linklist.h"
 *@@include #include "shared\notebook.h"
 *@@include #include "classes\xtrash.h"
 *@@include #include "classes\xtrashobj.h"
 *@@include #include "filesys\trash.h"
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

#ifndef TRASH_HEADER_INCLUDED
    #define TRASH_HEADER_INCLUDED

    #ifndef SOM_XWPTrashCan_h
        #error trash.h requires xtrash.h to be included.
    #endif

    #define CB_SUPPORTED_DRIVES     24  // fixed V0.9.4 (2000-06-08) [umoeller]

    #define XTRC_INVALID            0
    #define XTRC_SUPPORTED          1
    #define XTRC_UNSUPPORTED        2

    /* ******************************************************************
     *                                                                  *
     *   Trash can populating                                           *
     *                                                                  *
     ********************************************************************/

    VOID trshUpdateStatusBars(XWPTrashCan *somSelf);

    XWPTrashObject* trshCreateTrashObject(M_XWPTrashObject *somSelf,
                                          XWPTrashCan* pTrashCan,
                                          WPObject* pRelatedObject);

    BOOL trshSetupOnce(XWPTrashObject *somSelf,
                       PSZ pszSetupString);

    BOOL trshAddTrashObjectsForTrashDir(M_XWPTrashObject *pXWPTrashObjectClass,
                                        XWPTrashCan *pTrashCan,
                                        WPFolder *pTrashDir,
                                        PULONG pulObjectCount);

    VOID trshCalcTrashObjectSize(XWPTrashObject *pTrashObject,
                                 XWPTrashCan *pTrashCan);

    BOOL trshPopulateFirstTime(XWPTrashCan *somSelf,
                               ULONG ulFldrFlags);

    BOOL trshRefresh(XWPTrashCan *somSelf);

    /* ******************************************************************
     *                                                                  *
     *   Trash can / trash object operations                            *
     *                                                                  *
     ********************************************************************/

    BOOL trshDeleteIntoTrashCan(XWPTrashCan *pTrashCan,
                                WPObject *pObject);

    BOOL trshRestoreFromTrashCan(XWPTrashObject *pTrashObject,
                                 WPFolder *pTargetFolder);

    MRESULT trshDragOver(XWPTrashCan *somSelf,
                         PDRAGINFO pdrgInfo);

    MRESULT trshMoveDropped2TrashCan(XWPTrashCan *somSelf,
                                     PDRAGINFO pdrgInfo);

    BOOL trshEmptyTrashCan(XWPTrashCan *somSelf,
                           BOOL fConfirm);

    APIRET trshValidateTrashObject(XWPTrashObject *somSelf);

    // BOOL trshDestroyTrashObject(XWPTrashObject *somSelf);

    VOID trshUninitTrashObject(XWPTrashObject *somSelf);

    /* ******************************************************************
     *                                                                  *
     *   Trash can drives support                                       *
     *                                                                  *
     ********************************************************************/

    BOOL trshSetDrivesSupport(PBYTE pabSupportedDrives);

    BOOL trshQueryDrivesSupport(PBYTE pabSupportedDrives);

    VOID trshLoadDrivesSupport(M_XWPTrashCan *somSelf);

    BOOL trshIsOnSupportedDrive(WPObject *pObject);

    /* ******************************************************************
     *                                                                  *
     *   Trash can frame subclassing                                    *
     *                                                                  *
     ********************************************************************/

    /*
     *@@ SUBCLASSEDTRASHFRAME:
     *
     *@@added V0.9.1 (2000-01-31) [umoeller]
     */

    typedef struct _SUBCLASSEDTRASHFRAME
    {
        HWND            hwndFrame;
        XWPTrashCan     *somSelf;
        ULONG           ulView;
        PFNWP           pfnwpOrig;

        HWND            hwndCnr;

        WPObject        *pSourceObject;
        ULONG           ulSelection;
    } SUBCLASSEDTRASHFRAME, *PSUBCLASSEDTRASHFRAME;

    BOOL trshSubclassTrashCanFrame(HWND hwndFrame,
                                   XWPTrashCan *somSelf,
                                   ULONG ulView);

    PSUBCLASSEDTRASHFRAME trshQueryPSTF(HWND hwndFrame,
                                        PULONG pulIndex);

    VOID trshRemovePSTF(PSUBCLASSEDTRASHFRAME pstf);

    MRESULT EXPENTRY trsh_fnwpSubclassedTrashCanFrame(HWND hwndFrame,
                                                      ULONG msg,
                                                      MPARAM mp1,
                                                      MPARAM mp2);

    /* ******************************************************************
     *                                                                  *
     *   XWPTrashCan notebook callbacks (notebook.c)                    *
     *                                                                  *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID trshTrashCanSettingsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                          ULONG flFlags);

        MRESULT trshTrashCanSettingsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                                USHORT usItemID, USHORT usNotifyCode,
                                                ULONG ulExtra);

        VOID trshTrashCanDrivesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG flFlags);

        MRESULT trshTrashCanDrivesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                              USHORT usItemID, USHORT usNotifyCode,
                                              ULONG ulExtra);
    #endif

#endif


