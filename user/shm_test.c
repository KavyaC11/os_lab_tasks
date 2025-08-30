//---------- TASK 3.1----------
#include "kernel/types.h"
#include "user/user.h"

struct Shared {
  volatile int flag;
  char buf[64];
};

int
main(void)
{
  int key = 1;

  if (shm_create(key) < 0) {
    printf("shm_create failed\n");
    exit(1);
  }

  // Fork *before* mapping, and let each process attach explicitly.
  int pid = fork();
  if (pid < 0) { printf("fork failed\n"); exit(1); }

  if (pid == 0) {
    // CHILD attaches
    struct Shared *S = (struct Shared*) shm_get(key);
    if (!S) { printf("child shm_get failed\n"); exit(1); }

    // wait for parent
    while (S->flag < 1) ;                // simple demo spin
    printf("child: got \"%s\"\n", S->buf);
    strcpy(S->buf, "hi from child");
    __sync_synchronize();
    S->flag = 2;

    shm_close(key);
    exit(0);
  }

  // PARENT attaches
  struct Shared *S = (struct Shared*) shm_get(key);
  if (!S) { printf("parent shm_get failed\n"); exit(1); }

  strcpy(S->buf, "hello from parent");
  __sync_synchronize();
  S->flag = 1;

  while (S->flag < 2) ;
  printf("parent: got \"%s\"\n", S->buf);

  wait(0);
  shm_close(key);
  exit(0);
}
