#ifndef __MM_MESSAGE_H__
#define __MM_MESSAGE_H__

#include <pthread.h>
#include <stdbool.h>

#include <mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MESSAGE_ELEMENTS (16)    //20000

typedef struct message_t
{
    int                 id;
    int                 cmd;
    int                 para0;
    int                 para1;

    void*               mpData;
    int                 mDataSize;

    struct list_head    mList;
} message_t;

typedef struct message_queue_t
{
    struct list_head    mIdleMessageList;   //message_t
    struct list_head    mReadyMessageList;  //message_t
    struct list_head    mMessageBufList;    //MessagePool, sizeof(message_t)*MAX_MESSAGE_ELEMENTS

    int                 mMsgCnt;        //count in ready list
    int                 mMsgPoolCnt;    //MessagePool count

    pthread_mutex_t     mutex;
    pthread_cond_t      cond;
    bool                mWaitingFlag;
} message_queue_t;

int message_create(message_queue_t* message);
void message_destroy(message_queue_t* msg_queue);

int put_message(message_queue_t* msg_queue, message_t *msg_in);
int put_message_with_data(message_queue_t* msg_queue, message_t *msg_in);
int get_message(message_queue_t* msg_queue, message_t *msg_out);
int get_message_count(message_queue_t* message);

int wait_message_queue_not_empty(message_queue_t* msg_queue, message_t *msg_out, int timeoutMs);   //unit:ms, <=0:forever wait; >0:wait time by ms
int dry_wait_message_queue(message_queue_t* msg_queue, int timeoutMs);   //unit:ms, <=0:forever wait; >0:wait time by ms

#ifdef __cplusplus
}
#endif

#endif  //__MM_MESSAGE_H__
