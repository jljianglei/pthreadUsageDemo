#ifndef __CPU_THREAD_H__
#define __CPU_THREAD_H__

#include "types.h"
#include "CpuExt.h"
#include "CpuIf.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define ISC_DEFAULT_STACK_SIZE (1024*32)

/* Event types */
#define TIMEOUT_EVENT    0x00020000
#define ISC_EXIT_EVENT 0x00400000
#define ISC_MSG_EVENT    0x01000000
/* --------------------------------------------------------------------------*/
/**
* @brief
*/
/* ----------------------------------------------------------------------------*/
typedef struct Message
{
    uint8* message;
    uint16 length;
}ISC_WRITE_MSG_T;

/* --------------------------------------------------------------------------*/
/**
 * @brief  Message Queue Type Definition
 */
/* ----------------------------------------------------------------------------*/
typedef struct MsgQueueEntryTag
{
    struct MsgQueueEntryTag *next;
    void * message;
    uint16 event;
}IscMsgQueueEntry;


typedef struct
{
    uint8 id;
    IscMutexHandle  mMutex;
    void* instanceData;
    IscMsgQueueEntry* mQueueFirst;
    IscMsgQueueEntry* mQueueLast;
    IscEventHandle handle;
    IscThreadHandle mThreadHandle;
    uint8 running;           /*sched running flag*/
}IscThreadEntry;

void IscexitThread(IscThreadEntry *task);
int16_t IscThreadInit(uint8 id, uint8 task);
int16_t IscThreadDeinit(uint8 id);
IscThreadEntry* IscGetTaskEntry(uint8 id, uint8 task);
IscThreadEntry* IscAllocTaskEntry(uint8 id, uint8 task);
void IscAsyncReadTaskLoop(void* data);

void IscAsyncWriteTaskLoop(void* data);

#ifdef  __cplusplus
}
#endif
#endif
