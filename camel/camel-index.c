/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/*
 *  Copyright (C) 2001 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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

#include <ctype.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "camel-index.h"
#include "camel/camel-object.h"

#define w(x)
#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_INDEX_VERSION (0x01)

struct _CamelIndexPrivate {
	void *dummy;
};

#define _PRIVATE(o) (((CamelIndex *)(o))->priv)

#define CI_CLASS(o) ((CamelIndexClass *)(((CamelObject *)o)->classfuncs))

/* ********************************************************************** */
/* CamelIndex */
/* ********************************************************************** */

static CamelObjectClass *camel_index_parent;

static void
camel_index_class_init(CamelIndexClass *klass)
{
	camel_index_parent = CAMEL_OBJECT_CLASS(camel_type_get_global_classfuncs(camel_object_get_type()));
}

static void
camel_index_init(CamelIndex *idx)
{
	struct _CamelIndexPrivate *p;

	p = _PRIVATE(idx) = g_malloc0(sizeof(*p));

	idx->version = CAMEL_INDEX_VERSION;
}

static void
camel_index_finalise(CamelIndex *idx)
{
	g_free(idx->path);
	g_free(idx->priv);
}

CamelType
camel_index_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_object_get_type(), "CamelIndex",
					   sizeof (CamelIndex),
					   sizeof (CamelIndexClass),
					   (CamelObjectClassInitFunc) camel_index_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_index_init,
					   (CamelObjectFinalizeFunc) camel_index_finalise);
	}
	
	return type;
}

CamelIndex *
camel_index_new(const char *path, int flags)
{
	CamelIndex *idx = (CamelIndex *)camel_object_new(camel_index_get_type());

	camel_index_construct(idx, path, flags);

	return idx;
}

void
camel_index_construct(CamelIndex *idx, const char *path, int flags)
{
	g_free(idx->path);
	idx->path = g_strdup_printf("%s.index", path);
	idx->flags = flags;
}

int
camel_index_rename(CamelIndex *idx, const char *path)
{
	return CI_CLASS(idx)->rename(idx, path);
}

void
camel_index_set_normalise(CamelIndex *idx, CamelIndexNorm func, void *data)
{
	idx->normalise = func;
	idx->normalise_data = data;
}

int
camel_index_sync(CamelIndex *idx)
{
	return CI_CLASS(idx)->sync(idx);
}

int
camel_index_compress(CamelIndex *idx)
{
	return CI_CLASS(idx)->compress(idx);
}

int
camel_index_has_name(CamelIndex *idx, const char *name)
{
	return CI_CLASS(idx)->has_name(idx, name);
}

CamelIndexName *
camel_index_add_name(CamelIndex *idx, const char *name)
{
	return CI_CLASS(idx)->add_name(idx, name);
}

int
camel_index_write_name(CamelIndex *idx, CamelIndexName *idn)
{
	return CI_CLASS(idx)->write_name(idx, idn);
}

CamelIndexCursor *
camel_index_find_name(CamelIndex *idx, const char *name)
{
	return CI_CLASS(idx)->find_name(idx, name);
}

void
camel_index_delete_name(CamelIndex *idx, const char *name)
{
	return CI_CLASS(idx)->delete_name(idx, name);
}

CamelIndexCursor *
camel_index_find(CamelIndex *idx, const char *word)
{
	char *b = (char *)word;
	CamelIndexCursor *ret;

	if (idx->normalise)
		b = idx->normalise(idx, word, idx->normalise_data);

	ret = CI_CLASS(idx)->find(idx, b);

	if (b != word)
		g_free(b);

	return ret;
}

CamelIndexCursor *
camel_index_words(CamelIndex *idx)
{
	return CI_CLASS(idx)->words(idx);
}

CamelIndexCursor *
camel_index_names(CamelIndex *idx)
{
	return CI_CLASS(idx)->names(idx);
}

/* ********************************************************************** */
/* CamelIndexName */
/* ********************************************************************** */

static CamelObjectClass *camel_index_name_parent;

#define CIN_CLASS(o) ((CamelIndexNameClass *)(((CamelObject *)o)->classfuncs))

static void
camel_index_name_class_init(CamelIndexNameClass *klass)
{
	camel_index_name_parent = CAMEL_OBJECT_CLASS(camel_type_get_global_classfuncs(camel_object_get_type()));
}

static void
camel_index_name_init(CamelIndexName *idn)
{
}

static void
camel_index_name_finalise(CamelIndexName *idn)
{
	if (idn->index)
		camel_object_unref((CamelObject *)idn->index);
}

CamelType
camel_index_name_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_object_get_type(), "CamelIndexName",
					   sizeof (CamelIndexName),
					   sizeof (CamelIndexNameClass),
					   (CamelObjectClassInitFunc) camel_index_name_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_index_name_init,
					   (CamelObjectFinalizeFunc) camel_index_name_finalise);
	}
	
	return type;
}

CamelIndexName *
camel_index_name_new(CamelIndex *idx, const char *name)
{
	CamelIndexName *idn = (CamelIndexName *)camel_object_new(camel_index_name_get_type());

	idn->index = idx;
	camel_object_ref((CamelObject *)idx);

	return idn;
}

void
camel_index_name_add_word(CamelIndexName *idn, const char *word)
{
	char *b = (char *)word;

	if (idn->index->normalise)
		b = idn->index->normalise(idn->index, word, idn->index->normalise_data);

	CIN_CLASS(idn)->add_word(idn, b);

	if (b != word)
		g_free(b);
}

size_t
camel_index_name_add_buffer(CamelIndexName *idn, const char *buffer, size_t len)
{
	return CIN_CLASS(idn)->add_buffer(idn, buffer, len);
}

/* ********************************************************************** */
/* CamelIndexCursor */
/* ********************************************************************** */

static CamelObjectClass *camel_index_cursor_parent;

#define CIC_CLASS(o) ((CamelIndexCursorClass *)(((CamelObject *)o)->classfuncs))

static void
camel_index_cursor_class_init(CamelIndexCursorClass *klass)
{
	camel_index_cursor_parent = CAMEL_OBJECT_CLASS(camel_type_get_global_classfuncs(camel_object_get_type()));
}

static void
camel_index_cursor_init(CamelIndexCursor *idc)
{
}

static void
camel_index_cursor_finalise(CamelIndexCursor *idc)
{
	if (idc->index)
		camel_object_unref((CamelObject *)idc->index);
}

CamelType
camel_index_cursor_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_object_get_type(), "CamelIndexCursor",
					   sizeof (CamelIndexCursor),
					   sizeof (CamelIndexCursorClass),
					   (CamelObjectClassInitFunc) camel_index_cursor_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_index_cursor_init,
					   (CamelObjectFinalizeFunc) camel_index_cursor_finalise);
	}
	
	return type;
}

CamelIndexCursor *
camel_index_cursor_new(CamelIndex *idx, const char *name)
{
	CamelIndexCursor *idc = (CamelIndexCursor *)camel_object_new(camel_index_cursor_get_type());

	idc->index = idx;
	camel_object_ref((CamelObject *)idx);

	return idc;
}

const char *
camel_index_cursor_next(CamelIndexCursor *idc)
{
	return CIC_CLASS(idc)->next(idc);
}

void
camel_index_cursor_reset(CamelIndexCursor *idc)
{
	return CIC_CLASS(idc)->reset(idc);
}

