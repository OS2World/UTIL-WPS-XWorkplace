
/*
 *@@sourcefile fdrsplit.c:
 *      folder "split view" implementation.
 *
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 *@@header "filesys\folder.h"
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WININPUT
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA
#define INCL_WINSCROLLBARS
#define INCL_WINSYS

#define INCL_GPIBITMAPS
#define INCL_GPIREGIONS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\statbars.h"           // status bar translation logic

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private declarations
 *
 ********************************************************************/

PCSZ    WC_SPLITVIEWCLIENT  = "XWPSplitViewClient",
        WC_SPLITPOPULATE   = "XWPSplitViewPopulate";


/*
 *@@ IMAGECACHEENTRY:
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

typedef struct _IMAGECACHEENTRY
{
    WPFileSystem    *pobjImage;     // a WPImageFile really

    HBITMAP         hbm;            // result from wpQueryBitmapHandle

} IMAGECACHEENTRY, *PIMAGECACHEENTRY;

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

PFNWP       G_pfnFrameProc = NULL;

HMTX        G_hmtxImages = NULLHANDLE;
LINKLIST    G_llImages;     // linked list of IMAGECACHEENTRY structs, auto-free

/* ******************************************************************
 *
 *   Image file cache
 *
 ********************************************************************/

static BOOL LockImages(VOID)
{
    if (G_hmtxImages)
        return !DosRequestMutexSem(G_hmtxImages, SEM_INDEFINITE_WAIT);

    if (!DosCreateMutexSem(NULL,
                           &G_hmtxImages,
                           0,
                           TRUE))
    {
        lstInit(&G_llImages,
                TRUE);      // auto-free
        return TRUE;
    }

    return FALSE;
}

static VOID UnlockImages(VOID)
{
    DosReleaseMutexSem(G_hmtxImages);
}

/* ******************************************************************
 *
 *   Painting container backgrounds
 *
 ********************************************************************/

/*
 *@@ SUBCLCNR:
 *      QWL_USER data for containers that were
 *      subclassed to paint backgrounds.
 *
 *      This gets created and set by fdrMakeCnrPaint.
 *      The view settings are then manipulated
 *      every time the
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

typedef struct _SUBCLCNR
{
    HWND    hwndCnr;
    PFNWP   pfnwpOrig;

    SIZEL   szlCnr;         // container dimensions, updated with
                            // every WM_WINDOWPOSCHANGED

    LONG    lcolBackground;

    // only if usColorOrBitmap == 0x128, we set the following:
    HBITMAP hbm;            // folder background bitmap
    SIZEL   szlBitmap;      // size of that bitmap

    PIBMFDRBKGND    pBackground;        // pointer to WPFolder instance data
                                        // with the bitmap settings

    /*
    USHORT  usTiledOrScaled;        // 0x132 == normal
                                    // 0x133 == tiled
                                    // 0x134 == scaled
                        // #define BKGND_NORMAL        0x132
                        // #define BKGND_TILED         0x133
                        // #define BKGND_SCALED        0x134
    USHORT  usScaleFactor;          // normally 1
    */

} SUBCLCNR, *PSUBCLCNR;

/*
 *@@ DrawBitmapClipped:
 *      draws a subrectangle of the given bitmap at
 *      the given coordinates.
 *
 *      The problem with CM_PAINTBACKGROUND is that
 *      we receive an update rectangle, and we MUST
 *      ONLY PAINT IN THAT RECTANGLE, or we'll overpaint
 *      parts of the container viewport that will not
 *      be refreshed otherwise.
 *
 *      At the same time, we must center or tile bitmaps.
 *      This function is a wrapper around WinDrawBitmap
 *      that will paint the given bitmap at the pptlOrigin
 *      position while making sure that only parts in
 *      prclClip will actually be painted.
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

VOID DrawBitmapClipped(HPS hps,             // in: presentation space
                       HBITMAP hbm,         // in: bitmap to paint
                       PPOINTL pptlOrigin,  // in: position to paint bitmap at
                       PSIZEL pszlBitmap,   // in: bitmap size
                       PRECTL prclClip)     // in: clip (update) rectangle
{
    RECTL   rclSubBitmap;
    POINTL  ptl;

    #ifdef DEBUG_CNRBITMAPS
        _PmpfF(("pptlOrigin->x: %d, pptlOrigin->y: %d",
            pptlOrigin->x, pptlOrigin->y));
    #endif

    /*
       -- pathological case:

          cxBitmap = 200
          cyBitmap = 300

          ��presentation space (cnr)�����������Ŀ     500
          �                                     �
          �                                     �
          �      ��bitmap������Ŀ   �rclClip�   �     400
          �      �              �   �       �   �
          �      �              �   �       �   �
          �      �              �  350     450  �     300
          �      �              �   �       �   �
          �      �              �   �       �   �
          �      100            �   ���������   �     200
          �      �              �               �
          �      �              �               �
          �     pptlOrigin�100���               �     100
          �                                     �
          �                                     �
          ���������������������������������������     0

          0      100    200    300     400     500
    */

    // do nothing if the bitmap is not even in the
    // clip rectangle

    if (    (prclClip->xLeft >= pptlOrigin->x + pszlBitmap->cx)
         || (prclClip->xRight <= pptlOrigin->x)
         || (prclClip->yBottom >= pptlOrigin->y + pszlBitmap->cy)
         || (prclClip->yTop <= pptlOrigin->y)
       )
    {
        #ifdef DEBUG_CNRBITMAPS
            _Pmpf(("  pathological case, returning"));
        #endif

        return;
    }

    if (prclClip->xLeft > pptlOrigin->x)
    {
        /*
           -- rclClip is to the right of bitmap origin:

              cxBitmap = 300
              cyBitmap = 300

              ��presentation space (cnr)�����������Ŀ     500
              �                                     �
              �                                     �
              �      ��bitmap��������������Ŀ       �     400
              �      �                      �       �
              �      �                      �       �
              �      �             ��rclClip���Ŀ   �     300
              �      100           300      �   �   �
              �      �             �        �   �   �
              �      �             ������200�����   �     200
              �      �                      �       �
              �      �                      �       �
              �     pptlOrigin���100���������       �     100
              �                                     �
              �                                     �
              ���������������������������������������     0

              0      100    200    300     400     500

              this gives us: rclSubBitmap xLeft   = 300 - 100 = 200
                                          yBottom = 200 - 100 = 100
                                          xRight  = 450 - 100 = 350 --> too wide, but shouldn't matter
                                          yTop    = 300 - 100 = 200

        */

        rclSubBitmap.xLeft =   prclClip->xLeft
                             - pptlOrigin->x;
        rclSubBitmap.xRight =  prclClip->xRight
                             - pptlOrigin->x;
        ptl.x = prclClip->xLeft;
    }
    else
    {
        /*
           -- rclClip is to the left of bitmap origin:

              cxBitmap = 300
              cyBitmap = 300

              ��presentation space (cnr)�����������Ŀ     500
              �                                     �
              �                                     �
              �      ��bitmap��������������Ŀ       �     400
              �      �                      �       �
              �      �                      �       �
              �  ��rclClip��������Ŀ        �       �     300
              �  50  �            300       �       �
              �  �   �             �        �       �
              �  �������������������        �       �     200
              �     100                     �       �
              �      �                      �       �
              �     pptlOrigin���100���������       �     100
              �                                     �
              �                                     �
              ���������������������������������������     0

              0      100    200    300     400     500

        */

        rclSubBitmap.xLeft  = 0;
        rclSubBitmap.xRight = prclClip->xRight - pptlOrigin->x;

        ptl.x = pptlOrigin->x;

    }

    // same thing for y
    if (prclClip->yBottom > pptlOrigin->y)
    {

        rclSubBitmap.yBottom =   prclClip->yBottom
                               - pptlOrigin->y;
        rclSubBitmap.yTop    =   prclClip->yTop
                               - pptlOrigin->y;
        ptl.y = prclClip->yBottom;
    }
    else
    {
        rclSubBitmap.yBottom  = 0;
        rclSubBitmap.yTop = prclClip->yTop - pptlOrigin->y;

        ptl.y = pptlOrigin->y;

    }

    if (    (rclSubBitmap.xLeft < rclSubBitmap.xRight)
         && (rclSubBitmap.yBottom < rclSubBitmap.yTop)
       )
        WinDrawBitmap(hps,
                      hbm,
                      &rclSubBitmap,
                      &ptl,
                      0,
                      0,
                      0);
}

/*
 *@@ PaintCnrBackground:
 *      implementation of CM_PAINTBACKGROUND in fnwpSubclCnr.
 *
 *      This is a MAJOR, MAJOR MESS. No wonder noone ever
 *      uses CM_PAINTBACKGROUND.
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

MRESULT PaintCnrBackground(HWND hwndCnr,
                           POWNERBACKGROUND pob)        // in: mp1 of CM_PAINTBACKGROUND
{
    PSUBCLCNR       pSubCnr;
    PIBMFDRBKGND    pBackground;
    if (    (pSubCnr = (PSUBCLCNR)WinQueryWindowPtr(hwndCnr, QWL_USER))
         && (pBackground = pSubCnr->pBackground)
       )
    {
        BOOL    fSwitched = FALSE;
        POINTL  ptl;

        // we need not paint the background if we
        // have a bitmap AND it should fill the
        // entire folder (avoid flicker)
        if (    (!pSubCnr->hbm)
             || (BKGND_NORMAL == pBackground->BkgndStore.usTiledOrScaled)
           )
        {
            fSwitched = gpihSwitchToRGB(pob->hps);

            WinFillRect(pob->hps,
                        &pob->rclBackground,
                        pSubCnr->lcolBackground);
        }

        if (    (!pSubCnr->hbm)
             || (!pSubCnr->szlBitmap.cx)     // avoid division by zero below
             || (!pSubCnr->szlBitmap.cy)
           )
            // we're done:
            return (MRESULT)TRUE;

        // draw bitmap then
        switch (pBackground->BkgndStore.usTiledOrScaled)
        {

            /*
             * BKGND_NORMAL:
             *      center the bitmap in the container, and
             *      do NOT scale. As opposed to the WPS, we
             *      do not even scale the bitmap if it is
             *      smaller than the container. I have always
             *      thought that is terribly ugly.
             */

            case BKGND_NORMAL:

                // center bitmap
                ptl.x = (pSubCnr->szlCnr.cx - pSubCnr->szlBitmap.cx) / 2;
                ptl.y = (pSubCnr->szlCnr.cy - pSubCnr->szlBitmap.cy) / 2;

                // draw only the parts of the bitmap that are
                // affected by the cnr update rectangle
                DrawBitmapClipped(pob->hps,
                                  pSubCnr->hbm,
                                  &ptl,
                                  &pSubCnr->szlBitmap,
                                  // clip rectangle: cnr
                                  // update rectangle
                                  &pob->rclBackground);
            break;

            /*
             * BKGND_TILED:
             *      draw the bitmap tiled (as many times as
             *      it fits into the container, unscaled).
             *      As opposed to the WPS, again, we don't
             *      even scale the bitmap if it's smaller
             *      than the container.
             */

            case BKGND_TILED:
                // we implement tiling by simply drawing
                // the bitmap in two loops of "rows" and
                // "columns"; however, we start with the
                // first multiple of szlBitmap.cy that is
                // affected by the update rectangle's bottom
                // to speed things up
                ptl.y =   pob->rclBackground.yBottom
                        / pSubCnr->szlBitmap.cy
                        * pSubCnr->szlBitmap.cy;
                while (ptl.y < pob->rclBackground.yTop)
                {
                    // inner loop: the "columns"
                    ptl.x =   pob->rclBackground.xLeft
                            / pSubCnr->szlBitmap.cx
                            * pSubCnr->szlBitmap.cx;
                    while (ptl.x < pob->rclBackground.xRight)
                    {
                        DrawBitmapClipped(pob->hps,
                                          pSubCnr->hbm,
                                          &ptl,
                                          &pSubCnr->szlBitmap,
                                          &pob->rclBackground);

                        ptl.x += pSubCnr->szlBitmap.cx;
                    }

                    ptl.y += pSubCnr->szlBitmap.cy;
                }
            break;

            /*
             * BKGND_SCALED:
             *      scale the bitmap to match the container
             *      size. This is a bit more difficult,
             *      since we can't use WinDrawBitmap, which
             *      only allows us to specify a subrectangle
             *      of the _source_ bitmap. So we have to
             *      use a clip region and bitblt explicitly.
             *      Not blazingly fast, but unless we use
             *      double buffering, there's no other way.
             */

            case BKGND_SCALED:
            {
                // @@todo ignore scaling factor for now
                POINTL  aptl[4];
                HRGN    hrgnOldClip = NULLHANDLE;
                RECTL   rclClip;
                memcpy(&rclClip, &pob->rclBackground, sizeof(RECTL));

                #ifdef DEBUG_CNRBITMAPS
                    _PmpfF(("BKGND_SCALED"));
                    _Pmpf(("    szlCnr.cx %d, cy %d",
                        pSubCnr->szlCnr.cx, pSubCnr->szlCnr.cy));
                    _Pmpf(("    rclClip.xLeft %d, yBottom %d, xRight %d, yTop %d",
                        rclClip.xLeft, rclClip.yBottom, rclClip.xRight, rclClip.yTop));
                #endif

                // reset clip region to "all" to quickly
                // get the old clip region; we must save
                // and restore that, or the cnr will stop
                // painting quickly
                GpiSetClipRegion(pob->hps,
                                 NULLHANDLE,
                                 &hrgnOldClip);        // out: old clip region
                GpiIntersectClipRectangle(pob->hps,
                                          &rclClip);

                memset(aptl, 0, sizeof(aptl));

                // aptl[0]: target bottom-left, is all 0

                // aptl[1]: target top-right (inclusive!)
                aptl[1].x = pSubCnr->szlCnr.cx - 1;
                aptl[1].y = pSubCnr->szlCnr.cy - 1;

                // aptl[2]: source bottom-left, is all 0

                // aptl[3]: source top-right (exclusive!)
                aptl[3].x = pSubCnr->szlBitmap.cx;
                aptl[3].y = pSubCnr->szlBitmap.cy;


                GpiWCBitBlt(pob->hps,
                            pSubCnr->hbm,
                            4L,             // must always be 4
                            &aptl[0],       // points array
                            ROP_SRCCOPY,
                            BBO_IGNORE);

                // restore the old clip region
                GpiSetClipRegion(pob->hps,
                                 hrgnOldClip,       // can be NULLHANDLE
                                 &hrgnOldClip);

                // hrgnOldClip now has the clip region that was
                // created by GpiIntersectClipRectangle, so delete that
                if (hrgnOldClip)
                    GpiDestroyRegion(pob->hps,
                                     hrgnOldClip);
            }
            break;
        }

        return (MRESULT)TRUE;
    }

    return (MRESULT)FALSE;
}

/*
 *@@ fnwpSubclCnr:
 *      window proc that our containers are subclassed with
 *      to implement container background painting. See
 *      PaintCnrBackground.
 *
 *      We have a SUBCLCNR struct in QWL_USER, which was
 *      put there by fdrMakeCnrPaint.
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

MRESULT EXPENTRY fnwpSubclCnr(HWND hwndCnr, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;
    PSUBCLCNR   pSubCnr;

    switch (msg)
    {
        case CM_PAINTBACKGROUND:
            mrc = PaintCnrBackground(hwndCnr, (POWNERBACKGROUND)mp1);
        break;

        case WM_WINDOWPOSCHANGED:
            // when we resize and have a bitmap, repaint the
            // entire container to get everything right
            if (pSubCnr = (PSUBCLCNR)WinQueryWindowPtr(hwndCnr, QWL_USER))
            {
                if (((PSWP)mp1)->fl & SWP_SIZE)
                {
                    pSubCnr->szlCnr.cx = ((PSWP)mp1)->cx;
                    pSubCnr->szlCnr.cy = ((PSWP)mp1)->cy;

                    if (pSubCnr->hbm)
                        WinInvalidateRect(hwndCnr, NULL, TRUE);
                }

                mrc = pSubCnr->pfnwpOrig(hwndCnr, msg, mp1, mp2);
            }
        break;

        case WM_VSCROLL:
        case WM_HSCROLL:
            if (pSubCnr = (PSUBCLCNR)WinQueryWindowPtr(hwndCnr, QWL_USER))
            {
                switch (SHORT2FROMMP(mp2))
                {
                    // case SB_LINEUP:
                    // case SB_LINEDOWN:
                    // case SB_PAGEUP:
                    // case SB_PAGEDOWN:
                    case SB_ENDSCROLL:
                    case SB_SLIDERPOSITION:
                        if (pSubCnr->hbm)
                            WinInvalidateRect(hwndCnr, NULL, TRUE);

                }

                mrc = pSubCnr->pfnwpOrig(hwndCnr, msg, mp1, mp2);
            }
        break;

        case WM_DESTROY:
            if (pSubCnr = (PSUBCLCNR)WinQueryWindowPtr(hwndCnr, QWL_USER))
            {
                mrc = pSubCnr->pfnwpOrig(hwndCnr, msg, mp1, mp2);

                free(pSubCnr);
            }
        break;

        default:
            pSubCnr = (PSUBCLCNR)WinQueryWindowPtr(hwndCnr, QWL_USER);
            mrc = pSubCnr->pfnwpOrig(hwndCnr, msg, mp1, mp2);
        break;
    }

    return mrc;
}

/*
 *@@ fdrMakeCnrPaint:
 *      subclasses the given cnr with fnwpSubclCnr
 *      to make it process CM_PAINTBACKGROUND.
 *
 *      This creates a SUBCLCNR struct and puts it
 *      into the cnr's QWL_USER.
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

BOOL fdrMakeCnrPaint(HWND hwndCnr)
{
    PSUBCLCNR   pSubCnr;
    CNRINFO     CnrInfo;

    if (cnrhQueryCnrInfo(hwndCnr, &CnrInfo))
    {
        CnrInfo.flWindowAttr |= CA_OWNERPAINTBACKGROUND;
        WinSendMsg(hwndCnr,
                   CM_SETCNRINFO,
                   (MPARAM)&CnrInfo,
                   (MPARAM)CMA_FLWINDOWATTR);

        if (pSubCnr = NEW(SUBCLCNR))
        {
            ZERO(pSubCnr);

            pSubCnr->hwndCnr = hwndCnr;
            WinSetWindowPtr(hwndCnr, QWL_USER, pSubCnr);
            if (pSubCnr->pfnwpOrig = WinSubclassWindow(hwndCnr,
                                                       fnwpSubclCnr))
                return TRUE;
        }
    }

    return FALSE;
}

/*
 *@@ ResolveColor:
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

LONG ResolveColor(LONG lcol)         // in: explicit color or SYSCLR_* index
{
    // 0x40000000 is a special color value that I have
    // seen in use only by the Desktop itself; I assume
    // this is for getting the desktop icon text color
    if (lcol == 0x40000000)
    {
        CHAR    szTemp[20];
        LONG    lRed, lGreen, lBlue;
        PrfQueryProfileString(HINI_USER,
                              "PM_Colors",
                              "DesktopIconText",
                              "0 0 0",
                              szTemp,
                              sizeof(szTemp));
        sscanf(szTemp, "%d %d %d", &lRed, &lGreen, &lBlue);
        lcol = MAKE_RGB(lRed, lGreen, lBlue);
    }
    // otherwise, if the highest byte is set in the
    // color value, it is really a negative SYSCLR_*
    // index; for backgrounds, this is normally
    // SYSCLR_WINDOW, so check this
    else if (lcol & 0xFF000000)
    {
#ifdef DEBUG_CNRBITMAPS
        #define DUMPCOL(i) case i: pcsz = # i; break
        PCSZ pcsz = "unknown index";
        switch (lcol)
        {
            DUMPCOL(SYSCLR_SHADOWHILITEBGND);
            DUMPCOL(SYSCLR_SHADOWHILITEFGND);
            DUMPCOL(SYSCLR_SHADOWTEXT);
            DUMPCOL(SYSCLR_ENTRYFIELD);
            DUMPCOL(SYSCLR_MENUDISABLEDTEXT);
            DUMPCOL(SYSCLR_MENUHILITE);
            DUMPCOL(SYSCLR_MENUHILITEBGND);
            DUMPCOL(SYSCLR_PAGEBACKGROUND);
            DUMPCOL(SYSCLR_FIELDBACKGROUND);
            DUMPCOL(SYSCLR_BUTTONLIGHT);
            DUMPCOL(SYSCLR_BUTTONMIDDLE);
            DUMPCOL(SYSCLR_BUTTONDARK);
            DUMPCOL(SYSCLR_BUTTONDEFAULT);
            DUMPCOL(SYSCLR_TITLEBOTTOM);
            DUMPCOL(SYSCLR_SHADOW);
            DUMPCOL(SYSCLR_ICONTEXT);
            DUMPCOL(SYSCLR_DIALOGBACKGROUND);
            DUMPCOL(SYSCLR_HILITEFOREGROUND);
            DUMPCOL(SYSCLR_HILITEBACKGROUND);
            DUMPCOL(SYSCLR_INACTIVETITLETEXTBGND);
            DUMPCOL(SYSCLR_ACTIVETITLETEXTBGND);
            DUMPCOL(SYSCLR_INACTIVETITLETEXT);
            DUMPCOL(SYSCLR_ACTIVETITLETEXT);
            DUMPCOL(SYSCLR_OUTPUTTEXT);
            DUMPCOL(SYSCLR_WINDOWSTATICTEXT);
            DUMPCOL(SYSCLR_SCROLLBAR);
            DUMPCOL(SYSCLR_BACKGROUND);
            DUMPCOL(SYSCLR_ACTIVETITLE);
            DUMPCOL(SYSCLR_INACTIVETITLE);
            DUMPCOL(SYSCLR_MENU);
            DUMPCOL(SYSCLR_WINDOW);
            DUMPCOL(SYSCLR_WINDOWFRAME);
            DUMPCOL(SYSCLR_MENUTEXT);
            DUMPCOL(SYSCLR_WINDOWTEXT);
            DUMPCOL(SYSCLR_TITLETEXT);
            DUMPCOL(SYSCLR_ACTIVEBORDER);
            DUMPCOL(SYSCLR_INACTIVEBORDER);
            DUMPCOL(SYSCLR_APPWORKSPACE);
            DUMPCOL(SYSCLR_HELPBACKGROUND);
            DUMPCOL(SYSCLR_HELPTEXT);
            DUMPCOL(SYSCLR_HELPHILITE);
        }

        _Pmpf(("  --> %d (%s)", lcol, pcsz));

#endif  // DEBUG_CNRBITMAPS

        lcol = WinQuerySysColor(HWND_DESKTOP,
                                lcol,
                                0);
    }

    return lcol;
}

/*
 *@@ fdrRemoveFromImageCache:
 *      removes the given object from the image cache.
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

BOOL fdrRemoveFromImageCache(WPObject *pobjImage)
{
    BOOL    brc = FALSE,
            fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockImages())
        {
            PLISTNODE pNode = lstQueryFirstNode(&G_llImages);
            while (pNode)
            {
                PIMAGECACHEENTRY pice = (PIMAGECACHEENTRY)pNode->pItemData;
                if (pice->pobjImage == pobjImage)
                {
                    // found:

                    // delete the bitmap
                    GpiDeleteBitmap(pice->hbm);

                    lstRemoveNode(&G_llImages,
                                  pNode);       // auto-free
                    brc = TRUE;
                    break;
                }

                pNode = pNode->pNext;
            }
        }
    }
    CATCH(excpt1)
    {
    }
    END_CATCH();

    if (fLocked)
        UnlockImages();

    return brc;
}

typedef BOOL32 _System somTP_wpQueryBitmapHandle(WPObject *somSelf,
                                                 HBITMAP *phBitmap,
                                                 HPAL *phPalette,
                                                 ULONG ulWidth,
                                                 ULONG ulHeight,
                                                 ULONG ulFlags,
                                                 LONG lBackgroundColor,
                                                 BOOL *pbQuitEarly);
typedef somTP_wpQueryBitmapHandle *somTD_wpQueryBitmapHandle;

/*
 *@@ GetBitmap:
 *      returns the bitmap handle for the given folder
 *      background structure. This will either be
 *      a bitmap from our image cache or a newly
 *      created one, if the image was not in the
 *      cache.
 *
 *      Returns NULLHANDLE if
 *
 *      --  the folder has no background bitmap,
 *
 *      --  the specified bitmap file does not exist or
 *          is not understood or
 *
 *      --  we're running on Warp 3. We require
 *          the help of the WPImageFile class here.
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

HBITMAP GetBitmap(PIBMFDRBKGND pBkgnd)
{
    HBITMAP hbm = NULLHANDLE;
    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (    (pBkgnd->BkgndStore.usColorOrBitmap == BKGND_BITMAP)
             && (pBkgnd->BkgndStore.pszBitmapFile)
           )
        {
            WPObject    *pobjImage;

            // check if WPFolder has found the WPImageFile
            // for us already (that is, if the folder had
            // a legacy open view already)
            if (pobjImage = pBkgnd->pobjImage)
            {
                // yes: check that it's valid
                CHAR szFilename[CCHMAXPATH] = "unknown";
                _wpQueryFilename(pBkgnd->pobjImage, szFilename, TRUE);
                if (stricmp(pBkgnd->BkgndStore.pszBitmapFile, szFilename))
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Background bitmap mismatch:\n"
                           "Folder has \"%s\", but image file is from \"%s\"",
                           pBkgnd->BkgndStore.pszBitmapFile,
                           szFilename);
            }
            else
            {
                // find the WPImageFile from the path, but DO NOT
                // set the WPImageFile* pointer in the IBMFDRBKGND
                // struct, or the WPS will go boom
                CHAR    szTemp[CCHMAXPATH];
                if (strchr(pBkgnd->BkgndStore.pszBitmapFile, '\\'))
                    strcpy(szTemp, pBkgnd->BkgndStore.pszBitmapFile);
                else
                {
                    // not fully qualified: assume ?:\os2\bitmap then
                    sprintf(szTemp,
                            "?:\\os2\\bitmap\\%s",
                            pBkgnd->BkgndStore.pszBitmapFile);
                }

                if (szTemp[0] == '?')
                    szTemp[0] = doshQueryBootDrive();

                pobjImage = _wpclsQueryObjectFromPath(_WPFileSystem,
                                                      szTemp);
            }

            if (pobjImage)
            {
                // now we have the WPImageFile...
                // in any case, LOCK it so it won't go dormant
                // behind our butt
                _wpLockObject(pobjImage);

                // now we need to return a HBITMAP... unfortunately
                // WPImageFile does NO reference counting whatsover
                // when doing _wpQueryBitmapHandle, but will create
                // a new HBITMAP for every call, which is just plain
                // stupid. So that's what we need the image cache
                // for: create only one HBITMAP for every image file,
                // no matter how many times it is used...

                // so check if we can find it in the cache
                if (fLocked = LockImages())
                {
                    PLISTNODE pNode = lstQueryFirstNode(&G_llImages);
                    while (pNode)
                    {
                        PIMAGECACHEENTRY pice = (PIMAGECACHEENTRY)pNode->pItemData;
                        if (pice->pobjImage == pobjImage)
                        {
                            // found:
                            hbm = pice->hbm;

                            // then we've locked it before and can unlock
                            // it once
                            // _wpUnlockObject(pobjImage);

                            break;
                        }

                        pNode = pNode->pNext;
                    }

                    if (!hbm)
                    {
                        // image file was not in cache:
                        // then create a HBITMAP and add a cache entry
                        somTD_wpQueryBitmapHandle _wpQueryBitmapHandle;
                        if (_wpQueryBitmapHandle = (somTD_wpQueryBitmapHandle)wpshResolveFor(
                                                                pobjImage,
                                                                NULL,
                                                                "wpQueryBitmapHandle"))
                        {
                            if (    (_wpQueryBitmapHandle(pobjImage,
                                                          &hbm,
                                                          NULL,      // no palette
                                                          0,         // width (do not scale)
                                                          0,         // height (do not scale)
                                                          0,         // flags, unused apparently
                                                          0,         // background color
                                                          NULL))
                                 && (hbm)
                               )
                            {
                                PIMAGECACHEENTRY pice;
                                if (pice = NEW(IMAGECACHEENTRY))
                                {
                                    ZERO(pice);
                                    pice->pobjImage = pobjImage;
                                    pice->hbm = hbm;

                                    lstAppendItem(&G_llImages,
                                                  pice);

                                    // remove from cache when it goes dormant
                                    _xwpModifyFlags(pobjImage,
                                                    OBJLIST_IMAGECACHE,
                                                    OBJLIST_IMAGECACHE);
                                }
                            }
                        }
                    }
                } // end if (fLocked = LockImages())
            }
        }
    }
    CATCH(excpt1)
    {
    }
    END_CATCH();

    if (fLocked)
        UnlockImages();

    return hbm;
}

/*
 *@@ SetCnrLayout:
 *      one-shot function for setting the view settings
 *      of the given container according to the instance
 *      settings of the given folder.
 *
 *      This sets the container's background color and
 *      bitmap, if applicable, foreground color and font.
 *
 *@@added V0.9.21 (2002-08-24) [umoeller]
 */

VOID SetCnrLayout(HWND hwndCnr,         // in: cnr whose colors and fonts are to be set
                  XFolder *pFolder,     // in: folder that cnr gets filled with
                  ULONG ulView)         // in: one of OPEN_CONTENTS, OPEN_DETAILS, OPEN_TREE
{
    TRY_LOUD(excpt1)
    {
        // try to figure out the background color
        XFolderData     *somThis = XFolderGetData(pFolder);
        PIBMFOLDERDATA  pFdrData;
        PIBMFDRBKGND    pBkgnd;
        if (    (pFdrData = (PIBMFOLDERDATA)_pvWPFolderData)
             && (pBkgnd = pFdrData->pCurrentBackground)
           )
        {
            LONG        lcolBack,
                        lcolFore;
            PSUBCLCNR   pSubCnr;

            // avoid flicker; the cnr repaints on
            // every presparam change
            WinEnableWindowUpdate(hwndCnr, FALSE);

            // 1) background color and bitmap

            lcolBack = ResolveColor(pBkgnd->BkgndStore.lcolBackground);

            if (pSubCnr = (PSUBCLCNR)WinQueryWindowPtr(hwndCnr, QWL_USER))
            {
                // container is subclassed:

                pSubCnr->lcolBackground = lcolBack;

                // set the bitmap handle to what the
                // folder wants; this creates a bitmap
                // if necessary and puts it in the
                // image cache
                if (pSubCnr->hbm = GetBitmap(pBkgnd))
                {
                    // folder has a bitmap:
                    BITMAPINFOHEADER2 bih;
                    bih.cbFix = sizeof(bih);
                    GpiQueryBitmapInfoHeader(pSubCnr->hbm,
                                             &bih);
                    pSubCnr->szlBitmap.cx = bih.cx;
                    pSubCnr->szlBitmap.cy = bih.cy;
                }

                pSubCnr->pBackground = pBkgnd;

                /*
                pSubCnr->usTiledOrScaled = pBkgnd->BkgndStore.usTiledOrScaled;
                pSubCnr->usScaleFactor = pBkgnd->BkgndStore.usScaleFactor;
                */
            }
            else
                // container is not subclassed:
                winhSetPresColor(hwndCnr,
                                 PP_BACKGROUNDCOLOR,
                                 lcolBack);

            // 2) foreground color

            switch (ulView)
            {
                case OPEN_CONTENTS:
                    lcolFore = pFdrData->LongArray.rgbIconViewTextColColumns;
                break;

                case OPEN_DETAILS:
                    lcolFore = pFdrData->LongArray.rgbDetlViewTextCol;
                break;

                case OPEN_TREE:
                    lcolFore = pFdrData->LongArray.rgbTreeViewTextColIcons;
                break;
            }

            lcolFore = ResolveColor(lcolFore);

            winhSetPresColor(hwndCnr,
                             PP_FOREGROUNDCOLOR,
                             lcolFore);

            // 3) set the font according to the view flag;
            // _wpQueryFldrFont returns a default font
            // properly if there's no instance setting
            // for this view
            winhSetWindowFont(hwndCnr,
                              _wpQueryFldrFont(pFolder, ulView));

            WinEnableWindowUpdate(hwndCnr, TRUE);
        }
    }
    CATCH(excpt1)
    {
    }
    END_CATCH();
}

/* ******************************************************************
 *
 *   Populate management
 *
 ********************************************************************/

/*
 *@@ fdrGetFSFromRecord:
 *      returns the WPFileSystem* which is represented
 *      by the specified record.
 *      This resolves shadows and returns root folders
 *      for WPDisk objects cleanly.
 *
 *      Returns NULL
 *
 *      -- if (fFoldersOnly) and precc does not represent
 *         a folder;
 *
 *      -- if (!fFoldersOnly) and precc does not represent
 *         a file-system object;
 *
 *      -- if the WPDisk or WPShadow cannot be resolved.
 *
 *      If fFoldersOnly and the return value is not NULL,
 *      it is guaranteed to be some kind of folder.
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 */

WPFileSystem* fdrGetFSFromRecord(PMINIRECORDCORE precc,
                                 BOOL fFoldersOnly)
{
    WPObject *pobj = NULL;
    if (    (precc)
         && (pobj = OBJECT_FROM_PREC(precc))
         && (pobj = objResolveIfShadow(pobj))
       )
    {
        if (_somIsA(pobj, _WPDisk))
            pobj = _xwpSafeQueryRootFolder(pobj,
                                           FALSE,   // no change map
                                           NULL);

        if (pobj)
        {
            if (fFoldersOnly)
            {
                if (!_somIsA(pobj, _WPFolder))
                    pobj = NULL;
            }
            else
                if (!_somIsA(pobj, _WPFileSystem))
                    pobj = NULL;
        }
    }

    return pobj;
}

/*
 *@@ fdrIsInsertable:
 *      checks if pObject can be inserted in a container.
 *
 *      The return value depends on ulInsert:
 *
 *      --  INSERT_ALL: we allow all objects to be inserted,
 *          even broken shadows. This is used by the split
 *          view for the files container.
 *
 *          pcszFileMask is ignored in this case.
 *
 *      --  INSERT_FILESYSTEMS: this inserts all WPFileSystem and
 *          WPDisk objects plus shadows pointing to them. This
 *          is for the files container in the file dialog,
 *          obviously, because opening abstracts with a file
 *          dialog is impossible (unless the abstract is a
 *          shadow pointing to a file-system object).
 *
 *          For file-system objects, if (pcszFileMask != NULL), the
 *          object's real name is checked against that file mask also.
 *          For example, if (pcszFileMask == *.TXT), this will
 *          return TRUE only if pObject's real name matches
 *          *.TXT.
 *
 *          This is for the right (files) view.
 *
 *      --  INSERT_FOLDERSONLY: only folders are inserted.
 *          We will not even insert disk objects or shadows,
 *          even if they point to shadows. We will also
 *          not insert folder templates.
 *
 *          pcszFileMask is ignored in this case.
 *
 *          This is for the left (drives) view when items
 *          are expanded.
 *
 *          This will NOT resolve shadows because if we insert
 *          shadows of folders into a container, their contents
 *          cannot be inserted a second time. The WPS shares
 *          records so each object can only be inserted once.
 *
 *      --  INSERT_FOLDERSANDDISKS: in addition to folders
 *          (as with INSERT_FOLDERSONLY), we allow insertion
 *          of drive objects too.
 *
 *      In any case, FALSE is returned if the object matches
 *      the above, but the object has the "hidden" attribute on.
 */

BOOL fdrIsInsertable(WPObject *pObject,
                     ULONG ulFoldersOnly,
                     PCSZ pcszFileMask)     // in: upper-case file mask or NULL
{
    if (!pObject)
        return FALSE;       // in any case

    if (ulFoldersOnly > 1)      // INSERT_FOLDERSONLY or INSERT_FOLDERSANDDISKS
    {
        // folders only:
        WPObject *pobj2;

        if (_wpQueryStyle(pObject) & OBJSTYLE_TEMPLATE)
            return FALSE;

        // allow disks with INSERT_FOLDERSANDDISKS only
        if (    (ulFoldersOnly == INSERT_FOLDERSANDDISKS)
             && (    (_somIsA(pObject, _WPDisk))
                  || (    (pobj2 = objResolveIfShadow(pObject))
                       && (ctsIsSharedDir(pobj2))
                     )
                )
           )
        {
            // always insert, even if drive not ready
            return TRUE;
        }

        if (_somIsA(pObject, _WPFolder))
        {
            // filter out folder templates and hidden folders
            if (    (!(_wpQueryStyle(pObject) & OBJSTYLE_TEMPLATE))
                 && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
               )
                return TRUE;
        }

        return FALSE;
    }

    // INSERT_ALL or INSERT_FILESYSTEMS:

    // resolve shadows
    if (!(pObject = objResolveIfShadow(pObject)))
        // broken shadow:
        // disallow with INSERT_FILESYSTEMS
        return (ulFoldersOnly == INSERT_ALL);

    if (ulFoldersOnly == INSERT_ALL)
        return TRUE;

    // INSERT_FILESYSTEMS:

    if (_somIsA(pObject, _WPDisk))
        return TRUE;

    if (    // filter out non-file systems (shadows pointing to them have been resolved):
            (_somIsA(pObject, _WPFileSystem))
            // filter out hidden objects:
         && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
       )
    {
        // OK, non-hidden file-system object:
        // regardless of filters, always insert folders
        if (_somIsA(pObject, _WPFolder))
            return TRUE;          // templates too

        if ((pcszFileMask) && (*pcszFileMask))
        {
            // file mask specified:
            CHAR szRealName[CCHMAXPATH];
            if (_wpQueryFilename(pObject,
                                 szRealName,
                                 FALSE))       // not q'fied
            {
                return doshMatch(pcszFileMask, szRealName);
            }
        }
        else
            // no file mask:
            return TRUE;
    }

    return FALSE;
}

/*
 *@@ IsObjectInCnr:
 *      returns TRUE if pObject has already been
 *      inserted into hwndCnr.
 *
 */

BOOL fdrIsObjectInCnr(WPObject *pObject,
                      HWND hwndCnr)
{
    BOOL    brc = FALSE;
    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = !_wpRequestObjectMutexSem(pObject, SEM_INDEFINITE_WAIT))
        {
            PUSEITEM    pUseItem = NULL;
            for (pUseItem = _wpFindUseItem(pObject, USAGE_RECORD, NULL);
                 pUseItem;
                 pUseItem = _wpFindUseItem(pObject, USAGE_RECORD, pUseItem))
            {
                // USAGE_RECORD specifies where this object is
                // currently inserted
                PRECORDITEM pRecordItem = (PRECORDITEM)(pUseItem + 1);

                if (hwndCnr == pRecordItem->hwndCnr)
                {
                    brc = TRUE;
                    break;
                }
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
        _wpReleaseObjectMutexSem(pObject);

    return brc;
}

/*
 *@@ fdrInsertContents:
 *      inserts the contents of the given folder into
 *      the given container.
 *
 *      It is assumed that the folder is already populated.
 *
 *      If (precParent != NULL), the contents are inserted
 *      as child records below that record. Of course that
 *      will work in Tree view only.
 *
 *      In addition, if (hwndAddFirstChild != NULLHANDLE),
 *      this will fire an CM_ADDFIRSTCHILD msg to that
 *      window for every record that was inserted.
 *
 */

VOID fdrInsertContents(WPFolder *pFolder,              // in: populated folder
                       HWND hwndCnr,                   // in: cnr to insert records to
                       PMINIRECORDCORE precParent,     // in: parent record or NULL
                       ULONG ulFoldersOnly,            // in: as with fdrIsInsertable
                       HWND hwndAddFirstChild,         // in: if != 0, we post CM_ADDFIRSTCHILD for each item too
                       PCSZ pcszFileMask,              // in: file mask filter or NULL
                       PLINKLIST pllObjects)           // in/out: linked list of objs that were inserted
{
    BOOL        fFolderLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        // lock the folder contents for wpQueryContent
        if (fFolderLocked = !_wpRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            // build an array of objects that should be inserted
            // (we use an array because we want to use
            // _wpclsInsertMultipleObjects);
            // we run through the folder and check if the object
            // is insertable and, if so, add it to the array,
            // which is increased in size in chunks of 1000 pointers
            WPObject    *pObject;
            WPObject    **papObjects = NULL;
            ULONG       cObjects = 0,
                        cArray = 0,
                        cAddFirstChilds = 0;

            for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                    pObject;
                    pObject = *__get_pobjNext(pObject)
                )
            {
                if (fdrIsInsertable(pObject,
                                    ulFoldersOnly,
                                    pcszFileMask))
                {
                    // _wpclsInsertMultipleObjects fails on the
                    // entire array if only one is already in the
                    // cnr, so make sure it isn't
                    if (!fdrIsObjectInCnr(pObject, hwndCnr))
                    {
                        // create/expand array if necessary
                        if (cObjects >= cArray)     // on the first iteration,
                                                    // both are null
                        {
                            cArray += 1000;
                            papObjects = (WPObject**)realloc(papObjects, // NULL on first call
                                                             cArray * sizeof(WPObject*));
                        }

                        // store in array
                        papObjects[cObjects++] = pObject;

                        // if caller wants a list, add that to
                        if (pllObjects)
                            lstAppendItem(pllObjects, pObject);
                    }

                    // even if the object is already in the
                    // cnr, if we are in "add first child"
                    // mode, add the first child later;
                    // this works because hwndAddFirstChild
                    // is on the same thread
                    if (    (hwndAddFirstChild)
                         && (objIsAFolder(pObject))
                       )
                    {
                        if (!cAddFirstChilds)
                        {
                            // first post: tell thread to update
                            // the wait pointer
                            WinPostMsg(hwndAddFirstChild,
                                       FM2_ADDFIRSTCHILD_BEGIN,
                                       0,
                                       0);
                            cAddFirstChilds++;
                        }

                        WinPostMsg(hwndAddFirstChild,
                                   FM2_ADDFIRSTCHILD_NEXT,
                                   (MPARAM)pObject,
                                   NULL);
                    }
                }
            }

            #ifdef DEBUG_POPULATESPLITVIEW
                _PmpfF(("--> got %d objects to insert, %d to add first child",
                        cObjects, cAddFirstChilds));
            #endif

            if (cObjects)
                _wpclsInsertMultipleObjects(_somGetClass(pFolder),
                                            hwndCnr,
                                            NULL,
                                            (PVOID*)papObjects,
                                            precParent,
                                            cObjects);

            if (papObjects)
                free(papObjects);

            if (cAddFirstChilds)
            {
                // we had any "add-first-child" posts:
                // post another msg which will get processed
                // after all the "add-first-child" things
                // so that the wait ptr can be reset
                WinPostMsg(hwndAddFirstChild,
                           FM2_ADDFIRSTCHILD_DONE,
                           0, 0);
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fFolderLocked)
        _wpReleaseFolderMutexSem(pFolder);
}

/*
 *@@ ClearContainer:
 *      removes all objects that were inserted into the
 *      specified container and updates the USEITEM's
 *      correctly.
 */

ULONG fdrClearContainer(HWND hwndCnr,              // in: cnr to clear
                        PLINKLIST pllObjects)      // in: list of objects to remove
{
    ULONG       ulrc = 0;
    PLISTNODE   pNode;
    // OK, nuke the container contents...
    // we could simply call wpCnrRemoveObject
    // for each object in the container, but
    // this causes a lot of overhead.
    // From my testing, wpCnrRemoveObject
    // does the following:
    // 1) search the object's useitems and
    //    remove the matching RECORDITEM;
    // 2) only after that, it attempts to
    //    remove the record from the container.
    //    If that fails, the useitem has been
    //    removed anyway.

    // So to speed things up, we just kill
    // the entire container contents in one
    // flush and then call wpCnrRemoveObject
    // on each object that was inserted...
    WinSendMsg(hwndCnr,
               CM_REMOVERECORD,
               NULL,                // all records
               MPFROM2SHORT(0,      // all records
                            CMA_INVALIDATE));
                                // no free, WPS shares records

    // WinEnableWindowUpdate(hwndCnr, FALSE);

    pNode = lstQueryFirstNode(pllObjects);
    while (pNode)
    {
        WPObject *pobj = (WPObject*)pNode->pItemData;
        _wpCnrRemoveObject(pobj,
                           hwndCnr);
        _wpUnlockObject(pobj);       // was locked by "insert"
        pNode = pNode->pNext;
        ulrc++;
    }

    lstClear(pllObjects);

    //  WinEnableWindowUpdate(hwndCnr, TRUE);

    return (ulrc);
}

/* ******************************************************************
 *
 *   Split Populate thread
 *
 ********************************************************************/

/*
 *@@ AddFirstChild:
 *      adds the first child record for precParent
 *      to the given container if precParent represents
 *      a folder that has subfolders.
 *
 *      This gets called for every record in the drives
 *      tree so we properly add the "+" expansion signs
 *      to each record without having to fully populate
 *      each folder. This is an imitation of the standard
 *      WPS behavior in Tree views.
 *
 *      Runs on the split populate thread (fntSplitPopulate).
 *
 *@@added V0.9.18 (2002-02-06) [umoeller]
 */

static WPObject* AddFirstChild(WPFolder *pFolder,
                               PMINIRECORDCORE precParent,     // in: folder record to insert first child for
                               HWND hwndCnr,                   // in: cnr where precParent is inserted
                               PLINKLIST pll)                  // in/out: list of objs
{
    PMINIRECORDCORE     precFirstChild;
    WPFolder            *pFirstChildFolder = NULL;

    if (!(precFirstChild = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                       CM_QUERYRECORD,
                                                       (MPARAM)precParent,
                                                       MPFROM2SHORT(CMA_FIRSTCHILD,
                                                                    CMA_ITEMORDER))))
    {
        // we don't have a first child already:

        // check if we have a subfolder in the folder already
        BOOL    fFolderLocked = FALSE,
                fFindLocked = FALSE;

        #ifdef DEBUG_POPULATESPLITVIEW
            _Pmpf(("  "__FUNCTION__": CM_QUERYRECORD returned NULL"));
        #endif

        TRY_LOUD(excpt1)
        {
            // request the find sem to make sure we won't have a populate
            // on the other thread; otherwise we get duplicate objects here
            if (fFindLocked = !_wpRequestFindMutexSem(pFolder, SEM_INDEFINITE_WAIT))
            {
                WPObject    *pObject;

                if (fFolderLocked = !_wpRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
                {
                    for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                            pObject;
                            pObject = *__get_pobjNext(pObject))
                    {
                        if (fdrIsInsertable(pObject,
                                            INSERT_FOLDERSONLY,
                                            NULL))
                        {
                            pFirstChildFolder = pObject;
                            break;
                        }
                    }

                    _wpReleaseFolderMutexSem(pFolder);
                    fFolderLocked = FALSE;
                }

                #ifdef DEBUG_POPULATESPLITVIEW
                    _Pmpf(("  "__FUNCTION__": pFirstChildFolder pop is 0x%lX", pFirstChildFolder));
                #endif

                if (!pFirstChildFolder)
                {
                    // no folder awake in folder yet:
                    // do a quick DosFindFirst loop to find the
                    // first subfolder in here
                    HDIR          hdir = HDIR_CREATE;
                    FILEFINDBUF3  ffb3     = {0};
                    ULONG         ulFindCount    = 1;        // look for 1 file at a time
                    APIRET        arc            = NO_ERROR;

                    CHAR          szFolder[CCHMAXPATH],
                                  szSearchMask[CCHMAXPATH];

                    _wpQueryFilename(pFolder, szFolder, TRUE);
                    sprintf(szSearchMask, "%s\\*", szFolder);

                    #ifdef DEBUG_POPULATESPLITVIEW
                        _Pmpf(("  "__FUNCTION__": searching %s", szSearchMask));
                    #endif

                    ulFindCount = 1;
                    arc = DosFindFirst(szSearchMask,
                                       &hdir,
                                       MUST_HAVE_DIRECTORY | FILE_ARCHIVED | FILE_SYSTEM | FILE_READONLY,
                                             // but exclude hidden
                                       &ffb3,
                                       sizeof(ffb3),
                                       &ulFindCount,
                                       FIL_STANDARD);

                    while ((arc == NO_ERROR))
                    {
                        #ifdef DEBUG_POPULATESPLITVIEW
                            _Pmpf(("      "__FUNCTION__": got %s", ffb3.achName));
                        #endif

                        // do not use "." and ".."
                        if (    (strcmp(ffb3.achName, ".") != 0)
                             && (strcmp(ffb3.achName, "..") != 0)
                           )
                        {
                            // this is good:
                            CHAR szFolder2[CCHMAXPATH];
                            sprintf(szFolder2, "%s\\%s", szFolder, ffb3.achName);

                            #ifdef DEBUG_POPULATESPLITVIEW
                                _Pmpf(("      "__FUNCTION__": awaking %s", szFolder2));
                            #endif

                            pObject = _wpclsQueryFolder(_WPFolder,
                                                        szFolder2,
                                                        TRUE);
                            // exclude templates
                            if (fdrIsInsertable(pObject,
                                                INSERT_FOLDERSONLY,
                                                NULL))
                            {
                                pFirstChildFolder = pObject;
                                break;
                            }
                        }

                        // search next file
                        ulFindCount = 1;
                        arc = DosFindNext(hdir,
                                         &ffb3,
                                         sizeof(ffb3),
                                         &ulFindCount);

                    } // end while (rc == NO_ERROR)

                    DosFindClose(hdir);
                }
            }
        }
        CATCH(excpt1)
        {
        } END_CATCH();

        if (fFolderLocked)
            _wpReleaseFolderMutexSem(pFolder);
        if (fFindLocked)
            _wpReleaseFindMutexSem(pFolder);

        if (pFirstChildFolder)
        {
            POINTL ptl = {0, 0};
            if (_wpCnrInsertObject(pFirstChildFolder,
                                   hwndCnr,
                                   &ptl,        // without this the func fails
                                   precParent,
                                   NULL))
                lstAppendItem(pll,
                              pFirstChildFolder);
        }
    }

    return pFirstChildFolder;
}

/*
 *@@ fnwpSplitPopulate:
 *      object window for populate thread.
 *
 *      Runs on the split populate thread (fntSplitPopulate).
 */

static MRESULT EXPENTRY fnwpSplitPopulate(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);       // PFDRSPLITVIEW
        break;

        /*
         *@@ FM2_POPULATE:
         *      posted by fnwpMainControl when
         *      FM_FILLFOLDER comes in to offload
         *      populate to this second thread.
         *
         *      After populate is done, we post the
         *      following back to fnwpMainControl:
         *
         *      --  FM_POPULATED_FILLTREE always so
         *          that the drives tree can get
         *          updated;
         *
         *      --  FM_POPULATED_SCROLLTO, if the
         *          FFL_SCROLLTO flag was set;
         *
         *      --  FM_POPULATED_FILLFILES, if the
         *          FFL_FOLDERSONLY flag was _not_
         *          set.
         *
         *      This processing is all new with V0.9.18
         *      to finally synchronize the populate with
         *      the main thread better.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCORE mp1
         *
         *      --  ULONG mp2: flags, as with FM_FILLFOLDER.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_POPULATE:
        {
            PFDRSPLITVIEW   psv = WinQueryWindowPtr(hwnd, QWL_USER);
            WPFolder        *pFolder;
            PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;
            ULONG           fl = (ULONG)mp2;

            if (pFolder = fdrGetFSFromRecord(prec, TRUE))
            {
                BOOL fFoldersOnly = ((fl & FFL_FOLDERSONLY) != 0);

                // set wait pointer
                (psv->cThreadsRunning)++;
                WinPostMsg(psv->hwndMainControl,
                           FM_UPDATEPOINTER,
                           0,
                           0);

                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("populating %s", _wpQueryTitle(pFolder)));
                #endif

                if (fdrCheckIfPopulated(pFolder,
                                        fFoldersOnly))
                {
                    // in any case, refresh the tree
                    WinPostMsg(psv->hwndMainControl,
                               FM_POPULATED_FILLTREE,
                               (MPARAM)prec,
                               (MPARAM)fl);
                            // fnwpMainControl will check fl again and
                            // fire "add first child" msgs accordingly

                    if (fl & FFL_SCROLLTO)
                        WinPostMsg(psv->hwndMainControl,
                                   FM_POPULATED_SCROLLTO,
                                   (MPARAM)prec,
                                   0);

                    if (!fFoldersOnly)
                        // refresh the files only if we are not
                        // in folders-only mode
                        WinPostMsg(psv->hwndMainControl,
                                   FM_POPULATED_FILLFILES,
                                   (MPARAM)prec,
                                   (MPARAM)pFolder);
                }

                // clear wait pointer
                (psv->cThreadsRunning)--;
                WinPostMsg(psv->hwndMainControl,
                           FM_UPDATEPOINTER,
                           0,
                           0);
            }
        }
        break;

        /*
         *@@ FM2_ADDFIRSTCHILD_BEGIN:
         *      posted by InsertContents before the first
         *      FM2_ADDFIRSTCHILD_NEXT is posted so we
         *      can update the "wait" ptr accordingly.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_ADDFIRSTCHILD_BEGIN:
        {
            PFDRSPLITVIEW   psv = WinQueryWindowPtr(hwnd, QWL_USER);
            (psv->cThreadsRunning)++;
            WinPostMsg(psv->hwndMainControl,
                       FM_UPDATEPOINTER,
                       0, 0);
        }
        break;

        /*
         *@@ FM2_ADDFIRSTCHILD_NEXT:
         *      fired by InsertContents for every folder that
         *      is added to the drives tree.
         *
         *      Parameters:
         *
         *      --  WPFolder* mp1: folder to add first child for.
         *          This better be in the tree.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_ADDFIRSTCHILD_NEXT:
            if (mp1)
            {
                PFDRSPLITVIEW       psv = WinQueryWindowPtr(hwnd, QWL_USER);
                HWND                hwndCnr = psv->hwndTreeCnr;
                WPFolder            *pFolder = (WPObject*)mp1;
                PMINIRECORDCORE     precParent = _wpQueryCoreRecord(pFolder);

                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("CM_ADDFIRSTCHILD %s", _wpQueryTitle(mp1)));
                #endif

                AddFirstChild(pFolder,
                              precParent,
                              hwndCnr,
                              &psv->llTreeObjectsInserted);
            }
        break;

        /*
         *@@ FM2_ADDFIRSTCHILD_DONE:
         *      posted by InsertContents after the last
         *      FM2_ADDFIRSTCHILD_NEXT was posted so we
         *      can reset the "wait" ptr.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_ADDFIRSTCHILD_DONE:
        {
            PFDRSPLITVIEW   psv = WinQueryWindowPtr(hwnd, QWL_USER);
            (psv->cThreadsRunning)--;
            WinPostMsg(psv->hwndMainControl,
                       FM_UPDATEPOINTER,
                       0,
                       0);
        }
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ fntSplitPopulate:
 *      "split populate" thread. This creates an object window
 *      so that we can easily serialize the order in which
 *      folders are populate and such.
 *
 *      This is responsible for both populating folders _and_
 *      doing the "add first child" processing. This was all
 *      new with V0.9.18's file dialog and was my second attempt
 *      at getting the thread synchronization right, which
 *      turned out to work pretty well.
 *
 *      We _need_ a second thread for "add first child" too
 *      because even adding the first child can take quite a
 *      while. For example, if a folder has 1,000 files in it
 *      and the 999th is a directory, the file system has to
 *      scan the entire contents first.
 */

static VOID _Optlink fntSplitPopulate(PTHREADINFO ptiMyself)
{
    TRY_LOUD(excpt1)
    {
        QMSG qmsg;
        PFDRSPLITVIEW psv = (PFDRSPLITVIEW)ptiMyself->ulData;

        #ifdef DEBUG_POPULATESPLITVIEW
            _PmpfF(("thread starting"));
        #endif

        WinRegisterClass(ptiMyself->hab,
                         (PSZ)WC_SPLITPOPULATE,
                         fnwpSplitPopulate,
                         0,
                         sizeof(PVOID));
        if (!(psv->hwndSplitPopulate = winhCreateObjectWindow(WC_SPLITPOPULATE,
                                                              psv)))
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Cannot create split populate object window.");

        // thread 1 is waiting for obj window to be created
        DosPostEventSem(ptiMyself->hevRunning);

        while (WinGetMsg(ptiMyself->hab, &qmsg, NULLHANDLE, 0, 0))
            WinDispatchMsg(ptiMyself->hab, &qmsg);

        WinDestroyWindow(psv->hwndSplitPopulate);

        #ifdef DEBUG_POPULATESPLITVIEW
            _PmpfF(("thread ending"));
        #endif

    }
    CATCH(excpt1) {} END_CATCH();

}

/*
 *@@ fdrSplitPopulate:
 *      posts FM2_POPULATE to fnwpSplitPopulate to
 *      populate the folder represented by the
 *      given MINIRECORDCORE according to fl.
 *
 *      fnwpSplitPopulate does the following:
 *
 *      1)  Before populating, raise psv->cThreadsRunning
 *          and post FM_UPDATEPOINTER to hwndMainControl.
 *          to have it display the "wait" pointer.
 *
 *      2)  Run _wpPopulate on the folder represented
 *          by prec.
 *
 *      3)  Post FM_POPULATED_FILLTREE back to hwndMainControl
 *          in any case to fill the tree under prec with the
 *          subfolders that were found.
 *
 *      4)  If the FFL_SCROLLTO flag is set, post
 *          FM_POPULATED_SCROLLTO back to hwndMainControl
 *          so that the tree can be scrolled properly.
 *
 *      5)  If the FFL_FOLDERSONLY flag was _not_ set,
 *          post FM_POPULATED_FILLFILES to hwndMainControl
 *          so it can insert all objects into the files
 *          container.
 *
 *      6)  Decrement psv->cThreadsRunning and post
 *          FM_UPDATEPOINTER again to reset the wait
 *          pointer.
 */

VOID fdrSplitPopulate(PFDRSPLITVIEW psv,
                      PMINIRECORDCORE prec,
                      ULONG fl)
{
    #ifdef DEBUG_POPULATESPLITVIEW
        _PmpfF(("psv->hwndSplitPopulate: 0x%lX", psv->hwndSplitPopulate));
    #endif

    WinPostMsg(psv->hwndSplitPopulate,
               FM2_POPULATE,
               (MPARAM)prec,
               (MPARAM)fl);
}

#ifdef DEBUG_POPULATESPLITVIEW
VOID DumpFlags(ULONG fl)
{
    _Pmpf(("  fl %s %s %s",
                (fl & FFL_FOLDERSONLY) ? "FFL_FOLDERSONLY " : "",
                (fl & FFL_SCROLLTO) ? "FFL_SCROLLTO " : "",
                (fl & FFL_EXPAND) ? "FFL_EXPAND " : ""));
}
#else
    #define DumpFlags(fl)
#endif

/*
 *@@ fdrPostFillFolder:
 *      posts FM_FILLFOLDER to the main control
 *      window with the given parameters.
 *
 *      The main control window will then call
 *      fdrSplitPopulate to have FM2_POPULATE
 *      posted to the split populate thread.
 */

VOID fdrPostFillFolder(PFDRSPLITVIEW psv,
                       PMINIRECORDCORE prec,       // in: record with folder to populate
                       ULONG fl)                   // in: FFL_* flags
{
    psv->precFolderContentsShowing = prec;

    WinPostMsg(psv->hwndMainControl,
               FM_FILLFOLDER,
               (MPARAM)prec,
               (MPARAM)fl);
}

/*
 *@@ fdrSplitQueryPointer:
 *      returns the HPOINTER that should be used
 *      according to the present thread state.
 *
 *      Returns a HPOINTER for either the wait or
 *      arrow pointer.
 */

HPOINTER fdrSplitQueryPointer(PFDRSPLITVIEW psv)
{
    ULONG           idPtr = SPTR_ARROW;

    if (    (psv)
         && (psv->cThreadsRunning)
       )
        idPtr = SPTR_WAIT;

    return WinQuerySysPointer(HWND_DESKTOP,
                              idPtr,
                              FALSE);
}

/* ******************************************************************
 *
 *   Split view main control (main frame's client)
 *
 ********************************************************************/

static MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);

/*
 *@@ fdrCreateFrameWithCnr:
 *      creates a new WC_FRAME with a WC_CONTAINER as its
 *      client window, with hwndParentOwner being the parent
 *      and owner of the frame.
 *
 *      With flCnrStyle, specify the cnr style to use. The
 *      following may optionally be set:
 *
 *      --  CCS_MINIICONS (optionally)
 *
 *      --  CCS_EXTENDSEL: allow zero, one, many icons to be
 *          selected (default WPS style).
 *
 *      --  CCS_SINGLESEL: allow only exactly one icon to be
 *          selected at a time.
 *
 *      --  CCS_MULTIPLESEL: allow zero, one, or more icons
 *          to be selected, and toggle selections (totally
 *          unusable).
 *
 *      WS_VISIBLE, WS_SYNCPAINT, CCS_AUTOPOSITION, and
 *      CCS_MINIRECORDCORE will always be set.
 *
 *      Returns the frame.
 *
 */

HWND fdrCreateFrameWithCnr(ULONG ulFrameID,
                           HWND hwndParentOwner,     // in: main client window
                           ULONG flCnrStyle,         // in: cnr style
                           HWND *phwndClient)        // out: client window (cnr)
{
    HWND    hwndFrame;
    ULONG   ws =   WS_VISIBLE
                 | WS_SYNCPAINT
                 | CCS_AUTOPOSITION
                 | CCS_MINIRECORDCORE
                 | flCnrStyle;

    if (hwndFrame = winhCreateStdWindow(hwndParentOwner, // parent
                                        NULL,          // pswpFrame
                                        FCF_NOBYTEALIGN,
                                        WS_VISIBLE,
                                        "",
                                        0,             // resources ID
                                        WC_CONTAINER,  // client
                                        ws,            // client style
                                        ulFrameID,
                                        NULL,
                                        phwndClient))
    {
        // set client as owner
        WinSetOwner(hwndFrame, hwndParentOwner);
    }

    return hwndFrame;
}

/*
 *@@ fdrSetupSplitView:
 *      creates all the subcontrols of the main controller
 *      window, that is, the split window with the two
 *      subframes and containers.
 *
 *      Returns NULL if no error occured. As a result,
 *      the return value can be returned from WM_CREATE,
 *      which stops window creation if != 0 is returned.
 */

MPARAM fdrSetupSplitView(HWND hwnd,
                         BOOL fMultipleSel,     // in: allow multiple selections in files cnr?
                         PFDRSPLITVIEW psv)
{
    MPARAM mrc = (MPARAM)FALSE;         // return value of WM_CREATE: 0 == OK

    SPLITBARCDATA sbcd;
    HAB hab = WinQueryAnchorBlock(hwnd);

    lstInit(&psv->llTreeObjectsInserted, FALSE);
    lstInit(&psv->llFileObjectsInserted, FALSE);

    // set the window font for the main client...
    // all controls will inherit this
    winhSetWindowFont(hwnd,
                      cmnQueryDefaultFont());

    /*
     *  split window with two containers
     *
     */

    // create two subframes to be linked in split window

    // 1) left: drives tree
    psv->hwndTreeFrame = fdrCreateFrameWithCnr(ID_TREEFRAME,
                                               hwnd,    // main client
                                               CCS_MINIICONS | CCS_SINGLESEL,
                                               &psv->hwndTreeCnr);
    BEGIN_CNRINFO()
    {
        cnrhSetView(   CV_TREE | CA_TREELINE | CV_ICON
                     | CV_MINI);
        cnrhSetTreeIndent(20);
        cnrhSetSortFunc(fnCompareName);             // shared/cnrsort.c
    } END_CNRINFO(psv->hwndTreeCnr);

    // 2) right: files
    psv->hwndFilesFrame = fdrCreateFrameWithCnr(ID_FILESFRAME,
                                                hwnd,    // main client
                                                (fMultipleSel)
                                                   ? CCS_MINIICONS | CCS_EXTENDSEL
                                                   : CCS_MINIICONS | CCS_SINGLESEL,
                                                &psv->hwndFilesCnr);
    BEGIN_CNRINFO()
    {
        cnrhSetView(   CV_NAME | CV_FLOW
                     | CV_MINI);
        cnrhSetTreeIndent(30);
        cnrhSetSortFunc(fnCompareNameFoldersFirst);     // shared/cnrsort.c
    } END_CNRINFO(psv->hwndFilesCnr);

    // 3) fonts
    winhSetWindowFont(psv->hwndTreeCnr,
                      cmnQueryDefaultFont());
    winhSetWindowFont(psv->hwndFilesCnr,
                      cmnQueryDefaultFont());

    // create split window
    sbcd.ulSplitWindowID = 1;
        // split window becomes client of main frame
    sbcd.ulCreateFlags =   SBCF_VERTICAL
                         | SBCF_PERCENTAGE
                         | SBCF_3DEXPLORERSTYLE
                         | SBCF_MOVEABLE;
    sbcd.lPos = psv->lSplitBarPos;   // in percent
    sbcd.ulLeftOrBottomLimit = 100;
    sbcd.ulRightOrTopLimit = 100;
    sbcd.hwndParentAndOwner = hwnd;         // client

    if (!(psv->hwndSplitWindow = ctlCreateSplitWindow(hab,
                                                      &sbcd)))
    {
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                "Cannot create split window.");
        // stop window creation!
        mrc = (MPARAM)TRUE;
    }
    else
    {
        // link left and right container
        WinSendMsg(psv->hwndSplitWindow,
                   SPLM_SETLINKS,
                   (MPARAM)psv->hwndTreeFrame,      // left
                   (MPARAM)psv->hwndFilesFrame);    // right
    }

    // create the "populate" thread
    thrCreate(&psv->tiSplitPopulate,
              fntSplitPopulate,
              &psv->tidSplitPopulate,
              "SplitPopulate",
              THRF_PMMSGQUEUE | THRF_WAIT_EXPLICIT,
                        // we MUST wait until the thread
                        // is ready; populate posts event
                        // sem when it has created its obj wnd
              (ULONG)psv);
    // this will wait until the object window has been created

    return mrc;
}

/*
 *@@ fdrCleanupSplitView:
 *
 *      Does NOT free psv because we can't know how it was
 *      allocated.
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 */

VOID fdrCleanupSplitView(PFDRSPLITVIEW psv)
{
    // stop threads; we crash if we exit
    // before these are stopped
    WinPostMsg(psv->hwndSplitPopulate,
               WM_QUIT,
               0,
               0);

    psv->tiSplitPopulate.fExit = TRUE;
    DosSleep(0);
    while (psv->tidSplitPopulate)
        winhSleep(50);

    // prevent dialog updates
    psv->fFileDlgReady = FALSE;
    fdrClearContainer(psv->hwndTreeCnr,
                      &psv->llTreeObjectsInserted);
    fdrClearContainer(psv->hwndFilesCnr,
                      &psv->llFileObjectsInserted);

    if (psv->pRootFolder)
        _wpUnlockObject(psv->pRootFolder);

    // clean up
    if (psv->hwndSplitWindow)
        WinDestroyWindow(psv->hwndSplitWindow);

    if (psv->hwndTreeFrame)
        WinDestroyWindow(psv->hwndTreeFrame);
    if (psv->hwndFilesFrame)
        WinDestroyWindow(psv->hwndFilesFrame);
    if (psv->hwndMainFrame)
        WinDestroyWindow(psv->hwndMainFrame);
}

/*
 *@@ fnwpSplitController:
 *
 */

MRESULT EXPENTRY fnwpSplitController(HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      we get PFDRSPLITVIEW in mp1.
         */

        case WM_CREATE:
        {
            PFDRSPLITVIEW psv = (PFDRSPLITVIEW)mp1;
            WinSetWindowPtr(hwndClient, QWL_USER, mp1);

            mrc = fdrSetupSplitView(hwndClient,
                                    TRUE,       // multiple selections
                                    psv);
        }
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *
         */

        case WM_WINDOWPOSCHANGED:
        {
            // this msg is passed two SWP structs:
            // one for the old, one for the new data
            // (from PM docs)
            PSWP pswpNew = PVOIDFROMMP(mp1);
            // PSWP pswpOld = pswpNew + 1;

            // resizing?
            if (pswpNew->fl & SWP_SIZE)
            {
                PFDRSPLITVIEW  psv;
                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    // adjust size of "split window",
                    // which will rearrange all the linked
                    // windows (comctl.c)
                    WinSetWindowPos(psv->hwndSplitWindow,
                                    HWND_TOP,
                                    0,
                                    0,
                                    pswpNew->cx,
                                    pswpNew->cy,
                                    SWP_SIZE);
                }
            }

            // return default NULL
        }
        break;

        /*
         * WM_MINMAXFRAME:
         *      when minimizing, we hide the "split window",
         *      because otherwise the child dialogs will
         *      display garbage
         */

        case WM_MINMAXFRAME:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                PSWP pswp = (PSWP)mp1;
                if (pswp->fl & SWP_MINIMIZE)
                    WinShowWindow(psv->hwndSplitWindow, FALSE);
                else if (pswp->fl & SWP_RESTORE)
                    WinShowWindow(psv->hwndSplitWindow, TRUE);
            }
        }
        break;

        /*
         *@@ FM_FILLFOLDER:
         *      posted to the main control from the tree
         *      frame to fill the dialog when a new folder
         *      has been selected in the tree.
         *
         *      Use fdrPostFillFolder for posting this
         *      message, which is type-safe.
         *
         *      This automatically offloads populate
         *      to fntSplitPopulate, which will then post
         *      a bunch of messages back to us so we
         *      can update the dialog properly.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  ULONG mp2: dialog flags.
         *
         *      mp2 can be any combination of the following:
         *
         *      --  If FFL_FOLDERSONLY is set, this operates
         *          in "folders only" mode. We will then
         *          populate the folder with subfolders only
         *          and expand the folder on the left. The
         *          files list is not changed.
         *
         *          If the flag is not set, the folder is
         *          fully populated and the files list is
         *          updated as well.
         *
         *      --  If FFL_SCROLLTO is set, we will scroll
         *          the drives tree so that the given record
         *          becomes visible.
         *
         *      --  If FFL_EXPAND is set, we will also expand
         *          the record in the drives tree after
         *          populate and run "add first child" for
         *          each subrecord that was inserted.
         *
         *      --  If FFL_SETBACKGROUND is set, we will
         *          revamp the files container to use the
         *          view settings of the given folder.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_FILLFOLDER:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;

                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("FM_FILLFOLDER %s", prec->pszIcon));
                #endif

                DumpFlags((ULONG)mp2);

                if (!((ULONG)mp2 & FFL_FOLDERSONLY))
                {
                    // not folders-only: then we need to
                    // refresh the files list

                    WPFolder    *pFolder;

                    fdrClearContainer(psv->hwndFilesCnr,
                                      &psv->llFileObjectsInserted);

                    // change files container background NOW
                    // to give user immediate feedback
                    if (    ((ULONG)mp2 & FFL_SETBACKGROUND)
                         && (pFolder = fdrGetFSFromRecord(prec,
                                                          TRUE)) // folders only
                       )
                    {
                        SetCnrLayout(psv->hwndFilesCnr,
                                     pFolder,
                                     OPEN_CONTENTS);
                    }
                }

                // post FM2_POPULATE
                fdrSplitPopulate(psv,
                                 prec,
                                 (ULONG)mp2);
            }
        }
        break;

        /*
         *@@ FM_POPULATED_FILLTREE:
         *      posted by fntSplitPopulate after populate has been
         *      done for a folder. This gets posted in any case,
         *      if the folder was populated in folders-only mode
         *      or not.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  ULONG mp2: FFL_* flags for whether to
         *          expand.
         *
         *      This then calls fdrInsertContents to insert
         *      the subrecords into the tree. If FFL_EXPAND
         *      was set, the tree is expanded, and we pass
         *      the controller window handle to fdrInsertContents
         *      so that we can "add first child" messages back.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_FILLTREE:
        {
            PFDRSPLITVIEW  psv;
            PMINIRECORDCORE prec;

            #ifdef DEBUG_POPULATESPLITVIEW
                _PmpfF(("FM_POPULATED_FILLTREE %s",
                        mp1
                            ? ((PMINIRECORDCORE)mp1)->pszIcon
                            : "NULL"));
            #endif

            if (    (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                 && (prec = (PMINIRECORDCORE)mp1)
               )
            {
                WPFolder    *pFolder = fdrGetFSFromRecord(mp1, TRUE);
                PLISTNODE   pNode;
                HWND        hwndAddFirstChild = NULLHANDLE;

                if ((ULONG)mp2 & FFL_EXPAND)
                {
                    BOOL        fOld = psv->fFileDlgReady;
                    // stop control notifications from messing with this
                    psv->fFileDlgReady = FALSE;
                    cnrhExpandFromRoot(psv->hwndTreeCnr,
                                       (PRECORDCORE)prec);
                    // then fire CM_ADDFIRSTCHILD too
                    hwndAddFirstChild = psv->hwndSplitPopulate;

                    // re-enable control notifications
                    psv->fFileDlgReady = fOld;
                }

                // insert subfolders into tree on the left
                fdrInsertContents(pFolder,
                                  psv->hwndTreeCnr,
                                  (PMINIRECORDCORE)mp1,
                                  INSERT_FOLDERSANDDISKS,
                                  hwndAddFirstChild,
                                  NULL,       // file mask
                                  &psv->llTreeObjectsInserted);
            }
        }
        break;

        /*
         *@@ FM_POPULATED_SCROLLTO:
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) that was populated
         *          and should now be scrolled to.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_SCROLLTO:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                BOOL    fOld = psv->fFileDlgReady;
                ULONG   ul;

                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("FM_POPULATED_SCROLLTO %s",
                            mp1
                                ? ((PMINIRECORDCORE)mp1)->pszIcon
                                : "NULL"));
                #endif

                // stop control notifications from messing with this
                psv->fFileDlgReady = FALSE;

                ul = cnrhScrollToRecord(psv->hwndTreeCnr,
                                        (PRECORDCORE)mp1,
                                        CMA_ICON | CMA_TEXT | CMA_TREEICON,
                                        TRUE);       // keep parent
                cnrhSelectRecord(psv->hwndTreeCnr,
                                 (PRECORDCORE)mp1,
                                 TRUE);
                if (ul && ul != 3)
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                            "Error: cnrhScrollToRecord returned %d", ul);

                // re-enable control notifications
                psv->fFileDlgReady = fOld;
            }
        }
        break;

        /*
         *@@ FM_POPULATED_FILLFILES:
         *      posted by fntSplitPopulate after populate has been
         *      done for the newly selected folder, if this
         *      was not in folders-only mode. We must then fill
         *      the right half of the dialog with all the objects.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  WPFolder* mp2: folder that was populated
         *          for that record.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_FILLFILES:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {

                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("FM_POPULATED_FILLFILES %s",
                            mp1
                                ? ((PMINIRECORDCORE)mp1)->pszIcon
                                : "NULL"));
                #endif

                if ((mp1) && (mp2))
                {
                    WPFolder    *pFolder = (WPFolder*)mp2;
                    CHAR        szPathName[2*CCHMAXPATH];

                    // insert all contents into list on the right
                    fdrInsertContents(pFolder,
                                      psv->hwndFilesCnr,
                                      NULL,        // parent
                                      INSERT_ALL,
                                      NULLHANDLE,  // no add first child
                                      NULL,        // no file mask
                                      &psv->llFileObjectsInserted);

                    // now, if this is for the files cnr, we must make
                    // sure that the container owner (the parent frame)
                    // has been subclassed by us... we do this AFTER
                    // the WPS subclasses the owner, which happens during
                    // record insertion

                    if (!psv->fFilesFrameSubclassed)
                    {
                        // not subclassed yet:
                        // subclass now

                        psv->psfvFiles
                            = fdrCreateSFV(psv->hwndFilesFrame,
                                           psv->hwndFilesCnr,
                                           QWL_USER,
                                           pFolder,
                                           pFolder);
                        psv->psfvFiles->pfnwpOriginal
                            = WinSubclassWindow(psv->hwndFilesFrame,
                                                fnwpSubclassedFilesFrame);
                        psv->fFilesFrameSubclassed = TRUE;
                    }
                    else
                    {
                        // already subclassed:
                        // update the folder pointers in the SFV
                        psv->psfvFiles->somSelf = pFolder;
                        psv->psfvFiles->pRealObject = pFolder;
                    }
                }
            }
        }
        break;

        /*
         * CM_UPDATEPOINTER:
         *      posted when threads exit etc. to update
         *      the current pointer.
         */

        case FM_UPDATEPOINTER:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                WinSetPointer(HWND_DESKTOP,
                              fdrSplitQueryPointer(psv));
            }
        }
        break;

        /*
         * WM_CHAR:
         *
         */

        case WM_CHAR:
        {
            USHORT usFlags    = SHORT1FROMMP(mp1);
            UCHAR  ucScanCode = CHAR4FROMMP(mp1);
            USHORT usvk       = SHORT2FROMMP(mp2);
            PFDRSPLITVIEW  psv;

            if (    (usFlags & KC_VIRTUALKEY)
                 && (usvk == VK_TAB)
                 && (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
               )
            {
                // process only key-down messages
                if  ((usFlags & KC_KEYUP) == 0)
                {
                    HWND hwndFocus = WinQueryFocus(HWND_DESKTOP);
                    if (hwndFocus == psv->hwndTreeCnr)
                        hwndFocus = psv->hwndFilesCnr;
                    else
                        hwndFocus = psv->hwndTreeCnr;
                    WinSetFocus(HWND_DESKTOP, hwndFocus);
                }

                mrc = (MRESULT)TRUE;
            }
            else
                mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
        }
        break;

        /*
         * WM_CLOSE:
         *      clean up
         */

        case WM_CLOSE:
        {
            PFDRSPLITVIEW  psv;
            #ifdef DEBUG_POPULATESPLITVIEW
                _PmpfF(("WM_CLOSE"));
            #endif

            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                // clear all containers, stop populate thread etc.
                fdrCleanupSplitView(psv);

                // destroy the frame window (which destroys us too)
                WinDestroyWindow(psv->hwndMainFrame);
            }

            // return default NULL
        }
        break;

        default:
            mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
    }

    return mrc;
}

/* ******************************************************************
 *
 *   Left tree frame and client
 *
 ********************************************************************/

/*
 *@@ TreeFrameControl:
 *      implementation for WM_CONTROL for FID_CLIENT
 *      in fnwpSubclassedTreeFrame.
 *
 *      Set *pfCallDefault to TRUE if you want the
 *      parent window proc to be called.
 *
 *@@added V0.9.21 (2002-08-26) [umoeller]
 */

MRESULT TreeFrameControl(HWND hwndFrame,
                         MPARAM mp1,
                         MPARAM mp2,
                         PBOOL pfCallDefault)
{
    MRESULT mrc = 0;
    HWND                hwndMainControl;
    PFDRSPLITVIEW       psv;
    PMINIRECORDCORE     prec;

    switch (SHORT2FROMMP(mp1))
    {
        /*
         * CN_EMPHASIS:
         *      selection changed:
         */

        case CN_EMPHASIS:
        {
            PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;

            if (    (pnre->pRecord)
                 && (pnre->fEmphasisMask & CRA_SELECTED)
                 && (prec = (PMINIRECORDCORE)pnre->pRecord)
                 && (prec->flRecordAttr & CRA_SELECTED)
                 && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 // notifications not disabled?
                 && (psv->fFileDlgReady)
               )
            {
                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("CN_EMPHASIS %s",
                        prec->pszIcon));
                #endif

                // record changed?
                if (prec != psv->precFolderContentsShowing)
                {
                    // then go refresh the files container
                    fdrPostFillFolder(psv,
                                      prec,
                                      FFL_SETBACKGROUND);
                }

                if (psv->hwndStatusBar)
                {
                    #ifdef DEBUG_POPULATESPLITVIEW
                        _Pmpf(( "CN_EMPHASIS: posting STBM_UPDATESTATUSBAR to hwnd %lX",
                                psfv->hwndStatusBar ));
                    #endif

                    // have the status bar updated and make
                    // sure the status bar retrieves its info
                    // from the _left_ cnr
                    WinPostMsg(psv->hwndStatusBar,
                               STBM_UPDATESTATUSBAR,
                               (MPARAM)psv->hwndTreeCnr,
                               MPNULL);
                }
            }
        }
        break;

        /*
         * CN_EXPANDTREE:
         *      user clicked on "+" sign next to
         *      tree item; expand that, but start
         *      "add first child" thread again
         */

        case CN_EXPANDTREE:
            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 // notifications not disabled?
                 && (psv->fFileDlgReady)
                 && (prec = (PMINIRECORDCORE)mp2)
               )
            {
                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("CN_EXPANDTREE %s",
                        prec->pszIcon));
                #endif

                fdrPostFillFolder(psv,
                                  prec,
                                  FFL_FOLDERSONLY | FFL_EXPAND);

                // and call default because xfolder
                // handles auto-scroll
                *pfCallDefault = TRUE;
            }
        break;

        /*
         * CN_ENTER:
         *      intercept this so that we won't open
         *      a folder view.
         *
         *      Before this, we should have gotten
         *      CN_EMPHASIS so the files list has
         *      been updated already.
         *
         *      Instead, check whether the record has
         *      been expanded or collapsed and do
         *      the reverse.
         */

        case CN_ENTER:
        {
            PNOTIFYRECORDENTER pnre;

            if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                 && (prec = (PMINIRECORDCORE)pnre->pRecord)
                            // can be null for whitespace!
               )
            {
                ULONG ulmsg = CM_EXPANDTREE;
                if (prec->flRecordAttr & CRA_EXPANDED)
                    ulmsg = CM_COLLAPSETREE;

                WinPostMsg(pnre->hwndCnr,
                           ulmsg,
                           (MPARAM)prec,
                           0);
            }
        }
        break;

        /*
         * CN_CONTEXTMENU:
         *      we need to intercept this for context menus
         *      on whitespace, because the WPS won't do it.
         *      We pass all other cases on because the WPS
         *      does do things correctly for object menus.
         */

        case CN_CONTEXTMENU:
        {
            *pfCallDefault = TRUE;

            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 && (!mp2)      // whitespace:
                    // display the menu for the root folder
               )
            {
                POINTL  ptl;
                WinQueryPointerPos(HWND_DESKTOP, &ptl);
                // convert to cnr coordinates
                WinMapWindowPoints(HWND_DESKTOP,        // from
                                   psv->hwndTreeCnr,   // to
                                   &ptl,
                                   1);
                _wpDisplayMenu(psv->pRootFolder,
                               psv->hwndTreeFrame, // owner
                               psv->hwndTreeCnr,   // parent
                               &ptl,
                               MENU_OPENVIEWPOPUP,
                               0);

                *pfCallDefault = FALSE;
            }
        }
        break;

        default:
            *pfCallDefault = TRUE;
    }

    return mrc;
}

/*
 *@@ fnwpSubclassedTreeFrame:
 *      subclassed frame window on the right for the
 *      "Files" container. This has the files cnr
 *      as its FID_CLIENT.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 */

MRESULT EXPENTRY fnwpSubclassedTreeFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    BOOL                fCallDefault = FALSE;
    PSUBCLFOLDERVIEW    psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);
    HWND                hwndMainControl;
    PFDRSPLITVIEW       psv;
    PMINIRECORDCORE     prec;

    switch (msg)
    {
        case WM_CONTROL:
            if (SHORT1FROMMP(mp1) == FID_CLIENT)     // that's the container
                mrc = TreeFrameControl(hwndFrame,
                                       mp1,
                                       mp2,
                                       &fCallDefault);
            else
                fCallDefault = TRUE;
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        /*
         * WM_QUERYOBJECTPTR:
         *      we receive this message from the WPS somewhere
         *      when it tries to process menu items for the
         *      whitespace context menu. I guess normally this
         *      message is taken care of when a frame is registered
         *      as a folder view, but this frame is not. So we must
         *      answer by returning the folder that this frame
         *      represents.
         *
         *      Answering this message will enable all WPS whitespace
         *      magic: both displaying the whitespace context menu
         *      and making messages work for the whitespace menu items.
         */

        case WM_QUERYOBJECTPTR:
            _PmpfF(("WM_QUERYOBJECTPTR"));
            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
               )
                return OBJECT_FROM_PREC(psv->precFolderContentsShowing);
        break;

        /*
         * WM_CONTROLPOINTER:
         *      show wait pointer if we're busy.
         */

        case WM_CONTROLPOINTER:
        {
            hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            psv = WinQueryWindowPtr(hwndMainControl, QWL_USER);

            mrc = (MPARAM)fdrSplitQueryPointer(psv);
        }
        break;

        default:
            fCallDefault = TRUE;
    }

    if (fCallDefault)
        mrc = fdrProcessFolderMsgs(hwndFrame,
                                   msg,
                                   mp1,
                                   mp2,
                                   psfv,
                                   psfv->pfnwpOriginal);

    return mrc;
}

/* ******************************************************************
 *
 *   Right files frame and client
 *
 ********************************************************************/

/*
 *@@ FilesFrameControl:
 *      implementation for WM_CONTROL for FID_CLIENT
 *      in fnwpSubclassedFilesFrame.
 *
 *      Set *pfCallDefault to TRUE if you want the
 *      parent window proc to be called.
 *
 *@@added V0.9.21 (2002-08-26) [umoeller]
 */

MRESULT FilesFrameControl(HWND hwndFrame,
                          MPARAM mp1,
                          MPARAM mp2,
                          PBOOL pfCallDefault)
{
    MRESULT mrc = 0;
    HWND                hwndMainControl;
    PFDRSPLITVIEW       psv;

    switch (SHORT2FROMMP(mp1))
    {
        /*
         * CN_EMPHASIS:
         *      selection changed: refresh
         *      the status bar.
         */

        case CN_EMPHASIS:
        {
            PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
            PMINIRECORDCORE prec;

            if (    // (pnre->pRecord)
                 // && (pnre->fEmphasisMask & CRA_SELECTED)
                 // && (prec = (PMINIRECORDCORE)pnre->pRecord)
                 // && (prec->flRecordAttr & CRA_SELECTED)
                    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 // notifications not disabled?
                 && (psv->fFileDlgReady)
               )
            {
                if (psv->hwndStatusBar)
                {
                    #ifdef DEBUG_POPULATESPLITVIEW
                        _Pmpf(( "CN_EMPHASIS: posting STBM_UPDATESTATUSBAR to hwnd %lX",
                                psfv->hwndStatusBar ));
                    #endif

                    // have the status bar updated and make
                    // sure the status bar retrieves its info
                    // from the _left_ cnr
                    WinPostMsg(psv->hwndStatusBar,
                               STBM_UPDATESTATUSBAR,
                               (MPARAM)psv->hwndFilesCnr,
                               MPNULL);
                }
            }
        }
        break;

        /*
         * CN_ENTER:
         *      double-click on tree record: intercept
         *      folders so we can influence the tree
         *      view on the right.
         */

        case CN_ENTER:
        {
            PNOTIFYRECORDENTER pnre;
            PMINIRECORDCORE prec;
            WPObject *pobj;

            if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                 && (prec = (PMINIRECORDCORE)pnre->pRecord)
                            // can be null for whitespace!
                 && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 && (pobj = fdrGetFSFromRecord(prec,
                                               TRUE))       // folders only:
               )
            {
                // double click on folder:

            }
            else
                *pfCallDefault = TRUE;
        }
        break;

        /*
         * CN_CONTEXTMENU:
         *      we need to intercept this for context menus
         *      on whitespace, because the WPS won't do it.
         *      We pass all other cases on because the WPS
         *      does do things correctly for object menus.
         */

        case CN_CONTEXTMENU:
        {
            WPFolder *pCurrent;

            *pfCallDefault = TRUE;

            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                    // display the menu for the folder that is
                    // currently selected on the right;
                    // store the record core in our psv
                 && (!(psv->precContextMenu = (PMINIRECORDCORE)mp2))
                 && (pCurrent = OBJECT_FROM_PREC(psv->precFolderContentsShowing))
               )
            {
                POINTL  ptl;
                WinQueryPointerPos(HWND_DESKTOP, &ptl);
                // convert to cnr coordinates
                WinMapWindowPoints(HWND_DESKTOP,        // from
                                   psv->hwndFilesCnr,   // to
                                   &ptl,
                                   1);
                _wpDisplayMenu(pCurrent,
                               psv->hwndFilesFrame, // owner
                               psv->hwndFilesCnr,   // parent
                               &ptl,
                               MENU_OPENVIEWPOPUP,
                               0);

                *pfCallDefault = FALSE;
            }
        }
        break;

        default:
            *pfCallDefault = TRUE;
    }

    return mrc;
}

/*
 *@@ fnwpSubclassedFilesFrame:
 *      subclassed frame window on the right for the
 *      "Files" container. This has the tree cnr
 *      as its FID_CLIENT.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 */

static MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    BOOL                fCallDefault = FALSE;
    PSUBCLFOLDERVIEW    psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);
    HWND                hwndMainControl;
    PFDRSPLITVIEW       psv;

    switch (msg)
    {
        case WM_CONTROL:
        {
            if (SHORT1FROMMP(mp1) == FID_CLIENT)     // that's the container
                mrc = FilesFrameControl(hwndFrame,
                                        mp1,
                                        mp2,
                                        &fCallDefault);
            else
                fCallDefault = TRUE;
        }
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame,
                                                     QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        /*
         * WM_QUERYOBJECTPTR:
         *      we receive this message from the WPS somewhere
         *      when it tries to process menu items for the
         *      whitespace context menu. I guess normally this
         *      message is taken care of when a frame is registered
         *      as a folder view, but this frame is not. So we must
         *      answer by returning the folder that this frame
         *      represents.
         *
         *      Answering this message will enable all WPS whitespace
         *      magic: both displaying the whitespace context menu
         *      and making messages work for the whitespace menu items.
         */

        case WM_QUERYOBJECTPTR:
            _PmpfF(("WM_QUERYOBJECTPTR"));
            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
               )
                return OBJECT_FROM_PREC(psv->precFolderContentsShowing);
        break;

        /*
         * WM_CONTROLPOINTER:
         *      show wait pointer if we're busy.
         */

        case WM_CONTROLPOINTER:
            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
               )
                mrc = (MPARAM)fdrSplitQueryPointer(psv);
        break;

        default:
            fCallDefault = TRUE;
    }

    if (fCallDefault)
        mrc = fdrProcessFolderMsgs(hwndFrame,
                                   msg,
                                   mp1,
                                   mp2,
                                   psfv,
                                   psfv->pfnwpOriginal);

    return mrc;
}

/* ******************************************************************
 *
 *   Folder split view
 *
 ********************************************************************/

/*
 *@@ SPLITVIEWPOS:
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 */

typedef struct _SPLITVIEWPOS
{
    LONG        x,
                y,
                cx,
                cy;

    LONG        lSplitBarPos;

} SPLITVIEWPOS, *PSPLITVIEWPOS;

/*
 *@@ SPLITVIEWDATA:
 *
 */

typedef struct _SPLITVIEWDATA
{
    USHORT          cb;

    USEITEM         ui;
    VIEWITEM        vi;

    CHAR            szFolderPosKey[10];

    FDRSPLITVIEW    sv;         // pRootFolder == somSelf

} SPLITVIEWDATA, *PSPLITVIEWDATA;

/*
 *@@ fnwpSplitViewFrame:
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 */

MRESULT EXPENTRY fnwpSplitViewFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_SYSCOMMAND:
         *
         */

        case WM_SYSCOMMAND:
        {
            PSPLITVIEWDATA psvd;
            if (    (SHORT1FROMMP(mp1) == SC_CLOSE)
                 && (psvd = (PSPLITVIEWDATA)WinQueryWindowPtr(hwndFrame,
                                                              QWL_USER))
               )
            {
                // save window position
                SWP swp;
                SPLITVIEWPOS pos;
                WinQueryWindowPos(hwndFrame,
                                  &swp);
                pos.x = swp.x;
                pos.y = swp.y;
                pos.cx = swp.cx;
                pos.cy = swp.cy;

                pos.lSplitBarPos = ctlQuerySplitPos(psvd->sv.hwndSplitWindow);

                PrfWriteProfileData(HINI_USER,
                                    (PSZ)INIAPP_FDRSPLITVIEWPOS,
                                    psvd->szFolderPosKey,
                                    &pos,
                                    sizeof(pos));
            }

            mrc = G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
        }
        break;

        /*
         * WM_QUERYFRAMECTLCOUNT:
         *      this gets sent to the frame when PM wants to find out
         *      how many frame controls the frame has. According to what
         *      we return here, SWP structures are allocated for WM_FORMATFRAME.
         *      We call the "parent" window proc and add one for the status bar.
         */

        case WM_QUERYFRAMECTLCOUNT:
        {
            // query the standard frame controls count
            ULONG ulCount = (ULONG)G_pfnFrameProc(hwndFrame, msg, mp1, mp2);

            PSPLITVIEWDATA psvd;
            if (    (psvd = (PSPLITVIEWDATA)WinQueryWindowPtr(hwndFrame,
                                                              QWL_USER))
                 && (psvd->sv.hwndStatusBar)
               )
                ulCount++;

            mrc = (MRESULT)ulCount;
        }
        break;

        /*
         * WM_FORMATFRAME:
         *    this message is sent to a frame window to calculate the sizes
         *    and positions of all of the frame controls and the client window.
         *
         *    Parameters:
         *          mp1     PSWP    pswp        structure array; for each frame
         *                                      control which was announced with
         *                                      WM_QUERYFRAMECTLCOUNT, PM has
         *                                      allocated one SWP structure.
         *          mp2     PRECTL  pprectl     pointer to client window
         *                                      rectangle of the frame
         *          returns USHORT  ccount      count of the number of SWP
         *                                      arrays returned
         *
         *    It is the responsibility of this message code to set up the
         *    SWP structures in the given array to position the frame controls.
         *    We call the "parent" window proc and then set up our status bar.
         */

        case WM_FORMATFRAME:
        {
            //  query the number of standard frame controls
            ULONG ulCount = (ULONG)G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
            PSPLITVIEWDATA psvd;
            if (    (psvd = (PSPLITVIEWDATA)WinQueryWindowPtr(hwndFrame,
                                                              QWL_USER))
                 && (psvd->sv.hwndStatusBar)
               )
            {
                // we have a status bar:
                // format the frame
                fdrFormatFrame(hwndFrame,
                               psvd->sv.hwndStatusBar,
                               mp1,
                               ulCount,
                               NULL);

                // increment the number of frame controls
                // to include our status bar
                ulCount++;
            }

            mrc = (MRESULT)ulCount;
        }
        break;

        /*
         * WM_CALCFRAMERECT:
         *     this message occurs when an application uses the
         *     WinCalcFrameRect function.
         *
         *     Parameters:
         *          mp1     PRECTL  pRect      rectangle structure
         *          mp2     USHORT  usFrame    frame indicator
         *          returns BOOL    rc         rectangle-calculated indicator
         */

        case WM_CALCFRAMERECT:
        {
            PSPLITVIEWDATA psvd;
            mrc = G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
            if (    (psvd = (PSPLITVIEWDATA)WinQueryWindowPtr(hwndFrame,
                                                              QWL_USER))
                 && (psvd->sv.hwndStatusBar)
               )
            {
                fdrCalcFrameRect(mp1, mp2);
            }
        }
        break;

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
        {
            PSPLITVIEWDATA psvd;

            if (psvd = (PSPLITVIEWDATA)WinQueryWindowPtr(hwndFrame,
                                                         QWL_USER))
            {
                #ifdef DEBUG_POPULATESPLITVIEW
                    _PmpfF(("PSPLITVIEWDATA is 0x%lX", psvd));
                #endif

                _wpDeleteFromObjUseList(psvd->sv.pRootFolder,  // somSelf
                                        &psvd->ui);

                if (psvd->sv.hwndStatusBar)
                    WinDestroyWindow(psvd->sv.hwndStatusBar);

                _wpFreeMem(psvd->sv.pRootFolder,  // somSelf,
                           (PBYTE)psvd);
            }

            mrc = G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
        }
        break;

        default:
            mrc = G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ fdrCreateSplitView:
 *      creates a frame window with a split window and
 *      does the usual "register view and pass a zillion
 *      QWL_USER pointers everywhere" stuff.
 *
 *      Returns the frame window or NULLHANDLE on errors.
 *
 */

HWND fdrCreateSplitView(WPFolder *somSelf,
                        ULONG ulView)
{
    static  s_fRegistered = FALSE;

    HWND    hwndReturn = NULLHANDLE;

    TRY_LOUD(excpt1)
    {
        PSPLITVIEWDATA  psvd;
        ULONG           rc;

        if (!s_fRegistered)
        {
            s_fRegistered = TRUE;

            WinRegisterClass(winhMyAnchorBlock(),
                             (PSZ)WC_SPLITVIEWCLIENT,
                             fnwpSplitController,
                             CS_SIZEREDRAW,
                             sizeof(PSPLITVIEWDATA));
        }

        // allocate our SPLITVIEWDATA, which contains the
        // FDRSPLITVIEW for the split view engine, plus
        // the useitem for this view
        if (psvd = (PSPLITVIEWDATA)_wpAllocMem(somSelf,
                                               sizeof(SPLITVIEWDATA),
                                               &rc))
        {
            SPLITVIEWPOS pos;
            ULONG   cbPos;
            ULONG   flFrame = FCF_NOBYTEALIGN
                                  | FCF_TITLEBAR
                                  | FCF_SYSMENU
                                  | FCF_SIZEBORDER
                                  | FCF_AUTOICON;
            ULONG   ulButton = _wpQueryButtonAppearance(somSelf);

            ZERO(psvd);

            if (ulButton == DEFAULTBUTTON)
                ulButton = PrfQueryProfileInt(HINI_USER,
                                              "PM_ControlPanel",
                                              "MinButtonType",
                                              HIDEBUTTON);

            if (ulButton == HIDEBUTTON)
                flFrame |= FCF_HIDEMAX;     // hide and maximize
            else
                flFrame |= FCF_MINMAX;      // minimize and maximize

            psvd->cb = sizeof(SPLITVIEWDATA);

            psvd->sv.pRootFolder = somSelf;

            // try to restore window position, if present;
            // we put these in a separate XWorkplace app
            // because we're using a special format to
            // allow for saving the split position
            sprintf(psvd->szFolderPosKey,
                    "%lX",
                    _wpQueryHandle(somSelf));
            cbPos = sizeof(pos);
            if (    (!(PrfQueryProfileData(HINI_USER,
                                           (PSZ)INIAPP_FDRSPLITVIEWPOS,
                                           psvd->szFolderPosKey,
                                           &pos,
                                           &cbPos)))
                 || (cbPos != sizeof(SPLITVIEWPOS))
               )
            {
                // no position stored yet:
                pos.x = (winhQueryScreenCX() - 600) / 2;
                pos.y = (winhQueryScreenCY() - 400) / 2;
                pos.cx = 600;
                pos.cy = 400;
                psvd->sv.lSplitBarPos = 30;
            }
            else
                psvd->sv.lSplitBarPos = pos.lSplitBarPos;

            // create the frame and the client;
            // the client gets psvd in mp1 with WM_CREATE
            // and creates the split window and everything else
            if (psvd->sv.hwndMainFrame = winhCreateStdWindow(HWND_DESKTOP, // parent
                                                             NULL,         // pswp
                                                             flFrame,
                                                             WS_ANIMATE,   // frame style, not yet visible
                                                             "Split view",
                                                             0,            // resids
                                                             WC_SPLITVIEWCLIENT,
                                                             WS_VISIBLE,   // client style
                                                             0,            // frame ID
                                                             &psvd->sv,
                                                             &psvd->sv.hwndMainControl))
            {
                PMINIRECORDCORE pRootRec;
                POINTL  ptlIcon = {0, 0};

                if (stbFolderWantsStatusBars(somSelf))      // V0.9.19 (2002-04-17) [umoeller]
                {
                    psvd->sv.hwndStatusBar = stbCreateBar(somSelf,
                                                          somSelf,
                                                          psvd->sv.hwndMainFrame,
                                                          psvd->sv.hwndTreeCnr);

                }

                // insert somSelf as the root of the tree
                pRootRec = _wpCnrInsertObject(psvd->sv.pRootFolder,
                                              psvd->sv.hwndTreeCnr,
                                              &ptlIcon,
                                              NULL,       // parent record
                                              NULL);      // RECORDINSERT

                // _wpCnrInsertObject subclasses the container owner,
                // so subclass this with the XFolder subclass
                // proc again; otherwise the new menu items
                // won't work
                psvd->sv.psfvTree = fdrCreateSFV(psvd->sv.hwndTreeFrame,
                                                 psvd->sv.hwndTreeCnr,
                                                 QWL_USER,
                                                 psvd->sv.pRootFolder,
                                                 psvd->sv.pRootFolder);
                psvd->sv.psfvTree->pfnwpOriginal = WinSubclassWindow(psvd->sv.hwndTreeFrame,
                                                                     fnwpSubclassedTreeFrame);

                // and populate this once we're running
                fdrPostFillFolder(&psvd->sv,
                                  pRootRec,
                                  // full populate, and expand tree on the left,
                                  // and set background
                                  FFL_SETBACKGROUND | FFL_EXPAND);

                // view-specific stuff

                WinSetWindowPtr(psvd->sv.hwndMainFrame,
                                QWL_USER,
                                psvd);
                G_pfnFrameProc = WinSubclassWindow(psvd->sv.hwndMainFrame,
                                                   fnwpSplitViewFrame);

                // register the view
                cmnRegisterView(somSelf,
                                &psvd->ui,
                                ulView,
                                psvd->sv.hwndMainFrame,
                                cmnGetString(ID_XFSI_FDR_SPLITVIEW));

                // subclass the cnrs to let us paint the bitmaps
                fdrMakeCnrPaint(psvd->sv.hwndTreeCnr);
                fdrMakeCnrPaint(psvd->sv.hwndFilesCnr);

                // set tree view colors
                SetCnrLayout(psvd->sv.hwndTreeCnr,
                             somSelf,
                             OPEN_TREE);

                // now go show the damn thing
                WinSetWindowPos(psvd->sv.hwndMainFrame,
                                HWND_TOP,
                                pos.x,
                                pos.y,
                                pos.cx,
                                pos.cy,
                                SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

                // and set focus to the tree view
                WinSetFocus(HWND_DESKTOP,
                            psvd->sv.hwndTreeCnr);

                psvd->sv.fFileDlgReady = TRUE;

                hwndReturn = psvd->sv.hwndMainFrame;
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    return hwndReturn;
}
