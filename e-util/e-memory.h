/*
 * Copyright (C) 2000, Helix Code Inc.
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

#ifndef _E_MEMORY_H
#define _E_MEMORY_H

/* memchunks - allocate/free fixed-size blocks of memory */
/* this is like gmemchunk, only faster and less overhead (only 4 bytes for every atomcount allocations) */
typedef struct _EMemChunk EMemChunk;

EMemChunk *e_memchunk_new(int atomcount, int atomsize);
void *e_memchunk_alloc(EMemChunk *m);
void *e_memchunk_alloc0(EMemChunk *m);
void e_memchunk_free(EMemChunk *m, void *mem);
void e_memchunk_empty(EMemChunk *m);
void e_memchunk_clean(EMemChunk *m);
void e_memchunk_destroy(EMemChunk *m);

/* mempools - allocate variable sized blocks of memory, and free as one */
/* allocation is very fast, but cannot be freed individually */
typedef struct _EMemPool EMemPool;
typedef enum {
	E_MEMPOOL_ALIGN_STRUCT = 0,	/* allocate to native structure alignment */
	E_MEMPOOL_ALIGN_WORD = 1,	/* allocate to words - 16 bit alignment */
	E_MEMPOOL_ALIGN_BYTE = 2,	/* allocate to bytes - 8 bit alignment */
	E_MEMPOOL_ALIGN_MASK = 3, /* which bits determine the alignment information */
} EMemPoolFlags;

EMemPool *e_mempool_new(int blocksize, int threshold, EMemPoolFlags flags);
void *e_mempool_alloc(EMemPool *pool, int size);
char *e_mempool_strdup(EMemPool *pool, const char *str);
void e_mempool_flush(EMemPool *pool, int freeall);
void e_mempool_destroy(EMemPool *pool);

/* strv's string arrays that can be efficiently modified and then compressed mainly for retrival */
/* building is relatively fast, once compressed it takes the minimum amount of memory possible to store */
typedef struct _e_strv EStrv;

EStrv *e_strv_new(int size);
EStrv *e_strv_set_ref(EStrv *strv, int index, char *str);
EStrv *e_strv_set(EStrv *strv, int index, const char *str);
EStrv *e_strv_pack(EStrv *strv);
char *e_strv_get(EStrv *strv, int index);
void e_strv_destroy(EStrv *strv);

#endif /* ! _E_MEMORY_H */
