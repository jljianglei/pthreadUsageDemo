#ifndef __CPU_EXT_H_
#define __CPU_EXT_H_

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef uint16 IscResult;

#define ISC_RESULT_SUCCESS  ((IscResult) 0x0000)
#define ISC_RESULT_FAILURE  ((IscResult) 0xFFFF)

/* Result codes */ #define ISC_RESULT_NO_MORE_EVENTS    ((IscResult) 0x0001)
#define ISC_RESULT_INVALID_POINTER   ((IscResult) 0x0002)
#define ISC_RESULT_INVALID_HANDLE    ((IscResult) 0x0003)
#define ISC_RESULT_NO_MORE_MUTEXES   ((IscResult) 0x0004)
#define ISC_RESULT_TIMEOUT           ((IscResult) 0x0005)
#define ISC_RESULT_NO_MORE_THREADS   ((IscResult) 0x0006)
#define ISC_RESULT_NO_MORE_TIMERS    ((IscResult) 0x0007)

#define ISC_EVENT_WAIT_INFINITE         ((uint16) 0xFFFF)

typedef pthread_mutex_t IscMutexHandle;
typedef pthread_t IscThreadHandle;

typedef struct IscEvent
{
    pthread_cond_t event;
    pthread_mutex_t mutex;
    uint32 eventBits;
}IscEventHandle;

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscEventCreate
 *
 *  DESCRIPTION
 *      Creates an event and returns a handle to the created event.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS          in case of success
 *          ISC_RESULT_NO_MORE_EVENTS   in case of out of event resources
 *          ISC_RESULT_INVALID_POINTER  in case the eventHandle pointer is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscEventCreate(IscEventHandle *eventHandle);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscEventWait
 *
 *  DESCRIPTION
 *      Wait for one or more of the event bits to be set.
 *      It is not possible to pass a bit mask in eventBits
 *      to wait for -- eventBits is an output variable only.
 *      If the wait times out before any events are signalled,
 *      the eventBits variable is zeroed.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS              in case of success
 *          ISC_RESULT_TIMEOUT              in case of timeout
 *          ISC_RESULT_INVALID_HANDLE       in case the eventHandle is invalid
 *          ISC_RESULT_INVALID_POINTER      in case the eventBits pointer is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscEventWait(IscEventHandle *eventHandle, uint16 timeoutInMs, uint32 *eventBits);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscEventSet
 *
 *  DESCRIPTION
 *      Set an event.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS              in case of success
 *          ISC_RESULT_INVALID_HANDLE       in case the eventHandle is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscEventSet(IscEventHandle *eventHandle, uint32 eventBits);

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

void IscEventDestroy(IscEventHandle *eventHandle);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscMutexCreate
 *
 *  DESCRIPTION
 *      Create a mutex and return a handle to the created mutex.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_NO_MORE_MUTEXES   in case of out of mutex resources
 *          ISC_RESULT_INVALID_POINTER   in case the mutexHandle pointer is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscMutexCreate(IscMutexHandle *mutexHandle);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscMutexLock
 *
 *  DESCRIPTION
 *      Lock the mutex refered to by the provided handle.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_INVALID_HANDLE    in case the mutexHandle is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscMutexLock(IscMutexHandle *mutexHandle);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscMutexUnlock
 *
 *  DESCRIPTION
 *      Unlock the mutex refered to by the provided handle.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_INVALID_HANDLE    in case the mutexHandle is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscMutexUnlock(IscMutexHandle *mutexHandle);

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

void IscMutexDestroy(IscMutexHandle *mutexHandle);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscGlobalMutexLock
 *
 *  DESCRIPTION
 *      Lock the global mutex. The global mutex is a single pre-initialised
 *      shared mutex, spinlock or similar that does not need to be created prior
 *      to use. The limitation is that there is only one single lock shared
 *      between all code. Consequently, it must only be used very briefly to
 *      either protect simple one-time initialisation or to protect the creation
 *      of a dedicated mutex by calling IscMutexCreate.
 *
 *----------------------------------------------------------------------------*/

void IscGlobalMutexLock(void);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscGlobalMutexUnlock
 *
 *  DESCRIPTION
 *      Unlock the global mutex.
 *
 *----------------------------------------------------------------------------*/

void IscGlobalMutexUnlock(void);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadCreate
 *
 *  DESCRIPTION
 *      Create thread function and return a handle to the created thread.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS           in case of success
 *          ISC_RESULT_NO_MORE_THREADS   in case of out of thread resources
 *          ISC_RESULT_INVALID_POINTER   in case one of the supplied pointers is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscThreadCreate(void (*threadFunction)(void *pointer), void *pointer,
                          uint32 stackSize, uint16 priority,
                          const int8 *threadName, IscThreadHandle *threadHandle);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadGetHandle
 *
 *  DESCRIPTION
 *      Return thread handle of calling thread.
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS             in case of success
 *          ISC_RESULT_INVALID_POINTER  in case the threadHandle pointer is invalid
 *
 *----------------------------------------------------------------------------*/

IscResult IscThreadGetHandle(IscThreadHandle *threadHandle);

/*----------------------------------------------------------------------------*
 *  NAME
 *      IscThreadEqual
 *
 *  DESCRIPTION
 *      Compare thread handles
 *
 *  RETURNS
 *      Possible values:
 *          WIFI_RESULT_SUCCESS             in case thread handles are identical
 *          ISC_RESULT_INVALID_POINTER  in case either threadHandle pointer is invalid
 *          WIFI_RESULT_FAILURE             otherwise
 *
 *----------------------------------------------------------------------------*/

IscResult IscThreadEqual(IscThreadHandle *threadHandle1, IscThreadHandle *threadHandle2);

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

void IscThreadSleep(uint16 sleepTimeInMs);

void IscSetTaskName(uint8 id, uint8 task);

#ifdef __cplusplus
}
#endif

#endif /*__ISC_EXT_H_*/
