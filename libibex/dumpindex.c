/*
  Dump the hash tables from an ibex file.
 */

#include <stdio.h>
#include <stdlib.h>

#include "ibex_internal.h"

extern void ibex_hash_dump(struct _IBEXIndex *index);

static void
index_iterate(struct _IBEXIndex *index)
{
	struct _IBEXCursor *idc;
	int len;
	char *key;
	int total = 0, totallen = 0;

	idc = index->klass->get_cursor(index);
	key = idc->klass->next_key(idc, &len);
	while (len) {
		total++;
		totallen += len;
		printf("%s\n", key);
		g_free(key);
		key = idc->klass->next_key(idc, &len);
	}
	g_free(key);

	idc->klass->close(idc);

	printf("Iterate Totals: %d items, total bytes %d\n", total, totallen);
}

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

	index_iterate(ib->words->wordindex);
	index_iterate(ib->words->nameindex);

	ibex_close(ib);

	return 0;
}
