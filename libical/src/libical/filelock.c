
/*
  I Stole this from:
  http://www.cis.temple.edu/~ingargio/old/cis307s96/readings/rwlockexample.html

  CIS 307: An example using Read/Write File Locks
  [fcntl.h], [fcntl.c], [fcntlmain.c]

In Stevens "Advanced Programming in the Unix Environment" we see ways to use
the Unix service fcntl to lock portions of a file for reading and writing in
the manner stated in the Reader and Writer problem [any number of readers at
a time, but writers must operate alone]. Here we have three files that adapt
and use the code from Stevens:

*fcntl.h: Specification of the locking functions.
*fcntl.c: Implementation of the locking functions.
*fcntlmain.c: Driver that does a simple test of the locking functions.

WARNING: A file lock request which is blocked can be interrupted by a
signal. In this case the lock operation returns EINTR. Thus we may think we
got a lock when we really don't. A solution is to block signals when
locking. Another solution is to test the value returned by the lock
operation and relock if the value is EINTR. Another solution, which we adopt
here, is to do nothing about it.

/* fcntl.c  -- Defines mutexes in terms of read/write locks on files.
 *             (code is mostly from Stevens: Advanced Programming in the
 *             Unix environment. See from page 367 on.
 *             filerwlock, filerwlockCreate, filerwlockDelete,
 *             filerwreadlock, filerwlongreadlock, filerwlongunlock,
 *             filerwlockUnlock
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int lock_reg(int, int, int, off_t, int, off_t);

#define read_lock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)

#define readw_lock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLKW, F_RDLCK, offset, whence, len)

#define write_lock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)

#define writew_lock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLKW, F_WRLCK, offset, whence, len)

#define un_lock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)

pid_t   lock_test(int, int , off_t , int , off_t );

#define is_readlock(fd, offset, whence, len) \
                        lock_test(fd, F_RDLCK, offset, whence, len)

#define is_writelock(fd, offset, whence, len) \
                        lock_test(fd, F_WRLCK, offset, whence, len)

int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
  struct flock lock;
  lock.l_type = type;     /* F_RDLCK, F_WRLCK, F_UNLCK */
  lock.l_start = offset;  /* byte offset relative to l_whence */
  lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */
  lock.l_len = len;       /* #bytes (0 means to EOF) */
  return (fcntl(fd, cmd, &lock));
}

pid_t   lock_test(int fd, int type, off_t offset, int whence, off_t len)
{
  struct flock lock;
  lock.l_type = type;     /* F_RDLCK or F_WRLCK */
  lock.l_start = offset;  /* byte offset relative to l_whence */
  lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */
  lock.l_len = len;       /* #bytes (0 means to EOF) */
  if (fcntl(fd,F_GETLK,&lock) < 0){
    perror("fcntl"); exit(1);}
  if (lock.l_type == F_UNLCK)
    return (0);        /* false, region is not locked by another process */
  return (lock.l_pid); /* true, return pid of lock owner */
}

typedef struct {
    int fd;
    int n;} filerwlock;

/* Create N read/write locks and returns the id of this cluster of locks. */
filerwlock * filerwlockCreate(char *filename, int n) {
   filerwlock *fl = (filerwlock *)malloc(sizeof(filerwlock));
   if (((fl->fd) = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR)) < 0) {
      perror("open");
      exit(1);}
   fl->n = n;
   return fl;
 }

/* Delete the cluster of read/write locks associated with fl. */
int filerwlockDelete(filerwlock *fl) {
   if (close(fl->fd) < 0) {
     perror("close");
     exit(1);}
   return free(fl);
 }

/* Given the read/write lock cluster fl, lock its ith element */
int filerwreadlock(filerwlock *fl, int i) {
   if ((i < 0) | (i >= fl->n)) {
     printf("filerwlockLock needs i in range 0 .. %d\n", (fl->n)-1);
     exit(0);}
   readw_lock(fl->fd, i, SEEK_SET, 1);
 }

int filerwwritelock(filerwlock *fl, int i) {
   if ((i < 0) | (i >= fl->n)) {
     printf("filerwlockLock needs i in range 0 .. %d\n", (fl->n)-1);
     exit(0);}
   writew_lock(fl->fd, i, SEEK_SET, 1);
 }

/* Given the lock cluster fl, unlock its ith element */
int filerwunlock(filerwlock *fl, int i){

   if ((i < 0) | (i >= fl->n)) {
     printf("filerwlockUnlock needs i in range 0 .. %d\n", (fl->n)-1);
     exit(0);}
   un_lock(fl->fd, i, SEEK_SET, 1);
 }

/* Given the lock cluster fl, it read locks all its elements */
int filerwlongreadlock(filerwlock *fl) {
  readw_lock(fl->fd, 0, SEEK_SET, fl->n);
}

/* Given the lock cluster fl, it unlocks all its elements */
int filerwlongunlock(filerwlock *fl) {
  un_lock(fl->fd, 0, SEEK_SET, fl->n);
}

