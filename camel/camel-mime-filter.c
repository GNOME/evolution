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

#include "camel-mime-filter.h"

struct _CamelMimeFilterPrivate {
	char *inbuf;
	size_t inlen;
};

#define PRE_HEAD (64)
#define BACK_HEAD (64)
#define _PRIVATE(o) (((CamelMimeFilter *)(o))->priv)
#define FCLASS(o) ((CamelMimeFilterClass *)((GtkObject *)(o))->klass)

static void camel_mime_filter_class_init (CamelMimeFilterClass *klass);
static void camel_mime_filter_init       (CamelMimeFilter *obj);

static CamelObjectClass *camel_mime_filter_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_mime_filter_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMimeFilter",
			sizeof (CamelMimeFilter),
			sizeof (CamelMimeFilterClass),
			(GtkClassInitFunc) camel_mime_filter_class_init,
			(GtkObjectInitFunc) camel_mime_filter_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_object_get_type (), &type_info);
	}
	
	return type;
}

static void
finalise(GtkObject *o)
{
	CamelMimeFilter *f = (CamelMimeFilter *)o;
	struct _CamelMimeFilterPrivate *p = _PRIVATE(f);

	g_free(f->outreal);
	g_free(f->backbuf);
	g_free(p->inbuf);
	g_free(p);

	((GtkObjectClass *)camel_mime_filter_parent)->finalize (o);
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	/* default - do nothing */
}

static void
camel_mime_filter_class_init (CamelMimeFilterClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	camel_mime_filter_parent = gtk_type_class (camel_object_get_type ());

	object_class->finalize = finalise;

	klass->complete = complete;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_mime_filter_init (CamelMimeFilter *obj)
{
	obj->outreal = NULL;
	obj->outbuf = NULL;
	obj->outsize = 0;

	obj->backbuf = NULL;
	obj->backsize = 0;
	obj->backlen = 0;

	_PRIVATE(obj) = g_malloc0(sizeof(*obj->priv));
}

/**
 * camel_mime_filter_new:
 *
 * Create a new CamelMimeFilter object.
 * 
 * Return value: A new CamelMimeFilter widget.
 **/
CamelMimeFilter *
camel_mime_filter_new (void)
{
	CamelMimeFilter *new = CAMEL_MIME_FILTER ( gtk_type_new (camel_mime_filter_get_type ()));
	return new;
}

static void filter_run(CamelMimeFilter *f,
		       char *in, size_t len, size_t prespace,
		       char **out, size_t *outlen, size_t *outprespace,
		       void (*filterfunc)(CamelMimeFilter *f,
					  char *in, size_t len, size_t prespace,
					  char **out, size_t *outlen, size_t *outprespace))
{
	struct _CamelMimeFilterPrivate *p;

	/*
	  here we take a performance hit, if the input buffer doesn't
	  have the pre-space required.  We make a buffer that does ...
	*/
	if (prespace < f->backlen) {
		int newlen = len+prespace;
		p = _PRIVATE(f);
		if (p->inlen < newlen) {
			/* NOTE: g_realloc copies data, we dont need that (slower) */
			g_free(p->inbuf);
			p->inbuf = g_malloc(newlen+PRE_HEAD);
			p->inlen = newlen+PRE_HEAD;
		}
		/* copy to end of structure */
		memcpy(p->inbuf+p->inlen - len, in, len);
		in = p->inbuf+p->inlen - len;
		prespace = p->inlen - len;
	}

	/* preload any backed up data */
	if (f->backlen > 0) {
		memcpy(in-f->backlen, f->backbuf, f->backlen);
		in -= f->backlen;
		prespace -= f->backlen;
		f->backlen = 0;
	}
	
	filterfunc(f, in, len, prespace, out, outlen, outprespace);
}

void camel_mime_filter_filter(CamelMimeFilter *f,
			      char *in, size_t len, size_t prespace,
			      char **out, size_t *outlen, size_t *outprespace)
{
	if (FCLASS(f)->filter)
		filter_run(f, in, len, prespace, out, outlen, outprespace, FCLASS(f)->filter);
	else
		g_error("Filter function unplmenented in class");
}

void camel_mime_filter_complete(CamelMimeFilter *f,
				char *in, size_t len, size_t prespace,
				char **out, size_t *outlen, size_t *outprespace)
{
	if (FCLASS(f)->complete)
		filter_run(f, in, len, prespace, out, outlen, outprespace, FCLASS(f)->complete);
}

void camel_mime_filter_reset(CamelMimeFilter *f)
{
	if (FCLASS(f)->reset) {
		FCLASS(f)->reset(f);
	}

	/* could free some buffers, if they are really big? */
	f->backlen = 0;
}

/* sets number of bytes backed up on the input, new calls replace previous ones */
void camel_mime_filter_backup(CamelMimeFilter *f, char *data, size_t length)
{
	if (f->backsize < length) {
		/* g_realloc copies data, unnecessary overhead */
		g_free(f->backbuf);
		f->backbuf = g_malloc(length+BACK_HEAD);
		f->backsize = length+BACK_HEAD;
	}
	f->backlen = length;
	memcpy(f->backbuf, data, length);
}

/* ensure this much size available for filter output (if required) */
void camel_mime_filter_set_size(CamelMimeFilter *f, size_t size, int keep)
{
	if (f->outsize < size) {
		int offset = f->outptr - f->outreal;
		if (keep) {
			f->outreal = g_realloc(f->outreal, size + PRE_HEAD*4);
		} else {
			g_free(f->outreal);
			f->outreal = g_malloc(size + PRE_HEAD*4);
		}
		f->outptr = f->outreal + offset;
		f->outbuf = f->outreal + PRE_HEAD*4;
		f->outsize = size;
		/* this could be offset from the end of the structure, but 
		   this should be good enough */
		f->outpre = PRE_HEAD*4;
	}
}

