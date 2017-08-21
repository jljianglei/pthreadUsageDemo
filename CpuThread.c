#include "isc.h"
#include "stdlib.h"
#include "channel_def.h"
#include "private.h"
#include "CpuIf.h"
#include "CpuThread.h"
#include "types.h"
#include "CpuExt.h"

#ifdef CPU_FOR_LINUX
#include <utils/Log.h>
#else
#include "stc_private.h"
#endif

#ifdef  __cplusplus
extern "C" {
#endif

static int8 iscWriteRes[ISC_MAX_ID] ={ISC_SUCCESS};
static uint8 reSendCount[ISC_MAX_ID] ={0};
/* include read thread & write thread*/
 IscThreadEntry* mThreadEntry[ISC_MAX_ID][ISC_MAX_TASK] = {{NULL, NULL},};
 IscReceivedMsg mReceiveCb[ISC_MAX_ID];
 const ISC_CHANNALE_MATRIX_T ChannelMatrix[ISC_MAX_ID][ISC_MAX_TASK] =
{
    {{FUNC_WR_CHANNEL, "FuncWr"}, {FUNC_RD_CHANNEL, "FuncRd"}},
    {{SYSD_WR_CHANNEL,"SysdWr"}, {SYSD_RD_CHANNEL, "SysdRd"}},
    {{TESTMODE_WR_CHANNEL, "TstWr"}, {TESTMODE_RD_CHANNEL, "TstRd"}},
    {{LOG_WR_CHANNEL, "LogWr"}, {LOG_RD_CHANNEL, "LogRd"}},
    {{INVALID_CHANNEL,"InvaildWr"},{INVALID_CHANNEL,"InvaildRd"}},
    {{INVALID_CHANNEL,"InvaildWr"},{INVALID_CHANNEL,"InvaildRd"}},
    {{HID_WR_CHANNEL,"HidWr"},{HID_RD_CHANNEL,"HidRd"}},
    {{MIX_WR_CHANNEL,"MixWr"},{MIX_RD_CHANNEL,"MixRd"}},
    {{INVALID_CHANNEL,"InvaildWr"},{ITRONECNS_RD_CHANNEL,"EcnsRd"}},
};

static void IscPutMessage(uint8 id, uint8* msg, uint16 len);
static uint8 IscGetOneMessage(IscThreadEntry * task, uint8 **msg, uint16* len);

IscThreadEntry* IscGetTaskEntry(uint8 id, uint8 task)
{
    if(id >=ISC_MAX_ID|| task >= ISC_MAX_TASK)
        return NULL;

   return  mThreadEntry[id][task];
}

IscThreadEntry* IscAllocTaskEntry(uint8 id, uint8 task)
{
    if(id >=ISC_MAX_ID|| task >= ISC_MAX_TASK)
    {
        return NULL;
    }
	mThreadEntry[id][task] =  (IscThreadEntry*)IscMalloc(sizeof(IscThreadEntry));
	if(mThreadEntry[id][task] != NULL)
	{
		mThreadEntry[id][task]->instanceData = NULL;
		mThreadEntry[id][task]->mQueueFirst = NULL;
		mThreadEntry[id][task]->mQueueLast = NULL;
	}
    return mThreadEntry[id][task];
}

static void API_BUFFER_DUMP(char* tmp, uint16 maxLen, void* msg, uint16 len)
{
    uint16 i = 0;

    if(len > 20)
        len = 20;
    snprintf(tmp+strlen(tmp), maxLen-strlen(tmp), "[");
    if((msg == NULL) || (len == 0))
    {
        snprintf(tmp+strlen(tmp), maxLen-strlen(tmp), " ");
    }
    else
    {
        for(; i < len; i++)
        {
            snprintf(tmp+strlen(tmp), maxLen-strlen(tmp), "0x%.2X,",*((unsigned char*)msg + i));
        }
    }
    snprintf(tmp+strlen(tmp), maxLen-strlen(tmp), "]");
}

void IscAsyncReadTaskLoop(void* data)
{

    IscThreadEntry* task = (IscThreadEntry*)data;
    uint8 result = ISC_SUCCESS;
    uint8 id = task->id;
    uint32 eventBits;
    task->running = 1;
    uint32 channel = ChannelMatrix[id][ISC_RD_TASK].ch;

    if(channel == INVALID_CHANNEL)
    {
        ISCLOGI("Func: %s,ch:%x if invalid!", __func__,channel);
        return;
    }
    IscSetTaskName(id,ISC_RD_TASK);
    ISCLOGI("Func: %s", __func__);

    if(task != NULL)
    {
        while(task->running)
        {
            int err = 0;
            uint8* buf = NULL;
		int hasdata = 1;
            eventBits = 0;
            /*wait for exit event && delay 10ms for read from shared memory*/
            if(channel<=ISC_MAX_NORMAL_CHANNEL)
            {
                result = IscEventWait(&(task->handle), 3, &eventBits);
            }
            if((result == ISC_SUCCESS) && (eventBits & ISC_EXIT_EVENT))
            {
                task->running = 0;
                break;
            }
		while(hasdata)
		{
	            /*read msg*/
	            if(id == ISC_FUNC_ID)
	            {
	                err = IscRead(channel, &buf);
	            }else
	            {
	                err = IscSRead(channel, &buf);
	            }

	            if(err > 0 && buf != NULL)
	            {
	                if(mReceiveCb[id] != NULL)
	                {
	                    char tmp[1024];
	                    memset(tmp, 0, sizeof(tmp));
	                    API_BUFFER_DUMP(tmp, 1024, buf, err);
	                    ISCLOGT("Callback: %s length %d id %d,%s ", __func__, err, id,tmp);
	                    (mReceiveCb[id])(buf, err);
	                }
			}
			else
			{
				hasdata = 0;
			}
	            if(buf)
	            {
	                IscFree(buf);
	            }
		}
        }
    }
ISCLOGT("%s,@@@@@@@@@@@@@@EXIT FUNCION,id:%d",__func__,id);
}


void IscAsyncWriteTaskLoop(void* data)
{
    IscThreadEntry* task = (IscThreadEntry*)data;
    IscResult result;
    uint8 id = task->id;
    uint32 eventBits = 0;

    uint32 channel = ChannelMatrix[id][ISC_WR_TASK].ch;
    if(channel == INVALID_CHANNEL)
    {
        ISCLOGI("Func: %s,ch:%x if invalid!", __func__,channel);
        return;
    }
    IscSetTaskName(id,ISC_WR_TASK);
    ISCLOGI("Func: %s", __func__);
    task->running = 1;
    /*set thread name todo */
    if(task != NULL)
    {
        while(task->running)
        {
            eventBits = 0;
            result = IscEventWait(&(task->handle), ISC_EVENT_WAIT_INFINITE, &eventBits);
            if(result == ISC_RESULT_SUCCESS && eventBits != 0)
            {
		uint8* message = NULL;
		uint16 len;
                /*exit event*/
                if(eventBits & ISC_EXIT_EVENT)
                {
                    task->running = 0;
                    /*exit task*/
			while(IscGetOneMessage(task, &message, &len) == 0x00)
			{
				if(message != NULL)
				{
	                            IscFree(message);
					message = NULL;
				}
			}
                    break;
                }
                /*have msg*/
                if(eventBits | ISC_MSG_EVENT)
                {
                    /*received send msg*/
                    ISCLOGT("**********************%s id %d  task  %p ********************", __func__, id, task);
                    while(IscGetOneMessage(task, &message, &len) == 0x00)
                    {
                        char tmp[1024];
                        memset(tmp, 0, sizeof(tmp));
                        API_BUFFER_DUMP(tmp, 1024, message, len);
                        ISCLOGT("%s,*********Write*****,%d,%s",__func__, id,tmp);
                        iscWriteRes[id] = IscWrite(channel, message, len);
                        if(iscWriteRes[id]  < ISC_SUCCESS)
                        {
                            ISCLOGE("ISC write error, the errID:%d",iscWriteRes[id]);
                            if(iscWriteRes[id] == ISC_ERR_NOMEM)
                            {
                                if(reSendCount[id] > 4)
                                {
                                    ISCLOGE("****the buffer is full, cannot write data again");
                                }
                                else{
                                    reSendCount[id]++;
                                    IscThreadSleep(10);
                                    IscPutMessage(id,message,len);
					continue;
                                }
                            }
                            else
                            {
                                ISCLOGE("ISC write error, the errID:%d",iscWriteRes[id]);
                            }
                        }
                        else
                        {
                            reSendCount[id] =0;
                        }
                        if(message != NULL)
                        {
                            IscFree(message);
				message = NULL;
                        }
                    }
                }
            }
        }
    }

ISCLOGT("%s,@@@@@@@@@@@@@@EXIT FUNCION,id:%d",__func__,id);

}

static uint8 IscGetOneMessage(IscThreadEntry * task, uint8 **msg, uint16* len)
{
    uint8 flag = 0XFF;
    if(task != NULL)
    {
        IscMsgQueueEntry* message = NULL;
        IscMutexLock(&(task->mMutex));
        if(task->mQueueFirst != NULL)
        {
            message = task->mQueueFirst;
            if(message)
            {
                if(msg)
                {

                    *msg = (uint8*)(message->message);
                }
                if(len)
                {
                    *len = message->event;
                }
            }
            task->mQueueFirst = message->next;
            if(task->mQueueLast == message)
            {
                task->mQueueLast = NULL;
            }
            message->next = NULL;
            /*IscFree message*/
            IscFree(message);
            flag = 0x00;
        }
        IscMutexUnlock(&(task->mMutex));
    }
    return flag;
}

static void IscPutMessage(uint8 id, uint8* msg, uint16 len)
{
    if(id > ISC_MAX_ID)
    {
        ISCLOGE("**********************%s id %d  over", __func__, id);
        return;
    }

    IscThreadEntry* task = mThreadEntry[id][ISC_WR_TASK];
    if(task != NULL)
    {
        IscMsgQueueEntry* message = (IscMsgQueueEntry*)IscMalloc(sizeof(IscMsgQueueEntry));
        if(message != NULL)
        {
            message->message = msg;
            message->next = NULL;
            message->event = len;
        }else
        {
            IscFree(msg);
            return;
        }
        ISCLOGT("**********************%s id %d  task  %p ********************", __func__, id, task);
        IscMutexLock(&(task)->mMutex);
        if(task->mQueueLast == NULL)
        {
            task->mQueueFirst = message;
            task->mQueueLast = message;
        }else
        {
            task->mQueueLast->next = message;
            task->mQueueLast = message;
        }
        IscMutexUnlock(&(task->mMutex));
        IscEventSet(&(task->handle), ISC_MSG_EVENT);

    }
    else
    {
        IscFree(msg);
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief  send message to the read
 *
 * @param id
 * @param message
 * @param length
 *
 * @retval
 */
/* ----------------------------------------------------------------------------*/
uint8 IscSendMessage(uint8 id, uint8 mix_id,  uint8* message, uint16 length)
{
    uint8* msg = NULL;
    uint16 len = length;
    if(mix_id != 0)
    {
        len = length + 1;/*1 byte to same mix_id*/
    }

    if(iscWriteRes[id] < 0)
        return iscWriteRes[id];

    msg = (uint8*) IscMalloc(len);
    if(msg != NULL && message != NULL)
    {
        char tmp[224];
        memset(tmp, 0, sizeof(tmp));
        API_BUFFER_DUMP(tmp, 1024, message, length);
        ISCLOGT("*********Write*****%s", tmp);
        /*for mix channel to same the mix id*/
        if(mix_id != 0)
        {
            msg[0] = mix_id;
            memcpy(&msg[1], message, length);
        }
        else
        {

            memcpy(&msg[0], message, length);
        }
        ISCLOGT("%s message %p length %d", __func__, msg, len);
        IscPutMessage(id, msg, len);
        return ISC_SUCCESS;
    }
    return ISC_ERR_ALLOC;
}

int16 IscDirectWrite(uint8 id, uint8_t* buf, uint16_t bufLen)
{
    uint32 channel = ChannelMatrix[id][ISC_WR_TASK].ch;
	if(channel!= INVALID_CHANNEL)
		return IscWrite(channel, buf, bufLen);
	return ISC_INVALID_CHANNEL;
}

int16 IscDirectRead(uint id, uint8_t *buf)
{
	uint32 channel = ChannelMatrix[id][ISC_RD_TASK].ch;
	if(channel!= INVALID_CHANNEL)
		return IscRead(channel,buf);
	return ISC_INVALID_CHANNEL;
}


void IscexitThread(IscThreadEntry *task)
{
	 if(task == NULL)
		return;
	IscEventSet(&(task->handle), ISC_EXIT_EVENT);
}
uint8 IscRegisterCb(uint8 id, IscReceivedMsg cb)
{
    if(id < ISC_MAX_ID)
    {
        mReceiveCb[id] = cb;
    }
    else
    {
        ISCLOGE("%s, the param is invaild",__func__);
        return ISC_ERR_DINVAL;
    }
    ISCLOGT("%s id %d register success", __func__, id);
    return ISC_SUCCESS;
}

uint8 IscUnRegisterCb(uint8 id)
{
    if(id < ISC_MAX_ID)
    {
        mReceiveCb[id] = NULL;
    }
    else
    {
        ISCLOGE("%s, the param is invaild",__func__);
        return ISC_ERR_DINVAL;
    }
    ISCLOGT("%s id %d unregister success", __func__, id);
    return ISC_SUCCESS;
}

#ifdef  __cplusplus
}
#endif

