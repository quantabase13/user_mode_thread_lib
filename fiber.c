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
    int task_index;
    bool running;
    struct list_head list;
};

static LIST_HEAD(tasklist);

void foo();
void bar();
void baz();

static long int i64_ptr_mangle(long int p);
static long int i64_ptr_mangle_re(long int p);


void thread_create(void*(*start_routine)(void*));
void thread_initilize();
void scheduler(int sig);
void yield();

int main()
{

    thread_create((void *)foo);
    thread_create((void* )bar);
    thread_create((void*)baz);
    for(;;){
        ;
    }

}

void foo()
{

    int counter_foo = 0;
    while(1){
        printf("counter_foo = %d\n", counter_foo);
        counter_foo +=1;
        // yield();
    }
}

void bar()
{

    int counter_bar = 0;
    while(1){
        printf("counter_bar= %d\n", counter_bar);
        counter_bar +=2;
        // yield();
    }
}

void baz()
{

    int counter_baz = 0;
    while(1){
        printf("counter_baz= %d\n", counter_baz);
        counter_baz +=5;
        // yield();
    }
}


void thread_create(void*(*start_routine)(void*))
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
    t->task_index = index;
    index++;
    INIT_LIST_HEAD(&t->list);
    list_add_tail(&t->list, &tasklist);

}

void thread_initilize()
{
    // jmp_buf env;
    // setjmp(env);
    // struct task *t_main = malloc(sizeof(struct task));

    // long int rbp_encrypted = env[0].__jmpbuf[1];
    // long int rbp = i64_ptr_mangle_re(rbp_encrypted);
    // long int rbp_former = *((long int*)(rbp));
    // long int rip_former = *(long int*)(rbp +8);
    // long int rsp_former = rbp + 16;
    // env[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
    // env[0].__jmpbuf[7]=i64_ptr_mangle(rip_former); 
    // env[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);


    // memcpy(t_main->env, env, sizeof(jmp_buf));
    // INIT_LIST_HEAD(&t_main->list);
    // t_main->task_index = 0;
    // t_main ->running = true;
    struct sigaction act;
    act.sa_handler = scheduler;
    act.sa_flags = SA_NODEFER;
    // list_add_tail(&t_main->list, &tasklist);
    signal(SIGALRM, scheduler);
    sigaction(SIGALRM, &act, NULL);
    ualarm(500000,50000);
    // alarm(1);

}

void yield()
{
    jmp_buf t_current;
    setjmp(t_current);
    struct task *cur;
    list_for_each_entry(cur, &tasklist, list){
        printf("id = %d\n", cur->task_index);
        if (cur->running == true){
            printf("task id: %d\n", cur->task_index);
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

            printf("next task id: %d\n", t_next->task_index);
            if (t_next->task_index == 0){
                t_next = list_entry(cur->list.prev, struct task, list);
            }
            t_next->running = true;
            // alarm(1);
            longjmp(t_next->env, 1);
        }
    }
}

void scheduler(int sig)
{
    static bool first_run = true;
    if (first_run){
        first_run = false;
        struct task *t_first = list_first_entry(&tasklist, struct task, list);
        t_first->running = true;
        longjmp(t_first->env, 1);
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