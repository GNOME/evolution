/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-select-names.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>

#include <gal/e-table/e-table-simple.h>
#include <gal/widgets/e-font.h>

#include <addressbook/gui/widgets/e-addressbook-model.h>
#include <addressbook/gui/widgets/e-addressbook-table-adapter.h>
#include <addressbook/gui/component/e-cardlist-model.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/gui/component/addressbook-component.h>
#include <shell/evolution-shell-client.h>

#include "e-select-names.h"
#include <addressbook/backend/ebook/e-card-simple.h>
#include "e-select-names-table-model.h"

static void e_select_names_init		(ESelectNames		 *card);
static void e_select_names_class_init	(ESelectNamesClass	 *klass);
static void e_select_names_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_select_names_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_destroy (GtkObject *object);

static GnomeDialogClass *parent_class = NULL;
#define PARENT_TYPE gnome_dialog_get_type()

/* The arguments we take */
enum {
	ARG_0,
};

typedef struct {
	char              *title;
	ETableModel       *model;
	ESelectNamesModel *source;
	ESelectNames      *names;
	GtkWidget         *label;
} ESelectNamesChild;

GtkType
e_select_names_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info =
		{
			"ESelectNames",
			sizeof (ESelectNames),
			sizeof (ESelectNamesClass),
			(GtkClassInitFunc) e_select_names_class_init,
			(GtkObjectInitFunc) e_select_names_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

static void
e_select_names_class_init (ESelectNamesClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_arg = e_select_names_set_arg;
	object_class->get_arg = e_select_names_get_arg;
	object_class->destroy = e_select_names_destroy;
}

#define SPEC "<ETableSpecification no-headers=\"true\" cursor-mode=\"line\"> \
  <ETableColumn model_col= \"%d\" _title=\"Name\"          expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
	<ETableState>                   			       \
		<column source=\"0\"/>     			       \
	        <grouping> <leaf column=\"0\" ascending=\"true\"/> </grouping> \
	</ETableState>                  			       \
</ETableSpecification>"

#define SPEC2 "<ETableSpecification no-headers=\"true\" cursor-mode=\"line\"> \
  <ETableColumn model_col= \"0\" _title=\"Name\"          expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\"       compare=\"string\"/> \
	<ETableState>                   			       \
		<column source=\"0\"/>     			       \
        	<grouping> <leaf column=\"0\" ascending=\"true\"/> </grouping> \
	</ETableState>                  			       \
</ETableSpecification>"

GtkWidget *e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2);

static void
set_book(EBook *book, EBookStatus status, EAddressbookModel *model)
{
	gtk_object_set(GTK_OBJECT(model),
		       "book", book,
		       NULL);
	gtk_object_unref(GTK_OBJECT(book));
}

static void
addressbook_model_set_uri(EAddressbookModel *model, char *uri)
{
	EBook *book;
	book = e_book_new();
	gtk_object_ref(GTK_OBJECT(model));
	gtk_object_ref(GTK_OBJECT(book));
	e_book_load_uri(book, uri, (EBookCallback) set_book, model);
}

static void
real_add_address_cb (int model_row,
		     gpointer closure)
{
	ESelectNamesChild *child = closure;
	ESelectNames *names = child->names;
	ECard *card = e_addressbook_model_get_card(E_ADDRESSBOOK_MODEL(names->model), model_row);
	EDestination *dest = e_destination_new ();

	e_destination_set_card (dest, card, 0);

	e_select_names_model_append (child->source, dest);
	e_select_names_model_clean (child->source);

	gtk_object_unref(GTK_OBJECT(card));
}

static void
real_add_address(ESelectNames *names, ESelectNamesChild *child)
{
	e_table_selected_row_foreach(e_table_scrolled_get_table(names->table),
				     real_add_address_cb, child);
}

static void
add_address(ETable *table, int row, int col, GdkEvent *event, ESelectNames *names)
{
	ESelectNamesChild *child;

	child = g_hash_table_lookup(names->children, names->def);
	if (child) {
		real_add_address(names, child);
	}
}

GtkWidget *
e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2)
{
	ETableModel *adapter;
	EAddressbookModel *model;
	GtkWidget *table;
	char *filename;
	char *uri;
	char *spec;

	model = e_addressbook_model_new ();
	adapter = E_TABLE_MODEL (e_addressbook_table_adapter_new(model));

	filename = gnome_util_prepend_user_home("evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf("file://%s", filename);

	addressbook_model_set_uri(model, uri);

	g_free(uri);
	g_free(filename);

	gtk_object_set(GTK_OBJECT(model),
		       "editable", FALSE,
		       "query", "(contains \"email\" \"\")",
		       NULL);

	spec = g_strdup_printf(SPEC, E_CARD_SIMPLE_FIELD_NAME_OR_ORG);
	table = e_table_scrolled_new (adapter, NULL, spec, NULL);
	g_free(spec);

	gtk_object_set_data(GTK_OBJECT(table), "adapter", adapter);
	gtk_object_set_data(GTK_OBJECT(table), "model", model);

	return table;
}

static void
set_current_selection(ETableScrolled *table, int row, ESelectNames *names)
{
	names->currently_selected = row;
}

typedef struct {
	char *description;
	char *display_name;
	char *physical_uri;

} ESelectNamesFolder;

static void
e_select_names_folder_free(ESelectNamesFolder *e_folder)
{
	g_free(e_folder->description );
	g_free(e_folder->display_name);
	g_free(e_folder->physical_uri);
	g_free(e_folder);
}

static void
e_select_names_option_activated(GtkWidget *widget, ESelectNames *e_select_names)
{
	ESelectNamesFolder *e_folder = gtk_object_get_data (GTK_OBJECT (widget), "EsnChoiceFolder");

	addressbook_model_set_uri(e_select_names->model, e_folder->physical_uri);
}

typedef struct {
	ESelectNames *names;
	GtkWidget *menu;
} NamesAndMenu;

static void
add_menu_item		(gpointer	key,
			 gpointer	value,
			 gpointer	user_data)
{
	GtkWidget *menu;
	GtkWidget *item;
	ESelectNamesFolder *e_folder;
	NamesAndMenu *nnm;
	ESelectNames *e_select_names;

	nnm = user_data;
	e_folder = value;
	menu = nnm->menu;
	e_select_names = nnm->names;

	item = gtk_menu_item_new_with_label (e_folder->display_name);
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_object_set_data (GTK_OBJECT (item), "EsnChoiceFolder", e_folder);

	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (e_select_names_option_activated),
			    e_select_names);
}

static void
update_option_menu(ESelectNames *e_select_names)
{
	GtkWidget *menu;
	GtkWidget *option;

	option = glade_xml_get_widget (e_select_names->gui,
				       "optionmenu-folder");
	if (option) {
		NamesAndMenu nnm;
		menu = gtk_menu_new ();

		nnm.names = e_select_names;
		nnm.menu = menu;

		g_hash_table_foreach	(e_select_names->folders,
					 add_menu_item,
					 &nnm);

		gtk_widget_show_all (menu);

		gtk_option_menu_set_menu (GTK_OPTION_MENU (option), 
					  menu);
		gtk_option_menu_set_history (GTK_OPTION_MENU (option), 0);
		gtk_widget_set_sensitive (option, TRUE);
	}
}

static void
new_folder      (EvolutionStorageListener *storage_listener,
		 const char *path,
		 const GNOME_Evolution_Folder *folder,
		 ESelectNames *e_select_names)
{
	if (!strcmp(folder->type, "contacts")) {
		ESelectNamesFolder *e_folder = g_new(ESelectNamesFolder, 1);
		e_folder->description  = g_strdup(folder->description );
		e_folder->display_name = g_strdup(folder->display_name);
		e_folder->physical_uri = g_strdup(folder->physical_uri);
		g_hash_table_insert(e_select_names->folders,
				    g_strdup(path), e_folder);
		update_option_menu(e_select_names);
	}
}

static void
update_folder   (EvolutionStorageListener *storage_listener,
		 const char *path,
		 const char *display_name,
		 ESelectNames *e_select_names)
{
	ESelectNamesFolder *e_folder = g_hash_table_lookup(e_select_names->folders, path);
	if (e_folder) {
		g_free(e_folder->display_name);
		e_folder->display_name = g_strdup(e_folder->display_name);
		update_option_menu(e_select_names);
	}
}

static void
removed_folder  (EvolutionStorageListener *storage_listener,
		 const char *path,
		 ESelectNames *e_select_names)
{
	ESelectNamesFolder *e_folder;
	char *orig_path;

	if (g_hash_table_lookup_extended(e_select_names->folders, path, (void **) &orig_path, (void **) &e_folder)) {
		g_hash_table_remove(e_select_names->folders, path);
		e_select_names_folder_free(e_folder);
		g_free(orig_path);
		update_option_menu(e_select_names);
	}
}

static void
e_select_names_hookup_shell_listener (ESelectNames *e_select_names)
{
	GNOME_Evolution_Storage storage;
	GNOME_Evolution_StorageListener listener;
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

	storage = (GNOME_Evolution_Storage) (evolution_shell_client_get_local_storage(addressbook_component_get_shell_client()));
	e_select_names->listener = evolution_storage_listener_new();

	listener = evolution_storage_listener_corba_objref(e_select_names->listener);

	gtk_signal_connect(GTK_OBJECT(e_select_names->listener), "new_folder",
			   GTK_SIGNAL_FUNC(new_folder), e_select_names);
	gtk_signal_connect(GTK_OBJECT(e_select_names->listener), "update_folder",
			   GTK_SIGNAL_FUNC(update_folder), e_select_names);
	gtk_signal_connect(GTK_OBJECT(e_select_names->listener), "removed_folder",
			   GTK_SIGNAL_FUNC(removed_folder), e_select_names);

	GNOME_Evolution_Storage_addListener(storage, listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_select_names_init: Exception adding listener to "
			   "remote GNOME_Evolution_Storage interface.\n");
		CORBA_exception_free (&ev);
		return;
	}

	bonobo_object_release_unref(storage, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("e_select_names_init: Exception unref'ing "
			   "remote GNOME_Evolution_Storage interface.\n");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free(&ev);
}

static void
e_select_names_init (ESelectNames *e_select_names)
{
	GladeXML *gui;
	GtkWidget *widget;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/select-names.glade", NULL);
	e_select_names->gui = gui;

	e_select_names->children = g_hash_table_new(g_str_hash, g_str_equal);
	e_select_names->child_count = 0;
	e_select_names->def = NULL;

	widget = glade_xml_get_widget(gui, "table-top");
	if (!widget) {
		return;
	}
	gtk_widget_ref(widget);
	gtk_widget_unparent(widget);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(e_select_names)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref(widget);

	gnome_dialog_append_buttons(GNOME_DIALOG(e_select_names),
				    GNOME_STOCK_BUTTON_OK,
				    GNOME_STOCK_BUTTON_CANCEL,
				    NULL);
	
	gtk_window_set_policy(GTK_WINDOW(e_select_names), FALSE, TRUE, FALSE);

	e_select_names->table = E_TABLE_SCROLLED(glade_xml_get_widget(gui, "table-source"));
	e_select_names->model = gtk_object_get_data(GTK_OBJECT(e_select_names->table), "model");
	e_select_names->adapter = gtk_object_get_data(GTK_OBJECT(e_select_names->table), "adapter");

	e_select_names->currently_selected = -1;

	e_select_names->folders = g_hash_table_new(g_str_hash, g_str_equal);

	e_select_names_hookup_shell_listener (e_select_names);

	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(e_select_names->table)), "cursor_activated",
			   GTK_SIGNAL_FUNC(set_current_selection), e_select_names);

	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(e_select_names->table)), "double_click",
			   GTK_SIGNAL_FUNC(add_address), e_select_names);
}

static void e_select_names_child_free(char *key, ESelectNamesChild *child, ESelectNames *e_select_names)
{
	g_free(child->title);
	gtk_object_unref(GTK_OBJECT(child->model));
	gtk_object_unref(GTK_OBJECT(child->source));
	g_free(key);
}

static void
e_select_names_destroy (GtkObject *object) {
	ESelectNames *e_select_names = E_SELECT_NAMES(object);
	
	gtk_signal_disconnect_by_data(GTK_OBJECT(e_select_names->listener), e_select_names);
	gtk_object_unref(GTK_OBJECT(e_select_names->listener));

	gtk_object_unref(GTK_OBJECT(e_select_names->gui));
	g_hash_table_foreach(e_select_names->children, (GHFunc) e_select_names_child_free, e_select_names);
	g_hash_table_destroy(e_select_names->children);

	g_free(e_select_names->def);
}

GtkWidget*
e_select_names_new (void)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_select_names_get_type ()));
	return widget;
}

static void
e_select_names_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ESelectNames *editor;

	editor = E_SELECT_NAMES (o);
	
	switch (arg_id){
	default:
		return;
	}
}

static void
e_select_names_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNames *e_select_names;

	e_select_names = E_SELECT_NAMES (object);

	switch (arg_id) {
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
button_clicked(GtkWidget *button, ESelectNamesChild *child)
{
	real_add_address(child->names, child);
}

static void
remove_address(ETable *table, int row, int col, GdkEvent *event, ESelectNamesChild *child)
{
	e_select_names_model_delete (child->source, row);
}

void
e_select_names_add_section(ESelectNames *e_select_names, char *name, char *id, ESelectNamesModel *source)
{
	ESelectNamesChild *child;
	GtkWidget *button;
	GtkWidget *alignment;
	GtkWidget *label;
	GtkTable *table;
	char *label_text;

	ETableModel *model;
	GtkWidget *etable;

	if (g_hash_table_lookup(e_select_names->children, id)) {
		return;
	}

	table = GTK_TABLE(glade_xml_get_widget (e_select_names->gui, "table-recipients"));

	child = g_new(ESelectNamesChild, 1);

	child->names = e_select_names;
	child->title = g_strdup(_(name));

	e_select_names->child_count++;

	alignment = gtk_alignment_new(0, 0, 1, 0);

	button = gtk_button_new ();

	label_text = g_strconcat (child->title, " ->", NULL);
	label = gtk_label_new (label_text);
	g_free (label_text);
	gtk_container_add (GTK_CONTAINER (button), label);
	child->label = label;

	gtk_container_add(GTK_CONTAINER(alignment), button);
	gtk_widget_show_all(alignment);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(button_clicked), child);
	gtk_table_attach(table, alignment,
			 0, 1,
			 e_select_names->child_count,
			 e_select_names->child_count + 1,
			 GTK_FILL, GTK_FILL,
			 0, 0);
	
	model = e_select_names_table_model_new(source);
	etable = e_table_scrolled_new (model, NULL, SPEC2, NULL);
	
	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(E_TABLE_SCROLLED(etable))), "double_click",
			   GTK_SIGNAL_FUNC(remove_address), child);

	child->model = model;
	child->source = source;
	gtk_object_ref(GTK_OBJECT(child->model));
	gtk_object_ref(GTK_OBJECT(child->source));
	
	gtk_widget_show(etable);
	
	gtk_table_attach(table, etable,
			 1, 2,
			 e_select_names->child_count,
			 e_select_names->child_count + 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	g_hash_table_insert(e_select_names->children, g_strdup(id), child);
}

static void *
card_copy(const void *value, void *closure)
{
	gtk_object_ref(GTK_OBJECT(value));
	return (void *)value;
}

static void
card_free(void *value, void *closure)
{
	gtk_object_unref(GTK_OBJECT(value));
}

EList *
e_select_names_get_section(ESelectNames *e_select_names, char *id)
{
	ESelectNamesChild *child;
	int i;
	int rows;
	EList *list;
	
	child = g_hash_table_lookup(e_select_names->children, id);
	if (!child)
		return NULL;
	rows = e_table_model_row_count(child->model);
	
	list = e_list_new(card_copy, card_free, NULL);
	for (i = 0; i < rows; i++) {
		ECard *card = e_cardlist_model_get(E_CARDLIST_MODEL(child->model), i);
		e_list_append(list, card);
		gtk_object_unref(GTK_OBJECT(card));
	}
	return list;
}

ESelectNamesModel *
e_select_names_get_source(ESelectNames *e_select_names,
			  char *id)
{
	ESelectNamesChild *child = g_hash_table_lookup(e_select_names->children, id);
	if (child) {
		if (child->source)
			gtk_object_ref(GTK_OBJECT(child->source));
		return child->source;
	} else
		return NULL;
}

void
e_select_names_set_default (ESelectNames *e_select_names,
			    const char *id)
{
	ESelectNamesChild *child;

	if (e_select_names->def) {
		child = g_hash_table_lookup(e_select_names->children, e_select_names->def);
		if (child)
			gtk_widget_restore_default_style(child->label);
	}

	g_free(e_select_names->def);
	e_select_names->def = g_strdup(id);

	if (e_select_names->def) {
		child = g_hash_table_lookup(e_select_names->children, e_select_names->def);
		if (child) {
			EFont *efont;
			GdkFont *gdkfont;
			GtkStyle *style, *oldstyle;

			oldstyle = gtk_widget_get_style(child->label);
			style = gtk_style_copy(oldstyle);

			efont = e_font_from_gdk_font(style->font);
			gdkfont = e_font_to_gdk_font(efont, E_FONT_BOLD);
			e_font_unref(efont);

			gdk_font_ref(gdkfont);
			gdk_font_unref(style->font);
			style->font = gdkfont;

			gtk_widget_set_style(child->label, style);

			gtk_style_unref(oldstyle);
		}
	}
}
