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

fcntl.h

*/

/* fcntl.h  -- Defines mutexes in terms of read/write locks on files.
 *             filerwlock, filerwlockCreate, filerwlockDelete,
 *             filerwreadlock, filerwlockUnlock
 */

typedef struct {
    int fd;
    int n;
} filerwlock;

/* Create N read/write locks and returns the id of this cluster of locks. */
filerwlock * filerwlockCreate(char *filename, int n);

/* Delete the cluster of read/write locks associated with fl. */
int filerwlockDelete(filerwlock *fl);

/* Given the read/write lock cluster fl, lock its ith element */
int filerwreadlock(filerwlock *fl, int i);

int filerwwritelock(filerwlock *fl, int i);

/* Given the lock cluster fl, unlock its ith element */
int filerwunlock(filerwlock *fl, int i);

/* Given the lock cluster fl, it read locks all its elements */
int filerwlongreadlock(filerwlock *fl);

/* Given the lock cluster fl, it unlocks all its elements */
int filerwlongunlock(filerwlock *fl);
