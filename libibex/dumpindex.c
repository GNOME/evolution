/*
  Dump the hash tables from an ibex file.
 */

#include <stdio.h>
#include <stdlib.h>

#include "ibex_internal.h"

extern void ibex_hash_dump(struct _IBEXIndex *index);

int main(int argc, char **argv)
{
	ibex *ib;

	if (argc != 2) {
		printf("Usage: %s ibexfile\n", argv[0]);
		return 1;
	}
	ib = ibex_open(argv[1], O_RDONLY, 0);
	if (ib == NULL) {
		perror("Opening ibex file\n");
		return 1;
	}

	ibex_hash_dump(ib->words->wordindex);
	ibex_hash_dump(ib->words->nameindex);

	ibex_close(ib);

	return 0;
}
