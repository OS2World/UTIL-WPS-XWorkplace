
/*
 *@@sourcefile filetypes.c:
 *      extended file types implementation code. This has
 *      both method implementations for XFldDataFile as
 *      well as notebook pages for the "Workplace Shell"
 *      object (XFldWPS).
 *
 *      This file is ALL new with V0.9.0.
 *
 *      There are several entry points into this mess:
 *
 *      --  ftypQueryAssociatedProgram gets called from
 *          XFldDataFile::wpQueryAssociatedProgram and also
 *          from XFldDataFile::wpOpen. This must return a
 *          single association according to a given view ID.
 *
 *      --  ftypModifyDataFileOpenSubmenu gets called from
 *          the XFldDataFile menu methods to hack the
 *          "Open" submenu.
 *
 *      --  ftypBuildAssocsList could be called separately
 *          to build a complete associations list for an
 *          object.
 *
 *      Function prefix for this file:
 *      --  ftyp*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\filetype.h"
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

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT           // WM_CHAR
#define INCL_WINPOINTERS
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINMENUS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSTDDRAG
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers
#include "helpers\tree.h"               // red-black binary trees

// SOM headers which don't crash with prec. header files
#include "xfwps.ih"
#include "xfdataf.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\filetype.h"           // extended file types implementation

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise
#include <wppgm.h>              // WPProgram
#include <wppgmf.h>             // WPProgramFile
#include <wpshadow.h>           // WPShadow

/* ******************************************************************
 *
 *   Additional declarations
 *
 ********************************************************************/

/*
 *@@ XWPTYPEWITHFILTERS:
 *      structure representing one entry in "XWorkplace:FileFilters"
 *      from OS2.INI. These are now cached for speed.
 *
 *      A linked list of these exists in G_llTypesWithFilters,
 *      which is built on the first call to
 *      ftypGetCachedTypesWithFilters.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

typedef struct _XWPTYPEWITHFILTERS
{
    PSZ         pszType;        // e.g. "C Code" (malloc copy)
    PSZ         pszFilters;     // e.g. "*.c\0*.h\0\0" (malloc copy)
} XWPTYPEWITHFILTERS, *PXWPTYPEWITHFILTERS;

/*
 *@@ WPSTYPEASSOCTREENODE:
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

typedef struct _WPSTYPEASSOCTREENODE
{
    TREE        Tree;
    PSZ         pszType;
    PSZ         pszObjectHandles;
    ULONG       cbObjectHandles;
} WPSTYPEASSOCTREENODE, *PWPSTYPEASSOCTREENODE;

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

LINKLIST            G_llTypesWithFilters;
                        // contains PXWPTYPEWITHFILTERS ptrs;
                        // list is auto-free
BOOL                G_fTypesWithFiltersValid = FALSE;
                        // set to TRUE if G_llTypesWithFilters has been filled

TREE                *G_WPSTypeAssocsTreeRoot = NULL;
BOOL                G_fWPSTypesValid = FALSE;

HMTX                G_hmtxAssocsCaches = NULLHANDLE;
                        // mutex protecting all the caches

/* ******************************************************************
 *
 *   Associations cache
 *
 ********************************************************************/

/*
 *@@ ftypLockCaches:
 *      locks the association caches.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

BOOL ftypLockCaches(VOID)
{
    BOOL    brc = FALSE;
    if (G_hmtxAssocsCaches == NULLHANDLE)
    {
        // first call:
        brc = (DosCreateMutexSem(NULL,
                                 &G_hmtxAssocsCaches,
                                 0,
                                 TRUE)     // lock!
                     == NO_ERROR);
        if (brc)
        {
            lstInit(&G_llTypesWithFilters,
                    TRUE);         // auto-free
            treeInit(&G_WPSTypeAssocsTreeRoot);
        }
    }
    else
        brc = (!WinRequestMutexSem(G_hmtxAssocsCaches, SEM_INDEFINITE_WAIT));

    return (brc);
}

/*
 *@@ ftypUnlockCaches:
 *      unlocks the association caches.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

VOID ftypUnlockCaches(VOID)
{
    DosReleaseMutexSem(G_hmtxAssocsCaches);
}

/*
 *@@ TraverseWPSTypes:
 *      traversal function for treeTraverse (tree.c)
 *      to add all tree nodes to a linked list so
 *      we can delete all tree nodes quickly.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

void TraverseWPSTypes(TREE *t,          // in: PWPSTYPEASSOCTREENODE
                      void *pUser)      // in: PLINKLIST to add to
{
    lstAppendItem((PLINKLIST)pUser,
                  t);
}

/*
 *@@ ftypInvalidateCaches:
 *      invalidates all the global association caches
 *      and frees all allocated memory.
 *
 *      This must ALWAYS be called when any of the data
 *      in OS2.INI related to associations (WPS or XWP)
 *      is changed. Otherwise XWP can't pick up the changes.
 *
 *      After the caches have been invalidated, the next
 *      call to ftypGetCachedTypesWithFilters or ftypFindWPSType
 *      will reinitialize the caches automatically.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

VOID ftypInvalidateCaches(VOID)
{
    if (ftypLockCaches())
    {
        LINKLIST llDelete;

        // 1) clear the XWP types with filters
        PLISTNODE pNode = lstQueryFirstNode(&G_llTypesWithFilters);
        while (pNode)
        {
            PXWPTYPEWITHFILTERS pThis = (PXWPTYPEWITHFILTERS)pNode->pItemData;
            if (pThis->pszType)
            {
                free(pThis->pszType);
                pThis->pszType = NULL;
            }
            if (pThis->pszFilters)
            {
                free(pThis->pszFilters);
                pThis->pszFilters = NULL;
            }

            pNode = pNode->pNext;
        }

        lstClear(&G_llTypesWithFilters);
                    // this frees the XWPTYPEWITHFILTERS structs themselves

        G_fTypesWithFiltersValid = FALSE;

        // 2) clear the WPS types... this is a bit strange,
        // we traverse the tree and thus add all tree nodes
        // to a temporary linked list, which we can then
        // delete. This way however we avoid rebalancing the
        // tree for each node getting deleted.
        lstInit(&llDelete, TRUE);
        treeTraverse(G_WPSTypeAssocsTreeRoot,
                     TraverseWPSTypes,
                     &llDelete,
                     1);

        // now llDelete has all the PWPSTYPEASSOCTREENODE pointers
        pNode = lstQueryFirstNode(&llDelete);
        while (pNode)
        {
            PWPSTYPEASSOCTREENODE pWPSType = (PWPSTYPEASSOCTREENODE)pNode->pItemData;
            if (pWPSType->pszType)
            {
                free(pWPSType->pszType);
                pWPSType->pszType = NULL;
            }
            if (pWPSType->pszObjectHandles)
            {
                free(pWPSType->pszObjectHandles);
                pWPSType->pszObjectHandles = NULL;
            }
            pNode = pNode->pNext;
        }
        // clear the list now.. this frees the PWPSTYPEASSOCTREENODE ptrs also
        lstClear(&llDelete);

        // reset the tree root
        treeInit(&G_WPSTypeAssocsTreeRoot);
        G_fWPSTypesValid = FALSE;

        ftypUnlockCaches();
    }
}

/*
 *@@ ftypGetCachedTypesWithFilters:
 *      returns the LINKLIST containing XWPTYPEWITHFILTERS pointers,
 *      which is built internally if this hasn't been done yet.
 *
 *      This is new with V0.9.9 to speed up getting the filters
 *      for the XWP file types so we don't have to retrieve these
 *      from OS2.INI all the time (for each single data file which
 *      is awoken during folder population).
 *
 *      Preconditions:
 *
 *      -- The caller must lock the caches before calling.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

PLINKLIST ftypGetCachedTypesWithFilters(VOID)
{
    if (!G_fTypesWithFiltersValid)
    {
        // caches have been cleared, or first call:
        // build the list in the global variable from OS2.INI...

        PSZ     pszTypesWithFiltersList = prfhQueryKeysForApp(HINI_USER,
                                                              INIAPP_XWPFILEFILTERS);
                                                                // "XWorkplace:FileFilters"
        if (pszTypesWithFiltersList)
        {
            PSZ pTypeWithFilterThis = pszTypesWithFiltersList;

            while (*pTypeWithFilterThis != 0)
            {
                // pFilterThis has the current type now
                // (e.g. "C Code");
                // get filters for that (e.g. "*.c");
                //  this is another list of null-terminated strings
                ULONG cbFiltersForTypeList = 0;
                PSZ pszFiltersForTypeList = prfhQueryProfileData(HINI_USER,
                                                                 INIAPP_XWPFILEFILTERS, // "XWorkplace:FileFilters"
                                                                 pTypeWithFilterThis,
                                                                 &cbFiltersForTypeList);
                        // this would be e.g. "*.c" now
                if (pszFiltersForTypeList)
                {
                    PXWPTYPEWITHFILTERS pNew = malloc(sizeof(XWPTYPEWITHFILTERS));
                    if (pNew)
                    {
                        pNew->pszType = strdup(pTypeWithFilterThis);
                        pNew->pszFilters = pszFiltersForTypeList;
                                        // no copy, we have malloc() already

                        lstAppendItem(&G_llTypesWithFilters, pNew);
                    }
                    else
                        // malloc failed:
                        break;
                }

                pTypeWithFilterThis += strlen(pTypeWithFilterThis) + 1;   // next type/filter
            } // end while (*pTypeWithFilterThis != 0)

            free(pszTypesWithFiltersList);
                        // we created copies of each string here

            G_fTypesWithFiltersValid = TRUE;
        }
    }

    return (&G_llTypesWithFilters);
}

/*
 *@@ CompareWPSTypes:
 *      tree node comparison func (helpers\tree.c).
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

int CompareWPSTypes(TREE *t1, TREE *t2)
{
    int i = strcmp(((PWPSTYPEASSOCTREENODE)t1)->pszType,
                   ((PWPSTYPEASSOCTREENODE)t2)->pszType);
    if (i < 0) return -1;
    if (i > 0) return +1;
    return (0);
}

/*
 *@@ CompareWPSTypeData:
 *      tree node comparison func (helpers\tree.c).
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

int CompareWPSTypeData(TREE *t1, void *pData)
{
    int i = strcmp(((PWPSTYPEASSOCTREENODE)t1)->pszType,
                   (const char*)pData);
    if (i < 0) return -1;
    if (i > 0) return +1;
    return (0);
}

/*
 *@@ ftypFindWPSType:
 *      returns the PWPSTYPEASSOCTREENODE containing the
 *      WPS association objects for the specified type.
 *
 *      This is retrieved from the internal cache, which
 *      is built if necessary.
 *
 *      Preconditions:
 *
 *      -- The caller must lock the caches before calling.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

PWPSTYPEASSOCTREENODE ftypFindWPSType(const char *pcszType)
{
    PWPSTYPEASSOCTREENODE pWPSType = NULL;

    if (!G_fWPSTypesValid)
    {
        // create a copy of the data from OS2.INI... this
        // is much faster than using the Prf* functions all
        // the time
        PSZ     pszAssocData = prfhQueryKeysForApp(HINI_USER,
                                                   WPINIAPP_ASSOCTYPE); // "PMWP_ASSOC_TYPE"
        if (pszAssocData)
        {
            PSZ pTypeThis = pszAssocData;
            while (*pTypeThis)
            {
                PWPSTYPEASSOCTREENODE pNewNode = malloc(sizeof(WPSTYPEASSOCTREENODE));
                if (pNewNode)
                {
                    pNewNode->pszType = strdup(pTypeThis);
                    pNewNode->pszObjectHandles = prfhQueryProfileData(HINI_USER,
                                                                      WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE"
                                                                      pTypeThis,
                                                                      &pNewNode->cbObjectHandles);

                    #ifdef DEBUG_ASSOCS
                        _Pmpf((__FUNCTION__ ": inserting type %s, %d bytes data", pTypeThis, pNewNode->cbObjectHandles));
                    #endif

                    // insert into binary tree
                    treeInsertNode(&G_WPSTypeAssocsTreeRoot,
                                   (TREE*)pNewNode,
                                   CompareWPSTypes,
                                   FALSE);
                }
                else
                    break;

                pTypeThis += strlen(pTypeThis) + 1;   // next type
            }

            free(pszAssocData);

            G_fWPSTypesValid = TRUE;
        }
    }

    if (G_fWPSTypesValid)
    {
        #ifdef DEBUG_ASSOCS
            _Pmpf((__FUNCTION__ ": looking for %s now...", pcszType));
        #endif

        pWPSType = treeFindEQData(&G_WPSTypeAssocsTreeRoot,
                                  (PVOID)pcszType,
                                  CompareWPSTypeData);
        #ifdef DEBUG_ASSOCS
            _Pmpf(("    got 0x%lX", pWPSType));
        #endif
    }

    return (pWPSType);
}

/* ******************************************************************
 *
 *   Extended associations helper funcs
 *
 ********************************************************************/

/*
 *@@ AppendSingleTypeUnique:
 *      appends the given type to the given list, if
 *      it's not on the list yet. Returns TRUE if the
 *      item either existed or was appended.
 *
 *      This assumes that pszNewType is free()'able. If
 *      pszNewType is already on the list, the string
 *      is freed!
 *
 *@@changed V0.9.6 (2000-11-12) [umoeller]: fixed memory leak
 */

BOOL AppendSingleTypeUnique(PLINKLIST pll,    // in: list to append to; list gets created on first call
                            PSZ pszNewType)     // in: new type to append (must be free()'able!)
{
    BOOL brc = FALSE;

    PLISTNODE pNode = lstQueryFirstNode(pll);
    while (pNode)
    {
        PSZ psz = (PSZ)pNode->pItemData;
        if (psz)
            if (strcmp(psz, pszNewType) == 0)
            {
                // matches: it's already on the list,
                // so stop
                brc = TRUE;
                // and free the string (the caller has always created a copy)
                free(pszNewType);
                break;
            }

        pNode = pNode->pNext;
    }

    if (!brc)
        // not found:
        brc = (lstAppendItem(pll, pszNewType) != NULL);

    return (brc);
}

/*
 *@@ AppendTypesFromString:
 *      this splits a standard WPS file types
 *      string (where several file types are
 *      separated by line feeds, \n) into
 *      a linked list of newly allocated PSZ's.
 *
 *      The list is not cleared, but simply appended to.
 *      The type is only added if it doesn't exist in
 *      the list yet.
 *
 *      The list should be in auto-free mode
 *      so that the strings are automatically
 *      freed upon lstClear. See lstInit for details.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

ULONG AppendTypesFromString(PSZ pszTypes, // in: types string (e.g. "C Code\nPlain text")
                            PLINKLIST pllTypes) // in/out: list of newly allocated PSZ's
                                                // with file types (e.g. "C Code", "Plain text")
{
    ULONG   ulrc = 0;
    // if we have several file types (which are then separated
    // by line feeds (\n), get the one according to ulView.
    // For example, if pszType has "C Code\nPlain text" and
    // ulView is 0x1001, get "Plain text".
    PSZ     pTypeThis = pszTypes,
            pLF = 0;

    // loop thru types list
    while (pTypeThis)
    {
        // get next line feed
        pLF = strchr(pTypeThis, '\n');
        if (pLF)
        {
            // line feed found:
            // extract type and store in list
            AppendSingleTypeUnique(pllTypes,
                                   strhSubstr(pTypeThis,
                                              pLF));
            ulrc++;
            // next type (after LF)
            pTypeThis = pLF + 1;
        }
        else
        {
            // no next line feed found:
            // store last item
            if (strlen(pTypeThis))
            {
                AppendSingleTypeUnique(pllTypes,
                                       strdup(pTypeThis));
                ulrc++;
            }
            break;
        }
    }

    return (ulrc);
}

/*
 *@@ AppendTypesForFile:
 *      this lists all extended file types which have
 *      a file filter assigned to them which matches
 *      the given object title.
 *
 *      For example, if you pass "filetype.c" to this
 *      function and "C Code" has the "*.c" filter
 *      assigned, this would add "C Code" to the given
 *      list.
 *
 *      The list is not cleared, but simply appended to.
 *      The type is only added if it doesn't exist in
 *      the list yet.
 *
 *      In order to query all associations for a given
 *      object, pass the object title to this function
 *      first (to get the associated types). Then, for
 *      all types on the list which was filled for this
 *      function, call ftypListAssocsForType with the
 *      same objects list for each call.
 *
 *      Note that this func creates _copies_ of the
 *      file types in the given list, using strdup().
 *
 *      This returns the no. of items which were added
 *      (0 if none).
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

ULONG AppendTypesForFile(const char *pcszObjectTitle,
                         PLINKLIST pllTypes)   // in/out: list of newly allocated PSZ's
                                               // with file types (e.g. "C Code", "Plain text")
{
    ULONG   ulrc = 0;
    // loop thru all extended file types which have
    // filters assigned to them to check whether the
    // filter matches the object title

    if (ftypLockCaches())
    {
        PLINKLIST pllTypesWithFilters = ftypGetCachedTypesWithFilters();

        if (pllTypesWithFilters)
        {
            PLISTNODE pNode = lstQueryFirstNode(pllTypesWithFilters);
            while (pNode)
            {
                PXWPTYPEWITHFILTERS pTypeWithFilters = (PXWPTYPEWITHFILTERS)pNode->pItemData;

                // second loop: thru all filters for this file type
                PSZ pFilterThis = pTypeWithFilters->pszFilters;
                while (*pFilterThis != 0)
                {
                    // check if this matches the data file name
                    if (strhMatchOS2(pFilterThis, pcszObjectTitle))
                    {
                        // matches:
                        // now we have:
                        // --  pszFilterThis e.g. "*.c"
                        // --  pTypeFilterThis e.g. "C Code"
                        // store the type (not the filter) in the output list
                        AppendSingleTypeUnique(pllTypes,
                                               strdup(pTypeWithFilters->pszType));

                        ulrc++;     // found something

                        break;      // this file type is done, so go for next type
                    }

                    pFilterThis += strlen(pFilterThis) + 1;   // next type/filter
                } // end while (*pFilterThis)

                pNode = pNode->pNext;
            }
        }

        ftypUnlockCaches();
    }
    return (ulrc);
}

/*
 *@@ ftypListAssocsForType:
 *      this lists all associated WPProgram or WPProgramFile
 *      objects which have been assigned to the given type.
 *
 *      For example, if "System editor" has been assigned to
 *      the "C Code" type, this would add the "System editor"
 *      program object to the given list.
 *
 *      This adds plain SOM pointers to the given list, so
 *      you better not free() those.
 *
 *      NOTE: This locks each object instantiated as a
 *      result of the call.
 *
 *      This returns the no. of objects added to the list
 *      (0 if none).
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 *@@changed V0.9.9 (2001-03-27) [umoeller]: now avoiding duplicate assocs
 */

ULONG ftypListAssocsForType(PSZ pszType0,         // in: file type (e.g. "C Code")
                            PLINKLIST pllAssocs) // in/out: list of WPProgram or WPProgramFile
                                                // objects to append to
{
    ULONG   ulrc = 0;

    if (ftypLockCaches())
    {
        ULONG   cbAssocData = 0;

        PSZ     pszType2 = pszType0,
                pszParentForType = 0;

        // outer loop for climbing up the file type parents
        do
        {
            // get associations from WPS INI data
            PWPSTYPEASSOCTREENODE pWPSType = ftypFindWPSType(pszType2);

            #ifdef DEBUG_ASSOCS
                _Pmpf((__FUNCTION__ ": got %d bytes for type %s",
                    (pWPSType) ? pWPSType->cbObjectHandles : 0,
                    pszType2));
            #endif

            if (pWPSType)
            {
                // pWPSType->pszObjectHandles now has the handles of the associated
                // objects (as decimal strings, which we'll decode now)
                PSZ     pAssoc = pWPSType->pszObjectHandles;
                if (pAssoc)
                {
                    HOBJECT hobjAssoc;
                    WPObject *pobjAssoc;

                    // now parse the handles string
                    while (*pAssoc)
                    {
                        hobjAssoc = atoi(pAssoc);
                        pobjAssoc = _wpclsQueryObject(_WPObject, hobjAssoc);
                        if (pobjAssoc)
                        {
                            // look if the object has already been added;
                            // this might happen if the same object has
                            // been defined for several types (inheritance!)
                            // V0.9.9 (2001-03-27) [umoeller]
                            if (lstNodeFromItem(pllAssocs, pobjAssoc) == NULL)
                            {
                                // no:
                                lstAppendItem(pllAssocs, pobjAssoc);
                                ulrc++;
                            }

                            // go for next object handle (after the 0 byte)
                            pAssoc += strlen(pAssoc) + 1;
                            if (pAssoc >= pWPSType->pszObjectHandles + pWPSType->cbObjectHandles)
                                break; // while (*pAssoc)
                        }
                        else
                            break; // while (*pAssoc)
                    } // end while (*pAssoc)
                }

                // free(pszAssocData);
            }

            // get parent type
            {
                PSZ     psz2Free = 0;
                if (pszParentForType)
                    psz2Free = pszParentForType;
                pszParentForType = prfhQueryProfileData(HINI_USER,
                                                        INIAPP_XWPFILETYPES, // "XWorkplace:FileTypes"
                                                        pszType2,
                                                        NULL);
                #ifdef DEBUG_ASSOCS
                    _Pmpf(("        Got %s as parent for %s", pszParentForType, pszType2));
                #endif

                if (psz2Free)
                    free(psz2Free);     // from last loop
                if (pszParentForType)
                    // parent found: use that one
                    pszType2 = pszParentForType;
                else
                    break;
            }

        } while (TRUE);

        ftypUnlockCaches();
    }

    return (ulrc);
}

/*
 *@@ ftypRenameFileType:
 *      renames a file type and updates all associated
 *      INI entries.
 *
 *      This returns:
 *
 *      -- ERROR_FILE_NOT_FOUND: pcszOld is not a valid file type.
 *
 *      -- ERROR_FILE_EXISTS: pcszNew is already occupied.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

APIRET ftypRenameFileType(const char *pcszOld,      // in: existing file type
                          const char *pcszNew)      // in: new name for pcszOld
{
    APIRET arc = FALSE;

    // check WPS file types... this better exist, or we'll stop
    // right away
    PWPSTYPEASSOCTREENODE pWPSType = ftypFindWPSType(pcszOld);
    if (!pWPSType)
        arc = ERROR_FILE_NOT_FOUND;
    else
    {
        // pcszNew must not be used yet.
        if (ftypFindWPSType(pcszNew))
            arc = ERROR_FILE_EXISTS;
        else
        {
            // OK... first of all, we must write a new entry
            // into "PMWP_ASSOC_TYPE" with the old handles.
            // There's no such thing as a "rename INI entry",
            // so we can only write a new one and delete the old
            // one.

            if (!PrfWriteProfileData(HINI_USER,
                                     (PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE"
                                     (PSZ)pcszNew,
                                     pWPSType->pszObjectHandles,
                                     pWPSType->cbObjectHandles))
                arc = ERROR_INVALID_DATA;
            else
            {
                PSZ pszXWPParentTypes;
                // OK so far: delete the old entry
                PrfWriteProfileData(HINI_USER,
                                    (PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE"
                                    (PSZ)pcszOld,
                                    NULL,
                                    0);

                // now update the the XWP entries, if any exist

                // 1) associations linked to this file type:
                prfhRenameKey(HINI_USER,
                              INIAPP_XWPFILEFILTERS, // "XWorkplace:FileFilters"
                              pcszOld,
                              NULL,         // same app
                              pcszNew);
                // 2) parent types for this:
                prfhRenameKey(HINI_USER,
                              INIAPP_XWPFILETYPES, // "XWorkplace:FileTypes"
                              pcszOld,
                              NULL,         // same app
                              pcszNew);

                // 3) now... go thru ALL of "XWorkplace:FileTypes"
                // and check if any of the types in there has specified
                // pcszOld as its parent type. If so, update that entry.
                pszXWPParentTypes = prfhQueryKeysForApp(HINI_USER,
                                                     INIAPP_XWPFILETYPES); // "XWorkplace:FileTypes"
                if (pszXWPParentTypes)
                {
                    PSZ pParentThis = pszXWPParentTypes;
                    while (*pParentThis)
                    {
                        PSZ pszThisParent = prfhQueryProfileData(HINI_USER,
                                                                 INIAPP_XWPFILETYPES, // "XWorkplace:FileTypes"
                                                                 pParentThis,
                                                                 NULL);
                        if (pszThisParent)
                        {
                            if (!strcmp(pszThisParent, pcszOld))
                                // replace this entry
                                PrfWriteProfileString(HINI_USER,
                                                      (PSZ)INIAPP_XWPFILETYPES, // "XWorkplace:FileTypes"
                                                      pParentThis,
                                                      (PSZ)pcszNew);

                            free(pszThisParent);
                        }
                        pParentThis += strlen(pParentThis) + 1;
                    }

                    free(pszXWPParentTypes);
                }
            }

        }

        ftypInvalidateCaches();
    }

    return (arc);
}

/* ******************************************************************
 *
 *   XFldDataFile extended associations
 *
 ********************************************************************/

/*
 *@@ ftypBuildAssocsList:
 *      this helper function builds a list of all
 *      associated WPProgram and WPProgramFile objects
 *      in the data file's instance data.
 *
 *      This is the heart of the extended associations
 *      engine. This function gets called whenever
 *      extended associations are needed.
 *
 *      --  From ftypQueryAssociatedProgram, this gets
 *          called with (fUsePlainTextAsDefault == FALSE),
 *          mostly (inheriting that func's param).
 *          Since that method is called during folder
 *          population to find the correct icon for the
 *          data file, we do NOT want all data files to
 *          receive the icons for plain text files.
 *
 *      --  From ftypModifyDataFileOpenSubmenu, this gets
 *          called with (fUsePlainTextAsDefault == TRUE).
 *          We do want the items for "plain text" in the
 *          "Open" context menu if no other type has been
 *          assigned.
 *
 *      The list (which is of type PLINKLIST, containing
 *      plain WPObject* pointers) is returned and should
 *      be freed later using lstFree.
 *
 *      This returns NULL if an error occured or no
 *      associations were added.
 *
 *      NOTE: This locks each object instantiated as a
 *      result of the call. Use ftypFreeAssocsList instead
 *      of lstFree to free the list returned by this
 *      function.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 *@@changed V0.9.6 (2000-10-16) [umoeller]: now returning a PLINKLIST
 *@@changed V0.9.7 (2001-01-11) [umoeller]: no longer using plain text always
 *@@changed V0.9.9 (2001-03-27) [umoeller]: no longer creating list if no assocs exist, returning NULL now
 */

PLINKLIST ftypBuildAssocsList(WPDataFile *somSelf,
                              BOOL fUsePlainTextAsDefault)
{
    PLINKLIST   pllAssocs = NULL;

    // create list of type strings (to be freed)
    PLINKLIST   pllTypes = lstCreate(TRUE);       // free items
    if (pllTypes)
    {
        ULONG       cTypes = 0;

        // check if the data file has a file type
        // assigned explicitly
        PSZ pszTypes = _wpQueryType(somSelf);

        if (pszTypes)
        {
            // yes, explicit type(s):
            // decode those
            cTypes = AppendTypesFromString(pszTypes,
                                           pllTypes);
        }

        // add automatic (extended) file types based on the
        // object file name
        cTypes += AppendTypesForFile(_wpQueryTitle(somSelf),
                                     pllTypes);

        if ((cTypes == 0) && (fUsePlainTextAsDefault))
        {
            // we still have no types: this happens if
            // 1) no explicit type was assigned and
            // 2) none of the type filters matched
            // --> in that case, use "Plain Text"
            if (lstAppendItem(pllTypes, strdup("Plain Text")))
                cTypes++;
        }

        if (cTypes)
        {
            // OK, file type(s) found (either explicit or automatic):

            // create list of associations (WPProgram or WPProgramFile)
            // from the types list we had above
            PLISTNODE pNode = lstQueryFirstNode(pllTypes);

            // loop thru list
            while (pNode)
            {
                PSZ pszTypeThis = (PSZ)pNode->pItemData;

                if (!pllAssocs)
                    // first loop: create list of assocs to be returned
                    pllAssocs = lstCreate(FALSE);

                ftypListAssocsForType(pszTypeThis,
                                      pllAssocs);
                pNode = pNode->pNext;
            }

            #ifdef DEBUG_ASSOCS
                _Pmpf(("    ftypQueryAssociatedProgram: got %d assocs", cAssocObjects));
            #endif
        }

        lstFree(pllTypes);
    }

    return (pllAssocs);
}

/*
 *@@ ftypFreeAssocsList:
 *      frees all resources allocated by ftypBuildAssocsList
 *      by unlocking all objects on the specified list and
 *      then freeing the list.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

ULONG ftypFreeAssocsList(PLINKLIST pllAssocs)    // in: list created by ftypBuildAssocsList
{
    ULONG       ulrc = 0;
    PLISTNODE   pNode = lstQueryFirstNode(pllAssocs);
    while (pNode)
    {
        WPObject *pObj = (WPObject*)pNode->pItemData;
        _wpUnlockObject(pObj);

        pNode = pNode->pNext;
        ulrc++;
    }

    lstFree(pllAssocs);

    return (ulrc);
}

/*
 *@@ ftypQueryAssociatedProgram:
 *      implementation for XFldDataFile::wpQueryAssociatedProgram.
 *
 *      This gets called _instead_ of the WPDataFile version if
 *      extended associations have been enabled.
 *
 *      This also gets called from XFldDataFile::wpOpen to find
 *      out which of the new associations needs to be opened.
 *
 *      It is the responsibility of this method to return the
 *      associated WPProgram or WPProgramFile object for the
 *      given data file according to ulView.
 *
 *      Normally, ulView is the menu item ID in the "Open"
 *      submenu, which is >= 0x1000. HOWEVER, for some reason,
 *      the WPS also uses OPEN_RUNNING as the default view.
 *      We can also get OPEN_DEFAULT.
 *
 *      The object returned has been locked by this function.
 *      Use _wpUnlockObject to unlock it again.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 *@@changed V0.9.6 (2000-10-16) [umoeller]: lists are temporary only now
 *@@changed V0.9.6 (2000-11-12) [umoeller]: added pulView output
 */

WPObject* ftypQueryAssociatedProgram(WPDataFile *somSelf,       // in: data file
                                     PULONG pulView,            // in: default view (normally 0x1000,
                                                                // can be > 1000 if the default view
                                                                // has been manually changed on the
                                                                // "Menu" page);
                                                                // out: "real" default view if this
                                                                // was OPEN_RUNNING or something
                                     BOOL fUsePlainTextAsDefault)
                                            // in: use "plain text" as standard if no other type was found?
{
    WPObject *pObjReturn = 0;

    PLINKLIST   pllAssocObjects = ftypBuildAssocsList(somSelf,
                                                      fUsePlainTextAsDefault);

    if (pllAssocObjects)
    {
        ULONG       cAssocObjects = lstCountItems(pllAssocObjects);
        if (cAssocObjects)
        {
            // any items found:
            ULONG               ulIndex = 0;
            PLISTNODE           pAssocObjectNode = 0;

            if (*pulView == OPEN_RUNNING)
                *pulView = 0x1000;
            else if (*pulView == OPEN_DEFAULT)
                *pulView = _wpQueryDefaultView(somSelf);
                        // returns 0x1000, unless the user has changed
                        // the default association on the "Menu" page

            // calc index to search...
            if (*pulView >= 0x1000)
            {
                ulIndex = *pulView - 0x1000;
                if (ulIndex > cAssocObjects)
                    ulIndex = 0;
            }
            else
                ulIndex = 0;

            pAssocObjectNode = lstNodeFromIndex(pllAssocObjects,
                                                ulIndex);
            if (pAssocObjectNode)
            {
                pObjReturn = (WPObject*)pAssocObjectNode->pItemData;
                // raise lock count on this object again (i.e. lock
                // twice) because ftypFreeAssocsList unlocks each
                // object on the list once
                _wpLockObject(pObjReturn);
            }
        }

        ftypFreeAssocsList(pllAssocObjects);
    }

    return (pObjReturn);
}

/*
 *@@ ftypModifyDataFileOpenSubmenu:
 *      this adds associations to an "Open" submenu.
 *
 *      -- On Warp 3, this gets called from the wpDisplayMenu
 *         override with (fDeleteExisting == TRUE).
 *
 *         This is a brute-force hack to get around the
 *         limitations which IBM has imposed on manipulation
 *         of the "Open" submenu. See XFldDataFile::wpDisplayMenu
 *         for details.
 *
 *         We remove all associations from the "Open" menu and
 *         add our own ones instead.
 *
 *      -- Instead, on Warp 4, this gets called from our
 *         XFldDataFile::wpModifyMenu hack with
 *         (fDeleteExisting == FALSE).
 *
 *      This gets called only if extended associations are
 *      enabled.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

BOOL ftypModifyDataFileOpenSubmenu(WPDataFile *somSelf, // in: data file in question
                                   HWND hwndOpenSubmenu,
                                            // in: "Open" submenu window
                                   BOOL fDeleteExisting)
                                            // in: if TRUE, we remove all items from "Open"
{
    BOOL            brc = FALSE;

    if (hwndOpenSubmenu)
    {
        ULONG       ulItemID = 0;

        // 1) remove existing (default WPS) association
        // items from "Open" submenu

        if (!fDeleteExisting)
            brc = TRUE;     // continue
        else
        {
            // delete existing:
            // find first item
            do
            {
                ulItemID = (ULONG)WinSendMsg(hwndOpenSubmenu,
                                             MM_ITEMIDFROMPOSITION,
                                             0,       // first item
                                             0);      // reserved
                if ((ulItemID) && (ulItemID != MIT_ERROR))
                {
                    #ifdef DEBUG_ASSOCS
                        PSZ pszItemText = winhQueryMenuItemText(hwndOpenSubmenu, ulItemID);
                        _Pmpf(("mnuModifyDataFilePopupMenu: removing 0x%lX (%s)",
                                    ulItemID,
                                    pszItemText));
                        free(pszItemText);
                    #endif

                    winhDeleteMenuItem(hwndOpenSubmenu, ulItemID);

                    brc = TRUE;
                }
                else
                    break;

            } while (TRUE);
        }

        // 2) add the new extended associations

        if (brc)
        {
            PLINKLIST   pllAssocObjects = ftypBuildAssocsList(somSelf,
                                                              // use "plain text" as default:
                                                              TRUE);
            if (pllAssocObjects)
            {
                ULONG       cAssocObjects = lstCountItems(pllAssocObjects);
                if (cAssocObjects)
                {
                    // now add all the associations; this list has
                    // instances of WPProgram and WPProgramFile
                    PLISTNODE       pNode = lstQueryFirstNode(pllAssocObjects);

                    // get data file default associations; this should
                    // return something >= 0x1000 also
                    ULONG           ulDefaultView = _wpQueryDefaultView(somSelf);

                    // initial menu item ID; all associations must have
                    // IDs >= 0x1000
                    ulItemID = 0x1000;

                    while (pNode)
                    {
                        WPObject *pAssocThis = (WPObject*)pNode->pItemData;
                        if (pAssocThis)
                        {
                            PSZ pszAssocTitle = _wpQueryTitle(pAssocThis);
                            if (pszAssocTitle)
                            {
                                winhInsertMenuItem(hwndOpenSubmenu,  // still has "Open" submenu
                                                   MIT_END,
                                                   ulItemID,
                                                   pszAssocTitle,
                                                   MIS_TEXT,
                                                   // if this is the default view,
                                                   // mark as default
                                                   (ulItemID == ulDefaultView)
                                                        ? MIA_CHECKED
                                                        : 0);
                            }
                        }

                        ulItemID++;     // raise item ID even if object was invalid;
                                        // this must be the same in wpMenuItemSelected

                        pNode = pNode->pNext;
                    }
                } // end if (cAssocObjects)

                ftypFreeAssocsList(pllAssocObjects);
            }
        }
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Helper functions for file-type dialogs
 *
 ********************************************************************/

/*
 *  See ftypFileTypesInitPage for an introduction
 *  how all this crap works. This is complex.
 */

// forward decl here
typedef struct _FILETYPELISTITEM *PFILETYPELISTITEM;

/*
 * FILETYPERECORD:
 *      extended record core structure for
 *      "File types" container (Tree view).
 *
 *@@changed V0.9.9 (2001-03-27) [umoeller]: now using CHECKBOXRECORDCORE
 */

typedef struct _FILETYPERECORD
{
    CHECKBOXRECORDCORE  recc;               // extended record core for checkboxes;
                                            // see comctl.c
    PFILETYPELISTITEM   pliFileType;        // added V0.9.9 (2001-02-06) [umoeller]
} FILETYPERECORD, *PFILETYPERECORD;

/*
 * FILETYPELISTITEM:
 *      list item structure for building an internal
 *      linked list of all file types (linklist.c).
 */

typedef struct _FILETYPELISTITEM
{
    PFILETYPERECORD     precc;
    PSZ                 pszFileType;        // copy of file type in INI (malloc)
    BOOL                fProcessed;
    BOOL                fCircular;          // security; prevent circular references
} FILETYPELISTITEM;

/*
 *@@ AddFileType2Cnr:
 *      this adds the given file type to the given
 *      container window, which should be in Tree
 *      view to have a meaningful display.
 *
 *      pliAssoc->pftrecc is set to the new record
 *      core, which is also returned.
 */

PFILETYPERECORD AddFileType2Cnr(HWND hwndCnr,           // in: cnr to insert into
                                PFILETYPERECORD preccParent,  // in: parent recc for tree view
                                PFILETYPELISTITEM pliAssoc,   // in: file type to add
                                PLINKLIST pllCheck,      // in: list of types for checking records
                                PLINKLIST pllDisable)    // in: list of types for disabling records
{
    PFILETYPERECORD preccNew
        = (PFILETYPERECORD)cnrhAllocRecords(hwndCnr, sizeof(FILETYPERECORD), 1);
    // recc attributes
    BOOL        fExpand = FALSE;
    ULONG       ulAttrs = CRA_COLLAPSED | CRA_DROPONABLE;      // records can be dropped

    if (preccNew)
    {
        PLISTNODE   pNode;

        // store reverse linkage V0.9.9 (2001-02-06) [umoeller]
        preccNew->recc.ulStyle = WS_VISIBLE | BS_AUTOCHECKBOX;
        preccNew->recc.usCheckState = 0;
        // for the CHECKBOXRECORDCORE item id, we use the list
        // item pointer... this is unique
        preccNew->recc.ulItemID = (ULONG)pliAssoc;

        preccNew->pliFileType = pliAssoc;

        if (pllCheck)
        {
            pNode = lstQueryFirstNode(pllCheck);
            while (pNode)
            {
                PSZ pszType = (PSZ)pNode->pItemData;
                if (!strcmp(pszType, pliAssoc->pszFileType))
                {
                    // matches:
                    preccNew->recc.usCheckState = 1;
                    fExpand = TRUE;
                    break;
                }
                pNode = pNode->pNext;
            }
        }

        if (pllDisable)
        {
            pNode = lstQueryFirstNode(pllDisable);
            while (pNode)
            {
                PSZ pszType = (PSZ)pNode->pItemData;
                if (!strcmp(pszType, pliAssoc->pszFileType))
                {
                    // matches:
                    preccNew->recc.usCheckState = 1;
                    ulAttrs |= CRA_DISABLED;
                    fExpand = TRUE;
                    break;
                }
                pNode = pNode->pNext;
            }
        }

        if (fExpand)
        {
            cnrhExpandFromRoot(hwndCnr,
                               (PRECORDCORE)preccParent);
        }

        // insert the record
        cnrhInsertRecords(hwndCnr,
                          (PRECORDCORE)preccParent,
                          (PRECORDCORE)preccNew,
                          TRUE, // invalidate
                          pliAssoc->pszFileType,
                          ulAttrs,
                          1);
    }

    pliAssoc->precc = preccNew;
    pliAssoc->fProcessed = TRUE;

    return (preccNew);
}

/*
 *@@ AddFileTypeAndAllParents:
 *
 */

PFILETYPERECORD AddFileTypeAndAllParents(HWND hwndCnr,          // in: cnr to insert into
                                         PLINKLIST pllFileTypes, // in: list of all file types
                                         PSZ pszKey,
                                         PLINKLIST pllCheck,      // in: list of types for checking records
                                         PLINKLIST pllDisable)    // in: list of types for disabling records
{
    PFILETYPERECORD     pftreccParent = NULL,
                        pftreccReturn = NULL;
    PLISTNODE           pAssocNode;

    // query the parent for pszKey
    PSZ pszParentForKey = prfhQueryProfileData(HINI_USER,
                                               INIAPP_XWPFILETYPES, // "XWorkplace:FileTypes"
                                               pszKey,
                                               NULL);

    if (pszParentForKey)
    {
        // key has a parent: recurse first! we need the
        // parent records before we insert the actual file
        // type as a child of this
        pftreccParent = AddFileTypeAndAllParents(hwndCnr,
                                                 pllFileTypes,
                                                 // recurse with parent
                                                 pszParentForKey,
                                                 pllCheck,
                                                 pllDisable);
        free(pszParentForKey);
    }

    // we arrive here after the all the parents
    // of pszKey have been added;
    // if we have no parent, pftreccParent is NULL

    // now find the file type list item
    // which corresponds to pKey
    pAssocNode = lstQueryFirstNode(pllFileTypes);
    while (pAssocNode)
    {
        PFILETYPELISTITEM pliAssoc = (PFILETYPELISTITEM)pAssocNode->pItemData;

        if (strcmp(pliAssoc->pszFileType,
                   pszKey) == 0)
        {
            if (!pliAssoc->fProcessed)
            {
                if (!pliAssoc->fCircular)
                    // add record core, which will be stored in
                    // pliAssoc->pftrecc
                    pftreccReturn = AddFileType2Cnr(hwndCnr,
                                                    pftreccParent,
                                                        // parent record; this might be NULL
                                                    pliAssoc,
                                                    pllCheck,
                                                    pllDisable);
                pliAssoc->fCircular = TRUE;
            }
            else
                // record core already created:
                // return that one
                pftreccReturn = pliAssoc->precc;

            // in any case, stop
            break;
        }

        pAssocNode = pAssocNode->pNext;
    }

    if (pAssocNode == NULL)
    {
        // no file type found which corresponds
        // to the hierarchy INI item: delete it,
        // since it has no further meaning
        PrfWriteProfileString(HINI_USER,
                              (PSZ)INIAPP_XWPFILETYPES,    // "XWorkplace:FileTypes"
                              pszKey,
                              NULL);  // delete key
        ftypInvalidateCaches();
    }

    // return the record core which we created;
    // if this is a recursive call, this will
    // be used as a parent by the parent call
    return (pftreccReturn);
}

/*
 *@@ FillCnrWithAvailableTypes:
 *      fills the specified container with the available
 *      file types.
 *
 *      This happens in four steps:
 *
 *      1)  load the WPS file types list
 *          from OS2.INI ("PMWP_ASSOC_FILTER")
 *          and create a linked list from
 *          it in FILETYPESPAGEDATA.pllFileTypes;
 *
 *      2)  load the XWorkplace file types
 *          hierarchy from OS2.INI
 *          ("XWorkplace:FileTypes");
 *
 *      3)  insert all hierarchical file
 *          types in that list;
 *
 *      4)  finally, insert all remaining
 *          WPS file types which have not
 *          been found in the hierarchical
 *          list.
 *
 *      pllCheck and pllDisable are used to automatically
 *      check and/or disable the CHECKBOXRECORDCORE's.
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

VOID FillCnrWithAvailableTypes(HWND hwndCnr,
                               PLINKLIST pllFileTypes,  // in: list to append types to
                               PLINKLIST pllCheck,      // in: list of types for checking records
                               PLINKLIST pllDisable)    // in: list of types for disabling records
{

    PSZ pszAssocTypeList;

    HPOINTER hptrOld = winhSetWaitPointer();

    // step 1: load WPS file types list
    pszAssocTypeList = prfhQueryKeysForApp(HINI_USER,
                                           WPINIAPP_ASSOCTYPE);
                                                // "PMWP_ASSOC_TYPE"
    if (pszAssocTypeList)
    {
        PSZ         pKey = pszAssocTypeList;
        PSZ         pszFileTypeHierarchyList;
        PLISTNODE   pAssocNode;

        // if the list had been created before, free it now
        lstClear(pllFileTypes);

        while (*pKey != 0)
        {
            // for each WPS file type,
            // create a list item
            PFILETYPELISTITEM pliAssoc = malloc(sizeof(FILETYPELISTITEM));
            // mark as "not processed"
            pliAssoc->fProcessed = FALSE;
            // set anti-recursion flag
            pliAssoc->fCircular = FALSE;
            // store file type
            pliAssoc->pszFileType = strdup(pKey);
            // add item to list
            lstAppendItem(pllFileTypes, pliAssoc);

            // go for next key
            pKey += strlen(pKey)+1;
        }

        free(pszAssocTypeList);

        // step 2: load XWorkplace file types hierarchy
        WinEnableWindowUpdate(hwndCnr, FALSE);

        pszFileTypeHierarchyList = prfhQueryKeysForApp(HINI_USER,
                                                       INIAPP_XWPFILETYPES);
                                                        // "XWorkplace:FileTypes"

        // step 3: go thru the file type hierarchy
        // and add parents;
        // AddFileTypeAndAllParents will mark the
        // inserted items as processed (for step 4)
        if (pszFileTypeHierarchyList)
        {
            pKey = pszFileTypeHierarchyList;
            while (*pKey != 0)
            {
                PFILETYPERECORD precc = AddFileTypeAndAllParents(hwndCnr,
                                                                 pllFileTypes,
                                                                 pKey,
                                                                 pllCheck,
                                                                 pllDisable);
                                                // this will recurse

                // go for next key
                pKey += strlen(pKey)+1;
            }
        }

        // step 4: add all remaining file types
        // to root level
        pAssocNode = lstQueryFirstNode(pllFileTypes);
        while (pAssocNode)
        {
            PFILETYPELISTITEM pliAssoc = (PFILETYPELISTITEM)(pAssocNode->pItemData);
            if (!pliAssoc->fProcessed)
            {
                PFILETYPERECORD precc = AddFileType2Cnr(hwndCnr,
                                                        NULL,       // parent record == root
                                                        pliAssoc,
                                                        pllCheck,
                                                        pllDisable);
            }
            pAssocNode = pAssocNode->pNext;
        }

        WinShowWindow(hwndCnr, TRUE);
    }

    WinSetPointer(HWND_DESKTOP, hptrOld);
}

/*
 *@@ ClearAvailableTypes:
 *      cleans up the mess FillCnrWithAvailableTypes
 *      created.
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

VOID ClearAvailableTypes(HWND hwndCnr,
                         PLINKLIST pllFileTypes)
{
    PLISTNODE pAssocNode = lstQueryFirstNode(pllFileTypes);
    PFILETYPELISTITEM pliAssoc;

    // first clear the container because the records
    // point into the file-type list items
    cnrhRemoveAll(hwndCnr);

    while (pAssocNode)
    {
        pliAssoc = pAssocNode->pItemData;
        if (pliAssoc->pszFileType)
            free(pliAssoc->pszFileType);
        // the pliAssoc will be freed by lstFree

        pAssocNode = pAssocNode->pNext;
    }

    lstClear(pllFileTypes);
}

/* ******************************************************************
 *
 *   XFldWPS notebook callbacks (notebook.c) for "File Types" page
 *
 ********************************************************************/

/*
 * ASSOCRECORD:
 *      extended record core structure for the
 *      "Associations" container.
 */

typedef struct _ASSOCRECORD
{
    RECORDCORE          recc;
    HOBJECT             hobj;
} ASSOCRECORD, *PASSOCRECORD;

/*
 * FILETYPEPAGEDATA:
 *      this is created in PCREATENOTEBOOKPAGE
 *      to store various data for the file types
 *      page.
 */

typedef struct _FILETYPESPAGEDATA
{
    // reverse linkage to notebook page data
    // (needed for subwindows)
    PCREATENOTEBOOKPAGE pcnbp;

    // linked list of file types (linklist.c);
    // this contains FILETYPELISTITEM structs
    LINKLIST llFileTypes;           // auto-free

    // linked list of various items which have
    // been allocated using malloc(); this
    // will be cleaned up when the page is
    // destroyed (CBI_DESTROY)
    LINKLIST llCleanup;             // auto-free

    // controls which are used all the time
    HWND    hwndTypesCnr,
            hwndAssocsCnr,
            hwndFiltersCnr,
            hwndIconStatic;

    // popup menus
    HWND    hmenuFileTypeSel,
            hmenuFileTypeNoSel,
            hmenuFileFilterSel,
            hmenuFileFilterNoSel,
            hmenuFileAssocSel,
            hmenuFileAssocNoSel;

    // non-modal "Import WPS Filters" dialog
    // or NULLHANDLE, if not open
    HWND    hwndWPSImportDlg;

    // original window proc of the "Associations" list box
    // (which has been subclassed to allow for d'n'd);
    PFNWP   pfnwpListBoxOriginal;

    // currently selected record core in container
    // (updated by CN_EMPHASIS)
    PFILETYPERECORD pftreccSelected;

    // drag'n'drop within the file types container
    // BOOL fFileTypesDnDValid;

    // drag'n'drop of WPS objects to assocs container;
    // NULL if d'n'd is invalid
    WPObject *pobjDrop;
    // record core after which item is to be inserted
    PRECORDCORE preccAfter;

    // rename of file types:
    PSZ     pszFileTypeOld;         // NULL until a file type was actually renamed

} FILETYPESPAGEDATA, *PFILETYPESPAGEDATA;

// define a new rendering mechanism, which only
// our own container supports (this will make
// sure that we can only do d'n'd within this
// one container)
#define DRAG_RMF  "(DRM_XWPFILETYPES)x(DRF_UNKNOWN)"

#define RECORD_DISABLED CRA_DISABLED

/*
 *@@ AddAssocObject2Cnr:
 *      this adds the given WPProgram or WPProgramFile object
 *      to the given position in the "Associations" container.
 *      The object's object handle is stored in the ASSOCRECORD.
 *
 *      PM provides those "handles" for each list box item,
 *      which can be used for any purpose by an application.
 *      We use it for the object handles here.
 *
 *      Note: This does NOT invalidate the container.
 *
 *      Returns the new ASSOCRECORD or NULL upon errors.
 *
 *@@changed V0.9.4 (2000-06-14) [umoeller]: fixed repaint problems
 */

PASSOCRECORD AddAssocObject2Cnr(HWND hwndAssocsCnr,
                                WPObject *pObject,  // in: must be a WPProgram or WPProgramFile
                                PRECORDCORE preccInsertAfter, // in: record to insert after (or CMA_FIRST or CMA_END)
                                BOOL fEnableRecord) // in: if FALSE, the record will be disabled
{
    // ULONG ulrc = LIT_ERROR;

    PSZ pszObjectTitle = _wpQueryTitle(pObject);

    PASSOCRECORD preccNew
        = (PASSOCRECORD)cnrhAllocRecords(hwndAssocsCnr,
                                         sizeof(ASSOCRECORD),
                                         1);
    if (preccNew)
    {
        ULONG   flRecordAttr = CRA_RECORDREADONLY | CRA_DROPONABLE;

        if (!fEnableRecord)
            flRecordAttr |= RECORD_DISABLED;

        // store object handle for later
        preccNew->hobj = _wpQueryHandle(pObject);

        #ifdef DEBUG_ASSOCS
            _Pmpf(("AddAssoc: flRecordAttr %lX", flRecordAttr));
        #endif

        cnrhInsertRecordAfter(hwndAssocsCnr,
                              (PRECORDCORE)preccNew,
                              pszObjectTitle,
                              flRecordAttr,
                              (PRECORDCORE)preccInsertAfter,
                              FALSE);   // no invalidate
    }

    return (preccNew);
}

/*
 *@@ WriteAssocs2INI:
 *      this updates the "PMWP_ASSOC_FILTER" key in OS2.INI
 *      when the associations for a certain file type have
 *      been changed.
 *
 *      This func automatically determines the file type
 *      to update (== the INI "key") from hwndCnr and
 *      reads the associations for it from the list box.
 *      The list box entries must have been added using
 *      AddAssocObject2Cnr, because otherwise this
 *      func cannot find the object handles.
 */

BOOL WriteAssocs2INI(PSZ  pszProfileKey, // in: either "PMWP_ASSOC_TYPE" or "PMWP_ASSOC_FILTER"
                     HWND hwndTypesCnr,  // in: cnr with selected FILETYPERECORD
                     HWND hwndAssocsCnr) // in: cnr with ASSOCRECORDs
{
    BOOL    brc = FALSE;

    if ((hwndTypesCnr) && (hwndAssocsCnr))
    {
        // get selected file type; since the cnr is in
        // Tree view, there can be only one
        PFILETYPERECORD preccSelected
            = (PFILETYPERECORD)WinSendMsg(hwndTypesCnr,
                                          CM_QUERYRECORDEMPHASIS,
                                          (MPARAM)CMA_FIRST,
                                          (MPARAM)CRA_SELECTED);
        if (    (preccSelected)
             && ((LONG)preccSelected != -1)
           )
        {
            // the file type is equal to the record core title
            PSZ     pszFileType = preccSelected->recc.recc.pszIcon;

            CHAR    szAssocs[1000] = "";
            ULONG   cbAssocs = 0;

            // now create the handles string for PMWP_ASSOC_FILTER
            // from the ASSOCRECORD handles (which have been set
            // by AddAssocObject2Cnr above)

            PASSOCRECORD preccThis = (PASSOCRECORD)WinSendMsg(
                                                hwndAssocsCnr,
                                                CM_QUERYRECORD,
                                                NULL,
                                                MPFROM2SHORT(CMA_FIRST, CMA_ITEMORDER));

            while ((preccThis) && ((ULONG)preccThis != -1))
            {
                if (preccThis->hobj == 0)
                    // item not found: exit
                    break;

                // if the assocs record is not disabled (i.e. not from
                // the parent record), add it to the string
                if ((preccThis->recc.flRecordAttr & RECORD_DISABLED) == 0)
                {
                    cbAssocs += sprintf(&(szAssocs[cbAssocs]), "%d", preccThis->hobj) + 1;
                }

                preccThis = (PASSOCRECORD)WinSendMsg(hwndAssocsCnr,
                                                     CM_QUERYRECORD,
                                                     preccThis,
                                                     MPFROM2SHORT(CMA_NEXT, CMA_ITEMORDER));
            }

            if (cbAssocs == 0)
                // always write at least the initial 0 byte
                cbAssocs = 1;

            brc = PrfWriteProfileData(HINI_USER,
                                      pszProfileKey,
                                      pszFileType,        // from cnr
                                      szAssocs,
                                      cbAssocs);
            ftypInvalidateCaches();
        }
    }

    return (brc);
}

/*
 *@@ UpdateAssocsCnr:
 *      this updates the given list box with associations,
 *      calling AddAssocObject2Cnr to add the
 *      program objects.
 *
 *      This gets called in several situations:
 *
 *      a)  when we're in "Associations"
 *          mode and a new record core gets selected in
 *          the container, or if we're switching to
 *          "Associations" mode; in this case, the
 *          "Associations" list box on the notebook
 *          page is updated (using "PMWP_ASSOC_TYPE");
 *
 *      b)  from the "Import WPS Filters" dialog,
 *          with the "Associations" list box in that
 *          dialog (and "PMWP_ASSOC_FILTER").
 */

VOID UpdateAssocsCnr(HWND hwndAssocsCnr,    // in: container to update
                     PSZ  pszTypeOrFilter,  // in: file type or file filter
                     PSZ  pszINIApp,        // in: "PMWP_ASSOC_TYPE" or "PMWP_ASSOC_FILTER"
                     BOOL fEmpty,           // in: if TRUE, list box will be emptied beforehand
                     BOOL fEnableRecords)   // in: if FALSE, the records will be disabled
{
    // get WPS associations from OS2.INI for this file type/filter
    ULONG cbAssocData;
    PSZ pszAssocData = prfhQueryProfileData(HINI_USER,
                                            pszINIApp, // "PMWP_ASSOC_TYPE" or "PMWP_ASSOC_FILTER"
                                            pszTypeOrFilter,
                                            &cbAssocData);

    if (fEmpty)
        // empty listbox
        WinSendMsg(hwndAssocsCnr,
                   CM_REMOVERECORD,
                   NULL,
                   MPFROM2SHORT(0, CMA_FREE | CMA_INVALIDATE));

    if (pszAssocData)
    {
        // pszAssocData now has the handles of the associated
        // objects (as decimal strings, which we'll decode now)
        PSZ     pAssoc = pszAssocData;
        if (pAssoc)
        {
            HOBJECT hobjAssoc;
            WPObject *pobjAssoc;

            // now parse the handles string
            while (*pAssoc)
            {
                sscanf(pAssoc, "%d", &hobjAssoc);
                pobjAssoc = _wpclsQueryObject(_WPObject, hobjAssoc);
                if (pobjAssoc)
                {
                    #ifdef DEBUG_ASSOCS
                        _Pmpf(("UpdateAssocsCnr: Adding record, fEnable: %d", fEnableRecords));
                    #endif

                    AddAssocObject2Cnr(hwndAssocsCnr,
                                       pobjAssoc,
                                       (PRECORDCORE)CMA_END,
                                       fEnableRecords);

                    // go for next object handle (after the 0 byte)
                    pAssoc += strlen(pAssoc) + 1;
                    if (pAssoc >= pszAssocData + cbAssocData)
                        break; // while (*pAssoc)
                }
                else
                    break; // while (*pAssoc)
            } // end while (*pAssoc)

            cnrhInvalidateAll(hwndAssocsCnr);
        }

        free(pszAssocData);
    }
}

/*
 *@@ AddFilter2Cnr:
 *      this adds the given file filter to the "Filters"
 *      container window, which should be in Name view
 *      to have a meaningful display.
 *
 *      The new record core is returned.
 */

PRECORDCORE AddFilter2Cnr(PFILETYPESPAGEDATA pftpd,
                          PSZ pszFilter)    // in: filter name
{
    PRECORDCORE preccNew = cnrhAllocRecords(pftpd->hwndFiltersCnr,
                                              sizeof(RECORDCORE), 1);
    if (preccNew)
    {
        PSZ pszNewFilter = strdup(pszFilter);
        // always make filters upper-case
        strupr(pszNewFilter);

        // insert the record (helpers/winh.c)
        cnrhInsertRecords(pftpd->hwndFiltersCnr,
                          (PRECORDCORE)NULL,      // parent
                          (PRECORDCORE)preccNew,
                          TRUE, // invalidate
                          pszNewFilter,
                          CRA_RECORDREADONLY,
                          1); // one record

        // store the new title in the linked
        // list of items to be cleaned up
        lstAppendItem(&pftpd->llCleanup, pszNewFilter);
    }

    return (preccNew);
}

/*
 *@@ WriteXWPFilters2INI:
 *      this updates the "XWorkplace:FileFilters" key in OS2.INI
 *      when the filters for a certain file type have been changed.
 *
 *      This func automatically determines the file type
 *      to update (== the INI "key") from the "Types" container
 *      and reads the filters for that type for it from the "Filters"
 *      container.
 *
 *      Returns the number of filters written into the INI data.
 */

ULONG WriteXWPFilters2INI(PFILETYPESPAGEDATA pftpd)
{
    ULONG ulrc = 0;

    if (pftpd->pftreccSelected)
    {
        PSZ     pszFileType = pftpd->pftreccSelected->recc.recc.pszIcon;
        CHAR    szFilters[2000] = "";   // should suffice
        ULONG   cbFilters = 0;
        PRECORDCORE preccFilter = NULL;
        // now create the filters string for INIAPP_XWPFILEFILTERS
        // from the record core titles in the "Filters" container
        while (TRUE)
        {
            preccFilter = WinSendMsg(pftpd->hwndFiltersCnr,
                                     CM_QUERYRECORD,
                                     preccFilter,   // ignored when CMA_FIRST
                                     MPFROM2SHORT((preccFilter)
                                                    ? CMA_NEXT
                                                    // first call:
                                                    : CMA_FIRST,
                                           CMA_ITEMORDER));

            if ((preccFilter == NULL) || (preccFilter == (PRECORDCORE)-1))
                // record not found: exit
                break;

            cbFilters += sprintf(&(szFilters[cbFilters]),
                                "%s",
                                preccFilter->pszIcon    // filter string
                               ) + 1;
            ulrc++;
        }

        PrfWriteProfileData(HINI_USER,
                            (PSZ)INIAPP_XWPFILEFILTERS, // "XWorkplace:FileFilters"
                            pszFileType,        // from cnr
                            (cbFilters)
                                ? szFilters
                                : NULL,     // no items found: delete key
                            cbFilters);
        ftypInvalidateCaches();
    }

    return (ulrc);
}

/*
 *@@ UpdateFiltersCnr:
 *      this updates the "Filters" container
 *      (which is only visible in "Filters" mode)
 *      according to FILETYPESPAGEDATA.pftreccSelected.
 *      (This structure is stored in CREATENOTEBOOKPAGE.pUser.)
 *
 *      This gets called when we're in "Filters"
 *      mode and a new record core gets selected in
 *      the container, or if we're switching to
 *      "Filters" mode.
 *
 *      In both cases, FILETYPESPAGEDATA.pftreccSelected
 *      has the currently selected file type in the container.
 */

VOID UpdateFiltersCnr(PFILETYPESPAGEDATA pftpd)
{
    // get text of selected record core
    PSZ pszFileType = pftpd->pftreccSelected->recc.recc.pszIcon;
    // pszFileType now has the selected file type

    // get the XWorkplace-defined filters for this file type
    ULONG cbFiltersData;
    PSZ pszFiltersData = prfhQueryProfileData(HINI_USER,
                                INIAPP_XWPFILEFILTERS,  // "XWorkplace:FileFilters"
                                pszFileType,
                                &cbFiltersData);

    // empty container
    WinSendMsg(pftpd->hwndFiltersCnr,
               CM_REMOVERECORD,
               (MPARAM)NULL,
               MPFROM2SHORT(0,      // all records
                        CMA_FREE | CMA_INVALIDATE));

    if (pszFiltersData)
    {
        // pszFiltersData now has the handles of the associated
        // objects (as decimal strings, which we'll decode now)
        PSZ     pFilter = pszFiltersData;

        if (pFilter)
        {
            // now parse the filters string
            while (*pFilter)
            {
                // add the filter to the "Filters" container
                AddFilter2Cnr(pftpd, pFilter);

                // go for next object filter (after the 0 byte)
                pFilter += strlen(pFilter) + 1;
                if (pFilter >= pszFiltersData + cbFiltersData)
                    break; // while (*pFilter))
            } // end while (*pFilter)
        }

        free(pszFiltersData);
    }
}

/*
 *@@ CreateFileType:
 *      creates a new file type.
 *
 *      Returns FALSE if an error occured, e.g. if
 *      the file type already existed.
 *
 *@@added V0.9.7 (2000-12-13) [umoeller]
 */

BOOL CreateFileType(PFILETYPESPAGEDATA pftpd,
                    PSZ pszNewType,             // in: new type (malloc!)
                    PFILETYPERECORD pParent)    // in: parent record or NULL if root type
{
    BOOL brc = FALSE;

    ULONG cbData = 0;
    // check if WPS type exists already
    if (    (!PrfQueryProfileSize(HINI_USER,
                                  (PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE"
                                  pszNewType,
                                  &cbData))
         && (cbData == 0)
       )
    {
        // no:
        // write to WPS's file types list
        BYTE bData = 0;
        if (PrfWriteProfileData(HINI_USER,
                                (PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE"
                                pszNewType,
                                &bData,
                                1))     // one byte
        {
            if (pParent)
                // parent specified:
                // add parent type to XWP's types to make
                // it a subtype
                brc = PrfWriteProfileString(HINI_USER,
                                            // application: "XWorkplace:FileTypes"
                                            (PSZ)INIAPP_XWPFILETYPES,
                                            // key --> the new file type:
                                            pszNewType,
                                            // string --> the parent:
                                            pParent->recc.recc.pszIcon);
            else
                brc = TRUE;

            if (brc)
            {
                // create new list item
                PFILETYPELISTITEM pliAssoc = (PFILETYPELISTITEM)malloc(sizeof(FILETYPELISTITEM));
                // mark as "processed"
                pliAssoc->fProcessed = TRUE;
                // store file type
                pliAssoc->pszFileType = pszNewType;     // malloc!
                // add record core, which will be stored in
                // pliAssoc->pftrecc
                AddFileType2Cnr(pftpd->hwndTypesCnr,
                                pParent,
                                pliAssoc,
                                NULL,
                                NULL);
                brc = (lstAppendItem(&pftpd->llFileTypes, pliAssoc) != NULL);
            }
        }

        ftypInvalidateCaches();
    }

    return (brc);
}

/*
 *@@ CheckFileTypeDrag:
 *      checks a drag operation for whether the container
 *      can accept it.
 *
 *      This has been exported from ftypFileTypesInitPage
 *      because both CN_DRAGOVER and CN_DROP need to
 *      check this. CN_DRAGOVER is never received in a
 *      lazy drag operation.
 *
 *      This does not change pftpd->fFileTypesDnDValid,
 *      but returns TRUE or FALSE instead.
 *
 *@@added V0.9.7 (2000-12-13) [umoeller]
 */

BOOL CheckFileTypeDrag(PFILETYPESPAGEDATA pftpd,
                       PDRAGINFO pDragInfo,     // in: drag info
                       PFILETYPERECORD pTargetRec, // in: target record from CNRDRAGINFO
                       PUSHORT pusIndicator,    // out: DOR_* flag for indicator (ptr can be NULL)
                       PUSHORT pusOperation)    // out: DOR_* flag for operation (ptr can be NULL)
{
    BOOL brc = FALSE;

    // OK so far:
    if (
            // accept no more than one single item at a time;
            // we cannot move more than one file type
            (pDragInfo->cditem != 1)
            // make sure that we only accept drops from ourselves,
            // not from other windows
         || (pDragInfo->hwndSource
                    != pftpd->hwndTypesCnr)
        )
    {
        if (pusIndicator)
            *pusIndicator = DOR_NEVERDROP;
        #ifdef DEBUG_ASSOCS
            _Pmpf(("   invalid items or invalid target"));
        #endif
    }
    else
    {

        // accept only default drop operation or move
        if (    (pDragInfo->usOperation == DO_DEFAULT)
             || (pDragInfo->usOperation == DO_MOVE)
           )
        {
            // get the item being dragged (PDRAGITEM)
            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pDragInfo, 0);
            if (pdrgItem)
            {
                if (    (pTargetRec // pcdi->pRecord  // target recc
                          != (PFILETYPERECORD)pdrgItem->ulItemID) // source recc
                     && (DrgVerifyRMF(pdrgItem, "DRM_XWPFILETYPES", "DRF_UNKNOWN"))
                   )
                {
                    // do not allow dragging the record on
                    // a child record of itself
                    // V0.9.7 (2000-12-13) [umoeller] thanks Martin Lafaix
                    if (!cnrhIsChildOf(pftpd->hwndTypesCnr,
                                       (PRECORDCORE)pTargetRec, // pcdi->pRecord,   // target
                                       (PRECORDCORE)pdrgItem->ulItemID)) // source
                    {
                        // allow drop
                        if (pusIndicator)
                            *pusIndicator = DOR_DROP;
                        if (pusOperation)
                            *pusOperation = DO_MOVE;
                        brc = TRUE;
                    }
                    #ifdef DEBUG_ASSOCS
                    else
                        _Pmpf(("   target is child of source"));
                    #endif
                }
                #ifdef DEBUG_ASSOCS
                else
                    _Pmpf(("   invalid RMF"));
                #endif
            }
            #ifdef DEBUG_ASSOCS
            else
                _Pmpf(("   cannot get drag item"));
            #endif
        }
        #ifdef DEBUG_ASSOCS
        else
            _Pmpf(("   invalid operation 0x%lX", pDragInfo->usOperation));
        #endif
    }

    return (brc);
}

/*
 *@@ G_ampFileTypesPage:
 *      resizing information for "File types" page.
 *      Stored in CREATENOTEBOOKPAGE of the
 *      respective "add notebook page" method.
 *
 *@@added V0.9.4 (2000-08-08) [umoeller]
 */

MPARAM G_ampFileTypesPage[] =
    {
        MPFROM2SHORT(ID_XSDI_FT_GROUP, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_XSDI_FT_CONTAINER, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_XSDI_FT_FILTERS_TXT, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FT_FILTERSCNR, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FT_ASSOCS_TXT, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FT_ASSOCSCNR, XAC_MOVEX | XAC_SIZEY)
    };

extern MPARAM *G_pampFileTypesPage = G_ampFileTypesPage;
extern ULONG G_cFileTypesPage = sizeof(G_ampFileTypesPage) / sizeof(G_ampFileTypesPage[0]);

/*
 *@@ ftypFileTypesInitPage:
 *      notebook callback function (notebook.c) for the
 *      "File types" page in the "Workplace Shell" object.
 *
 *      This page is maybe the most complicated of the
 *      "Workplace Shell" settings pages, but maybe also
 *      the most useful. ;-)
 *
 *      In this function, we set up the following window
 *      hierarchy (all of which exists in the dialog
 *      template loaded from the NLS DLL):
 *
 +      CREATENOTEBOOKPAGE.hwndPage (dialog in notebook,
 +        |                          maintained by notebook.c)
 +        |
 +        +-- ID_XSDI_FT_CONTAINER (container with file types);
 +        |     this thing is rather smart in that it can handle
 +        |     d'n'd within the same window (see
 +        |     ftypFileTypesItemChanged for details).
 +        |
 +        +-- ID_XSDI_FT_FILTERSCNR (container with filters);
 +        |     this has the filters for the file type (e.g. "*.txt");
 +        |     a plain container in flowed text view.
 +        |     This gets updated via UpdateFiltersCnr.
 +        |
 +        +-- ID_XSDI_FT_ASSOCSCNR (container with associations);
 +              this container is in non-flowed name view to hold
 +              the associations for the current file type. This
 +              accepts WPProgram's and WPProgramFile's via drag'n'drop.
 +              This gets updated via UpdateAssocsCnr.
 *
 *      The means of interaction between all these controls is the
 *      FILETYPESPAGEDATA structure which contains lots of data
 *      so all the different functions know what's going on. This
 *      is created here and stored in CREATENOTEBOOKPAGE.pUser so
 *      that it is always accessible and will be free()'d automatically.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.6 (2000-10-16) [umoeller]: fixed excessive menu creation
 */

VOID ftypFileTypesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                           ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FT_CONTAINER);

    /*
     * CBI_INIT:
     *      initialize page (called only once)
     */

    if (flFlags & CBI_INIT)
    {
        // HWND    hwndListBox;
        // CNRINFO CnrInfo;

        if (pcnbp->pUser == NULL)
        {
            PFILETYPESPAGEDATA pftpd;

            // first call: create FILETYPESPAGEDATA
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pftpd = malloc(sizeof(FILETYPESPAGEDATA));
            pcnbp->pUser = pftpd;
            memset(pftpd, 0, sizeof(FILETYPESPAGEDATA));

            pftpd->pcnbp = pcnbp;

            lstInit(&pftpd->llFileTypes, TRUE);

            // create "cleanup" list; this will hold all kinds
            // of items which need to be free()'d when the
            // notebook page is destroyed.
            // We just keep storing stuff in here so we need not
            // keep track of where we allocated what.
            lstInit(&pftpd->llCleanup, TRUE);

            // store container hwnd's
            pftpd->hwndTypesCnr = hwndCnr;
            pftpd->hwndFiltersCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FT_FILTERSCNR);
            pftpd->hwndAssocsCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FT_ASSOCSCNR);

            // set up file types container
            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TREE | CA_TREELINE | CV_TEXT);
                cnrhSetTreeIndent(15);
                cnrhSetSortFunc(fnCompareName);
            } END_CNRINFO(hwndCnr);

            // set up filters container
            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TEXT | CV_FLOW);
                cnrhSetSortFunc(fnCompareName);
            } END_CNRINFO(pftpd->hwndFiltersCnr);

            // set up assocs container
            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TEXT
                             | CA_ORDEREDTARGETEMPH
                                            // allow dropping only _between_ records
                             | CA_OWNERDRAW);
                                            // for disabled records
                // no sort here
            } END_CNRINFO(pftpd->hwndAssocsCnr);

            pftpd->hmenuFileTypeSel = WinLoadMenu(HWND_OBJECT,
                                                  cmnQueryNLSModuleHandle(FALSE),
                                                  ID_XSM_FILETYPES_SEL);
            pftpd->hmenuFileTypeNoSel = WinLoadMenu(HWND_OBJECT,
                                                    cmnQueryNLSModuleHandle(FALSE),
                                                    ID_XSM_FILETYPES_NOSEL);
            pftpd->hmenuFileFilterSel = WinLoadMenu(HWND_OBJECT,
                                                    cmnQueryNLSModuleHandle(FALSE),
                                                    ID_XSM_FILEFILTER_SEL);
            pftpd->hmenuFileFilterNoSel = WinLoadMenu(HWND_OBJECT,
                                                      cmnQueryNLSModuleHandle(FALSE),
                                                      ID_XSM_FILEFILTER_NOSEL);
            pftpd->hmenuFileAssocSel = WinLoadMenu(HWND_OBJECT,
                                                   cmnQueryNLSModuleHandle(FALSE),
                                                   ID_XSM_FILEASSOC_SEL);
            pftpd->hmenuFileAssocNoSel = WinLoadMenu(HWND_OBJECT,
                                                     cmnQueryNLSModuleHandle(FALSE),
                                                     ID_XSM_FILEASSOC_NOSEL);
        }
    }

    /*
     * CBI_SET:
     *      set controls' data
     */

    if (flFlags & CBI_SET)
    {
        PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;

        ClearAvailableTypes(pftpd->hwndTypesCnr,
                            &pftpd->llFileTypes);

        FillCnrWithAvailableTypes(pftpd->hwndTypesCnr,
                                  &pftpd->llFileTypes,
                                  NULL,         // check list
                                  NULL);        // disable list
    }

    /*
     * CBI_SHOW / CBI_HIDE:
     *      notebook page is turned to or away from
     */

    if (flFlags & (CBI_SHOW | CBI_HIDE))
    {
        PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;
        if (pftpd->hwndWPSImportDlg)
            WinShowWindow(pftpd->hwndWPSImportDlg, (flFlags & CBI_SHOW));
    }

    /*
     * CBI_DESTROY:
     *      clean up page before destruction
     */

    if (flFlags & CBI_DESTROY)
    {
        PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;

        WinDestroyWindow(pftpd->hmenuFileTypeSel);
        WinDestroyWindow(pftpd->hmenuFileTypeNoSel);
        WinDestroyWindow(pftpd->hmenuFileFilterSel);
        WinDestroyWindow(pftpd->hmenuFileFilterNoSel);
        WinDestroyWindow(pftpd->hmenuFileAssocSel);
        WinDestroyWindow(pftpd->hmenuFileAssocNoSel);

        if (pftpd->hwndWPSImportDlg)
            WinDestroyWindow(pftpd->hwndWPSImportDlg);

        ClearAvailableTypes(pftpd->hwndTypesCnr,
                            &pftpd->llFileTypes);

        // destroy "cleanup" list; this will
        // also free all the data on the list
        // using free()
        lstClear(&pftpd->llCleanup);
    }
}

/*
 *@@ ftypFileTypesItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "File types" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *      This is a real monster function, since it handles
 *      all the controls on the page, including container
 *      drag and drop.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.7 (2000-12-10) [umoeller]: DrgFreeDraginfo was missing
 *@@changed V0.9.7 (2000-12-13) [umoeller]: fixed dragging file type onto its child; thanks Martin Lafaix
 *@@changed V0.9.7 (2000-12-13) [umoeller]: fixed cleanup problem with lazy drag; thanks Martin Lafaix
 *@@changed V0.9.7 (2000-12-13) [umoeller]: lazy drop menu items were enabled wrong; thanks Martin Lafaix
 *@@changed V0.9.7 (2000-12-13) [umoeller]: made lazy drop menu items work
 *@@changed V0.9.7 (2000-12-13) [umoeller]: added "Create subtype" menu item
 */

MRESULT ftypFileTypesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG ulItemID,
                                 USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    MRESULT mrc = (MPARAM)0;

    PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;

    switch (ulItemID)
    {
        /*
         * ID_XSDI_FT_CONTAINER:
         *      "File types" container;
         *      supports d'n'd within itself
         */

        case ID_XSDI_FT_CONTAINER:
        {
            switch (usNotifyCode)
            {
                /*
                 * CN_EMPHASIS:
                 *      new file type selected in file types tree container:
                 *      update the data of the other controls.
                 *      (There can only be one selected record in the tree
                 *      at once.)
                 */

                case CN_EMPHASIS:
                {
                    // ulExtra has new selected record core;
                    if (pftpd->pftreccSelected != (PFILETYPERECORD)ulExtra)
                    {
                        PSZ     pszFileType = 0,
                                pszParentFileType = 0;
                        BOOL    fFirstLoop = TRUE;

                        // store it in FILETYPESPAGEDATA
                        pftpd->pftreccSelected = (PFILETYPERECORD)ulExtra;

                        UpdateFiltersCnr(pftpd);

                        // now go for this file type and keep climbing up
                        // the parent file types
                        pszFileType = strdup(pftpd->pftreccSelected->recc.recc.pszIcon);
                        while (pszFileType)
                        {
                            UpdateAssocsCnr(pftpd->hwndAssocsCnr,
                                                    // cnr to update
                                            pszFileType,
                                                    // file type to search for
                                                    // (selected record core)
                                            (PSZ)WPINIAPP_ASSOCTYPE,  // "PMWP_ASSOC_TYPE"
                                            fFirstLoop,   // empty container first
                                            fFirstLoop);  // enable records
                            fFirstLoop = FALSE;

                            // get parent file type;
                            // this will be NULL if there is none
                            pszParentFileType = prfhQueryProfileData(HINI_USER,
                                                    INIAPP_XWPFILETYPES,    // "XWorkplace:FileTypes"
                                                    pszFileType,
                                                    NULL);
                            free(pszFileType);
                            pszFileType = pszParentFileType;
                        } // end while (pszFileType)

                        if (pftpd->hwndWPSImportDlg)
                            // "Merge" dialog open:
                            // update "Merge with" text
                            WinSetDlgItemText(pftpd->hwndWPSImportDlg,
                                              ID_XSDI_FT_TYPE,
                                              pftpd->pftreccSelected->recc.recc.pszIcon);
                    }
                break; } // case CN_EMPHASIS

                /*
                 * CN_INITDRAG:
                 *      file type being dragged
                 * CN_PICKUP:
                 *      lazy drag initiated (Alt+MB2)
                 */

                case CN_INITDRAG:
                case CN_PICKUP:
                {
                    PCNRDRAGINIT pcdi = (PCNRDRAGINIT)ulExtra;

                    if (DrgQueryDragStatus())
                        // (lazy) drag currently in progress: stop
                        break;

                    if (pcdi)
                        // filter out whitespace
                        if (pcdi->pRecord)
                            cnrhInitDrag(pcdi->hwndCnr,
                                         pcdi->pRecord,
                                         usNotifyCode,
                                         DRAG_RMF,
                                         DO_MOVEABLE);

                break; } // case CN_INITDRAG

                /*
                 * CN_DRAGOVER:
                 *      file type being dragged over other file type
                 */

                case CN_DRAGOVER:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                    USHORT      usIndicator = DOR_NODROP,
                                    // cannot be dropped, but send
                                    // DM_DRAGOVER again
                                usOp = DO_UNKNOWN;
                                    // target-defined drop operation:
                                    // user operation (we don't want
                                    // the WPS to copy anything)

                    #ifdef DEBUG_ASSOCS
                    _Pmpf(("CN_DRAGOVER: entering"));
                    #endif

                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcdi->pDragInfo))
                    {
                        CheckFileTypeDrag(pftpd,
                                          pcdi->pDragInfo,
                                          (PFILETYPERECORD)pcdi->pRecord,   // target
                                          &usIndicator,
                                          &usOp);
                        DrgFreeDraginfo(pcdi->pDragInfo);
                    }

                    #ifdef DEBUG_ASSOCS
                    _Pmpf(("CN_DRAGOVER: returning ind 0x%lX, op 0x%lX", usIndicator, usOp));
                    #endif

                    // and return the drop flags
                    mrc = (MRFROM2SHORT(usIndicator, usOp));
                break; } // case CN_DRAGOVER

                /*
                 * CN_DROP:
                 *      file type being dropped
                 *      (both for modal d'n'd and non-modal lazy drag).
                 *
                 *      Must always return 0.
                 */

                case CN_DROP:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;

                    #ifdef DEBUG_ASSOCS
                    _Pmpf(("CN_DROP: entering"));
                    #endif

                    // check global valid recc, which was set above
                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcdi->pDragInfo))
                    {
                        if (CheckFileTypeDrag(pftpd,
                                              pcdi->pDragInfo,
                                              (PFILETYPERECORD)pcdi->pRecord,   // target
                                              NULL, NULL))
                        {
                            // valid operation:
                            // OK, move the record core tree to the
                            // new location.

                            // This is a bit unusual, because normally
                            // it is the source application which does
                            // the operation as a result of d'n'd. But
                            // who cares, source and target are the
                            // same window here anyway, so let's go.
                            PDRAGITEM   pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0);
                            if (pdrgItem)
                            {
                                PFILETYPERECORD precDropped = (PFILETYPERECORD)pdrgItem->ulItemID;
                                PFILETYPERECORD precTarget = (PFILETYPERECORD)pcdi->pRecord;
                                // update container
                                if (cnrhMoveTree(pcdi->pDragInfo->hwndSource,
                                                 // record to move:
                                                 (PRECORDCORE)precDropped,
                                                 // new parent (might be NULL for root level):
                                                 (PRECORDCORE)precTarget,
                                                 // sort function (cnrsort.c)
                                                 (PFNCNRSORT)fnCompareName))
                                {
                                    // update XWP type parents in OS2.INI
                                    PrfWriteProfileString(HINI_USER,
                                                          // application: "XWorkplace:FileTypes"
                                                          (PSZ)INIAPP_XWPFILETYPES,
                                                          // key --> the dragged record:
                                                          precDropped->recc.recc.pszIcon,
                                                          // string --> the parent:
                                                          (precTarget)
                                                              // the parent
                                                              ? precTarget->recc.recc.pszIcon
                                                              // NULL == root: delete key
                                                              : NULL);
                                            // aaarrgh

                                    ftypInvalidateCaches();
                                }
                            }
                            #ifdef DEBUG_ASSOCS
                            else
                                _Pmpf(("  Cannot get drag item"));
                            #endif
                        }

                        DrgFreeDraginfo(pcdi->pDragInfo);
                                    // V0.9.7 (2000-12-10) [umoeller]
                    }
                    #ifdef DEBUG_ASSOCS
                    else
                        _Pmpf(("  Cannot get draginfo"));
                    #endif

                    // If CN_DROP was the result of a "real" (modal) d'n'd,
                    // the DrgDrag function in CN_INITDRAG (above)
                    // returns now.

                    // If CN_DROP was the result of a lazy drag (pickup and drop),
                    // the container will now send CN_DROPNOTIFY (below).

                    // In both cases, we clean up the resources: either in
                    // CN_INITDRAG or in CN_DROPNOTIFY.

                    #ifdef DEBUG_ASSOCS
                    _Pmpf(("CN_DROP: returning"));
                    #endif

                break; } // case CN_DROP

                /*
                 * CN_DROPNOTIFY:
                 *      this is only sent to the container when
                 *      a lazy drag operation (pickup and drop)
                 *      is finished. Since lazy drag is non-modal
                 *      (DrgLazyDrag in CN_PICKUP returned immediately),
                 *      this is where we must clean up the resources
                 *      (the same as we have done in CN_INITDRAG modally).
                 *
                 *      According to PMREF, if a lazy drop was successful,
                 *      the target window is first sent DM_DROP and
                 *      the source window is is then posted DM_DROPNOTIFY.
                 *
                 *      The standard DM_DROPNOTIFY has the DRAGINFO in mp1
                 *      and the target window in mp2. With the container,
                 *      the target window goes into the CNRLAZYDRAGINFO
                 *      structure.
                 *
                 *      If a lazy drop is cancelled (e.g. for DrgCancelLazyDrag),
                 *      only the source window is posted DM_DROPNOTIFY, with
                 *      mp2 == NULLHANDLE.
                 */

                case CN_DROPNOTIFY:
                {
                    PCNRLAZYDRAGINFO pcldi = (PCNRLAZYDRAGINFO)ulExtra;

                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcldi->pDragInfo))
                    {
                        // get the moved record core
                        PDRAGITEM   pdrgItem = DrgQueryDragitemPtr(pcldi->pDragInfo, 0);

                        // remove record "pickup" emphasis
                        WinSendMsg(pcldi->pDragInfo->hwndSource, // hwndCnr
                                   CM_SETRECORDEMPHASIS,
                                   // record to move
                                   (MPARAM)(pdrgItem->ulItemID),
                                   MPFROM2SHORT(FALSE,
                                                CRA_PICKED));

                        // fixed V0.9.7 (2000-12-13) [umoeller]
                        DrgDeleteDraginfoStrHandles(pcldi->pDragInfo);
                        DrgFreeDraginfo(pcldi->pDragInfo);
                    }
                break; }

                /*
                 * CN_CONTEXTMENU:
                 *      ulExtra has the record core or NULL
                 *      if whitespace.
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu;

                    // get drag status
                    BOOL    fDragging = DrgQueryDragStatus();

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        // popup menu on container recc:
                        hPopupMenu = pftpd->hmenuFileTypeSel;

                        // if lazy drag is currently in progress,
                        // disable "Pickup" item (we can handle only one)
                        WinEnableMenuItem(hPopupMenu,
                                          ID_XSMI_FILETYPES_PICKUP,
                                          !fDragging);
                    }
                    else
                    {
                        // on whitespace: different menu
                        hPopupMenu = pftpd->hmenuFileTypeNoSel;

                        // already open: disable
                        WinEnableMenuItem(hPopupMenu,
                                          ID_XSMI_FILEFILTER_IMPORTWPS,
                                          (pftpd->hwndWPSImportDlg == NULLHANDLE));
                    }

                    // both menu types:
                    // disable drop and cancel-drag if
                    // no lazy drag in progress:
                    // V0.9.7 (2000-12-13) [umoeller]
                    WinEnableMenuItem(hPopupMenu,
                                      ID_XSMI_FILETYPES_DROP,
                                      fDragging);
                    // disable cancel-drag
                    WinEnableMenuItem(hPopupMenu,
                                      ID_XSMI_FILETYPES_CANCELDRAG,
                                      fDragging);
                    cnrhShowContextMenu(pcnbp->hwndControl,     // cnr
                                        (PRECORDCORE)pcnbp->preccSource,
                                        hPopupMenu,
                                        pcnbp->hwndDlgPage);    // owner
                break; }

                case CN_REALLOCPSZ:
                {
                    // rename of file type has ended V0.9.9 (2001-02-06) [umoeller]:
                    PCNREDITDATA pced = (PCNREDITDATA)ulExtra;
                    if (pced->pRecord)
                    {
                        // this was for a record (should always be the case):
                        // we must now allocate sufficient memory for the
                        // new file type...
                        // PCNREDITDATA->cbText has the memory that the cnr
                        // needs to copy the string now.
                        PFILETYPERECORD pRecord = (PFILETYPERECORD)pced->pRecord;
                        if (    (pced->cbText)
                             && (*(pced->ppszText))
                             && (strlen(*(pced->ppszText)))
                           )
                        {
                            pftpd->pszFileTypeOld = *(pced->ppszText);
                                        // this was allocated using malloc()
                            *(pced->ppszText) = malloc(pced->cbText);

                            mrc = (MPARAM)TRUE;
                        }
                    }
                break; }

                case CN_ENDEDIT:
                {
                    PCNREDITDATA pced = (PCNREDITDATA)ulExtra;
                    if (    (pced->pRecord)
                         && (pftpd->pszFileTypeOld)
                                // this is only != NULL if CN_REALLOCPSZ came in
                                // before, i.e. direct edit was not cancelled
                       )
                    {
                        PFILETYPERECORD pRecord = (PFILETYPERECORD)pced->pRecord;
                        PSZ     pszSet;
                        ULONG   ulMsg = 0;
                        PSZ     pszMsg;
                        APIRET arc = ftypRenameFileType(pftpd->pszFileTypeOld,
                                                        *(pced->ppszText));

                        switch (arc)
                        {
                            case ERROR_FILE_NOT_FOUND:
                                pszMsg = pftpd->pszFileTypeOld;
                                ulMsg = 209;
                            break;

                            case ERROR_FILE_EXISTS:
                                pszMsg = *(pced->ppszText);
                                ulMsg = 210;
                            break;
                        }

                        if (ulMsg)
                        {
                            cmnMessageBoxMsgExt(pcnbp->hwndDlgPage,
                                                104,        // error
                                                &pszMsg,
                                                1,
                                                ulMsg,
                                                MB_CANCEL);
                            pszSet = pftpd->pszFileTypeOld;
                        }
                        else
                            pszSet = *(pced->ppszText);

                        // now take care of memory management here...
                        // we must reset the text pointers in the FILETYPERECORD
                        pRecord->recc.recc.pszIcon =
                        pRecord->recc.recc.pszText =
                        pRecord->recc.recc.pszName =
                        pRecord->recc.recc.pszTree = pszSet;

                        // also point the list item to the new text
                        pRecord->pliFileType->pszFileType = pszSet;

                        if (ulMsg)
                        {
                            // error:
                            // invalidate the record
                            WinSendMsg(pftpd->hwndTypesCnr,
                                       CM_INVALIDATERECORD,
                                       (MPARAM)&pRecord,
                                       MPFROM2SHORT(1,
                                                    CMA_ERASE | CMA_TEXTCHANGED));
                            // and free new
                            free(*(pced->ppszText));
                        }
                        else
                            // no error: free old
                            free(pftpd->pszFileTypeOld);

                        pftpd->pszFileTypeOld = NULL;
                    }
                break; }

            } // end switch (usNotifyCode)

        break; } // case ID_XSDI_FT_CONTAINER

        /*
         * ID_XSMI_FILETYPES_DELETE:
         *      "Delete file type" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_DELETE:
        {
            PFILETYPERECORD pftrecc = (PFILETYPERECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (pftrecc)
            {
                // delete file type from INI
                PrfWriteProfileString(HINI_USER,
                                      (PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                                      pftrecc->recc.recc.pszIcon,
                                      NULL);     // delete
                // and remove record core from container
                WinSendMsg(pftpd->hwndTypesCnr,
                           CM_REMOVERECORD,
                           (MPARAM)&pftrecc,
                           MPFROM2SHORT(1,  // only one record
                                    CMA_FREE | CMA_INVALIDATE));
                ftypInvalidateCaches();
            }
        break; }

        /*
         * ID_XSMI_FILETYPES_NEW:
         *      "Create file type" context menu item
         *      (file types container tree); this can
         *      be both on whitespace and on a type record
         */

        case ID_XSMI_FILETYPES_NEW:
        {
            HWND hwndDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                                      pcnbp->hwndFrame,  // owner
                                      WinDefDlgProc,
                                      cmnQueryNLSModuleHandle(FALSE),
                                      ID_XSD_NEWFILETYPE,   // "New File Type" dlg
                                      NULL);            // pCreateParams
            if (hwndDlg)
            {
                winhSetEntryFieldLimit(WinWindowFromID(hwndDlg, ID_XSDI_FT_ENTRYFIELD),
                                       50);

                if (WinProcessDlg(hwndDlg) == DID_OK)
                {
                    // initial data for file type: 1 null-byte,
                    // meaning that no associations have been defined...
                    // get new file type name from dlg
                    PSZ pszNewType = winhQueryDlgItemText(hwndDlg,
                                                          ID_XSDI_FT_ENTRYFIELD);
                    if (pszNewType)
                    {
                        if (!CreateFileType(pftpd,
                                            pszNewType,
                                            (PFILETYPERECORD)pcnbp->preccSource))
                                                 // can be NULL
                        {
                            PSZ pTable = pszNewType;
                            cmnMessageBoxMsgExt(pcnbp->hwndFrame,  // owner
                                                104,        // xwp error
                                                &pTable,
                                                1,
                                                196,
                                                MB_OK);
                        }
                    }
                }
                WinDestroyWindow(hwndDlg);
            }

        break; }

        /*
         * ID_XSMI_FILETYPES_PICKUP:
         *      "Pickup file type" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_PICKUP:
        {
            if (    (pcnbp->preccSource)
                 && (pcnbp->preccSource != (PRECORDCORE)-1)
                 // lazy drag not currently in progress: V0.9.7 (2000-12-13) [umoeller]
                 && (!DrgQueryDragStatus())
               )
            {
                // initialize lazy drag just as if the
                // user had pressed Alt+MB2
                cnrhInitDrag(pftpd->hwndTypesCnr,
                             pcnbp->preccSource,
                             CN_PICKUP,
                             DRAG_RMF,
                             DO_MOVEABLE);
            }
        break; }

        /*
         * ID_XSMI_FILETYPES_DROP:
         *      "Drop file type" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_DROP:
        {
            DrgLazyDrop(pftpd->hwndTypesCnr,
                        DO_MOVE,        // fixed V0.9.7 (2000-12-13) [umoeller]
                        &(pcnbp->ptlMenuMousePos));
                            // this is the pointer position at
                            // the time the context menu was
                            // requested (in Desktop coordinates),
                            // which should be over the target record
                            // core or container whitespace
        break; }

        /*
         * ID_XSMI_FILETYPES_CANCELDRAG:
         *      "Cancel drag" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_CANCELDRAG:
        {
            // for one time, this is simple
            DrgCancelLazyDrag();
        break; }

        /*
         * ID_XSDI_FT_FILTERSCNR:
         *      "File filters" container
         */

        case ID_XSDI_FT_FILTERSCNR:
        {
            switch (usNotifyCode)
            {
                /*
                 * CN_CONTEXTMENU:
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu;

                    // get drag status
                    // BOOL    fDragging = DrgQueryDragStatus();

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        // popup menu on container recc:
                        hPopupMenu = pftpd->hmenuFileFilterSel;
                    }
                    else
                    {
                        // no selection: different menu
                        hPopupMenu = pftpd->hmenuFileFilterNoSel;
                        // already open: disable
                        WinEnableMenuItem(hPopupMenu,
                                          ID_XSMI_FILEFILTER_IMPORTWPS,
                                          (pftpd->hwndWPSImportDlg == NULLHANDLE));
                    }

                    cnrhShowContextMenu(pcnbp->hwndControl,
                                        (PRECORDCORE)pcnbp->preccSource,
                                        hPopupMenu,
                                        pcnbp->hwndDlgPage);    // owner
                break; }

            } // end switch (usNotifyCode)
        break; } // case ID_XSDI_FT_FILTERSCNR

        /*
         * ID_XSMI_FILEFILTER_DELETE:
         *      "Delete filter" context menu item
         *      (file filters container)
         */

        case ID_XSMI_FILEFILTER_DELETE:
        {
            ULONG       ulSelection = 0;

            PRECORDCORE precc = cnrhQuerySourceRecord(pftpd->hwndFiltersCnr,
                                                      pcnbp->preccSource,
                                                      &ulSelection);

            while (precc)
            {
                PRECORDCORE preccNext = 0;

                if (ulSelection == SEL_MULTISEL)
                    // get next record first, because we can't query
                    // this after the record has been removed
                    preccNext = cnrhQueryNextSelectedRecord(pftpd->hwndFiltersCnr,
                                                        precc);

                WinSendMsg(pftpd->hwndFiltersCnr,
                           CM_REMOVERECORD,
                           (MPARAM)&precc,
                           MPFROM2SHORT(1,  // only one record
                                    CMA_FREE | CMA_INVALIDATE));

                precc = preccNext;
                // NULL if none
            }

            WriteXWPFilters2INI(pftpd);
        break; }

        /*
         * ID_XSMI_FILEFILTER_NEW:
         *      "New filter" context menu item
         *      (file filters container)
         */

        case ID_XSMI_FILEFILTER_NEW:
        {
            HWND hwndDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                                      pcnbp->hwndFrame,  // owner
                                      WinDefDlgProc,
                                      cmnQueryNLSModuleHandle(FALSE),
                                      ID_XSD_NEWFILTER, // "New Filter" dlg
                                      NULL);            // pCreateParams
            if (hwndDlg)
            {
                WinSendDlgItemMsg(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                    EM_SETTEXTLIMIT,
                                    (MPARAM)50,
                                    MPNULL);
                if (WinProcessDlg(hwndDlg) == DID_OK)
                {
                    CHAR szNewFilter[50];
                    // get new file type name from dlg
                    WinQueryDlgItemText(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                    sizeof(szNewFilter)-1, szNewFilter);

                    AddFilter2Cnr(pftpd, szNewFilter);
                    WriteXWPFilters2INI(pftpd);
                }
                WinDestroyWindow(hwndDlg);
            }
        break; }

        /*
         * ID_XSMI_FILEFILTER_IMPORTWPS:
         *      "Import filter" context menu item
         *      (file filters container)
         */

        case ID_XSMI_FILEFILTER_IMPORTWPS:
        {
            if (pftpd->hwndWPSImportDlg == NULLHANDLE)
            {
                // dialog not presently open:
                pftpd->hwndWPSImportDlg = WinLoadDlg(
                               HWND_DESKTOP,     // parent
                               pcnbp->hwndFrame,  // owner
                               fnwpImportWPSFilters,
                               cmnQueryNLSModuleHandle(FALSE),
                               ID_XSD_IMPORTWPS, // "Import WPS Filters" dlg
                               pftpd);           // FILETYPESPAGEDATA for the dlg
            }
            WinShowWindow(pftpd->hwndWPSImportDlg, TRUE);
        break; }

        /*
         * ID_XSDI_FT_ASSOCSCNR:
         *      the "Associations" container, which can handle
         *      drag'n'drop.
         */

        case ID_XSDI_FT_ASSOCSCNR:
        {

            switch (usNotifyCode)
            {

                /*
                 * CN_INITDRAG:
                 *      file type being dragged (we don't support
                 *      lazy drag here, cos I'm too lazy)
                 */

                case CN_INITDRAG:
                {
                    PCNRDRAGINIT pcdi = (PCNRDRAGINIT)ulExtra;

                    if (pcdi)
                        // filter out whitespace
                        if (pcdi->pRecord)
                        {
                            // filter out disabled (parent) associations
                            if ((pcdi->pRecord->flRecordAttr & RECORD_DISABLED) == 0)
                                cnrhInitDrag(pcdi->hwndCnr,
                                                pcdi->pRecord,
                                                usNotifyCode,
                                                DRAG_RMF,
                                                DO_MOVEABLE);
                        }

                break; } // case CN_INITDRAG

                /*
                 * CN_DRAGAFTER:
                 *      something's being dragged over the "Assocs" cnr;
                 *      we allow dropping only for single WPProgram and
                 *      WPProgramFile objects or if one of the associations
                 *      is dragged _within_ the container.
                 *
                 *      Note that since we have set CA_ORDEREDTARGETEMPH
                 *      for the "Assocs" cnr, we do not get CN_DRAGOVER,
                 *      but CN_DRAGAFTER only.
                 */

                case CN_DRAGAFTER:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                    USHORT      usIndicator = DOR_NODROP,
                                    // cannot be dropped, but send
                                    // DM_DRAGOVER again
                                usOp = DO_UNKNOWN;
                                    // target-defined drop operation:
                                    // user operation (we don't want
                                    // the WPS to copy anything)

                    // reset global variable
                    pftpd->pobjDrop = NULL;

                    // OK so far:
                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcdi->pDragInfo))
                    {
                        if (
                                // accept no more than one single item at a time;
                                // we cannot move more than one file type
                                (pcdi->pDragInfo->cditem != 1)
                                // make sure that we only accept drops from ourselves,
                                // not from other windows
                            )
                        {
                            usIndicator = DOR_NEVERDROP;
                        }
                        else
                        {
                            // OK, we have exactly one item:

                            // 1)   is this a WPProgram or WPProgramFile from the WPS?
                            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0);

                            if (DrgVerifyRMF(pdrgItem, "DRM_OBJECT", NULL))
                            {
                                // the WPS stores the MINIRECORDCORE of the
                                // object in ulItemID of the DRAGITEM structure;
                                // we use OBJECT_FROM_PREC to get the SOM pointer
                                WPObject *pObject = OBJECT_FROM_PREC(pdrgItem->ulItemID);

                                if (pObject)
                                {
                                    // dereference shadows
                                    while (     (pObject)
                                             && (_somIsA(pObject, _WPShadow))
                                          )
                                        pObject = _wpQueryShadowedObject(pObject,
                                                            TRUE);  // lock

                                    if (    (_somIsA(pObject, _WPProgram))
                                         || (_somIsA(pObject, _WPProgramFile))
                                       )
                                    {
                                        usIndicator = DOR_DROP;

                                        // store dragged object
                                        pftpd->pobjDrop = pObject;
                                        // store record after which to insert
                                        pftpd->preccAfter = pcdi->pRecord;
                                        if (pftpd->preccAfter == NULL)
                                            // if cnr whitespace: make last
                                            pftpd->preccAfter = (PRECORDCORE)CMA_END;
                                    }
                                }
                            } // end if (DrgVerifyRMF(pdrgItem, "DRM_OBJECT", NULL))

                            if (pftpd->pobjDrop == NULL)
                            {
                                // no object found yet:
                                // 2)   is this a record being dragged within our
                                //      container?
                                if (pcdi->pDragInfo->hwndSource != pftpd->hwndAssocsCnr)
                                    usIndicator = DOR_NEVERDROP;
                                else
                                {
                                    if (    (pcdi->pDragInfo->usOperation == DO_DEFAULT)
                                         || (pcdi->pDragInfo->usOperation == DO_MOVE)
                                       )
                                    {
                                        // do not allow drag upon whitespace,
                                        // but only between records
                                        if (pcdi->pRecord)
                                        {
                                            if (   (pcdi->pRecord == (PRECORDCORE)CMA_FIRST)
                                                || (pcdi->pRecord == (PRECORDCORE)CMA_LAST)
                                               )
                                                // allow drop
                                                usIndicator = DOR_DROP;

                                            // do not allow dropping after
                                            // disabled records
                                            else if ((pcdi->pRecord->flRecordAttr & RECORD_DISABLED) == 0)
                                                usIndicator = DOR_DROP;

                                            if (usIndicator == DOR_DROP)
                                            {
                                                pftpd->pobjDrop = (WPObject*)-1;
                                                        // ugly special flag: this is no
                                                        // object, but our own record core
                                                // store record after which to insert
                                                pftpd->preccAfter = pcdi->pRecord;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        DrgFreeDraginfo(pcdi->pDragInfo);
                    }

                    // and return the drop flags
                    mrc = (MRFROM2SHORT(usIndicator, usOp));
                break; }

                /*
                 * CN_DROP:
                 *      something _has_ now been dropped on the cnr.
                 */

                case CN_DROP:
                {
                    // check the global variable which has been set
                    // by CN_DRAGOVER above:
                    if (pftpd->pobjDrop)
                    {
                        // CN_DRAGOVER above has considered this valid:

                        if (pftpd->pobjDrop == (WPObject*)-1)
                        {
                            // special flag for record being dragged within
                            // the container:

                            // get access to the drag'n'drop structures
                            PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                            if (DrgAccessDraginfo(pcdi->pDragInfo))
                            {
                                // OK, move the record core tree to the
                                // new location.

                                // This is a bit unusual, because normally
                                // it is the source application which does
                                // the operation as a result of d'n'd. But
                                // who cares, source and target are the
                                // same window here anyway, so let's go.
                                PDRAGITEM   pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0);

                                // update container
                                PRECORDCORE preccMove = (PRECORDCORE)pdrgItem->ulItemID;
                                cnrhMoveRecord(pftpd->hwndAssocsCnr,
                                               preccMove,
                                               pftpd->preccAfter);

                                DrgFreeDraginfo(pcdi->pDragInfo);
                                            // V0.9.7 (2000-12-10) [umoeller]
                            }
                        }
                        else
                        {
                            // WPProgram or WPProgramFile:
                            AddAssocObject2Cnr(pftpd->hwndAssocsCnr,
                                               pftpd->pobjDrop,
                                               pftpd->preccAfter,
                                                    // record to insert after; has been set by CN_DRAGOVER
                                               TRUE);
                                                    // enable record
                            cnrhInvalidateAll(pftpd->hwndAssocsCnr);
                        }

                        // write the new associations to INI
                        WriteAssocs2INI((PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                                        pftpd->hwndTypesCnr,
                                        pftpd->hwndAssocsCnr);
                        pftpd->pobjDrop = NULL;
                    }
                break; }

                /*
                 * CN_CONTEXTMENU ("Associations" container):
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu;
                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;

                    if (ulExtra)
                    {
                        // popup menu on record core:
                        hPopupMenu = pftpd->hmenuFileAssocSel;
                        // association from parent file type:
                        // do not allow deletion
                        WinEnableMenuItem(hPopupMenu,
                                          ID_XSMI_FILEASSOC_DELETE,
                                          ((pcnbp->preccSource->flRecordAttr & CRA_DISABLED) == 0));
                    }
                    else
                    {
                        // on whitespace: different menu
                        hPopupMenu = pftpd->hmenuFileAssocNoSel;

                        // already open: disable
                        WinEnableMenuItem(hPopupMenu,
                                          ID_XSMI_FILEFILTER_IMPORTWPS,
                                          (pftpd->hwndWPSImportDlg == NULLHANDLE));
                    }

                    cnrhShowContextMenu(pcnbp->hwndControl,
                                        (PRECORDCORE)pcnbp->preccSource,
                                        hPopupMenu,
                                        pcnbp->hwndDlgPage);    // owner
                break; }
            } // end switch (usNotifyCode)

        break; }

        /*
         * ID_XSMI_FILEASSOC_DELETE:
         *      "Delete association" context menu item
         *      (file association container)
         */

        case ID_XSMI_FILEASSOC_DELETE:
        {
            ULONG       ulSelection = 0;

            PRECORDCORE precc = cnrhQuerySourceRecord(pftpd->hwndAssocsCnr,
                                                      pcnbp->preccSource,
                                                      &ulSelection);

            while (precc)
            {
                PRECORDCORE preccNext = 0;

                if (ulSelection == SEL_MULTISEL)
                    // get next record first, because we can't query
                    // this after the record has been removed
                    preccNext = cnrhQueryNextSelectedRecord(pftpd->hwndAssocsCnr,
                                                        precc);

                WinSendMsg(pftpd->hwndAssocsCnr,
                           CM_REMOVERECORD,
                           (MPARAM)&precc,
                           MPFROM2SHORT(1,  // only one record
                                    CMA_FREE | CMA_INVALIDATE));

                precc = preccNext;
                // NULL if none
            }

            // write the new associations to INI
            WriteAssocs2INI((PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                            pftpd->hwndTypesCnr,
                            pftpd->hwndAssocsCnr);
        break; }

    }

    return (mrc);
}

/*
 *@@ FillListboxWithWPSFilters:
 *      fills the "filters" listbox in the "import" dlg
 *      with the wps filters.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

VOID FillListboxWithWPSFilters(HWND hwndDlg)
{
    HPOINTER hptrOld = winhSetWaitPointer();

    if (ftypLockCaches())
    {
        HWND        hwndListBox = WinWindowFromID(hwndDlg,
                                                  ID_XSDI_FT_FILTERLIST);

        PSZ         pszAssocsList = prfhQueryKeysForApp(HINI_USER,
                                                      WPINIAPP_ASSOCFILTER);
                                                            // "PMWP_ASSOC_FILTER"

        BOOL        fUnknownOnly = winhIsDlgItemChecked(hwndDlg,
                                                        ID_XSDI_FT_UNKNOWNONLY);

        PLINKLIST   pllXWPTypesWithFilters = ftypGetCachedTypesWithFilters();

        WinSendMsg(hwndListBox,
                   LM_DELETEALL,
                   0, 0);

        if (pszAssocsList)
        {
            PSZ         pAssocThis = pszAssocsList;
            // add each WPS filter to the list box
            while (*pAssocThis != 0)
            {
                BOOL    fInsert = TRUE;
                ULONG   ulSize = 0;
                // insert the list box item only if
                // a) associations have been defined _and_
                // b) the filter has not been assigned to a
                //    file type yet

                // a) check WPS filters
                if (!PrfQueryProfileSize(HINI_USER,
                                         (PSZ)WPINIAPP_ASSOCFILTER,  // "PMWP_ASSOC_FILTER"
                                         pAssocThis,
                                         &ulSize))
                    fInsert = FALSE;
                else if (ulSize < 2)
                    // a length of 1 is the NULL byte == no assocs
                    fInsert = FALSE;

                if ((fInsert) && (fUnknownOnly))
                {
                    // b) now check XWorkplace filters
                    // V0.9.9 (2001-02-06) [umoeller]
                    PLISTNODE pNode = lstQueryFirstNode(pllXWPTypesWithFilters);
                    while (pNode)
                    {
                        PXWPTYPEWITHFILTERS pTypeWithFilters
                            = (PXWPTYPEWITHFILTERS)pNode->pItemData;

                        // second loop: thru all filters for this file type
                        PSZ pFilterThis = pTypeWithFilters->pszFilters;
                        while (*pFilterThis != 0)
                        {
                            // check if this matches the data file name
                            if (!strcmp(pFilterThis, pAssocThis))
                            {
                                fInsert = FALSE;
                                break;
                            }

                            pFilterThis += strlen(pFilterThis) + 1;   // next type/filter
                        } // end while (*pFilterThis)

                        pNode = pNode->pNext;
                    }
                }

                if (fInsert)
                    WinInsertLboxItem(hwndListBox,
                                      LIT_SORTASCENDING,
                                      pAssocThis);

                // go for next key
                pAssocThis += strlen(pAssocThis)+1;
            }

            free(pszAssocsList);

        } // end if (pszAssocsList)

        ftypUnlockCaches();
    }

    WinSetPointer(HWND_DESKTOP, hptrOld);
}

/*
 * fnwpImportWPSFilters:
 *      dialog procedure for the "Import WPS Filters" dialog,
 *      which gets loaded upon ID_XSMI_FILEFILTER_IMPORTWPS from
 *      ftypFileTypesItemChanged.
 *
 *      This needs the PFILETYPESPAGEDATA from the notebook
 *      page as a pCreateParam in WinLoadDlg (passed to
 *      WM_INITDLG here).
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.6 (2000-11-08) [umoeller]: fixed "Close" behavior
 *@@changed V0.9.9 (2001-02-06) [umoeller]: setting proper fonts now
 */

MRESULT EXPENTRY fnwpImportWPSFilters(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)WinQueryWindowPtr(hwndDlg,
                                                                     QWL_USER);

    switch (msg)
    {
        /*
         * WM_INITDLG:
         *      mp2 has the PFILETYPESPAGEDATA
         */

        case WM_INITDLG:
        {
            pftpd = (PFILETYPESPAGEDATA)mp2;
            WinSetWindowPtr(hwndDlg, QWL_USER, pftpd);

            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TEXT | CA_ORDEREDTARGETEMPH);
            } END_CNRINFO(WinWindowFromID(hwndDlg,
                                          ID_XSDI_FT_ASSOCSCNR));

            cmnSetControlsFont(hwndDlg,
                               1,
                               3000);

            if (pftpd)
            {
                // 1) set the "Merge with" text to
                //    the selected record core

                WinSetDlgItemText(hwndDlg,
                                  ID_XSDI_FT_TYPE,
                                  pftpd->pftreccSelected->recc.recc.pszIcon);

                // 2) fill the left list box with the
                //    WPS-defined filters
                FillListboxWithWPSFilters(hwndDlg);

            } // end if (pftpd)

            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
        {
            USHORT usItemID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);

            switch (usItemID)
            {
                // "Filters" listbox
                case ID_XSDI_FT_FILTERLIST:
                {
                    if (usNotifyCode == LN_SELECT)
                    {
                        // selection changed on the left: fill right container

                        HWND    hwndFiltersListBox = WinWindowFromID(hwndDlg,
                                                              ID_XSDI_FT_FILTERLIST),
                                hwndAssocsCnr     =  WinWindowFromID(hwndDlg,
                                                              ID_XSDI_FT_ASSOCSCNR);
                        SHORT   sItemSelected = LIT_FIRST;
                        CHAR    szFilterText[200];
                        BOOL    fFirstRun = TRUE;

                        HPOINTER hptrOld = winhSetWaitPointer();

                        // OK, something has been selected in
                        // the "Filters" list box.
                        // Since we support multiple selections,
                        // we go thru all the selected items
                        // and add the corresponding filters
                        // to the right container.

                        // WinEnableWindowUpdate(hwndAssocsListBox, FALSE);

                        while (TRUE)
                        {
                            sItemSelected = (SHORT)WinSendMsg(hwndFiltersListBox,
                                       LM_QUERYSELECTION,
                                       (MPARAM)sItemSelected,  // initially LIT_FIRST
                                       MPNULL);

                            if (sItemSelected == LIT_NONE)
                                // no more selections:
                                break;
                            else
                            {
                                WinSendMsg(hwndFiltersListBox,
                                           LM_QUERYITEMTEXT,
                                           MPFROM2SHORT(sItemSelected,
                                                    sizeof(szFilterText)-1),
                                           (MPARAM)szFilterText);

                                UpdateAssocsCnr(hwndAssocsCnr,
                                                        // cnr to update
                                                szFilterText,
                                                        // file filter to search for
                                                (PSZ)WPINIAPP_ASSOCFILTER,
                                                        // "PMWP_ASSOC_FILTER"
                                                fFirstRun,
                                                        // empty container the first time
                                                TRUE);
                                                        // enable all records

                                // winhLboxSelectAll(hwndAssocsListBox, TRUE);

                                fFirstRun = FALSE;
                            }
                        } // end while (TRUE);

                        WinSetPointer(HWND_DESKTOP, hptrOld);
                    }
                break; }

                case ID_XSDI_FT_UNKNOWNONLY:
                    if (    (usNotifyCode == BN_CLICKED)
                         || (usNotifyCode == BN_DBLCLICKED) // added V0.9.0
                       )
                        // refresh the filters listbox
                        FillListboxWithWPSFilters(hwndDlg);
                break;
            } // end switch (usItemID)

        break; } // case case WM_CONTROL

        /*
         * WM_COMMAND:
         *      process buttons pressed
         */

        case WM_COMMAND:
        {
            SHORT usCmd = SHORT1FROMMP(mp1);

            switch (usCmd)
            {
                /*
                 * DID_OK:
                 *      "Merge" button
                 */

                case DID_OK:
                {
                    HWND    hwndFiltersListBox = WinWindowFromID(hwndDlg,
                                                          ID_XSDI_FT_FILTERLIST),
                            hwndAssocsCnr     =  WinWindowFromID(hwndDlg,
                                                          ID_XSDI_FT_ASSOCSCNR);
                    SHORT   sItemSelected = LIT_FIRST;
                    CHAR    szFilterText[200];
                    PASSOCRECORD preccThis = (PASSOCRECORD)CMA_FIRST;

                    // 1)  copy the selected filters in the
                    //     "Filters" listbox to the "Filters"
                    //     container on the notebook page

                    while (TRUE)
                    {
                        sItemSelected = (SHORT)WinSendMsg(hwndFiltersListBox,
                                   LM_QUERYSELECTION,
                                   (MPARAM)sItemSelected,  // initially LIT_FIRST
                                   MPNULL);

                        if (sItemSelected == LIT_NONE)
                            break;
                        else
                        {
                            // copy filter from list box to container
                            WinSendMsg(hwndFiltersListBox,
                                       LM_QUERYITEMTEXT,
                                       MPFROM2SHORT(sItemSelected,
                                                sizeof(szFilterText)-1),
                                       (MPARAM)szFilterText);
                            AddFilter2Cnr(pftpd, szFilterText);
                        }
                    } // end while (TRUE);

                    // write the new filters to OS2.INI
                    WriteXWPFilters2INI(pftpd);

                    // 2)  copy all the selected container items from
                    //     the "Associations" container in the dialog
                    //     to the one on the notebook page

                    while (TRUE)
                    {
                        // get first or next selected record
                        preccThis = (PASSOCRECORD)WinSendMsg(hwndAssocsCnr,
                                            CM_QUERYRECORDEMPHASIS,
                                            (MPARAM)preccThis,
                                            (MPARAM)CRA_SELECTED);

                        if ((preccThis == 0) || ((LONG)preccThis == -1))
                            // last or error
                            break;
                        else
                        {
                            // get object from handle
                            WPObject *pobjAssoc = _wpclsQueryObject(_WPObject,
                                                                    preccThis->hobj);

                            if (pobjAssoc)
                                // add the object to the notebook list box
                                AddAssocObject2Cnr(pftpd->hwndAssocsCnr,
                                                   pobjAssoc,
                                                   (PRECORDCORE)CMA_END,
                                                   TRUE);
                        }
                    } // end while (TRUE);

                    cnrhInvalidateAll(pftpd->hwndAssocsCnr);

                    // write the new associations to INI
                    WriteAssocs2INI((PSZ)WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                                    pftpd->hwndTypesCnr,
                                        // for selected record core
                                    pftpd->hwndAssocsCnr);
                                        // from updated list box
                break; } // case DID_OK

                case ID_XSDI_FT_SELALL:
                case ID_XSDI_FT_DESELALL:
                    cnrhSelectAll(WinWindowFromID(hwndDlg,
                                                  ID_XSDI_FT_ASSOCSCNR),
                                     (usCmd == ID_XSDI_FT_SELALL)); // TRUE = select
                break;

                case DID_CANCEL:
                    // "close" button:
                    WinPostMsg(hwndDlg,
                               WM_SYSCOMMAND,
                               (MPARAM)SC_CLOSE,
                               MPFROM2SHORT(CMDSRC_PUSHBUTTON,
                                            TRUE));  // pointer (who cares)
                break;

                /* default:
                    // this includes "Close"
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2); */
            } // end switch (usCmd)

        break; } // case WM_CONTROL

        case WM_HELP:
            // display help using the "Workplace Shell" SOM object
            // (in CREATENOTEBOOKPAGE structure)
            cmnDisplayHelp(pftpd->pcnbp->somSelf,
                           pftpd->pcnbp->ulDefaultHelpPanel + 1);
                            // help panel which follows the one on the main page
        break;

        case WM_SYSCOMMAND:
            switch ((ULONG)mp1)
            {
                case SC_CLOSE:
                    WinDestroyWindow(hwndDlg);
                    mrc = 0;
                break;

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            }
        break;

        case WM_DESTROY:
            pftpd->hwndWPSImportDlg = NULLHANDLE;
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XFldDataFile notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ G_ampDatafileTypesPage:
 *      resizing information for data file "File types" page.
 *      Stored in CREATENOTEBOOKPAGE of the
 *      respective "add notebook page" method.
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

MPARAM G_ampDatafileTypesPage[] =
    {
        MPFROM2SHORT(ID_XSDI_DATAF_AVAILABLE_CNR, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_XSDI_DATAF_GROUP, XAC_SIZEX | XAC_SIZEY)
    };

extern MPARAM *G_pampDatafileTypesPage = G_ampDatafileTypesPage;
extern ULONG G_cDatafileTypesPage = ARRAYITEMCOUNT(G_ampDatafileTypesPage);

/*
 *@@ DATAFILETYPESPAGE:
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

typedef struct _DATAFILETYPESPAGE
{
    // linked list of file types (linklist.c);
    // this contains FILETYPELISTITEM structs
    LINKLIST llAvailableTypes;           // auto-free

    // controls which are used all the time
    HWND    hwndTypesCnr;

    // backup of explicit types (for Undo)
    PSZ     pszTypesBackup;

} DATAFILETYPESPAGE, *PDATAFILETYPESPAGE;

/*
 *@@ ftypDatafileTypesInitPage:
 *      notebook callback function (notebook.c) for the
 *      "File types" page in a data file instance settings notebook.
 *      Initializes and fills the page.
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

VOID ftypDatafileTypesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags)
{
    if (flFlags & CBI_INIT)
    {
        PDATAFILETYPESPAGE pdftp = (PDATAFILETYPESPAGE)malloc(sizeof(DATAFILETYPESPAGE));
        if (pdftp)
        {
            PSZ     pszTypes = _wpQueryType(pcnbp->somSelf);

            memset(pdftp, 0, sizeof(*pdftp));
            pcnbp->pUser = pdftp;

            // backup existing types for "Undo"
            if (pszTypes)
                pdftp->pszTypesBackup = strdup(pszTypes);

            lstInit(&pdftp->llAvailableTypes, TRUE);     // auto-free

            pdftp->hwndTypesCnr = WinWindowFromID(pcnbp->hwndDlgPage,
                                                  ID_XSDI_DATAF_AVAILABLE_CNR);

            ctlMakeCheckboxContainer(pcnbp->hwndDlgPage,
                                     ID_XSDI_DATAF_AVAILABLE_CNR);
                    // this switches to tree view etc., but
                    // we need to override some settings
            BEGIN_CNRINFO()
            {
                cnrhSetSortFunc(fnCompareName);
            } END_CNRINFO(pdftp->hwndTypesCnr);
        }
    }

    if (flFlags & CBI_SET)
    {
        PDATAFILETYPESPAGE pdftp = (PDATAFILETYPESPAGE)pcnbp->pUser;
        if (pdftp)
        {
            // build list of explicit types to be passed
            // to FillCnrWithAvailableTypes; all the types
            // on this list will be CHECKED
            PLINKLIST pllExplicitTypes = NULL,
                      pllAutomaticTypes = lstCreate(TRUE);
            PSZ pszTypes = _wpQueryType(pcnbp->somSelf);
            if (pszTypes)
            {
                pllExplicitTypes = lstCreate(TRUE);
                AppendTypesFromString(pszTypes,
                                      pllExplicitTypes);
            }

            // build list of automatic types;
            // all these records will be DISABLED
            // in the container
            AppendTypesForFile(_wpQueryTitle(pcnbp->somSelf),
                               pllAutomaticTypes);

            // clear container and memory
            ClearAvailableTypes(pdftp->hwndTypesCnr,
                                &pdftp->llAvailableTypes);
            // insert all records and check/disable them in one flush
            FillCnrWithAvailableTypes(pdftp->hwndTypesCnr,
                                      &pdftp->llAvailableTypes,
                                      pllExplicitTypes,  // check list
                                      pllAutomaticTypes);        // disable list

            if (pllExplicitTypes)
                lstFree(pllExplicitTypes);
            lstFree(pllAutomaticTypes);
        }
    }

    if (flFlags & CBI_DESTROY)
    {
        PDATAFILETYPESPAGE pdftp = (PDATAFILETYPESPAGE)pcnbp->pUser;
        if (pdftp)
        {
            ClearAvailableTypes(pdftp->hwndTypesCnr,
                                &pdftp->llAvailableTypes);

            if (pdftp->pszTypesBackup)
            {
                free(pdftp->pszTypesBackup);
                pdftp->pszTypesBackup = NULL;
            }
        }
    }
}

/*
 *@@ ftypDatafileTypesItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "File types" page in a data file instance settings notebook.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

MRESULT ftypDatafileTypesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID,
                                     USHORT usNotifyCode,
                                     ULONG ulExtra)
{
    MRESULT mrc = 0;

    switch (ulItemID)
    {
        case ID_XSDI_DATAF_AVAILABLE_CNR:
            if (usNotifyCode == CN_RECORDCHECKED)
            {
                PFILETYPERECORD precc = (PFILETYPERECORD)ulExtra;

                // get existing types
                PSZ pszTypes = _wpQueryType(pcnbp->somSelf);

                if (precc->recc.usCheckState)
                {
                    // checked -> type added:
                    if (pszTypes)
                    {
                        // explicit types exist already:
                        // append a new one
                        PSZ pszNew = (PSZ)malloc(   strlen(pszTypes)
                                                  + 1       // for \n
                                                  + strlen(precc->pliFileType->pszFileType)
                                                  + 1);     // for \0
                        if (pszNew)
                        {
                            sprintf(pszNew,
                                    "%s\n%s",
                                    pszTypes,
                                    precc->pliFileType->pszFileType);

                            _wpSetType(pcnbp->somSelf, pszNew, 0);

                            free(pszNew);
                        }
                    }
                    else
                        // no explicit types yet:
                        // just set this new type as the only one
                        _wpSetType(pcnbp->somSelf, precc->pliFileType->pszFileType, 0);
                }
                else
                {
                    // unchecked -> type removed:
                    if (pszTypes)
                    {
                        if (strchr(pszTypes, '\n'))
                        {
                            // we have more than one type:
                            XSTRING str;
                            ULONG ulOfs = 0;
                            xstrInitCopy(&str, pszTypes, 0);
                            // remove this type
                            xstrFindReplaceC(&str,
                                             &ulOfs,
                                             precc->pliFileType->pszFileType,
                                             "");
                            // since the types are separated with \n if there
                            // are several, we now might have a duplicate \n\n
                            // if there was more than one type
                            ulOfs = 0;
                            xstrFindReplaceC(&str,
                                             &ulOfs,
                                             "\n\n",
                                             "\n");
                            _wpSetType(pcnbp->somSelf, str.psz, 0);
                            xstrClear(&str);
                        }
                        else
                            // we had only one type:
                            _wpSetType(pcnbp->somSelf, NULL, 0);
                    }
                }
            }
        break;

        case DID_UNDO:
        {
            PDATAFILETYPESPAGE pdftp = (PDATAFILETYPESPAGE)malloc(sizeof(DATAFILETYPESPAGE));
            if (pdftp)
            {
                // set type to what was saved on init
                _wpSetType(pcnbp->somSelf, pdftp->pszTypesBackup, 0);
                // call "init" callback to reinitialize the page
                pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            }
        break; }

        case DID_DEFAULT:
            // kill all explicit types
            _wpSetType(pcnbp->somSelf, NULL, 0);
            // call "init" callback to reinitialize the page
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

    } // end switch (ulItemID)

    return (mrc);
}

/* ******************************************************************
 *
 *   XWPProgram/XFldProgramFile notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ ftypAssociationsInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Associations" page in program settings notebooks.
 *      Sets the controls on the page according to the
 *      file types and instance settings.
 *
 *      Note that this is shared between XWPProgram and
 *      XFldProgramFile.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

VOID ftypAssociationsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                              ULONG flFlags)        // CBI_* flags (notebook.h)
{
}

/*
 *@@ ftypAssociationsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Associations" page in program settings notebooks.
 *      Reacts to changes of any of the dialog controls.
 *
 *      Note that this is shared between XWPProgram and
 *      XFldProgramFile.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

MRESULT ftypAssociationsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    ULONG ulItemID,
                                    USHORT usNotifyCode,
                                    ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;

    return (mrc);
}

