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

#include "camel-stream-filter.h"

struct _filter {
	struct _filter *next;
	int id;
	CamelMimeFilter *filter;
};

struct _CamelStreamFilterPrivate {
	struct _filter *filters;
	int filterid;		/* next filter id */
	
	char *realbuffer;	/* buffer - READ_PAD */
	char *buffer;		/* READ_SIZE bytes */

	char *filtered;		/* the filtered data */
	size_t filteredlen;

	int last_was_read;	/* was the last op read or write? */
};

#define READ_PAD (64)		/* bytes padded before buffer */
#define READ_SIZE (4096)

#define _PRIVATE(o) (((CamelStreamFilter *)(o))->priv)

static void camel_stream_filter_class_init (CamelStreamFilterClass *klass);
static void camel_stream_filter_init       (CamelStreamFilter *obj);

static	gint      do_read       (CamelStream *stream, gchar *buffer, gint n);
static	gint      do_write      (CamelStream *stream, const gchar *buffer, gint n);
static	void      do_flush      (CamelStream *stream);
static	gboolean  do_eos        (CamelStream *stream);
static	void      do_reset      (CamelStream *stream);

static CamelStreamClass *camel_stream_filter_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_stream_filter_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelStreamFilter",
			sizeof (CamelStreamFilter),
			sizeof (CamelStreamFilterClass),
			(GtkClassInitFunc) camel_stream_filter_class_init,
			(GtkObjectInitFunc) camel_stream_filter_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_stream_get_type (), &type_info);
	}
	
	return type;
}

static void
finalise(GtkObject *o)
{
	CamelStreamFilter *filter = (CamelStreamFilter *)o;
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);
	struct _filter *fn, *f;

	f = p->filters;
	while (f) {
		fn = f->next;
		gtk_object_unref((GtkObject *)f->filter);
		g_free(f);
		f = fn;
	}
	g_free(p->realbuffer);
	g_free(p);
	gtk_object_unref((GtkObject *)filter->source);

	GTK_OBJECT_CLASS (camel_stream_filter_parent)->finalize (o);
}


static void
camel_stream_filter_class_init (CamelStreamFilterClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	CamelStreamClass *camel_stream_class = (CamelStreamClass *) klass;

	camel_stream_filter_parent = gtk_type_class (camel_stream_get_type ());

	object_class->finalize = finalise;

	camel_stream_class->read = do_read;
	camel_stream_class->write = do_write;
	camel_stream_class->flush = do_flush;
	camel_stream_class->eos = do_eos; 
	camel_stream_class->reset = do_reset;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_stream_filter_init (CamelStreamFilter *obj)
{
	struct _CamelStreamFilterPrivate *p;
	
	_PRIVATE(obj) = p = g_malloc0(sizeof(*p));
	p->realbuffer = g_malloc(READ_SIZE + READ_PAD);
	p->buffer = p->realbuffer + READ_PAD;
	p->last_was_read = TRUE;
}

/**
 * camel_stream_filter_new:
 *
 * Create a new CamelStreamFilter object.
 * 
 * Return value: A new CamelStreamFilter object.
 **/
CamelStreamFilter *
camel_stream_filter_new_with_stream(CamelStream *stream)
{
	CamelStreamFilter *new = CAMEL_STREAM_FILTER ( gtk_type_new (camel_stream_filter_get_type ()));

	new->source = stream;
	gtk_object_ref ((GtkObject *)stream);

	return new;
}


/**
 * camel_stream_filter_add:
 * @filter: Initialised CamelStreamFilter.
 * @mf:  Filter to perform processing on stream.
 * 
 * Add a new CamelMimeFilter to execute during the processing of this
 * stream.  Each filter added is processed after the previous one.
 *
 * Note that a filter should only be added to a single stream
 * at a time, otherwise unpredictable results may occur.
 * 
 * Return value: A filter id for this CamelStreamFilter.
 **/
int
camel_stream_filter_add(CamelStreamFilter *filter, CamelMimeFilter *mf)
{
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);
	struct _filter *fn, *f;

	fn = g_malloc(sizeof(*fn));
	fn->id = p->filterid++;
	fn->filter = mf;
	gtk_object_ref((GtkObject *)mf);

	/* sure, we could use a GList, but we wouldn't save much */
	f = (struct _filter *)&p->filters;
	while (f->next)
		f = f->next;
	f->next = fn;
	fn->next = NULL;
	return fn->id;
}

/**
 * camel_stream_filter_remove:
 * @filter: Initialised CamelStreamFilter.
 * @id: Filter id, as returned from camel_stream_filter_add().
 * 
 * Remove a processing filter from the stream, by id.
 **/
void
camel_stream_filter_remove(CamelStreamFilter *filter, int id)
{
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);
	struct _filter *fn, *f;

	f = (struct _filter *)&p->filters;
	while (f && f->next) {
		fn = f->next;
		if (fn->id == id) {
			f->next = fn->next;
			gtk_object_unref((GtkObject *)fn->filter);
			g_free(fn);
		}
		f = f->next;
	}
}

static	gint      do_read       (CamelStream *stream, gchar *buffer, gint n)
{
	CamelStreamFilter *filter = (CamelStreamFilter *)stream;
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);
	int size;
	struct _filter *f;

	p->last_was_read = TRUE;

	if (p->filteredlen<=0) {
		int presize = READ_SIZE;

		size = camel_stream_read(filter->source, p->buffer, READ_SIZE);
		if (size<=0) {
			/* this is somewhat untested */
			if (camel_stream_eos(filter->source)) {
				f = p->filters;
				p->filtered = p->buffer;
				p->filteredlen = 0;
				while (f) {
					camel_mime_filter_complete(f->filter, p->filtered, p->filteredlen, presize, &p->filtered, &p->filteredlen, &presize);
					f = f->next;
				}
				size = p->filteredlen;
			}
			if (size<=0)
				return size;
		} else {
			f = p->filters;
			p->filtered = p->buffer;
			p->filteredlen = size;
			while (f) {
				camel_mime_filter_filter(f->filter, p->filtered, p->filteredlen, presize, &p->filtered, &p->filteredlen, &presize);
				f = f->next;
			}
		}
	}

	size = MIN(n, p->filteredlen);
	memcpy(buffer, p->filtered, size);
	p->filteredlen -= size;
	p->filtered += size;

	return size;
}

static	gint      do_write      (CamelStream *stream, const gchar *buf, gint n)
{
	CamelStreamFilter *filter = (CamelStreamFilter *)stream;
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);
	struct _filter *f;
	int presize;
	char *buffer = (char *)buf;

	p->last_was_read = FALSE;

	f = p->filters;
	presize = 0;
	while (f) {
		camel_mime_filter_filter(f->filter, buffer, n, presize, &buffer, &n, &presize);
		f = f->next;
	}

	return camel_stream_write(filter->source, buffer, n);
}

static	void      do_flush      (CamelStream *stream)
{
	CamelStreamFilter *filter = (CamelStreamFilter *)stream;
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);
	struct _filter *f;
	char *buffer;
	int len, presize;

	if (p->last_was_read) {
		g_warning("Flushing a filter stream without writing to it");
		return;
	}

	buffer = "";
	len = 0;
	presize = 0;
	f = p->filters;
	while (f) {
		camel_mime_filter_complete(f->filter, buffer, len, presize, &buffer, &len, &presize);
		f = f->next;
	}
	if (camel_stream_write(filter->source, buffer, len) == -1) {
		g_warning("Flushing filter failed to write, no way to signal failure ...");
	}

	return camel_stream_flush(filter->source);
}

static	gboolean  do_eos        (CamelStream *stream)
{
	CamelStreamFilter *filter = (CamelStreamFilter *)stream;
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);

	if (p->filteredlen >0)
		return FALSE;

	return camel_stream_eos(filter->source);
}

static	void      do_close      (CamelStream *stream)
{
	CamelStreamFilter *filter = (CamelStreamFilter *)stream;
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);

	if (p->last_was_read == 0) {
		camel_stream_flush(stream);
	}

	p->filteredlen = 0;
	camel_stream_close(filter->source);
}

static	void      do_reset      (CamelStream *stream)
{
	CamelStreamFilter *filter = (CamelStreamFilter *)stream;
	struct _CamelStreamFilterPrivate *p = _PRIVATE(filter);
	struct _filter *f;

	p->filteredlen = 0;
	camel_stream_reset(filter->source);

	/* and reset filters */
	f = p->filters;
	while (f) {
		camel_mime_filter_reset(f->filter);
		f = f->next;
	}
}

