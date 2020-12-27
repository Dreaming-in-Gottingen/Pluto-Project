#define MM_LOG_LEVEL 2
#define LOG_TAG "mm_test"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <mm_semaphore.h>
#include <mm_message.h>
#include <mm_log.h>

int gCnt;

void* sem_thread(void*ptr)
{
    ALOGD("sem_thread begin, waiting...");
    mm_sem_t *sem = (mm_sem_t *)ptr;
    sleep(5);       //first mm_sem_signal, then mm_sem_wait, will wait forever! but up&down is ok
    //mm_sem_wait(sem);
    mm_sem_down(sem);
    //mm_cond_wait_while(*sem, gCnt!=10);

    ALOGD("sem_thread wait finish!");
    return NULL;
}

void sem_test()
{
    ALOGD("sem_test begin!");
    mm_sem_t sem;
    pthread_t tid;

    mm_sem_init(&sem, 0);
    pthread_create(&tid, NULL, sem_thread, &sem);
    sleep(2);
    //mm_sem_signal(&sem);
    mm_sem_up(&sem);

    pthread_join(tid, NULL);
    mm_sem_deinit(&sem);
    ALOGD("sem_test end!");
}


void* msg_thread(void*ptr)
{
    ALOGD("msg_thread begin! waiting...");

    //sleep(4);
    message_queue_t *msg_queue = (message_queue_t *)ptr;
    message_t msg;
    //wait_message_queue_not_empty(msg_queue, &msg, 0);
    while (1)
    {
        int ret = wait_message_queue_not_empty(msg_queue, &msg, 2000);
        if (0 == ret) {
            ALOGD("wait one msg! id=%d, cmd=%d, mpData=[%s], mDataSz=%d",
                msg.id, msg.cmd, (char*)msg.mpData, msg.mDataSize);
            if (10 == msg.id)
                break;
        } else {
            ALOGD("not get msg! ret=%#x\n", ret);
        }
    }

    while (dry_wait_message_queue(msg_queue, 2000) != 0) {
        get_message(msg_queue, &msg);
        ALOGD("dry_wait one msg! id=%d, cmd=%d, mpData=[%s], mDataSz=%d",
            msg.id, msg.cmd, (char*)msg.mpData, msg.mDataSize);
    }

    ALOGD("msg_thread end!");

    return NULL;
}

void msg_test()
{
    ALOGD("msg_test begin!");

    message_queue_t msg_queue;
    pthread_t tid;

    message_create(&msg_queue);
    pthread_create(&tid, NULL, msg_thread, &msg_queue);

    sleep(5);
    int i = 0;
    while (i++<20)
    {
        message_t msg;
        msg.id = i;
        msg.cmd = i*10;
        char *str = malloc(48);
        sprintf(str, "hello,world. -%02d-", i);
        msg.mpData = str;
        msg.mDataSize = 48;
        put_message_with_data(&msg_queue, &msg);
        //put_message(&msg_queue, &msg);
        free(str);
        sleep(1);
    }

    pthread_join(tid, NULL);
    message_destroy(&msg_queue);

    ALOGD("msg_test end!");
}


int main()
{
    ALOGW("-----------test begin------------");
    sem_test();
    msg_test();
    ALOGW("-----------test end------------");
    return 0;
}
