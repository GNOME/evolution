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

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>
#include <errno.h>

#include "camel-mime-filter-save.h"

static void camel_mime_filter_save_class_init (CamelMimeFilterSaveClass *klass);
static void camel_mime_filter_save_init       (CamelMimeFilterSave *obj);

static CamelMimeFilterClass *camel_mime_filter_save_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_mime_filter_save_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMimeFilterSave",
			sizeof (CamelMimeFilterSave),
			sizeof (CamelMimeFilterSaveClass),
			(GtkClassInitFunc) camel_mime_filter_save_class_init,
			(GtkObjectInitFunc) camel_mime_filter_save_init,
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
	CamelMimeFilterSave *f = (CamelMimeFilterSave *)o;

	g_free(f->filename);
	if (f->fd != -1) {
		/* FIXME: what do we do with failed writes???? */
		close(f->fd);
	}

	((GtkObjectClass *)camel_mime_filter_save_parent)->finalize (o);
}

static void
reset(CamelMimeFilter *mf)
{
	CamelMimeFilterSave *f = (CamelMimeFilterSave *)mf;

	/* i dunno, how do you 'reset' a file?  reopen it? do i care? */
	if (f->fd != -1){
		lseek(f->fd, 0, SEEK_SET);
	}
}

/* all this code just to support this little trivial filter! */
static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterSave *f = (CamelMimeFilterSave *)mf;

	if (f->fd != -1) {
		/* FIXME: check return */
		int outlen = write(f->fd, in, len);
		if (outlen != len) {
			g_warning("could not write to '%s': %s", f->filename?f->filename:"<descriptor>", strerror(errno));
		}
	}
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}

static void
camel_mime_filter_save_class_init (CamelMimeFilterSaveClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;

	camel_mime_filter_save_parent = gtk_type_class (camel_mime_filter_get_type ());

	object_class->finalize = finalise;

	filter_class->reset = reset;
	filter_class->filter = filter;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_mime_filter_save_init (CamelMimeFilterSave *f)
{
	f->fd = -1;
}

/**
 * camel_mime_filter_save_new:
 *
 * Create a new CamelMimeFilterSave object.
 * 
 * Return value: A new CamelMimeFilterSave widget.
 **/
CamelMimeFilterSave *
camel_mime_filter_save_new (void)
{
	CamelMimeFilterSave *new = CAMEL_MIME_FILTER_SAVE ( gtk_type_new (camel_mime_filter_save_get_type ()));
	return new;
}

CamelMimeFilterSave *
camel_mime_filter_save_new_name (const char *name, int flags, int mode)
{
	CamelMimeFilterSave *new = NULL;

	new = camel_mime_filter_save_new();
	if (new) {
		new->fd = open(name, flags, mode);
		if (new->fd != -1) {
			new->filename = g_strdup(name);
		} else {
			gtk_object_unref((GtkObject *)new);
			new = NULL;
		}
	}
	return new;
}

