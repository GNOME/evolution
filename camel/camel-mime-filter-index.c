/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "camel-mime-filter-index.h"


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

	g_free(f->name);
	f->index = NULL;	/* ibex's need refcounting? */
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterIndex *f = (CamelMimeFilterIndex *)mf;

	if (f->index == NULL || f->name==NULL) {
		goto donothing;
	}

	ibex_index_buffer(f->index, f->name, in, len, NULL);

donothing:
	*out = in;
	*outlenptr = len;
	*outprespace = prespace;
}

static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterIndex *f = (CamelMimeFilterIndex *)mf;
	int inleft = 0;

	if (f->index == NULL || f->name==NULL) {
		goto donothing;
	}

	ibex_index_buffer(f->index, f->name, in, len, &inleft);

	if (inleft>0) {
		camel_mime_filter_backup(mf, in+(len-inleft), inleft);
	}

	*out = in;
	*outlenptr = len-inleft;
	*outprespace = prespace;
	return;

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

CamelMimeFilterIndex      *camel_mime_filter_index_new_ibex (ibex *index)
{
	CamelMimeFilterIndex *new = camel_mime_filter_index_new();

	if (new) {
		new->index = index;
		new->name = g_strdup("");
	}
	return new;
}

/* Set the match name for any indexed words */
void camel_mime_filter_index_set_name (CamelMimeFilterIndex *mf, char *name)
{
	g_free(mf->name);
	mf->name = g_strdup(name);
}

void camel_mime_filter_index_set_ibex (CamelMimeFilterIndex *mf, ibex *index)
{
	if (mf->index) {
		char *out;
		size_t outlen, outspace;

		camel_mime_filter_complete((CamelMimeFilter *)mf, "", 0, 0, &out, &outlen, &outspace);
	}
	mf->index = index;
}



