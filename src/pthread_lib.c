#include "pthread_lib.h"


static void pthread_wrapper(struct task *task);
static void pthread_subsystem_init();
static void pthread_scheduler(int sig);
static long int i64_ptr_mangle(long int p);
static long int i64_ptr_mangle_re(long int p);


int pthread_create(_pthread_t *thread, const _pthread_attr_t *attr, void*(*start_routine)(void*), void *args)
{
    static bool first_run = true;
    static int thread_index = 1;
    if (first_run){
        first_run = false;
         pthread_subsystem_init();
    }
    
    struct task *task = malloc(sizeof(struct task));
    long int *stack = (long int *) malloc(STACK_SIZE);
    stack += STACK_SIZE >>2;
    
    (task->env)[0].__jmpbuf[6] = i64_ptr_mangle((long int)stack);
    (task->env)[0].__jmpbuf[7] = i64_ptr_mangle((long int)start_routine);

    task->state = INITIALIZED;
    task->thread_waited = 0;
    task->args = args;
    task->stack = stack;
    task->thread_index = thread_index;
    *thread = thread_index;
    thread_index++;
    INIT_LIST_HEAD(&task->list);
    list_add_tail(&task->list, &(tasklist.task));

    return 0;
}


static void pthread_scheduler(int sig)
{
    static bool first_run = true;
    if (list_empty(&(tasklist.task))){
        return;
    }
    if (first_run){
        first_run = false;
        //TODO: add lock to protect tasklist
        struct task *task_first = list_first_entry(&(tasklist.task), struct task, list);
        task_first->state = RUNNING;
        tasklist.current = task_first;
        //end TODO
        pthread_wrapper(task_first);
    }
    
    jmp_buf task_current;
    setjmp(task_current);

/*context switch code start*/
    long int rbp_encrypted = task_current[0].__jmpbuf[1];
    long int rbp = i64_ptr_mangle_re(rbp_encrypted);
    long int rbp_former = *((long int*)(rbp));
    long int rip_former = *(long int*)(rbp +8);
    long int rsp_former = rbp + 16;
    task_current[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
    task_current[0].__jmpbuf[7]=i64_ptr_mangle(rip_former); 
    task_current[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);
    memcpy((tasklist.current)->env, task_current, sizeof(jmp_buf));

    struct task *task_next = list_entry((tasklist.current)->list.next, struct task, list);
    if (&task_next->list  == &(tasklist.task)){
        task_next = list_first_entry(&(tasklist.task), struct task, list);
    }

    switch(task_next->state){
        case INITIALIZED:{
            task_next->state = RUNNING;
            tasklist.current = task_next;
            pthread_wrapper(task_next);
        }
     
        default:
            tasklist.current = task_next;
            longjmp(task_next->env, 1);
    }
    /*context switch code end*/

}

void yield()
{
    jmp_buf task_current;
    setjmp(task_current);

    long int rbp_encrypted = task_current[0].__jmpbuf[1];
    long int rbp = i64_ptr_mangle_re(rbp_encrypted);
    long int rbp_former = *((long int*)(rbp));
    long int rip_former = *(long int*)(rbp +8);
    long int rsp_former = rbp + 16;
    task_current[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
    task_current[0].__jmpbuf[7]=i64_ptr_mangle(rip_former); 
    task_current[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);
    memcpy((tasklist.current)->env, task_current, sizeof(jmp_buf));

    struct task *task_next = list_entry((tasklist.current)->list.next, struct task, list);
    if (&task_next->list  == &(tasklist.task)){
        task_next = list_first_entry(&(tasklist.task), struct task, list);
    }

    switch(task_next->state){
        case INITIALIZED:{
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
    struct task *task_next = list_entry((tasklist.current)->list.next, struct task, list);
    tasklist.current->state = TERMINATED;
    tasklist.current->retval = value_ptr;
    long int *stack = tasklist.current->stack - (STACK_SIZE>>2);
    free(stack);
    

    switch(task_next->state){
        case INITIALIZED:{
            task_next->state = RUNNING;
            tasklist.current = task_next;
            pthread_wrapper(task_next);
        }
        default:
            tasklist.current = task_next;
            longjmp(task_next->env, 1);
        }
   
}

static void pthread_subsystem_init()
{
    tasklist = (tasklist_t)TASKLIST_INITIALIZER(tasklist);
    struct sigaction *act = malloc(sizeof(struct sigaction));
    act->sa_handler = pthread_scheduler;
    act->sa_flags = SA_NODEFER;
    sigaction(SIGALRM, act, NULL);
    ualarm(500,500);
}

static void pthread_wrapper(struct task *task)
{
    long int rsp_encrypted = (task->env)[0].__jmpbuf[6];
    long int rsp = i64_ptr_mangle_re(rsp_encrypted);
    long int pc_encrypted = (task->env)[0].__jmpbuf[7];
    long int pc = i64_ptr_mangle_re(pc_encrypted);
    void *value_ptr;
    asm("mov %0, %%rsp;" : :"r"(rsp)); 
    ((void(*)(void *)) (pc)) (task->args);
    asm("mov %%rax, %0;":"=r"(value_ptr)::);
    pthread_exit(value_ptr);
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
    : "%rax"
    );
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
    : "%rax"
    );
    return ret;
}