#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include "list.h"

#define STACK_SIZE 32767

struct task{
    jmp_buf env;
    void *arg;
    int task_index;
    long int *stack_address;
    bool running;
    bool initialized;
    struct list_head list;
};

typedef int _pthread_t;
typedef struct {
    struct task *task;
}_pthread_attr_t;

static LIST_HEAD(tasklist);

void foo();
void bar();
void baz();

static long int i64_ptr_mangle(long int p);
static long int i64_ptr_mangle_re(long int p);

int pthread_create(_pthread_t *thread, const _pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
void thread_create(void*(*start_routine)(void*));
void pthread_exit(void *value_ptr);
void pthread_wrapper_init(struct task *task);
_pthread_t pthread_self(void);

void thread_initilize();
void scheduler(int sig);
void yield();

int main()
{
    int test = 199;
    int test1 = 50;
    int test2 = 10;

    _pthread_t a;
    _pthread_t b;
    _pthread_t c;
    pthread_create(&a, NULL, (void *)foo, (void *) &test);
    pthread_create(&b, NULL, (void *)bar, (void *) &test1);
    pthread_create(&c, NULL, (void *)baz, (void *) &test2);
    for(;;){
        ;
    }

}

void foo(void *arg)
{

    int counter_foo = *(int *)arg;
    int i  = 0;
    while(1){
        printf("counter_foo = %d\n", counter_foo);
        counter_foo +=1;
        // printf("index = %d\n",(int) pthread_self());
        // i++;
        // yield();
    }
    // pthread_exit(NULL);
}

void bar(void *arg)
{

    int counter_bar = *(int *)arg;
    int j = 0;
    while(1){
        printf("counter_bar= %d\n", counter_bar);
        counter_bar +=10;
        // printf("j = %d\n", j);
        // j++;
        // yield();
    }
    // pthread_exit(NULL);
}

void baz(void *arg)
{

    int counter_baz = *(int *)arg;
    int i = 0;
    while(i < 100){
        printf("counter_baz= %d\n", counter_baz);
        counter_baz +=100;
        i++;
        // yield();
    }
    pthread_exit(NULL);
}

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
    
    // (t->env)[0].__jmpbuf[1] = i64_ptr_mangle((long int)stack_address);
    (t->env)[0].__jmpbuf[6] = i64_ptr_mangle((long int)stack_address);
    (t->env)[0].__jmpbuf[7]=i64_ptr_mangle((long int)start_routine);

    t->running = false;
    t->initialized = false;
    t->task_index = index;
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

// void thread_create(void*(*start_routine)(void*))
// {
//     static bool first_run = true;
//     static int index = 1;
//     if (first_run){
//         first_run = false;
//         thread_initilize();
//     }
//     struct task *t = malloc(sizeof(struct task));
//     long int *stack_address = (long int *)malloc(STACK_SIZE);
//     stack_address += STACK_SIZE>>2 ;
    
//     (t->env)[0].__jmpbuf[1] = i64_ptr_mangle((long int)stack_address);
//     (t->env)[0].__jmpbuf[6] = i64_ptr_mangle((long int)stack_address);
//     (t->env)[0].__jmpbuf[7]=i64_ptr_mangle((long int)start_routine);

//     t->running = false;
//     t->task_index = index;
//     index++;
//     INIT_LIST_HEAD(&t->list);
//     list_add_tail(&t->list, &tasklist);

// }

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
    ualarm(5000,5000);
}

// void yield()
// {
//     jmp_buf t_current;
//     setjmp(t_current);
//     struct task *cur;
//     list_for_each_entry(cur, &tasklist, list){
//         printf("id = %d\n", cur->task_index);
//         if (cur->running == true){
//             printf("task id: %d\n", cur->task_index);
//             cur->running = false;

//             long int rbp_encrypted = t_current[0].__jmpbuf[1];
//             long int rbp = i64_ptr_mangle_re(rbp_encrypted);
//             long int rbp_former = *((long int*)(rbp));
//             long int rip_former = *(long int*)(rbp +8);
//             long int rsp_former = rbp + 16;
//             t_current[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
//             t_current[0].__jmpbuf[7]=i64_ptr_mangle(rip_former); 
//             t_current[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);
//             memcpy(cur->env, t_current, sizeof(jmp_buf));

            
//             struct task *t_next = list_entry(cur->list.next, struct task, list);

//             printf("next task id: %d\n", t_next->task_index);
//             if (t_next->task_index == 0){
//                 t_next = list_entry(cur->list.prev, struct task, list);
//             }
//             t_next->running = true;
//             // alarm(1);
//             longjmp(t_next->env, 1);
//         }
//     }
// }

void pthread_wrapper_init(struct task *task){

    long int rsp_encrypted = (task->env)[0].__jmpbuf[6];
    long int rsp = i64_ptr_mangle_re(rsp_encrypted);
    long int pc_encrypted = (task->env)[0].__jmpbuf[7];
    long int pc = i64_ptr_mangle_re(pc_encrypted);
    asm("mov %0, %%rdi;"::"r"(task->arg));
    asm("mov %0, %%rsp;" : :"r"(rsp)); 
     asm("mov %0, %%rax;"::"r"(pc+1));
    asm("jmp *%rax;");
    // ((void(*)(void *)) (pc)) (task->arg);
}

/*a simple Round-Robin scheduler*/
void scheduler(int sig)
{
    char *a = "2\n";
    write(STDOUT_FILENO, a, 2);
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
        // longjmp(t_first->env, 1);
        
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