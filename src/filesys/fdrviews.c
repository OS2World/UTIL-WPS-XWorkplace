
/*
 *@@sourcefile fdrview.c:
 *      shared code for all folder views.
 *
 *@@added V1.0.0 (2002-08-28) [umoeller]
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
#define INCL_WINSTDDRAG
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
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"
#include "xfdataf.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\fdrsplit.h"           // folder split views
#include "filesys\fdrviews.h"           // common code for folder views
#include "filesys\object.h"             // XFldObject implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private declarations
 *
 ********************************************************************/

/*
 *@@ IMAGECACHEENTRY:
 *
 *@@added V1.0.0 (2002-08-24) [umoeller]
 */

typedef struct _IMAGECACHEENTRY
{
    WPFileSystem    *pobjImage;     // a WPImageFile really

    HBITMAP         hbm;            // result from WPImageFile::wpQueryBitmapHandle

} IMAGECACHEENTRY, *PIMAGECACHEENTRY;

/*
 *@@ SUBCLCNR:
 *      QWL_USER data for containers that were
 *      subclassed to paint backgrounds.
 *
 *      This gets created and set by fdrMakeCnrPaint.
 *      The view settings are then manipulated
 *      every time the
 *
 *@@added V1.0.0 (2002-08-24) [umoeller]
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

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

HMTX        G_hmtxImages = NULLHANDLE;
LINKLIST    G_llImages;     // linked list of IMAGECACHEENTRY structs, auto-free

/* ******************************************************************
 *
 *   Image file cache
 *
 ********************************************************************/

/*
 *@@ LockImages:
 *
 */

STATIC BOOL LockImages(VOID)
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

/*
 *@@ UnlockImages:
 *
 */

STATIC VOID UnlockImages(VOID)
{
    DosReleaseMutexSem(G_hmtxImages);
}

/* ******************************************************************
 *
 *   Painting container backgrounds
 *
 ********************************************************************/

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
 *@@added V1.0.0 (2002-08-24) [umoeller]
 */

VOID DrawBitmapClipped(HPS hps,             // in: presentation space
                       HBITMAP hbm,         // in: bitmap to paint
                       PPOINTL pptlOrigin,  // in: position to paint bitmap at
                       PSIZEL pszlBitmap,   // in: bitmap size
                       PRECTL prclClip)     // in: clip (update) rectangle
{
    RECTL   rclSubBitmap;
    POINTL  ptl;

    PMPF_CNRBITMAPS(("pptlOrigin->x: %d, pptlOrigin->y: %d",
            pptlOrigin->x, pptlOrigin->y));

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
        PMPF_CNRBITMAPS(("  pathological case, returning"));

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
 *@@added V1.0.0 (2002-08-24) [umoeller]
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

                PMPF_CNRBITMAPS(("BKGND_SCALED"));
                PMPF_CNRBITMAPS(("    szlCnr.cx %d, cy %d",
                        pSubCnr->szlCnr.cx, pSubCnr->szlCnr.cy));
                PMPF_CNRBITMAPS(("    rclClip.xLeft %d, yBottom %d, xRight %d, yTop %d",
                        rclClip.xLeft, rclClip.yBottom, rclClip.xRight, rclClip.yTop));

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
 *@@added V1.0.0 (2002-08-24) [umoeller]
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
 *@@ fdrvMakeCnrPaint:
 *      subclasses the given cnr with fnwpSubclCnr
 *      to make it process CM_PAINTBACKGROUND.
 *
 *      This creates a SUBCLCNR struct and puts it
 *      into the cnr's QWL_USER.
 *
 *@@added V1.0.0 (2002-08-24) [umoeller]
 */

BOOL fdrvMakeCnrPaint(HWND hwndCnr)
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
 *@@added V1.0.0 (2002-08-24) [umoeller]
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
#ifdef __DEBUG__
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

        PMPF_CNRBITMAPS(("  --> %d (%s)", lcol, pcsz));

#endif

        lcol = WinQuerySysColor(HWND_DESKTOP,
                                lcol,
                                0);
    }

    return lcol;
}

/*
 *@@ fdrvRemoveFromImageCache:
 *      removes the given object from the image cache.
 *
 *@@added V1.0.0 (2002-08-24) [umoeller]
 */

BOOL fdrvRemoveFromImageCache(WPObject *pobjImage)
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
 *@@added V1.0.0 (2002-08-24) [umoeller]
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
                        if (hbm = _wpQueryHandleFromContents(pobjImage))
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
 *@@added V1.0.0 (2002-08-24) [umoeller]
 */

VOID fdrvSetCnrLayout(HWND hwndCnr,         // in: cnr whose colors and fonts are to be set
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

            WinShowWindow(hwndCnr, TRUE);
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

WPFileSystem* fdrvGetFSFromRecord(PMINIRECORDCORE precc,
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
 *      --  INSERT_ALL (0): we allow all objects to be inserted,
 *          even broken shadows. This is used by the split
 *          view for the files container.
 *
 *          pcszFileMask is ignored in this case.
 *
 *      --  INSERT_FILESYSTEMS (1): this inserts all WPFileSystem and
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
 *      --  INSERT_FOLDERSONLY (2): only real folders are inserted.
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
 *      --  INSERT_FOLDERSANDDISKS (3): in addition to folders
 *          (as with INSERT_FOLDERSONLY), we allow insertion
 *          of drive objects too.
 *
 *      In any case, FALSE is returned if the object matches
 *      the above, but the object has the "hidden" attribute on.
 */

BOOL fdrvIsInsertable(WPObject *pObject,
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

BOOL fdrvIsObjectInCnr(WPObject *pObject,
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
 *@@ fncbClearCnr:
 *      callback for cnrhForAllRecords, specified
 *      by fdrvClearContainer.
 *
 *@@added V1.0.0 (2002-09-09) [umoeller]
 */

STATIC ULONG XWPENTRY fncbClearCnr(HWND hwndCnr,
                                   PRECORDCORE precc,
                                   ULONG ulUnlock)      // callback parameter
{
    WPObject *pobj = OBJECT_FROM_PREC(precc);

    _wpCnrRemoveObject(pobj,
                       hwndCnr);
            // @@todo this is _very_ inefficient. Removing 1,000 records
            // can take up to a second this way. Use wpclsRemoveObjects
            // instead.

    if (ulUnlock)
        // this is the "unlock" that corresponds to the "lock"
        // that was issued on every object by wpPopulate
        _wpUnlockObject(pobj);

    return 0;
}

/*
 *@@ fdrvClearContainer:
 *      removes all objects that were inserted into the
 *      specified container and updates the USEITEM's
 *      correctly.
 *
 *      Returns the no. of records that were removed.
 *
 *      flClear can be set to any combination of the
 *      following:
 *
 *      --  Only if CLEARFL_TREEVIEW is set, we will
 *          recurse into subrecords and remove these
 *          also. This makes cleanup a lot faster for
 *          non-tree views.
 *
 *      --  If CLEARFL_UNLOCKOBJECTS is set, we will
 *          unlock every object that we remove once.
 *
 *@@changed V1.0.0 (2002-09-13) [umoeller]: added flClear
 *@@changed V1.0.0 (2002-11-23) [umoeller]: fixed broken lazy drag in progress @@fixes 225
 */

ULONG fdrvClearContainer(HWND hwndCnr,      // in: cnr to clear
                         ULONG flClear)     // in: CLEARFL_* flags
{
    ULONG       ulrc;
    PDRAGINFO   pdrgInfo;

    // disable window updates
    // for the cnr or this takes forever
    WinEnableWindowUpdate(hwndCnr, FALSE);

    // if this container is the current source of a
    // lazy drag operation, cancel the lazy drag
    // before clearing the container
    if (    (DrgQueryDragStatus() == DGS_LAZYDRAGINPROGRESS)
         && (pdrgInfo = DrgQueryDraginfoPtr(NULL))
         && (pdrgInfo->hwndSource == hwndCnr)
       )
        DrgCancelLazyDrag();

    ulrc = cnrhForAllRecords(hwndCnr,
                             // recurse only for tree view
                             (flClear & CLEARFL_TREEVIEW)
                                ? NULL
                                : (PRECORDCORE)-1,
                             fncbClearCnr,
                             // callback parameter:
                             (flClear & CLEARFL_UNLOCKOBJECTS));

    WinShowWindow(hwndCnr, TRUE);

    return ulrc;
}


