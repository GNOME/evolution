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
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table-without.h>
#include <gal/widgets/e-font.h>
#include <gal/widgets/e-popup-menu.h>

#include <addressbook/gui/widgets/e-addressbook-model.h>
#include <addressbook/gui/widgets/e-addressbook-table-adapter.h>
#include <addressbook/gui/component/e-cardlist-model.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/gui/component/addressbook-component.h>
#include <addressbook/gui/component/addressbook-storage.h>
#include <addressbook/gui/component/addressbook.h>
#include <shell/evolution-shell-client.h>

#include "e-select-names.h"
#include <addressbook/backend/ebook/e-card-simple.h>
#include "e-select-names-table-model.h"
#include <gal/widgets/e-categories-master-list-combo.h>
#include <gal/widgets/e-unicode.h>
#include <gal/e-text/e-entry.h>
#include <e-util/e-categories-master-list-wombat.h>

static void e_select_names_init		(ESelectNames		 *card);
static void e_select_names_class_init	(ESelectNamesClass	 *klass);
static void e_select_names_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_select_names_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_destroy (GtkObject *object);
static void update_query (GtkWidget *button, ESelectNames *e_select_names);

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
set_book(EBook *book, EBookStatus status, ESelectNames *esn)
{
	gtk_object_set(GTK_OBJECT(esn->model),
		       "book", book,
		       NULL);
	update_query (NULL, esn);
	gtk_object_unref(GTK_OBJECT(book));
	gtk_object_unref(GTK_OBJECT(esn));
	gtk_object_unref(GTK_OBJECT(esn->model));
}

static void
set_book_with_model_data(EBook *book, EBookStatus status, EAddressbookModel *model)
{
	gtk_object_set(GTK_OBJECT(model),
		       "book", book,
		       "query", "(contains \"email\" \"\")",
		       NULL);
	gtk_object_unref(GTK_OBJECT(book));
	gtk_object_unref(GTK_OBJECT(model));
}

static void
addressbook_model_set_uri(ESelectNames *e_select_names, EAddressbookModel *model, char *uri)
{
	EBook *book;

	/* If uri == the current uri, then we don't have to do anything */
	book = e_addressbook_model_get_ebook (model);
	if (book) {
		const gchar *current_uri = e_book_get_uri (book);
		if (uri && current_uri && !strcmp (uri, current_uri))
			return;
	}

	book = e_book_new();
	if (e_select_names) {
		gtk_object_ref(GTK_OBJECT(e_select_names));
		gtk_object_ref(GTK_OBJECT(model));
		addressbook_load_uri(book, uri, (EBookCallback) set_book, e_select_names);
	} else {
		gtk_object_ref(GTK_OBJECT(model));
		addressbook_load_uri(book, uri, (EBookCallback) set_book_with_model_data, model);
	}
}

static void *
card_key (ECard *card)
{
	EBook *book;
	const gchar *book_uri;
	
	if (card == NULL)
		return NULL;

	g_assert (E_IS_CARD (card));

	book = e_card_get_book (card);
	book_uri = book ? e_book_get_uri (book) : "NoBook";
	return g_strdup_printf ("%s|%s", book_uri ? book_uri : "NoURI", e_card_get_id (card));
}

static void
sync_one_model (gpointer k, gpointer val, gpointer closure)
{
	ETableWithout *etw = E_TABLE_WITHOUT (closure);
	ESelectNamesChild *child = val;
	ESelectNamesModel *model = child->source;
	gint i, count;
	ECard *card;
	void *key;
	
	count = e_select_names_model_count (model);
	for (i = 0; i < count; ++i) {
		card = e_select_names_model_get_card (model, i);
		if (card) {
			key = card_key (card);
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
	ECard *card;
	EDestination *dest = e_destination_new ();
	gint mapped_row;

	mapped_row = e_table_subset_view_to_model_row (E_TABLE_SUBSET (names->without), model_row);

	card = e_addressbook_model_get_card (E_ADDRESSBOOK_MODEL(names->model), mapped_row);
	
	if (card != NULL) {
		e_destination_set_card (dest, card, 0);

		e_select_names_model_append (child->source, dest);
		e_select_names_model_clean (child->source);

		gtk_object_unref(GTK_OBJECT(card));
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

static void *
esn_get_key_fn (ETableModel *source, int row, void *closure)
{
	EAddressbookModel *model = E_ADDRESSBOOK_MODEL (closure);
	ECard *card = e_addressbook_model_get_card (model, row);
	void *key = card_key (card);
	gtk_object_unref (GTK_OBJECT (card));
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
	EAddressbookModel *model;
	GtkWidget *table;
	char *filename;
	char *uri;
	char *spec;

	model = e_addressbook_model_new ();
	adapter = E_TABLE_MODEL (e_addressbook_table_adapter_new (model));

	filename = gnome_util_prepend_user_home("evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf("file://%s", filename);

	addressbook_model_set_uri(NULL, model, uri);

	g_free(uri);
	g_free(filename);

	gtk_object_set(GTK_OBJECT(model),
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

	spec = g_strdup_printf(SPEC, E_CARD_SIMPLE_FIELD_NAME_OR_ORG);
	table = e_table_scrolled_new (without, NULL, spec, NULL);
	g_free(spec);

	gtk_object_set_data(GTK_OBJECT(table), "adapter", adapter);
	gtk_object_set_data(GTK_OBJECT(table), "without", without);
	gtk_object_set_data(GTK_OBJECT(table), "model", model);

	return table;
}

typedef struct {
	char *description;
	char *display_name;
	char *uri;

} ESelectNamesFolder;

static void
e_select_names_folder_free(ESelectNamesFolder *e_folder)
{
	g_free(e_folder->description );
	g_free(e_folder->display_name);
	g_free(e_folder->uri);
	g_free(e_folder);
}

static void
e_select_names_option_activated(GtkWidget *widget, ESelectNames *e_select_names)
{
	ESelectNamesFolder *e_folder = gtk_object_get_data (GTK_OBJECT (widget), "EsnChoiceFolder");

	addressbook_model_set_uri(e_select_names, e_select_names->model, e_folder->uri);
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
	gchar *label;

	nnm = user_data;
	e_folder = value;
	menu = nnm->menu;
	e_select_names = nnm->names;

	label = e_utf8_to_locale_string (_(e_folder->display_name));
	item = gtk_menu_item_new_with_label (label);
	g_free (label);

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
	if (!strcmp(folder->type, "contacts")
	    || !strcmp(folder->type, "ldap-contacts")) {
		ESelectNamesFolder *e_folder = g_new(ESelectNamesFolder, 1);
		e_folder->description  = g_strdup(folder->description );
		if (!strcmp (folder->type, "ldap-contacts"))
			e_folder->display_name = g_strdup_printf ("%s [LDAP]", folder->displayName);
		else
			e_folder->display_name = g_strdup(folder->displayName);

		if (!strncmp (folder->physicalUri, "file:", 5))
			e_folder->uri = g_strdup_printf ("%s/addressbook.db", folder->physicalUri);
		else
			e_folder->uri = g_strdup(folder->physicalUri);
		g_hash_table_insert(e_select_names->folders,
				    g_strdup(path), e_folder);
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
update_query (GtkWidget *button, ESelectNames *e_select_names)
{
	char *category = "";
	char *search = "";
	char *query;
	char *q_array[4];
	int i;
	if (e_select_names->categories_entry) {
		category = gtk_entry_get_text (GTK_ENTRY (e_select_names->categories_entry));
	}
	if (e_select_names->search_entry) {
		search = gtk_entry_get_text (GTK_ENTRY (e_select_names->search_entry));
	}
	i = 0;
	q_array[i++] = "(contains \"email\" \"\")";
	if (category && *category)
		q_array[i++] = g_strdup_printf ("(is \"category\" \"%s\")", category);
	if (search && *search)
		q_array[i++] = g_strdup_printf ("(or (beginswith \"email\" \"%s\") "
						"    (beginswith \"full_name\" \"%s\") "
						"    (beginswith \"nickname\" \"%s\"))",
						search, search, search);
	q_array[i++] = NULL;
	if (i > 2) {
		char *temp = g_strjoinv (" ", q_array);
		query = g_strdup_printf ("(and %s)", temp);
		g_free (temp);
	} else {
		query = g_strdup (q_array[0]);
	}
	gtk_object_set (GTK_OBJECT (e_select_names->model),
			"query", query,
			NULL);
	for (i = 1; q_array[i]; i++) {
		g_free (q_array[i]);
	}
	g_free (query);
}

static void
status_message (EAddressbookModel *model, const gchar *message, ESelectNames *e_select_names)
{
	if (message == NULL)
		gtk_label_set_text (GTK_LABEL (e_select_names->status_message), "");
	else
		gtk_label_set_text (GTK_LABEL (e_select_names->status_message), message);
}

static void
hookup_listener (ESelectNames *e_select_names,
		 GNOME_Evolution_Storage storage,
		 EvolutionStorageListener *listener,
		 CORBA_Environment *ev)
{
	GNOME_Evolution_StorageListener corba_listener;

	g_return_if_fail (storage != CORBA_OBJECT_NIL);

	corba_listener = evolution_storage_listener_corba_objref(listener);

	gtk_signal_connect(GTK_OBJECT(listener), "new_folder",
			   GTK_SIGNAL_FUNC(new_folder), e_select_names);
	gtk_signal_connect(GTK_OBJECT(listener), "removed_folder",
			   GTK_SIGNAL_FUNC(removed_folder), e_select_names);

	GNOME_Evolution_Storage_addListener(storage, corba_listener, ev);

	if (ev->_major != CORBA_NO_EXCEPTION) {
		g_warning ("e_select_names_init: Exception adding listener to "
			   "remote GNOME_Evolution_Storage interface.\n");
		return;
	}
}

static void
add_additional_select_names_uris (ESelectNames *e_select_names, CORBA_Environment *ev)
{
	Bonobo_ConfigDatabase config_db;
	guint32 num_additional_uris;
	int i;

	config_db = addressbook_config_database (ev);

	num_additional_uris = bonobo_config_get_ulong_with_default (config_db, "/Addressbook/additional_select_names_folders/num", 0, ev);
	for (i = 0; i < num_additional_uris; i ++) {
		ESelectNamesFolder *e_folder;
		char *config_path;
		char *path;
		char *name;
		char *uri;

		config_path = g_strdup_printf ("/Addressbook/additional_select_names_folders/folder_%d_path", i);
		path = bonobo_config_get_string (config_db, config_path, ev);
		g_free (config_path);

		config_path = g_strdup_printf ("/Addressbook/additional_select_names_folders/folder_%d_name", i);
		name = bonobo_config_get_string (config_db, config_path, ev);
		g_free (config_path);

		config_path = g_strdup_printf ("/Addressbook/additional_select_names_folders/folder_%d_uri", i);
		uri = bonobo_config_get_string (config_db, config_path, ev);
		g_free (config_path);

		if (!path || !name || !uri) {
			g_free (path);
			g_free (name);
			g_free (uri);
			continue;
		}

		e_folder = g_new(ESelectNamesFolder, 1);
		e_folder->description  = g_strdup("");
		e_folder->display_name = g_strdup(name);
		if (!strncmp (uri, "file:", 5))
			e_folder->uri = g_strdup_printf ("%s/addressbook.db", uri);
		else
			e_folder->uri = g_strdup(uri);
		g_hash_table_insert(e_select_names->folders,
				    g_strdup(path), e_folder);
	}

	if (num_additional_uris)
		update_option_menu(e_select_names);
}

static void
e_select_names_hookup_shell_listeners (ESelectNames *e_select_names)
{
	EvolutionStorage *other_contact_storage;
	GNOME_Evolution_Storage storage;
	CORBA_Environment ev;
	
	CORBA_exception_init(&ev);

	storage = (GNOME_Evolution_Storage) (evolution_shell_client_get_local_storage(addressbook_component_get_shell_client()));
	e_select_names->local_listener = evolution_storage_listener_new();

	/* This should really never happen, but a bug report (ximian #5193) came in w/ a backtrace suggesting that it did in
	   fact happen to someone, so the best we can do is try to avoid crashing in this case. */
	if (storage == CORBA_OBJECT_NIL) {
		GtkWidget *oh_shit;

#if 0
		oh_shit = gnome_error_dialog (_("Evolution is unable to get the addressbook local storage.\n"
						"This may have been caused by the evolution-addressbook component crashing.\n"
						"To help us better understand and ultimately resolve this problem,\n"
						"please send an e-mail to Jon Trowbridge <trow@ximian.com> with a\n"
						"detailed description of the circumstances under which this error\n"
						"occurred.  Thank you."));
#endif

		oh_shit = gnome_error_dialog (_("Evolution is unable to get the addressbook local storage.\n"
						"Under normal circumstances, this should never happen.\n"
						"You may need to exit and restart Evolution in order to\n"
						"correct this problem."));
		gtk_widget_show (oh_shit);
		return;
	}
	else {
		hookup_listener (e_select_names, storage, e_select_names->local_listener, &ev);
		bonobo_object_release_unref(storage, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("e_select_names_init: Exception unref'ing "
				   "remote GNOME_Evolution_Storage interface.\n");
			CORBA_exception_free (&ev);
			return;
		}
	}

	other_contact_storage = addressbook_get_other_contact_storage ();
	if (other_contact_storage) {
		storage = bonobo_object_corba_objref (BONOBO_OBJECT (other_contact_storage));
		e_select_names->other_contacts_listener = evolution_storage_listener_new();

		hookup_listener (e_select_names, storage, e_select_names->other_contacts_listener, &ev);
	}

	/* XXX kludge to fill in folders for the exchange plugin.  we
	   latch onto a set of bonobo-conf settings that are
	   maintained by the plugin and do the right magic to get the
	   folders displayed in the option menu */
	add_additional_select_names_uris (e_select_names, &ev);
	
	CORBA_exception_free(&ev);
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
	GtkWidget *combo;

	ecml = e_categories_master_list_wombat_new ();
	combo = e_categories_master_list_combo_new (ecml);
	gtk_object_unref (GTK_OBJECT (ecml));

	return combo;
}

static void
e_select_names_init (ESelectNames *e_select_names)
{
	GladeXML *gui;
	GtkWidget *widget, *button;

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
	gnome_dialog_set_default(GNOME_DIALOG(e_select_names), 0);

	gtk_window_set_title(GTK_WINDOW(e_select_names), _("Select Contacts from Addressbook")); 
	gtk_window_set_policy(GTK_WINDOW(e_select_names), FALSE, TRUE, FALSE);

	e_select_names->table = E_TABLE_SCROLLED(glade_xml_get_widget(gui, "table-source"));
	e_select_names->model = gtk_object_get_data(GTK_OBJECT(e_select_names->table), "model");
	e_select_names->adapter = gtk_object_get_data(GTK_OBJECT(e_select_names->table), "adapter");
	e_select_names->without = gtk_object_get_data(GTK_OBJECT(e_select_names->table), "without");

	e_select_names->status_message = glade_xml_get_widget (gui, "status-message");
	if (e_select_names->status_message && !GTK_IS_LABEL (e_select_names->status_message))
		e_select_names->status_message = NULL;
	if (e_select_names->status_message)
		gtk_signal_connect (GTK_OBJECT (e_select_names->model), "status_message",
				    GTK_SIGNAL_FUNC (status_message), e_select_names);

	e_select_names->categories = glade_xml_get_widget (gui, "custom-categories");
	if (e_select_names->categories && !GTK_IS_COMBO (e_select_names->categories))
		e_select_names->categories = NULL;
	if (e_select_names->categories) {
		e_select_names->categories_entry = GTK_COMBO (e_select_names->categories)->entry;
	} else
		e_select_names->categories_entry = NULL;
	e_select_names->search_entry = glade_xml_get_widget (gui, "entry-find");
	if (e_select_names->search_entry && !GTK_IS_ENTRY (e_select_names->search_entry))
		e_select_names->search_entry = NULL;
	if (e_select_names->search_entry)
		gtk_signal_connect(GTK_OBJECT(e_select_names->search_entry), "activate",
				   GTK_SIGNAL_FUNC(update_query), e_select_names);
	if (e_select_names->categories_entry)
		gtk_signal_connect(GTK_OBJECT(e_select_names->categories_entry), "changed",
				   GTK_SIGNAL_FUNC(update_query), e_select_names);

	button  = glade_xml_get_widget (gui, "button-find");
	if (button)
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(update_query), e_select_names);

	e_select_names->folders = g_hash_table_new(g_str_hash, g_str_equal);

	e_select_names_hookup_shell_listeners (e_select_names);

	gtk_signal_connect (GTK_OBJECT (e_table_scrolled_get_table (e_select_names->table)), "double_click",
			    GTK_SIGNAL_FUNC (add_address), e_select_names);
}

static void e_select_names_child_free(char *key, ESelectNamesChild *child, ESelectNames *e_select_names)
{
	gtk_signal_disconnect_by_func (GTK_OBJECT (child->source), GTK_SIGNAL_FUNC (sync_table_and_models), e_select_names);
	g_free(child->title);
	gtk_object_unref(GTK_OBJECT(child->model));
	gtk_object_unref(GTK_OBJECT(child->source));
	g_free(key);
}

static void
e_select_names_destroy (GtkObject *object)
{
	ESelectNames *e_select_names = E_SELECT_NAMES(object);

	if (e_select_names->local_listener) {
		gtk_signal_disconnect_by_data(GTK_OBJECT(e_select_names->local_listener), e_select_names);
		gtk_object_unref(GTK_OBJECT(e_select_names->local_listener));
	}

	if (e_select_names->other_contacts_listener) {
		gtk_signal_disconnect_by_data(GTK_OBJECT(e_select_names->other_contacts_listener), e_select_names);
		gtk_object_unref(GTK_OBJECT(e_select_names->other_contacts_listener));
	}

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

struct _RightClickData {
	ETable *table;
	ESelectNamesChild *child;
};
typedef struct _RightClickData RightClickData;

static GSList *selected_rows = NULL;

static void
etable_selection_foreach_cb (int row, void *data)
{
	/* Build a list of rows in reverse order, then remove them,
           necessary because otherwise it'll start trying to delete
           rows out of index in ETableModel */
	selected_rows = g_slist_prepend (selected_rows, GINT_TO_POINTER (row));
}

static void
selected_rows_foreach_cb (void *row, void *data)
{
	ESelectNamesChild *child = data;

	remove_address (NULL, GPOINTER_TO_INT (row), 0, NULL, child);
}

static void
remove_cb (GtkWidget *widget, void *data)
{
	RightClickData *rcdata = (RightClickData *)data;

	e_select_names_model_freeze (rcdata->child->source);

	/* Build a list of selected rows */
	e_table_selected_row_foreach (rcdata->table,
				      etable_selection_foreach_cb,
				      rcdata->child);

	/* Now process the list we made, removing each selected row */
	g_slist_foreach (selected_rows,
			 (GFunc)selected_rows_foreach_cb,
			 rcdata->child);

	e_select_names_model_thaw (rcdata->child->source);

	/* Free everything we've created */
	g_free (rcdata);
	g_slist_free (selected_rows);
	selected_rows = NULL;
}

static void
section_right_click_cb (ETable *table, gint row, gint col, GdkEvent *event, ESelectNamesChild *child)
{
	EPopupMenu right_click_menu[] = {
		{ N_("Remove"), NULL,
		  GTK_SIGNAL_FUNC (remove_cb), NULL, 0 },
		{ NULL, NULL, NULL, 0 }
	};

	RightClickData *rcdata = g_new0 (RightClickData, 1);
	rcdata->table = table;
	rcdata->child = child;

	e_popup_menu_run (right_click_menu, event, 0, 0,
			  rcdata);
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
	child->title = e_utf8_from_locale_string(_(name));

	e_select_names->child_count++;

	alignment = gtk_alignment_new(0, 0, 1, 0);

	button = gtk_button_new ();

	label = e_entry_new ();
	gtk_object_set(GTK_OBJECT(label),
		       "draw_background", FALSE,
		       "draw_borders", FALSE,
		       "draw_button", TRUE,
		       "editable", FALSE,
		       "text", "",
		       "use_ellipsis", FALSE,
		       "justification", GTK_JUSTIFY_CENTER,
		       NULL);

	label_text = g_strconcat (child->title, " ->", NULL);
	gtk_object_set (GTK_OBJECT (label),
			"text", label_text,
			"emulate_label_resize", TRUE,
			NULL);
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

	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(E_TABLE_SCROLLED(etable))), "right_click",
			   GTK_SIGNAL_FUNC(section_right_click_cb), child);
	gtk_signal_connect(GTK_OBJECT(e_table_scrolled_get_table(E_TABLE_SCROLLED(etable))), "double_click",
			   GTK_SIGNAL_FUNC(remove_address), child);

	child->model = model;
	child->source = source;
	gtk_object_ref(GTK_OBJECT(child->model));
	gtk_object_ref(GTK_OBJECT(child->source));

	gtk_signal_connect (GTK_OBJECT (child->source),
			    "changed",
			    GTK_SIGNAL_FUNC (sync_table_and_models),
			    e_select_names);
	
	gtk_widget_show(etable);
	
	gtk_table_attach(table, etable,
			 1, 2,
			 e_select_names->child_count,
			 e_select_names->child_count + 1,
			 GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
			 0, 0);

	g_hash_table_insert(e_select_names->children, g_strdup(id), child);

	sync_table_and_models (child->source, e_select_names);
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
			gtk_object_set (GTK_OBJECT (E_ENTRY (child->label)->item),
					"bold", FALSE,
					NULL);
	}

	g_free(e_select_names->def);
	e_select_names->def = g_strdup(id);

	if (e_select_names->def) {
		child = g_hash_table_lookup(e_select_names->children, e_select_names->def);
		if (child)
			gtk_object_set (GTK_OBJECT (E_ENTRY (child->label)->item),
					"bold", TRUE,
					NULL);
	}
}
