/*
 * Copyright (C) 2001 Ximian Inc.
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

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "camel-store-summary.h"

#include "camel-file-utils.h"

#include "hash-table-utils.h"
#include "e-util/md5-utils.h"
#include "e-util/e-memory.h"

#include "camel-private.h"

#define d(x)
#define io(x)			/* io debug */

#define CAMEL_STORE_SUMMARY_VERSION (13)

#define _PRIVATE(o) (((CamelStoreSummary *)(o))->priv)

static int summary_header_load(CamelStoreSummary *, FILE *);
static int summary_header_save(CamelStoreSummary *, FILE *);

static CamelFolderInfo * folder_info_new(CamelStoreSummary *, const char *);
static CamelFolderInfo * folder_info_load(CamelStoreSummary *, FILE *);
static int		 folder_info_save(CamelStoreSummary *, FILE *, CamelFolderInfo *);
static void		 folder_info_free(CamelStoreSummary *, CamelFolderInfo *);

static const char *folder_info_string(CamelStoreSummary *, const CamelFolderInfo *, int);
static void folder_info_set_string(CamelStoreSummary *, CamelFolderInfo *, int, const char *);

static void camel_store_summary_class_init (CamelStoreSummaryClass *klass);
static void camel_store_summary_init       (CamelStoreSummary *obj);
static void camel_store_summary_finalise   (CamelObject *obj);

static CamelObjectClass *camel_store_summary_parent;

static void
camel_store_summary_class_init (CamelStoreSummaryClass *klass)
{
	camel_store_summary_parent = camel_type_get_global_classfuncs (camel_object_get_type ());

	klass->summary_header_load = summary_header_load;
	klass->summary_header_save = summary_header_save;

	klass->folder_info_new  = folder_info_new;
	klass->folder_info_load = folder_info_load;
	klass->folder_info_save = folder_info_save;
	klass->folder_info_free = folder_info_free;

	klass->folder_info_string = folder_info_string;
	klass->folder_info_set_string = folder_info_set_string;
}

static void
camel_store_summary_init (CamelStoreSummary *s)
{
	struct _CamelStoreSummaryPrivate *p;

	p = _PRIVATE(s) = g_malloc0(sizeof(*p));

	s->folder_info_size = sizeof(CamelFolderInfo);

	s->folder_info_chunks = NULL;

	s->version = CAMEL_STORE_SUMMARY_VERSION;
	s->flags = 0;
	s->count = 0;
	s->time = 0;

	s->folders = g_ptr_array_new();
	s->folders_full = g_hash_table_new(g_str_hash, g_str_equal);

#ifdef ENABLE_THREADS
	p->summary_lock = g_mutex_new();
	p->io_lock = g_mutex_new();
	p->alloc_lock = g_mutex_new();
	p->ref_lock = g_mutex_new();
#endif
}

static void
camel_store_summary_finalise (CamelObject *obj)
{
	struct _CamelStoreSummaryPrivate *p;
	CamelStoreSummary *s = (CamelStoreSummary *)obj;

	p = _PRIVATE(obj);

	camel_store_summary_clear(s);
	g_ptr_array_free(s->folders, TRUE);
	g_hash_table_destroy(s->folders_full);

	g_free(s->summary_path);

	if (s->folder_info_chunks)
		e_memchunk_destroy(s->folder_info_chunks);

#ifdef ENABLE_THREADS
	g_mutex_free(p->summary_lock);
	g_mutex_free(p->io_lock);
	g_mutex_free(p->alloc_lock);
	g_mutex_free(p->ref_lock);
#endif

	g_free(p);
}

CamelType
camel_store_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (), "CamelStoreSummary",
					    sizeof (CamelStoreSummary),
					    sizeof (CamelStoreSummaryClass),
					    (CamelObjectClassInitFunc) camel_store_summary_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_store_summary_init,
					    (CamelObjectFinalizeFunc) camel_store_summary_finalise);
	}
	
	return type;
}

/**
 * camel_store_summary_new:
 *
 * Create a new CamelStoreSummary object.
 * 
 * Return value: A new CamelStoreSummary widget.
 **/
CamelStoreSummary *
camel_store_summary_new (void)
{
	CamelStoreSummary *new = CAMEL_STORE_SUMMARY ( camel_object_new (camel_store_summary_get_type ()));	return new;
}

/**
 * camel_store_summary_set_filename:
 * @s: 
 * @name: 
 * 
 * Set the filename where the summary will be loaded to/saved from.
 **/
void camel_store_summary_set_filename(CamelStoreSummary *s, const char *name)
{
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	g_free(s->summary_path);
	s->summary_path = g_strdup(name);

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
}

void camel_store_summary_set_uri_prefix(CamelStoreSummary *s, const char *prefix)
{
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	g_free(s->uri_prefix);
	s->uri_prefix = g_strdup(prefix);

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
}

/**
 * camel_store_summary_count:
 * @s: 
 * 
 * Get the number of summary items stored in this summary.
 * 
 * Return value: The number of items int he summary.
 **/
int
camel_store_summary_count(CamelStoreSummary *s)
{
	return s->folders->len;
}

/**
 * camel_store_summary_index:
 * @s: 
 * @i: 
 * 
 * Retrieve a summary item by index number.
 *
 * A referenced to the summary item is returned, which may be
 * ref'd or free'd as appropriate.
 * 
 * Return value: The summary item, or NULL if the index @i is out
 * of range.
 * It must be freed using camel_store_summary_info_free().
 **/
CamelFolderInfo *
camel_store_summary_index(CamelStoreSummary *s, int i)
{
	CamelFolderInfo *info = NULL;

	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	if (i<s->folders->len)
		info = g_ptr_array_index(s->folders, i);

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);

	if (info)
		info->refcount++;

	CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);

	return info;
}

/**
 * camel_store_summary_index:
 * @s: 
 * @i: 
 * 
 * Obtain a copy of the summary array.  This is done atomically,
 * so cannot contain empty entries.
 *
 * It must be freed using camel_store_summary_array_free().
 **/
GPtrArray *
camel_store_summary_array(CamelStoreSummary *s)
{
	CamelFolderInfo *info;
	GPtrArray *res = g_ptr_array_new();
	int i;
	
	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	g_ptr_array_set_size(res, s->folders->len);
	for (i=0;i<s->folders->len;i++) {
		info = res->pdata[i] = g_ptr_array_index(s->folders, i);
		info->refcount++;
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
	CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);

	return res;
}

/**
 * camel_store_summary_array_free:
 * @s: 
 * @array: 
 * 
 * Free the folder summary array.
 **/
void
camel_store_summary_array_free(CamelStoreSummary *s, GPtrArray *array)
{
	int i;

	for (i=0;i<array->len;i++)
		camel_store_summary_info_free(s, array->pdata[i]);

	g_ptr_array_free(array, TRUE);
}

/**
 * camel_store_summary_full:
 * @s: 
 * @full: 
 * 
 * Retrieve a summary item by full name.
 *
 * A referenced to the summary item is returned, which may be
 * ref'd or free'd as appropriate.
 * 
 * Return value: The summary item, or NULL if the @full name
 * is not available.
 * It must be freed using camel_store_summary_info_free().
 **/
CamelFolderInfo *
camel_store_summary_full(CamelStoreSummary *s, const char *full)
{
	CamelFolderInfo *info;

	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	info = g_hash_table_lookup(s->folders_full, full);

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);

	if (info)
		info->refcount++;

	CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);

	return info;
}

int
camel_store_summary_load(CamelStoreSummary *s)
{
	FILE *in;
	int i;
	CamelFolderInfo *mi;

	g_assert(s->summary_path);

	in = fopen(s->summary_path, "r");
	if (in == NULL)
		return -1;

	CAMEL_STORE_SUMMARY_LOCK(s, io_lock);
	if ( ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->summary_header_load(s, in) == -1)
		goto error;

	/* now read in each message ... */
	for (i=0;i<s->count;i++) {
		mi = ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->folder_info_load(s, in);

		if (mi == NULL)
			goto error;

		camel_store_summary_add(s, mi);
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
	
	if (fclose(in) == -1)
		return -1;

	s->flags &= ~CAMEL_STORE_SUMMARY_DIRTY;

	return 0;

error:
	g_warning("Cannot load summary file: %s", strerror(ferror(in)));
	CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
	fclose(in);
	s->flags |= ~CAMEL_STORE_SUMMARY_DIRTY;

	return -1;
}

/**
 * camel_store_summary_save:
 * @s: 
 * 
 * Writes the summary to disk.  The summary is only written if changes
 * have occured.
 * 
 * Return value: Returns -1 on error.
 **/
int
camel_store_summary_save(CamelStoreSummary *s)
{
	FILE *out;
	int fd;
	int i;
	guint32 count;
	CamelFolderInfo *mi;

	g_assert(s->summary_path);

	if ((s->flags & CAMEL_STORE_SUMMARY_DIRTY) == 0)
		return 0;

	fd = open(s->summary_path, O_RDWR|O_CREAT, 0600);
	if (fd == -1)
		return -1;
	out = fdopen(fd, "w");
	if ( out == NULL ) {
		close(fd);
		return -1;
	}

	io(printf("saving header\n"));

	CAMEL_STORE_SUMMARY_LOCK(s, io_lock);

	if ( ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->summary_header_save(s, out) == -1) {
		fclose(out);
		CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
		return -1;
	}

	/* now write out each message ... */

	/* FIXME: Locking? */

	count = s->folders->len;
	for (i=0;i<count;i++) {
		mi = s->folders->pdata[i];
		((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->folder_info_save(s, out, mi);
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);

	if (fclose(out) == -1)
		return -1;

	s->flags &= ~CAMEL_STORE_SUMMARY_DIRTY;
	return 0;
}

/**
 * camel_store_summary_header_load:
 * @s: Summary object.
 * 
 * Only load the header information from the summary,
 * keep the rest on disk.  This should only be done on
 * a fresh summary object.
 * 
 * Return value: -1 on error.
 **/
int camel_store_summary_header_load(CamelStoreSummary *s)
{
	FILE *in;
	int ret;

	g_assert(s->summary_path);

	in = fopen(s->summary_path, "r");
	if (in == NULL)
		return -1;

	CAMEL_STORE_SUMMARY_LOCK(s, io_lock);
	ret = ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->summary_header_load(s, in);
	CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
	
	fclose(in);
	s->flags &= ~CAMEL_STORE_SUMMARY_DIRTY;
	return ret;
}

/**
 * camel_store_summary_add:
 * @s: 
 * @info: 
 * 
 * Adds a new @info record to the summary.  If @info->uid is NULL, then a new
 * uid is automatically re-assigned by calling :next_uid_string().
 *
 * The @info record should have been generated by calling one of the
 * info_new_*() functions, as it will be free'd based on the summary
 * class.  And MUST NOT be allocated directly using malloc.
 **/
void camel_store_summary_add(CamelStoreSummary *s, CamelFolderInfo *info)
{
	if (info == NULL)
		return;

	if (camel_folder_info_full(s, info) == NULL) {
		g_warning("Trying to add a folder info with missing required full name\n");
		return;
	}

	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	g_ptr_array_add(s->folders, info);
	g_hash_table_insert(s->folders_full, (char *)camel_folder_info_full(s, info), info);
	s->flags |= CAMEL_STORE_SUMMARY_DIRTY;

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
}

/**
 * camel_store_summary_add_from_full:
 * @s: 
 * @h: 
 * 
 * Build a new info record based on the name, and add it to the summary.
 *
 * Return value: The newly added record.
 **/
CamelFolderInfo *camel_store_summary_add_from_full(CamelStoreSummary *s, const char *full)
{
	CamelFolderInfo *info;

	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	info = g_hash_table_lookup(s->folders_full, full);
	if (info != NULL) {
		g_warning("Trying to add folder '%s' to summary that already has it", full);
		info = NULL;
	} else {
		info = camel_store_summary_info_new_from_full(s, full);
		g_ptr_array_add(s->folders, info);
		g_hash_table_insert(s->folders_full, (char *)camel_folder_info_full(s, info), info);
		s->flags |= CAMEL_STORE_SUMMARY_DIRTY;
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);

	return info;
}

/**
 * camel_store_summary_info_new_from_full:
 * @s: 
 * @h: 
 * 
 * Create a new info record from a name.
 * 
 * Return value: Guess?  This info record MUST be freed using
 * camel_store_summary_info_free(), camel_folder_info_free() will not work.
 **/
CamelFolderInfo *camel_store_summary_info_new_from_full(CamelStoreSummary *s, const char *f)
{
	return ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s))) -> folder_info_new(s, f);
}

/**
 * camel_store_summary_info_free:
 * @s: 
 * @mi: 
 * 
 * Unref and potentially free the message info @mi, and all associated memory.
 **/
void camel_store_summary_info_free(CamelStoreSummary *s, CamelFolderInfo *mi)
{
	g_assert(mi);
	g_assert(s);

	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);

	g_assert(mi->refcount >= 1);

	mi->refcount--;
	if (mi->refcount > 0) {
		CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);
		return;
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);

	((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->folder_info_free(s, mi);		
}

/**
 * camel_store_summary_info_ref:
 * @s: 
 * @mi: 
 * 
 * Add an extra reference to @mi.
 **/
void camel_store_summary_info_ref(CamelStoreSummary *s, CamelFolderInfo *mi)
{
	g_assert(mi);
	g_assert(s);

	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);
	g_assert(mi->refcount >= 1);
	mi->refcount++;
	CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);
}

/**
 * camel_store_summary_touch:
 * @s: 
 * 
 * Mark the summary as changed, so that a save will save it.
 **/
void
camel_store_summary_touch(CamelStoreSummary *s)
{
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
	s->flags |= CAMEL_STORE_SUMMARY_DIRTY;
	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
}

/**
 * camel_store_summary_clear:
 * @s: 
 * 
 * Empty the summary contents.
 **/
void
camel_store_summary_clear(CamelStoreSummary *s)
{
	int i;

	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
	if (camel_store_summary_count(s) == 0) {
		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
		return;
	}

	for (i=0;i<s->folders->len;i++)
		camel_store_summary_info_free(s, s->folders->pdata[i]);

	g_ptr_array_set_size(s->folders, 0);
	g_hash_table_destroy(s->folders_full);
	s->folders_full = g_hash_table_new(g_str_hash, g_str_equal);
	s->flags |= CAMEL_STORE_SUMMARY_DIRTY;
	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
}

/**
 * camel_store_summary_remove:
 * @s: 
 * @info: 
 * 
 * Remove a specific @info record from the summary.
 **/
void camel_store_summary_remove(CamelStoreSummary *s, CamelFolderInfo *info)
{
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
	g_hash_table_remove(s->folders_full, camel_folder_info_full(s, info));
	g_ptr_array_remove(s->folders, info);
	s->flags |= CAMEL_STORE_SUMMARY_DIRTY;
	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);

	camel_store_summary_info_free(s, info);
}

/**
 * camel_store_summary_remove_uid:
 * @s: 
 * @full: 
 * 
 * Remove a specific info record from the summary, by @full.
 **/
void camel_store_summary_remove_full(CamelStoreSummary *s, const char *full)
{
        CamelFolderInfo *oldinfo;
        char *oldfull;

	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
        if (g_hash_table_lookup_extended(s->folders_full, full, (void *)&oldfull, (void *)&oldinfo)) {
		/* make sure it doesn't vanish while we're removing it */
		oldinfo->refcount++;
		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
		CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);
		camel_store_summary_remove(s, oldinfo);
		camel_store_summary_info_free(s, oldinfo);
        } else {
		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
		CAMEL_STORE_SUMMARY_UNLOCK(s, ref_lock);
	}
}

/**
 * camel_store_summary_remove_index:
 * @s: 
 * @index: 
 * 
 * Remove a specific info record from the summary, by index.
 **/
void camel_store_summary_remove_index(CamelStoreSummary *s, int index)
{
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
	if (index < s->folders->len) {
		CamelFolderInfo *info = s->folders->pdata[index];

		g_hash_table_remove(s->folders_full, camel_folder_info_full(s, info));
		g_ptr_array_remove_index(s->folders, index);
		s->flags |= CAMEL_STORE_SUMMARY_DIRTY;

		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
		camel_store_summary_info_free(s, info);
	} else {
		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
	}
}

static int
summary_header_load(CamelStoreSummary *s, FILE *in)
{
	gint32 version, flags, count;
	time_t time;

	fseek(in, 0, SEEK_SET);

	io(printf("Loading header\n"));

	if (camel_file_util_decode_fixed_int32(in, &version) == -1
	    || camel_file_util_decode_fixed_int32(in, &flags) == -1
	    || camel_file_util_decode_time_t(in, &time) == -1
	    || camel_file_util_decode_fixed_int32(in, &count) == -1) {
		return -1;
	}

	s->flags = flags;
	s->time = time;
	s->count = count;
	if (s->version != version) {
		g_warning("Summary header version mismatch");
		return -1;
	}
	return 0;
}

static int
summary_header_save(CamelStoreSummary *s, FILE *out)
{
	fseek(out, 0, SEEK_SET);

	io(printf("Savining header\n"));

	camel_file_util_encode_fixed_int32(out, s->version);
	camel_file_util_encode_fixed_int32(out, s->flags);
	camel_file_util_encode_time_t(out, s->time);
	return camel_file_util_encode_fixed_int32(out, camel_store_summary_count(s));
}

/**
 * camel_store_summary_info_new:
 * @s: 
 * 
 * Allocate a new camel message info, suitable for adding
 * to this summary.
 * 
 * Return value: 
 **/
CamelFolderInfo *
camel_store_summary_info_new(CamelStoreSummary *s)
{
	CamelFolderInfo *mi;

	CAMEL_STORE_SUMMARY_LOCK(s, alloc_lock);
	if (s->folder_info_chunks == NULL)
		s->folder_info_chunks = e_memchunk_new(32, s->folder_info_size);
	mi = e_memchunk_alloc0(s->folder_info_chunks);
	CAMEL_STORE_SUMMARY_UNLOCK(s, alloc_lock);
	mi->refcount = 1;
	return mi;
}

const char *camel_folder_info_string(CamelStoreSummary *s, const CamelFolderInfo *mi, int type)
{
	return ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->folder_info_string(s, mi, type);
}

void camel_folder_info_set_string(CamelStoreSummary *s, CamelFolderInfo *mi, int type, const char *value)
{
	return ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->folder_info_set_string(s, mi, type, value);
}

static CamelFolderInfo *
folder_info_new(CamelStoreSummary *s, const char *f)
{
	CamelFolderInfo *mi;

	mi = camel_store_summary_info_new(s);

	mi->full = g_strdup(f);
	mi->unread = CAMEL_STORE_SUMMARY_UNKNOWN;
	mi->total = CAMEL_STORE_SUMMARY_UNKNOWN;

	return mi;
}

static CamelFolderInfo *
folder_info_load(CamelStoreSummary *s, FILE *in)
{
	CamelFolderInfo *mi;

	mi = camel_store_summary_info_new(s);

	io(printf("Loading folder info\n"));

	camel_file_util_decode_string(in, &mi->full);
	camel_file_util_decode_uint32(in, &mi->flags);
	camel_file_util_decode_uint32(in, &mi->unread);
	camel_file_util_decode_uint32(in, &mi->total);

	if (!ferror(in))
		return mi;

	camel_store_summary_info_free(s, mi);

	return NULL;
}

static int
folder_info_save(CamelStoreSummary *s, FILE *out, CamelFolderInfo *mi)
{
	io(printf("Saving folder info\n"));

	camel_file_util_encode_string(out, camel_folder_info_full(s, mi));
	camel_file_util_encode_uint32(out, mi->flags);
	camel_file_util_encode_uint32(out, mi->unread);
	camel_file_util_encode_uint32(out, mi->total);

	return ferror(out);
}

static void
folder_info_free(CamelStoreSummary *s, CamelFolderInfo *mi)
{
	g_free(mi->full);
	g_free(mi->uri);
	e_memchunk_free(s->folder_info_chunks, mi);
}

static const char *
folder_info_string(CamelStoreSummary *s, const CamelFolderInfo *mi, int type)
{
	const char *p;

	/* FIXME: Locks? */

	g_assert (mi != NULL);

	switch (type) {
	case CAMEL_STORE_SUMMARY_FULL:
		return mi->full;
	case CAMEL_STORE_SUMMARY_NAME:
		p = strrchr(mi->full, '/');
		if (p)
			return p;
		else
			return mi->full;
	case CAMEL_STORE_SUMMARY_URI:
		if (mi->uri)
			return mi->uri;
		if (s->uri_prefix)
			return (((CamelFolderInfo *)mi)->uri = g_strdup_printf("%s%s", s->uri_prefix, mi->full));
	}

	return "";
}

static void
folder_info_set_string (CamelStoreSummary *s, CamelFolderInfo *mi, int type, const char *str)
{
	const char *p;
	char *v;
	int len;

	g_assert (mi != NULL);

	switch(type) {
	case CAMEL_STORE_SUMMARY_FULL:
		g_free(mi->full);
		g_free(mi->uri);
		mi->full = g_strdup(str);
		break;
	case CAMEL_STORE_SUMMARY_NAME:
		p = strrchr(mi->full, '/');
		if (p) {
			len = p-mi->full+1;
			v = g_malloc(len+strlen(str)+1);
			memcpy(v, mi->full, len);
			strcpy(v+len, str);
		} else {
			v = g_strdup(str);
		}
		g_free(mi->full);
		mi->full = v;
		break;
	case CAMEL_STORE_SUMMARY_URI:
		if (s->uri_prefix) {
			len = strlen(s->uri_prefix);
			if (len > strlen(str)
			    || strncmp(s->uri_prefix, str, len) != 0) {
				g_warning("Trying to set folderinfo uri '%s' for summary with prefix '%s'",
					  str, s->uri_prefix);
				return;
			}
			g_free(mi->full);
			g_free(mi->uri);
			mi->full = g_strdup(str + len);
			mi->uri = g_strdup(str);
		} else {
			g_warning("Trying to set folderinfo uri '%s' for summary with no uri prefix", str);
		}
		break;
	}
}
