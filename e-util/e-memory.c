/*
 * Copyright (c) 2000, 2001 Ximian Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *	    Jacob Berkman <jacob@ximian.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#include <string.h> /* memset() */
#include <stdlib.h> /* alloca() */
#include <glib.h>

#define s(x)			/* strv debug */
#define p(x)   /* poolv debug */
#define p2(x)   /* poolv assertion checking */

/*#define MALLOC_CHECK*/

/*#define PROFILE_POOLV*/

#ifdef PROFILE_POOLV
#include <time.h>
#define pp(x) x
#else
#define pp(x)
#endif

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

#ifdef MALLOC_CHECK
#include <mcheck.h>
#include <stdio.h>
static void
checkmem(void *p)
{
	if (p) {
		int status = mprobe(p);

		switch (status) {
		case MCHECK_HEAD:
			printf("Memory underrun at %p\n", p);
			abort();
		case MCHECK_TAIL:
			printf("Memory overrun at %p\n", p);
			abort();
		case MCHECK_FREE:
			printf("Double free %p\n", p);
			abort();
		}
	}
}
#define MPROBE(x) checkmem((void *)(x))
#else
#define MPROBE(x)
#endif

/* mempool class */

typedef struct _MemChunkFreeNode {
	struct _MemChunkFreeNode *next;
	unsigned int atoms;
} MemChunkFreeNode;

typedef struct _EMemChunk {
	unsigned int blocksize;	/* number of atoms in a block */
	unsigned int atomsize;	/* size of each atom */
	GPtrArray *blocks;	/* blocks of raw memory */
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
	m->blocks = g_ptr_array_new();
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
	char *b;
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
		b = g_malloc(m->blocksize * m->atomsize);
		g_ptr_array_add(m->blocks, b);
		f = (MemChunkFreeNode *)&b[m->atomsize];
		f->atoms = m->blocksize-1;
		f->next = NULL;
		m->free = f;
		return b;
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
	int i;
	MemChunkFreeNode *f, *h = NULL;

	for (i=0;i<m->blocks->len;i++) {
		f = (MemChunkFreeNode *)m->blocks->pdata[i];
		f->atoms = m->blocksize;
		f->next = h;
		h = f;
	}
	m->free = h;
}

struct _cleaninfo {
	struct _cleaninfo *next;
	char *base;
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
	if (a->base <= mem) {
		if (mem < &a->base[a->size])
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
	int i;
	MemChunkFreeNode *f;
	struct _cleaninfo *ci, *hi = NULL;

	f = m->free;
	if (m->blocks->len == 0 || f == NULL)
		return;

	/* first, setup the tree/list so we can map free block addresses to block addresses */
	tree = g_tree_new((GCompareFunc)tree_compare);
	for (i=0;i<m->blocks->len;i++) {
		ci = alloca(sizeof(*ci));
		ci->count = 0;
		ci->base = m->blocks->pdata[i];
		ci->size = m->blocksize * m->atomsize;
		g_tree_insert(tree, ci, ci);
		ci->next = hi;
		hi = ci;
	}

	/* now, scan all free nodes, and count them in their tree node */
	while (f) {
		ci = g_tree_search(tree, (GCompareFunc) tree_search, f);
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
			MemChunkFreeNode *prev = NULL;
			
			f = m->free;
			while (f) {
				if (tree_search (ci, (void *) f) == 0) {
					/* prune this node from our free-node list */
					if (prev)
						prev->next = f->next;
					else
						m->free = f->next;
				} else {
					prev = f;
				}
				
				f = f->next;
			}
			
			g_ptr_array_remove_fast(m->blocks, ci->base);
			g_free(ci->base);
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
	int i;

	if (m == NULL)
		return;

	for (i=0;i<m->blocks->len;i++)
		g_free(m->blocks->pdata[i]);
	g_ptr_array_free(m->blocks, TRUE);
	g_free(m);
}

typedef struct _MemPoolNode {
	struct _MemPoolNode *next;

	int free;
} MemPoolNode;

typedef struct _MemPoolThresholdNode {
	struct _MemPoolThresholdNode *next;
} MemPoolThresholdNode;

#define ALIGNED_SIZEOF(t)	((sizeof (t) + G_MEM_ALIGN - 1) & -G_MEM_ALIGN)

typedef struct _EMemPool {
	int blocksize;
	int threshold;
	unsigned int align;
	struct _MemPoolNode *blocks;
	struct _MemPoolThresholdNode *threshold_blocks;
} MemPool;

/* a pool of mempool header blocks */
static MemChunk *mempool_memchunk;
#ifdef G_THREADS_ENABLED
static GStaticMutex mempool_mutex = G_STATIC_MUTEX_INIT;
#endif

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

#ifdef G_THREADS_ENABLED
	g_static_mutex_lock(&mempool_mutex);
#endif
	if (mempool_memchunk == NULL) {
		mempool_memchunk = e_memchunk_new(8, sizeof(MemPool));
	}
	pool = e_memchunk_alloc(mempool_memchunk);
#ifdef G_THREADS_ENABLED
	g_static_mutex_unlock(&mempool_mutex);
#endif
	if (threshold >= blocksize)
		threshold = blocksize * 2 / 3;
	pool->blocksize = blocksize;
	pool->threshold = threshold;
	pool->blocks = NULL;
	pool->threshold_blocks = NULL;

	switch (flags & E_MEMPOOL_ALIGN_MASK) {
	case E_MEMPOOL_ALIGN_STRUCT:
	default:
		pool->align = G_MEM_ALIGN-1;
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
void *e_mempool_alloc(MemPool *pool, register int size)
{
	size = (size + pool->align) & (~(pool->align));
	if (size>=pool->threshold) {
		MemPoolThresholdNode *n;

		n = g_malloc(ALIGNED_SIZEOF(*n) + size);
		n->next = pool->threshold_blocks;
		pool->threshold_blocks = n;
		return (char *) n + ALIGNED_SIZEOF(*n);
	} else {
		register MemPoolNode *n;

		n = pool->blocks;
		if (n && n->free >= size) {
			n->free -= size;
			return (char *) n + ALIGNED_SIZEOF(*n) + n->free;
		}

		/* maybe we could do some sort of the free blocks based on size, but
		   it doubt its worth it at all */

		n = g_malloc(ALIGNED_SIZEOF(*n) + pool->blocksize);
		n->next = pool->blocks;
		pool->blocks = n;
		n->free = pool->blocksize - size;
		return (char *) n + ALIGNED_SIZEOF(*n) + n->free;
	}
}

char *e_mempool_strdup(EMemPool *pool, const char *str)
{
	char *out;

	out = e_mempool_alloc(pool, strlen(str)+1);
	strcpy(out, str);

	return out;
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
#ifdef G_THREADS_ENABLED
		g_static_mutex_lock(&mempool_mutex);
#endif
		e_memchunk_free(mempool_memchunk, pool);
#ifdef G_THREADS_ENABLED
		g_static_mutex_unlock(&mempool_mutex);
#endif
	}
}


/*
  string array classes
*/

#define STRV_UNPACKED ((unsigned char)(~0))

struct _EStrv {
	unsigned char length;	/* how many entries we have (or the token STRV_UNPACKED) */
	char data[1];		/* data follows */
};

struct _s_strv_string {
	char *string;		/* the string to output */
	char *free;		/* a string to free, if we referenced it */
};

struct _e_strvunpacked {
	unsigned char type;	/* we overload last to indicate this is unpacked */
	MemPool *pool;		/* pool of memory for strings */
	struct _EStrv *source;	/* if we were converted from a packed one, keep the source around for a while */
	unsigned int length;
	struct _s_strv_string strings[1]; /* the string array data follows */
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
struct _EStrv *
e_strv_new(int size)
{
	struct _e_strvunpacked *s;

	g_assert(size<255);

	s = g_malloc(sizeof(*s) + (size-1)*sizeof(s->strings[0]));
	s(printf("new strv=%p, size = %d bytes\n", s, sizeof(*s) + (size-1)*sizeof(char *)));
	s->type = STRV_UNPACKED;
	s->pool = NULL;
	s->length = size;
	s->source = NULL;
	memset(s->strings, 0, size*sizeof(s->strings[0]));

	return (struct _EStrv *)s;
}

static struct _e_strvunpacked *
strv_unpack(struct _EStrv *strv)
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
		s->strings[i].string = p;
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
struct _EStrv *
e_strv_set_ref(struct _EStrv *strv, int index, char *str)
{
	struct _e_strvunpacked *s;

	s(printf("set ref %d '%s'\nawkmeharder: %s\n ", index, str, str));

	if (strv->length != STRV_UNPACKED)
		s = strv_unpack(strv);
	else
		s = (struct _e_strvunpacked *)strv;

	g_assert(index>=0 && index < s->length);

	s->strings[index].string = str;

	return (struct _EStrv *)s;
}

/**
 * e_strv_set_ref_free:
 * @strv: 
 * @index: 
 * @str: 
 * 
 * Set a string by reference, similar to set_ref, but also
 * free the string when finished with it.  The string
 * is not copied until the strv is packed, and not at
 * all if the index is overwritten.
 * 
 * Return value: @strv if already unpacked, otherwise an packed
 * EStrv.
 **/
struct _EStrv *
e_strv_set_ref_free(struct _EStrv *strv, int index, char *str)
{
	struct _e_strvunpacked *s;

	s(printf("set ref %d '%s'\nawkmeevenharder: %s\n ", index, str, str));

	if (strv->length != STRV_UNPACKED)
		s = strv_unpack(strv);
	else
		s = (struct _e_strvunpacked *)strv;

	g_assert(index>=0 && index < s->length);

	s->strings[index].string = str;
	if (s->strings[index].free)
		g_free(s->strings[index].free);
	s->strings[index].free = str;

	return (struct _EStrv *)s;
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
struct _EStrv *
e_strv_set(struct _EStrv *strv, int index, const char *str)
{
	struct _e_strvunpacked *s;

	s(printf("set %d '%s'\n", index, str));

	if (strv->length != STRV_UNPACKED)
		s = strv_unpack(strv);
	else
		s = (struct _e_strvunpacked *)strv;

	g_assert(index>=0 && index < s->length);

	if (s->pool == NULL)
		s->pool = e_mempool_new(1024, 512, E_MEMPOOL_ALIGN_BYTE);

	s->strings[index].string = e_mempool_alloc(s->pool, strlen(str)+1);
	strcpy(s->strings[index].string, str);

	return (struct _EStrv *)s;
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
struct _EStrv *
e_strv_pack(struct _EStrv *strv)
{
	struct _e_strvunpacked *s;
	int len, i;
	register char *src, *dst;

	if (strv->length == STRV_UNPACKED) {
		s = (struct _e_strvunpacked *)strv;

		s(printf("packing string\n"));

		len = 0;
		for (i=0;i<s->length;i++)
			len += s->strings[i].string?strlen(s->strings[i].string)+1:1;

		strv = g_malloc(sizeof(*strv) + len);
		s(printf("allocating strv=%p, size = %d\n", strv, sizeof(*strv)+len));
		strv->length = s->length;
		dst = strv->data;
		for (i=0;i<s->length;i++) {
			if ((src = s->strings[i].string)) {
				while ((*dst++ = *src++))
					;
			} else {
				*dst++ = 0;
			}
		}
		e_strv_destroy((struct _EStrv *)s);
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
e_strv_get(struct _EStrv *strv, int index)
{
	struct _e_strvunpacked *s;
	char *p;

	if (strv->length != STRV_UNPACKED) {
		g_assert(index>=0 && index < strv->length);
		p = strv->data;
		while (index > 0) {
			while (*p++ != 0)
				;
			index--;
		}
		return p;
	} else {
		s = (struct _e_strvunpacked *)strv;
		g_assert(index>=0 && index < s->length);
		return s->strings[index].string?s->strings[index].string:"";
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
e_strv_destroy(struct _EStrv *strv)
{
	struct _e_strvunpacked *s;
	int i;

	s(printf("freeing strv\n"));

	if (strv->length == STRV_UNPACKED) {
		s = (struct _e_strvunpacked *)strv;
		if (s->pool)
			e_mempool_destroy(s->pool);
		if (s->source)
			e_strv_destroy(s->source);
		for (i=0;i<s->length;i++) {
			if (s->strings[i].free)
				g_free(s->strings[i].free);
		}
	}

	s(printf("freeing strv=%p\n", strv));

	g_free(strv);
}



/* string pool stuff */

/* TODO:
    garbage collection, using the following technique:
      Create a memchunk for each possible size of poolv, and allocate every poolv from those
      To garbage collect, scan all memchunk internally, ignoring any free areas (or mark each
        poolv when freeing it - set length 0?), and find out which strings are not anywhere,
	then free them.

    OR:
       Just keep a refcount in the hashtable, instead of duplicating the key pointer.

   either would also require a free for the mempool, so ignore it for now */

/*#define POOLV_REFCNT*/ /* Define to enable refcounting code that does
			automatic garbage collection of unused strings */

static GHashTable *poolv_pool = NULL;
static EMemPool *poolv_mempool = NULL;

#ifdef MALLOC_CHECK
static GPtrArray *poolv_table = NULL;
#endif

#ifdef PROFILE_POOLV
static gulong poolv_hits = 0;
static gulong poolv_misses = 0;
static unsigned long poolv_mem, poolv_count;
#endif

#ifdef G_THREADS_ENABLED
static GStaticMutex poolv_mutex = G_STATIC_MUTEX_INIT;
#endif

struct _EPoolv {
	unsigned char length;
	char *s[1];
};

/**
 * e_poolv_new: @size: The number of elements in the poolv, maximum of 254 elements.
 *
 * create a new poolv (string vector which shares a global string
 * pool).  poolv's can be used to work with arrays of strings which
 * save memory by eliminating duplicated allocations of the same
 * string.
 *
 * this is useful when you have a log of read-only strings that do not
 * go away and are duplicated a lot (such as email headers).
 *
 * we should probably in the future ref count the strings contained in
 * the hash table, but for now let's not.
 *
 * Return value: new pooled string vector
 **/
EPoolv *
e_poolv_new(unsigned int size)
{
	EPoolv *poolv;

	g_assert(size < 255);

	poolv = g_malloc0(sizeof (*poolv) + (size - 1) * sizeof (char *));
	poolv->length = size;

#ifdef G_THREADS_ENABLED
	g_static_mutex_lock(&poolv_mutex);
#endif
	if (!poolv_pool)
		poolv_pool = g_hash_table_new(g_str_hash, g_str_equal);

	if (!poolv_mempool)
		poolv_mempool = e_mempool_new(32 * 1024, 512, E_MEMPOOL_ALIGN_BYTE);

#ifdef MALLOC_CHECK
	{
		int i;

		if (poolv_table == NULL)
			poolv_table = g_ptr_array_new();

		for (i=0;i<poolv_table->len;i++)
			MPROBE(poolv_table->pdata[i]);

		g_ptr_array_add(poolv_table, poolv);
	}
#endif

#ifdef G_THREADS_ENABLED
	g_static_mutex_unlock(&poolv_mutex);
#endif

	p(printf("new poolv=%p\tsize=%d\n", poolv, sizeof(*poolv) + (size-1)*sizeof(char *)));

#ifdef PROFILE_POOLV
	poolv_count++;
#endif
	return poolv;
}

/**
 * e_poolv_cpy:
 * @dest: destination pooled string vector
 * @src: source pooled string vector
 *
 * Copy the contents of a pooled string vector
 *
 * Return value: @dest, which may be re-allocated if the strings
 * are different lengths.
 **/
EPoolv *
e_poolv_cpy(EPoolv *dest, const EPoolv *src)
{
#ifdef POOLV_REFCNT
	int i;
	unsigned int ref;
	char *key;
#endif

	p2(g_return_val_if_fail (dest != NULL, NULL));
	p2(g_return_val_if_fail (src != NULL, NULL));

	MPROBE(dest);
	MPROBE(src);

	if (dest->length != src->length) {
		e_poolv_destroy(dest);
		dest = e_poolv_new(src->length);
	}

#ifdef POOLV_REFCNT
#ifdef G_THREADS_ENABLED
	g_static_mutex_lock(&poolv_mutex);
#endif
	/* ref new copies */
	for (i=0;i<src->length;i++) {
		if (src->s[i]) {
			if (g_hash_table_lookup_extended(poolv_pool, src->s[i], (void **)&key, (void **)&ref)) {
				g_hash_table_insert(poolv_pool, key, (void *)(ref+1));
			} else {
				g_assert_not_reached();
			}
		}
	}

	/* unref the old ones */
	for (i=0;i<dest->length;i++) {
		if (dest->s[i]) {
			if (g_hash_table_lookup_extended(poolv_pool, dest->s[i], (void **)&key, (void **)&ref)) {
				/* if ref == 1 free it */
				g_assert(ref > 0);
				g_hash_table_insert(poolv_pool, key, (void *)(ref-1));
			} else {
				g_assert_not_reached();
			}
		}
	}
#ifdef G_THREADS_ENABLED
	g_static_mutex_unlock(&poolv_mutex);
#endif
#endif

	memcpy(dest->s, src->s, src->length * sizeof (char *));

	return dest;
}

#ifdef PROFILE_POOLV
static void
poolv_profile_update (void)
{
	static time_t last_time = 0;
	time_t new_time;

	new_time = time (NULL);
	if (new_time - last_time < 5)
		return;

	printf("poolv profile: %lu hits, %lu misses: %d%% hit rate, memory: %lu, instances: %lu\n", 
	       poolv_hits, poolv_misses, 
	       (int)(100.0 * ((double) poolv_hits / (double) (poolv_hits + poolv_misses))),
	       poolv_mem, poolv_count);

	last_time = new_time;
}
#endif

/**
 * e_poolv_set:
 * @poolv: pooled string vector
 * @index: index in vector of string
 * @str: string to set
 * @freeit: whether the caller is releasing its reference to the
 * string
 *
 * Set a string vector reference.  If the caller will no longer be
 * referencing the string, freeit should be TRUE.  Otherwise, this
 * will duplicate the string if it is not found in the pool.
 *
 * Return value: @poolv
 **/
EPoolv *
e_poolv_set (EPoolv *poolv, int index, char *str, int freeit)
{
#ifdef POOLV_REFCNT
	unsigned int ref;
	char *key;
#endif

	p2(g_return_val_if_fail (poolv != NULL, NULL));

	g_assert(index >=0 && index < poolv->length);

	MPROBE(poolv);

	p(printf("setting %d `%s'\n", index, str));

	if (!str) {
#ifdef POOLV_REFCNT
		if (poolv->s[index]) {
			if (g_hash_table_lookup_extended(poolv_pool, poolv->s[index], (void **)&key, (void **)&ref)) {
				g_assert(ref > 0);
				g_hash_table_insert(poolv_pool, key, (void *)(ref-1));
			} else {
				g_assert_not_reached();
			}
		}
#endif
		poolv->s[index] = NULL;
		return poolv;
	}

#ifdef G_THREADS_ENABLED
	g_static_mutex_lock(&poolv_mutex);
#endif

#ifdef POOLV_REFCNT
	if (g_hash_table_lookup_extended(poolv_pool, str, (void **)&key, (void **)&ref)) {
		g_hash_table_insert(poolv_pool, key, (void *)(ref+1));
		poolv->s[index] = key;
# ifdef PROFILE_POOLV
		poolv_hits++;
		poolv_profile_update ();
# endif
	} else {
# ifdef PROFILE_POOLV
		poolv_misses++;
		poolv_mem += strlen(str);
		poolv_profile_update ();
# endif
		poolv->s[index] = e_mempool_strdup(poolv_mempool, str);
		g_hash_table_insert(poolv_pool, poolv->s[index], (void *)1);
	}

#else  /* !POOLV_REFCNT */
	if ((poolv->s[index] = g_hash_table_lookup(poolv_pool, str)) != NULL) {
# ifdef PROFILE_POOLV
		poolv_hits++;
		poolv_profile_update ();
# endif
	} else {
# ifdef PROFILE_POOLV
		poolv_misses++;
		poolv_mem += strlen(str);
		poolv_profile_update ();
# endif
		poolv->s[index] = e_mempool_strdup(poolv_mempool, str);
		g_hash_table_insert(poolv_pool, poolv->s[index], poolv->s[index]);
	}
#endif /* !POOLV_REFCNT */

#ifdef G_THREADS_ENABLED
	g_static_mutex_unlock(&poolv_mutex);
#endif

	if (freeit)
		g_free(str);

	return poolv;
}

/**
 * e_poolv_get:
 * @poolv: pooled string vector
 * @index: index in vector of string
 *
 * Retrieve a string by index.  This could possibly just be a macro.
 *
 * Since the pool is never freed, this string does not need to be
 * duplicated, but should not be modified.
 *
 * Return value: string at that index.
 **/
const char *
e_poolv_get(EPoolv *poolv, int index)
{
	g_assert(poolv != NULL);
	g_assert(index>= 0 && index < poolv->length);

	MPROBE(poolv);

	p(printf("get %d = `%s'\n", index, poolv->s[index]));

	return poolv->s[index]?poolv->s[index]:"";
}

/**
 * e_poolv_destroy:
 * @poolv: pooled string vector to free
 *
 * Free a pooled string vector.  This doesn't free the strings from
 * the vector, however.
 **/
void
e_poolv_destroy(EPoolv *poolv)
{
#ifdef POOLV_REFCNT
	int i;
	unsigned int ref;
	char *key;

	MPROBE(poolv);

#ifdef G_THREADS_ENABLED
	g_static_mutex_lock(&poolv_mutex);
#endif

#ifdef MALLOC_CHECK
	for (i=0;i<poolv_table->len;i++)
		MPROBE(poolv_table->pdata[i]);

	g_ptr_array_remove_fast(poolv_table, poolv);
#endif

	for (i=0;i<poolv->length;i++) {
		if (poolv->s[i]) {
			if (g_hash_table_lookup_extended(poolv_pool, poolv->s[i], (void **)&key, (void **)&ref)) {
				/* if ref == 1 free it */
				g_assert(ref > 0);
				g_hash_table_insert(poolv_pool, key, (void *)(ref-1));
			} else {
				g_assert_not_reached();
			}
		}
	}
#ifdef G_THREADS_ENABLED
	g_static_mutex_unlock(&poolv_mutex);
#endif
#endif

#ifdef PROFILE_POOLV
	poolv_count++;
#endif
	g_free(poolv);
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
	struct _EStrv *s;

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
