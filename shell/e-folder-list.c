/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-folder-list.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <gtk/gtkframe.h>

#include <libgnome/gnome-i18n.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <glade/glade.h>

#include <gal/e-table/e-table-memory-store.h>
#include <gal/util/e-xml-utils.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-option-menu.h>

#include "e-folder-list.h"

#include "Evolution.h"

static GtkVBoxClass *parent_class = NULL;
#define PARENT_TYPE (gtk_vbox_get_type ())

enum {
	CHANGED,
	OPTION_MENU_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

/* The arguments we take */
enum {
	ARG_0,
	ARG_TITLE,
	ARG_POSSIBLE_TYPES,
};

struct _EFolderListPrivate {
	GladeXML *gui;

	ETableScrolled *scrolled_table;
	char *title;
	GtkFrame *frame;
	ETableMemoryStore *model;
	EvolutionShellClient *client;
	GNOME_Evolution_StorageRegistry corba_storage_registry;
	EOptionMenu *option_menu;

	char **possible_types;
};


static GNOME_Evolution_Folder *
get_folder_for_uri (EFolderList *efl,
		    const char *uri)
{
	EFolderListPrivate *priv = efl->priv;
	CORBA_Environment ev;
	GNOME_Evolution_Folder *folder;

	CORBA_exception_init (&ev);
	folder = GNOME_Evolution_StorageRegistry_getFolderByUri (
		priv->corba_storage_registry, uri, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		folder = CORBA_OBJECT_NIL;
	CORBA_exception_free (&ev);

	return folder;
}

static char *
create_display_string (EFolderList *efl, char *folder_uri, char *folder_name)
{
	char *storage_lname, *p;
	char *label_text;

	storage_lname = NULL;
	p = strchr (folder_uri, '/');
	if (p) {
		p = strchr (p + 1, '/');
		if (p) {
			GNOME_Evolution_Folder *storage_folder;
			char *storage_uri;

			storage_uri = g_strndup (folder_uri,
						 p - folder_uri);
			storage_folder = get_folder_for_uri (efl, storage_uri);
			storage_lname = g_strdup (storage_folder->displayName);
			CORBA_free (storage_folder);
			g_free (storage_uri);
		}
	}

	if (storage_lname) {
		label_text = g_strdup_printf (_("\"%s\" in \"%s\""), folder_name,
					      storage_lname);
		g_free (storage_lname);
	} else
		label_text = g_strdup_printf ("\"%s\"", folder_name);

	return label_text;
}



static void
e_folder_list_changed (EFolderList *efl)
{
	gtk_signal_emit (GTK_OBJECT (efl), signals[CHANGED]);
}

static void
e_folder_list_destroy (GtkObject *object)
{
	EFolderList *efl = E_FOLDER_LIST (object);

	if (efl->priv->gui)
		gtk_object_unref (GTK_OBJECT (efl->priv->gui));

	if (efl->priv->client)
		g_object_unref (efl->priv->client);

	g_free (efl->priv);
	efl->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
set_frame_label (EFolderList *efl)
{
	GtkFrame *frame = efl->priv->frame;
	char *title = efl->priv->title;
	if (frame) {
		if (title) {
			gtk_frame_set_label (frame, title);
			gtk_frame_set_shadow_type (frame, GTK_SHADOW_ETCHED_IN);
		} else {
			gtk_frame_set_label (frame, "");
			gtk_frame_set_shadow_type (frame, GTK_SHADOW_NONE);
		}
	}
}

static void
e_folder_list_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EFolderList *efl = E_FOLDER_LIST(object);

	switch (arg_id) {
	case ARG_TITLE:
		g_free (efl->priv->title);
		efl->priv->title = g_strdup (GTK_VALUE_STRING (*arg));
		set_frame_label (efl);
		break;
	case ARG_POSSIBLE_TYPES:
		g_strfreev (efl->priv->possible_types);
		efl->priv->possible_types = e_strdupv (GTK_VALUE_POINTER (*arg));
		break;
	default:
		break;
	}
}

static void
e_folder_list_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EFolderList *efl = E_FOLDER_LIST(object);

	switch (arg_id) {
	case ARG_TITLE:
		GTK_VALUE_STRING (*arg) = g_strdup (efl->priv->title);
		break;
	case ARG_POSSIBLE_TYPES:
		GTK_VALUE_POINTER (*arg) = e_strdupv ((const gchar **) efl->priv->possible_types);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_folder_list_class_init (EFolderListClass *klass)
{
	GtkObjectClass *object_class;
	GtkVBoxClass *vbox_class;

	object_class               = (GtkObjectClass*) klass;
	vbox_class                 = (GtkVBoxClass *) klass;

	glade_gnome_init();

	parent_class               = gtk_type_class (PARENT_TYPE);

	object_class->set_arg      = e_folder_list_set_arg;
	object_class->get_arg      = e_folder_list_get_arg;
	object_class->destroy      = e_folder_list_destroy;

	klass->changed             = NULL;
	klass->option_menu_changed = NULL;

	signals [OPTION_MENU_CHANGED] =
		gtk_signal_new ("option_menu_changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (EFolderListClass, option_menu_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	signals [CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (EFolderListClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("EFolderList::title", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_TITLE);
	gtk_object_add_arg_type ("EFolderList::possible_types", GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE, ARG_POSSIBLE_TYPES);
}

#define SPEC 	"<ETableSpecification cursor-mode=\"line\"" \
	"		      selection-mode=\"browse\"" \
	"                     no-headers=\"true\"" \
        ">" \
	"  <ETableColumn model_col=\"0\"" \
	"	         expansion=\"0.0\"" \
	"                cell=\"pixbuf\"" \
 	"                minimum_width=\"18\"" \
	"                resizable=\"false\"" \
	"	         _title=\"icon\"" \
	"                compare=\"string\"" \
        "                search=\"string\"/>" \
	"  <ETableColumn model_col=\"1\"" \
	"	         expansion=\"1.0\"" \
	"                cell=\"string\"" \
 	"                minimum_width=\"32\"" \
	"                resizable=\"true\"" \
	"	         _title=\"blah\"" \
	"                compare=\"string\"" \
        "                search=\"string\"/>" \
	"  <ETableState>" \
	"    <column source=\"0\"/>" \
	"    <column source=\"1\"/>" \
	"    <grouping>" \
	"    </grouping>" \
	"  </ETableState>" \
	"</ETableSpecification>"


static ETableMemoryStoreColumnInfo columns[] = {
	E_TABLE_MEMORY_STORE_PIXBUF,
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_STRING,
	E_TABLE_MEMORY_STORE_TERMINATOR
};

GtkWidget *
create_custom_optionmenu (char *name, char *string1, char *string2, int int1, int int2);

GtkWidget *
create_custom_optionmenu (char *name, char *string1, char *string2, int int1, int int2)
{
	return e_option_menu_new (NULL);
}

GtkWidget *
create_custom_folder_list (char *name, char *string1, char *string2, int int1, int int2);

GtkWidget *
create_custom_folder_list (char *name, char *string1, char *string2, int int1, int int2)
{
	ETableModel *model;
	GtkWidget *scrolled;
	model = e_table_memory_store_new (columns);
	scrolled = e_table_scrolled_new (model, NULL, SPEC, NULL);
	gtk_object_set_data (GTK_OBJECT (scrolled), "table-model", model);
	return scrolled;
}

static void
add_clicked (GtkButton *button, EFolderList *efl)
{
	GNOME_Evolution_Folder *folder;

	evolution_shell_client_user_select_folder (efl->priv->client,
						   GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (efl))),
						   _("Add a Folder"),
						   "",
						   (const gchar **) efl->priv->possible_types,
						   &folder);

	if (folder != NULL) {
		GdkPixbuf *pixbuf;
		char *display_string = create_display_string (efl, folder->evolutionUri, folder->displayName);

		pixbuf = evolution_shell_client_get_pixbuf_for_type (efl->priv->client, folder->type, TRUE);
		e_table_memory_store_insert (efl->priv->model, -1, NULL, pixbuf, display_string,
					     folder->displayName, folder->evolutionUri, folder->physicalUri);
		e_folder_list_changed (efl);
		gdk_pixbuf_unref (pixbuf);
		g_free (display_string);
	}
}

static void
make_list (int model_row, gpointer closure)
{
	GList **list = closure;
	*list = g_list_prepend (*list, GINT_TO_POINTER (model_row));
}

static void
remove_row (gpointer data, gpointer user_data)
{
	ETableMemoryStore *etms = user_data;
	int row = GPOINTER_TO_INT (data);

	e_table_memory_store_remove (etms, row);
}

static void
remove_clicked (GtkButton *button, EFolderList *efl)
{
	GList *list = NULL;
	ETable *table;

	table = e_table_scrolled_get_table (efl->priv->scrolled_table);

	e_table_selected_row_foreach (table, make_list, &list);

	g_list_foreach (list, remove_row, efl->priv->model);

	g_list_free (list);

	e_folder_list_changed (efl);
}

static void
optionmenu_changed (EOptionMenu *option_menu, int value, EFolderList *efl)
{
	gtk_signal_emit (GTK_OBJECT (efl), signals[OPTION_MENU_CHANGED], value);
}

static void
update_buttons (EFolderList *efl)
{
	int cursor_row;
	int selection_count;
	ETable *table;

	table = e_table_scrolled_get_table (efl->priv->scrolled_table);
	cursor_row = e_table_get_cursor_row (table);
	selection_count = e_table_selected_count (table);
	
	e_glade_xml_set_sensitive (efl->priv->gui, "button-remove", selection_count >= 1);
}

static void
cursor_changed (ESelectionModel *selection_model, int row, int col, EFolderList *efl)
{
	update_buttons (efl);
}

static void
selection_changed (ESelectionModel *selection_model, EFolderList *efl)
{
	update_buttons (efl);
}

static void
e_folder_list_init (EFolderList *efl)
{
	GladeXML *gui;
	ESelectionModel *selection_model;

	efl->priv = g_new (EFolderListPrivate, 1);

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/e-folder-list.glade", NULL, NULL);
	efl->priv->gui = gui;

	efl->priv->title = NULL;
	efl->priv->frame = GTK_FRAME (glade_xml_get_widget(gui, "frame-toplevel"));
	if (efl->priv->frame == NULL)
		return;

	gtk_widget_reparent(GTK_WIDGET (efl->priv->frame),
			    GTK_WIDGET (efl));

	efl->priv->option_menu = E_OPTION_MENU(glade_xml_get_widget (gui, "custom-optionmenu"));
	e_folder_list_set_option_menu_strings (efl, NULL);
	efl->priv->scrolled_table = E_TABLE_SCROLLED(glade_xml_get_widget(gui, "custom-folder-list"));
	efl->priv->model = E_TABLE_MEMORY_STORE (gtk_object_get_data (GTK_OBJECT (efl->priv->scrolled_table), "table-model"));

	e_glade_xml_connect_widget (gui, "button-add", "clicked",
				    GTK_SIGNAL_FUNC (add_clicked), efl);
	e_glade_xml_connect_widget (gui, "button-remove", "clicked",
				    GTK_SIGNAL_FUNC (remove_clicked), efl);
	e_glade_xml_connect_widget (gui, "custom-optionmenu", "changed",
				    GTK_SIGNAL_FUNC (optionmenu_changed), efl);

	selection_model = e_table_get_selection_model (e_table_scrolled_get_table (efl->priv->scrolled_table));

	gtk_signal_connect (GTK_OBJECT (selection_model), "selection_changed",
			    GTK_SIGNAL_FUNC (selection_changed), efl);
	gtk_signal_connect (GTK_OBJECT (selection_model), "cursor_changed",
			    GTK_SIGNAL_FUNC (cursor_changed), efl);

	efl->priv->possible_types = NULL;
	set_frame_label (efl);
}

EFolderListItem *
e_folder_list_parse_xml (char *xml)
{
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *node;
	int i;
	EFolderListItem *items;

	if (xml == NULL || *xml == 0) {
		items = g_new (EFolderListItem, 1);
		items[0].uri = NULL;
		items[0].physical_uri = NULL;
		items[0].display_name = NULL;
		return items;
	}

	doc = xmlParseMemory (xml, strlen (xml));

	root = xmlDocGetRootElement (doc);

	for (node = root->xmlChildrenNode, i = 0; node; node = node->next, i++)
		/* Intentionally empty */;

	items = g_new (EFolderListItem, i + 1);

	for (node = root->xmlChildrenNode, i = 0; node; node = node->next, i++) {
		items[i].uri = e_xml_get_string_prop_by_name_with_default (node, "uri", "");
		items[i].physical_uri = e_xml_get_string_prop_by_name_with_default (node, "physical-uri", "");
		items[i].display_name = e_xml_get_string_prop_by_name_with_default (node, "display-name", "");
	}
	items[i].uri = NULL;
	items[i].physical_uri = NULL;
	items[i].display_name = NULL;

	xmlFreeDoc (doc);

	return items;
}

char *
e_folder_list_create_xml (EFolderListItem *items)
{
	xmlDoc *doc;
	xmlNode *root;
	char *xml;
	xmlChar *temp;
	int length;
	int i;

	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "EvolutionFolderList", NULL);

	xmlDocSetRootElement (doc, root);

	for (i = 0; items[i].uri; i++) {
		xmlNode *node = xmlNewChild (root, NULL, "folder", NULL);
		e_xml_set_string_prop_by_name (node, "uri", items[i].uri);
		e_xml_set_string_prop_by_name (node, "physical-uri", items[i].physical_uri);
		e_xml_set_string_prop_by_name (node, "display-name", items[i].display_name);
	}

	xmlDocDumpMemory (doc, &temp, &length);

	xml = g_strdup (temp);

	xmlFree (temp);
	xmlFreeDoc (doc);

	return xml;
}

void
e_folder_list_free_items (EFolderListItem *items)
{
	int i;
	for (i = 0; items[i].uri; i++) {
		g_free (items[i].uri);
		g_free (items[i].physical_uri);
		g_free (items[i].display_name);
	}
	g_free (items);
}

GtkType
e_folder_list_get_type (void)
{
	static GtkType folder_list_type = 0;

	if (!folder_list_type)
		{
			static const GtkTypeInfo folder_list_info =
			{
				"EFolderList",
				sizeof (EFolderList),
				sizeof (EFolderListClass),
				(GtkClassInitFunc) e_folder_list_class_init,
				(GtkObjectInitFunc) e_folder_list_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
				(GtkClassInitFunc) NULL,
			};

			folder_list_type = gtk_type_unique (PARENT_TYPE, &folder_list_info);
		}

	return folder_list_type;
}

GtkWidget*
e_folder_list_new (EvolutionShellClient *client, char *xml)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_folder_list_get_type ()));
	e_folder_list_construct (E_FOLDER_LIST (widget), client, xml);
	return widget;
}

GtkWidget*
e_folder_list_construct (EFolderList *efl, EvolutionShellClient *client, char *xml)
{
	g_object_ref (client);
	efl->priv->client = client;

	efl->priv->corba_storage_registry = evolution_shell_client_get_storage_registry_interface (client);
	e_folder_list_set_xml (efl, xml);
	return GTK_WIDGET (efl);
}

void
e_folder_list_set_items (EFolderList *efl, EFolderListItem *items)
{
	int i;
	e_table_memory_store_clear (efl->priv->model);
	for (i = 0; items[i].uri; i++) {
		GNOME_Evolution_Folder *folder;
		GdkPixbuf *pixbuf;
		char *display_string;

		folder = get_folder_for_uri (efl, items[i].uri);
		if (!folder)
			continue;
		display_string = create_display_string (efl, items[i].uri, items[i].display_name);

		pixbuf = evolution_shell_client_get_pixbuf_for_type (efl->priv->client, folder->type, TRUE);
		
		e_table_memory_store_insert (efl->priv->model, -1, NULL,
					     pixbuf, display_string,
					     items[i].display_name, items[i].uri, items[i].physical_uri);
		CORBA_free (folder);
		gdk_pixbuf_unref (pixbuf);
		g_free (display_string);
	}
}

EFolderListItem *
e_folder_list_get_items (EFolderList *efl)
{
	EFolderListItem *items;
	int count;
	int i;

	count = e_table_model_row_count (E_TABLE_MODEL (efl->priv->model));

	items = g_new (EFolderListItem, count + 1);

	for (i = 0; i < count; i++) {
		items[i].display_name = g_strdup (e_table_model_value_at (E_TABLE_MODEL (efl->priv->model), 2, i));
		items[i].uri = g_strdup (e_table_model_value_at (E_TABLE_MODEL (efl->priv->model), 3, i));
		items[i].physical_uri = g_strdup (e_table_model_value_at (E_TABLE_MODEL (efl->priv->model), 4, i));
	}
	items[i].uri = NULL;
	items[i].physical_uri = NULL;

	return items;
}

void
e_folder_list_set_xml (EFolderList *efl, char *xml)
{
	EFolderListItem *items;

	items = e_folder_list_parse_xml (xml);
	e_folder_list_set_items (efl, items);
	e_folder_list_free_items (items);
}

char *
e_folder_list_get_xml (EFolderList *efl)
{
	EFolderListItem *items;
	char *xml;

	items = e_folder_list_get_items (efl);
	xml = e_folder_list_create_xml (items);
	e_folder_list_free_items (items);

	return xml;
}

void
e_folder_list_set_option_menu_strings_from_array (EFolderList *efl, gchar **strings)
{
	e_option_menu_set_strings_from_array (efl->priv->option_menu, strings);
	if (strings && *strings)
		gtk_widget_show (GTK_WIDGET (efl->priv->option_menu));
	else
		gtk_widget_hide (GTK_WIDGET (efl->priv->option_menu));
}

void
e_folder_list_set_option_menu_strings (EFolderList *efl, gchar *first_label, ...)
{
	char **array;

	GET_STRING_ARRAY_FROM_ELLIPSIS (array, first_label);

	e_folder_list_set_option_menu_strings_from_array (efl, array);

	g_free (array);
}

int
e_folder_list_get_option_menu_value (EFolderList *efl)
{
	return e_option_menu_get_value (efl->priv->option_menu);
}

void
e_folder_list_set_option_menu_value (EFolderList *efl, int value)
{
	gtk_option_menu_set_history (GTK_OPTION_MENU (efl->priv->option_menu), value);
}
