#ifndef __MM_SEMAPHORE__
#define __MM_SEMAPHORE__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mm_sem_t
{
    pthread_cond_t condition;
    pthread_mutex_t mutex;
    int sem_val;
} mm_sem_t;

int mm_sem_init(mm_sem_t* sem, int val);
void mm_sem_deinit(mm_sem_t* sem);

void mm_sem_down(mm_sem_t* sem);
void mm_sem_up(mm_sem_t* sem);
void mm_sem_up_unique(mm_sem_t* sem);

void mm_sem_wait(mm_sem_t* sem);
void mm_sem_signal(mm_sem_t* sem);

void mm_sem_reset(mm_sem_t* sem);
int mm_sem_get_val(mm_sem_t* sem);


#define mm_cond_wait_while_exp(sem, expression) \
    pthread_mutex_lock(&sem.mutex); \
    while (expression) { \
        pthread_cond_wait(&sem.condition, &sem.mutex); \
    } \
    pthread_mutex_unlock(&sem.mutex);

#define mm_cond_wait_if_exp(sem, expression) \
    pthread_mutex_lock(&sem.mutex); \
    if (expression) { \
        pthread_cond_wait(&sem.condition, &sem.mutex); \
    } \
    pthread_mutex_unlock(&sem.mutex);

#ifdef __cplusplus
}
#endif

#endif  //__MM_SEMAPHORE__
