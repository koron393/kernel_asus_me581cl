/*************************************************************************/ /*!
@File
@Title          Resource Manager
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description	Provide resource management
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "resman.h"
#include "allocmem.h"
#include "pvr_debug.h"
#include "pvrsrv.h"
#include "osfunc.h"

/* use mutex here if required */
#define ACQUIRE_SYNC_OBJ
#define RELEASE_SYNC_OBJ


#define RESMAN_SIGNATURE 0x12345678

#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/async.h>
#define MIN_ON_DUTY_INTERVAL        300000 /* in ms, 5min by default */
#define LAZYFREE_RETRY_WAIT_PERIOD  100 /* in ms, 100ms by default */

#define ONE_FRAME_TIME_NS_U64       (IMG_UINT64)(1000.0f/60.0f/0.000001)
#define _BOUNDARY_ALIGN(ts)         (ts > (ONE_FRAME_TIME_NS_U64 >> 1)? \
                                    (ONE_FRAME_TIME_NS_U64 >> 2): ts)

/* in ns, 3ms by default; if lgt 1/2 frame time, take 1/4 frame time */
#define RESFREE_YIELD_TIMESLICE     (_BOUNDARY_ALIGN(3000000))

static LIST_HEAD(ResManQueue);
static LIST_HEAD(AsyncPageList);
static DEFINE_MUTEX(ResManLock);
static struct task_struct *pResManLazyFreeThread = NULL;

/******************************************************************************
 * resman structures
 *****************************************************************************/

/* resman item structure */
typedef struct _RESMAN_ITEM_
{
	IMG_UINT32				ui32Signature;

	struct _RESMAN_ITEM_	**ppsThis;	/*!< list navigation */
	struct _RESMAN_ITEM_	*psNext;	/*!< list navigation */

	IMG_UINT32				ui32ResType;/*!< res type */
	IMG_PVOID				pvParam;	/*!< param for callback */
	RESMAN_FREE_FN			pfnFreeResource;/*!< resman item free callback */
} RESMAN_ITEM;


/* resman context structure */
typedef struct _RESMAN_CONTEXT_
{

	IMG_UINT32					ui32Signature;
	PRESMAN_DEFER_CONTEXT		psDeferContext;	/*!< Defer context to which the the DeferResManContext should be added */

	struct	_RESMAN_CONTEXT_	**ppsThis;/*!< list navigation */
	struct	_RESMAN_CONTEXT_	*psNext;/*!< list navigation */

	struct list_head			ResManList;

	RESMAN_ITEM					*psResItemList;/*!< res item list for context */

} RESMAN_CONTEXT;

typedef struct _RESMAN_DEFER_CONTEXT_
{
	IMG_UINT32					ui32Signature;
	IMG_HANDLE					hCleanupEventObject;      /*!< used to trigger deferred clean-up when it is required */

	RESMAN_CONTEXT				*psDeferResManContextList; /*!< list of contexts for deferred clean-up */
} RESMAN_DEFER_CONTEXT;

/* resman list structure */
typedef struct
{
	RESMAN_CONTEXT	*psContextList; /*!< resman context list */

} RESMAN_LIST, *PRESMAN_LIST;	/* PRQA S 3205 */

/* list of page to be free'd */
typedef struct _ASYNC_PAGE_
{
    struct list_head            PageList;
    struct page*                psPageToBeFreed;
    async_func_t                AsyncFunc;

} ASYNC_PAGE;

#include "lists.h"	/* PRQA S 5087 */ /* include lists.h required here */

static IMPLEMENT_LIST_ANY_VA(RESMAN_ITEM)
static IMPLEMENT_LIST_ANY_VA_2(RESMAN_ITEM, IMG_BOOL, IMG_FALSE)
static IMPLEMENT_LIST_INSERT(RESMAN_ITEM)
static IMPLEMENT_LIST_REMOVE(RESMAN_ITEM)

static IMPLEMENT_LIST_FOR_EACH_SAFE(RESMAN_CONTEXT)
static IMPLEMENT_LIST_REMOVE(RESMAN_CONTEXT)
static IMPLEMENT_LIST_INSERT(RESMAN_CONTEXT)

/******************************************************** Forword references */

static PVRSRV_ERROR FreeResourceByPtr(RESMAN_ITEM *psItem);

static PVRSRV_ERROR FreeResourceByCriteria(PRESMAN_CONTEXT	psContext,
										   IMG_UINT32		ui32SearchCriteria,
										   IMG_UINT32		ui32ResType,
										   IMG_PVOID		pvParam,
										   IMG_BOOL			bDefer);

static IMG_VOID ResManFreeResources(PRESMAN_CONTEXT psResManContext,
									IMG_BOOL bDefer);

static IMG_VOID ResManFreeResourcesAsLazyBone(PRESMAN_CONTEXT psResManContext,
                                              IMG_BOOL bDefer);

/*!
******************************************************************************

 @Function	ResManInit

 @Description initialises the resman

 @Return   none

******************************************************************************/
PVRSRV_ERROR ResManInit(IMG_VOID)
{
	return PVRSRV_OK;
}


/*!
******************************************************************************

 @Function	ResManDeInit

 @Description de-initialises the resman

 @Return   none

******************************************************************************/
IMG_VOID ResManDeInit(IMG_VOID)
{
}


/*!
******************************************************************************

 @Function	PVRSRVResManConnect

 @Description Opens a connection to the Resource Manager

 @output 	phResManContext - Resman context

 @Return    error code or PVRSRV_OK

******************************************************************************/
PVRSRV_ERROR PVRSRVResManConnect(PRESMAN_DEFER_CONTEXT psDeferContext,
								 PRESMAN_CONTEXT *phResManContext)
{
	PRESMAN_CONTEXT	psResManContext;

	/* Allocate memory for the new context. */
	psResManContext	= OSAllocMem(sizeof(*psResManContext));
	if (psResManContext == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVResManConnect: ERROR allocating new RESMAN context struct"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psResManContext->ui32Signature = RESMAN_SIGNATURE;
	psResManContext->psResItemList	= IMG_NULL;
	psResManContext->psDeferContext = psDeferContext;

	/*Acquire resource list sync object*/
	ACQUIRE_SYNC_OBJ;

	/* Release resource list sync object */
	RELEASE_SYNC_OBJ;

	*phResManContext = psResManContext;

	return PVRSRV_OK;
}

/*!
******************************************************************************

 @Function	FreeAsyncPages

 @Description Change page attributes (wc/wb pair) and free asynchronously

 @input 	IMG_VOID

 @Return	IMG_VOID

******************************************************************************/
static IMG_VOID FreeAsyncPages(IMG_VOID) {
    ASYNC_PAGE* psAsyncPage;

    /* lazy to change page attr. then free it asynchronously */
    do {
        mutex_lock(&ResManLock);

        psAsyncPage =
            list_first_or_null_rcu(&AsyncPageList,
                                   ASYNC_PAGE,
                                   PageList
                                  );
        if (psAsyncPage)
            list_del_rcu(&psAsyncPage->PageList);

        mutex_unlock(&ResManLock);

        /* no page to be free'd asynchronously */
        if (!psAsyncPage)
            break;

        /* no reader would access, comment it for overhead sake on each page free */
        //synchronize_rcu_expedited();

        /* carry out asynchronous free on page */
        psAsyncPage->AsyncFunc(psAsyncPage->psPageToBeFreed, ULLONG_MAX);

        kfree(psAsyncPage);

    } while (IMG_TRUE);
}

/*!
******************************************************************************

 @Function	ResManLazyFreeThread

 @Description LazyFree thread main routine

 @input 	data - not used

 @Return	IMG_VOID

******************************************************************************/
static int ResManLazyFreeThread(void* data) {
    PRESMAN_CONTEXT psResManContext;
    PRESMAN_CONTEXT psFirstRetryResManContext = IMG_NULL;

    /* definitely as a lazy bone to free context.
     *
     * To walk thru and free all context is seen pretty time-consumed.
     * Try to be the most lazy one to free resources.
     */
    set_user_nice(current, 19);

    do {
        if (unlikely(IS_ERR(pResManLazyFreeThread) ||
                     !pResManLazyFreeThread)) {
            break;
        }

        /* free context */
        psFirstRetryResManContext = IMG_NULL;
        do {
            ACQUIRE_SYNC_OBJ;
            mutex_lock(&ResManLock);

            psResManContext =
                list_first_or_null_rcu(&ResManQueue,
                                       struct _RESMAN_CONTEXT_,
                                       ResManList
                                      );
            if (psResManContext)
                list_del_rcu(&psResManContext->ResManList);

            mutex_unlock(&ResManLock);
            RELEASE_SYNC_OBJ;

            if (!psResManContext ||
                unlikely(psFirstRetryResManContext == psResManContext))
                break;

            synchronize_rcu_expedited();

            OSAcquireBridgeLock();
            ACQUIRE_SYNC_OBJ;

            /* Free all the resources */
#if 0
            ResManFreeResources(psResManContext, IMG_FALSE);
#else
            /* take lazy bone algo. as default handler */
            ResManFreeResourcesAsLazyBone(psResManContext, IMG_FALSE);
#endif

            RELEASE_SYNC_OBJ;
            OSReleaseBridgeLock();

            /* Ensure that there are no resources left */
            if (psResManContext->psResItemList != IMG_NULL) {
                PVRSRV_ERROR ret;

                PVR_DPF((PVR_DBG_WARNING, "ResManLazyFree: Resman context (%p) disconnect deferred", psResManContext));
                printk(KERN_WARNING "ResManLazyFree: Resman context (%p) disconnect deferred\n", psResManContext);

                /* let clean up thread do the deferred job on behalf of us */
                OSAcquireBridgeLock();
                ACQUIRE_SYNC_OBJ;
                List_RESMAN_CONTEXT_Insert(&psResManContext->psDeferContext->psDeferResManContextList, psResManContext);
                RELEASE_SYNC_OBJ;

                /* Now signal clean up thread */
                ret = OSEventObjectSignal(psResManContext->psDeferContext->hCleanupEventObject);
                OSReleaseBridgeLock();
                PVR_LOG_IF_ERROR(ret, "OSEventObjectSignal");

            } else {
                /* Free the context struct */
                OSFreeMem(psResManContext);
            }

        } while (likely(psResManContext));

        /* free asynchronous pages
         * as final outpost to free pages for dying context;
         * awaken by other finalize routine(s) issuing _FreeOSPages()
         */
        FreeAsyncPages();

        /* wake-up on event or time-out to secure all pending resources to be free'd */
        schedule_timeout_uninterruptible(
            (psFirstRetryResManContext != IMG_NULL?
             msecs_to_jiffies(LAZYFREE_RETRY_WAIT_PERIOD):
             msecs_to_jiffies(MIN_ON_DUTY_INTERVAL)
            ));
    } while (!kthread_should_stop());

    return 0;
}

/*!
******************************************************************************

 @Function      PVRSRVAsyncPageFree

 @Description   Enqueue asynchronous page to be free'd later

 @input         psPage - pointer to page descriptor
                psFunc - asynchronous function to carry out page free

 @Return        IMG_TRUE means successful to queue an asynchronous req.;
                otherwise, IMG_FALSE

******************************************************************************/
IMG_BOOL PVRSRVAsyncPageFree(IMG_PVOID psPage, IMG_PVOID psFunc) {
    ASYNC_PAGE* psAsyncPage;

    if (unlikely(IS_ERR(pResManLazyFreeThread) ||
                 !pResManLazyFreeThread)) {
        return IMG_FALSE;
    }

    psAsyncPage = kzalloc(sizeof(ASYNC_PAGE), GFP_ATOMIC);
    if (!psAsyncPage)
        return IMG_FALSE;

    psAsyncPage->psPageToBeFreed = (struct page*)psPage;
    psAsyncPage->AsyncFunc = (async_func_t)psFunc;

    mutex_lock(&ResManLock);
    list_add_tail_rcu(&psAsyncPage->PageList, &AsyncPageList);
    mutex_unlock(&ResManLock);

    wake_up_process(pResManLazyFreeThread);

    return IMG_TRUE;
}

/*!
******************************************************************************

 @Function	PVRSRVResManLazyFreeInit

 @Description LazyFree thread init and kick-of

 @input 	IMG_VOID

 @Return	IMG_VOID

******************************************************************************/
IMG_VOID PVRSRVResManLazyFreeInit(IMG_VOID) {
    pResManLazyFreeThread =
        kthread_create(ResManLazyFreeThread,
                       NULL,
                       "ResManLazyFree"
                      );

    if (IS_ERR(pResManLazyFreeThread)) {
        printk(KERN_WARNING "ResManLazyFree thread failed to create, fallback to original PVR design\n");
        return;
    }

    get_task_struct(pResManLazyFreeThread);
    wake_up_process(pResManLazyFreeThread);
}

/*!
******************************************************************************

 @Function	PVRSRVResManLazyFreeDeInit

 @Description LazyFree thread de-init and clean-up

 @input 	IMG_VOID

 @Return	IMG_VOID

******************************************************************************/
IMG_VOID PVRSRVResManLazyFreeDeInit(IMG_VOID) {
    if (IS_ERR(pResManLazyFreeThread) || !pResManLazyFreeThread)
        return;

    wake_up_process(pResManLazyFreeThread);
    kthread_stop(pResManLazyFreeThread);
    put_task_struct(pResManLazyFreeThread);
    pResManLazyFreeThread = NULL;
}

/*!
******************************************************************************

 @Function	PVRSRVResManDisconnect

 @Description Closes a Resource Manager connection and frees all resources

 @input 	hResManContext - Resman context
 @input 	bKernelContext - IMG_TRUE for kernel contexts

 @Return	IMG_VOID

******************************************************************************/
IMG_VOID PVRSRVResManDisconnect(PRESMAN_CONTEXT psResManContext)
{
	IMG_BOOL bDefer = IMG_FALSE;

	/* If we have a deferred resman context then allow freeing to be deferred */
	if (psResManContext->psDeferContext)
	{
		bDefer = IMG_TRUE;
	}

	/* Acquire resource list sync object */
	ACQUIRE_SYNC_OBJ;

#if !defined(NO_PVRRESMAN_CONTEXT_LAZYFREE)
    /* NEVER enable for non-deferrable context!! */
    if (bDefer &&
        likely(!IS_ERR(pResManLazyFreeThread) && pResManLazyFreeThread)) {
        /* We're being at the last clean-up (disconnect) phase,
         * context has already been removed from context linked list
         * before disconnect routine is invoked.
         * The context structure would be free'd
         * at the end of this procedure so no dependency is relied on.
         *
         * It's safe to add the dying context to our lazy-free list
         * then carry out the free jobs on behalf of in this place.
         */
        mutex_lock(&ResManLock);
        list_add_tail_rcu(&psResManContext->ResManList, &ResManQueue);
        mutex_unlock(&ResManLock);
        wake_up_process(pResManLazyFreeThread);
        RELEASE_SYNC_OBJ;
        return;
    }
#endif

	/* Free or defer all the resources */
	ResManFreeResources(psResManContext, bDefer);

	/* Ensure that there are no resources left */
	if (psResManContext->psResItemList != IMG_NULL)
	{
		PVR_ASSERT(psResManContext->psDeferContext);
		PVR_DPF((PVR_DBG_WARNING, "PVRSRVResManDisconnect: Resman context (%p) disconnect deferred", psResManContext));
	}
	else
	{
		/* Free the context struct */
		OSFreeMem(psResManContext);
	}

	/* Release resource list sync object */
	RELEASE_SYNC_OBJ;
}

/*!
******************************************************************************

 @Function	PVRSRVResManCreateDeferContext

 @Description
            Create a "defer context" which is used to store resman contexts
            if they have resources that can't be freed at resman disconnect
            time

 @output    phDeferContext - Created defer context

 @Return	IMG_VOID

******************************************************************************/
PVRSRV_ERROR PVRSRVResManCreateDeferContext(IMG_HANDLE hCleanupEventObject,
		PRESMAN_DEFER_CONTEXT *phDeferContext)
{
	PRESMAN_DEFER_CONTEXT psDeferContext;

	PVR_ASSERT(hCleanupEventObject);

	psDeferContext = OSAllocMem(sizeof(RESMAN_DEFER_CONTEXT));
	if (psDeferContext == IMG_NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* Remember the handle of the clean up event object so that it may be
	 * signal when deferredclean is required. Stored as a global RESMAN 
	 * member since the point where deferred clean up is detected involves 
	 * a deep call stack.
	 */
	psDeferContext->ui32Signature = RESMAN_SIGNATURE;
	psDeferContext->hCleanupEventObject = hCleanupEventObject;
	psDeferContext->psDeferResManContextList = IMG_NULL;

	*phDeferContext = psDeferContext;
	return PVRSRV_OK;
}

static IMG_VOID FlushDeferResManContext(PRESMAN_CONTEXT psResManContext)
{
	/* If there are no items on this list then it shouldn’t be here */
	PVR_ASSERT(psResManContext->psResItemList);
	if (psResManContext->psResItemList)
	{
		/* Free what we can */
		ResManFreeResources(psResManContext, IMG_FALSE);
	}

	/*
		If we've freed everything then remove this context from
		the defer context and destroy it
	*/
	if (!psResManContext->psResItemList)
	{
		PVR_DPF((PVR_DBG_WARNING, "PVRSRVResManDisconnect: Resman context (%p) deferred free finished", psResManContext));
		List_RESMAN_CONTEXT_Remove(psResManContext);
		PVRSRVResManDisconnect(psResManContext);
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVResManDisconnect: Resman context (%p) still has pending resources", psResManContext));
		PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVResManDisconnect: psResManContext->psResItemList = %p", psResManContext->psResItemList));
		PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVResManDisconnect: type = %d, pvParam = %p",
				psResManContext->psResItemList->ui32ResType,
				psResManContext->psResItemList->pvParam));
	}
}

/*!
******************************************************************************

 @Function	PVRSRVResManFlushDeferContext

 @Description
            Try to free resources on resman contexts that have been moved to
            this defer context

 @input 	psResManDeferContext - Defer context

 @Return	IMG_BOOL - true when resources still need deferred cleanup
                       false otherwise, the deferred context list is empty

******************************************************************************/
IMG_BOOL PVRSRVResManFlushDeferContext(PRESMAN_DEFER_CONTEXT psDeferContext)
{
	/* Acquire resource list sync object */
	ACQUIRE_SYNC_OBJ;

	/* Go through checking all resman contexts on this defer context */
	List_RESMAN_CONTEXT_ForEachSafe(psDeferContext->psDeferResManContextList, FlushDeferResManContext);

	/* Release resource list sync object */
	RELEASE_SYNC_OBJ;

	PVR_DPF_RETURN_VAL((psDeferContext->psDeferResManContextList != IMG_NULL) ? IMG_TRUE : IMG_FALSE);
}

/*!
******************************************************************************

 @Function	PVRSRVResManDestroyDeferContext

 @Description Destroy the defer context

 @input 	psResManDeferContext - Defer context

 @Return	IMG_VOID

******************************************************************************/
IMG_VOID PVRSRVResManDestroyDeferContext(PRESMAN_DEFER_CONTEXT psDeferContext)
{
	/*
	  If there are still items waiting on the defer list then we must try
	  and free them now.
	*/
	if (psDeferContext->psDeferResManContextList != IMG_NULL)
	{
		/* Useful to know how many contexts are waiting at this point... */
		IMG_UINT32  ui32DeferedCount = 0;
		RESMAN_CONTEXT*  psItem = psDeferContext->psDeferResManContextList;
	
		while (psItem != IMG_NULL)
		{
			ui32DeferedCount++;
			psItem = psItem->psNext;
		}
		
		PVR_DPF((PVR_DBG_WARNING, "PVRSRVResManDestroyDeferContext: %d resman context(s) waiting to be freed!", ui32DeferedCount));

		/* Attempt to free more resources... */
		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			PVRSRVResManFlushDeferContext(psDeferContext);
			
			/* If the driver is not in a okay state then don't try again... */
			if (PVRSRVGetPVRSRVData()->eServicesState != PVRSRV_SERVICES_STATE_OK)
			{
				break;
			}
			
			/* Break out if we have freed them all... */
			if (psDeferContext->psDeferResManContextList == IMG_NULL)
			{
				break;
			}
		
			OSSleepms((MAX_HW_TIME_US / 1000) / 10);
		} END_LOOP_UNTIL_TIMEOUT();

		/* Once more for luck and then force the issue... */
		PVRSRVResManFlushDeferContext(psDeferContext);
		if (psDeferContext->psDeferResManContextList != IMG_NULL)
		{
			ui32DeferedCount = 0;
			psItem           = psDeferContext->psDeferResManContextList;
			while (psItem != IMG_NULL)
			{
				PVR_DPF((PVR_DBG_ERROR, "PVRSRVResManDestroyDeferContext: Resman context (%p) has not been freed!", psItem));
				ui32DeferedCount++;
				psItem = psItem->psNext;
			}
			PVR_DPF((PVR_DBG_ERROR, "PVRSRVResManDestroyDeferContext: %d resman context(s) still waiting!", ui32DeferedCount));

			PVR_ASSERT(psDeferContext->psDeferResManContextList == IMG_NULL);
		}
	}

	/* Free the defer context... */
	OSFreeMem(psDeferContext);
}

/*!
******************************************************************************
 @Function	 ResManRegisterRes

 @Description    : Inform the resource manager that the given resource has
				   been alloacted and freeing of it will be the responsibility
				   of the resource manager

 @input 	psResManContext - resman context
 @input 	ui32ResType - identify what kind of resource it is
 @input 	pvParam - address of resource
 @input 	ui32Param - size of resource
 @input 	pfnFreeResource - pointer to function that frees this resource

 @Return   On success a pointer to an opaque data structure that represents
						the allocated resource, else NULL

**************************************************************************/
PRESMAN_ITEM ResManRegisterRes(PRESMAN_CONTEXT	psResManContext,
							   IMG_UINT32		ui32ResType,
							   IMG_PVOID		pvParam,
							   RESMAN_FREE_FN	pfnFreeResource)
{
	PRESMAN_ITEM	psNewResItem;

	PVR_ASSERT(psResManContext != IMG_NULL);
	PVR_ASSERT(ui32ResType != 0);

	if (psResManContext == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "ResManRegisterRes: invalid parameter - psResManContext"));
		return (PRESMAN_ITEM) IMG_NULL;
	}

	/* Acquire resource list sync object */
	ACQUIRE_SYNC_OBJ;

	PVR_DPF((PVR_DBG_MESSAGE, "ResManRegisterRes: register resource "
			"Context %p, ResType 0x%x, pvParam %p, "
			"FreeFunc %p",
			psResManContext, ui32ResType, pvParam, pfnFreeResource));

	/* Allocate memory for the new resource structure */
	psNewResItem = OSAllocMem(sizeof(RESMAN_ITEM));
	if (psNewResItem == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "ResManRegisterRes: "
				"ERROR allocating new resource item"));

		/* Release resource list sync object */
		RELEASE_SYNC_OBJ;

		return((PRESMAN_ITEM)IMG_NULL);
	}

	/* Fill in details about this resource */
	psNewResItem->ui32Signature		= RESMAN_SIGNATURE;
	psNewResItem->ui32ResType		= ui32ResType;
	psNewResItem->pvParam			= pvParam;
	psNewResItem->pfnFreeResource	= pfnFreeResource;

	/* Insert new structure after dummy first entry */
	List_RESMAN_ITEM_Insert(&psResManContext->psResItemList, psNewResItem);

	/* Release resource list sync object */
	RELEASE_SYNC_OBJ;

	return(psNewResItem);
}

/*!
******************************************************************************
 @Function	 	ResManFreeResByPtr

 @Description   frees a resource by matching on pointer type

 @inputs        psResItem - pointer to resource item to free

 @Return   		PVRSRV_ERROR
**************************************************************************/
PVRSRV_ERROR ResManFreeResByPtr(RESMAN_ITEM	*psResItem)
{
	PVRSRV_ERROR eError;

	PVR_ASSERT(psResItem != IMG_NULL);

	if (psResItem == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "ResManFreeResByPtr: NULL ptr - nothing to do"));
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "ResManFreeResByPtr: freeing resource at %p",
			 psResItem));

	/*Acquire resource list sync object*/
	ACQUIRE_SYNC_OBJ;

	/*Free resource*/
	eError = FreeResourceByPtr(psResItem);

	/*Release resource list sync object*/
	RELEASE_SYNC_OBJ;

	return(eError);
}


/*!
******************************************************************************
 @Function	 	ResManFindPrivateDataByPtr

 @Description   finds the private date for a resource by matching on pointer type

 @inputs        psResItem - pointer to resource item

 @Return   		PVRSRV_ERROR
**************************************************************************/
PVRSRV_ERROR
ResManFindPrivateDataByPtr(
                           RESMAN_ITEM *psResItem,
                           IMG_PVOID *ppvParam1
                           )
{
	PVR_ASSERT(psResItem != IMG_NULL);

	if (psResItem == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "ResManFindPrivateDataByPtr: NULL ptr - nothing to do"));
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "ResManFindPrivateDataByPtr: looking up private data for resource at %p",
			psResItem));

	/*Acquire resource list sync object*/
	ACQUIRE_SYNC_OBJ;

    /* verify signature */
    PVR_ASSERT(psResItem->ui32Signature == RESMAN_SIGNATURE);

    /* lookup params */
    if (ppvParam1 != IMG_NULL)
    {
        *ppvParam1 = psResItem->pvParam;
    }

	/*Release resource list sync object*/
	RELEASE_SYNC_OBJ;

	return PVRSRV_OK;
}


/*!
******************************************************************************
 @Function	 	ResManFreeResByCriteria

 @Description   frees a resource by matching on criteria

 @inputs	 	hResManContext - handle for resman context
 @inputs        ui32SearchCriteria - indicates which parameters should be
 				used in search for resources to free
 @inputs        ui32ResType - identify what kind of resource to free
 @inputs        pvParam - address of resource to be free
 @inputs        ui32Param - size of resource to be free

 @Return   		PVRSRV_ERROR
**************************************************************************/
PVRSRV_ERROR ResManFreeResByCriteria(PRESMAN_CONTEXT	psResManContext,
									 IMG_UINT32			ui32SearchCriteria,
									 IMG_UINT32			ui32ResType,
									 IMG_PVOID			pvParam)
{
	PVRSRV_ERROR	eError;

	PVR_ASSERT(psResManContext != IMG_NULL);

	/* Acquire resource list sync object */
	ACQUIRE_SYNC_OBJ;

	PVR_DPF((PVR_DBG_MESSAGE, "ResManFreeResByCriteria: "
			"Context %p, Criteria 0x%x, Type 0x%x, Addr %p",
			psResManContext, ui32SearchCriteria, ui32ResType,
			pvParam));

	/* Free resources by criteria for this context */
	eError = FreeResourceByCriteria(psResManContext, ui32SearchCriteria,
									ui32ResType, pvParam, IMG_FALSE);

	/* Release resource list sync object */
	RELEASE_SYNC_OBJ;

	return eError;
}


/*!
******************************************************************************
 @Function	 	ResManDissociateRes

 @Description   Moves a resource from one context to another.

 @inputs        psResItem - pointer to resource item to dissociate
 @inputs	 	psNewResManContext - new resman context for the resource

 @Return   		IMG_VOID
**************************************************************************/
PVRSRV_ERROR ResManDissociateRes(RESMAN_ITEM		*psResItem,
							 PRESMAN_CONTEXT	psNewResManContext)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVR_ASSERT(psResItem != IMG_NULL);

	if (psResItem == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "ResManDissociateRes: invalid parameter - psResItem"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_ASSERT(psResItem->ui32Signature == RESMAN_SIGNATURE);

	if (psNewResManContext != IMG_NULL)
	{
		/* Remove this item from its old resource list */
		List_RESMAN_ITEM_Remove(psResItem);

		/* Re-insert into new list */
		List_RESMAN_ITEM_Insert(&psNewResManContext->psResItemList, psResItem);

	}
	else
	{
		/* Remove this item from its old resource list */
		List_RESMAN_ITEM_Remove(psResItem);
		
		/* Free this item as no one refers to it now */
		OSFreeMem(psResItem);
	}

	return eError;
}

/*!
******************************************************************************
 @Function	 	ResManFindResourceByPtr_AnyVaCb

 @Description
 					Compares the resman item with a given pointer.

 @inputs	 	psCurItem - theThe item to check
 @inputs        va - Variable argument list with:
					 psItem - pointer to resource item to find

 @Return   		IMG_BOOL
**************************************************************************/
static IMG_BOOL ResManFindResourceByPtr_AnyVaCb(RESMAN_ITEM *psCurItem, va_list va)
{
	RESMAN_ITEM		*psItem;

	psItem = va_arg(va, RESMAN_ITEM*);

	return (IMG_BOOL)(psCurItem == psItem);
}


/*!
******************************************************************************
 @Function	 	ResManFindResourceByPtr

 @Description
 					Attempts to find a resource in the list for this context

 @inputs	 	hResManContext - handle for resman context
 @inputs        psItem - pointer to resource item to find

 @Return   		PVRSRV_ERROR
**************************************************************************/
IMG_INTERNAL PVRSRV_ERROR ResManFindResourceByPtr(PRESMAN_CONTEXT	psResManContext,
												  RESMAN_ITEM		*psItem)
{
/*	RESMAN_ITEM		*psCurItem;*/

	PVRSRV_ERROR	eResult;

	PVR_ASSERT(psResManContext != IMG_NULL);
	PVR_ASSERT(psItem != IMG_NULL);

	if ((psItem == IMG_NULL) || (psResManContext == IMG_NULL))
	{
		PVR_DPF((PVR_DBG_ERROR, "ResManFindResourceByPtr: invalid parameter"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_ASSERT(psItem->ui32Signature == RESMAN_SIGNATURE);

	/* Acquire resource list sync object */
	ACQUIRE_SYNC_OBJ;

	PVR_DPF((PVR_DBG_MESSAGE,
			"FindResourceByPtr: psItem=%p, psItem->psNext=%p",
			psItem, psItem->psNext));

	PVR_DPF((PVR_DBG_MESSAGE,
			"FindResourceByPtr: Resource Ctx %p, Type 0x%x, Addr %p, "
			"FnCall %p",
			psResManContext, psItem->ui32ResType, psItem->pvParam,
			psItem->pfnFreeResource));

	/* Search resource items starting at after the first dummy item */
	if(List_RESMAN_ITEM_IMG_BOOL_Any_va(psResManContext->psResItemList,
										&ResManFindResourceByPtr_AnyVaCb,
										psItem))
	{
		eResult = PVRSRV_OK;
	}
	else
	{
		eResult = PVRSRV_ERROR_NOT_OWNER;
	}

	/* Release resource list sync object */
	RELEASE_SYNC_OBJ;

	return eResult;
}

/*!
******************************************************************************
 @Function	 	FreeResourceByPtr

 @Description
 					Frees a resource and move it from the list
					NOTE : this function must be called with the resource
					list sync object held

 @inputs        psItem - pointer to resource item to free
 				bExecuteCallback - execute callback?

 @Return   		PVRSRV_ERROR
**************************************************************************/
static PVRSRV_ERROR FreeResourceByPtr(RESMAN_ITEM	*psItem)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	PVR_ASSERT(psItem != IMG_NULL);

	if (psItem == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "FreeResourceByPtr: invalid parameter"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_ASSERT(psItem->ui32Signature == RESMAN_SIGNATURE);

	PVR_DPF((PVR_DBG_MESSAGE,
			"FreeResourceByPtr: psItem=%p, psItem->psNext=%p",
			psItem, psItem->psNext));

	PVR_DPF((PVR_DBG_MESSAGE,
			 "FreeResourceByPtr: Type 0x%x, Addr %p, "
			 "FnCall %p",
			 psItem->ui32ResType, psItem->pvParam,
			 psItem->pfnFreeResource));

	/* Release resource list sync object just in case the free routine calls the resource manager */
	RELEASE_SYNC_OBJ;

	/* Call the freeing routine */
	eError = psItem->pfnFreeResource(psItem->pvParam);
 	if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_RETRY))
	{
		PVR_DPF((PVR_DBG_ERROR, "FreeResourceByPtr: ERROR calling FreeResource function for %p", psItem));
	}

	if (eError == PVRSRV_ERROR_RETRY)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "FreeResourceByPtr: Got retry while calling FreeResource function for %p", psItem));
	}

	/* Acquire resource list sync object */
	ACQUIRE_SYNC_OBJ;

	if (eError != PVRSRV_ERROR_RETRY)
	{
		/* Remove this item from the resource list */
		List_RESMAN_ITEM_Remove(psItem);
	
		/* Free memory for the resource item */
		OSFreeMem(psItem);
	}

	return(eError);
}

/*!
******************************************************************************
 @Function	 	FreeResourceByCriteria_AnyVaCb

 @Description
 					Matches a resource manager item with a given criteria.

 @inputs        psCuItem - the item to be matched
 @inputs		va - a variable argument list with:.
					ui32SearchCriteria - indicates which parameters should be used
					search for resources to free
					ui32ResType - identify what kind of resource to free
					pvParam - address of resource to be free
					ui32Param - size of resource to be free


 @Return   		psCurItem if matched, IMG_NULL otherwise.
**************************************************************************/
static IMG_VOID* FreeResourceByCriteria_AnyVaCb(RESMAN_ITEM *psCurItem, va_list va)
{
	IMG_UINT32 ui32SearchCriteria;
	IMG_UINT32 ui32ResType;
	IMG_PVOID pvParam;

	ui32SearchCriteria = va_arg(va, IMG_UINT32);
	ui32ResType = va_arg(va, IMG_UINT32);
	pvParam = va_arg(va, IMG_PVOID);

	/*check that for all conditions are either disabled or eval to true*/
	if(
	/* Check resource type */
		(((ui32SearchCriteria & RESMAN_CRITERIA_RESTYPE) == 0UL) ||
		(psCurItem->ui32ResType == ui32ResType))
	&&
	/* Check address */
		(((ui32SearchCriteria & RESMAN_CRITERIA_PVOID_PARAM) == 0UL) ||
			 (psCurItem->pvParam == pvParam))
		)
	{
		return psCurItem;
	}
	else
	{
		return IMG_NULL;
	}
}

/*!
******************************************************************************
 @Function	 	FreeResourceByCriteria

 @Description
                Frees all resources that match the given criteria for the
                context.
                If we've been asked to defer the free then if any resource
                returns retry then we move the resman context onto the defer
                context and immediately bail.
                If we haven't been asked to defer the free then we will
                try and free all resources that match the criteria, regardless
                of any errors. If any resource returns a retry error then this
                will be returned (after we've tried to free all the other
                resources).

 @inputs        psResManContext - pointer to resman context
 @inputs        ui32SearchCriteria - indicates which parameters should be used
 @inputs        search for resources to free
 @inputs        ui32ResType - identify what kind of resource to free
 @inputs        pvParam - address of resource to be free
 @inputs        bDefer - If the free fails defer the resource free from this process

 @Return   		PVRSRV_ERROR
**************************************************************************/
static PVRSRV_ERROR FreeResourceByCriteria(PRESMAN_CONTEXT	psResManContext,
										   IMG_UINT32		ui32SearchCriteria,
										   IMG_UINT32		ui32ResType,
										   IMG_PVOID		pvParam,
										   IMG_BOOL			bDefer)
{
	PRESMAN_ITEM	psCurItem;
	PVRSRV_ERROR	eError = PVRSRV_OK;
	IMG_BOOL		bContinue = IMG_TRUE;
	IMG_BOOL		bRetry = IMG_FALSE;

	/* Search resource items starting at after the first dummy item */
	/*while we get a match and not an error*/
	while((psCurItem = (PRESMAN_ITEM)
				List_RESMAN_ITEM_Any_va(psResManContext->psResItemList,
										&FreeResourceByCriteria_AnyVaCb,
										ui32SearchCriteria,
										ui32ResType,
						 				pvParam)) != IMG_NULL
		  	&& bContinue)
	{
		eError = FreeResourceByPtr(psCurItem);

		/*
			We failed to free the resource, if we got a retry and this process
			disconnect time then defer the free until later.
		*/
		if ((eError == PVRSRV_ERROR_RETRY) && bDefer)
		{
			PVRSRV_ERROR ret;

			PVR_ASSERT(psResManContext->psDeferContext);
			PVR_DPF((PVR_DBG_WARNING, "FreeResourceByCriteria: Resource %p (type %d) returned retry. Moving resman context to defer context", psCurItem, ui32ResType));

			/*
				Due to the fact the not all resources are refcounted against each
				other then as soon as we get a retry from one resource free we
				have to assume that we can't free any further resources.

				Insert this resman context into the defer context so we can free
				items at our leisure and then bail out.
			*/
			List_RESMAN_CONTEXT_Insert(&psResManContext->psDeferContext->psDeferResManContextList, psResManContext);
			bContinue = IMG_FALSE;

			/* Now signal clean up thread */
			ret = OSEventObjectSignal(psResManContext->psDeferContext->hCleanupEventObject);
			PVR_LOG_IF_ERROR(ret, "OSEventObjectSignal");
		}
		else
		{
			/*
				Regardless of error or retry continue trying to free other
				resources of this type as this is safe.
				However, if any resource did return a retry error then we must
				report that back so we don't continue down the resource list
				trying to free resources which the busy resource might be dependant
				on.
			*/

			if (eError == PVRSRV_ERROR_RETRY)
			{
				PVR_DPF((PVR_DBG_MESSAGE, "FreeResourceByCriteria: Got retry on resource %p", psCurItem));
				bRetry = IMG_TRUE;
				bContinue = IMG_FALSE;
			}
			else if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "FreeResourceByCriteria: Error freeing resource %p (%s)", psCurItem, PVRSRVGetErrorStringKM(eError)));
				bContinue = IMG_FALSE;
			}
		}
	}

	if (bRetry)
	{
		return PVRSRV_ERROR_RETRY;
	}
	return eError;
}

static IMG_UINT32 g_ui32OrderedFreeList [] = {
	RESMAN_TYPE_EVENT_OBJECT,
	RESMAN_TYPE_SHARED_EVENT_OBJECT,

	/* RGX types */
	RESMAN_TYPE_RGX_POPULATION,
	RESMAN_TYPE_RGX_FWIF_HWRTDATA,
	RESMAN_TYPE_RGX_FWIF_FREELIST,
	RESMAN_TYPE_RGX_FWIF_ZSBUFFER,
	RESMAN_TYPE_RGX_FWIF_RENDERTARGET,
	RESMAN_TYPE_RGX_SERVER_RENDER_CONTEXT,
	RESMAN_TYPE_RGX_SERVER_TQ_CONTEXT,
	RESMAN_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
	RESMAN_TYPE_RGX_SERVER_RAY_CONTEXT,
	RESMAN_TYPE_SHARED_PB_DESC_CREATE_LOCK,
	RESMAN_TYPE_SHARED_PB_DESC,

	/* Common */
	RESMAN_TYPE_DEVMEM_MEM_EXPORT,
	RESMAN_TYPE_SERVER_OP_COOKIE,
	RESMAN_TYPE_SYNC_PRIMITIVE,
	RESMAN_TYPE_SERVER_SYNC_PRIMITIVE,
	RESMAN_TYPE_SERVER_SYNC_EXPORT,
	RESMAN_TYPE_SYNC_PRIMITIVE_BLOCK,
	RESMAN_TYPE_SYNC_INFO,
	RESMAN_TYPE_DEVICECLASSMEM_MAPPING,
	RESMAN_TYPE_DEVICEMEM_WRAP,
	RESMAN_TYPE_DEVICEMEM_MAPPING,
	RESMAN_TYPE_KERNEL_DEVICEMEM_ALLOCATION,
	RESMAN_TYPE_DEVICEMEM_ALLOCATION,
	RESMAN_TYPE_DEVICEMEM_CONTEXT,
	RESMAN_TYPE_SHARED_MEM_INFO,
	RESMAN_TYPE_DEVICEMEM2_MAPPING,
	RESMAN_TYPE_DEVICEMEM2_RESERVATION,
	RESMAN_TYPE_DEVICEMEM2_HEAP,
	RESMAN_TYPE_DEVICEMEM2_CONTEXT_EXPORT,
	RESMAN_TYPE_DEVICEMEM2_CONTEXT,
	RESMAN_TYPE_PMR_PAGELIST,
	RESMAN_TYPE_PMR_EXPORT,
	RESMAN_TYPE_PMR,

	/* DISPLAY CLASS types: */
	RESMAN_TYPE_DC_PIN_HANDLE,
	RESMAN_TYPE_DC_DISPLAY_CONTEXT,
	RESMAN_TYPE_DC_BUFFER,
	RESMAN_TYPE_DC_DEVICE,

	RESMAN_TYPE_DISPLAYCLASS_SWAPCHAIN_REF,
	RESMAN_TYPE_DISPLAYCLASS_DEVICE,

	/* BUFFER CLASS types: */
	RESMAN_TYPE_BUFFERCLASS_DEVICE,

	/* OS-specific user mode mappings: */
	RESMAN_TYPE_OS_USERMODE_MAPPING,

	/* TRANSPORT LAYER types: */
	RESMAN_TYPE_TL_STREAM_DESC,

	/* RI types: */
	RESMAN_TYPE_RI_HANDLE,
};

/*!
******************************************************************************
 @Function	 	ResManFreeResources

 @Description
                Free or defer the freeing of all resources on this context.
                NOTE : this function must be called with the resource
                list sync object held

 @inputs        psResManContext - pointer to resman context
 @inputs        bDefer - Defer the free if we can't free the resource now

 @Return   		None
**************************************************************************/
static IMG_VOID ResManFreeResources(PRESMAN_CONTEXT psResManContext,
									IMG_BOOL bDefer)
{
	IMG_UINT32 i;
	PVRSRV_ERROR eError;

	for (i=0;i<(sizeof(g_ui32OrderedFreeList)/sizeof(g_ui32OrderedFreeList[0]));i++)
	{
		eError = FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE, g_ui32OrderedFreeList[i], IMG_NULL, bDefer);
		if (eError != PVRSRV_OK)
		{
			/* Bail on error */
			break;
		}
	}
}

/*!
******************************************************************************
 @Function      ResManFreeResourcesAsLazyBone

 @Description
                Free or defer the freeing of all resources on this context As a lazy bone.
                NOTE : this function must be called with the resource
                list sync object held

 @inputs        psResManContext - pointer to resman context
 @inputs        bDefer - Defer the free if we can't free the resource now

 @Return        None
**************************************************************************/
static IMG_VOID ResManFreeResourcesAsLazyBone(PRESMAN_CONTEXT psResManContext,
                                              IMG_BOOL bDefer) {
    IMG_UINT32 i;
    PVRSRV_ERROR eError;
    IMG_UINT64 ui64YieldTimeMark;
    IMG_UINT64 ui64NextFrameMark;
    IMG_UINT64 ui64TimeNow;

    /* evaluate next timeslice marks */
    ui64YieldTimeMark = ui64NextFrameMark = OSClockns64();
    ui64YieldTimeMark += RESFREE_YIELD_TIMESLICE;
    ui64NextFrameMark += ONE_FRAME_TIME_NS_U64;

    for (i = 0; i < (sizeof(g_ui32OrderedFreeList) / sizeof(g_ui32OrderedFreeList[0])); i++) {
        eError = FreeResourceByCriteria(psResManContext, RESMAN_CRITERIA_RESTYPE, g_ui32OrderedFreeList[i], IMG_NULL, bDefer);
        if (eError != PVRSRV_OK) {
            /* Bail on error */
            break;
        }

        ui64TimeNow = OSClockns64();
        if (ui64TimeNow > ui64YieldTimeMark) {
            /* we've owned bridge lock over allocated timeslices, yield to other tasks */
            OSReleaseBridgeLock();

            /* Lazy algo:
             *  a) accumulated time <= 3ms (default), keep going
             *  b) accumulated time is [3ms, 8.33ms], sleep the rest of one frame time
             *  c) accumulated time is > 8.33ms, sleep one whole frame time
             */

            if (ui64TimeNow < ui64NextFrameMark &&
                (ui64NextFrameMark - ui64TimeNow) >= (ONE_FRAME_TIME_NS_U64 >> 1)) {
                /* sleep until one frame time gets passed by */
                usleep_range((ui64NextFrameMark - ui64TimeNow) / NSEC_PER_USEC,
                             (ui64NextFrameMark - ui64TimeNow) / NSEC_PER_USEC
                            );

            } else {
                /* run over 1/2 frame time :( make up one more frame time */
                usleep_range(ONE_FRAME_TIME_NS_U64 / NSEC_PER_USEC,
                             ONE_FRAME_TIME_NS_U64 / NSEC_PER_USEC
                            );

                printk(KERN_WARNING "%s: bridge lock is owned %s, distance: %u ms\n",
                       __FUNCTION__,
                       (ui64TimeNow <= ui64NextFrameMark ? "<= 16.6ms" : "> 16.6ms"),
                       (ui64TimeNow <= ui64NextFrameMark ?
                        (ui64NextFrameMark - ui64TimeNow) / NSEC_PER_MSEC :
                        ((ui64TimeNow - ui64NextFrameMark) / NSEC_PER_MSEC))
                      );
            }

            /* run a little bit first, avoid too many pages queued to free */
            FreeAsyncPages();

            /* re-acquire bridge lock to free next resource */
            OSAcquireBridgeLock();

            /* right after owning bridge lock, re-evaluate next timeslice marks */
            ui64YieldTimeMark = ui64NextFrameMark = OSClockns64();
            ui64YieldTimeMark += RESFREE_YIELD_TIMESLICE;
            ui64NextFrameMark += ONE_FRAME_TIME_NS_U64;
        }
    }
}

/******************************************************************************
 End of file (resman.c)
******************************************************************************/
