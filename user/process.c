//------------TASK 3.2---------------
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE     4096
#define EDGE_CAP   ((PGSIZE - 8) / 2)

typedef unsigned short ushort;
#define END_MARK ((ushort)0xFFFF)

struct MazePage {
  ushort a_start;
  ushort b_start;
  ushort end_marker;
  ushort pad;
  ushort edge[EDGE_CAP];
};

int main(int argc, char *argv[]) {
  // invoked only by master: process A|B shmkey a2b b2a printkey
  if (argc < 6) {
    printf("process: worker program, not standalone\n");
    printf("usage: process A|B shmkey a2b b2a printkey\n");
    exit(1);
  }

  char role = argv[1][0];
  int shmkey = atoi(argv[2]);
  int a2b    = atoi(argv[3]);
  int b2a    = atoi(argv[4]);
  int prn    = atoi(argv[5]);

  struct MazePage *M = (struct MazePage*) shm_get(shmkey);
  if (!M) { printf("%c: shm_get failed\n", role); exit(1); }

  ushort my_cur = (role == 'A') ? M->a_start : M->b_start;

  // strict protocol to avoid deadlock:
  // A: send -> recv ; B: recv -> send
  int steps = 0;
  while (1) {
    int other_int;
    ushort other;

    if (role == 'A') {
      if (mbox_send(a2b, (int)my_cur) < 0) { printf("A: send failed\n"); exit(1); }
      if (mbox_recv(b2a, &other_int) < 0)  { printf("A: recv failed\n"); exit(1); }
      other = (ushort)other_int;
    } else { // role == 'B'
      if (mbox_recv(a2b, &other_int) < 0)  { printf("B: recv failed\n"); exit(1); }
      other = (ushort)other_int;
      if (mbox_send(b2a, (int)my_cur) < 0) { printf("B: send failed\n"); exit(1); }
    }

    // compute next hops with bounds checks
    ushort my_next = END_MARK;
    ushort partner_next = END_MARK;
    if ((int)other  >= 0 && (int)other  < EDGE_CAP) my_next      = M->edge[other];
    if ((int)my_cur >= 0 && (int)my_cur < EDGE_CAP) partner_next = M->edge[my_cur];

    // synchronized logging via print baton
    int token;
    if (mbox_recv(prn, &token) < 0) token = 1;
    int myn_print   = (my_next      == END_MARK) ? -1 : (int)my_next;
    int partn_print = (partner_next == END_MARK) ? -1 : (int)partner_next;
    printf("%c step=%d cur=%d other=%d -> my_next=%d partner_next=%d\n",
           role, steps, (int)my_cur, (int)other, myn_print, partn_print);
    mbox_send(prn, token);

    // advance & stop condition: both see ENDs → loop ends next iteration naturally
    if (my_next == END_MARK && partner_next == END_MARK) break;
    if (my_next != END_MARK) my_cur = my_next;

    steps++;
    if (steps > 2000) {  // safety guard
      if (mbox_recv(prn, &token) == 0) {
        printf("%c: safety break\n", role);
        mbox_send(prn, token);
      }
      break;
    }
  }

  shm_close(shmkey);
  exit(0);
}
