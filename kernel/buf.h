struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt; 
  struct buf *prev;
  struct buf *next; // LRU cache list
  uchar data[BSIZE];
  uint timestamp; //time stamp
};

