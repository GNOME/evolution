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
#include <regex.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-file-entry.h>
#include <gal/widgets/e-unicode.h>

#include "filter-file.h"
#include "e-util/e-sexp.h"

#define d(x) 

static gboolean validate (FilterElement *fe);
static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

static void filter_file_class_init	(FilterFileClass *class);
static void filter_file_init	(FilterFile *gspaper);
static void filter_file_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterFile *)(x))->priv)

struct _FilterFilePrivate {
};

static FilterElementClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GtkType
filter_file_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterFile",
			sizeof (FilterFile),
			sizeof (FilterFileClass),
			(GtkClassInitFunc) filter_file_class_init,
			(GtkObjectInitFunc) filter_file_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_file_class_init (FilterFileClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	FilterElementClass *filter_element = (FilterElementClass *) klass;
	
	parent_class = gtk_type_class (filter_element_get_type ());
	
	object_class->finalize = filter_file_finalise;
	
	/* override methods */
	filter_element->validate = validate;
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;
	
	/* signals */
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_file_init (FilterFile *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
filter_file_finalise (GtkObject *obj)
{
	FilterFile *o = (FilterFile *) obj;
	
	xmlFree (o->type);
	g_free (o->path);
	
	g_free (o->priv);
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
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
	return (FilterFile *) gtk_type_new (filter_file_get_type ());
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
	GtkWidget *dialog;
	struct stat st;
	
	if (!file->path) {
		dialog = gnome_ok_dialog (_("You must specify a file name"));
		
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		return FALSE;
	}
	
	if (strcmp (file->type, "command") != 0) {
		if (stat (file->path, &st) == -1 || !S_ISREG (st.st_mode)) {
			char *errmsg;
			
			errmsg = g_strdup_printf (_("File '%s' does not exist or is not a regular file."), file->path);
			dialog = gnome_ok_dialog (errmsg);
			g_free (errmsg);
			
			gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
			return FALSE;
		}
	}
	
	return TRUE;
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create (fe, node);
	
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	FilterFile *file = (FilterFile *) fe;
	xmlNodePtr cur, value;
	char *encstr, *type;
	
	type = file->type ? file->type : "file";
	
	d(printf ("Encoding %s as xml\n", type));
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", type);
	
	cur = xmlNewChild (value, NULL, type, NULL);
	encstr = e_utf8_xml1_encode (file->path);
	xmlNodeSetContent (cur, encstr);
	g_free (encstr);
	
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
	
	n = node->childs;
	if (!strcmp (n->name, type)) {
		char *decstr;
		
		str = xmlNodeGetContent (n);
		if (str) {
			decstr = e_utf8_xml1_decode (str);
			xmlFree (str);
		} else
			decstr = g_strdup ("");
		
		d(printf ("  '%s'\n", decstr));
		
		file->path = decstr;
	} else {
		g_warning ("Unknown node type '%s' encountered decoding a %s\n", n->name, type);
	}
	
	return 0;
}

static void
entry_changed (GtkEntry *entry, FilterElement *fe)
{
	FilterFile *file = (FilterFile *) fe;
	char *new;
	
	new = e_utf8_gtk_entry_get_text (entry);
	
	g_free (file->path);
	file->path = new;
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterFile *file = (FilterFile *) fe;
	GtkWidget *fileentry, *entry;
	
	fileentry = gnome_file_entry_new (NULL, _("Choose a file"));
	gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (fileentry), file->path);
	gnome_file_entry_set_modal (GNOME_FILE_ENTRY (fileentry), TRUE);
	
	entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (fileentry));
	e_utf8_gtk_entry_set_text (entry, file->path);
	
	gtk_signal_connect (GTK_OBJECT (entry), "changed", entry_changed, fe);
	
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
