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
#include <gnome.h>
#include "e-select-names.h"
#include <e-table-simple.h>
#include <e-table.h>
#include <e-cell-text.h>
#include <addressbook/gui/component/e-addressbook-model.h>
#include <addressbook/gui/component/e-cardlist-model.h>
#include <addressbook/backend/ebook/e-book.h>
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
	ARG_BOOK,
	ARG_QUERY,
};

typedef struct {
	char         *title;
	ETableModel  *model;
	ESelectNamesModel *source;
	ESelectNames *names;
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

	gtk_object_add_arg_type ("ESelectNames::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("ESelectNames::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);

	object_class->set_arg = e_select_names_set_arg;
	object_class->get_arg = e_select_names_get_arg;
	object_class->destroy = e_select_names_destroy;
}

#define SPEC "<ETableSpecification no-header=\"1\">    	       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
                <column> 2 </column>                                   \
	</columns-shown>                 			       \
	<grouping> <leaf column=\"0\" ascending=\"1\"/> </grouping>    \
</ETableSpecification>"

#define SPEC2 "<ETableSpecification no-header=\"1\">    	       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
                <column> 2 </column>                                   \
	</columns-shown>                 			       \
	<grouping> </grouping>                                         \
</ETableSpecification>"

GtkWidget *e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2);

static void
set_book(EBook *book, EBookStatus status, ETableModel *model)
{
	gtk_object_set(GTK_OBJECT(model),
		       "book", book,
		       NULL);
	gtk_object_unref(GTK_OBJECT(book));
}

GtkWidget *
e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2)
{
	ETableModel *model;
	ETableHeader *header;
	ECell *cell_left_just;
	EBook *book;
	GtkWidget *table;
	char *filename;
	char *uri;

	model = e_addressbook_model_new();
	cell_left_just = e_cell_text_new (model, NULL, GTK_JUSTIFY_LEFT);

	header = e_table_header_new ();
	e_table_header_add_column (header, e_table_col_new (0, "Full Name", 1.0, 20, cell_left_just,
							    g_str_compare, TRUE), 0);

	book = e_book_new();
	gtk_object_ref(GTK_OBJECT(model));
	gtk_object_ref(GTK_OBJECT(book));
	filename = gnome_util_prepend_user_home("evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf("file://%s", filename);
	e_book_load_uri(book, uri, (EBookCallback) set_book, model);
	g_free(uri);
	g_free(filename);
	table = e_table_new (header, model, SPEC);

	gtk_object_set(GTK_OBJECT(table),
		       "cursor_mode", E_TABLE_CURSOR_LINE,
		       NULL);

	gtk_object_set_data(GTK_OBJECT(table), "model", model);
	return table;
}

static void
set_current_selection(ETable *table, int row, ESelectNames *names)
{
	names->currently_selected = row;
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

	e_select_names->table = E_TABLE(glade_xml_get_widget(gui, "table-source"));
	e_select_names->model = gtk_object_get_data(GTK_OBJECT(e_select_names->table), "model");

	e_select_names->currently_selected = -1;

	gtk_signal_connect(GTK_OBJECT(e_select_names->table), "cursor_change",
			   GTK_SIGNAL_FUNC(set_current_selection), e_select_names);
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
	
	gtk_object_unref(GTK_OBJECT(e_select_names->gui));
	g_hash_table_foreach(e_select_names->children, (GHFunc) e_select_names_child_free, e_select_names);
	g_hash_table_destroy(e_select_names->children);
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
	ESelectNames *names = child->names;
	int row = names->currently_selected;
	if (row != -1) {
		ECard *card = e_addressbook_model_get_card(E_ADDRESSBOOK_MODEL(names->model), row);
		ESelectNamesModelData new = {E_SELECT_NAMES_MODEL_DATA_TYPE_CARD,
					     card,
					     NULL};
		char *name, *email;
		ECardSimple *simple = e_card_simple_new(card);
		EIterator *iterator;

		name = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_FULL_NAME);
		email = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_EMAIL);
		if (name && *name && email && *email) {
			new.string = g_strdup_printf("%s <%s>", name, email);
		} else if (email && *email) {
			new.string = g_strdup_printf("%s", email);
		} else if (name && *name) {
			new.string = g_strdup_printf("%s <>", name);
		} else {
			new.string = g_strdup("");
		}

		iterator = e_list_get_iterator(e_select_names_model_get_data(child->source));
		e_iterator_last(iterator);
		e_select_names_model_add_item(child->source, iterator, &new);

		gtk_object_unref(GTK_OBJECT(simple));
		gtk_object_unref(GTK_OBJECT(card));
		g_free(email);
		g_free(name);
		g_free(new.string);
	}
}

void
e_select_names_add_section(ESelectNames *e_select_names, char *name, char *id, ESelectNamesModel *source)
{
	ESelectNamesChild *child;
	GtkWidget *button;
	GtkWidget *alignment;
	GtkTable *table;
	char *label;

	ETableModel *model;
	GtkWidget *etable;
	ETableHeader *header;
	ECell *cell_left_just;

	if (g_hash_table_lookup(e_select_names->children, id)) {
		return;
	}

	table = GTK_TABLE(glade_xml_get_widget (e_select_names->gui, "table-recipients"));

	child = g_new(ESelectNamesChild, 1);

	child->names = e_select_names;

	e_select_names->child_count++;

	alignment = gtk_alignment_new(0, 0, 1, 0);
	label = g_strdup_printf("%s ->", _(name));
	button = gtk_button_new_with_label(label);
	g_free(label);
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
	header = e_table_header_new ();
	cell_left_just = e_cell_text_new (model, NULL, GTK_JUSTIFY_LEFT);
	e_table_header_add_column (header, e_table_col_new (0, "Full Name", 1.0, 20, cell_left_just,
							    g_str_compare, TRUE), 0);
	etable = e_table_new (header, model, SPEC2);
	
	gtk_object_set(GTK_OBJECT(etable),
		       "cursor_mode", E_TABLE_CURSOR_LINE,
		       NULL);

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
