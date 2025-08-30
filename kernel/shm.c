//---------- BEGIN TASK 3.1----------
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "memlayout.h"
#include "proc.h"
#include "defs.h"

#define SHMBASE 0x40000000ULL

struct {
  struct spinlock lock;
  void  *frames[NSHM];
  int    refcnt[NSHM];
} shm;

void
shminit(void)
{
  initlock(&shm.lock, "shm");
  for (int i = 0; i < NSHM; i++) {
    shm.frames[i] = 0;
    shm.refcnt[i] = 0;
  }
}

static inline uint64
key2va(int key) { return SHMBASE + ((uint64)key)*PGSIZE; }

int
shm_create(int key)
{
  if (key < 0 || key >= NSHM) return -1;
  acquire(&shm.lock);
  if (shm.frames[key] == 0) {
    void *pa = kalloc();
    if (!pa) { release(&shm.lock); return -1; }
    memset(pa, 0, PGSIZE);
    shm.frames[key] = pa;
    shm.refcnt[key] = 0;
  }
  release(&shm.lock);
  return key;
}

void*
shm_get(int key)
{
  if (key < 0 || key >= NSHM) return 0;
  struct proc *p = myproc();
  uint64 va = key2va(key);

  acquire(&shm.lock);
  void *pa = shm.frames[key];
  if (!pa) { release(&shm.lock); return 0; } // must call shm_create first
  int first = (p->shm_used[key] == 0);
  if (first) { shm.refcnt[key]++; p->shm_used[key] = 1; }
  release(&shm.lock);

  pte_t *pte = walk(p->pagetable, va, 0);
  if (!(pte && (*pte & PTE_V))) {
    if (mappages(p->pagetable, va, PGSIZE, (uint64)pa, PTE_R|PTE_W|PTE_U) < 0) {
      acquire(&shm.lock);
      if (first) { shm.refcnt[key]--; p->shm_used[key] = 0; }
      release(&shm.lock);
      return 0;
    }
  }
  return (void*)va;
}

int
shm_close(int key)
{
  if (key < 0 || key >= NSHM) return -1;
  struct proc *p = myproc();
  if (!p->shm_used[key]) return -1;

  uint64 va = key2va(key);
  uvmunmap(p->pagetable, va, 1, 0);

  acquire(&shm.lock);
  p->shm_used[key] = 0;
  if (shm.refcnt[key] > 0) {
    shm.refcnt[key]--;
    if (shm.refcnt[key] == 0 && shm.frames[key]) {
      kfree(shm.frames[key]);
      shm.frames[key] = 0;
    }
  }
  release(&shm.lock);
  return 0;
}

void
shm_detach_all(struct proc *p)
{
  acquire(&shm.lock);
  for (int key = 0; key < NSHM; key++) {
    if (p->shm_used[key]) {
      uint64 va = key2va(key);
      uvmunmap(p->pagetable, va, 1, 0);
      p->shm_used[key] = 0;
      if (shm.refcnt[key] > 0) {
        shm.refcnt[key]--;
        if (shm.refcnt[key] == 0 && shm.frames[key]) {
          kfree(shm.frames[key]);
          shm.frames[key] = 0;
        }
      }
    }
  }
  release(&shm.lock);
}
//---------- END TASK 3.1----------