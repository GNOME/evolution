/*
 *  Copyright (C) 2000 Ximian Inc.
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

#include "camel-mime-filter-index.h"

#include "camel-text-index.h"

static void camel_mime_filter_index_class_init (CamelMimeFilterIndexClass *klass);
static void camel_mime_filter_index_finalize   (CamelObject *o);

static CamelMimeFilterClass *camel_mime_filter_index_parent;

CamelType
camel_mime_filter_index_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (), "CamelMimeFilterIndex",
					    sizeof (CamelMimeFilterIndex),
					    sizeof (CamelMimeFilterIndexClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_index_class_init,
					    NULL,
					    NULL,
					    (CamelObjectFinalizeFunc) camel_mime_filter_index_finalize);
	}
	
	return type;
}

static void
camel_mime_filter_index_finalize(CamelObject *o)
{
	CamelMimeFilterIndex *f = (CamelMimeFilterIndex *)o;

	if (f->name)
		camel_object_unref((CamelObject *)f->name);
	camel_object_unref((CamelObject *)f->index);
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterIndex *f = (CamelMimeFilterIndex *)mf;

	if (f->index == NULL || f->name==NULL) {
		goto donothing;
	}

	camel_index_name_add_buffer(f->name, in, len);
	camel_index_name_add_buffer(f->name, NULL, 0);

donothing:
	*out = in;
	*outlenptr = len;
	*outprespace = prespace;
}

static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterIndex *f = (CamelMimeFilterIndex *)mf;

	if (f->index == NULL || f->name==NULL) {
		goto donothing;
	}

	camel_index_name_add_buffer(f->name, in, len);

donothing:
	*out = in;
	*outlenptr = len;
	*outprespace = prespace;
}

static void
camel_mime_filter_index_class_init (CamelMimeFilterIndexClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	camel_mime_filter_index_parent = CAMEL_MIME_FILTER_CLASS (camel_type_get_global_classfuncs (camel_mime_filter_get_type ()));

	/*filter_class->reset = reset;*/
	filter_class->filter = filter;
	filter_class->complete = complete;
}

/**
 * camel_mime_filter_index_new:
 *
 * Create a new CamelMimeFilterIndex object.
 * 
 * Return value: A new CamelMimeFilterIndex widget.
 **/
CamelMimeFilterIndex *
camel_mime_filter_index_new (void)
{
	CamelMimeFilterIndex *new = CAMEL_MIME_FILTER_INDEX ( camel_object_new (camel_mime_filter_index_get_type ()));
	return new;
}

CamelMimeFilterIndex      *camel_mime_filter_index_new_index (struct _CamelIndex *index)
{
	CamelMimeFilterIndex *new = camel_mime_filter_index_new();

	if (new) {
		new->index = index;
		if (index)
			camel_object_ref((CamelObject *)index);
	}
	return new;
}

/* Set the match name for any indexed words */
void camel_mime_filter_index_set_name (CamelMimeFilterIndex *mf, struct _CamelIndexName *name)
{
	if (mf->name)
		camel_object_unref((CamelObject *)mf->name);
	mf->name = name;
	if (name)
		camel_object_ref((CamelObject *)name);
}

void camel_mime_filter_index_set_index (CamelMimeFilterIndex *mf, CamelIndex *index)
{
	if (mf->index) {
		char *out;
		size_t outlen, outspace;

		camel_mime_filter_complete((CamelMimeFilter *)mf, "", 0, 0, &out, &outlen, &outspace);
	}

	mf->index = index;
	if (index)
		camel_object_ref((CamelObject *)index);
}



