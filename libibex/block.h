
/* public interfaces for block io routines */

#ifndef _BLOCK_H
#define _BLOCK_H

/*#define IBEX_STATS*/		/* define to get/dump block access stats */

#include <glib.h>

typedef guint32 nameid_t;
typedef guint32 blockid_t;

#define BLOCK_BITS (8)
#define BLOCK_SIZE (1<<BLOCK_BITS)
#define CACHE_SIZE 256		/* blocks in disk cache */

/* root block */
struct _root {
	char version[4];

	blockid_t free;		/* list of free blocks */
	blockid_t roof;		/* top of allocated space, everything below is in a free or used list */
	blockid_t tail;		/* list of 'tail' blocks */

	blockid_t words;	/* root of words index */
	blockid_t names;	/* root of names index */
};

/* basic disk structure for (data) blocks */
struct _block {
	unsigned int next:32-BLOCK_BITS;	/* next block */
	unsigned int used:BLOCK_BITS;		/* number of elements used */

	nameid_t bl_data[(BLOCK_SIZE-4)/4];	/* references */
};

/* custom list structure, for a simple/efficient cache */
struct _listnode {
	struct _listnode *next;
	struct _listnode *prev;
};
struct _list {
	struct _listnode *head;
	struct _listnode *tail;
	struct _listnode *tailpred;
};

void ibex_list_new(struct _list *v);
struct _listnode *ibex_list_addhead(struct _list *l, struct _listnode *n);
struct _listnode *ibex_list_addtail(struct _list *l, struct _listnode *n);
struct _listnode *ibex_list_remove(struct _listnode *n);

/* in-memory structure for block cache */
struct _memblock {
	struct _memblock *next;
	struct _memblock *prev;

	blockid_t block;
	int flags;

	struct _block data;
};
#define BLOCK_DIRTY (1<<0)

struct _memcache {
	struct _list nodes;
	int count;		/* nodes in cache */

	GHashTable *index;	/* blockid->memblock mapping */
	int fd;			/* file fd */

#ifdef IBEX_STATS
	GHashTable *stats;
#endif
	/* temporary here */
	struct _IBEXWord *words; /* word index */
};

#ifdef IBEX_STATS
struct _stat_info {
	int read;
	int write;
	int cache_hit;
	int cache_miss;
};
#endif /* IBEX_STATS */

struct _memcache *ibex_block_cache_open(const char *name, int flags, int mode);
void ibex_block_cache_close(struct _memcache *block_cache);
void ibex_block_cache_sync(struct _memcache *block_cache);
void ibex_block_cache_flush(struct _memcache *block_cache);

blockid_t ibex_block_get(struct _memcache *block_cache);
void ibex_block_free(struct _memcache *block_cache, blockid_t blockid);
void ibex_block_dirty(struct _block *block);
struct _block *ibex_block_read(struct _memcache *block_cache, blockid_t blockid);

#define block_number(x) ((x)>>BLOCK_BITS)
#define block_location(x) ((x)<<BLOCK_BITS)

#endif /* ! _BLOCK_H */
