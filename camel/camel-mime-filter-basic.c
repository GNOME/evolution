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

#include "camel-mime-filter-basic.h"

#include "camel-mime-utils.h"

static void camel_mime_filter_basic_class_init (CamelMimeFilterBasicClass *klass);
static void camel_mime_filter_basic_init       (CamelMimeFilterBasic *obj);

static CamelMimeFilterClass *camel_mime_filter_basic_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_mime_filter_basic_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMimeFilterBasic",
			sizeof (CamelMimeFilterBasic),
			sizeof (CamelMimeFilterBasicClass),
			(GtkClassInitFunc) camel_mime_filter_basic_class_init,
			(GtkObjectInitFunc) camel_mime_filter_basic_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_mime_filter_get_type (), &type_info);
	}
	
	return type;
}

static void
finalise(GtkObject *o)
{
	((GtkObjectClass *)camel_mime_filter_basic_parent)->finalize (o);
}

/* should this 'flush' outstanding state/data bytes? */
static void
reset(CamelMimeFilter *mf)
{
	CamelMimeFilterBasic *f = (CamelMimeFilterBasic *)mf;

	f->state = 0;
	f->save = 0;
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterBasic *f = (CamelMimeFilterBasic *)mf;
	int newlen;

	switch(f->type) {
	case CAMEL_MIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		camel_mime_filter_set_size(mf, len*2, FALSE);
		newlen = base64_encode_close(in, len, mf->outbuf, &f->state, &f->save);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_ENC:
		/* FIXME: *3 is probably not quite enough ... */
		camel_mime_filter_set_size(mf, len*3, FALSE);
		newlen = quoted_encode_close(in, len, mf->outbuf, &f->state, &f->save);
		break;
	case CAMEL_MIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
		camel_mime_filter_set_size(mf, len, FALSE);
		newlen = base64_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size */
		camel_mime_filter_set_size(mf, len, FALSE);
		newlen = quoted_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		break;
	default:
		g_warning("unknown type %d in CamelMimeFilterBasic", f->type);
		goto donothing;
	}

	*out = mf->outbuf;
	*outlen = newlen;
	*outprespace = mf->outpre;

	return;
donothing:
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

/* here we do all of the basic mime filtering */
static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterBasic *f = (CamelMimeFilterBasic *)mf;
	int newlen;

	switch(f->type) {
	case CAMEL_MIME_FILTER_BASIC_BASE64_ENC:
		/* wont go to more than 2x size (overly conservative) */
		camel_mime_filter_set_size(mf, len*2, FALSE);
		newlen = base64_encode_step(in, len, mf->outbuf, &f->state, &f->save);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_ENC:
		/* FIXME: *3 is probably not quite enough ... */
		camel_mime_filter_set_size(mf, len*3, FALSE);
		newlen = quoted_encode_step(in, len, mf->outbuf, &f->state, &f->save);
		break;
	case CAMEL_MIME_FILTER_BASIC_BASE64_DEC:
		/* output can't possibly exceed the input size */
		camel_mime_filter_set_size(mf, len, FALSE);
		newlen = base64_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		break;
	case CAMEL_MIME_FILTER_BASIC_QP_DEC:
		/* output can't possibly exceed the input size */
		camel_mime_filter_set_size(mf, len, FALSE);
		newlen = quoted_decode_step(in, len, mf->outbuf, &f->state, &f->save);
		break;
	default:
		g_warning("unknown type %d in CamelMimeFilterBasic", f->type);
		goto donothing;
	}

	*out = mf->outbuf;
	*outlen = newlen;
	*outprespace = mf->outpre;

	return;
donothing:
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
camel_mime_filter_basic_class_init (CamelMimeFilterBasicClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	camel_mime_filter_basic_parent = gtk_type_class (camel_mime_filter_get_type ());

	object_class->finalize = finalise;

	filter_class->reset = reset;
	filter_class->filter = filter;
	filter_class->complete = complete;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_mime_filter_basic_init (CamelMimeFilterBasic *obj)
{
	obj->state = 0;
	obj->save = 0;
}

/**
 * camel_mime_filter_basic_new:
 *
 * Create a new CamelMimeFilterBasic object.
 * 
 * Return value: A new CamelMimeFilterBasic widget.
 **/
CamelMimeFilterBasic *
camel_mime_filter_basic_new (void)
{
	CamelMimeFilterBasic *new = CAMEL_MIME_FILTER_BASIC ( gtk_type_new (camel_mime_filter_basic_get_type ()));
	return new;
}

CamelMimeFilterBasic *
camel_mime_filter_basic_new_type(CamelMimeFilterBasicType type)
{
	CamelMimeFilterBasic *new;

	switch (type) {
	case CAMEL_MIME_FILTER_BASIC_BASE64_ENC:
	case CAMEL_MIME_FILTER_BASIC_QP_ENC:
	case CAMEL_MIME_FILTER_BASIC_BASE64_DEC:
	case CAMEL_MIME_FILTER_BASIC_QP_DEC:
		new = camel_mime_filter_basic_new();
		new->type = type;
		break;
	default:
		g_warning("Invalid type of CamelMimeFilterBasic requested: %d", type);
		new = NULL;
		break;
	}
	return new;
}

