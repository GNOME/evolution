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
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-store-summary.h"

#include "camel-file-utils.h"

#include "libedataserver/md5-utils.h"
#include "libedataserver/e-memory.h"

#include "camel-private.h"
#include "camel-url.h"

#define d(x)
#define io(x)			/* io debug */

/* possible versions, for versioning changes */
#define CAMEL_STORE_SUMMARY_VERSION_0 (1)
#define CAMEL_STORE_SUMMARY_VERSION_2 (2)

/* current version */
#define CAMEL_STORE_SUMMARY_VERSION (2)

#define _PRIVATE(o) (((CamelStoreSummary *)(o))->priv)

static int summary_header_load(CamelStoreSummary *, FILE *);
static int summary_header_save(CamelStoreSummary *, FILE *);

static CamelStoreInfo * store_info_new(CamelStoreSummary *, const char *);
static CamelStoreInfo * store_info_load(CamelStoreSummary *, FILE *);
static int		 store_info_save(CamelStoreSummary *, FILE *, CamelStoreInfo *);
static void		 store_info_free(CamelStoreSummary *, CamelStoreInfo *);

static const char *store_info_string(CamelStoreSummary *, const CamelStoreInfo *, int);
static void store_info_set_string(CamelStoreSummary *, CamelStoreInfo *, int, const char *);

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

	klass->store_info_new  = store_info_new;
	klass->store_info_load = store_info_load;
	klass->store_info_save = store_info_save;
	klass->store_info_free = store_info_free;

	klass->store_info_string = store_info_string;
	klass->store_info_set_string = store_info_set_string;
}

static void
camel_store_summary_init (CamelStoreSummary *s)
{
	struct _CamelStoreSummaryPrivate *p;

	p = _PRIVATE(s) = g_malloc0(sizeof(*p));

	s->store_info_size = sizeof(CamelStoreInfo);

	s->store_info_chunks = NULL;

	s->version = CAMEL_STORE_SUMMARY_VERSION;
	s->flags = 0;
	s->count = 0;
	s->time = 0;

	s->folders = g_ptr_array_new();
	s->folders_path = g_hash_table_new(g_str_hash, g_str_equal);
	
	p->summary_lock = g_mutex_new();
	p->io_lock = g_mutex_new();
	p->alloc_lock = g_mutex_new();
	p->ref_lock = g_mutex_new();
}

static void
camel_store_summary_finalise (CamelObject *obj)
{
	struct _CamelStoreSummaryPrivate *p;
	CamelStoreSummary *s = (CamelStoreSummary *)obj;

	p = _PRIVATE(obj);

	camel_store_summary_clear(s);
	g_ptr_array_free(s->folders, TRUE);
	g_hash_table_destroy(s->folders_path);

	g_free(s->summary_path);

	if (s->store_info_chunks)
		e_memchunk_destroy(s->store_info_chunks);
	
	g_mutex_free(p->summary_lock);
	g_mutex_free(p->io_lock);
	g_mutex_free(p->alloc_lock);
	g_mutex_free(p->ref_lock);
	
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

void camel_store_summary_set_uri_base(CamelStoreSummary *s, CamelURL *base)
{
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	if (s->uri_base)
		camel_url_free(s->uri_base);
	s->uri_base = camel_url_new_with_base(base, "");

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
CamelStoreInfo *
camel_store_summary_index(CamelStoreSummary *s, int i)
{
	CamelStoreInfo *info = NULL;

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
	CamelStoreInfo *info;
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
 * camel_store_summary_path:
 * @s: 
 * @path: 
 * 
 * Retrieve a summary item by path name.
 *
 * A referenced to the summary item is returned, which may be
 * ref'd or free'd as appropriate.
 * 
 * Return value: The summary item, or NULL if the @path name
 * is not available.
 * It must be freed using camel_store_summary_info_free().
 **/
CamelStoreInfo *
camel_store_summary_path(CamelStoreSummary *s, const char *path)
{
	CamelStoreInfo *info;

	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	info = g_hash_table_lookup(s->folders_path, path);

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
	CamelStoreInfo *mi;

	g_assert(s->summary_path);

	in = fopen(s->summary_path, "r");
	if (in == NULL)
		return -1;

	CAMEL_STORE_SUMMARY_LOCK(s, io_lock);
	if ( ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->summary_header_load(s, in) == -1)
		goto error;

	/* now read in each message ... */
	for (i=0;i<s->count;i++) {
		mi = ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->store_info_load(s, in);

		if (mi == NULL)
			goto error;

		camel_store_summary_add(s, mi);
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
	
	if (fclose (in) != 0)
		return -1;

	s->flags &= ~CAMEL_STORE_SUMMARY_DIRTY;

	return 0;

error:
	i = ferror (in);
	g_warning ("Cannot load summary file: %s", strerror (ferror (in)));
	CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
	fclose (in);
	s->flags |= ~CAMEL_STORE_SUMMARY_DIRTY;
	errno = i;
	
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
	CamelStoreInfo *mi;

	g_assert(s->summary_path);

	io(printf("** saving summary\n"));

	if ((s->flags & CAMEL_STORE_SUMMARY_DIRTY) == 0) {
		io(printf("**  summary clean no save\n"));
		return 0;
	}

	fd = open(s->summary_path, O_RDWR|O_CREAT|O_TRUNC, 0600);
	if (fd == -1) {
		io(printf("**  open error: %s\n", strerror (errno)));
		return -1;
	}
	out = fdopen(fd, "w");
	if ( out == NULL ) {
		i = errno;
		printf("**  fdopen error: %s\n", strerror (errno));
		close(fd);
		errno = i;
		return -1;
	}

	io(printf("saving header\n"));

	CAMEL_STORE_SUMMARY_LOCK(s, io_lock);

	if ( ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->summary_header_save(s, out) == -1) {
		i = errno;
		fclose(out);
		CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
		errno = i;
		return -1;
	}

	/* now write out each message ... */

	/* FIXME: Locking? */

	count = s->folders->len;
	for (i=0;i<count;i++) {
		mi = s->folders->pdata[i];
		((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->store_info_save(s, out, mi);
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, io_lock);
	
	if (fflush (out) != 0 || fsync (fileno (out)) == -1) {
		i = errno;
		fclose (out);
		errno = i;
		return -1;
	}
	
	if (fclose (out) != 0)
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
void camel_store_summary_add(CamelStoreSummary *s, CamelStoreInfo *info)
{
	if (info == NULL)
		return;

	if (camel_store_info_path(s, info) == NULL) {
		g_warning("Trying to add a folder info with missing required path name\n");
		return;
	}

	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	g_ptr_array_add(s->folders, info);
	g_hash_table_insert(s->folders_path, (char *)camel_store_info_path(s, info), info);
	s->flags |= CAMEL_STORE_SUMMARY_DIRTY;

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
}

/**
 * camel_store_summary_add_from_path:
 * @s: 
 * @h: 
 * 
 * Build a new info record based on the name, and add it to the summary.
 *
 * Return value: The newly added record.
 **/
CamelStoreInfo *camel_store_summary_add_from_path(CamelStoreSummary *s, const char *path)
{
	CamelStoreInfo *info;

	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);

	info = g_hash_table_lookup(s->folders_path, path);
	if (info != NULL) {
		g_warning("Trying to add folder '%s' to summary that already has it", path);
		info = NULL;
	} else {
		info = camel_store_summary_info_new_from_path(s, path);
		g_ptr_array_add(s->folders, info);
		g_hash_table_insert(s->folders_path, (char *)camel_store_info_path(s, info), info);
		s->flags |= CAMEL_STORE_SUMMARY_DIRTY;
	}

	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);

	return info;
}

/**
 * camel_store_summary_info_new_from_path:
 * @s: 
 * @h: 
 * 
 * Create a new info record from a name.
 * 
 * Return value: Guess?  This info record MUST be freed using
 * camel_store_summary_info_free(), camel_store_info_free() will not work.
 **/
CamelStoreInfo *camel_store_summary_info_new_from_path(CamelStoreSummary *s, const char *f)
{
	return ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s))) -> store_info_new(s, f);
}

/**
 * camel_store_summary_info_free:
 * @s: 
 * @mi: 
 * 
 * Unref and potentially free the message info @mi, and all associated memory.
 **/
void camel_store_summary_info_free(CamelStoreSummary *s, CamelStoreInfo *mi)
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

	((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->store_info_free(s, mi);		
}

/**
 * camel_store_summary_info_ref:
 * @s: 
 * @mi: 
 * 
 * Add an extra reference to @mi.
 **/
void camel_store_summary_info_ref(CamelStoreSummary *s, CamelStoreInfo *mi)
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
	g_hash_table_destroy(s->folders_path);
	s->folders_path = g_hash_table_new(g_str_hash, g_str_equal);
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
void camel_store_summary_remove(CamelStoreSummary *s, CamelStoreInfo *info)
{
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
	g_hash_table_remove(s->folders_path, camel_store_info_path(s, info));
	g_ptr_array_remove(s->folders, info);
	s->flags |= CAMEL_STORE_SUMMARY_DIRTY;
	CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);

	camel_store_summary_info_free(s, info);
}

/**
 * camel_store_summary_remove_uid:
 * @s: 
 * @path: 
 * 
 * Remove a specific info record from the summary, by @path.
 **/
void camel_store_summary_remove_path(CamelStoreSummary *s, const char *path)
{
        CamelStoreInfo *oldinfo;
        char *oldpath;

	CAMEL_STORE_SUMMARY_LOCK(s, ref_lock);
	CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
        if (g_hash_table_lookup_extended(s->folders_path, path, (void *)&oldpath, (void *)&oldinfo)) {
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
		CamelStoreInfo *info = s->folders->pdata[index];

		g_hash_table_remove(s->folders_path, camel_store_info_path(s, info));
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
	s->version = version;

	if (version < CAMEL_STORE_SUMMARY_VERSION_0) {
		g_warning("Store summary header version too low");
		return -1;
	}

	return 0;
}

static int
summary_header_save(CamelStoreSummary *s, FILE *out)
{
	fseek(out, 0, SEEK_SET);

	io(printf("Savining header\n"));

	/* always write latest version */
	camel_file_util_encode_fixed_int32(out, CAMEL_STORE_SUMMARY_VERSION);
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
CamelStoreInfo *
camel_store_summary_info_new(CamelStoreSummary *s)
{
	CamelStoreInfo *mi;

	CAMEL_STORE_SUMMARY_LOCK(s, alloc_lock);
	if (s->store_info_chunks == NULL)
		s->store_info_chunks = e_memchunk_new(32, s->store_info_size);
	mi = e_memchunk_alloc0(s->store_info_chunks);
	CAMEL_STORE_SUMMARY_UNLOCK(s, alloc_lock);
	mi->refcount = 1;
	return mi;
}

const char *camel_store_info_string(CamelStoreSummary *s, const CamelStoreInfo *mi, int type)
{
	return ((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->store_info_string(s, mi, type);
}

void camel_store_info_set_string(CamelStoreSummary *s, CamelStoreInfo *mi, int type, const char *value)
{
	((CamelStoreSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->store_info_set_string(s, mi, type, value);
}

static CamelStoreInfo *
store_info_new(CamelStoreSummary *s, const char *f)
{
	CamelStoreInfo *mi;

	mi = camel_store_summary_info_new(s);

	mi->path = g_strdup(f);
	mi->unread = CAMEL_STORE_INFO_FOLDER_UNKNOWN;
	mi->total = CAMEL_STORE_INFO_FOLDER_UNKNOWN;

	return mi;
}

static CamelStoreInfo *
store_info_load(CamelStoreSummary *s, FILE *in)
{
	CamelStoreInfo *mi;

	mi = camel_store_summary_info_new(s);

	io(printf("Loading folder info\n"));

	camel_file_util_decode_string(in, &mi->path);
	camel_file_util_decode_uint32(in, &mi->flags);
	camel_file_util_decode_uint32(in, &mi->unread);
	camel_file_util_decode_uint32(in, &mi->total);

	/* Ok, brown paper bag bug - prior to version 2 of the file, flags are
	   stored using the bit number, not the bit. Try to recover as best we can */
	if (s->version < CAMEL_STORE_SUMMARY_VERSION_2) {
		guint32 flags = 0;

		if (mi->flags & 1)
			flags |= CAMEL_STORE_INFO_FOLDER_NOSELECT;
		if (mi->flags & 2)
			flags |= CAMEL_STORE_INFO_FOLDER_READONLY;
		if (mi->flags & 3)
			flags |= CAMEL_STORE_INFO_FOLDER_SUBSCRIBED;
		if (mi->flags & 4)
			flags |= CAMEL_STORE_INFO_FOLDER_FLAGGED;

		mi->flags = flags;
	}

	if (!ferror(in))
		return mi;

	camel_store_summary_info_free(s, mi);

	return NULL;
}

static int
store_info_save(CamelStoreSummary *s, FILE *out, CamelStoreInfo *mi)
{
	io(printf("Saving folder info\n"));

	camel_file_util_encode_string(out, camel_store_info_path(s, mi));
	camel_file_util_encode_uint32(out, mi->flags);
	camel_file_util_encode_uint32(out, mi->unread);
	camel_file_util_encode_uint32(out, mi->total);

	return ferror(out);
}

static void
store_info_free(CamelStoreSummary *s, CamelStoreInfo *mi)
{
	g_free(mi->path);
	g_free(mi->uri);
	e_memchunk_free(s->store_info_chunks, mi);
}

static const char *
store_info_string(CamelStoreSummary *s, const CamelStoreInfo *mi, int type)
{
	const char *p;

	/* FIXME: Locks? */

	g_assert (mi != NULL);

	switch (type) {
	case CAMEL_STORE_INFO_PATH:
		return mi->path;
	case CAMEL_STORE_INFO_NAME:
		p = strrchr(mi->path, '/');
		if (p)
			return p+1;
		else
			return mi->path;
	case CAMEL_STORE_INFO_URI:
		if (mi->uri == NULL) {
			CamelURL *uri;

			uri = camel_url_new_with_base(s->uri_base, mi->path);
			((CamelStoreInfo *)mi)->uri = camel_url_to_string(uri, 0);
			camel_url_free(uri);
		}
		return mi->uri;
	}

	return "";
}

static void
store_info_set_string (CamelStoreSummary *s, CamelStoreInfo *mi, int type, const char *str)
{
	const char *p;
	char *v;
	int len;

	g_assert (mi != NULL);

	switch(type) {
	case CAMEL_STORE_INFO_PATH:
		CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
		g_hash_table_remove(s->folders_path, (char *)camel_store_info_path(s, mi));
		g_free(mi->path);
		g_free(mi->uri);
		mi->path = g_strdup(str);
		g_hash_table_insert(s->folders_path, (char *)camel_store_info_path(s, mi), mi);
		s->flags |= CAMEL_STORE_SUMMARY_DIRTY;
		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
		break;
	case CAMEL_STORE_INFO_NAME:
		CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
		g_hash_table_remove(s->folders_path, (char *)camel_store_info_path(s, mi));
		p = strrchr(mi->path, '/');
		if (p) {
			len = p-mi->path+1;
			v = g_malloc(len+strlen(str)+1);
			memcpy(v, mi->path, len);
			strcpy(v+len, str);
		} else {
			v = g_strdup(str);
		}
		g_free(mi->path);
		mi->path = v;
		g_hash_table_insert(s->folders_path, (char *)camel_store_info_path(s, mi), mi);
		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
		break;
	case CAMEL_STORE_INFO_URI:
		g_warning("Cannot set store info uri, aborting");
		abort();
		break;
	}
}
