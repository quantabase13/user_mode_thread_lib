#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "list.h"

#define STACK_SIZE 32767

struct task{
    jmp_buf env;
    void *arg;
    int task_index;
    long int *stack_address;
    bool running;
    bool block;
    bool terminated;
    bool initialized;
    int thread_waited;
    struct list_head list;
};

typedef int _pthread_t;
typedef struct {
    struct task *task;
}_pthread_attr_t;

static LIST_HEAD(tasklist);

int pthread_create(_pthread_t *thread, const _pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
void pthread_exit(void *value_ptr);
int pthread_join(_pthread_t thread, void **value_ptr);
static void pthread_wrapper_init(struct task *task);
_pthread_t pthread_self(void);
static void thread_initilize();
static void scheduler(int sig);
static void yield();
static long int i64_ptr_mangle(long int p);
static long int i64_ptr_mangle_re(long int p);