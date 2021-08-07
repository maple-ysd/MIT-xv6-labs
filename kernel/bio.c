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
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf hashbucket[NBUCKET];
  struct spinlock hblock[NBUCKET];
} bcache;



void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; ++i)
    initlock(&bcache.hblock[i], "bcache");

  // Create linked list of buffers
  for (int i = 0; i < NBUCKET; ++i) {
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  int i, pos;
  for(b = bcache.buf, i = 0; b < bcache.buf+NBUF; b++, i++){
    pos = i % NBUCKET;
    b->next = bcache.hashbucket[pos].next;
    b->prev = &bcache.hashbucket[pos];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[pos].next->prev = b;
    bcache.hashbucket[pos].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  
  int buck = blockno % NBUCKET;
  acquire(&bcache.hblock[buck]);

  // Is the block already cached?
  for(b = bcache.hashbucket[buck].next; b != &bcache.hashbucket[buck]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->timestamp = ticks;
      release(&bcache.hblock[buck]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.hashbucket[buck].prev; b != &bcache.hashbucket[buck]; b = b->prev){
    if(b->refcnt == 0) {
      b->timestamp = ticks;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.hblock[buck]);
      // printf("release lock:	%d\n", buck);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  release(&bcache.hblock[buck]);
  acquire(&bcache.lock);
  acquire(&bcache.hblock[buck]);
  
  // once again to avoid two copy of one blcok when now interrupt happens and switch to another process and that process search this block too and reach here 
  // ---------------------------------------------------------------------------------------------------------------------------------------------------------
  // Is the block already cached?
  for(b = bcache.hashbucket[buck].next; b != &bcache.hashbucket[buck]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->timestamp = ticks;
      release(&bcache.hblock[buck]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.hashbucket[buck].prev; b != &bcache.hashbucket[buck]; b = b->prev){
    if(b->refcnt == 0) {
      b->timestamp = ticks;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.hblock[buck]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // ---------------------------------------------------------------------------------------------------------------------------------------------------------  
  for (int i = (buck + 1) % NBUCKET; i != buck; i = (i + 1) % NBUCKET) {
    // printf("-");
    acquire(&bcache.hblock[i]);
    for (b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev){
      if(b->refcnt == 0) {
        b->timestamp = ticks;
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
      
        b->prev->next = b->next;
        b->next->prev = b->prev;
      
        b->next = bcache.hashbucket[buck].next;
        b->prev = &bcache.hashbucket[buck];
      
        bcache.hashbucket[buck].next->prev = b;
        bcache.hashbucket[buck].next = b;
      
        release(&bcache.hblock[i]);
        release(&bcache.hblock[buck]);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.hblock[i]);
  }

  // find LRU if no free block
  int min_timestamp = bcache.hashbucket[buck].timestamp;
  b = bcache.hashbucket[buck].prev;
  for (int i = (buck + 1) % NBUCKET; i != buck; i = (i + 1) % NBUCKET) {
    acquire(&bcache.hblock[i]);
    if (min_timestamp > bcache.hashbucket[i].timestamp){
      min_timestamp = bcache.hashbucket[i].timestamp;
      b = bcache.hashbucket[i].prev;
    }
  }
  b->timestamp = ticks;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
      
  b->prev->next = b->next;
  b->next->prev = b->prev;
      
  b->next = bcache.hashbucket[buck].next;
  b->prev = &bcache.hashbucket[buck];
      
  bcache.hashbucket[buck].next->prev = b;
  bcache.hashbucket[buck].next = b;
  
  for (int i = 0; i < NBUCKET; ++i)
    release(&bcache.hblock[i]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
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

  int buck = b->blockno % NBUCKET;
  acquire(&bcache.hblock[buck]);
  b->refcnt--;
  b->timestamp = ticks;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[buck].next;
    b->prev = &bcache.hashbucket[buck];
    bcache.hashbucket[buck].next->prev = b;
    bcache.hashbucket[buck].next = b;
  }
  
   release(&bcache.hblock[buck]);
}

void
bpin(struct buf *b) {
  int buck = b->blockno % NBUCKET;
  acquire(&bcache.hblock[buck]);
  b->refcnt++;
  b->timestamp = ticks;
  release(&bcache.hblock[buck]);
}

void
bunpin(struct buf *b) {
  int buck = b->blockno % NBUCKET;
  acquire(&bcache.hblock[buck]);
  b->refcnt--;
  b->timestamp = ticks;
  release(&bcache.hblock[buck]);
}


