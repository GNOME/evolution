/* This is just a driver to test the filerwlock objects defined in fcntl.c */
/* MAXCHILD processes are forked. They take turns in using LOCKSSIZE locks.*/
/* I compiled the program as follows                                       */
/*      cc fcntlmain.c fcntl.c -o fcntlmain                                 */
/* and then run the image fcntlmain.                                        */
/* Notice that after the program has run I find the file "mylock" in my    */
/* directory. Not very desirable. Perhaps there is a way to avoid that?    */

#include <stdio.h>
#include <sys/types.h>
#include "fcntl.h"

#define LOCKFILE "mylock"
#define LOCKSSIZE 5
#define MAXCHILD 4

void child (int self);

pid_t cldrn[4];
filerwlock *fl;

int
main(void){
    int i;

    fl = filerwlockCreate(LOCKFILE, LOCKSSIZE);

    for (i=0;i < MAXCHILD; i++) {
       if ((cldrn[i]=fork()) < 0) {
          perror("fork");
          exit(1);}
       if (cldrn[i]==0)
          child(i);
     }
    for (i=0; i < MAXCHILD; i++)
       wait();

    filerwlockDelete(fl);
    exit(0);
}

void child (int self) {
    int i, j;
    char s[256];
    for (j=0; j<8; j++) {

	if (self == 0) {
	    filerwwritelock(fl,1);
	} else if (self == (MAXCHILD-1)) {
	    filerwlongreadlock(fl);
	} else {
	    filerwreadlock(fl,1);
	}

	printf("Child %d starts to sleep on lock %d\n", self, 1);

	sleep(3);

	printf("Child %d ends sleep on lock %d\n", self, 1);

	if (self == (MAXCHILD-1)) {
	    filerwlongunlock(fl);
	} else {
	    filerwunlock(fl,1);
	}

	sleep(1);
   }
   exit(0);
}
