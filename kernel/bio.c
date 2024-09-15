// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

extern uint ticks;

struct {
  struct spinlock cachelock;
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.cachelock, "bcache");
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    for(b = bcache.buf + i; b < bcache.buf+NBUF; b+=NBUCKET){
      initsleeplock(&b->lock, "buffer");
      b->timestamp = 0;
      b->prev = &bcache.head[i];
      b->next = bcache.head[i].next;
      b->blockno = i;
      b->dev = 0x7fffffff;
      bcache.head[i].next->prev = b;
      bcache.head[i].next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bno = blockno % NBUCKET;
  acquire(&bcache.lock[bno]);

  // Is the block already cached?
  for(b = bcache.head[bno].next; b != &bcache.head[bno]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bno]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[bno]);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  uint mts = ticks + 1;  
  struct buf *mb = 0;
  for (int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.lock[i]);
    uint8 haschanged = 0;
    for(b = bcache.head[i].next; b != &bcache.head[i]; b = b->next){
      if(b->refcnt == 0 && b->timestamp <= mts){
        if(mb && mb->blockno % NBUCKET != i){
          release(&bcache.lock[mb->blockno % NBUCKET]);
        }
        mb = b;
        mts = b->timestamp;
        haschanged = 1;
      }
    }
    if (!haschanged)
      release(&bcache.lock[i]);
  }
  if(mb){
      struct spinlock *prelock = &bcache.lock[mb->blockno % NBUCKET];
      mb->dev = 0x7fffffff;
      mb->blockno = blockno;
      mb->valid = 0;
      mb->refcnt = 0;
      mb->next->prev = mb->prev;
      mb->prev->next = mb->next;
      release(prelock);
      struct spinlock *newlock = &bcache.lock[mb->blockno % NBUCKET];
      acquire(newlock);
      mb->next = bcache.head[mb->blockno % NBUCKET].next;
      mb->prev = &bcache.head[mb->blockno % NBUCKET];
      bcache.head[mb->blockno % NBUCKET].next->prev = mb;
      bcache.head[mb->blockno % NBUCKET].next = mb;
      for(b = bcache.head[bno].next; b != &bcache.head[bno]; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
          b->refcnt++;
          release(&bcache.lock[bno]);
          acquiresleep(&b->lock);
          return b;
        }
      }
      mb->dev = dev;
      mb->refcnt = 1;
      release(newlock);
      acquiresleep(&mb->lock);
      return mb;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int bno = b->blockno % NBUCKET;
  releasesleep(&b->lock);

  acquire(&bcache.lock[bno]);
  b->refcnt--;
  b->timestamp = ticks;
  
  release(&bcache.lock[bno]);
}

void
bpin(struct buf *b) {
  int bno = b->blockno % NBUCKET;
  acquire(&bcache.lock[bno]);
  b->refcnt++;
  release(&bcache.lock[bno]);
}

void
bunpin(struct buf *b) {
  int bno = b->blockno % NBUCKET;
  acquire(&bcache.lock[bno]);
  b->refcnt--;
  release(&bcache.lock[bno]);
}


