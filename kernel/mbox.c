//---------- BEGIN TASK 3.1----------
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

struct mailbox {
  int in_use;
  int key;
  int buf[MBOX_CAP];
  int head, tail, count;
  struct spinlock lock;
};

struct {
  struct spinlock lock;
  struct mailbox box[MBOX_MAX];
} mbs;

void
mboxinit(void)
{
  initlock(&mbs.lock, "mboxes");
  for (int i = 0; i < MBOX_MAX; i++) {
    mbs.box[i].in_use = 0;
    mbs.box[i].key = -1;
    mbs.box[i].head = mbs.box[i].tail = mbs.box[i].count = 0;
    initlock(&mbs.box[i].lock, "mbox");
  }
}

int kmbox_create(int key) {
  if (key < 0) return -1;
  acquire(&mbs.lock);

  // If a mailbox with this key already exists, RESET its queue for a fresh run.
  for (int i = 0; i < MBOX_MAX; i++) {
    if (mbs.box[i].in_use && mbs.box[i].key == key) {
      // Clear queue safely while holding the per-box lock
      acquire(&mbs.box[i].lock);
      mbs.box[i].head = 0;
      mbs.box[i].tail = 0;
      mbs.box[i].count = 0;

      //  wake up any senders/receivers that might be blocked
      wakeup(&mbs.box[i]);

      release(&mbs.box[i].lock);
      release(&mbs.lock);
      return i;
    }
  }

  // Otherwise allocate a new mailbox
  for (int i = 0; i < MBOX_MAX; i++) {
    if (!mbs.box[i].in_use) {
      mbs.box[i].in_use = 1;
      mbs.box[i].key = key;
      mbs.box[i].head = mbs.box[i].tail = mbs.box[i].count = 0;
      release(&mbs.lock);
      return i;
    }
  }

  release(&mbs.lock);
  return -1;
}


static inline struct mailbox*
get_box(int id)
{
  if (id < 0 || id >= MBOX_MAX) return 0;
  if (!mbs.box[id].in_use) return 0;
  return &mbs.box[id];
}

int
kmbox_send(int id, int msg)
{
  struct mailbox *b = get_box(id);
  if (!b) return -1;

  acquire(&b->lock);
  while (b->count == MBOX_CAP) {
    sleep(b, &b->lock);
  }
  b->buf[b->tail] = msg;
  b->tail = (b->tail + 1) % MBOX_CAP;
  b->count++;
  wakeup(b); // wake receivers
  release(&b->lock);
  return 0;
}

int
kmbox_recv(int id, uint64 uaddr)
{
  struct mailbox *b = get_box(id);
  if (!b) return -1;

  acquire(&b->lock);
  while (b->count == 0) {
    sleep(b, &b->lock);
  }
  int msg = b->buf[b->head];
  b->head = (b->head + 1) % MBOX_CAP;
  b->count--;
  wakeup(b); // wake senders
  release(&b->lock);

  struct proc *p = myproc();
  if (copyout(p->pagetable, uaddr, (char*)&msg, sizeof(msg)) < 0)
    return -1;
  return 0;
}
//---------- END TASK 3.1----------