/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2003 Ximian Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "libedataserver/e-msgport.h"

#include "camel-block-file.h"
#include "camel-partition-table.h"

/* Do we synchronously write table updates - makes the
   tables consistent after program crash without sync */
/*#define SYNC_UPDATES*/

#define d(x) /*(printf("%s(%d):%s: ",  __FILE__, __LINE__, __PRETTY_FUNCTION__),(x))*/
/* key index debug */
#define k(x) /*(printf("%s(%d):%s: ",  __FILE__, __LINE__, __PRETTY_FUNCTION__),(x))*/


struct _CamelPartitionTablePrivate {
	pthread_mutex_t lock;	/* for locking partition */
};

#define CAMEL_PARTITION_TABLE_LOCK(kf, lock) (pthread_mutex_lock(&(kf)->priv->lock))
#define CAMEL_PARTITION_TABLE_UNLOCK(kf, lock) (pthread_mutex_unlock(&(kf)->priv->lock))


static void
camel_partition_table_class_init(CamelPartitionTableClass *klass)
{
}

static void
camel_partition_table_init(CamelPartitionTable *cpi)
{
	struct _CamelPartitionTablePrivate *p;

	e_dlist_init(&cpi->partition);

	p = cpi->priv = g_malloc0(sizeof(*cpi->priv));
	pthread_mutex_init(&p->lock, NULL);
}

static void
camel_partition_table_finalise(CamelPartitionTable *cpi)
{
	CamelBlock *bl;
	struct _CamelPartitionTablePrivate *p;

	p = cpi->priv;

	if (cpi->blocks) {
		while ((bl = (CamelBlock *)e_dlist_remhead(&cpi->partition))) {
			camel_block_file_sync_block(cpi->blocks, bl);
			camel_block_file_unref_block(cpi->blocks, bl);
		}
		camel_block_file_sync(cpi->blocks);

		camel_object_unref((CamelObject *)cpi->blocks);
	}
	
	pthread_mutex_destroy(&p->lock);
	
	g_free(p);

}

CamelType
camel_partition_table_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_object_get_type(), "CamelPartitionTable",
					   sizeof (CamelPartitionTable),
					   sizeof (CamelPartitionTableClass),
					   (CamelObjectClassInitFunc) camel_partition_table_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_partition_table_init,
					   (CamelObjectFinalizeFunc) camel_partition_table_finalise);
	}
	
	return type;
}

/* ********************************************************************** */

/*
  Have 2 hashes:
  Name -> nameid
  Word -> wordid

nameid is pointer to name file, includes a bit to say if name is deleted
wordid is a pointer to word file, includes pointer to start of word entries

delete a name -> set it as deleted, do nothing else though

lookup word, if nameid is deleted, mark it in wordlist as unused and mark for write (?)
*/

/* ********************************************************************** */

/* This simple hash seems to work quite well */
static camel_hash_t hash_key(const char *key)
{
	camel_hash_t hash = 0xABADF00D;

	while (*key) {
		hash = hash * (*key) ^ (*key);
		key++;
	}

	return hash;
}

/* Call with lock held */
static CamelBlock *find_partition(CamelPartitionTable *cpi, camel_hash_t id, int *indexp)
{
	int index, jump;
	CamelBlock *bl;
	CamelPartitionMapBlock *ptb;
	CamelPartitionMap *part;

	/* first, find the block this key might be in, then binary search the block */
	bl = (CamelBlock *)cpi->partition.head;
	while (bl->next) {
		ptb = (CamelPartitionMapBlock *)&bl->data;
		part = ptb->partition;
		if (ptb->used > 0 && id <= part[ptb->used-1].hashid) {
			index = ptb->used/2;
			jump = ptb->used/4;

			if (jump == 0)
				jump = 1;

			while (1) {
				if (id <= part[index].hashid) {
					if (index == 0 || id > part[index-1].hashid)
						break;
					index -= jump;
				} else {
					if (index >= ptb->used-1)
						break;
					index += jump;
				}
				jump = jump/2;
				if (jump == 0)
					jump = 1;
			}
			*indexp = index;

			return bl;
		}
		bl = bl->next;
	}

	g_warning("could not find a partition that could fit !  partition table corrupt!");

	/* This should never be reached */

	return NULL;
}

CamelPartitionTable *camel_partition_table_new(struct _CamelBlockFile *bs, camel_block_t root)
{
	CamelPartitionTable *cpi;
	CamelPartitionMapBlock *ptb;
	CamelPartitionKeyBlock *kb;
	CamelBlock *block, *pblock;

	cpi = (CamelPartitionTable *)camel_object_new(camel_partition_table_get_type());
	cpi->rootid = root;
	cpi->blocks = bs;
	camel_object_ref((CamelObject *)bs);

	/* read the partition table into memory */
	do {
		block = camel_block_file_get_block(bs, root);
		if (block == NULL)
			goto fail;

		ptb = (CamelPartitionMapBlock *)&block->data;

		d(printf("Adding partition block, used = %d, hashid = %08x\n", ptb->used, ptb->partition[0].hashid));

		/* if we have no data, prime initial block */
		if (ptb->used == 0 && e_dlist_empty(&cpi->partition) && ptb->next == 0) {
			pblock = camel_block_file_new_block(bs);
			if (pblock == NULL) {
				camel_block_file_unref_block(bs, block);
				goto fail;
			}
			kb = (CamelPartitionKeyBlock *)&pblock->data;
			kb->used = 0;
			ptb->used = 1;
			ptb->partition[0].hashid = 0xffffffff;
			ptb->partition[0].blockid = pblock->id;
			camel_block_file_touch_block(bs, pblock);
			camel_block_file_unref_block(bs, pblock);
			camel_block_file_touch_block(bs, block);
#ifdef SYNC_UPDATES
			camel_block_file_sync_block(bs, block);
#endif
		}

		root = ptb->next;
		camel_block_file_detach_block(bs, block);
		e_dlist_addtail(&cpi->partition, (EDListNode *)block);
	} while (root);

	return cpi;

fail:
	camel_object_unref((CamelObject *)cpi);
	return NULL;
}

/* sync our blocks, the caller must still sync the blockfile itself */
int
camel_partition_table_sync(CamelPartitionTable *cpi)
{
	CamelBlock *bl, *bn;
	struct _CamelPartitionTablePrivate *p;
	int ret = 0;

	CAMEL_PARTITION_TABLE_LOCK(cpi, lock);

	p = cpi->priv;

	if (cpi->blocks) {
		bl = (CamelBlock *)cpi->partition.head;
		bn = bl->next;
		while (bn) {
			ret = camel_block_file_sync_block(cpi->blocks, bl);
			if (ret == -1)
				goto fail;
			bl = bn;
			bn = bn->next;
		}
	}
fail:
	CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);

	return ret;
}

camel_key_t camel_partition_table_lookup(CamelPartitionTable *cpi, const char *key)
{
	CamelPartitionKeyBlock *pkb;
	CamelPartitionMapBlock *ptb;
	CamelBlock *block, *ptblock;
	camel_hash_t hashid;
	camel_key_t keyid = 0;
	int index, i;

	hashid = hash_key(key);

	CAMEL_PARTITION_TABLE_LOCK(cpi, lock);

	ptblock = find_partition(cpi, hashid, &index);
	if (ptblock == NULL) {
		CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
		return 0;
	}
	ptb = (CamelPartitionMapBlock *)&ptblock->data;
	block = camel_block_file_get_block(cpi->blocks, ptb->partition[index].blockid);
	if (block == NULL) {
		CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
		return 0;
	}

	pkb = (CamelPartitionKeyBlock *)&block->data;

	/* What to do about duplicate hash's? */
	for (i=0;i<pkb->used;i++) {
		if (pkb->keys[i].hashid == hashid) {
			/* !! need to: lookup and compare string value */
			/* get_key() if key == key ... */
			keyid = pkb->keys[i].keyid;
			break;
		}
	}

	CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
	
	camel_block_file_unref_block(cpi->blocks, block);

	return keyid;
}

void camel_partition_table_remove(CamelPartitionTable *cpi, const char *key)
{
	CamelPartitionKeyBlock *pkb;
	CamelPartitionMapBlock *ptb;
	CamelBlock *block, *ptblock;
	camel_hash_t hashid;
	camel_key_t keyid = 0;
	int index, i;

	hashid = hash_key(key);

	CAMEL_PARTITION_TABLE_LOCK(cpi, lock);
	
	ptblock = find_partition(cpi, hashid, &index);
	if (ptblock == NULL) {
		CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
		return;
	}
	ptb = (CamelPartitionMapBlock *)&ptblock->data;
	block = camel_block_file_get_block(cpi->blocks, ptb->partition[index].blockid);
	if (block == NULL) {
		CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
		return;
	}
	pkb = (CamelPartitionKeyBlock *)&block->data;

	/* What to do about duplicate hash's? */
	for (i=0;i<pkb->used;i++) {
		if (pkb->keys[i].hashid == hashid) {
			/* !! need to: lookup and compare string value */
			/* get_key() if key == key ... */
			keyid = pkb->keys[i].keyid;

			/* remove this key */
			pkb->used--;
			for (;i<pkb->used;i++) {
				pkb->keys[i].keyid = pkb->keys[i+1].keyid;
				pkb->keys[i].hashid = pkb->keys[i+1].hashid;
			}
			camel_block_file_touch_block(cpi->blocks, block);
			break;
		}
	}

	CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
	
	camel_block_file_unref_block(cpi->blocks, block);
}

static int
keys_cmp(const void *ap, const void *bp)
{
	const CamelPartitionKey *a = ap;
	const CamelPartitionKey *b = bp;

	if (a->hashid < b->hashid)
		return -1;
	else if (a->hashid > b->hashid)
		return 1;

	return 0;
}

int
camel_partition_table_add(CamelPartitionTable *cpi, const char *key, camel_key_t keyid)
{
	camel_hash_t hashid, partid;
	int index, newindex = 0; /* initialisation of this and pkb/nkb is just to silence compiler */
	CamelPartitionMapBlock *ptb, *ptn;
	CamelPartitionKeyBlock *kb, *newkb, *nkb = NULL, *pkb = NULL;
	CamelBlock *block, *ptblock, *ptnblock;
	int i, half, len;
	struct _CamelPartitionKey keys[CAMEL_BLOCK_SIZE/4];
	int ret = -1;

#define KEY_SIZE (sizeof(kb->keys)/sizeof(kb->keys[0]))

	hashid = hash_key(key);

	CAMEL_PARTITION_TABLE_LOCK(cpi, lock);
	ptblock = find_partition(cpi, hashid, &index);
	if (ptblock == NULL) {
		CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
		return -1;
	}
	ptb = (CamelPartitionMapBlock *)&ptblock->data;
	block = camel_block_file_get_block(cpi->blocks, ptb->partition[index].blockid);
	if (block == NULL) {
		CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);
		return -1;
	}
	kb = (CamelPartitionKeyBlock *)&block->data;

	/* TODO: Keep the key array in sorted order, cheaper lookups and split operation */

	if (kb->used < sizeof(kb->keys)/sizeof(kb->keys[0])) {
		/* Have room, just put it in */
		kb->keys[kb->used].hashid = hashid;
		kb->keys[kb->used].keyid = keyid;
		kb->used++;
	} else {
		CamelBlock *newblock = NULL, *nblock = NULL, *pblock = NULL;

		/* Need to split?  See if previous or next has room, then split across that instead */

		/* TODO: Should look at next/previous partition table block as well ... */

		if (index > 0) {
			pblock = camel_block_file_get_block(cpi->blocks, ptb->partition[index-1].blockid);
			if (pblock == NULL)
				goto fail;
			pkb = (CamelPartitionKeyBlock *)&pblock->data;
		}
		if (index < (ptb->used-1)) {
			nblock = camel_block_file_get_block(cpi->blocks, ptb->partition[index+1].blockid);
			if (nblock == NULL) {
				if (pblock)
					camel_block_file_unref_block(cpi->blocks, pblock);
				goto fail;
			}
			nkb = (CamelPartitionKeyBlock *)&nblock->data;
		}

		if (pblock && pkb->used < KEY_SIZE) {
			if (nblock && nkb->used < KEY_SIZE) {
				if (pkb->used < nkb->used) {
					newindex = index+1;
					newblock = nblock;
				} else {
					newindex = index-1;
					newblock = pblock;
				}
			} else {
				newindex = index-1;
				newblock = pblock;
			}
		} else {
			if (nblock && nkb->used < KEY_SIZE) {
				newindex = index+1;
				newblock = nblock;
			}
		}

		/* We had no room, need to split across another block */
		if (newblock == NULL) {
			/* See if we have room in the partition table for this block or need to split that too */
			if (ptb->used >= sizeof(ptb->partition)/sizeof(ptb->partition[0])) {
				/* TODO: Could check next block to see if it'll fit there first */
				ptnblock = camel_block_file_new_block(cpi->blocks);
				if (ptnblock == NULL) {
					if (nblock)
						camel_block_file_unref_block(cpi->blocks, nblock);
					if (pblock)
						camel_block_file_unref_block(cpi->blocks, pblock);
					goto fail;
				}
				camel_block_file_detach_block(cpi->blocks, ptnblock);

				/* split block and link on-disk, always sorted */
				ptn = (CamelPartitionMapBlock *)&ptnblock->data;
				ptn->next = ptb->next;
				ptb->next = ptnblock->id;
				len = ptb->used / 2;
				ptn->used = ptb->used - len;
				ptb->used = len;
				memcpy(ptn->partition, &ptb->partition[len], ptn->used * sizeof(ptb->partition[0]));

				/* link in-memory */
				ptnblock->next = ptblock->next;
				ptblock->next->prev = ptnblock;
				ptblock->next = ptnblock;
				ptnblock->prev = ptblock;

				/* write in right order to ensure structure */
				camel_block_file_touch_block(cpi->blocks, ptnblock);
#ifdef SYNC_UPDATES
				camel_block_file_sync_block(cpi->blocks, ptnblock);
#endif
				if (index > len) {
					camel_block_file_touch_block(cpi->blocks, ptblock);
#ifdef SYNC_UPDATES
					camel_block_file_sync_block(cpi->blocks, ptblock);
#endif
					index -= len;
					ptb = ptn;
					ptblock = ptnblock;
				}
			}

			/* try get newblock before modifying existing */
			newblock = camel_block_file_new_block(cpi->blocks);
			if (newblock == NULL) {
				if (nblock)
					camel_block_file_unref_block(cpi->blocks, nblock);
				if (pblock)
					camel_block_file_unref_block(cpi->blocks, pblock);
				goto fail;
			}

			for (i=ptb->used-1;i>index;i--) {
				ptb->partition[i+1].hashid = ptb->partition[i].hashid;
				ptb->partition[i+1].blockid = ptb->partition[i].blockid;
			}
			ptb->used++;

			newkb = (CamelPartitionKeyBlock *)&newblock->data;
			newkb->used = 0;
			newindex = index+1;

			ptb->partition[newindex].hashid = ptb->partition[index].hashid;
			ptb->partition[newindex].blockid = newblock->id;

			if (nblock)
				camel_block_file_unref_block(cpi->blocks, nblock);
			if (pblock)
				camel_block_file_unref_block(cpi->blocks, pblock);
		} else {
			newkb = (CamelPartitionKeyBlock *)&newblock->data;

			if (newblock == pblock) {
				if (nblock)
					camel_block_file_unref_block(cpi->blocks, nblock);
			} else {
				if (pblock)
					camel_block_file_unref_block(cpi->blocks, pblock);
			}
		}

		/* sort keys to find midpoint */
		len = kb->used;
		memcpy(keys, kb->keys, sizeof(kb->keys[0])*len);
		memcpy(keys+len, newkb->keys, sizeof(newkb->keys[0])*newkb->used);
		len += newkb->used;
		keys[len].hashid = hashid;
		keys[len].keyid = keyid;
		len++;
		qsort(keys, len, sizeof(keys[0]), keys_cmp);

		/* Split keys, fix partition table */
		half = len/2;
		partid = keys[half-1].hashid;

		if (index < newindex) {
			memcpy(kb->keys, keys, sizeof(keys[0])*half);
			kb->used = half;
			memcpy(newkb->keys, keys+half, sizeof(keys[0])*(len-half));
			newkb->used = len-half;
			ptb->partition[index].hashid = partid;
		} else {
			memcpy(newkb->keys, keys, sizeof(keys[0])*half);
			newkb->used = half;
			memcpy(kb->keys, keys+half, sizeof(keys[0])*(len-half));
			kb->used = len-half;
			ptb->partition[newindex].hashid = partid;
		}

		camel_block_file_touch_block(cpi->blocks, ptblock);
#ifdef SYNC_UPDATES
		camel_block_file_sync_block(cpi->blocks, ptblock);
#endif
		camel_block_file_touch_block(cpi->blocks, newblock);
		camel_block_file_unref_block(cpi->blocks, newblock);
	}

	camel_block_file_touch_block(cpi->blocks, block);
	camel_block_file_unref_block(cpi->blocks, block);

	ret = 0;
fail:
	CAMEL_PARTITION_TABLE_UNLOCK(cpi, lock);

	return ret;
}

/* ********************************************************************** */


struct _CamelKeyTablePrivate {
	pthread_mutex_t lock;	/* for locking key */
};

#define CAMEL_KEY_TABLE_LOCK(kf, lock) (pthread_mutex_lock(&(kf)->priv->lock))
#define CAMEL_KEY_TABLE_UNLOCK(kf, lock) (pthread_mutex_unlock(&(kf)->priv->lock))


static void
camel_key_table_class_init(CamelKeyTableClass *klass)
{
}

static void
camel_key_table_init(CamelKeyTable *ki)
{
	struct _CamelKeyTablePrivate *p;

	p = ki->priv = g_malloc0(sizeof(*ki->priv));
	pthread_mutex_init(&p->lock, NULL);
}

static void
camel_key_table_finalise(CamelKeyTable *ki)
{
	struct _CamelKeyTablePrivate *p;

	p = ki->priv;

	if (ki->blocks) {
		if (ki->root_block) {
			camel_block_file_sync_block(ki->blocks, ki->root_block);
			camel_block_file_unref_block(ki->blocks, ki->root_block);
		}
		camel_block_file_sync(ki->blocks);
		camel_object_unref((CamelObject *)ki->blocks);
	}
	
	pthread_mutex_destroy(&p->lock);
	
	g_free(p);

}

CamelType
camel_key_table_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_object_get_type(), "CamelKeyTable",
					   sizeof (CamelKeyTable),
					   sizeof (CamelKeyTableClass),
					   (CamelObjectClassInitFunc) camel_key_table_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_key_table_init,
					   (CamelObjectFinalizeFunc) camel_key_table_finalise);
	}
	
	return type;
}


CamelKeyTable *
camel_key_table_new(CamelBlockFile *bs, camel_block_t root)
{
	CamelKeyTable *ki;

	ki = (CamelKeyTable *)camel_object_new(camel_key_table_get_type());

	ki->blocks = bs;
	camel_object_ref((CamelObject *)bs);
	ki->rootid = root;

	ki->root_block = camel_block_file_get_block(bs, ki->rootid);
	if (ki->root_block == NULL) {
		camel_object_unref((CamelObject *)ki);
		ki = NULL;
	} else {
		camel_block_file_detach_block(bs, ki->root_block);
		ki->root = (CamelKeyRootBlock *)&ki->root_block->data;

		k(printf("Opening key index\n"));
		k(printf(" first %u\n last %u\n free %u\n", ki->root->first, ki->root->last, ki->root->free));
	}

	return ki;
}

int
camel_key_table_sync(CamelKeyTable *ki)
{
#ifdef SYNC_UPDATES
	return 0;
#else
	return camel_block_file_sync_block(ki->blocks, ki->root_block);
#endif
}

camel_key_t
camel_key_table_add(CamelKeyTable *ki, const char *key, camel_block_t data, unsigned int flags)
{
	CamelBlock *last, *next;
	CamelKeyBlock *kblast, *kbnext;
	int len, left;
	unsigned int offset;
	camel_key_t keyid = 0;

	/* Maximum key size = 128 chars */
	len = strlen(key);
	if (len > CAMEL_KEY_TABLE_MAX_KEY)
		len = 128;

	CAMEL_KEY_TABLE_LOCK(ki, lock);

	if (ki->root->last == 0) {
		last = camel_block_file_new_block(ki->blocks);
		if (last == NULL)
			goto fail;
		ki->root->last = ki->root->first = last->id;
		camel_block_file_touch_block(ki->blocks, ki->root_block);
		k(printf("adding first block, first = %u\n", ki->root->first));
	} else {
		last = camel_block_file_get_block(ki->blocks, ki->root->last);
		if (last == NULL)
			goto fail;
	}

	kblast = (CamelKeyBlock *)&last->data;

	if (kblast->used >= 127)
		goto fail;

	if (kblast->used > 0) {
		/*left = &kblast->u.keydata[kblast->u.keys[kblast->used-1].offset] - (char *)(&kblast->u.keys[kblast->used+1]);*/
		left = kblast->u.keys[kblast->used-1].offset - sizeof(kblast->u.keys[0])*(kblast->used+1);
		d(printf("key '%s' used = %d (%d), filled = %d, left = %d  len = %d?\n",
			 key, kblast->used, kblast->used * sizeof(kblast->u.keys[0]),
			 sizeof(kblast->u.keydata) - kblast->u.keys[kblast->used-1].offset,
			 left, len));
		if (left < len) {
			next = camel_block_file_new_block(ki->blocks);
			if (next == NULL) {
				camel_block_file_unref_block(ki->blocks, last);
				goto fail;
			}
			kbnext = (CamelKeyBlock *)&next->data;
			kblast->next = next->id;
			ki->root->last = next->id;
			d(printf("adding new block, first = %u, last = %u\n", ki->root->first, ki->root->last));
			camel_block_file_touch_block(ki->blocks, ki->root_block);
			camel_block_file_touch_block(ki->blocks, last);
			camel_block_file_unref_block(ki->blocks, last);
			kblast = kbnext;
			last = next;
		}
	}

	if (kblast->used > 0)
		offset = kblast->u.keys[kblast->used-1].offset - len;
	else
		offset = sizeof(kblast->u.keydata)-len;

	kblast->u.keys[kblast->used].flags = flags;
	kblast->u.keys[kblast->used].data = data;
	kblast->u.keys[kblast->used].offset = offset;
	memcpy(kblast->u.keydata + offset, key, len);

	keyid = (last->id & (~(CAMEL_BLOCK_SIZE-1))) | kblast->used;

	kblast->used++;

	g_assert(kblast->used < 127);

	camel_block_file_touch_block(ki->blocks, last);
	camel_block_file_unref_block(ki->blocks, last);

#ifdef SYNC_UPDATES
	camel_block_file_sync_block(ki->blocks, ki->root_block);
#endif
fail:
	CAMEL_KEY_TABLE_UNLOCK(ki, lock);

	return keyid;
}

void
camel_key_table_set_data(CamelKeyTable *ki, camel_key_t keyid, camel_block_t data)
{
	CamelBlock *bl;
	camel_block_t blockid;
	int index;
	CamelKeyBlock *kb;

	if (keyid == 0)
		return;

	blockid =  keyid & (~(CAMEL_BLOCK_SIZE-1));
	index = keyid & (CAMEL_BLOCK_SIZE-1);

	bl = camel_block_file_get_block(ki->blocks, blockid);
	if (bl == NULL)
		return;
	kb = (CamelKeyBlock *)&bl->data;

	CAMEL_KEY_TABLE_LOCK(ki, lock);

	if (kb->u.keys[index].data != data) {
		kb->u.keys[index].data = data;
		camel_block_file_touch_block(ki->blocks, bl);
	}

	CAMEL_KEY_TABLE_UNLOCK(ki, lock);

	camel_block_file_unref_block(ki->blocks, bl);
}

void
camel_key_table_set_flags(CamelKeyTable *ki, camel_key_t keyid, unsigned int flags, unsigned int set)
{
	CamelBlock *bl;
	camel_block_t blockid;
	int index;
	CamelKeyBlock *kb;
	unsigned int old;

	if (keyid == 0)
		return;

	blockid =  keyid & (~(CAMEL_BLOCK_SIZE-1));
	index = keyid & (CAMEL_BLOCK_SIZE-1);

	bl = camel_block_file_get_block(ki->blocks, blockid);
	if (bl == NULL)
		return;
	kb = (CamelKeyBlock *)&bl->data;

	g_assert(kb->used < 127);
	g_assert(index < kb->used);

	CAMEL_KEY_TABLE_LOCK(ki, lock);

	old = kb->u.keys[index].flags;
	if ((old & set) != (flags & set)) {
		kb->u.keys[index].flags = (old & (~set)) | (flags & set);
		camel_block_file_touch_block(ki->blocks, bl);
	}

	CAMEL_KEY_TABLE_UNLOCK(ki, lock);

	camel_block_file_unref_block(ki->blocks, bl);
}

camel_block_t
camel_key_table_lookup(CamelKeyTable *ki, camel_key_t keyid, char **keyp, unsigned int *flags)
{
	CamelBlock *bl;
	camel_block_t blockid;
	int index, len, off;
	char *key;
	CamelKeyBlock *kb;

	if (keyp)
		*keyp = 0;
	if (flags)
		*flags = 0;
	if (keyid == 0)
		return 0;

	blockid =  keyid & (~(CAMEL_BLOCK_SIZE-1));
	index = keyid & (CAMEL_BLOCK_SIZE-1);

	bl = camel_block_file_get_block(ki->blocks, blockid);
	if (bl == NULL)
		return 0;

	kb = (CamelKeyBlock *)&bl->data;

#if 1
	g_assert(kb->used < 127); /* this should be more accurate */
	g_assert(index < kb->used);
#else
	if (kb->used >=127 || index >= kb->used) {
		g_warning("Block %x: Invalid index or content: index %d used %d\n", blockid, index, kb->used);
		return 0;
	}
#endif

	CAMEL_KEY_TABLE_LOCK(ki, lock);

	blockid = kb->u.keys[index].data;
	if (flags)
		*flags = kb->u.keys[index].flags;

	if (keyp) {
		off = kb->u.keys[index].offset;
		if (index == 0)
			len = sizeof(kb->u.keydata) - off;
		else
			len = kb->u.keys[index-1].offset - off;
		*keyp = key = g_malloc(len+1);
		memcpy(key, kb->u.keydata + off, len);
		key[len] = 0;
	}

	CAMEL_KEY_TABLE_UNLOCK(ki, lock);

	camel_block_file_unref_block(ki->blocks, bl);

	return blockid;
}

/* iterate through all keys */
camel_key_t
camel_key_table_next(CamelKeyTable *ki, camel_key_t next, char **keyp, unsigned int *flagsp, camel_block_t *datap)
{
	CamelBlock *bl;
	CamelKeyBlock *kb;
	camel_block_t blockid;
	int index;

	if (keyp)
		*keyp = 0;
	if (flagsp)
		*flagsp = 0;
	if (datap)
		*datap = 0;

	CAMEL_KEY_TABLE_LOCK(ki, lock);

	if (next == 0) {
		next = ki->root->first;
		if (next == 0) {
			CAMEL_KEY_TABLE_UNLOCK(ki, lock);
			return 0;
		}
	} else
		next++;

	do {
		blockid =  next & (~(CAMEL_BLOCK_SIZE-1));
		index = next & (CAMEL_BLOCK_SIZE-1);
		
		bl = camel_block_file_get_block(ki->blocks, blockid);
		if (bl == NULL) {
			CAMEL_KEY_TABLE_UNLOCK(ki, lock);
			return 0;
		}

		kb = (CamelKeyBlock *)&bl->data;

		/* see if we need to goto the next block */
		if (index >= kb->used) {
			/* FIXME: check for loops */
			next = kb->next;
			camel_block_file_unref_block(ki->blocks, bl);
			bl = NULL;
		}
	} while (bl == NULL);

	/* invalid block data */
	if ((kb->u.keys[index].offset >= sizeof(kb->u.keydata)
	     /*|| kb->u.keys[index].offset < kb->u.keydata - (char *)&kb->u.keys[kb->used])*/
	     || kb->u.keys[index].offset < sizeof(kb->u.keys[0]) * kb->used
	    || (index > 0 &&
		(kb->u.keys[index-1].offset >= sizeof(kb->u.keydata)
		 /*|| kb->u.keys[index-1].offset < kb->u.keydata - (char *)&kb->u.keys[kb->used]))) {*/
		 || kb->u.keys[index-1].offset < sizeof(kb->u.keys[0]) * kb->used)))) {
		g_warning("Block %u invalid scanning keys", bl->id);
		camel_block_file_unref_block(ki->blocks, bl);
		CAMEL_KEY_TABLE_UNLOCK(ki, lock);
		return 0;
	}

	if (datap)
		*datap = kb->u.keys[index].data;

	if (flagsp)
		*flagsp = kb->u.keys[index].flags;

	if (keyp) {
		int len, off = kb->u.keys[index].offset;
		char *key;

		if (index == 0)
			len = sizeof(kb->u.keydata) - off;
		else
			len = kb->u.keys[index-1].offset - off;
		*keyp = key = g_malloc(len+1);
		memcpy(key, kb->u.keydata + off, len);
		key[len] = 0;
	}

	CAMEL_KEY_TABLE_UNLOCK(ki, lock);

	camel_block_file_unref_block(ki->blocks, bl);

	return next;
}

/* ********************************************************************** */
