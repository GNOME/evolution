/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-select-names.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <string.h>
#include <glib.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>

#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table-without.h>
#include <gal/widgets/e-popup-menu.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <addressbook/gui/widgets/e-addressbook-model.h>
#include <addressbook/gui/widgets/e-addressbook-table-adapter.h>
#include <addressbook/util/eab-book-util.h>
#include <addressbook/gui/component/addressbook-component.h>
#include <addressbook/gui/component/addressbook.h>

#include "e-select-names-config.h"
#include "e-select-names.h"
#include "e-select-names-table-model.h"
#include <gal/widgets/e-categories-master-list-option-menu.h>
#include <gal/e-text/e-entry.h>
#include <e-util/e-categories-master-list-wombat.h>
#include "e-util/e-sexp.h"

static void  e_select_names_init       (ESelectNames		 *names);
static void  e_select_names_class_init (ESelectNamesClass	 *klass);
static void  e_select_names_dispose    (GObject *object);
static void  update_query              (GtkWidget *widget, ESelectNames *e_select_names);
static char *get_query_string          (ESelectNames *e_select_names);

static void sync_table_and_models (ESelectNamesModel *triggering_model, ESelectNames *esl);

static GtkDialogClass *parent_class = NULL;
#define PARENT_TYPE gtk_dialog_get_type()

/* The arguments we take */
enum {
	ARG_0,
};

typedef struct {
	char                   *title;
	ESelectNamesModel      *source;
	ESelectNamesTableModel *table_model;
	ESelectNames           *names;
	GtkWidget              *label;
	GtkWidget              *button;
	GtkWidget              *recipient_table;
	gulong                  changed_id;
} ESelectNamesChild;

GType
e_select_names_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ESelectNamesClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_select_names_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESelectNames),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_select_names_init,
		};

		type = g_type_register_static (PARENT_TYPE, "ESelectNames", &info, 0);
	}

	return type;
}

static void
e_select_names_class_init (ESelectNamesClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_select_names_dispose;
}

GtkWidget *e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2);

static void
search_result (EABModel *model, EBookViewStatus status, ESelectNames *esn)
{
	sync_table_and_models (NULL, esn);
}

static void
set_book(EBook *book, EBookStatus status, ESelectNames *esn)
{
	char *query_str = get_query_string (esn);
	g_object_set(esn->model,
		     "book", book,
		     "query", query_str,
		     NULL);
	g_free (query_str);
	g_object_unref(book);
	g_object_unref(esn->model);
	g_object_unref(esn);
}

static ESource *
find_first_source (ESourceList *source_list)
{
	GSList *groups, *sources, *l, *m;
			
	groups = e_source_list_peek_groups (source_list);
	for (l = groups; l; l = l->next) {
		ESourceGroup *group = l->data;
				
		sources = e_source_group_peek_sources (group);
		for (m = sources; m; m = m->next) {
			ESource *source = m->data;

			return source;
		}				
	}

	return NULL;
}

static void
addressbook_model_set_source (ESelectNames *e_select_names, EABModel *model, ESource *source)
{
	EBook *book;

	book = e_book_new(source, NULL);

	g_object_ref(e_select_names);
	g_object_ref(model);

	addressbook_load (book, (EBookCallback) set_book, e_select_names);
}

static void *
contact_key (const EContact *contact)
{
	EBook *book = NULL;
	const gchar *book_uri;
	
	if (contact == NULL)
		return NULL;

	g_assert (E_IS_CONTACT (contact));

#if notyet
	/* XXX we need a way to reproduce this here somehow.. or at
	   least make sure we never collide between two contacts in
	   different books. */
	book = e_contact_get_book (contact);
#endif
	book_uri = book ? e_book_get_uri (book) : "NoBook";
	return g_strdup_printf ("%s|%s", book_uri ? book_uri : "NoURI", (char*)e_contact_get_const ((EContact*)contact, E_CONTACT_UID));
}

static void
sync_one_model (gpointer k, gpointer val, gpointer closure)
{
	ETableWithout *etw = E_TABLE_WITHOUT (closure);
	ESelectNamesChild *child = val;
	ESelectNamesModel *model = child->source;
	gint i, count;
	EContact *contact;
	void *key;
	
	count = e_select_names_model_count (model);
	for (i = 0; i < count; ++i) {
		contact = e_select_names_model_get_contact (model, i);
		if (contact) {
			key = contact_key (contact);
			e_table_without_hide (etw, key);
			g_free (key);
		}
	}
}

static void
sync_table_and_models (ESelectNamesModel *triggering_model, ESelectNames *esl)
{
	e_table_without_show_all (E_TABLE_WITHOUT (esl->without));
	g_hash_table_foreach (esl->children, sync_one_model, esl->without);
}

static void
real_add_address_cb (int model_row, gpointer closure)
{
	ESelectNamesChild *child = closure;
	ESelectNames *names = child->names;
	const EContact *contact;
	EDestination *dest = e_destination_new ();
	gint mapped_row;

	mapped_row = e_table_subset_view_to_model_row (E_TABLE_SUBSET (names->without), model_row);

	contact = eab_model_contact_at (EAB_MODEL(names->model), mapped_row);
	
	if (contact != NULL) {
		e_destination_set_contact (dest, (EContact*)contact, 0);

		e_select_names_model_append (child->source, dest);
		e_select_names_model_clean (child->source, FALSE);
	}
}

static void
real_add_address(ESelectNames *names, ESelectNamesChild *child)
{
	e_select_names_model_freeze (child->source);
	e_table_selected_row_foreach(e_table_scrolled_get_table(names->table),
				     real_add_address_cb, child);
	e_select_names_model_thaw (child->source);
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

static void
sensitize_button (gpointer key, gpointer data, gpointer user_data)
{
	gboolean *sensitive = user_data;
	ESelectNamesChild *child = data;

	gtk_widget_set_sensitive (child->button, *sensitive);
}

static void
selection_change (ETable *table, ESelectNames *names)
{
	gboolean sensitive;

	sensitive = e_table_selected_count (table) > 0;

	g_hash_table_foreach (names->children, sensitize_button, &sensitive);
}

static void *
esn_get_key_fn (ETableModel *source, int row, void *closure)
{
	EABModel *model = EAB_MODEL (closure);
	const EContact *contact = eab_model_contact_at (model, row);
	void *key = contact_key (contact);
	return key;
}

static void *
esn_dup_key_fn (const void *key, void *closure)
{
	void *dup = (void *) g_strdup ((const gchar *) key);
	return dup;
}

static void
esn_free_gotten_key_fn (void *key, void *closure)
{
	g_free (key);
}

static void
esn_free_duped_key_fn (void *key, void *closure)
{
	g_free (key);
}

GtkWidget *
e_addressbook_create_ebook_table(char *name, char *string1, char *string2, int num1, int num2)
{
	ETableModel *adapter;
	ETableModel *without;
	EABModel *model;
	GtkWidget *table;

	model = eab_model_new ();
	adapter = E_TABLE_MODEL (eab_table_adapter_new (model));

	g_object_set(model,
		     "editable", FALSE,
		     NULL);

	without = e_table_without_new (adapter,
				       g_str_hash,
				       g_str_equal,
				       esn_get_key_fn,
				       esn_dup_key_fn,
				       esn_free_gotten_key_fn,
				       esn_free_duped_key_fn,
				       model);

	table = e_table_scrolled_new_from_spec_file (without,
						     NULL,
						     EVOLUTION_ETSPECDIR "/e-select-names.etspec",
						     NULL);

	g_object_set_data(G_OBJECT(table), "adapter", adapter);
	g_object_set_data(G_OBJECT(table), "without", without);
	g_object_set_data(G_OBJECT(table), "model", model);

	return table;
}

static void
source_selected (ESourceOptionMenu *menu, ESource *source, ESelectNames *e_select_names)
{
	addressbook_model_set_source (e_select_names, e_select_names->model, source);
	e_select_names_config_set_last_completion_book (e_source_peek_uid (source));
}

static char *
get_query_string (ESelectNames *e_select_names)
{
	char *category = "";
	const char *search = "";
	EBookQuery *query;
	EBookQuery *q_array[4];
	char *query_str;
	int i;

	if (e_select_names->categories) {
		category = e_categories_master_list_option_menu_get_category (E_CATEGORIES_MASTER_LIST_OPTION_MENU (e_select_names->categories));
	}
	if (e_select_names->select_entry) {
		search = gtk_entry_get_text (GTK_ENTRY (e_select_names->select_entry));
	}

	i = 0;
	q_array[i++] = e_book_query_field_test (E_CONTACT_EMAIL, E_BOOK_QUERY_CONTAINS, "");
	if (category && *category)
		q_array[i++] = e_book_query_field_test (E_CONTACT_CATEGORY_LIST, E_BOOK_QUERY_IS, category);
	if (search && *search)
		q_array[i++] = e_book_query_orv (e_book_query_field_test (E_CONTACT_EMAIL, E_BOOK_QUERY_BEGINS_WITH, search),
						 e_book_query_field_test (E_CONTACT_FULL_NAME, E_BOOK_QUERY_BEGINS_WITH, search),
						 e_book_query_field_test (E_CONTACT_NICKNAME, E_BOOK_QUERY_BEGINS_WITH, search),
						 e_book_query_field_test (E_CONTACT_FILE_AS, E_BOOK_QUERY_BEGINS_WITH, search),
						 NULL);
	if (i > 1) {
		query = e_book_query_and (i, q_array, TRUE);
	}
	else {
		query = q_array[0];
	}
	query_str = e_book_query_to_string (query);
	e_book_query_unref (query);
	return query_str;
}


static void
update_query (GtkWidget *widget, ESelectNames *e_select_names)
{
	char *query_str = get_query_string (e_select_names);
	printf ("query_str = %s\n", query_str);
	g_object_set (e_select_names->model,
		      "query", query_str,
		      NULL);
	g_free (query_str);
}

static void
status_message (EABModel *model, const gchar *message, ESelectNames *e_select_names)
{
	if (message == NULL)
		gtk_label_set_text (GTK_LABEL (e_select_names->status_message), "");
	else
		gtk_label_set_text (GTK_LABEL (e_select_names->status_message), message);
}

static void
categories_changed (GtkWidget *widget, ESelectNames *e_select_names)
{
	update_query (widget, e_select_names);
}

static void
select_entry_changed (GtkWidget *widget, ESelectNames *e_select_names)
{
	if (e_select_names->select_entry) {
		const char *select_string = gtk_entry_get_text (GTK_ENTRY (e_select_names->select_entry));
		char *select_strcoll_string = g_utf8_collate_key (select_string, -1);
		int count;
		ETable *table;
		int i;

		table = e_table_scrolled_get_table (e_select_names->table);

		count = e_table_model_row_count (e_select_names->without);

		for (i = 0; i < count; i++) {
			int model_row = e_table_view_to_model_row (table, i);
			char *row_strcoll_string =
				g_utf8_collate_key (e_table_model_value_at (e_select_names->without,
									    E_CONTACT_FULL_NAME,
									    model_row),
						    -1);
			if (strcmp (select_strcoll_string, row_strcoll_string) <= 0) {
				g_free (row_strcoll_string);
				break;
			}
			g_free (row_strcoll_string);
		}
		g_free (select_strcoll_string);
		if (i == count)
			i --;

		if (count > 0) {
			i = e_table_view_to_model_row (table, i);
			e_table_set_cursor_row (table, i);
		}
	}
}

GtkWidget *e_select_names_create_categories (gchar *name,
					     gchar *string1, gchar *string2,
					     gint int1, gint int2);

GtkWidget *
e_select_names_create_categories (gchar *name,
				  gchar *string1, gchar *string2,
				  gint int1, gint int2)
{
	ECategoriesMasterList *ecml;
	GtkWidget *option_menu;

	ecml = e_categories_master_list_wombat_new ();
	option_menu = e_categories_master_list_option_menu_new (ecml);
	g_object_unref (ecml);

	return option_menu;
}

static void
clear_widget (gpointer data, GObject *where_object_was)
{
	GtkWidget **widget_ref = data;
	*widget_ref = NULL;
}

static void
e_select_names_init (ESelectNames *e_select_names)
{
	GladeXML *gui;
	GtkWidget *widget, *button, *table, *esom;
	ESource *source = NULL;
	char *uid;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/select-names.glade", NULL, NULL);
	widget = glade_xml_get_widget (gui, "select-names-box");
	if (!widget) {
		g_object_unref (gui);
		return;
	}
	gtk_widget_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (e_select_names)->vbox), widget, TRUE, TRUE, 0);
	gtk_widget_unref (widget);

	gtk_dialog_add_buttons (GTK_DIALOG (e_select_names),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_modal (GTK_WINDOW (e_select_names), TRUE);
	gtk_window_set_default_size (GTK_WINDOW (e_select_names), 472, 512);
	gtk_window_set_title (GTK_WINDOW (e_select_names), _("Select Contacts from Address Book"));
	gtk_window_set_resizable (GTK_WINDOW (e_select_names), TRUE);
	gtk_dialog_set_has_separator (GTK_DIALOG (e_select_names), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (e_select_names), 4);

	/* FIXME What to do on error/NULL ? */
	e_select_names->source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");
	
	e_select_names->gui = gui;

	/* Add the source menu */
	esom = e_source_option_menu_new (e_select_names->source_list);
	g_signal_connect (esom, "source_selected", G_CALLBACK (source_selected), e_select_names);
	gtk_widget_show (esom);

	table = glade_xml_get_widget (gui, "show_contacts_table");
	gtk_table_attach (GTK_TABLE (table), esom, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

	/* Set up the rest of the widgets */
	e_select_names->children = g_hash_table_new(g_str_hash, g_str_equal);
	e_select_names->child_count = 0;
	e_select_names->def = NULL;

	gtk_dialog_set_default_response (GTK_DIALOG (e_select_names),
					 GTK_RESPONSE_OK);

	e_select_names->table = E_TABLE_SCROLLED(glade_xml_get_widget(gui, "table-source"));
	e_select_names->model = g_object_get_data(G_OBJECT(e_select_names->table), "model");
	e_select_names->adapter = g_object_get_data(G_OBJECT(e_select_names->table), "adapter");
	e_select_names->without = g_object_get_data(G_OBJECT(e_select_names->table), "without");
	gtk_widget_show (GTK_WIDGET (e_select_names->table));

	e_select_names->status_message = glade_xml_get_widget (gui, "status-message");
	if (e_select_names->status_message && !GTK_IS_LABEL (e_select_names->status_message))
		e_select_names->status_message = NULL;
	if (e_select_names->status_message) {
		e_select_names->status_id = g_signal_connect (e_select_names->model, "status_message",
							      G_CALLBACK (status_message), e_select_names);
		g_object_weak_ref (G_OBJECT (e_select_names->status_message), clear_widget, &e_select_names->status_message);
	}

	e_select_names->search_id = g_signal_connect (e_select_names->model,
						      "search_result", G_CALLBACK (search_result),
						      e_select_names);

	e_select_names->categories = glade_xml_get_widget (gui, "custom-categories");
	if (e_select_names->categories && !E_IS_CATEGORIES_MASTER_LIST_OPTION_MENU (e_select_names->categories))
		e_select_names->categories = NULL;
	if (e_select_names->categories) {
		g_signal_connect(e_select_names->categories, "changed",
				 G_CALLBACK(categories_changed), e_select_names);
		g_object_weak_ref (G_OBJECT (e_select_names->categories), clear_widget, &e_select_names->categories);
	}
	gtk_widget_show (e_select_names->categories);

	e_select_names->select_entry = glade_xml_get_widget (gui, "entry-select");
	if (e_select_names->select_entry && !GTK_IS_ENTRY (e_select_names->select_entry))
		e_select_names->select_entry = NULL;
	if (e_select_names->select_entry) {
		g_signal_connect(e_select_names->select_entry, "changed",
				 G_CALLBACK(select_entry_changed), e_select_names);
		g_signal_connect(e_select_names->select_entry, "activate",
				 G_CALLBACK(update_query), e_select_names);
		g_object_weak_ref (G_OBJECT (e_select_names->select_entry), clear_widget, &e_select_names->select_entry);
	}

	button  = glade_xml_get_widget (gui, "button-find");
	if (button && GTK_IS_BUTTON (button))
		g_signal_connect(button, "clicked",
				 G_CALLBACK(update_query), e_select_names);

	g_signal_connect (e_table_scrolled_get_table (e_select_names->table), "double_click",
			  G_CALLBACK (add_address), e_select_names);
	g_signal_connect (e_table_scrolled_get_table (e_select_names->table), "selection_change",
			  G_CALLBACK (selection_change), e_select_names);
	selection_change (e_table_scrolled_get_table (e_select_names->table), e_select_names);

	/* Select a source for to display initially */
	uid = e_select_names_config_get_last_completion_book ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (e_select_names->source_list, uid);
		g_free (uid);
	}
	
	if (!source)	
		source = find_first_source (e_select_names->source_list);

	/* FIXME What if we still can't find a source? */
	e_source_option_menu_select (E_SOURCE_OPTION_MENU (esom), source);

}

static void e_select_names_child_free(char *key, ESelectNamesChild *child, ESelectNames *e_select_names)
{
	g_signal_handler_disconnect(child->source, child->changed_id);

	g_free(child->title);
	g_object_unref(child->table_model);
	g_object_unref(child->source);
	g_free(key);
	g_free(child);
}

static void
e_select_names_dispose (GObject *object)
{
	ESelectNames *e_select_names = E_SELECT_NAMES(object);

	if (e_select_names->source_list) {
		g_object_unref (e_select_names->source_list);
		e_select_names->source_list = NULL;
	}
	
	if (e_select_names->status_id) {
		g_signal_handler_disconnect(e_select_names->model, e_select_names->status_id);
		e_select_names->status_id = 0;
	}

	if (e_select_names->search_id) {
		g_signal_handler_disconnect(e_select_names->model, e_select_names->search_id);
		e_select_names->search_id = 0;
	}

	if (e_select_names->gui) {
		g_object_unref(e_select_names->gui);
		e_select_names->gui = NULL;
	}

	if (e_select_names->children) {
		g_hash_table_foreach(e_select_names->children, (GHFunc) e_select_names_child_free, e_select_names);
		g_hash_table_destroy(e_select_names->children);
		e_select_names->children = NULL;
	}

	if (e_select_names->without) {
		g_object_unref(e_select_names->without);
		e_select_names->without = NULL;
	}
	if (e_select_names->adapter) {
		g_object_unref(e_select_names->adapter);
		e_select_names->adapter = NULL;
	}
	if (e_select_names->model) {
		g_object_unref(e_select_names->model);
		e_select_names->model = NULL;
	}

	if (e_select_names->def) {
		g_free(e_select_names->def);
		e_select_names->def = NULL;
	}

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

GtkWidget*
e_select_names_new (void)
{
	ESelectNames *e_select_names;
	
	e_select_names = g_object_new (E_TYPE_SELECT_NAMES, NULL);

	return GTK_WIDGET (e_select_names);
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

struct _RightClickData {
	ESelectNamesChild *child;
	int row;
};
typedef struct _RightClickData RightClickData;

static void
remove_cb (GtkWidget *widget, void *data)
{
	RightClickData *rcdata = (RightClickData *)data;

	e_select_names_model_delete (rcdata->child->source, rcdata->row);

	/* Free everything we've created */
	g_free (rcdata);
}

static void
section_right_click_cb (ETable *et, int row, int col, GdkEvent *ev, ESelectNamesChild *child)
{
	static EPopupMenu right_click_menu[] = {
		E_POPUP_ITEM (N_("Remove"), G_CALLBACK (remove_cb), 0),
		E_POPUP_TERMINATOR
	};
	RightClickData *rcdata = g_new0 (RightClickData, 1);

	rcdata->row = row;
	rcdata->child = child;

	e_popup_menu_run (right_click_menu, (GdkEvent *)ev, 0, 0, rcdata);
}

void
e_select_names_add_section (ESelectNames *e_select_names,
			    const char *name, const char *id,
			    ESelectNamesModel *source)
{
	ESelectNamesChild *child;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *alignment;
	GtkTable *table;
	char *label_text;
	ETable *etable;
	ETableExtras *extras;
	ECell *string_cell;

	GtkWidget *sw;

	if (g_hash_table_lookup(e_select_names->children, id)) {
		return;
	}

	table = GTK_TABLE(glade_xml_get_widget (e_select_names->gui, "table-recipients"));

	child = g_new(ESelectNamesChild, 1);

	child->names = e_select_names;
	child->title = g_strdup (_(name));

	child->table_model = (ESelectNamesTableModel*)e_select_names_table_model_new (source);

	child->source = source;
	g_object_ref(child->source);

	alignment = gtk_alignment_new(0, 0, 1, 0);

	label_text = g_strconcat (child->title, " ->", NULL);

	label = gtk_label_new ("");

	gtk_label_set_markup (GTK_LABEL(label), label_text);

	g_free (label_text);

	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), label);
	child->label = label;
	child->button = button;

	gtk_container_add(GTK_CONTAINER(alignment), button);
	gtk_widget_show_all(alignment);
	g_signal_connect(button, "clicked",
			 G_CALLBACK(button_clicked), child);
	gtk_table_attach(table, alignment,
			 0, 1,
			 e_select_names->child_count,
			 e_select_names->child_count + 1,
			 GTK_FILL, GTK_FILL,
			 0, 0);

	etable = e_table_scrolled_get_table (e_select_names->table);
	gtk_widget_set_sensitive (button, e_table_selected_count (etable) > 0);

	extras = e_table_extras_new ();
	string_cell = e_table_extras_get_cell (extras, "string");

	g_object_set (string_cell,
		      "underline_column", 2,
		      NULL);
		      
	sw = e_table_scrolled_new_from_spec_file (E_TABLE_MODEL (child->table_model),
						  extras,
						  EVOLUTION_ETSPECDIR "/e-select-names-section.etspec",
						  NULL);
	g_object_unref (extras);

	child->recipient_table = GTK_WIDGET (e_table_scrolled_get_table (E_TABLE_SCROLLED (sw)));

	g_signal_connect (child->recipient_table,
			  "right_click",
			  G_CALLBACK (section_right_click_cb),
			  child);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	g_signal_connect(child->recipient_table, "double_click",
			 G_CALLBACK(remove_address), child);

	child->changed_id = g_signal_connect (child->source,
					      "changed",
					      G_CALLBACK (sync_table_and_models),
					      e_select_names);
	
	gtk_widget_show_all (sw);
	
	gtk_table_attach(table, sw,
			 1, 2,
			 e_select_names->child_count,
			 e_select_names->child_count + 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	g_hash_table_insert(e_select_names->children, g_strdup(id), child);

	sync_table_and_models (child->source, e_select_names);

	e_select_names->child_count++;
}

void
e_select_names_set_default (ESelectNames *e_select_names,
			    const char *id)
{
	ESelectNamesChild *child;

	if (e_select_names->def) {
		child = g_hash_table_lookup(e_select_names->children, e_select_names->def);
		if (child) {
			GtkWidget *label = child->label;

			/* set the previous default to non-bold */
			gtk_label_set_markup (GTK_LABEL (label), child->title);
		}
	}

	g_free(e_select_names->def);
	e_select_names->def = g_strdup(id);

	if (e_select_names->def) {
		child = g_hash_table_lookup(e_select_names->children, e_select_names->def);
		if (child) {
			GtkWidget *label = child->label;
			char *markup = g_strconcat ("<b>", child->title, "</b>", NULL);

			/* set the new default to bold */
			gtk_label_set_markup (GTK_LABEL (label), markup);
			g_free (markup);
		}
	}
}
