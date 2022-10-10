#include "list.h"
#define __ARCH_SPIN_LOCK_UNLOCKED    {0}

#define __RAW_SPIN_LOCK_INITIALIZER(lockname)	\
{						\
	.lock = __ARCH_SPIN_LOCK_UNLOCKED,	\
}

#define __SPIN_LOCK_UNLOCKED(lockname)	\
	(spinlock_t) __RAW_SPIN_LOCK_INITIALIZER(lockname)

#define __SEMAPHORE_INITIALIZER(name, n, p)				\
{									\
	.lock		= __SPIN_LOCK_UNLOCKED((name).lock),	\
	.count		= n,						\
    .pshared		= p,						\
	.wait_list	= LIST_HEAD_INIT((name).wait_list),		\
}

#define EINVAL 2
#define EBUSY 3


typedef struct spinlock {
    int lock;
} spinlock_t;


typedef struct semaphore {
    spinlock_t lock;
    unsigned int count;
    int pshared;
    struct list_head wait_list;
} sem_t;


int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);