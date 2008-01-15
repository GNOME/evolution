#include <stdio.h>

void bbdb_sync_buddy_list (void);

int
main (void)
{
	printf ("Syncing...\n");

	bbdb_sync_buddy_list ();

	printf ("Done!\n");

	return 0;
}
