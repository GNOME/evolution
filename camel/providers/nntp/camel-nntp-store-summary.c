/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Ximian Inc.
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

#include "camel-nntp-store-summary.h"

#include "camel-file-utils.h"

#include "e-util/md5-utils.h"
#include "e-util/e-memory.h"

#include "camel-private.h"
#include "camel-utf8.h"

#define d(x)
#define io(x)			/* io debug */

#define CAMEL_NNTP_STORE_SUMMARY_VERSION_0 (0)
#define CAMEL_NNTP_STORE_SUMMARY_VERSION_1 (1)

#define CAMEL_NNTP_STORE_SUMMARY_VERSION (1)

#define _PRIVATE(o) (((CamelNNTPStoreSummary *)(o))->priv)

static int summary_header_load(CamelStoreSummary *, FILE *);
static int summary_header_save(CamelStoreSummary *, FILE *);

/*static CamelStoreInfo * store_info_new(CamelStoreSummary *, const char *);*/
static CamelStoreInfo * store_info_load(CamelStoreSummary *, FILE *);
static int		 store_info_save(CamelStoreSummary *, FILE *, CamelStoreInfo *);
static void		 store_info_free(CamelStoreSummary *, CamelStoreInfo *);

static const char *store_info_string(CamelStoreSummary *, const CamelStoreInfo *, int);
static void store_info_set_string(CamelStoreSummary *, CamelStoreInfo *, int, const char *);

static void camel_nntp_store_summary_class_init (CamelNNTPStoreSummaryClass *klass);
static void camel_nntp_store_summary_init       (CamelNNTPStoreSummary *obj);
static void camel_nntp_store_summary_finalise   (CamelObject *obj);

static CamelStoreSummaryClass *camel_nntp_store_summary_parent;

static void
camel_nntp_store_summary_class_init (CamelNNTPStoreSummaryClass *klass)
{
	CamelStoreSummaryClass *ssklass = (CamelStoreSummaryClass *)klass;

	ssklass->summary_header_load = summary_header_load;
	ssklass->summary_header_save = summary_header_save;

	/*ssklass->store_info_new  = store_info_new;*/
	ssklass->store_info_load = store_info_load;
	ssklass->store_info_save = store_info_save;
	ssklass->store_info_free = store_info_free;

	ssklass->store_info_string = store_info_string;
	ssklass->store_info_set_string = store_info_set_string;
}

static void
camel_nntp_store_summary_init (CamelNNTPStoreSummary *s)
{
	/*struct _CamelNNTPStoreSummaryPrivate *p;

	  p = _PRIVATE(s) = g_malloc0(sizeof(*p));*/

	((CamelStoreSummary *) s)->store_info_size = sizeof (CamelNNTPStoreInfo);
	s->version = CAMEL_NNTP_STORE_SUMMARY_VERSION;
	memset (&s->last_newslist, 0, sizeof (s->last_newslist));
}

static void
camel_nntp_store_summary_finalise (CamelObject *obj)
{
	/*struct _CamelNNTPStoreSummaryPrivate *p;*/
	/*CamelNNTPStoreSummary *s = (CamelNNTPStoreSummary *)obj;*/

	/*p = _PRIVATE(obj);
	  g_free(p);*/
}

CamelType
camel_nntp_store_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		camel_nntp_store_summary_parent = (CamelStoreSummaryClass *)camel_store_summary_get_type();
		type = camel_type_register((CamelType)camel_nntp_store_summary_parent, "CamelNNTPStoreSummary",
					   sizeof (CamelNNTPStoreSummary),
					   sizeof (CamelNNTPStoreSummaryClass),
					   (CamelObjectClassInitFunc) camel_nntp_store_summary_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_nntp_store_summary_init,
					   (CamelObjectFinalizeFunc) camel_nntp_store_summary_finalise);
	}
	
	return type;
}

/**
 * camel_nntp_store_summary_new:
 *
 * Create a new CamelNNTPStoreSummary object.
 *
 * Return value: A new CamelNNTPStoreSummary widget.
 **/
CamelNNTPStoreSummary *
camel_nntp_store_summary_new (void)
{
	return (CamelNNTPStoreSummary *) camel_object_new (camel_nntp_store_summary_get_type ());
}

/**
 * camel_nntp_store_summary_full_name:
 * @s:
 * @path:
 *
 * Retrieve a summary item by full name.
 *
 * A referenced to the summary item is returned, which may be
 * ref'd or free'd as appropriate.
 *
 * Return value: The summary item, or NULL if the @full_name name
 * is not available.
 * It must be freed using camel_store_summary_info_free().
 **/
CamelNNTPStoreInfo *
camel_nntp_store_summary_full_name(CamelNNTPStoreSummary *s, const char *full_name)
{
	int count, i;
	CamelNNTPStoreInfo *info;
	
	count = camel_store_summary_count ((CamelStoreSummary *) s);
	for (i = 0; i < count; i++) {
		info = (CamelNNTPStoreInfo *)camel_store_summary_index ((CamelStoreSummary *) s, i);
		if (info) {
			if (strcmp (info->full_name, full_name) == 0)
				return info;
			camel_store_summary_info_free ((CamelStoreSummary *) s, (CamelStoreInfo *)info);
		}
	}
	
	return NULL;
}

char *
camel_nntp_store_summary_full_to_path (CamelNNTPStoreSummary *s, const char *full_name, char dir_sep)
{
	char *path, *p;
	int c;
	const char *f;

	if (dir_sep != '/') {
		p = path = g_alloca (strlen (full_name) * 3 + 1);
		f = full_name;
		while ((c = *f++ & 0xff)) {
			if (c == dir_sep)
				*p++ = '/';
			else if (c == '/' || c == '%')
				p += sprintf (p, "%%%02X", c);
			else
				*p++ = c;
		}
		*p = 0;
	} else
		path = (char *) full_name;
	
	return camel_utf7_utf8 (path);
}

static guint32
hexnib (guint32 c)
{
	if (c >= '0' && c <= '9')
		return c-'0';
	else if (c >= 'A' && c <= 'Z')
		return c - 'A' + 10;
	else
		return 0;
}

char *
camel_nntp_store_summary_path_to_full (CamelNNTPStoreSummary *s, const char *path, char dir_sep)
{
	unsigned char *full, *f;
	guint32 c, v = 0;
	const char *p;
	int state=0;
	char *subpath, *last = NULL;
	CamelStoreInfo *si;
	
	/* check to see if we have a subpath of path already defined */
	subpath = g_alloca (strlen (path) + 1);
	strcpy (subpath, path);
	do {
		si = camel_store_summary_path ((CamelStoreSummary *) s, subpath);
		if (si == NULL) {
			last = strrchr (subpath, '/');
			if (last)
				*last = 0;
		}
	} while (si == NULL && last);
	
	/* path is already present, use the raw version we have */
	if (si && strlen (subpath) == strlen (path)) {
		f = g_strdup (camel_nntp_store_info_full_name (s, si));
		camel_store_summary_info_free ((CamelStoreSummary *) s, si);
		return f;
	}
	
	f = full = g_alloca (strlen (path)*2+1);
	if (si)
		p = path + strlen (subpath);
	else
		p = path;
	
	while ((c = camel_utf8_getc ((const unsigned char **) &p))) {
		switch (state) {
		case 0:
			if (c == '%') {
				state = 1;
			} else {
				if (c == '/')
					c = dir_sep;
				camel_utf8_putc(&f, c);
			}
			break;
		case 1:
			state = 2;
			v = hexnib (c) << 4;
			break;
		case 2:
			state = 0;
			v |= hexnib (c);
			camel_utf8_putc (&f, v);
			break;
		}
	}
	camel_utf8_putc (&f, c);
	
	/* merge old path part if required */
	f = camel_utf8_utf7 (full);
	if (si) {
		full = g_strdup_printf ("%s%s", camel_nntp_store_info_full_name (s, si), f);
		g_free (f);
		camel_store_summary_info_free ((CamelStoreSummary *) s, si);
		f = full;
	}
	
	return f;
}

CamelNNTPStoreInfo *
camel_nntp_store_summary_add_from_full (CamelNNTPStoreSummary *s, const char *full, char dir_sep)
{
	CamelNNTPStoreInfo *info;
	char *pathu8;
	int len;
	char *full_name;
	
	d(printf("adding full name '%s' '%c'\n", full, dir_sep));
	
	len = strlen (full);
	full_name = g_alloca (len+1);
	strcpy(full_name, full);
	if (full_name[len-1] == dir_sep)
		full_name[len-1] = 0;
	
	info = camel_nntp_store_summary_full_name (s, full_name);
	if (info) {
		camel_store_summary_info_free ((CamelStoreSummary *) s, (CamelStoreInfo *) info);
		d(printf("  already there\n"));
		return info;
	}
	
	pathu8 = camel_nntp_store_summary_full_to_path (s, full_name, dir_sep);
	
	info = (CamelNNTPStoreInfo *) camel_store_summary_add_from_path ((CamelStoreSummary *) s, pathu8);
	if (info) {
		d(printf("  '%s' -> '%s'\n", pathu8, full_name));
		camel_store_info_set_string((CamelStoreSummary *)s, (CamelStoreInfo *)info, CAMEL_NNTP_STORE_INFO_FULL_NAME, full_name);
	} else
		d(printf("  failed\n"));
	
	return info;
}

static int
summary_header_load (CamelStoreSummary *s, FILE *in)
{
	CamelNNTPStoreSummary *is = (CamelNNTPStoreSummary *) s;
	gint32 version, nil;
	
	if (camel_nntp_store_summary_parent->summary_header_load ((CamelStoreSummary *) s, in) == -1
	    || camel_file_util_decode_fixed_int32 (in, &version) == -1)
		return -1;
	
	is->version = version;
	
	if (version < CAMEL_NNTP_STORE_SUMMARY_VERSION_0) {
		g_warning("Store summary header version too low");
		return -1;
	}
	
	if (fread (is->last_newslist, 1, NNTP_DATE_SIZE, in) < NNTP_DATE_SIZE)
		return -1;
	
	camel_file_util_decode_fixed_int32 (in, &nil);

	return 0;
}

static int
summary_header_save (CamelStoreSummary *s, FILE *out)
{
	CamelNNTPStoreSummary *is = (CamelNNTPStoreSummary *) s;
	
	/* always write as latest version */
	if (camel_nntp_store_summary_parent->summary_header_save ((CamelStoreSummary *) s, out) == -1
	    || camel_file_util_encode_fixed_int32 (out, CAMEL_NNTP_STORE_SUMMARY_VERSION) == -1
	    || fwrite (is->last_newslist, 1, NNTP_DATE_SIZE, out) < NNTP_DATE_SIZE
	    || camel_file_util_encode_fixed_int32 (out, 0) == -1)
		return -1;
	
	return 0;
}

static CamelStoreInfo *
store_info_load (CamelStoreSummary *s, FILE *in)
{
	CamelNNTPStoreInfo *ni;
	
	ni = (CamelNNTPStoreInfo *) camel_nntp_store_summary_parent->store_info_load (s, in);
	if (ni) {
		if (camel_file_util_decode_string (in, &ni->full_name) == -1) {
			camel_store_summary_info_free (s, (CamelStoreInfo *) ni);
			return NULL;
		}
		if (((CamelNNTPStoreSummary *)s)->version >= CAMEL_NNTP_STORE_SUMMARY_VERSION_1) {
			if (camel_file_util_decode_uint32(in, &ni->first) == -1
			    || camel_file_util_decode_uint32(in, &ni->last) == -1) {
				camel_store_summary_info_free (s, (CamelStoreInfo *) ni);
				return NULL;
			}
		}
		/* set the URL */
	}
	
	return (CamelStoreInfo *) ni;
}

static int
store_info_save (CamelStoreSummary *s, FILE *out, CamelStoreInfo *mi)
{
	CamelNNTPStoreInfo *isi = (CamelNNTPStoreInfo *)mi;
	
	if (camel_nntp_store_summary_parent->store_info_save (s, out, mi) == -1
	    || camel_file_util_encode_string (out, isi->full_name) == -1
	    || camel_file_util_encode_uint32(out, isi->first) == -1
	    || camel_file_util_encode_uint32(out, isi->last) == -1)
		return -1;
	
	return 0;
}

static void
store_info_free (CamelStoreSummary *s, CamelStoreInfo *mi)
{
	CamelNNTPStoreInfo *nsi = (CamelNNTPStoreInfo *) mi;
	
	g_free (nsi->full_name);
	camel_nntp_store_summary_parent->store_info_free (s, mi);
}

static const char *
store_info_string(CamelStoreSummary *s, const CamelStoreInfo *mi, int type)
{
	CamelNNTPStoreInfo *nsi = (CamelNNTPStoreInfo *)mi;
	
	/* FIXME: Locks? */
	
	g_assert (mi != NULL);
	
	switch (type) {
	case CAMEL_NNTP_STORE_INFO_FULL_NAME:
		return nsi->full_name;
	default:
		return camel_nntp_store_summary_parent->store_info_string(s, mi, type);
	}
}

static void
store_info_set_string(CamelStoreSummary *s, CamelStoreInfo *mi, int type, const char *str)
{
	CamelNNTPStoreInfo *nsi = (CamelNNTPStoreInfo *)mi;
	
	g_assert(mi != NULL);
	
	switch (type) {
	case CAMEL_NNTP_STORE_INFO_FULL_NAME:
		d(printf("Set full name %s -> %s\n", nsi->full_name, str));
		CAMEL_STORE_SUMMARY_LOCK(s, summary_lock);
		g_free (nsi->full_name);
		nsi->full_name = g_strdup (str);
		CAMEL_STORE_SUMMARY_UNLOCK(s, summary_lock);
		break;
	default:
		camel_nntp_store_summary_parent->store_info_set_string (s, mi, type, str);
		break;
	}
}
