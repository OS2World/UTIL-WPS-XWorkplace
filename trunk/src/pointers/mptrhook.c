// C Runtime
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// OS/2 Toolkit
#define INCL_ERRORS
#define INCL_PM
#define INCL_WIN
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSMISC
#include <os2.h>

// generic headers
#include "setup.h"              // code generation and debugging options

#include "pointers\mptrhook.h"
#include "pointers\title.h"
#include "pointers\debug.h"

// globale variablen
static HOOKDATA hookdataGlobal;

/*旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴커
 *� Name      : SetHookData                                                �
 *� Kommentar : initialisiert Hook-Daten                                   �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 10.10.1996                                                 �
 *� 럑derung  : 10.10.1996                                                 �
 *� aufgerufen: ###                                                        �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PHOOKDATA - Zeiger auf Hook-Daten                          �
 *� Aufgaben  : - Daten 갶ernehmen                                         �
 *� R갷kgabe  : APIRET - OS/2 Fehlercode                                   �
 *읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸
 */

APIRET _Export SetHookData
 (
     PSZ pszVersion,
     PHOOKDATA phookdata
)
{
    APIRET rc = NO_ERROR;

    do
    {
        // check parms
        if ((pszVersion == NULL) ||
            (strcmp(__VERSION__, pszVersion) != 0))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Zeiger 갶ernehmen
        memcpy(&hookdataGlobal, phookdata, sizeof(HOOKDATA));

    }
    while (FALSE);

    return rc;
}

/*旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴커
 *� Name      : InputHook                                                  �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 06.12.1995                                                 �
 *� Update    : 06.12.1995                                                 �
 *� called by : PM message queue                                           �
 *� calls     : Win*, Dos*                                                 �
 *� Input     : HAB, PQMSG, USHORT - parms of input hook                   �
 *� Tasks     : - intercepts trigger message and performs specified action �
 *�               on specified pushbutton.                                 �
 *� returns   : BOOL - remove flag                                         �
 *읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸
 */
BOOL _Export EXPENTRY InputHook
 (
     HAB hab,
     PQMSG pqmsg,
     USHORT usRemove
)

{
    BOOL fRemove = FALSE;
    USHORT x, y;
    static USHORT xold = 0, yold = 0;
    BOOL fMouseMoved = FALSE;


    if ((pqmsg->msg >= WM_BUTTONCLICKFIRST) &&
        (pqmsg->msg <= WM_BUTTONCLICKLAST))
        // send message on any click
        fMouseMoved = TRUE;
    else if (pqmsg->msg == WM_MOUSEMOVE)
    {
        // send message on move
        // check position, because for WARP 3 there is
        // a periodic mousemove message without moving
        x = SHORT1FROMMP(pqmsg->mp1);
        y = SHORT2FROMMP(pqmsg->mp1);
        if ((x != xold) || (y != yold))
            fMouseMoved = TRUE;
        xold = x;
        yold = y;
    }

// post the message
    if (fMouseMoved)
        WinPostMsg(hookdataGlobal.hwndNotify,
                   WM_TIMER,
                   (MPARAM)MOUSEMOVED_TIMER_ID,
                   MPFROMLONG(FALSE));

    return fRemove;
}
