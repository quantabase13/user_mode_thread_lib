#include <unistd.h>

volatile unsigned int lock = 0;

void SpinLock(volatile unsigned int *lock);
void SpinUnlock(volatile unsigned int *lock);
static inline uint xchg(volatile unsigned int *addr, unsigned int newval);

void SpinLock(volatile unsigned int *lock){
    while(xchg(lock, 1)==1)
        ;
}
void SpinUnlock(volatile unsigned int *lock){
    *lock = 0;
}

static inline uint xchg(volatile unsigned int *addr, unsigned int newval) {
 uint result;
 asm volatile("lock; xchgl %0, %1" :
 "+m" (*addr), "=a" (result) :
 "1" (newval) : "cc");
 return result;
}