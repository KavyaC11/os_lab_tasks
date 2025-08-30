//------------TASK 3.1---------------
#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int mreq = mbox_create(42);  // parent -> child
  int mresp = mbox_create(43); // child  -> parent
  if (mreq < 0 || mresp < 0) { printf("mbox_create failed\n"); exit(1); }

  int pid = fork();
  if (pid < 0) { printf("fork failed\n"); exit(1); }

  if (pid == 0) {
    // CHILD: recv request, then send response
    int v;
    if (mbox_recv(mreq, &v) < 0) { printf("child recv failed\n"); exit(1); }
    printf("child: received %d\n", v);
    if (mbox_send(mresp, v + 1) < 0) { printf("child send failed\n"); exit(1); }
    exit(0);
  }

  // PARENT: send request, then recv response
  if (mbox_send(mreq, 99) < 0) { printf("parent send failed\n"); exit(1); }
  int w;
  if (mbox_recv(mresp, &w) < 0) { printf("parent recv failed\n"); exit(1); }
  printf("parent: got %d\n", w);

  wait(0);
  exit(0);
}
