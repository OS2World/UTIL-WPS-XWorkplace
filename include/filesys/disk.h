
/*
 *@@sourcefile disk.h:
 *      header file for disk.c (XFldDisk implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #define INCL_DOSDEVIOCTL
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "filesys\disk.h"
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

#ifndef DISK_HEADER_INCLUDED
    #define DISK_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Disk implementation
     *
     ********************************************************************/

    #ifdef SOM_WPFolder_h
        WPFolder* dskCheckDriveReady(WPDisk *somSelf);
    #endif

    #ifdef INCL_DOSDEVIOCTL

        BOOL APIENTRY dskQueryInfo(PXDISKINFO paDiskInfos,
                                   ULONG ulLogicalDrive);
        typedef BOOL APIENTRY DSKQUERYINFO(PXDISKINFO paDiskInfos,
                                           ULONG ulLogicalDrive);
        typedef DSKQUERYINFO *PDSKQUERYINFO;

    #endif

    /* ******************************************************************
     *
     *   XFldDisk notebook callbacks (notebook.c)
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY dskDetailsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                         ULONG flFlags);

        MRESULT XWPENTRY dskDetailsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                      ULONG ulItemID,
                                      USHORT usNotifyCode,
                                      ULONG ulExtra);
    #endif

#endif


