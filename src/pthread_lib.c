#include "pthread_lib.h"
#include <sys/time.h>
#define TIME_QUANTUM 500000 /* in us */

/* timer management */
static struct itimerval time_quantum;

static void pthread_wrapper(struct task *task);
static void pthread_subsystem_init();
static void pthread_scheduler(int sig);
static long int i64_ptr_mangle(long int p);
static long int i64_ptr_mangle_re(long int p);

/* global spinlock for critical section _queue */
static uint _spinlock = 0;
static int thread_index = 0;

jmp_buf context_main;

int pthread_create(_pthread_t *thread,
                   const _pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *args)
{
    static bool first_run = true;
    if (first_run) {
        first_run = false;
        pthread_subsystem_init();
    }

    struct task *task = malloc(sizeof(struct task) + 1 + STACK_SIZE);

    (task->env)[0].__jmpbuf[6] =
        i64_ptr_mangle((unsigned long long) (task->stack + STACK_SIZE));
    (task->env)[0].__jmpbuf[7] =
        i64_ptr_mangle((unsigned long long) start_routine);

    task->state = INITIALIZED;
    task->thread_waited = 0;
    task->args = args;
    task->thread_index = thread_index;
    *thread = thread_index;
    thread_index++;
    INIT_LIST_HEAD(&task->list);
    while (__atomic_test_and_set(&_spinlock, __ATOMIC_ACQUIRE))
        ;
    list_add_tail(&task->list, &(tasklist.task));
    __atomic_store_n(&_spinlock, 0, __ATOMIC_RELEASE);
    struct sigaction sched_handler = {
        .sa_flags = SA_NODEFER,
        .sa_handler =
            &pthread_scheduler, /* set signal handler to call scheduler */
    };
    sigaction(SIGPROF, &sched_handler, NULL);

    setitimer(ITIMER_PROF, &time_quantum, NULL);
    setjmp(context_main);
    return 0;
}


static void pthread_scheduler(int sig)
{
    static bool first_run = true;
    if (list_empty(&(tasklist.task))) {
        return;
    }
    if (first_run) {
        first_run = false;
        // TODO: add lock to protect tasklist
        while (__atomic_test_and_set(&_spinlock, __ATOMIC_ACQUIRE))
            ;
        struct task *task_first =
            list_first_entry(&(tasklist.task), struct task, list);
        __atomic_store_n(&_spinlock, 0, __ATOMIC_RELEASE);
        task_first->state = RUNNING;
        tasklist.current = task_first;
        // end TODO
        pthread_wrapper(task_first);
    }

    while (__atomic_test_and_set(&_spinlock, __ATOMIC_ACQUIRE))
        ;
    struct task *task_next =
        list_entry((tasklist.current)->list.next, struct task, list);
    if (&task_next->list == &(tasklist.task)) {
        task_next = list_first_entry(&(tasklist.task), struct task, list);
    }
    __atomic_store_n(&_spinlock, 0, __ATOMIC_RELEASE);
    switch (setjmp((tasklist.current)->env)) {
    case 0:
        if (task_next->state == INITIALIZED) {
            task_next->state = RUNNING;
            tasklist.current = task_next;
            pthread_wrapper(task_next);
        } else {
            task_next->state = RUNNING;
            tasklist.current = task_next;
            longjmp(task_next->env, 1);
        }
    case 1:
        return;
    }
    /*context switch code end*/
}

void yield()
{
    jmp_buf task_current;
    setjmp(task_current);

    long int rbp_encrypted = task_current[0].__jmpbuf[1];
    long int rbp = i64_ptr_mangle_re(rbp_encrypted);
    long int rbp_former = *((long int *) (rbp));
    long int rip_former = *(long int *) (rbp + 8);
    long int rsp_former = rbp + 16;
    task_current[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
    task_current[0].__jmpbuf[7] = i64_ptr_mangle(rip_former);
    task_current[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);
    memcpy((tasklist.current)->env, task_current, sizeof(jmp_buf));

    struct task *task_next =
        list_entry((tasklist.current)->list.next, struct task, list);
    if (&task_next->list == &(tasklist.task)) {
        task_next = list_first_entry(&(tasklist.task), struct task, list);
    }

    switch (task_next->state) {
    case INITIALIZED: {
        task_next->state = RUNNING;
        tasklist.current = task_next;
        pthread_wrapper(task_next);
    }

    default:
        tasklist.current = task_next;
        longjmp(task_next->env, 1);
    }
}

_pthread_t pthread_self(void)
{
    return (_pthread_t)(tasklist.current)->thread_index;
}

void pthread_exit(void *value_ptr)
{
    while (__atomic_test_and_set(&_spinlock, __ATOMIC_ACQUIRE))
        ;
    struct task *task_next =
        list_entry((tasklist.current)->list.next, struct task, list);
    if (&task_next->list == &(tasklist.task)) {
        task_next = list_first_entry(&(tasklist.task), struct task, list);
    }
    __atomic_store_n(&_spinlock, 0, __ATOMIC_RELEASE);
    list_del(&(tasklist.current)->list);
    if (list_empty(&tasklist.task)) {
        longjmp(context_main, 1);
    } else {
        if (task_next->state == INITIALIZED) {
            task_next->state = RUNNING;
            tasklist.current = task_next;
            pthread_wrapper(task_next);
        } else {
            task_next->state = RUNNING;
            tasklist.current = task_next;
            longjmp(task_next->env, 1);
        }
    }
}

static void pthread_subsystem_init()
{
    tasklist = (tasklist_t) TASKLIST_INITIALIZER(tasklist);
    time_quantum.it_value.tv_sec = 0;
    time_quantum.it_value.tv_usec = TIME_QUANTUM;
    time_quantum.it_interval.tv_sec = 0;
    time_quantum.it_interval.tv_usec = TIME_QUANTUM;
}

static void pthread_wrapper(struct task *task)
{
    long int rsp_encrypted = (task->env)[0].__jmpbuf[6];
    long int rsp = i64_ptr_mangle_re(rsp_encrypted);
    long int pc_encrypted = (task->env)[0].__jmpbuf[7];
    long int pc = i64_ptr_mangle_re(pc_encrypted);
    void *value_ptr;
    asm("mov %0, %%rsp;" : : "r"(rsp));
    ((void (*)(void *))(pc))(task->args);
    // asm("mov %%rax, %0;" : "=r"(value_ptr)::);
    while (__atomic_test_and_set(&_spinlock, __ATOMIC_ACQUIRE))
        ;
    struct task *task_next =
        list_entry((tasklist.current)->list.next, struct task, list);
    if (&task_next->list == &(tasklist.task)) {
        task_next = list_first_entry(&(tasklist.task), struct task, list);
    }
    __atomic_store_n(&_spinlock, 0, __ATOMIC_RELEASE);
    list_del(&task->list);
    if (list_empty(&tasklist.task)) {
        longjmp(context_main, 1);
    } else {
        if (task_next->state == INITIALIZED) {
            task_next->state = RUNNING;
            tasklist.current = task_next;
            pthread_wrapper(task_next);
        } else {
            task_next->state = RUNNING;
            tasklist.current = task_next;
            longjmp(task_next->env, 1);
        }
    }
}


static long int i64_ptr_mangle(long int p)
{
    long int ret;
    asm(" mov %1, %%rax;\n"
        " xor %%fs:0x30, %%rax;"
        " rol $0x11, %%rax;"
        " mov %%rax, %0;"
        : "=r"(ret)
        : "r"(p)
        : "%rax");
    return ret;
}

static long int i64_ptr_mangle_re(long int p)
{
    long int ret;
    asm(" mov %1, %%rax;\n"
        " ror $0x11, %%rax;"
        " xor %%fs:0x30, %%rax;"
        " mov %%rax, %0;"
        : "=r"(ret)
        : "r"(p)
        : "%rax");
    return ret;
}