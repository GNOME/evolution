/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#include <config.h>

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <gal/widgets/e-unicode.h>

#include "filter-folder.h"
#include "shell/evolution-folder-selector-button.h"
#include "e-util/e-sexp.h"

#define d(x)

static gboolean validate (FilterElement *fe);
static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

extern EvolutionShellClient *global_shell_client;

static void filter_folder_class_init	(FilterFolderClass *class);
static void filter_folder_init	(FilterFolder *gspaper);
static void filter_folder_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterFolder *)(x))->priv)

struct _FilterFolderPrivate {
};

static FilterElementClass *parent_class;

guint
filter_folder_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterFolder",
			sizeof(FilterFolder),
			sizeof(FilterFolderClass),
			(GtkClassInitFunc)filter_folder_class_init,
			(GtkObjectInitFunc)filter_folder_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique (filter_element_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_folder_class_init (FilterFolderClass *class)
{
	GtkObjectClass *object_class;
	FilterElementClass *filter_element = (FilterElementClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class (filter_element_get_type ());

	object_class->finalize = filter_folder_finalise;

	/* override methods */
	filter_element->validate = validate;
	filter_element->xml_create = xml_create;
	filter_element->xml_encode = xml_encode;
	filter_element->xml_decode = xml_decode;
	filter_element->get_widget = get_widget;
	filter_element->build_code = build_code;
	filter_element->format_sexp = format_sexp;
}

static void
filter_folder_init (FilterFolder *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
filter_folder_finalise (GtkObject *obj)
{
	FilterFolder *o = (FilterFolder *)obj;
	
	g_free (o->uri);
	
        ((GtkObjectClass *)(parent_class))->finalize(obj);
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
	FilterFolder *o = (FilterFolder *)gtk_type_new (filter_folder_get_type ());
	return o;
}

void
filter_folder_set_value(FilterFolder *ff, const char *uri)
{
	g_free(ff->uri);
	ff->uri = g_strdup(uri);
}

static gboolean
validate (FilterElement *fe)
{
	FilterFolder *ff = (FilterFolder *) fe;
	
	if (ff->uri && *ff->uri) {
		return TRUE;
	} else {
		GtkWidget *dialog;
		
		dialog = gnome_ok_dialog (_("You forgot to choose a folder.\n"
					    "Please go back and specify a valid folder to deliver mail to."));
		
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		
		return FALSE;
	}
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        ((FilterElementClass *)(parent_class))->xml_create(fe, node);
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
	
	n = node->childs;
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
folder_selected (EvolutionFolderSelectorButton *button,
		 GNOME_Evolution_Folder *folder,
		 FilterFolder *ff)
{
	g_free (ff->uri);
	ff->uri = g_strdup (folder->physicalUri);
	
	gdk_window_raise (GTK_WIDGET (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW))->window);
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	static const char *allowed_types[] = { "mail", NULL };
	FilterFolder *ff = (FilterFolder *)fe;
	GtkWidget *button;
	
	button = evolution_folder_selector_button_new (global_shell_client,
						       _("Select Folder"),
						       ff->uri,
						       allowed_types);

	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "selected", folder_selected, ff);
	
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
