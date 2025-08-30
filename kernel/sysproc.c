#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//---------- BEGIN TASK 3.1----------
uint64 sys_shm_create(void){ 
  int key; 
  argint(0,&key); 
  return shm_create(key);
 }
uint64 sys_shm_get(void)   {
   int key;
    argint(0,&key);
     return (uint64)shm_get(key); 
    }
uint64 sys_shm_close(void) {
   int key; 
   argint(0,&key);
    return shm_close(key);
   }

uint64 sys_mbox_create(void){ 
  int key; 
  argint(0,&key);
   return kmbox_create(key); 
  }
uint64 sys_mbox_send(void)  { 
  int id,msg;
   argint(0,&id);
    argint(1,&msg);
     return kmbox_send(id,msg);
     }
uint64 sys_mbox_recv(void)  { 
  int id; 
  uint64 u; 
  argint(0,&id);
   argaddr(1,&u); 
   return kmbox_recv(id,u); 
  }

  //---------- END TASK 3.1----------