//------------TASK 3.2---------------
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE     4096
#define EDGE_CAP   ((PGSIZE - 8) / 2)

typedef unsigned short ushort;
#define END_MARK ((ushort)0xFFFF)

#define SHM_KEY     1
#define MREQ_KEY    100   // A -> B queue key
#define MRESP_KEY   101   // B -> A queue key
#define PRINT_KEY   200   // print baton

struct MazePage {
  ushort a_start;
  ushort b_start;
  ushort end_marker; // must be 0xFFFF
  ushort pad;        // keeps header 8 bytes so EDGE_CAP math stays exact
  ushort edge[EDGE_CAP]; // intertwined edges
};

// --- simple, xv6-safe itoa
static void itoa(int val, char *buf) {
  char tmp[16];
  int i = 0, j = 0;
  if (val == 0) { buf[0]='0'; buf[1]='\0'; return; }
  while (val > 0 && i < (int)sizeof(tmp)-1) { tmp[i++] = '0' + (val % 10); val /= 10; }
  while (i > 0) buf[j++] = tmp[--i];
  buf[j] = '\0';
}

static void init_maze(struct MazePage *M) {
  for (int i = 0; i < EDGE_CAP; i++) M->edge[i] = END_MARK;

  // demo intertwined paths: A[i] points to B[i+1], B[i] points to A[i+1]
  int A[] = {5, 20, 40, 77};
  int B[] = {10, 25, 41, 90};
  int na = sizeof(A)/sizeof(A[0]);
  int nb = sizeof(B)/sizeof(B[0]);

  M->a_start    = (ushort)A[0];
  M->b_start    = (ushort)B[0];
  M->end_marker = END_MARK;

  for (int i = 0; i + 1 < na; i++)
    if (A[i] >= 0 && A[i] < EDGE_CAP) M->edge[A[i]] = (ushort)B[i+1];
  for (int i = 0; i + 1 < nb; i++)
    if (B[i] >= 0 && B[i] < EDGE_CAP) M->edge[B[i]] = (ushort)A[i+1];
}

// tiny helper to avoid duplicating argv setup + fork/exec
static void spawn(char role, int shmkey, int a2b, int b2a, int prn) {
  char r[2]={role,0}, s[16], x[16], y[16], z[16];
  itoa(shmkey, s); itoa(a2b, x); itoa(b2a, y); itoa(prn, z);
  char *argv[] = {"process", r, s, x, y, z, 0};
  int pid = fork();
  if (pid == 0) {
    exec("process", argv);
    printf("master: exec %c failed\n", role);
    exit(1);
  } else if (pid < 0) {
    printf("master: fork %c failed\n", role);
    exit(1);
  }
}

int main(void) {
  // create + attach shared page
  if (shm_create(SHM_KEY) < 0) {
    // ok if already exists
  }
  struct MazePage *M = (struct MazePage*) shm_get(SHM_KEY);
  if (!M) { printf("master: shm_get failed\n"); exit(1); }

  init_maze(M);

  // create mailboxes (IDs returned)
  int a2b = mbox_create(MREQ_KEY);
  int b2a = mbox_create(MRESP_KEY);
  int prn = mbox_create(PRINT_KEY);
  if (a2b < 0 || b2a < 0 || prn < 0) {
    printf("master: mbox_create failed\n");
    exit(1);
  }

  // seed print baton so only one line prints at a time
  mbox_send(prn, 1);

  // spawn workers A (send-then-recv) and B (recv-then-send)
  spawn('A', SHM_KEY, a2b, b2a, prn);
  spawn('B', SHM_KEY, a2b, b2a, prn);

  // wait + cleanup
  wait(0);
  wait(0);
  shm_close(SHM_KEY);
  exit(0);
}
