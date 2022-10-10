#include <setjmp.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"


#define STACK_SIZE 32767
#define TASKLIST_INITIALIZER(name)\
{									\
	.current		= NULL,						\
	.task	= LIST_HEAD_INIT((name).task),		\
}


struct task{
    jmp_buf env;
    long int *stack;
    enum{
        INITIALIZED,
        RUNNING,
        WAITING,
        TERMINATED
    }state;
    void *args;
    void *retval;
    int thread_index;
    int thread_waited;
    struct list_head list;
};

typedef int _pthread_t;
typedef struct {
    struct task *task;
}_pthread_attr_t;

typedef struct tasklist{
    struct task* current;
    struct list_head task;
}tasklist_t;

tasklist_t tasklist;
// TASKLIST_INITIALIZER(tasklist);

int pthread_create(_pthread_t *thread, const _pthread_attr_t *attr, void*(*start_routine)(void* ), void* args);
void pthread_exit(void *value_ptr);
void yield();
_pthread_t pthread_self(void);