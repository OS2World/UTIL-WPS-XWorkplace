
/*
 *@@sourcefile callb_open.c:
 *      SES kernel hook code.
 */

/*
 *      Copyright (C) 2000 Ulrich M�ller.
 *      Based on the MWDD32.SYS example sources,
 *      Copyright (C) 1995, 1996, 1997  Matthieu Willm (willm@ibm.net).
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\devhlp32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

extern EVENTBUF_OPEN_MAX    G_OpenBuf = {0};

/* ******************************************************************
 *
 *   Callouts
 *
 ********************************************************************/

/*
 *@@ OPEN_PRE:
 *      SES kernel hook for OPEN_PRE.
 *
 *      Since this is stored in G_SecurityHooks (sec32_callbacks.c),
 *      this gets called from the OS/2 kernel to give us a chance
 *      to authorize this event.
 */

ULONG CallType OPEN_PRE(PSZ pszPath,        // in: full path of file
                        ULONG fsOpenFlags,  // in: open flags
                        ULONG fsOpenMode,   // in: open mode
                        ULONG SFN)          // in: system file number
{
    G_OpenBuf.buf.rc = NO_ERROR;

    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        G_OpenBuf.buf.fsOpenFlags = fsOpenFlags;
        G_OpenBuf.buf.fsOpenMode = fsOpenMode;
        G_OpenBuf.buf.SFN = SFN;

        G_OpenBuf.buf.ulPathLen = strlen(pszPath);

        memcpy(G_OpenBuf.buf.szPath,
               pszPath,
               G_OpenBuf.buf.ulPathLen + 1);

        G_OpenBuf.buf.cbStruct =   sizeof(EVENTBUF_OPEN)
                                 + G_OpenBuf.buf.ulPathLen;

        // authorize event if it is not from XWPShell
        if (G_pidShell != G_pLDT->LIS_CurProcID)
        {
        }

        if (G_bLog == LOG_ACTIVE)
            ctxtLogEvent(EVENT_OPENPRE,
                         &G_OpenBuf,
                         G_OpenBuf.buf.cbStruct);
    }

    return G_OpenBuf.buf.rc;
}

/*
 *@@ OPEN_POST:
 *      security callback for OPEN_POST.
 *
 *      Since this is stored in G_SecurityHooks (sec32_callbacks.c),
 *      this gets called from the OS/2 kernel to give us notification
 *      about this event.
 */

ULONG CallType OPEN_POST(PSZ pszPath,
                         ULONG fsOpenFlags,
                         ULONG fsOpenMode,
                         ULONG SFN,
                         ULONG Action,
                         ULONG RC)
{
    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        G_OpenBuf.buf.fsOpenFlags = fsOpenFlags;
        G_OpenBuf.buf.fsOpenMode = fsOpenMode;
        G_OpenBuf.buf.SFN = SFN;

        G_OpenBuf.buf.rc = RC;
        G_OpenBuf.buf.Action = Action;

        G_OpenBuf.buf.ulPathLen = strlen(pszPath);

        memcpy(G_OpenBuf.buf.szPath,
               pszPath,
               G_OpenBuf.buf.ulPathLen + 1);

        G_OpenBuf.buf.cbStruct =   sizeof(EVENTBUF_OPEN)
                                 + G_OpenBuf.buf.ulPathLen;

        // authorize event if it is not from XWPShell
        if (G_pidShell != G_pLDT->LIS_CurProcID)
        {
        }

        if (G_bLog == LOG_ACTIVE)
            ctxtLogEvent(EVENT_OPENPOST,
                         &G_OpenBuf,
                         G_OpenBuf.buf.cbStruct);
    }

    return NO_ERROR;
}


