/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-file-entry.h>

#include "filter-file.h"
#include "libedataserver/e-sexp.h"
#include "widgets/misc/e-error.h"

#define d(x)

static gboolean validate (FilterElement *fe);
static int file_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_file_class_init (FilterFileClass *klass);
static void filter_file_init (FilterFile *ff);
static void filter_file_finalise (GObject *obj);


static FilterElementClass *parent_class = NULL;


GType
filter_file_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterFileClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_file_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterFile),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_file_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterFile", &info, 0);
	}
	
	return type;
}

static void
filter_file_class_init (FilterFileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);
	
	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);
	
	object_class->finalize = filter_file_finalise;
	
	/* override methods */
	fe_class->validate = validate;
	fe_class->eq = file_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_file_init (FilterFile *ff)
{
	;
}

static void
filter_file_finalise (GObject *obj)
{
	FilterFile *ff = (FilterFile *) obj;
	
	xmlFree (ff->type);
	g_free (ff->path);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_file_new:
 *
 * Create a new FilterFile object.
 * 
 * Return value: A new #FilterFile object.
 **/
FilterFile *
filter_file_new (void)
{
	return (FilterFile *) g_object_new (FILTER_TYPE_FILE, NULL, NULL);
}


FilterFile *
filter_file_new_type_name (const char *type)
{
	FilterFile *file;
	
	file = filter_file_new ();
	file->type = xmlStrdup (type);
	
	return file;
}

void
filter_file_set_path (FilterFile *file, const char *path)
{
	g_free (file->path);
	file->path = g_strdup (path);
}

static gboolean
validate (FilterElement *fe)
{
	FilterFile *file = (FilterFile *) fe;
	struct stat st;
	
	if (!file->path) {
		/* FIXME: FilterElement should probably have a
                   GtkWidget member pointing to the value gotten with
                   ::get_widget() so that we can get the parent window
                   here. */
		e_error_run(NULL, "filter:no-file", NULL);

		return FALSE;
	}
	
	/* FIXME: do more to validate command-lines? */
	
	if (strcmp (file->type, "file") == 0) {
		if (stat (file->path, &st) == -1 || !S_ISREG (st.st_mode)) {
			/* FIXME: FilterElement should probably have a
			   GtkWidget member pointing to the value gotten with
			   ::get_widget() so that we can get the parent window
			   here. */
			e_error_run(NULL, "filter:bad-file", file->path, NULL);
			
			return FALSE;
		}
	} else if (strcmp (file->type, "command") == 0) {
		/* only requirements so far is that the command can't
		   be an empty string */
		return file->path[0] != '\0';
	}
	
	return TRUE;
}

static int
file_eq (FilterElement *fe, FilterElement *cm)
{
	FilterFile *ff = (FilterFile *)fe, *cf = (FilterFile *)cm;
	
        return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& ((ff->path && cf->path && strcmp (ff->path, cf->path) == 0)
		    || (ff->path == NULL && cf->path == NULL))
		&& ((ff->type && cf->type && strcmp (ff->type, cf->type) == 0)
		    || (ff->type == NULL && cf->type == NULL));
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	FilterFile *file = (FilterFile *) fe;
	xmlNodePtr cur, value;
	char *type;
	
	type = file->type ? file->type : "file";
	
	d(printf ("Encoding %s as xml\n", type));
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", type);
	
	cur = xmlNewChild (value, NULL, type, NULL);
	xmlNodeSetContent (cur, file->path);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterFile *file = (FilterFile *)fe;
	char *name, *str, *type;
	xmlNodePtr n;
	
	name = xmlGetProp (node, "name");
	type = xmlGetProp (node, "type");
	
	d(printf("Decoding %s from xml %p\n", type, fe));
	d(printf ("Name = %s\n", name));
	
	xmlFree (fe->name);
	fe->name = name;
	xmlFree (file->type);
	file->type = type;
	
	g_free (file->path);
	file->path = NULL;
	
	n = node->children;
	while (n != NULL) {
		if (!strcmp (n->name, type)) {
			str = xmlNodeGetContent (n);
			file->path = g_strdup (str ? str : "");
			xmlFree (str);
			
			d(printf ("  '%s'\n", file->path));
			break;
		} else if (n->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown node type '%s' encountered decoding a %s\n", n->name, type);
		}
		
		n = n->next;
	}
	
	return 0;
}

static void
entry_changed (GtkEntry *entry, FilterElement *fe)
{
	FilterFile *file = (FilterFile *) fe;
	const char *new;
	
	new = gtk_entry_get_text (entry);
	
	g_free (file->path);
	file->path = g_strdup (new);
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterFile *file = (FilterFile *) fe;
	GtkWidget *fileentry, *entry;
	
	fileentry = gnome_file_entry_new (NULL, _("Choose a file"));
	g_object_set (G_OBJECT (fileentry), "use_filechooser", TRUE, NULL);
	gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (fileentry), file->path);
	gnome_file_entry_set_modal (GNOME_FILE_ENTRY (fileentry), TRUE);
	
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fileentry));
	gtk_entry_set_text (GTK_ENTRY (entry), file->path);
	
	g_signal_connect (entry, "changed", G_CALLBACK (entry_changed), fe);
	
	return fileentry;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterFile *file = (FilterFile *) fe;
	
	e_sexp_encode_string (out, file->path);
}
