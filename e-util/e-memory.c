/*
 * Copyright (c) 2000 Helix Code Inc.
 *
 * Author: Michael Zucchi <notzed@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA

*/

#include "e-memory.h"
#include <glib.h>

#define s(x)			/* strv debug */

/*#define TIMEIT*/

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>

struct timeval timeit_start;

static time_start(const char *desc)
{
	gettimeofday(&timeit_start, NULL);
	printf("starting: %s\n", desc);
}

static time_end(const char *desc)
{
	unsigned long diff;
	struct timeval end;

	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= timeit_start.tv_sec * 1000 + timeit_start.tv_usec/1000;
	printf("%s took %ld.%03ld seconds\n",
	       desc, diff / 1000, diff % 1000);
}
#else
#define time_start(x)
#define time_end(x)
#endif

/* mempool class */

#define STRUCT_ALIGN (4)

typedef struct _MemChunkFreeNode {
	struct _MemChunkFreeNode *next;
	unsigned int atoms;
} MemChunkFreeNode;

typedef struct _MemChunkNode {
	struct _MemChunkNode *next;
	char data[1];
} MemChunkNode;

typedef struct _EMemChunk {
	unsigned int blocksize;	/* number of atoms in a block */
	unsigned int atomsize;	/* size of each atom */
	struct _MemChunkNode *blocks;
	struct _MemChunkFreeNode *free;
} MemChunk;

/**
 * e_memchunk_new:
 * @atomcount: The number of atoms stored in a single malloc'd block of memory.
 * @atomsize: The size of each allocation.
 * 
 * Create a new memchunk header.  Memchunks are an efficient way to allocate
 * and deallocate identical sized blocks of memory quickly, and space efficiently.
 * 
 * e_memchunks are effectively the same as gmemchunks, only faster (much), and
 * they use less memory overhead for housekeeping.
 *
 * Return value: The new header.
 **/
MemChunk *e_memchunk_new(int atomcount, int atomsize)
{
	MemChunk *m = g_malloc(sizeof(*m));

	m->blocksize = atomcount;
	m->atomsize = MAX(atomsize, sizeof(MemChunkFreeNode));
	m->blocks = NULL;
	m->free = NULL;

	return m;
}

/**
 * memchunk_alloc:
 * @m: 
 * 
 * Allocate a new atom size block of memory from a memchunk.
 **/
void *e_memchunk_alloc(MemChunk *m)
{
	MemChunkNode *b;
	MemChunkFreeNode *f;
	void *mem;

	f = m->free;
	if (f) {
		f->atoms--;
		if (f->atoms > 0) {
			mem = ((char *)f) + (f->atoms*m->atomsize);
		} else {
			mem = f;
			m->free = m->free->next;
		}
		return mem;
	} else {
		b = g_malloc(m->blocksize * m->atomsize + sizeof(*b) - sizeof(char));
		b->next = m->blocks;
		m->blocks = b;
		f = (MemChunkFreeNode *)&b->data[m->atomsize];
		f->atoms = m->blocksize-1;
		f->next = NULL;
		m->free = f;
		return &b->data;
	}
}

void *e_memchunk_alloc0(EMemChunk *m)
{
	void *mem;

	mem = e_memchunk_alloc(m);
	memset(mem, 0, m->atomsize);

	return mem;
}

/**
 * e_memchunk_free:
 * @m: 
 * @mem: Address of atom to free.
 * 
 * Free a single atom back to the free pool of atoms in the given
 * memchunk.
 **/
void
e_memchunk_free(MemChunk *m, void *mem)
{
	MemChunkFreeNode *f;

	/* put the location back in the free list.  If we knew if the preceeding or following
	   cells were free, we could merge the free nodes, but it doesn't really add much */
	f = mem;
	f->next = m->free;
	m->free = f;
	f->atoms = 1;

	/* we could store the free list sorted - we could then do the above, and also
	   probably improve the locality of reference properties for the allocator */
	/* and it would simplify some other algorithms at that, but slow this one down
	   significantly */
}

/**
 * e_memchunk_empty:
 * @m: 
 * 
 * Clean out the memchunk buffers.  Marks all allocated memory as free blocks,
 * but does not give it back to the system.  Can be used if the memchunk
 * is to be used repeatedly.
 **/
void
e_memchunk_empty(MemChunk *m)
{
	MemChunkNode *b;
	MemChunkFreeNode *f, *h = NULL;

	b = m->blocks;
	while (b) {
		f = (MemChunkFreeNode *)&b->data[0];
		f->atoms = m->blocksize;
		f->next = h;
		h = f;
	}
	m->free = h;
}

struct _cleaninfo {
	struct _cleaninfo *next;
	MemChunkNode *base;
	int count;
	int size;		/* just so tree_search has it, sigh */
};

static int tree_compare(struct _cleaninfo *a, struct _cleaninfo *b)
{
	if (a->base < b->base)
		return -1;
	else if (a->base > b->base)
		return 1;
	return 0;
}

static int tree_search(struct _cleaninfo *a, char *mem)
{
	if (a->base->data <= mem) {
		if (mem < &a->base->data[a->size])
			return 0;
		return 1;
	}
	return -1;
}

/**
 * e_memchunk_clean:
 * @m: 
 * 
 * Scan all empty blocks and check for blocks which can be free'd 
 * back to the system.
 *
 * This routine may take a while to run if there are many allocated
 * memory blocks (if the total number of allocations is many times
 * greater than atomcount).
 **/
void
e_memchunk_clean(MemChunk *m)
{
	GTree *tree;
	MemChunkNode *b, *n;
	MemChunkFreeNode *f;
	struct _cleaninfo *ci, *hi = NULL;

	b = m->blocks;
	f = m->free;
	if (b == NULL || f == NULL)
		return;

	/* first, setup the tree/list so we can map free block addresses to block addresses */
	tree = g_tree_new((GCompareFunc)tree_compare);
	while (b) {
		ci = alloca(sizeof(*ci));
		ci->count = 0;
		ci->base = b;
		ci->size = m->blocksize * m->atomsize;
		g_tree_insert(tree, ci, ci);
		ci->next = hi;
		hi = ci;
		b = b->next;
	}

	/* now, scan all free nodes, and count them in their tree node */
	while (f) {
		ci = g_tree_search(tree, (GSearchFunc)tree_search, f);
		if (ci) {
			ci->count += f->atoms;
		} else {
			g_warning("error, can't find free node in memory block\n");
		}
		f = f->next;
	}

	/* if any nodes are all free, free & unlink them */
	ci = hi;
	while (ci) {
		if (ci->count == m->blocksize) {
			b = (MemChunkNode *)&m->blocks;
			n = b->next;
			while (n) {
				if (n == ci->base) {
					b->next = n->next;
					g_free(n);
					break;
				}
				b = n;
				n = b->next;
			}
		}
		ci = ci->next;
	}

	g_tree_destroy(tree);
}

/**
 * e_memchunk_destroy:
 * @m: 
 * 
 * Free the memchunk header, and all associated memory.
 **/
void
e_memchunk_destroy(MemChunk *m)
{
	MemChunkNode *b, *n;

	if (m == NULL)
		return;

	b = m->blocks;
	while (b) {
		n = b->next;
		g_free(b);
		b = n;
	}
	g_free(m);
}

typedef struct _MemPoolNode {
	struct _MemPoolNode *next;

	int free;
	char data[1];
} MemPoolNode;

typedef struct _MemPoolThresholdNode {
	struct _MemPoolThresholdNode *next;
	char data[1];
} MemPoolThresholdNode;

typedef struct _EMemPool {
	int blocksize;
	int threshold;
	unsigned int align;
	struct _MemPoolNode *blocks;
	struct _MemPoolThresholdNode *threshold_blocks;
} MemPool;

/* a pool of mempool header blocks */
static MemChunk *mempool_memchunk;

/**
 * e_mempool_new:
 * @blocksize: The base blocksize to use for all system alocations.
 * @threshold: If the allocation exceeds the threshold, then it is
 * allocated separately and stored in a separate list.
 * @flags: Alignment options: E_MEMPOOL_ALIGN_STRUCT uses native
 * struct alignment, E_MEMPOOL_ALIGN_WORD aligns to 16 bits (2 bytes),
 * and E_MEMPOOL_ALIGN_BYTE aligns to the nearest byte.  The default
 * is to align to native structures.
 * 
 * Create a new mempool header.  Mempools can be used to efficiently
 * allocate data which can then be freed as a whole.
 *
 * Mempools can also be used to efficiently allocate arbitrarily
 * aligned data (such as strings) without incurring the space overhead
 * of aligning each allocation (which is not required for strings).
 *
 * However, each allocation cannot be freed individually, only all
 * or nothing.
 * 
 * Return value: 
 **/
MemPool *e_mempool_new(int blocksize, int threshold, EMemPoolFlags flags)
{
	MemPool *pool;

	if (mempool_memchunk == NULL) {
		mempool_memchunk = e_memchunk_new(8, sizeof(MemPool));
	}
	pool = e_memchunk_alloc(mempool_memchunk);
	if (threshold >= blocksize)
		threshold = blocksize * 2 / 3;
	pool->blocksize = blocksize;
	pool->threshold = threshold;
	pool->blocks = NULL;
	pool->threshold_blocks = NULL;

	switch (flags & E_MEMPOOL_ALIGN_MASK) {
	case E_MEMPOOL_ALIGN_STRUCT:
	default:
		pool->align = STRUCT_ALIGN-1;
		break;
	case E_MEMPOOL_ALIGN_WORD:
		pool->align = 2-1;
		break;
	case E_MEMPOOL_ALIGN_BYTE:
		pool->align = 1-1;
	}
	return pool;
}

/**
 * e_mempool_alloc:
 * @pool: 
 * @size: 
 * 
 * Allocate a new data block in the mempool.  Size will
 * be rounded up to the mempool's alignment restrictions
 * before being used.
 **/
void *e_mempool_alloc(MemPool *pool, int size)
{
	size = (size + pool->align) & (~(pool->align));
	if (size>=pool->threshold) {
		MemPoolThresholdNode *n;

		n = g_malloc(sizeof(*n) - sizeof(char) + size);
		n->next = pool->threshold_blocks;
		pool->threshold_blocks = n;
		return &n->data[0];
	} else {
		MemPoolNode *n;

		n = pool->blocks;
		while (n) {
			if (n->free >= size) {
				n->free -= size;
				return &n->data[n->free];
			}
			n = n->next;
		}

		n = g_malloc(sizeof(*n) - sizeof(char) + pool->blocksize);
		n->next = pool->blocks;
		pool->blocks = n;
		n->free = pool->blocksize - size;
		return &n->data[n->free];
	}
}

/**
 * e_mempool_flush:
 * @pool: 
 * @freeall: Free all system allocated blocks as well.
 * 
 * Flush used memory and mark allocated blocks as free.
 *
 * If @freeall is #TRUE, then all allocated blocks are free'd
 * as well.  Otherwise only blocks above the threshold are
 * actually freed, and the others are simply marked as empty.
 **/
void e_mempool_flush(MemPool *pool, int freeall)
{
	MemPoolThresholdNode *tn, *tw;
	MemPoolNode *pw, *pn;

	tw = pool->threshold_blocks;
	while (tw) {
		tn = tw->next;
		g_free(tw);
		tw = tn;
	}
	pool->threshold_blocks = NULL;

	if (freeall) {
		pw = pool->blocks;
		while (pw) {
			pn = pw->next;
			g_free(pw);
			pw = pn;
		}
		pool->blocks = NULL;
	} else {
		pw = pool->blocks;
		while (pw) {
			pw->free = pool->blocksize;
			pw = pw->next;
		}
	}
}

/**
 * e_mempool_destroy:
 * @pool: 
 * 
 * Free all memory associated with a mempool.
 **/
void e_mempool_destroy(MemPool *pool)
{
	if (pool) {
		e_mempool_flush(pool, 1);
		e_memchunk_free(mempool_memchunk, pool);
	}
}


/*
  string array classes
*/

#define STRV_UNPACKED ((unsigned char)(~0))

struct _e_strv {
	unsigned char length;	/* how many entries we have (or the token STRV_UNPACKED) */
	char data[1];		/* data follows */
};

struct _e_strvunpacked {
	unsigned char type;	/* we overload last to indicate this is unpacked */
	MemPool *pool;		/* pool of memory for strings */
	struct _e_strv *source;	/* if we were converted from a packed one, keep the source around for a while */
	unsigned int length;
	char *strings[1];	/* string array follows */
};

/**
 * e_strv_new:
 * @size: The number of elements in the strv.  Currently this is limited
 * to 254 elements.
 * 
 * Create a new strv (string array) header.  strv's can be used to
 * create and work with arrays of strings that can then be compressed
 * into a space-efficient static structure.  This is useful
 * where a number of strings are to be stored for lookup, and not
 * generally edited afterwards.
 *
 * The size limit is currently 254 elements.  This will probably not
 * change as arrays of this size suffer significant performance
 * penalties when looking up strings with high indices.
 * 
 * Return value: 
 **/
struct _e_strv *
e_strv_new(int size)
{
	struct _e_strvunpacked *s;

	g_assert(size<255);

	s = g_malloc(sizeof(*s) + (size-1)*sizeof(char *));
	s(printf("new strv=%p, size = %d bytes\n", s, sizeof(*s) + (size-1)*sizeof(char *)));
	s->type = STRV_UNPACKED;
	s->pool = NULL;
	s->length = size;
	s->source = NULL;
	memset(s->strings, 0, size*sizeof(s->strings[0]));

	return (struct _e_strv *)s;
}

static struct _e_strvunpacked *
strv_unpack(struct _e_strv *strv)
{
	struct _e_strvunpacked *s;
	register char *p;
	int i;

	s(printf("unpacking\n"));

	s = (struct _e_strvunpacked *)e_strv_new(strv->length);
	p = strv->data;
	for (i=0;i<s->length;i++) {
		if (i>0)
			while (*p++)
				;
		s->strings[i] = p;
	}
	s->source = strv;
	s->type = STRV_UNPACKED;

	return s;
}

/**
 * e_strv_set_ref:
 * @strv: 
 * @index: 
 * @str: 
 * 
 * Set a string array element by reference.  The string
 * is not copied until the array is packed.
 * 
 * If @strv has been packed, then it is unpacked ready
 * for more inserts, and should be packed again once finished with.
 * The memory used by the original @strv is not freed until
 * the new strv is packed, or freed itself.
 *
 * Return value: A new EStrv if the strv has already
 * been packed, otherwise @strv.
 **/
struct _e_strv *
e_strv_set_ref(struct _e_strv *strv, int index, char *str)
{
	struct _e_strvunpacked *s;

	s(printf("set ref %d '%s'\n ", index, str));

	if (strv->length != STRV_UNPACKED) {
		s = strv_unpack(strv);
	} else {
		s = (struct _e_strvunpacked *)strv;
	}

	s->strings[index] = str;

	return (struct _e_strv *)s;
}

/**
 * e_strv_set:
 * @strv: 
 * @index: 
 * @str: 
 * 
 * Set a string array reference.  The string @str is copied
 * into the string array at location @index.
 * 
 * If @strv has been packed, then it is unpacked ready
 * for more inserts, and should be packed again once finished with.
 *
 * Return value: A new EStrv if the strv has already
 * been packed, otherwise @strv.
 **/
struct _e_strv *
e_strv_set(struct _e_strv *strv, int index, const char *str)
{
	struct _e_strvunpacked *s;

	s(printf("set %d '%s'\n", index, str));

	if (strv->length != STRV_UNPACKED) {
		s = strv_unpack(strv);
	} else {
		s = (struct _e_strvunpacked *)strv;
	}

	if (s->pool == NULL)
		s->pool = e_mempool_new(1024, 512, E_MEMPOOL_ALIGN_BYTE);

	s->strings[index] = e_mempool_alloc(s->pool, strlen(str)+1);
	strcpy(s->strings[index], str);

	return (struct _e_strv *)s;
}

/**
 * e_strv_pack:
 * @strv: 
 * 
 * Pack the @strv into a space efficient structure for later lookup.
 *
 * All strings are packed into a single allocated block, separated
 * by single \0 characters, together with a count byte.
 * 
 * Return value: 
 **/
struct _e_strv *
e_strv_pack(struct _e_strv *strv)
{
	struct _e_strvunpacked *s;
	int len, i;
	register char *src, *dst;

	if (strv->length == STRV_UNPACKED) {
		s = (struct _e_strvunpacked *)strv;

		s(printf("packing string\n"));

		len = 0;
		for (i=0;i<s->length;i++) {
			len += s->strings[i]?strlen(s->strings[i])+1:1;
		}
		strv = g_malloc(sizeof(*strv) + len);
		s(printf("allocating strv=%p, size = %d\n", strv, sizeof(*strv)+len));
		strv->length = s->length;
		dst = strv->data;
		for (i=0;i<s->length;i++) {
			if ((src = s->strings[i])) {
				while ((*dst++ = *src++))
					;
			} else {
				*dst++ = 0;
			}
		}

		e_strv_destroy((struct _e_strv *)s);
	}
	return strv;
}

/**
 * e_strv_get:
 * @strv: 
 * @index: 
 * 
 * Retrieve a string by index.  This function works
 * identically on both packed and unpacked strv's, although
 * may be much slower on a packed strv.
 * 
 * Return value: 
 **/
char *
e_strv_get(struct _e_strv *strv, int index)
{
	struct _e_strvunpacked *s;
	char *p;

	if (strv->length != STRV_UNPACKED) {
		p = strv->data;
		while (index > 0) {
			while (*p++ != 0)
				;
			index--;
		}
		return p;
	} else {
		s = (struct _e_strvunpacked *)strv;
		return s->strings[index]?s->strings[index]:"";
	}
}

/**
 * e_strv_destroy:
 * @strv: 
 * 
 * Free a strv and all associated memory.  Works on packed
 * or unpacked strv's.
 **/
void
e_strv_destroy(struct _e_strv *strv)
{
	struct _e_strvunpacked *s;

	s(printf("freeing strv\n"));

	if (strv->length == STRV_UNPACKED) {
		s = (struct _e_strvunpacked *)strv;
		if (s->pool)
			e_mempool_destroy(s->pool);
		if (s->source)
			e_strv_destroy(s->source);
	}

	s(printf("freeing strv=%p\n", strv));

	g_free(strv);
}

#if 0

#define CHUNK_SIZE (20)
#define CHUNK_COUNT (32)

#define s(x)

main()
{
	int i;
	MemChunk *mc;
	void *mem, *last;
	GMemChunk *gmc;
	struct _e_strv *s;

	s = strv_new(8);
	s = strv_set(s, 1, "Testing 1");
	s = strv_set(s, 2, "Testing 2");
	s = strv_set(s, 3, "Testing 3");
	s = strv_set(s, 4, "Testing 4");
	s = strv_set(s, 5, "Testing 5");
	s = strv_set(s, 6, "Testing 7");

	for (i=0;i<8;i++) {
		printf("s[%d] = %s\n", i, strv_get(s, i));
	}

	s(sleep(5));

	printf("packing ...\n");
	s = strv_pack(s);

	for (i=0;i<8;i++) {
		printf("s[%d] = %s\n", i, strv_get(s, i));
	}

	printf("setting ...\n");

	s = strv_set_ref(s, 1, "Testing 1 x");

	for (i=0;i<8;i++) {
		printf("s[%d] = %s\n", i, strv_get(s, i));
	}

	printf("packing ...\n");
	s = strv_pack(s);

	for (i=0;i<8;i++) {
		printf("s[%d] = %s\n", i, strv_get(s, i));
	}

	strv_free(s);

#if 0
	time_start("Using memchunks");
	mc = memchunk_new(CHUNK_COUNT, CHUNK_SIZE);
	for (i=0;i<1000000;i++) {
		mem = memchunk_alloc(mc);
		if ((i & 1) == 0)
			memchunk_free(mc, mem);
	}
	s(sleep(10));
	memchunk_destroy(mc);
	time_end("allocating 1000000 memchunks, freeing 500k");

	time_start("Using gmemchunks");
	gmc = g_mem_chunk_new("memchunk", CHUNK_SIZE, CHUNK_SIZE*CHUNK_COUNT, G_ALLOC_AND_FREE);
	for (i=0;i<1000000;i++) {
		mem = g_mem_chunk_alloc(gmc);
		if ((i & 1) == 0)
			g_mem_chunk_free(gmc, mem);
	}
	s(sleep(10));
	g_mem_chunk_destroy(gmc);
	time_end("allocating 1000000 gmemchunks, freeing 500k");

	time_start("Using memchunks");
	mc = memchunk_new(CHUNK_COUNT, CHUNK_SIZE);
	for (i=0;i<1000000;i++) {
		mem = memchunk_alloc(mc);
	}
	s(sleep(10));
	memchunk_destroy(mc);
	time_end("allocating 1000000 memchunks");

	time_start("Using gmemchunks");
	gmc = g_mem_chunk_new("memchunk", CHUNK_SIZE, CHUNK_COUNT*CHUNK_SIZE, G_ALLOC_ONLY);
	for (i=0;i<1000000;i++) {
		mem = g_mem_chunk_alloc(gmc);
	}
	s(sleep(10));
	g_mem_chunk_destroy(gmc);
	time_end("allocating 1000000 gmemchunks");

	time_start("Using malloc");
	for (i=0;i<1000000;i++) {
		malloc(CHUNK_SIZE);
	}
	time_end("allocating 1000000 malloc");
#endif
	
}

#endif
