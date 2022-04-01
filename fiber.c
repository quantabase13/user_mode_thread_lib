#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define STACK_SIZE 32767

typedef  struct q_ele{
    jmp_buf buf;
    struct q_ele *next;
}q_ele;

typedef struct buf_q{
    q_ele *head;
    q_ele *tail;
    int size;
}buf_q;

void enq(buf_q *q, q_ele* q_ele);
q_ele *deq(buf_q *q);

void foo();
void bar();
int thread_create(void*(*start_routine)(void*));
static long int i64_ptr_mangle(long int p);
static long int i64_ptr_mangle_re(long int p);
void yield();

buf_q q;
int main()
{

    thread_create((void *)foo);
    thread_create((void* )bar);

}

void foo()
{

    int counter_foo = 0;
    while(1){
        printf("counter_foo = %d\n", counter_foo);
        counter_foo +=2;
        yield();
    }
}

void bar()
{

    int counter_bar = 1;
    while(1){
        printf("counter_bar= %d\n", counter_bar);
        counter_bar +=2;
        yield();
    }
}

void yield(){
    q_ele *q_element = (q_ele*)malloc(sizeof(q_ele));
    setjmp(q_element->buf);

    long int rbp_encypted = q_element->buf[0].__jmpbuf[1];
    long int rbp = i64_ptr_mangle_re(rbp_encypted);
    long int rbp_former = *((long int*)(rbp));
    long int rip_former = *(long int*)(rbp+8);
    long int rsp_former = rbp+16;

    q_element->buf[0].__jmpbuf[6] = i64_ptr_mangle(rsp_former);
    q_element->buf[0].__jmpbuf[7]=i64_ptr_mangle(rip_former); 
    q_element->buf[0].__jmpbuf[1] = i64_ptr_mangle(rbp_former);
    enq(&q, q_element);
    q_ele *next_to_run = deq(&q);
    jmp_buf buf;
    buf[0].__jmpbuf[0] =(next_to_run->buf)[0].__jmpbuf[0];
    buf[0].__jmpbuf[1]=(next_to_run->buf)[0].__jmpbuf[1];
    buf[0].__jmpbuf[2] =(next_to_run->buf)[0].__jmpbuf[2];
    buf[0].__jmpbuf[3]=(next_to_run->buf)[0].__jmpbuf[3];
    buf[0].__jmpbuf[4] =(next_to_run->buf)[0].__jmpbuf[4];
    buf[0].__jmpbuf[5]=(next_to_run->buf)[0].__jmpbuf[5];
    buf[0].__jmpbuf[6] =(next_to_run->buf)[0].__jmpbuf[6];
    buf[0].__jmpbuf[7]=(next_to_run->buf)[0].__jmpbuf[7];
    buf[0].__mask_was_saved = (next_to_run->buf)[0].__mask_was_saved;
    buf[0].__saved_mask = (next_to_run->buf)[0].__saved_mask; 
    free(next_to_run);   
    longjmp(buf, 1);

}

int thread_create(void*(*start_routine)(void*)){
    static bool first_call = true;
    long int *stack_address = (long int *)malloc(STACK_SIZE);
    stack_address += 0xfff;
    q_ele *q_element = (q_ele*)malloc(sizeof(q_ele));
    (q_element->buf)[0].__jmpbuf[6] = i64_ptr_mangle((long int)stack_address);
    (q_element->buf)[0].__jmpbuf[7]=i64_ptr_mangle((long int)start_routine);

    if (first_call){
        enq(&q, q_element);
        first_call = false;
        return 0;
    }else{
    enq(&q, q_element);
    q_ele *next_to_run = deq(&q);
    jmp_buf buf;
    buf[0].__jmpbuf[0] =(next_to_run->buf)[0].__jmpbuf[0];
    buf[0].__jmpbuf[1]=(next_to_run->buf)[0].__jmpbuf[1];
    buf[0].__jmpbuf[2] =(next_to_run->buf)[0].__jmpbuf[2];
    buf[0].__jmpbuf[3]=(next_to_run->buf)[0].__jmpbuf[3];
    buf[0].__jmpbuf[4] =(next_to_run->buf)[0].__jmpbuf[4];
    buf[0].__jmpbuf[5]=(next_to_run->buf)[0].__jmpbuf[5];
    buf[0].__jmpbuf[6] =(next_to_run->buf)[0].__jmpbuf[6];
    buf[0].__jmpbuf[7]=(next_to_run->buf)[0].__jmpbuf[7];
    buf[0].__mask_was_saved = (next_to_run->buf)[0].__mask_was_saved;
    buf[0].__saved_mask = (next_to_run->buf)[0].__saved_mask; 
    free(next_to_run);   
    longjmp(buf, 1);
    }
}


void enq(buf_q *q, q_ele *ele){
    if (q->size == 0){
        q->tail = ele;
        q->head = ele;
        q->size++;
        return;
    }
    q->tail->next = ele;
    q->tail = ele;
    q->size ++;
    if (q->size == 1){
        q->head = ele;
    }
    return;
}

q_ele *deq(buf_q *q){
    q_ele *ret_node = q->head;
    q->head = (q->head)->next;
    (q->size)--;
    if (q->size == 0){
        q->tail = NULL;
    }
    return ret_node;
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