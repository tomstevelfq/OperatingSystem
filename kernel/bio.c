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

#define NUM_BUCKET 13
#define BUFMAP_HASH(dev, blockno) ((((dev) << 27) | (blockno)) % NUM_BUCKET)


struct {
  struct buf buf[NBUF];
  struct spinlock eviction_lock;//驱逐锁
  //多个hash锁和多个head节点
  //使用了多个缓存链表
  struct buf bufmap[NUM_BUCKET];
  struct spinlock bufmap_locks[NUM_BUCKET];
} bcache;

void
binit(void)
{
  for(int i=0;i<NUM_BUCKET;i++){
    initlock(&bcache.bufmap_locks[i],"bcache_bufmap");
    bcache.bufmap[i].next=0;
  }

  for(int i=0;i<NBUF;i++){
    struct buf* b=&bcache.buf[i];
    initsleeplock(&b->lock,"buffer");
    b->lastuse=0;
    b->refcnt=0;

    //所有缓存添加到bufmap[0]
    b->next=bcache.bufmap[0].next;
    bcache.bufmap[0].next=b;
  }

  initlock(&bcache.eviction_lock,"bcache_eviction");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key=BUFMAP_HASH(dev,blockno);
  acquire(&bcache.bufmap_locks[key]);

  for(b=bcache.bufmap[key].next;b;b=b->next){
    if(b->dev==dev&&b->blockno==blockno){
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //不再缓存区
  //避免死锁，先释放当前桶锁
  release(&bcache.bufmap_locks[key]);
  acquire(&bcache.eviction_lock);

  for(b=bcache.bufmap[key].next;b;b=b->next){
    if(b->dev==dev&&b->blockno==blockno){
      acquire(&bcache.bufmap_locks[key]);
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.eviction_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf* before_least=0;
  uint holding_bucket=-1;
  //循环所有的桶
  for(int i=0;i<NUM_BUCKET;i++){
    acquire(&bcache.bufmap_locks[i]);
    int newfound=0;//是否找到新的lrubuf
    for(b=&bcache.bufmap[i];b->next;b=b->next){
      if(b->next->refcnt==0&&(!before_least||b->next->lastuse<before_least->next->lastuse)){
        before_least=b;
        newfound=1;
      }
    }

    if(!newfound){
      release(&bcache.bufmap_locks[i]);
    }else{
      if(holding_bucket!=-1){
        release(&bcache.bufmap_locks[holding_bucket]);
      }
      holding_bucket=i;
    }
  }

  if(!before_least){
    panic("bget:no buffers");
  }

  b=before_least->next;

  if(holding_bucket!=key){//要偷的块不在key桶
    before_least->next=b->next;
    release(&bcache.bufmap_locks[holding_bucket]);
    //将LRU_BUF添加到key桶
    acquire(&bcache.bufmap_locks[key]);
    b->next=bcache.bufmap[key].next;
    bcache.bufmap[key].next=b;
  }

  b->dev=dev;
  b->blockno=blockno;
  b->refcnt=1;
  b->valid=0;

  release(&bcache.bufmap_locks[key]);
  release(&bcache.eviction_lock);
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

  uint key=BUFMAP_HASH(b->dev,b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if(b->refcnt==0){
    b->lastuse=ticks;
  }
  release(&bcache.bufmap_locks[key]);
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

void
bpin(struct buf *b) {
  uint key=BUFMAP_HASH(b->dev,b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key=BUFMAP_HASH(b->dev,b->blockno);
  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}


