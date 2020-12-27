//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 3  //warning
#define LOG_TAG "mm_message"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>

#include <mm_message.h>
#include <mm_list.h>
#include <mm_log.h>

typedef struct MessagePool
{
    MM_S8*     mpBuffer;
    MM_S32     mSize;
    struct list_head mList;
} MessagePool;

static int MessageDeepCopyMessage(message_t *pDstMsg, message_t *pSrcMsg)
{
    pDstMsg->id = pSrcMsg->id;
    pDstMsg->cmd = pSrcMsg->cmd;
    pDstMsg->para0 = pSrcMsg->para0;
    pDstMsg->para1 = pSrcMsg->para1;
    if (pDstMsg->mpData && pDstMsg->mDataSize)
    {
        free(pDstMsg->mpData);
        pDstMsg->mpData = NULL;
        pDstMsg->mDataSize = 0;
    }
    if (pSrcMsg->mpData && pSrcMsg->mDataSize>=0)
    {
        pDstMsg->mpData = malloc(pSrcMsg->mDataSize);
        if (pDstMsg->mpData)
        {
            pDstMsg->mDataSize = pSrcMsg->mDataSize;
            memcpy(pDstMsg->mpData, pSrcMsg->mpData, pSrcMsg->mDataSize);
        }
        else
        {
            ALOGE("(f:%s, l:%d) fatal error! malloc MessageData fail!", __FUNCTION__, __LINE__);
            return -1;
        }
    }

    return 0;
}

static int MessageSetMessage(message_t *pDstMsg, message_t *pSrcMsg)
{
    pDstMsg->id = pSrcMsg->id;
    pDstMsg->cmd = pSrcMsg->cmd;
    pDstMsg->para0 = pSrcMsg->para0;
    pDstMsg->para1 = pSrcMsg->para1;
    pDstMsg->mpData = pSrcMsg->mpData;
    pDstMsg->mDataSize = pSrcMsg->mDataSize;
    return 0;
}

static int MessageIncreaseIdleMessageList(message_queue_t* pThiz)
{
    MessagePool *pMsgBufPool = malloc(sizeof(MessagePool));
    if (NULL == pMsgBufPool) {
        ALOGE("(f:%s, l:%d) fatal error! malloc fail!", __FUNCTION__, __LINE__);
        return -1;
    }

    if (pThiz->mMsgPoolCnt++ > 0) {
        ALOGW("(f:%s, l:%d) too much message! mMsgPoolCnt=%d", __FUNCTION__, __LINE__, pThiz->mMsgPoolCnt);
    }

    pMsgBufPool->mSize = sizeof(message_t)*MAX_MESSAGE_ELEMENTS;
    pMsgBufPool->mpBuffer = (MM_S8*)malloc(pMsgBufPool->mSize);
    if (NULL == pMsgBufPool->mpBuffer) {
        ALOGE("(f:%s, l:%d) Failed to alloc buffer size[%zd]", __FUNCTION__, __LINE__, sizeof(message_t)*MAX_MESSAGE_ELEMENTS);
        goto _ERROR_0;
    }
    memset(pMsgBufPool->mpBuffer, 0, pMsgBufPool->mSize);
    list_add_tail(&pMsgBufPool->mList, &pThiz->mMessageBufList);

    message_t *pMsg = (message_t*)pMsgBufPool->mpBuffer;
    int i;
    for(i=0; i<MAX_MESSAGE_ELEMENTS; i++)
    {
        list_add_tail(&pMsg->mList, &pThiz->mIdleMessageList);
        pMsg++;
    }
    return 0;

_ERROR_0:
    free(pMsgBufPool);
    return -1;
}


int message_create(message_queue_t* msg_queue)
{
    int ret;

    ret = pthread_mutex_init(&msg_queue->mutex, NULL);
    if (ret != 0) {
        return -1;
    }

    pthread_condattr_t  cond_attr;
    pthread_condattr_init(&cond_attr);
#if defined(__LP64__)
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
#endif
    pthread_condattr_destroy(&cond_attr);

    ret = pthread_cond_init(&msg_queue->cond, &cond_attr);
    if (ret != 0) {
        ALOGE("(f:%s, l:%d) fatal error! pthread cond init fail", __FUNCTION__, __LINE__);
        goto _ERROR_0;
    }

    msg_queue->mWaitingFlag = false;
    msg_queue->mMsgCnt = 0;
    msg_queue->mMsgPoolCnt = 0;

    INIT_LIST_HEAD(&msg_queue->mIdleMessageList);
    INIT_LIST_HEAD(&msg_queue->mReadyMessageList);
    INIT_LIST_HEAD(&msg_queue->mMessageBufList);
    if (0 != MessageIncreaseIdleMessageList(msg_queue)) {
        goto _ERROR_1;
    }
    return 0;

_ERROR_1:
    pthread_cond_destroy(&msg_queue->cond);

_ERROR_0:
    pthread_mutex_destroy(&msg_queue->mutex);
    return -1;
}

void message_destroy(message_queue_t* msg_queue)
{
    pthread_mutex_lock(&msg_queue->mutex);

    //move ready to idle
    if (!list_empty(&msg_queue->mReadyMessageList))
    {
        message_t *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &msg_queue->mReadyMessageList, mList)
        {
            ALOGW("(f:%s, l:%d) msg destroy! unprocessed msg: id[%#x] cmd[%#x] mpData[%p] mDataSize[%d]", __FUNCTION__, __LINE__, pEntry->id, pEntry->cmd, pEntry->mpData, pEntry->mDataSize);
            //if (pEntry->mpData)
            //{
            //    free(pEntry->mpData);
            //    pEntry->mpData = NULL;
            //    pEntry->mDataSize = 0;
            //}
            list_move_tail(&pEntry->mList, &msg_queue->mIdleMessageList);
            msg_queue->mMsgCnt--;
        }
    }

    if (msg_queue->mMsgCnt != 0) {
        ALOGE("(f:%s, l:%d) fatal error! mMsgCnt[%d] != 0", __FUNCTION__, __LINE__, msg_queue->mMsgCnt);
    }

    //free internel memory by deep copy
    message_t *pEntry;
    list_for_each_entry_reverse(pEntry, &msg_queue->mIdleMessageList, mList)
    {
        if (pEntry->mpData && pEntry->mDataSize)
        {
            ALOGV("message destroy! node_info: id[%#x], cmd[%#x], mpData[%p], mDataSize[%d]", pEntry->id, pEntry->cmd, pEntry->mpData, pEntry->mDataSize);
            free(pEntry->mpData);
            pEntry->mpData = NULL;
            pEntry->mDataSize = 0;
        }
    }

    //remove all message buffer pool and its message_t node
    if (!list_empty(&msg_queue->mMessageBufList))
    {
        MessagePool *pMPEntry, *pMPTmp;
        list_for_each_entry_safe(pMPEntry, pMPTmp, &msg_queue->mMessageBufList, mList)
        {
            if(pMPEntry->mpBuffer)
            {
                free(pMPEntry->mpBuffer);
                pMPEntry->mpBuffer = NULL;
            }
            pMPEntry->mSize = 0;
            list_del(&pMPEntry->mList);
            free(pMPEntry);
        }
    }

    INIT_LIST_HEAD(&msg_queue->mMessageBufList);
    INIT_LIST_HEAD(&msg_queue->mIdleMessageList);
    INIT_LIST_HEAD(&msg_queue->mReadyMessageList);

    pthread_mutex_unlock(&msg_queue->mutex);

    pthread_cond_destroy(&msg_queue->cond);
    pthread_mutex_destroy(&msg_queue->mutex);
}


//put message without data
int put_message(message_queue_t* msg_queue, message_t *msg_in)
{
    message_t message;
    memset(&message, 0, sizeof(message_t));
    message.id = msg_in->id;
    message.cmd = msg_in->cmd;
    message.para0 = msg_in->para0;
    message.para1 = msg_in->para1;
    message.mpData = NULL;
    message.mDataSize = 0;
    return put_message_with_data(msg_queue, &message);
}

int put_message_with_data(message_queue_t* msg_queue, message_t *msg_in)
{
    int ret = 0;
    pthread_mutex_lock(&msg_queue->mutex);
    if (list_empty(&msg_queue->mIdleMessageList))
    {
        ALOGV("(f:%s, l:%d) idleMessageList are all used, malloc more!", __FUNCTION__, __LINE__);
        //dumpCallStack("Msg");
        if (0 != MessageIncreaseIdleMessageList(msg_queue)) {
            pthread_mutex_unlock(&msg_queue->mutex);
            return -1;
        }
    }

    message_t *pMsgEntry = list_first_entry(&msg_queue->mIdleMessageList, message_t, mList);
    if (0 == MessageDeepCopyMessage(pMsgEntry, msg_in))
    {
        list_move_tail(&pMsgEntry->mList, &msg_queue->mReadyMessageList);
        msg_queue->mMsgCnt++;
        ALOGV("(f:%s, l:%d) new msg cmd[%d], para[%d][%d] pData[%p] pDataSize[%d]", __FUNCTION__, __LINE__,
            pMsgEntry->cmd, pMsgEntry->para0, pMsgEntry->para1, pMsgEntry->mpData, pMsgEntry->mDataSize);
        if (msg_queue->mWaitingFlag) {
            pthread_cond_signal(&msg_queue->cond);
        }
    }
    else
    {
        ret = -1;
    }
    pthread_mutex_unlock(&msg_queue->mutex);

    return ret;
}

int get_message(message_queue_t* msg_queue, message_t *msg_out)
{
    pthread_mutex_lock(&msg_queue->mutex);

    if (list_empty(&msg_queue->mReadyMessageList))
    {
        pthread_mutex_unlock(&msg_queue->mutex);
        return -1;
    }

    message_t *pMsgEntry = list_first_entry(&msg_queue->mReadyMessageList, message_t, mList);
    MessageSetMessage(msg_out, pMsgEntry);
    list_move_tail(&pMsgEntry->mList, &msg_queue->mIdleMessageList);
    msg_queue->mMsgCnt--;

    pthread_mutex_unlock(&msg_queue->mutex);

    return 0;
}

int get_message_count(message_queue_t* msg_queue)
{
    int mMsgCnt;

    pthread_mutex_lock(&msg_queue->mutex);
    mMsgCnt = msg_queue->mMsgCnt;
    pthread_mutex_unlock(&msg_queue->mutex);

    return mMsgCnt;
}

int wait_message_queue_not_empty(message_queue_t* msg_queue, message_t *msg_out, int timeout)
{
    int ret = 0;

    pthread_mutex_lock(&msg_queue->mutex);
    msg_queue->mWaitingFlag = true;
    if (timeout <= 0)
    {
        while (list_empty(&msg_queue->mReadyMessageList)) {
            ret = pthread_cond_wait(&msg_queue->cond, &msg_queue->mutex);
        }
        message_t *pMsgEntry = list_first_entry(&msg_queue->mReadyMessageList, message_t, mList);
        MessageSetMessage(msg_out, pMsgEntry);
        list_move_tail(&pMsgEntry->mList, &msg_queue->mIdleMessageList);
        msg_queue->mMsgCnt--;
        msg_queue->mWaitingFlag = false;
    }
    else
    {
        if (list_empty(&msg_queue->mReadyMessageList))
        {
            struct timespec ts;
#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE)
            ts.tv_sec  = timeout/1000;
            ts.tv_nsec = (timeout%1000) * 1000000;
            ret = pthread_cond_timedwait_relative_np(&msg_queue->cond, &msg_queue->mutex, &ts);
#else // HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE
#if defined(HAVE_POSIX_CLOCKS)
            //clock_gettime(CLOCK_REALTIME, &ts);
            clock_gettime(CLOCK_MONOTONIC, &ts);
#else // HAVE_POSIX_CLOCKS
            // we don't support the clocks here.
            struct timeval t;
            gettimeofday(&t, NULL);
            ts.tv_sec = t.tv_sec;
            ts.tv_nsec= t.tv_usec*1000;
#endif // HAVE_POSIX_CLOCKS
            long relative_sec = timeout/1000;
            long relative_nsec = (timeout%1000)*1000000;

            ts.tv_sec += relative_sec;
            ts.tv_nsec += relative_nsec;
            ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
            ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);

            ret = pthread_cond_timedwait(&msg_queue->cond, &msg_queue->mutex, &ts);
#endif // HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE
            if (ETIMEDOUT == ret) {
                ALOGE("(f:%s, l:%d) pthread cond timeout!", __FUNCTION__, __LINE__);
            } else if (0 == ret) {
                //at last, fetch the earlist msg
                message_t *pMsgEntry = list_first_entry(&msg_queue->mReadyMessageList, message_t, mList);
                MessageSetMessage(msg_out, pMsgEntry);
                list_move_tail(&pMsgEntry->mList, &msg_queue->mIdleMessageList);
                msg_queue->mMsgCnt--;
                msg_queue->mWaitingFlag = false;
            } else {
                ALOGE("(f:%s, l:%d) fatal error! pthread cond timeout! ret[%d]", __FUNCTION__, __LINE__, ret);
            }
        }
    }

    pthread_mutex_unlock(&msg_queue->mutex);

    return ret;
}

int dry_wait_message_queue(message_queue_t* msg_queue, int timeout)
{
    int msg_cnt = 0;

    pthread_mutex_lock(&msg_queue->mutex);
    msg_queue->mWaitingFlag = true;
    if (timeout <= 0)
    {
        while (list_empty(&msg_queue->mReadyMessageList)) {
            pthread_cond_wait(&msg_queue->cond, &msg_queue->mutex);
        }
    }
    else
    {
        if (list_empty(&msg_queue->mReadyMessageList))
        {
            struct timespec ts;
#if defined(HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE)
            ts.tv_sec  = timeout/1000;
            ts.tv_nsec = (timeout%1000) * 1000000;
            int ret = pthread_cond_timedwait_relative_np(&msg_queue->cond, &msg_queue->mutex, &ts);
#else // HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE
#if defined(HAVE_POSIX_CLOCKS)
            //clock_gettime(CLOCK_REALTIME, &ts);
            clock_gettime(CLOCK_MONOTONIC, &ts);
#else // HAVE_POSIX_CLOCKS
            // we don't support the clocks here.
            struct timeval t;
            gettimeofday(&t, NULL);
            ts.tv_sec = t.tv_sec;
            ts.tv_nsec= t.tv_usec*1000;
#endif // HAVE_POSIX_CLOCKS
            long relative_sec = timeout/1000;
            long relative_nsec = (timeout%1000)*1000000;

            ts.tv_sec += relative_sec;
            ts.tv_nsec += relative_nsec;
            ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
            ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);

            int ret = pthread_cond_timedwait(&msg_queue->cond, &msg_queue->mutex, &ts);
#endif // HAVE_PTHREAD_COND_TIMEDWAIT_RELATIVE
            if (ETIMEDOUT == ret) {
                ALOGE("(f:%s, l:%d) pthread cond timeout!", __FUNCTION__, __LINE__);
            } else if (0 == ret) {
            } else {
                ALOGE("(f:%s, l:%d) fatal error! pthread cond timeout! ret[%d]", __FUNCTION__, __LINE__, ret);
            }
        }
    }

    msg_cnt = msg_queue->mMsgCnt;
    msg_queue->mWaitingFlag = false;
    pthread_mutex_unlock(&msg_queue->mutex);

    return msg_cnt;
}
