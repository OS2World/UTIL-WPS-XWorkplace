
/*
 *@@sourcefile w_pulse.c:
 *      XCenter "Pulse" widget implementation.
 *      This is built into the XCenter and not in
 *      a plugin DLL.
 *
 *      Function prefix for this file:
 *      --  Bwgt*
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-11-27) [umoeller]
 *@@header "shared\center.h"
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
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINTIMER

#define INCL_GPICONTROL
#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIREGIONS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files

// XWorkplace implementation headers
#include "shared\common.h"              // the majestic XWorkplace include file

#include "shared\center.h"              // public XCenter interfaces

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

/*
 *@@ PULSESETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of WIDGETPRIVATE.
 *
 *      Putting these settings into a separate structure
 *      is no requirement, but comes in handy if you
 *      want to use the same setup string routines on
 *      both the open widget window and a settings dialog.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

typedef struct _PULSESETUP
{
    LONG            lcolBackground,
                    lcolGraph,
                    lcolGraphIntr,
                    lcolText;

    PSZ             pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!

    LONG            cx;
            // current width; we're sizeable, and we wanna
            // store this
} PULSESETUP, *PPULSESETUP;

/*
 *@@ WIDGETPRIVATE:
 *      more window data for the "pulse" widget.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpPulseWidget and stored in XCENTERWIDGET.pUser.
 */

typedef struct _WIDGETPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    PULSESETUP      Setup;
            // widget settings that correspond to a setup string

    PXBITMAP        pBitmap;        // bitmap for pulse graph; this contains only
                                    // the "client" (without the 3D frame)

    BOOL            fUpdateGraph;
                                    // if TRUE, PwgtPaint recreates the entire
                                    // bitmap

    THREADINFO      tiCollect;      // collect thread

    HEV             hevExit;        // event semaphore posted on destroy to tell
                                    // collect thread to exit
    HEV             hevExitComplete; // posted by collect thread when it's done

    HMTX            hmtxData;       // mutex for protecting the data

    // All the following data must be protected with the mutex.
    // Rules about the data: main thread allocates it according
    // to the window size, collect thread moves data around,
    // but never reallocates.

    PDOSHPERFSYS    pPerfData;      // performance data (doshPerf* calls)

    ULONG           cLoads;
    PLONG           palLoads;       // ptr to an array of LONGs containing previous
                                    // CPU loads
    PLONG           palIntrs;       // ptr to an array of LONGs containing previous
                                    // CPU interrupt loads
                                    // added V0.9.9 (2001-03-14) [umoeller]

    APIRET          arc;            // if != NO_ERROR, an error occured, and
                                    // the error code is displayed instead.

    BOOL            fCrashed;       // set to TRUE if the pulse crashed somewhere.
                                    // This will disable display then to avoid
                                    // crashing again on each timer tick.

} WIDGETPRIVATE, *PWIDGETPRIVATE;

/* ******************************************************************
 *
 *   Widget setup management
 *
 ********************************************************************/

/*
 *      This section contains shared code to manage the
 *      widget's settings. This can translate a widget
 *      setup string into the fields of a binary setup
 *      structure and vice versa. This code is used by
 *      both an open widget window and a settings dialog.
 */

/*
 *@@ PwgtClearSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID PwgtClearSetup(PPULSESETUP pSetup)
{
    if (pSetup)
    {
        if (pSetup->pszFont)
        {
            free(pSetup->pszFont);
            pSetup->pszFont = NULL;
        }
    }
}

/*
 *@@ PwgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtScanSetup(const char *pcszSetupString,
                   PPULSESETUP pSetup)
{
    PSZ p;

    // width
    p = ctrScanSetupString(pcszSetupString,
                           "WIDTH");
    if (p)
    {
        pSetup->cx = atoi(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->cx = 200;

    // background color
    p = ctrScanSetupString(pcszSetupString,
                           "BGNDCOL");
    if (p)
    {
        pSetup->lcolBackground = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolBackground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);

    // graph color:
    p = ctrScanSetupString(pcszSetupString,
                           "GRPHCOL");
    if (p)
    {
        pSetup->lcolGraph = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolGraph = RGBCOL_DARKCYAN; // RGBCOL_BLUE;

    // graph color: (interrupt load)
    p = ctrScanSetupString(pcszSetupString,
                           "GRPHINTRCOL");
    if (p)
    {
        pSetup->lcolGraphIntr = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolGraphIntr = RGBCOL_DARKBLUE;


    // text color:
    p = ctrScanSetupString(pcszSetupString,
                           "TEXTCOL");
    if (p)
    {
        pSetup->lcolText = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolText = WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWSTATICTEXT, 0);

    // font:
    // we set the font presparam, which automatically
    // affects the cached presentation spaces
    p = ctrScanSetupString(pcszSetupString,
                           "FONT");
    if (p)
    {
        pSetup->pszFont = strdup(p);
        ctrFreeSetupValue(p);
    }
    // else: leave this field null
}

/*
 *@@ PwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

VOID PwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   PPULSESETUP pSetup)
{
    CHAR    szTemp[100];
    PSZ     psz = 0;
    xstrInit(pstrSetup, 100);

    sprintf(szTemp, "WIDTH=%d;",
            pSetup->cx);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "BGNDCOL=%06lX;",
            pSetup->lcolBackground);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "GRPHCOL=%06lX;",
            pSetup->lcolGraph);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "TEXTCOL=%06lX;",
            pSetup->lcolText);
    xstrcat(pstrSetup, szTemp, 0);

    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s;",
                pSetup->pszFont);
        xstrcat(pstrSetup, szTemp, 0);
    }
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

// None currently.

/* ******************************************************************
 *
 *   Callbacks stored in XCENTERWIDGET
 *
 ********************************************************************/

/*
 *@@ PwgtSetupStringChanged:
 *      this gets called from ctrSetSetupString if
 *      the setup string for a widget has changed.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGET so that the XCenter knows that
 *      we can do this.
 */

VOID EXPENTRY PwgtSetupStringChanged(PXCENTERWIDGET pWidget,
                                     const char *pcszNewSetupString)
{
    PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        // reinitialize the setup data
        PwgtClearSetup(&pPrivate->Setup);
        PwgtScanSetup(pcszNewSetupString,
                      &pPrivate->Setup);
    }
}

// VOID EXPENTRY PwgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData)

/* ******************************************************************
 *
 *   Thread synchronization
 *
 ********************************************************************/

/*
 *@@ LockData:
 *      locks the private data. Note that we only
 *      wait half a second for the mutex to be available
 *      and return FALSE otherwise.
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

BOOL LockData(PWIDGETPRIVATE pPrivate)
{
    return (!DosRequestMutexSem(pPrivate->hmtxData, 500));
}

/*
 *@@ UnlockData:
 *      unlocks the private data.
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

VOID UnlockData(PWIDGETPRIVATE pPrivate)
{
    DosReleaseMutexSem(pPrivate->hmtxData);
}

/* ******************************************************************
 *
 *   "Collect" thread
 *
 ********************************************************************/

/*
 *@@ fntCollect:
 *      extra thread to collect the performance data from
 *      the OS/2 kernel.
 *
 *      This thread does NOT have a PM message queue. It
 *      is started from PwgtCreate and will simply collect
 *      the performance data every second.
 *
 *      Before 0.9.12, we collected the performance data
 *      on the widget thread (i.e. the XCenter thread).
 *      This worked OK as long as no application hogged
 *      the input queue... in that situation, no performance
 *      data was collected while the SIQ was locked which
 *      lead to a highly incorrect display for any busy
 *      PM application.
 *
 *      So what we do now is collect the data on this new
 *      thread and only do the display on the PM thread.
 *      The display is still not updated while the SIQ
 *      is busy, but after the SIQ becomes unlocked, it
 *      is then updated with all the data that was collected
 *      here in the meantime.
 *
 *      The optimal solution would be to move all drawing
 *      to this second thread... maybe at a later time.
 *
 *      The thread data is a pointer to WIDGETPRIVATE.
 *
 *      Note: This thread does not allocate the data fields
 *      WIDGETPRIVATE. It only updates the fields, while
 *      the allocations (and the resizes) are still done
 *      on the main thread.
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

VOID _Optlink fntCollect(PTHREADINFO ptiMyself)
{
    PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)ptiMyself->ulData;
    if (pPrivate)
    {
        BOOL    fLocked = FALSE;

        // give this thread a higher-than-regular priority;
        // this way, the "loads" array is always up-to-date,
        // while the display may be delayed if the system
        // is really busy

        DosSetPriority(PRTYS_THREAD,
                       PRTYC_TIMECRITICAL,  // 3, second highest class
                       0,                   // delta, let's not overdo it
                       0);                  // current thread

        TRY_LOUD(excpt1)
        {
            // open the performance counters
            if (!(pPrivate->arc = doshPerfOpen(&pPrivate->pPerfData)))
            {
                // alright:
                // start collecting

                while (TRUE)
                {
                    // LOCK the data while we're moving around here
                    if (fLocked = LockData(pPrivate))
                    {
                        // has main thread allocated data already?
                        if (pPrivate->palLoads || pPrivate->palIntrs)
                        {
                            // get new loads from OS/2 kernel
                            if (!(pPrivate->arc = doshPerfGet(pPrivate->pPerfData)))
                            {
                                // in the array of loads, move each entry one to the front;
                                // drop the oldest entry
                                if (pPrivate->palLoads)
                                {
                                    memcpy(&pPrivate->palLoads[0],
                                           &pPrivate->palLoads[1],
                                           sizeof(LONG) * (pPrivate->cLoads - 1));

                                    // and update the last entry with the current value
                                    pPrivate->palLoads[pPrivate->cLoads - 1]
                                        = pPrivate->pPerfData->palLoads[0];
                                }

                                // same thing for interrupt loads
                                if (pPrivate->palIntrs)
                                {
                                    memcpy(&pPrivate->palIntrs[0],
                                           &pPrivate->palIntrs[1],
                                           sizeof(LONG) * (pPrivate->cLoads - 1));
                                    pPrivate->palIntrs[pPrivate->cLoads - 1]
                                        = pPrivate->pPerfData->palIntrs[0];
                                }
                            }
                        }

                        UnlockData(pPrivate);
                        fLocked = FALSE;
                    } // if (fLocked)

                    // have main thread update the display
                    // (if SIQ is hogged, several WM_TIMERs
                    // will pile up)
                    if (!ptiMyself->fExit)
                    {
                        pPrivate->fUpdateGraph = TRUE;
                        WinPostMsg(pPrivate->pWidget->hwndWidget,
                                   WM_TIMER,
                                   (MPARAM)1,
                                   0);
                    }

                    // some security checks: do not sleep
                    // if we have problems
                    if (    (!pPrivate->arc)
                         && (!ptiMyself->fExit)         // or should exit
                         && (!pPrivate->fCrashed)
                       )
                    {
                        // OK, we need to sleep for a second before
                        // we get the next chunk of performance
                        // data. However, we should not use DosSleep
                        // because we MUST exit quickly when the
                        // widget gets destroyed. The trick is to
                        // wait on an event semaphore: this gets
                        // posted by by PwgtDestroy(), but we specify
                        // a timeout of 1 second, so we get the
                        // effect of DosSleep as well.
                        if (DosWaitEventSem(pPrivate->hevExit,
                                            1000)       // timeout == 1 second
                                != ERROR_TIMEOUT)
                            // whoa, semaphore was posted:
                            // GET OUTTA HERE
                            break;
                        // else timeout: continue, we then slept for a second
                    }
                    else
                        // stop right here
                        break;

                } // end while (TRUE);
            } // if (!(pPrivate->arc = doshPerfOpen(&pPrivate->pPerfData)))
        }
        CATCH(excpt1)
        {
            pPrivate->fCrashed = TRUE;
        }
        END_CATCH();

        if (fLocked)
            UnlockData(pPrivate);

        // PwgtDestroy is blocking on hevExitComplete,
        // so tell main thread we're done
        DosPostEventSem(pPrivate->hevExitComplete);
    }
}

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *@@ PwgtCreate:
 *      implementation for WM_CREATE.
 *
 *@@changed V0.9.12 (2001-05-20) [umoeller]: now using second thread
 */

MRESULT PwgtCreate(HWND hwnd, MPARAM mp1)
{
    MRESULT mrc = 0;        // continue window creation

    PSZ     p = NULL;
    APIRET  arc = NO_ERROR;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)mp1;
    PWIDGETPRIVATE pPrivate = malloc(sizeof(WIDGETPRIVATE));
    memset(pPrivate, 0, sizeof(WIDGETPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    PwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    winhSetWindowFont(hwnd,
                      (pPrivate->Setup.pszFont)
                       ? pPrivate->Setup.pszFont
                       // default font: use the same as in the rest of XWorkplace:
                       : cmnQueryDefaultFont());

    // enable context menu help
    pWidget->pcszHelpLibrary = cmnQueryHelpLibrary();
    pWidget->ulHelpPanelID = ID_XSH_WIDGET_PULSE_MAIN;

    // create mutex for data protection
    DosCreateMutexSem(NULL,
                      &pPrivate->hmtxData,
                      0,
                      FALSE);           // no request

    // create event sems for collect thread
    DosCreateEventSem(NULL,
                      &pPrivate->hevExit,
                      0,
                      0);               // not posted
    DosCreateEventSem(NULL,
                      &pPrivate->hevExitComplete,
                      0,
                      0);               // not posted

    // start the collect thread V0.9.12 (2001-05-20) [umoeller]
    thrCreate(&pPrivate->tiCollect,
              fntCollect,
              NULL,
              "PulseWidgetCollectThread",
              THRF_WAIT,
              (ULONG)pPrivate);        // thread data

    pPrivate->fUpdateGraph = TRUE;

    return (mrc);
}

/*
 *@@ PwgtControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

BOOL PwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            USHORT  usID = SHORT1FROMMP(mp1),
                    usNotifyCode = SHORT2FROMMP(mp1);

            if (usID == ID_XCENTER_CLIENT)
            {
                switch (usNotifyCode)
                {
                    /*
                     * XN_QUERYSIZE:
                     *      XCenter wants to know our size.
                     */

                    case XN_QUERYSIZE:
                    {
                        PSIZEL pszl = (PSIZEL)mp2;
                        pszl->cx = pPrivate->Setup.cx;
                        pszl->cy = 10;

                        brc = TRUE;
                    break; }
                }
            }
        } // end if (pPrivate)
    } // end if (pWidget)

    return (brc);
}

/*
 *@@ PwgtUpdateGraph:
 *      updates the graph bitmap. This does not paint
 *      on the screen yet.
 *
 *      Preconditions:
 *
 *      --  pPrivate->pBitmap must be ready.
 *
 *      --  Caller must hold the data mutex.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtUpdateGraph(HWND hwnd,
                     PWIDGETPRIVATE pPrivate)
{
    PXCENTERWIDGET pWidget = pPrivate->pWidget;

    RECTL   rclBmp;
    POINTL  ptl;

    // size for bitmap: same as widget, except
    // for the border
    WinQueryWindowRect(hwnd, &rclBmp);
    rclBmp.xRight -= 2;
    rclBmp.yTop -= 2;

    if (!pPrivate->pBitmap)
        pPrivate->pBitmap = gpihCreateXBitmap(pWidget->habWidget,
                                              rclBmp.xRight,
                                              rclBmp.yTop);
    if (pPrivate->pBitmap)
    {
        HPS hpsMem = pPrivate->pBitmap->hpsMem;

        // fill the bitmap rectangle
        GpiSetColor(hpsMem,
                    pPrivate->Setup.lcolBackground);
        gpihBox(hpsMem,
                DRO_FILL,
                &rclBmp);

        // go thru all values in the "Loads" LONG array
        for (ptl.x = 0;
             ((ptl.x < pPrivate->cLoads) && (ptl.x < rclBmp.xRight));
             ptl.x++)
        {
            ptl.y = 0;

            // interrupt load on bottom
            if (pPrivate->palIntrs)
            {
                GpiSetColor(hpsMem,
                            pPrivate->Setup.lcolGraphIntr);
                // go thru all values in the "Interrupt Loads" LONG array
                // Note: number of "loads" entries and "intrs" entries is the same
                GpiMove(hpsMem, &ptl);
                ptl.y += rclBmp.yTop * pPrivate->palIntrs[ptl.x] / 1000;
                GpiLine(hpsMem, &ptl);
            }

            // scan the CPU loads
            if (pPrivate->palLoads)
            {
                GpiSetColor(hpsMem,
                            pPrivate->Setup.lcolGraph);
                GpiMove(hpsMem, &ptl);
                ptl.y += rclBmp.yTop * pPrivate->palLoads[ptl.x] / 1000;
                GpiLine(hpsMem, &ptl);
            }
        } // end if (fLocked)
    }

    pPrivate->fUpdateGraph = FALSE;
}

/*
 * PwgtPaint2:
 *      this does the actual painting of the frame (if
 *      fDrawFrame==TRUE) and the pulse bitmap.
 *
 *      Gets called by PwgtPaint.
 *
 *      The specified HPS is switched to RGB mode before
 *      painting.
 *
 *      If DosPerfSysCall succeeds, this diplays the pulse.
 *      Otherwise an error msg is displayed.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added mutex
 */

VOID PwgtPaint2(HWND hwnd,
                HPS hps,
                PWIDGETPRIVATE pPrivate,
                BOOL fDrawFrame)     // in: if TRUE, everything is painted
{
    BOOL fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        PXCENTERWIDGET pWidget = pPrivate->pWidget;
        RECTL       rclWin;
        ULONG       ulBorder = 1;
        CHAR        szPaint[100] = "";
        ULONG       ulPaintLen = 0;

        WinQueryWindowRect(hwnd,
                           &rclWin);        // exclusive

        gpihSwitchToRGB(hps);

        rclWin.xRight--;
        rclWin.yTop--;
            // rclWin is now inclusive

        if (fDrawFrame)
        {
            LONG lDark, lLight;

            if (pPrivate->pWidget->pGlobals->flDisplayStyle & XCS_SUNKBORDERS)
            {
                lDark = pWidget->pGlobals->lcol3DDark;
                lLight = pWidget->pGlobals->lcol3DLight;
            }
            else
            {
                lDark =
                lLight = pPrivate->Setup.lcolBackground;
            }

            gpihDraw3DFrame(hps,
                            &rclWin,        // inclusive
                            ulBorder,
                            lDark,
                            lLight);
        }

        if (pPrivate->arc == NO_ERROR)
        {
            // performance counters are working:
            PCOUNTRYSETTINGS pCountrySettings = (PCOUNTRYSETTINGS)pWidget->pGlobals->pCountrySettings;
            POINTL      ptlBmpDest;
            LONG        lLoad1000 = 0;

            // lock out the collect thread, we're reading the loads array
            if (fLocked = LockData(pPrivate))
            {
                if (pPrivate->fUpdateGraph)
                    // graph bitmap needs to be updated:
                    PwgtUpdateGraph(hwnd, pPrivate);

                // in the string, display the total load
                // (busy plus interrupt) V0.9.9 (2001-03-14) [umoeller]
                if (pPrivate->palLoads)
                    lLoad1000 = pPrivate->pPerfData->palLoads[0];
                if (pPrivate->palIntrs)
                    lLoad1000 += pPrivate->pPerfData->palIntrs[0];

                // everything below is safe, so unlock
                UnlockData(pPrivate);
                fLocked = FALSE;

                ptlBmpDest.x = rclWin.xLeft + ulBorder;
                ptlBmpDest.y = rclWin.yBottom + ulBorder;
                // now paint graph from bitmap
                WinDrawBitmap(hps,
                              pPrivate->pBitmap->hbm,
                              NULL,     // entire bitmap
                              &ptlBmpDest,
                              0, 0,
                              DBM_NORMAL);

                sprintf(szPaint, "%lu%c%lu%c",
                        lLoad1000 / 10,
                        pCountrySettings->cDecimal,
                        lLoad1000 % 10,
                        '%');
            }
        }
        else
        {
            // performance counters are not working:
            // display error message
            rclWin.xLeft++;     // was made inclusive above
            rclWin.yBottom++;
            WinFillRect(hps, &rclWin, pPrivate->Setup.lcolBackground);
            sprintf(szPaint, "E %lu", pPrivate->arc);
        }

        ulPaintLen = strlen(szPaint);
        if (ulPaintLen)
            WinDrawText(hps,
                        ulPaintLen,
                        szPaint,
                        &rclWin,
                        0,      // background, ignored anyway
                        pPrivate->Setup.lcolText,
                        DT_CENTER | DT_VCENTER);
    }
    CATCH(excpt1)
    {
        pPrivate->fCrashed = TRUE;
    } END_CATCH();

    if (fLocked)
        UnlockData(pPrivate);
}

/*
 *@@ PwgtPaint:
 *      implementation for WM_PAINT.
 */

VOID PwgtPaint(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
        PwgtPaint2(hwnd,
                   hps,
                   pPrivate,
                   TRUE);        // draw frame
        WinEndPaint(hps);
    } // end if (pWidget && pPrivate)
}

/*
 *@@ PwgtGetNewLoad:
 *      updates the CPU loads array, updates the graph bitmap
 *      and invalidates the window.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added mutex
 */

VOID PwgtGetNewLoad(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        BOOL fLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            if (    (!pPrivate->fCrashed)
                 && (pPrivate->arc == NO_ERROR)
               )
            {
                RECTL rclClient;
                WinQueryWindowRect(hwnd, &rclClient);

                // lock out the collect thread
                if (fLocked = LockData(pPrivate))
                {
                    if (rclClient.xRight > 3)
                    {
                        HPS hps;
                        ULONG ulGraphCX = rclClient.xRight - 2;    // minus border
                        if (pPrivate->palLoads == NULL)
                        {
                            // create array of loads
                            pPrivate->cLoads = ulGraphCX;
                            pPrivate->palLoads = (PLONG)malloc(sizeof(LONG) * pPrivate->cLoads);
                            memset(pPrivate->palLoads, 0, sizeof(LONG) * pPrivate->cLoads);
                        }

                        if (pPrivate->palIntrs == NULL)
                        {
                            // create array of interrupt loads
                            pPrivate->cLoads = ulGraphCX;
                            pPrivate->palIntrs = (PLONG)malloc(sizeof(LONG) * pPrivate->cLoads);
                            memset(pPrivate->palIntrs, 0, sizeof(LONG) * pPrivate->cLoads);
                        }

                        UnlockData(pPrivate);
                        fLocked = FALSE;

                        hps = WinGetPS(hwnd);
                        PwgtPaint2(hwnd,
                                   hps,
                                   pPrivate,
                                   FALSE);        // no draw frame
                        WinReleasePS(hps);

                    } // end if (rclClient.xRight)
                }
            }
        }
        CATCH(excpt1)
        {
            pPrivate->fCrashed = TRUE;
        } END_CATCH();

        if (fLocked)
            UnlockData(pPrivate);

    } // end if (pWidget && pPrivate);
}

/*
 *@@ PwgtWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        BOOL fLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            PSWP pswpNew = (PSWP)mp1,
                 pswpOld = pswpNew + 1;
            if (pswpNew->fl & SWP_SIZE)
            {
                // window was resized:

                // destroy the bitmap because we need a new one
                if (pPrivate->pBitmap)
                    gpihDestroyXBitmap(&pPrivate->pBitmap);

                if (pswpNew->cx != pswpOld->cx)
                {
                    // width changed:
                    XSTRING strSetup;

                    // update load arrays

                    // lock out the collect thread
                    if (fLocked = LockData(pPrivate))
                    {
                        if (pPrivate->palLoads)
                        {
                            // we also need a new array of past loads
                            // since the array is cx items wide...
                            // so reallocate the array, but keep past
                            // values
                            ULONG ulNewClientCX = pswpNew->cx - 2;
                            PLONG palNewLoads = (PLONG)malloc(sizeof(LONG) * ulNewClientCX);

                            if (ulNewClientCX > pPrivate->cLoads)
                            {
                                // window has become wider:
                                // fill the front with zeroes
                                memset(palNewLoads,
                                       0,
                                       (ulNewClientCX - pPrivate->cLoads) * sizeof(LONG));
                                // and copy old values after that
                                memcpy(&palNewLoads[(ulNewClientCX - pPrivate->cLoads)],
                                       pPrivate->palLoads,
                                       pPrivate->cLoads * sizeof(LONG));
                            }
                            else
                            {
                                // window has become smaller:
                                // e.g. ulnewClientCX = 100
                                //      pPrivate->cLoads = 200
                                // drop the first items
                                ULONG ul = 0;
                                memcpy(palNewLoads,
                                       &pPrivate->palLoads[pPrivate->cLoads - ulNewClientCX],
                                       ulNewClientCX * sizeof(LONG));
                            }

                            free(pPrivate->palLoads);
                            pPrivate->palLoads = palNewLoads;

                            // do the same for the interrupt load
                            if (pPrivate->palIntrs)
                            {
                                PLONG palNewIntrs = (PLONG)malloc(sizeof(LONG) * ulNewClientCX);

                                if (ulNewClientCX > pPrivate->cLoads)
                                {
                                    // window has become wider:
                                    // fill the front with zeroes
                                    memset(palNewIntrs,
                                           0,
                                           (ulNewClientCX - pPrivate->cLoads) * sizeof(LONG));
                                    // and copy old values after that
                                    memcpy(&palNewIntrs[(ulNewClientCX - pPrivate->cLoads)],
                                           pPrivate->palIntrs,
                                           pPrivate->cLoads * sizeof(LONG));
                                }
                                else
                                {
                                    // window has become smaller:
                                    // e.g. ulnewClientCX = 100
                                    //      pPrivate->cLoads = 200
                                    // drop the first items
                                    ULONG ul = 0;
                                    memcpy(palNewIntrs,
                                           &pPrivate->palIntrs[pPrivate->cLoads - ulNewClientCX],
                                           ulNewClientCX * sizeof(LONG));
                                }

                                free(pPrivate->palIntrs);
                                pPrivate->palIntrs = palNewIntrs;
                            }

                            pPrivate->cLoads = ulNewClientCX;

                        } // end if (pPrivate->palLoads)

                        UnlockData(pPrivate);
                        fLocked = FALSE;
                    } // end if fLocked


                    pPrivate->Setup.cx = pswpNew->cx;
                    PwgtSaveSetup(&strSetup,
                                  &pPrivate->Setup);
                    if (strSetup.ulLength)
                        WinSendMsg(pWidget->pGlobals->hwndClient,
                                   XCM_SAVESETUP,
                                   (MPARAM)hwnd,
                                   (MPARAM)strSetup.psz);
                    xstrClear(&strSetup);
                } // end if (pswpNew->cx != pswpOld->cx)

                // force recreation of bitmap
                pPrivate->fUpdateGraph = TRUE;
                WinInvalidateRect(hwnd, NULL, FALSE);
            } // end if (pswpNew->fl & SWP_SIZE)
        }
        CATCH(excpt1)
        {
            pPrivate->fCrashed = TRUE;
        } END_CATCH();

        if (fLocked)
            UnlockData(pPrivate);
    } // end if (pWidget && pPrivate)
}

/*
 *@@ PwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 */

VOID PwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        BOOL fInvalidate = TRUE;
        switch (ulAttrChanged)
        {
            case 0:     // layout palette thing dropped
            case PP_BACKGROUNDCOLOR:
            case PP_FOREGROUNDCOLOR:
                pPrivate->Setup.lcolBackground
                    = winhQueryPresColor(hwnd,
                                         PP_BACKGROUNDCOLOR,
                                         FALSE,
                                         SYSCLR_DIALOGBACKGROUND);
                pPrivate->Setup.lcolGraph
                    = winhQueryPresColor(hwnd,
                                         PP_FOREGROUNDCOLOR,
                                         FALSE,
                                         -1);
                if (pPrivate->Setup.lcolGraph == -1)
                    pPrivate->Setup.lcolGraph = RGBCOL_DARKCYAN;
            break;

            case PP_FONTNAMESIZE:
            {
                PSZ pszFont = 0;
                if (pPrivate->Setup.pszFont)
                {
                    free(pPrivate->Setup.pszFont);
                    pPrivate->Setup.pszFont = NULL;
                }

                pszFont = winhQueryWindowFont(hwnd);
                if (pszFont)
                {
                    // we must use local malloc() for the font
                    pPrivate->Setup.pszFont = strdup(pszFont);
                    winhFree(pszFont);
                }
            break; }

            default:
                fInvalidate = FALSE;
        }

        if (fInvalidate)
        {
            XSTRING strSetup;
            // force recreation of bitmap
            pPrivate->fUpdateGraph = TRUE;
            WinInvalidateRect(hwnd, NULL, FALSE);

            PwgtSaveSetup(&strSetup,
                          &pPrivate->Setup);
            if (strSetup.ulLength)
                WinSendMsg(pWidget->pGlobals->hwndClient,
                           XCM_SAVESETUP,
                           (MPARAM)hwnd,
                           (MPARAM)strSetup.psz);
            xstrClear(&strSetup);
        }
    } // end if (pWidget && pPrivate)
}

/*
 *@@ PwgtDestroy:
 *      implementation for WM_DESTROY.
 *
 *@@changed V0.9.9 (2001-02-06) [umoeller]: fixed call to doshPerfClose
 */

VOID PwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        // FIRST thing to do is stop the collect thread
        // a) set fExit flag so that collect thread won't
        //    collect any more
        pPrivate->tiCollect.fExit = TRUE;
        // b) post exit event, in case the collect thread
        //    is currently sleeping
        DosPostEventSem(pPrivate->hevExit);
        // c) now wait for collect thread to post "exit done"
        WinWaitEventSem(pPrivate->hevExitComplete, 2000);

        if (pPrivate->pBitmap)
            gpihDestroyXBitmap(&pPrivate->pBitmap);

        DosCloseMutexSem(pPrivate->hmtxData);
        DosCloseEventSem(pPrivate->hevExit);
        DosCloseEventSem(pPrivate->hevExitComplete);

        if (pPrivate->pPerfData)
            doshPerfClose(&pPrivate->pPerfData);

        // do not destroy pPrivate->hdcWin, it is
        // destroyed automatically

        free(pPrivate);
    } // end if (pWidget && pPrivate);
}

/*
 *@@ fnwpPulseWidget:
 *      window procedure for the "Pulse" widget class.
 *
 *      Supported setup strings:
 *
 *      --  "TEXTCOL=rrggbb": color for the percentage text (if displayed).
 *
 *      --  "BGNDCOL=rrggbb": background color.
 *
 *      --  "GRPHCOL=rrggbb": graph color.
 *
 *      --  "FONT=point.face": presentation font.
 *
 *      --  "WIDTH=cx": widget display width.
 */

MRESULT EXPENTRY fnwpPulseWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      as with all widgets, we receive a pointer to the
         *      XCENTERWIDGET in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGET pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGET.pUser for allocating
         *      WIDGETPRIVATE for our own stuff.
         *
         *      Each widget must write its desired width into
         *      XCENTERWIDGET.cx and cy.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            mrc = PwgtCreate(hwnd, mp1);
        break;

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)PwgtControl(hwnd, mp1, mp2);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            PwgtPaint(hwnd);
        break;

        /*
         * WM_TIMER:
         *      clock timer --> repaint.
         */

        case WM_TIMER:
            PwgtGetNewLoad(hwnd);
                // repaints!
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *      on window resize, allocate new bitmap.
         */

        case WM_WINDOWPOSCHANGED:
            PwgtWindowPosChanged(hwnd, mp1, mp2);
        break;

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            PwgtPresParamChanged(hwnd, (ULONG)mp1);
        break;

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
            PwgtDestroy(hwnd);
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        default:
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}


