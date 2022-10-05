#include "pthread_lib.h"


int pthread_create(_pthread_t *thread, const _pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{
    static bool first_run = true;
    static int index = 1;
    if (first_run){
        first_run = false;
        thread_initilize();
    }
    
    struct task *t = malloc(sizeof(struct task));
    long int *stack_address = (long int *)malloc(STACK_SIZE);
    stack_address += STACK_SIZE>>2 ;
    
    (t->env)[0].__jmpbuf[6] = i64_ptr_mangle((long int)stack_address);
    (t->env)[0].__jmpbuf[7]=i64_ptr_mangle((long int)start_routine);

    t->running = false;
    t->block = false;
    t->initialized = false;
    t->terminated = false;
    t->task_index = index;
    t->thread_waited = 0;
    t->arg = arg;
    t->stack_address = stack_address;
    *thread = index;
    index++;
    INIT_LIST_HEAD(&t->list);
    list_add_tail(&t->list, &tasklist);
}
_pthread_t pthread_self(void)
{
    struct task *cur;
    list_for_each_entry(cur, &tasklist, list){
        if (cur->running == true){
            return (_pthread_t) cur->task_index;
        }
    }

}
void pthread_exit(void *value_ptr)
{
    struct task *cur;
    
    list_for_each_entry(cur, &tasklist, list){
        if (cur->running == true){
            struct task *t_next = list_entry(cur->list.next, struct task, list);
            list_del(&(cur->list));
            long int *_stack_address = (cur -> stack_address )-= STACK_SIZE>>2;
            free(_stack_address);
            free(cur);
            
             if (&t_next->list == &tasklist){
                t_next = list_first_entry(&tasklist, struct task, list);
            }    
           
            t_next->running = true;
            if (!t_next->initialized){
                t_next ->initialized = true;
                pthread_wrapper_init(t_next);
            }
            longjmp(t_next->env, 1);           
        }
    }
    
}

int pthread_join(_pthread_t thread, void **value_ptr)
{
    struct task *cur;
    struct task *caller;

    list_for_each_entry(cur, &tasklist, list){
        if (cur->task_index == (int)thread){
            if (cur->terminated){
                return 0;
            }else{
                    list_for_each_entry(caller, &tasklist, list){
                    if (caller->running){
                        caller->running = false;
                        caller->block = true;
                        cur->thread_waited = caller->task_index;
                     }
                    }
            }
        }
    }
}


void thread_initilize()
{
    struct sigaction *act = malloc(sizeof(struct sigaction));
    // struct sigaction act;
    // act.sa_handler = scheduler;
    // act.sa_flags = SA_NODEFER;
    // sigaction(SIGALRM, &act, NULL);
    act->sa_handler = scheduler;
    act->sa_flags = SA_NODEFER;
    sigaction(SIGALRM, act, NULL);
    ualarm(500,500);
}

void yield()
{
    jmp_buf t_current;
    setjmp(t_current);
    struct task *cur;
    list_for_each_entry(cur, &tasklist, list){
        if (cur->running == true){
            cur->running = false;

            long int rbp_encrypted = t_current[0].__jmpbuf[1];
            long int rbp = i64_ptr_mangle_re(rbp_encrypted);
            long int rbp_former = *((long int*)(rbp));
            long int rip_former = *(long int*)(rbp +8);
            long int rsp_former = rbp + 16;
            t_current[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
            t_current[0].__jmpbuf[7]=i64_ptr_mangle(rip_former); 
            t_current[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);
            memcpy(cur->env, t_current, sizeof(jmp_buf));

            
            struct task *t_next = list_entry(cur->list.next, struct task, list);

            t_next->running = true;
            if (!t_next->initialized){
            t_next ->initialized = true;
            pthread_wrapper_init(t_next);
            }
            longjmp(t_next->env, 1);
        }
    }
}

static void pthread_wrapper_init(struct task *task){

    long int rsp_encrypted = (task->env)[0].__jmpbuf[6];
    long int rsp = i64_ptr_mangle_re(rsp_encrypted);
    long int pc_encrypted = (task->env)[0].__jmpbuf[7];
    long int pc = i64_ptr_mangle_re(pc_encrypted);
    void *value_ptr;
    // asm("mov %0, %%rdi;"::"r"(task->arg));
    asm("mov %0, %%rsp;" : :"r"(rsp)); 
    // asm("mov %0, %%rax;"::"r"(pc+1));
    // asm("call *%rax;");
    ((void(*)(void *)) (pc)) (task->arg);
    asm("mov %%rax, %0;":"=r"(value_ptr)::);
    pthread_exit(value_ptr);
    // ((void(*)(void *)) (pc)) (task->arg);
}

/*a simple Round-Robin scheduler*/
void scheduler(int sig)
{
    static bool first_run = true;
    if (list_empty(&tasklist)){
        return;
    }
    if (first_run){
        first_run = false;
        struct task *t_first = list_first_entry(&tasklist, struct task, list);
        t_first->running = true;
        t_first->initialized = true;
        pthread_wrapper_init(t_first);
        
    }
    jmp_buf t_current;
    setjmp(t_current);
    struct task *cur;
    list_for_each_entry(cur, &tasklist, list){
        if (cur->running == true){
            cur->running = false;

            long int rbp_encrypted = t_current[0].__jmpbuf[1];
            long int rbp = i64_ptr_mangle_re(rbp_encrypted);
            long int rbp_former = *((long int*)(rbp));
            long int rip_former = *(long int*)(rbp +8);
            long int rsp_former = rbp + 16;
            t_current[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
            t_current[0].__jmpbuf[7]=i64_ptr_mangle(rip_former); 
            t_current[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);
            memcpy(cur->env, t_current, sizeof(jmp_buf));

            struct task *t_next = list_entry(cur->list.next, struct task, list);
            if (&t_next->list == &tasklist){
                t_next = list_first_entry(&tasklist, struct task, list);
            }
           
           
            t_next->running = true;
            if (!t_next->initialized){
            t_next ->initialized = true;
            pthread_wrapper_init(t_next);
            }
            longjmp(t_next->env, 1);
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