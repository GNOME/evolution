/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#include <string.h>

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>

#include "filter-folder.h"
#include "mail/em-folder-selection-button.h"
#include "mail/mail-component.h"
#include "e-util/e-sexp.h"
#include "widgets/misc/e-error.h"

#define d(x)

static gboolean validate (FilterElement *fe);
static int folder_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static int xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *, GString *);

static void filter_folder_class_init (FilterFolderClass *class);
static void filter_folder_init (FilterFolder *ff);
static void filter_folder_finalise (GObject *obj);


static FilterElementClass *parent_class = NULL;


GType
filter_folder_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterFolderClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_folder_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterFolder),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_folder_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterFolder", &info, 0);
	}
	
	return type;
}

static void
filter_folder_class_init (FilterFolderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);
	
	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);
	
	object_class->finalize = filter_folder_finalise;
	
	/* override methods */
	fe_class->validate = validate;
	fe_class->eq = folder_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_folder_init (FilterFolder *ff)
{
	;
}

static void
filter_folder_finalise (GObject *obj)
{
	FilterFolder *ff = (FilterFolder *) obj;
	
	g_free (ff->uri);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_folder_new:
 *
 * Create a new FilterFolder object.
 * 
 * Return value: A new #FilterFolder object.
 **/
FilterFolder *
filter_folder_new (void)
{
	return (FilterFolder *) g_object_new (FILTER_TYPE_FOLDER, NULL, NULL);
}

void
filter_folder_set_value (FilterFolder *ff, const char *uri)
{
	g_free (ff->uri);
	ff->uri = g_strdup (uri);
}

static gboolean
validate (FilterElement *fe)
{
	FilterFolder *ff = (FilterFolder *) fe;
	
	if (ff->uri && *ff->uri) {
		return TRUE;
	} else {
		/* FIXME: FilterElement should probably have a
                   GtkWidget member pointing to the value gotten with
                   ::get_widget() so that we can get the parent window
                   here. */
		e_error_run(NULL, "filter:no-folder", NULL);

		return FALSE;
	}
}

static int
folder_eq (FilterElement *fe, FilterElement *cm)
{
        return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& strcmp(((FilterFolder *)fe)->uri, ((FilterFolder *)cm)->uri) == 0;
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
	xmlNodePtr value, work;
	FilterFolder *ff = (FilterFolder *)fe;
	
	d(printf ("Encoding folder as xml\n"));
	
	value = xmlNewNode (NULL, "value");
	xmlSetProp (value, "name", fe->name);
	xmlSetProp (value, "type", "folder");
	
	work = xmlNewChild (value, NULL, "folder", NULL);
	xmlSetProp (work, "uri", ff->uri);
	
	return value;
}

static int
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterFolder *ff = (FilterFolder *)fe;
	xmlNodePtr n;
	
	d(printf ("Decoding folder from xml %p\n", fe));
	
	xmlFree (fe->name);
	fe->name = xmlGetProp (node, "name");
	
	n = node->children;
	while (n) {
		if (!strcmp (n->name, "folder")) {
			char *uri;
			
			uri = xmlGetProp (n, "uri");
			g_free (ff->uri);
			ff->uri = g_strdup (uri);
			xmlFree (uri);
			break;
		}
		n = n->next;
	}
	
	return 0;
}

static void
folder_selected(EMFolderSelectionButton *button, FilterFolder *ff)
{
	const char *uri;

	uri = em_folder_selection_button_get_selection(button);
	g_free(ff->uri);
	ff->uri = uri!=NULL?em_uri_from_camel(uri):NULL;
	
	gdk_window_raise (GTK_WIDGET (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW))->window);
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterFolder *ff = (FilterFolder *)fe;
	GtkWidget *button;
	char *uri;

	uri = em_uri_to_camel(ff->uri);
	button = em_folder_selection_button_new (_("Select Folder"), NULL);
	em_folder_selection_button_set_selection(EM_FOLDER_SELECTION_BUTTON(button), uri);
	g_free(uri);
	
	gtk_widget_show (button);
	g_signal_connect (button, "selected", G_CALLBACK (folder_selected), ff);
	
	return button;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterFolder *ff = (FilterFolder *)fe;
	
	e_sexp_encode_string (out, ff->uri);
}
