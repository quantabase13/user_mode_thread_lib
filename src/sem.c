#include "sem.h"
#include "pthread_lib.h"

static int ___down_common(sem_t *sem);
static void __up(sem_t *sem);


static int spin_lock(spinlock_t *lock);
static int spin_unlock(spinlock_t *lock);

int sem_init(sem_t *sem, int pshared, unsigned val)
{
    *sem = (sem_t) __SEMAPHORE_INITIALIZER(*sem, val, pshared);
}

// int sem_destroy(sem_t *sem){
//     int result = 0;
//     sem_t s = NULL;
//     if (sem == NULL || *sem == NULL){
//         result = -EINVAL;
//     }
//     s = *sem;
//     spin_lock(&sem->lock);
//     if (s.count < 0){
//         spin_unlock(&sem->lock);
//         result = -EBUSY;
//     }

//     if (result !=0){
//         return -1;
//     }
//     return 0;
// }

int sem_wait(sem_t *sem)
{
    spin_lock(&sem->lock);
    if (sem->count > 0) {
        sem->count--;
    } else {
        ___down_common(sem);
    }
    spin_unlock(&sem->lock);
    return 0;
}

int sem_post(sem_t *sem)
{
    spin_lock(&sem->lock);
    if (list_empty(&sem->wait_list))
        sem->count++;
    else
        __up(sem);
    spin_unlock(&sem->lock);
    return 0;
}

struct semaphore_waiter {
    struct list_head list;
    struct task *task;
    bool up;
};

static int ___down_common(sem_t *sem)
{
    struct semaphore_waiter waiter;

    list_add_tail(&waiter.list, &sem->wait_list);
    waiter.task = tasklist.current;
    waiter.up = false;

    for (;;) {
        spin_unlock(&sem->lock);
        tasklist.current->state = WAITING;
        yield();
        spin_lock(&sem->lock);
        if (waiter.up)
            return 0;
    }
}

static void __up(struct semaphore *sem)
{
    spin_lock(&sem->lock);
    struct semaphore_waiter *waiter =
        list_first_entry(&sem->wait_list, struct semaphore_waiter, list);
    list_del(&waiter->list);
    waiter->up = true;
    tasklist.current = waiter->task;
    waiter->task->state = RUNNING;
    spin_unlock(&sem->lock);
}



/**
 * @brief Acquire a lock and wait atomically for the lock object
 * @param lock Spinlock object
 */
static inline int spin_lock(spinlock_t *l)
{
    int out;
    volatile int *lck = &(l->lock);
    asm("whileloop:"
        "xchg %%al, (%1);"
        "test %%al,%%al;"
        "jne whileloop;"
        : "=r"(out)
        : "r"(lck));
    return 0;
}

/**
 * @brief Release lock atomically
 * @param lock Spinlock object
 */
static inline int spin_unlock(spinlock_t *l)
{
    int out;
    volatile int *lck = &(l->lock);
    asm("movl $0x0,(%1);" : "=r"(out) : "r"(lck));
    return 0;
}