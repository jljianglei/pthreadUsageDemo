#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>

#include "CpuExt.h"
#include "private.h"
#include "CpuThread.h"

static pthread_mutex_t globalMutex = PTHREAD_MUTEX_INITIALIZER;

extern IscThreadEntry* mThreadEntry[ISC_MAX_ID][ISC_MAX_TASK];
extern IscReceivedMsg mReceiveCb[ISC_MAX_ID];
extern const ISC_CHANNALE_MATRIX_T ChannelMatrix[ISC_MAX_ID][ISC_MAX_TASK] ;
/*----------------------------------------------------------------------------*
 *  NAME
 *      IscEventCreate
 *
 *  DESCRIPTION
 *      Creates an event and returns a handle to the created event.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS             in case of success
 *          ISC_RESULT_NO_MORE_EVENTS   in case of out of event resources
 *          ISC_RESULT_INVALID_POINTER  in case the eventHandle pointer is invalid
 *----------------------------------------------------------------------------*/
IscResult IscEventCreate(IscEventHandle *eventHandle)
{
    if (eventHandle == NULL) {
        return ISC_RESULT_INVALID_POINTER;
    }

    if (pthread_mutex_init(&(eventHandle->mutex), NULL) == 0) {
        if (pthread_cond_init(&(eventHandle->event), NULL) != 0) {
            return ISC_RESULT_NO_MORE_EVENTS;
        }

        eventHandle->eventBits = 0;
        return ISC_RESULT_SUCCESS;
    }
    else {
        return ISC_RESULT_NO_MORE_EVENTS;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscEventWait
 *
 *  DESCRIPTION
 *      Wait for the event to be set.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS                 in case of success
 *          ISC_RESULT_TIMEOUT              in case of timeout
 *          ISC_RESULT_INVALID_HANDLE       in case the eventHandle is invalid
 *          ISC_RESULT_INVALID_POINTER      in case the eventBits pointer is invalid
 *----------------------------------------------------------------------------*/
IscResult IscEventWait(IscEventHandle *eventHandle, uint16 timeoutInMs, uint32 *eventBits)
{
    struct timespec ts;
    IscResult result;
    int ret = 0;
    pthread_condattr_t condattr;
    if (eventHandle == NULL) {
        return ISC_RESULT_INVALID_HANDLE;
    }

    if (eventBits == NULL) {
        return ISC_RESULT_INVALID_POINTER;
    }
    ret = pthread_condattr_init(&condattr);
    if (ret != 0) {
	ISCLOGE("thread condattr init error: %d\n", ret);
	exit(1);
    }
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);

    pthread_cond_init(&(eventHandle->event), &(condattr));
    (void) pthread_mutex_lock(&(eventHandle->mutex));
    if ((eventHandle->eventBits == 0) && (timeoutInMs != 0)) {
        int rc = 0;
        if (timeoutInMs != ISC_EVENT_WAIT_INFINITE) {
            time_t sec;

            (void) clock_gettime(CLOCK_MONOTONIC, &ts);
            sec = timeoutInMs / 1000;
            ts.tv_sec = ts.tv_sec + sec;
            ts.tv_nsec = ts.tv_nsec + (timeoutInMs - sec * 1000) * 1000000;

            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_nsec -= 1000000000L;
                ts.tv_sec++;
            }

            while (eventHandle->eventBits == 0 && rc == 0) {
                rc = pthread_cond_timedwait(&(eventHandle->event), &(eventHandle->mutex), &ts);
            }
        }
        else {
            while (eventHandle->eventBits == 0 && rc == 0) {
                rc = pthread_cond_wait(&(eventHandle->event), &(eventHandle->mutex));
            }
        }
    }

    result = (eventHandle->eventBits == 0) ? ISC_RESULT_TIMEOUT : ISC_RESULT_SUCCESS;
    /* Indicate to caller which events were triggered and cleared */
    *eventBits = eventHandle->eventBits;
    /* Clear triggered events */
    eventHandle->eventBits = 0;
    (void) pthread_mutex_unlock(&(eventHandle->mutex));
    return result;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscEventSet
 *
 *  DESCRIPTION
 *      Set an event.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS              in case of success
 *          ISC_RESULT_INVALID_HANDLE       in case the eventHandle is invalid
 *
 *----------------------------------------------------------------------------*/
IscResult IscEventSet(IscEventHandle *eventHandle, uint32 eventBits)
{
    if (eventHandle == NULL) {
        return ISC_RESULT_INVALID_HANDLE;
    }

    (void) pthread_mutex_lock(&(eventHandle->mutex));
    eventHandle->eventBits |= eventBits;
    (void) pthread_cond_signal(&(eventHandle->event));
    (void) pthread_mutex_unlock(&(eventHandle->mutex));
    return ISC_RESULT_SUCCESS;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscEventDestroy
 *
 *  DESCRIPTION
 *      Destroy the event associated.
 *
 *  RETURNS
 *      void
 *
 *----------------------------------------------------------------------------*/
void IscEventDestroy(IscEventHandle *eventHandle)
{
    if (eventHandle == NULL) {
        return;
    }

    (void) pthread_cond_destroy(&(eventHandle->event));
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscMutexCreate
 *
 *  DESCRIPTION
 *      Create a mutex and return a handle to the created mutex.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_NO_MORE_MUTEXES   in case of out of mutex resources
 *          ISC_RESULT_INVALID_POINTER   in case the mutexHandle pointer is invalid
 *
 *----------------------------------------------------------------------------*/
IscResult IscMutexCreate(IscMutexHandle *mutexHandle)
{
    if (mutexHandle == NULL) {
        return ISC_RESULT_INVALID_POINTER;
    }

    if (pthread_mutex_init(mutexHandle, NULL) == 0) {
        return ISC_RESULT_SUCCESS;
    }
    else {
        return ISC_RESULT_NO_MORE_MUTEXES;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscMutexLock
 *
 *  DESCRIPTION
 *      Lock the mutex refered to by the provided handle.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_INVALID_HANDLE    in case the mutexHandle is invalid
 *
 *----------------------------------------------------------------------------*/
IscResult IscMutexLock(IscMutexHandle *mutexHandle)
{
    if (mutexHandle == NULL) {
        return ISC_RESULT_INVALID_HANDLE;
    }

    (void) pthread_mutex_lock(mutexHandle);
    return ISC_RESULT_SUCCESS;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscMutexUnlock
 *
 *  DESCRIPTION
 *      Unlock the mutex refered to by the provided handle.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_INVALID_HANDLE    in case the mutexHandle is invalid
 *
 *----------------------------------------------------------------------------*/
IscResult IscMutexUnlock(IscMutexHandle *mutexHandle)
{
    if (mutexHandle == NULL) {
        return ISC_RESULT_INVALID_HANDLE;
    }

    (void) pthread_mutex_unlock(mutexHandle);
    return ISC_RESULT_SUCCESS;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscMutexDestroy
 *
 *  DESCRIPTION
 *      Destroy the previously created mutex.
 *
 *  RETURNS
 *      void
 *
 *----------------------------------------------------------------------------*/
void IscMutexDestroy(IscMutexHandle *mutexHandle)
{
    if (mutexHandle == NULL) {
        return;
    }

    (void) pthread_mutex_destroy(mutexHandle);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscGlobalMutexLock
 *
 *  DESCRIPTION
 *      Lock the global mutex.
 *
 *----------------------------------------------------------------------------*/
void IscGlobalMutexLock(void)
{
    (void) pthread_mutex_lock(&globalMutex);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscGlobalMutexUnlock
 *
 *  DESCRIPTION
 *      Unlock the global mutex.
 *
 *----------------------------------------------------------------------------*/
void IscGlobalMutexUnlock(void)
{
    (void) pthread_mutex_unlock(&globalMutex);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadCreate
 *
 *  DESCRIPTION
 *      Create thread function and return a handle to the created thread.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_NO_MORE_THREADS   in case of out of thread resources
 *          ISC_RESULT_INVALID_POINTER   in case one of the supplied pointers is invalid
 *          ISC_RESULT_FAILURE           otherwise
 *
 *----------------------------------------------------------------------------*/
IscResult IscThreadCreate(void (*threadFunction)(void *pointer), void *pointer,
                          uint32 stackSize, uint16 priority,
                          const int8 *threadName, IscThreadHandle *threadHandle)
{
    int rc;
    pthread_attr_t threadAttr;
    uint32 stackS = 0;

    priority;
    threadName;

    if ((threadFunction == NULL) || (threadHandle == NULL)) {
        return ISC_RESULT_INVALID_POINTER;
    }

    rc = pthread_attr_init(&threadAttr);
    if (rc != 0) {
        ISCLOGE("thread attr init error: %d\n", rc);
        return ISC_RESULT_FAILURE;
    }

    rc = pthread_create(threadHandle, &threadAttr, (void *(*)(void *))threadFunction, pointer);
    if (rc != 0) {
        ISCLOGE("thread create error: %d\n", rc);
        return ISC_RESULT_NO_MORE_THREADS;
    }
    else {
        (void) pthread_detach(*threadHandle);
        rc = pthread_attr_setstacksize(&threadAttr, stackSize);
        if (rc != 0) {
            ISCLOGE("set stack size error stackSize=0x%x\n", stackSize);
        }
        else {
            rc = pthread_attr_getstacksize(&threadAttr, &stackS);
            if (rc == 0) {
                ISCLOGI("%s stack size=0x%x\n", __FUNCTION__, stackS);
            }
        }
    }
    return ISC_RESULT_SUCCESS;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadGetHandle
 *
 *  DESCRIPTION
 *      Return thread handle of calling thread.
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS             in case of success
 *          ISC_RESULT_INVALID_POINTER  in case the threadHandle pointer is invalid
 *
 *----------------------------------------------------------------------------*/
IscResult IscThreadGetHandle(IscThreadHandle *threadHandle)
{
    if (threadHandle == NULL) {
        return ISC_RESULT_INVALID_POINTER;
    }

    *threadHandle = pthread_self();
    return ISC_RESULT_SUCCESS;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadEqual
 *
 *  DESCRIPTION
 *      Compare thread handles
 *
 *  RETURNS
 *      Possible values:
 *          ISC_RESULT_SUCCESS             in case thread handles are identical
 *          ISC_RESULT_INVALID_POINTER  in case either threadHandle pointer is invalid
 *          ISC_RESULT_FAILURE             otherwise
 *
 *----------------------------------------------------------------------------*/
IscResult IscThreadEqual(IscThreadHandle *threadHandle1, IscThreadHandle *threadHandle2)
{
    if ((threadHandle1 == NULL) || (threadHandle2 == NULL)) {
        return ISC_RESULT_INVALID_POINTER;
    }

    if (pthread_equal(*threadHandle1, *threadHandle2) != 0) {
        return ISC_RESULT_SUCCESS;
    }
    else {
        return ISC_RESULT_FAILURE;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadSleep
 *
 *  DESCRIPTION
 *      Sleep for a given period.
 *
 *  RETURNS
 *      void
 *
 *----------------------------------------------------------------------------*/
void IscThreadSleep(uint16 sleepTimeInMs)
{
    struct timespec ts;
    uint16 seconds = (sleepTimeInMs / 1000U);
    uint32 nanoseconds = (sleepTimeInMs - seconds * 1000U) * 1000000U;

    ts.tv_sec = (time_t) seconds;
    ts.tv_nsec = (long) nanoseconds;

    (void) nanosleep(&ts, NULL);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadSleep
 *
 *  DESCRIPTION
 *      Sleep for a given period.
 *
 *  RETURNS
 *      void
 *
 *----------------------------------------------------------------------------*/
void IscSetTaskName(uint8 id, uint8 task)
{
	switch(id){
	case ISC_FUNC_ID:
	{
		if(task)
			prctl(PR_SET_NAME,"Func_ISCRD");
		else
			prctl(PR_SET_NAME,"Func_ISCRWR");
	}
	break;
	case ISC_SYSD_ID:
	{
		if(task)
			prctl(PR_SET_NAME,"Sysd_ISCRD");
		else
			prctl(PR_SET_NAME,"Sysd_ISCRWR");
	}
	break;
	case ISC_TSTD_ID:
			{
		if(task)
			prctl(PR_SET_NAME,"Tstd_ISCRD");
		else
			prctl(PR_SET_NAME,"Tstd_ISCRWR");
	}
	break;
	case ISC_LOGD_ID:
	{
		if(task)
			prctl(PR_SET_NAME,"Logd_ISCRD");
		else
			prctl(PR_SET_NAME,"Logd_ISCRWR");
	}
	break;
	case ISC_HID_ID:
	{
		if(task)
			prctl(PR_SET_NAME,"Hid_ISCRD");
		else
			prctl(PR_SET_NAME,"Hid_ISCRWR");
	}
	break;
	case ISC_MIX_ID:
	{
		if(task)
			prctl(PR_SET_NAME,"Mix_ISCRD");
		else
			prctl(PR_SET_NAME,"Mix_ISCRWR");
	}
	break;
	default:
		{
		if(task)
			prctl(PR_SET_NAME,"ISCRD");
		else
			prctl(PR_SET_NAME,"ISCRWR");
		}
	break;
	}
}
/* --------------------------------------------------------------------------*/
/**
 * @brief thread init
 *
 * @param id
 * @param cb callback for this id
 *
 * @retval
 */
/* ----------------------------------------------------------------------------*/
int16_t IscThreadInit(uint8 id, uint8 task)
{
    uint16 ret = ISC_SUCCESS;
    if(id < ISC_MAX_ID)
    {
        /*Write and Read Task*/
        uint8 i = task;
        for(i = ISC_WR_TASK; i < ISC_MAX_TASK; i++)
        {
		if(ChannelMatrix[id][i].ch == INVALID_CHANNEL)
		{
			ISCLOGT("%s:ch:%d,%d is invaild",__func__,id,i);
			continue;
		}
            mThreadEntry[id][i] = (IscThreadEntry*)IscMalloc(sizeof(IscThreadEntry));
            if(mThreadEntry[id][i] != NULL)
            {
		  mThreadEntry[id][i]->instanceData = NULL;
		  mThreadEntry[id][i]->mQueueFirst = NULL;
		  mThreadEntry[id][i]->mQueueLast = NULL;
                IscThreadEntry* task = mThreadEntry[id][i];
                /*save id*/
                task->id = id;
                /*event  create*/
                if(IscEventCreate(&(task->handle)))
                {
                    ISCLOGE("%s create event error id %d index i %d", __func__,id, i);
                }
		if(IscMutexCreate(&(task->mMutex)))
		{
			ISCLOGE("%s create mutex error id: %d, index i:%d",__func__,id,i);
		}
                /*thread create*/
                if(i == ISC_WR_TASK)
                {
                    /*Write task*/
                    if(IscThreadCreate(IscAsyncWriteTaskLoop, \
                                task, ISC_DEFAULT_STACK_SIZE,0, \
                                ChannelMatrix[id][i].name, \
                                &(task->mThreadHandle) ) != ISC_RESULT_SUCCESS)
                    {
                        ISCLOGE("%s create Write thread error id %d index i %d", __func__,id, i);
                        ret = ISC_ERR_DSYSTEM;
                    }
                    else
                    {
                        ISCLOGI("%s: write task create %d success", __func__, id);
                    }
                }else
                {
                    /*Read task*/
                    if(IscThreadCreate(IscAsyncReadTaskLoop, \
                                task,ISC_DEFAULT_STACK_SIZE, 0,\
                                ChannelMatrix[id][i].name, \
                                &(task->mThreadHandle) ) != ISC_RESULT_SUCCESS)
                    {
                        ISCLOGE("%s create Read thread error id %d index i %d",__func__, id, i);
                        ret = ISC_ERR_DSYSTEM;
                    }
                    else
                    {
                        ISCLOGI("%s: read task create %d success", __func__, id);
                    }
                }
            }
        }
    }
    return  ret;
}

int16_t IscThreadDeinit(uint8 id)
{
 	uint8 i ;
        for(i = ISC_WR_TASK; i < ISC_MAX_TASK; i++)
        {
        	IscThreadEntry* task = mThreadEntry[id][i];
		if(task != NULL)
		{
			IscexitThread(task);
			IscEventDestroy(&(task->handle));
			IscMutexDestroy(&(task->mMutex));
		}
        }
    return 0;
}
