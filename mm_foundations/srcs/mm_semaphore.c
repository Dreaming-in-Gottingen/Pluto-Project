//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "mm_semaphore"

#include <pthread.h>
#include <errno.h>

#include <mm_semaphore.h>
#include <mm_log.h>


/** Initializes the semaphore at a given value
 *
 * @param sem the semaphore to initialize
 * @param val the initial value of the semaphore
 *
 */
int mm_sem_init(mm_sem_t* sem, int val)
{
    int ret;

    ret = pthread_cond_init(&sem->condition, NULL);
    if (ret != 0)
        return -1;

    ret = pthread_mutex_init(&sem->mutex, NULL);
    if (ret != 0)
        return -1;

    sem->sem_val = val;

    return 0;
}

/** Destroy the semaphore
 *
 * @param sem the semaphore to destroy
 */
void mm_sem_deinit(mm_sem_t* sem)
{
    pthread_cond_destroy(&sem->condition);
    pthread_mutex_destroy(&sem->mutex);
}

/** Decreases the value of the semaphore. Blocks if the semaphore
 * value is zero.
 *
 * @param sem the semaphore to decrease
 */
void mm_sem_down(mm_sem_t* sem)
{
    pthread_mutex_lock(&sem->mutex);

    ALOGV("semdown:%p val:%d", sem, sem->sem_val);
    while (sem->sem_val == 0)
    {
        ALOGV("semdown wait:%p val:%d", sem, sem->sem_val);
        pthread_cond_wait(&sem->condition, &sem->mutex);
        ALOGV("semdown wait end:%p val:%d", sem, sem->sem_val);
    }

    sem->sem_val--;
    pthread_mutex_unlock(&sem->mutex);
}

/** Increases the value of the semaphore
 *
 * @param sem the semaphore to increase
 */
void mm_sem_up(mm_sem_t* sem)
{
    pthread_mutex_lock(&sem->mutex);

    sem->sem_val++;
    ALOGV("semup signal:%p val:%d", sem, sem->sem_val);
    pthread_cond_signal(&sem->condition);

    pthread_mutex_unlock(&sem->mutex);
}

void mm_sem_up_unique(mm_sem_t* sem)
{
    pthread_mutex_lock(&sem->mutex);

    if(0 == sem->sem_val)
    {
        sem->sem_val++;
        pthread_cond_signal(&sem->condition);
    }

    pthread_mutex_unlock(&sem->mutex);
}

/** Reset the value of the semaphore
 *
 * @param sem the semaphore to reset
 */
void mm_sem_reset(mm_sem_t* sem)
{
    pthread_mutex_lock(&sem->mutex);

    sem->sem_val = 0;

    pthread_mutex_unlock(&sem->mutex);
}

/** Wait on the condition.
 *
 * @param sem the semaphore to wait
 */
void mm_sem_wait(mm_sem_t* sem)
{
    pthread_mutex_lock(&sem->mutex);

    pthread_cond_wait(&sem->condition, &sem->mutex);

    pthread_mutex_unlock(&sem->mutex);
}

/** Signal the condition,if waiting
 *
 * @param sem the semaphore to signal
 */
void mm_sem_signal(mm_sem_t* sem)
{
    pthread_mutex_lock(&sem->mutex);

    pthread_cond_signal(&sem->condition);

    pthread_mutex_unlock(&sem->mutex);
}

int mm_sem_get_val(mm_sem_t* sem)
{
    return sem->sem_val;
}

