/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
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

#include <config.h>

#define SHELL

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <gal/widgets/e-unicode.h>

#include "filter-folder.h"
#ifdef SHELL
#include "shell/evolution-shell-client.h"
#endif
#include "e-util/e-sexp.h"

#define d(x)

static gboolean validate (FilterElement *fe);
static void xml_create(FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode(FilterElement *fe);
static int xml_decode(FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget(FilterElement *fe);
static void build_code(FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp(FilterElement *, GString *);

#ifdef SHELL
extern EvolutionShellClient *global_shell_client;
#endif

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
	g_free (o->name);
	
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

static gboolean
validate (FilterElement *fe)
{
	FilterFolder *ff = (FilterFolder *) fe;
	
	if (ff->uri && *ff->uri) {
		return TRUE;
	} else {
		GtkWidget *dialog;
		
		dialog = gnome_ok_dialog (_("Oops, you forgot to choose a folder.\n"
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
	xmlSetProp (work, "name", ff->name);
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
			xmlFree (ff->name);
			xmlFree (ff->uri);
			ff->name = xmlGetProp (n, "name");
			ff->uri = xmlGetProp (n, "uri");
			break;
		}
		n = n->next;
	}
	
	return 0;
}

static void
button_clicked (GtkButton *button, FilterFolder *ff)
{
#ifdef SHELL
	const char *allowed_types[] = { "mail", NULL };
	char *def, *physical_uri, *evolution_uri;
	static gboolean is_active = FALSE;
	gchar *s;
	
	if (is_active)
		return;
	
	is_active = TRUE;
	
	def = ff->uri ? ff->uri : "";
	
	evolution_shell_client_user_select_folder (global_shell_client,
						   _("Select Folder"),
						   def, allowed_types,
						   &evolution_uri,
						   &physical_uri);
	
	if (physical_uri != NULL && physical_uri[0] != '\0') {
		g_free (ff->uri);
		ff->uri = physical_uri;
		
		g_free (ff->name);
		ff->name = g_strdup (g_basename (evolution_uri));
		s = e_utf8_to_gtk_string (GTK_WIDGET (button), ff->name);
		gtk_label_set_text (GTK_LABEL (GTK_BIN (button)->child), s);
		g_free (s);
	} else {
		g_free (physical_uri);
	}
	g_free (evolution_uri);
	
	is_active = FALSE;
#else
	GnomeDialog *gd;
	GtkEntry *entry;
	char *uri, *str;

	gd = (GnomeDialog *)gnome_dialog_new(_("Enter folder URI"),
					     GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL,
					     NULL);
	gtk_window_set_policy(GTK_WINDOW(gd), FALSE, TRUE, FALSE);
	entry = (GtkEntry *)gtk_entry_new();
	if (ff->uri) {
		e_utf8_gtk_entry_set_text(entry, ff->uri);
	}
	gtk_box_pack_start((GtkBox *)gd->vbox, (GtkWidget *)entry, TRUE, TRUE, 3);
	gtk_widget_show((GtkWidget *)entry);
	switch (gnome_dialog_run(gd)) {
	case 0:
		g_free(ff->uri);
		g_free(ff->name);
		uri = e_utf8_gtk_entry_get_text(entry);
		ff->uri = uri;
		str = strstr(uri, "//");
		if (str)
			str = strchr(str+2, '/');
		if (str)
			str++;
		else
			str = uri;
		ff->name = g_strdup(str);
		s = e_utf8_to_gtk_string ((GtkWidget *) button, ff->name);
		gtk_label_set_text((GtkLabel *)GTK_BIN(button)->child, s);
		g_free (s);
	case 1:
		gnome_dialog_close(gd);
	case -1:
		/* nothing */
	}

#endif
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterFolder *ff = (FilterFolder *)fe;
	GtkWidget *button;
	GtkWidget *label;
	
	if (ff->name && ff->name[0])
		label = gtk_label_new (g_basename (ff->name));
	else
		label = gtk_label_new (_("<click here to select a folder>"));
	
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_widget_show (button);
	gtk_widget_show (label);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", button_clicked, ff);
	
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
