#include <stddef.h>
#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"
#include "mmu.h" // P3B: import to get PGSIZE

#define DATASIZE 128 // P3B: define size of thread_info array

// P3B: struct containing info on the original ptr malloc returned and the page aligned ptr
struct stack_info {
    int flag;
    void* original;
    void* aligned;
};

// P3B: array containing stack_info for each thread
struct stack_info data[DATASIZE];

// P3B: fetch and add primitive
static inline int fetch_and_add(int *var, int val) {   
     __asm__ volatile
     ("lock; xaddl %0, %1"
 	: "+r" (val),  "+m" (*var) // input + output
 	: // No input
 	: "memory"
     );
     return val;
}

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}

// P3B: thread_create
int thread_create(void (*start_routine)(void *, void *), void *arg1, void *arg2) {
   // malloc space for the stack and store the address of the stack
   void* origin = malloc(PGSIZE * 2);
   void* stack;

   // find the offset and page align the stack
   uint offset = PGSIZE - ((uint)origin % PGSIZE);
   stack = origin + offset;

   for (int i = 0; i < DATASIZE; i++) {
     if (data[i].flag == 0) {
        data[i].original = origin;
        data[i].aligned = stack;
        data[i].flag = 1;
        break;
     }
   }

   int pid = clone(start_routine, arg1, arg2, stack);
   
   return pid;
}
// P3B: thread_join
int thread_join() {
    void* stack;
    int pid = join(&stack);
    for (int i = 0; i < DATASIZE; i++) {
      if (data[i].flag == 1 && data[i].aligned == stack) {
        free(data[i].original);
        data[i].flag = 0;
        data[i].original = NULL;
        data[i].aligned = NULL;
        break;
      }
    }

    return pid;
}

// P3B: lock_init 
void lock_init(lock_t* lock) {
    lock->ticket = 0;
    lock->turn = 0;
}

// P3B: lock_acquire 
void lock_acquire(lock_t* lock) {
    int myturn = fetch_and_add(&lock->ticket, 1);
    while (fetch_and_add(&lock->turn, 0) != myturn);
}

// P3B: lock_release
void lock_release(lock_t* lock) {
    fetch_and_add(&lock->turn, 1);
}

