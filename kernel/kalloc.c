// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end, int cpuid);


extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
cpukfree(void *pa, int cpuid)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpuid].lock);
  r->next = kmem[cpuid].freelist;
  kmem[cpuid].freelist = r;
  release(&kmem[cpuid].lock);
}

void
kinit()
{
  uint64 pa_start = PGROUNDUP((uint64)end);
  uint64 pa_end = PHYSTOP;
  uint64 persize = (pa_end - pa_start) / NCPU;
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
    freerange((void*)pa_start, (void*)(pa_start + persize), i);
    pa_start += persize;
  }
}

void
freerange(void *pa_start, void *pa_end, int cpuid)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    cpukfree(p, cpuid);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  cpukfree(pa, cpuid());
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cid = cpuid();

  acquire(&kmem[cid].lock);
  r = kmem[cid].freelist;
  if(r) {
    kmem[cid].freelist = r->next;
    release(&kmem[cid].lock);
  }
  else {
    release(&kmem[cid].lock);
    for (int i = 1; i < NCPU; i++) {
      int bcid = (cid + i) % NCPU;
      acquire(&kmem[bcid].lock);
      r = kmem[bcid].freelist;
      if(r) {
        kmem[bcid].freelist = r->next;
        release(&kmem[bcid].lock);
        break;
      }
      release(&kmem[bcid].lock);
    }
  }


  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
