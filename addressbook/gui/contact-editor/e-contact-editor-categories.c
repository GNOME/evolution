/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-editor-categories.c
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
#include <e-contact-editor-categories.h>
#include <e-table-scrolled.h>
#include <e-table.h>
#include <e-table-simple.h>
#include <e-cell-text.h>
#include <e-cell-checkbox.h>
#include <e-util/e-unicode.h>

static void e_contact_editor_categories_init		(EContactEditorCategories		 *card);
static void e_contact_editor_categories_class_init	(EContactEditorCategoriesClass	 *klass);
static void e_contact_editor_categories_set_arg         (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_contact_editor_categories_get_arg         (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_contact_editor_categories_destroy         (GtkObject *object);
static int e_contact_editor_categories_col_count (ETableModel *etc, gpointer data);
static int e_contact_editor_categories_row_count (ETableModel *etc, gpointer data);
static void *e_contact_editor_categories_value_at (ETableModel *etc, int col, int row, gpointer data);
static void e_contact_editor_categories_set_value_at (ETableModel *etc, int col, int row, const void *val, gpointer data);
static gboolean e_contact_editor_categories_is_cell_editable (ETableModel *etc, int col, int row, gpointer data);
static void *e_contact_editor_categories_duplicate_value (ETableModel *etc, int col, const void *value, gpointer data);
static void e_contact_editor_categories_free_value (ETableModel *etc, int col, void *value, gpointer data);
static void *e_contact_editor_categories_initialize_value (ETableModel *etc, int col, gpointer data);
static gboolean e_contact_editor_categories_value_is_empty (ETableModel *etc, int col, const void *value, gpointer data);
static char * e_contact_editor_categories_value_to_string (ETableModel *etc, int col, const void *value, gpointer data);

static GnomeDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_CATEGORIES
};

GtkType
e_contact_editor_categories_get_type (void)
{
	static GtkType contact_editor_categories_type = 0;

	if (!contact_editor_categories_type)
		{
			static const GtkTypeInfo contact_editor_categories_info =
			{
				"EContactEditorCategories",
				sizeof (EContactEditorCategories),
				sizeof (EContactEditorCategoriesClass),
				(GtkClassInitFunc) e_contact_editor_categories_class_init,
				(GtkObjectInitFunc) e_contact_editor_categories_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
				(GtkClassInitFunc) NULL,
			};

			contact_editor_categories_type = gtk_type_unique (gnome_dialog_get_type (), &contact_editor_categories_info);
		}

	return contact_editor_categories_type;
}

static void
e_contact_editor_categories_class_init (EContactEditorCategoriesClass *klass)
{
	GtkObjectClass *object_class;
	GnomeDialogClass *dialog_class;

	object_class = (GtkObjectClass*) klass;
	dialog_class = (GnomeDialogClass *) klass;

	parent_class = gtk_type_class (gnome_dialog_get_type ());

	gtk_object_add_arg_type ("EContactEditorCategories::categories", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_CATEGORIES);
 
	object_class->set_arg = e_contact_editor_categories_set_arg;
	object_class->get_arg = e_contact_editor_categories_get_arg;
	object_class->destroy = e_contact_editor_categories_destroy;
}

gchar *builtin_categories[] = {
	"Business",
	"Competition",
	"Favorites",
	"Gifts",
	"Goals/Objectives",
	"Holiday",
	"Holiday Cards",
	"Hot Contacts",
	"Ideas",
	"International",
	"Key Customer",
	"Miscellaneous",
	"Personal",
	"Phone Calls",
	"Status",
	"Strategies",
	"Suppliers",
	"Time & Expenses",
	"VIP",
	"Waiting",
};

#define BUILTIN_CATEGORY_COUNT (sizeof(builtin_categories) / sizeof(builtin_categories[0]))

static void
add_list_unique(EContactEditorCategories *categories, char *string)
{
	int k;
	char *temp = e_strdup_strip(string);
	char **list = categories->category_list;

	if (!*temp) {
		g_free(temp);
		return;
	}
	for (k = 0; k < categories->list_length; k++) {
		if (!strcmp(list[k], temp)) {
			categories->selected_list[k] = TRUE;
			break;
		}
	}
	if (k == categories->list_length) {
		categories->selected_list[categories->list_length] = TRUE;
		list[categories->list_length++] = temp;
	} else {
		g_free(temp);
	}
}

static void
do_parse_categories(EContactEditorCategories *categories)
{
	char *str = categories->categories;
	int length = strlen(str);
	char *copy = g_new(char, length + 1);
	int i, j;
	char **list;
	int count = 1;

	e_table_model_pre_change(categories->model);

	for (i = 0; i < categories->list_length; i++)
		g_free(categories->category_list[i]);
	g_free(categories->category_list);
	g_free(categories->selected_list);

	for (i = 0; str[i]; i++) {
		switch (str[i]) {
		case '\\':
			i++;
			if (!str[i])
				i--;
			break;
		case ',':
			count ++;
			break;
		}
	}
	list = g_new(char *, count + 1 + BUILTIN_CATEGORY_COUNT);
	categories->category_list = list;

	categories->selected_list = g_new(gboolean, count + 1 + BUILTIN_CATEGORY_COUNT);

	for (count = 0; count < BUILTIN_CATEGORY_COUNT; count++) {
		list[count] = g_strdup(builtin_categories[count]);
		categories->selected_list[count] = 0;
	}

	categories->list_length = count;

	for (i = 0, j = 0; str[i]; i++, j++) {
		switch (str[i]) {
		case '\\':
			i++;
			if (str[i]) {
				copy[j] = str[i];
			} else
				i--;
			break;
		case ',':
			copy[j] = 0;
			add_list_unique(categories, copy);
			j = -1;
			break;
		default:
			copy[j] = str[i];
			break;
		}
	}
	copy[j] = 0;
	add_list_unique(categories, copy);
	g_free(copy);
	e_table_model_changed(categories->model);
}

static void
e_contact_editor_categories_entry_change (GtkWidget *entry, 
					  EContactEditorCategories *categories)
{
	g_free(categories->categories);
	categories->categories = e_utf8_gtk_entry_get_text(GTK_ENTRY(entry));
	do_parse_categories(categories);
}


#define INITIAL_SPEC "<ETableSpecification no-header=\"1\">    	       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 1 </column>     			       \
	</columns-shown>                 			       \
	<grouping> <leaf column=\"1\" ascending=\"1\"/> </grouping>    \
</ETableSpecification>"

static void
e_contact_editor_categories_init (EContactEditorCategories *categories)
{
	GladeXML *gui;
	GtkWidget *table;
	ECell *cell_left_just;
	ECell *cell_checkbox;
	ETableHeader *header;
	ETableCol *col;
	GtkWidget *e_table;

	categories->list_length = 0;
	categories->category_list = NULL;
	categories->selected_list = NULL;
	categories->categories = NULL;

	gnome_dialog_append_button ( GNOME_DIALOG(categories),
				     GNOME_STOCK_BUTTON_OK);
	
	gnome_dialog_append_button ( GNOME_DIALOG(categories),
				     GNOME_STOCK_BUTTON_CANCEL);

	gtk_window_set_policy(GTK_WINDOW(categories), FALSE, TRUE, FALSE);

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/categories.glade", NULL);
	categories->gui = gui;

	table = glade_xml_get_widget(gui, "table-categories");
	gtk_widget_ref(table);
	gtk_container_remove(GTK_CONTAINER(table->parent), table);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (categories)->vbox), table, TRUE, TRUE, 0);
	gtk_widget_unref(table);

	categories->entry = glade_xml_get_widget(gui, "entry-categories");
	
	gtk_signal_connect(GTK_OBJECT(categories->entry), "changed",
			   GTK_SIGNAL_FUNC(e_contact_editor_categories_entry_change), categories);

	categories->model = e_table_simple_new(e_contact_editor_categories_col_count, 
					       e_contact_editor_categories_row_count,
					       e_contact_editor_categories_value_at,
					       e_contact_editor_categories_set_value_at,
					       e_contact_editor_categories_is_cell_editable,
					       e_contact_editor_categories_duplicate_value,
					       e_contact_editor_categories_free_value,
					       e_contact_editor_categories_initialize_value,
					       e_contact_editor_categories_value_is_empty,
					       e_contact_editor_categories_value_to_string,
					       categories);

	header = e_table_header_new();

	cell_checkbox = e_cell_checkbox_new();
	col = e_table_col_new (0, "",
			       0, 20, cell_checkbox,
			       g_int_compare, TRUE);
	e_table_header_add_column (header, col, 0);

	cell_left_just = e_cell_text_new (categories->model, NULL, GTK_JUSTIFY_LEFT);
	col = e_table_col_new (1, "Category",
			       1.0, 20, cell_left_just,
			       g_str_compare, TRUE);
	e_table_header_add_column (header, col, 1);

	e_table = e_table_scrolled_new (header, categories->model, INITIAL_SPEC);

	gtk_object_sink(GTK_OBJECT(categories->model));
	
	gtk_widget_show(e_table);

	gtk_table_attach_defaults(GTK_TABLE(table),
				  e_table, 
				  0, 1,
				  3, 4);
}

void
e_contact_editor_categories_destroy (GtkObject *object)
{
	EContactEditorCategories *categories = E_CONTACT_EDITOR_CATEGORIES(object);
	int i;

	if (categories->gui)
		gtk_object_unref(GTK_OBJECT(categories->gui));

	g_free(categories->categories);
	for (i = 0; i < categories->list_length; i++)
		g_free(categories->category_list[i]);
	g_free(categories->category_list);
	g_free(categories->selected_list);
}

GtkWidget*
e_contact_editor_categories_new (char *categories)
{
	GtkWidget *widget = GTK_WIDGET (gtk_type_new (e_contact_editor_categories_get_type ()));
	gtk_object_set (GTK_OBJECT(widget),
			"categories", categories,
			NULL);
	return widget;
}

static void
e_contact_editor_categories_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EContactEditorCategories *e_contact_editor_categories;

	e_contact_editor_categories = E_CONTACT_EDITOR_CATEGORIES (o);
	
	switch (arg_id){
	case ARG_CATEGORIES:
		e_utf8_gtk_entry_set_text(GTK_ENTRY(e_contact_editor_categories->entry), GTK_VALUE_STRING (*arg));
		break;
	}
}

static void
e_contact_editor_categories_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EContactEditorCategories *e_contact_editor_categories;

	e_contact_editor_categories = E_CONTACT_EDITOR_CATEGORIES (object);

	switch (arg_id) {
	case ARG_CATEGORIES:
		GTK_VALUE_STRING (*arg) = g_strdup(e_contact_editor_categories->categories);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/* This function returns the number of columns in our ETableModel. */
static int
e_contact_editor_categories_col_count (ETableModel *etc, gpointer data)
{
	return 2;
}

/* This function returns the number of rows in our ETableModel. */
static int
e_contact_editor_categories_row_count (ETableModel *etc, gpointer data)
{
	EContactEditorCategories *categories = E_CONTACT_EDITOR_CATEGORIES(data);
	return categories->list_length;
}

/* This function returns the value at a particular point in our ETableModel. */
static void *
e_contact_editor_categories_value_at (ETableModel *etc, int col, int row, gpointer data)
{
	EContactEditorCategories *categories = E_CONTACT_EDITOR_CATEGORIES(data);
	if (col == 0)
		return (void *) categories->selected_list[row];
	else
		return categories->category_list[row];
}

/* This function sets the value at a particular point in our ETableModel. */
static void
e_contact_editor_categories_set_value_at (ETableModel *etc, int col, int row, const void *val, gpointer data)
{
	EContactEditorCategories *categories = E_CONTACT_EDITOR_CATEGORIES(data);
	if ( col == 0 ) {
		char **strs;
		int i, j;
		char *string;
		categories->selected_list[row] = (gboolean) val;
		strs = g_new(char *, categories->list_length + 1);
		for (i = 0, j = 0; i < categories->list_length; i++) {
			if (categories->selected_list[i])
				strs[j++] = categories->category_list[i];
		}
		strs[j] = 0;
		string = g_strjoinv(", ", strs);
		e_utf8_gtk_entry_set_text(GTK_ENTRY(categories->entry), string);
		g_free(string);
		g_free(strs);
	}
	if ( col == 1 )
		return;
}

/* This function returns whether a particular cell is editable. */
static gboolean
e_contact_editor_categories_is_cell_editable (ETableModel *etc, int col, int row, gpointer data)
{
	return col == 0;
}

/* This function duplicates the value passed to it. */
static void *
e_contact_editor_categories_duplicate_value (ETableModel *etc, int col, const void *value, gpointer data)
{
	if (col == 0)
		return (void *)value;
	else
		return g_strdup(value);
}

/* This function frees the value passed to it. */
static void
e_contact_editor_categories_free_value (ETableModel *etc, int col, void *value, gpointer data)
{
	if (col == 0)
		return;
	else
		g_free(value);
}

static void *
e_contact_editor_categories_initialize_value (ETableModel *etc, int col, gpointer data)
{
	if (col == 0)
		return NULL;
	else
		return g_strdup("");
}

static gboolean
e_contact_editor_categories_value_is_empty (ETableModel *etc, int col, const void *value, gpointer data)
{
	if (col == 0)
		return value == NULL;
	else
		return !(value && *(char *)value);
}

static char *
e_contact_editor_categories_value_to_string (ETableModel *etc, int col, const void *value, gpointer data)
{
	if (col == 0)
		return g_strdup_printf("%d", (int) value);
	else
		return g_strdup(value);
}
