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
  struct spinlock lock;
  struct spinlock buflock;
  struct spinlock locks[NBUCKET];
  struct buf buf[NBUF];
  
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];
  uint size;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache_biglock");
  initlock(&bcache.buflock, "bcache_buf");

  for(int i = 0; i < NBUCKET; i++){
    initlock(&bcache.locks[i], "bcache");
  }

    for(int i = 0; i < NBUCKET; i++){
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
   }
  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    //b->next = bcache.head.next;
    //b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int idx = blockno % NBUCKET;

  acquire(&bcache.locks[idx]);

  // Is the block already cached?
  for(b = bcache.head[idx].next; b != &bcache.head[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  acquire(&bcache.buflock);
  if(bcache.size < NBUF){
    b = &bcache.buf[bcache.size++];
    release(&bcache.buflock);
    b->prev = &bcache.head[idx];
    b->next = bcache.head[idx].next;
    (bcache.head[idx].next)->prev = b;
    bcache.head[idx].next = b;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.locks[idx]);
    acquiresleep(&b->lock);
    return b;
  }
  
  // to avoid deadlock.
  release(&bcache.buflock);
  release(&bcache.locks[idx]);

  acquire(&bcache.lock);
  acquire(&bcache.locks[idx]);
  for(b = bcache.head[idx].next; b != &bcache.head[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[idx]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.locks[idx]);

  int min = ((unsigned)-1)>>1; // min timestamp;
  int ref = -1;     
  struct buf* t;
  for(int i = 0; i < NBUCKET; i++){
    acquire(&bcache.locks[i]);
    for(t = bcache.head[i].next; t != &bcache.head[i]; t = t->next){
      if(t->refcnt == 0 && t->timestamp < min){
        b = t;
        ref = i;
        min = t->timestamp;
      }
    }
    release(&bcache.locks[i]);
  }

  if(ref >= 0){
    acquire(&bcache.locks[ref]);
    (b->prev)->next = b->next;
    (b->next)->prev = b->prev;
    release(&bcache.locks[ref]);
    acquire(&bcache.locks[idx]);
    b->prev = &bcache.head[idx];
    b->next = bcache.head[idx].next;
    (bcache.head[idx].next)->prev = b;
    bcache.head[idx].next = b;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.locks[idx]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);




  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //  if(b->refcnt == 0) {
  //  b->dev = dev;
  //  b->blockno = blockno;
  //  b->valid = 0;
  //  b->refcnt = 1;
  //  release(&bcache.lock);
  //  acquiresleep(&b->lock);
  //  return b;
  //  }
  //  }

  
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

  releasesleep(&b->lock);

  int idx = (b->blockno) % NBUCKET;
  acquire(&bcache.locks[idx]);
  b->refcnt--;
  if(b->refcnt == 0)
    b->timestamp = ticks;
  release(&bcache.locks[idx]);
}

void
bpin(struct buf *b) {
  int idx = (b->blockno) % NBUCKET;
  acquire(&bcache.locks[idx]);
  b->refcnt++;
  release(&bcache.locks[idx]);
}

void
bunpin(struct buf *b) {
  int idx = (b->blockno) % NBUCKET;
  acquire(&bcache.locks[idx]);
  b->refcnt--;
  release(&bcache.locks[idx]);
}


