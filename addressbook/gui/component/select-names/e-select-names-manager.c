/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@ximian.com>
 *   Jon Trowbridge  <trow@ximian.com.
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gal/e-text/e-entry.h>

#include <libgnome/gnome-i18n.h>
#include "e-select-names-manager.h"
#include "e-select-names-marshal.h"
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"
#include "e-select-names.h"
#include "e-select-names-completion.h"
#include "e-select-names-popup.h"
#include "e-folder-list.h"
#include <addressbook/util/eab-book-util.h>
#include <addressbook/util/eab-destination.h>
#include "addressbook/gui/component/addressbook.h"
#include <bonobo/bonobo-object.h>

#define DEFAULT_MINIMUM_QUERY_LENGTH 3

enum {
	CHANGED,
	OK,
	CANCEL,
	LAST_SIGNAL
};

static guint e_select_names_manager_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

typedef struct {
	char *id;
	char *title;
	ESelectNamesModel *model;
	ESelectNamesModel *original_model;
	ESelectNamesManager *manager;
	guint changed_tag;
} ESelectNamesManagerSection;

typedef struct {
	char *id;
	EEntry *entry;
	ESelectNamesManager *manager;
	ESelectNamesModel *model;
	ECompletion *comp;
	guint cleaning_tag;
} ESelectNamesManagerEntry;

static void e_select_names_manager_init (ESelectNamesManager *manager);
static void e_select_names_manager_class_init (ESelectNamesManagerClass *klass);

static void e_select_names_manager_dispose (GObject *object);

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

/* ESelectNamesManagerSection routines */

static void
section_model_changed_cb (ESelectNamesModel *model, gpointer closure)
{
	ESelectNamesManagerSection *section = closure;
	g_signal_emit (section->manager,
		       e_select_names_manager_signals[CHANGED], 0,
		       section->id,
		       FALSE);
}

static ESelectNamesManagerSection *
e_select_names_manager_section_new (ESelectNamesManager *manager,
				    const gchar *id,
				    const gchar *title,
				    ESelectNamesModel *model)
{
	ESelectNamesManagerSection *section;

	g_return_val_if_fail (E_IS_SELECT_NAMES_MANAGER (manager), NULL);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), NULL);

	section = g_new0 (ESelectNamesManagerSection, 1);

	section->id = g_strdup (id);
	section->title = g_strdup (title);

	section->manager = manager;

	section->model = model;
	g_object_ref (section->model);
	section->changed_tag =
		g_signal_connect (section->model,
				    "changed",
				    G_CALLBACK (section_model_changed_cb),
				    section);

	return section;
}

static void
e_select_names_manager_section_free (ESelectNamesManagerSection *section)
{
	if (section == NULL)
		return;

	g_free (section->id);
	g_free (section->title);

	if (section->model) {
		g_signal_handler_disconnect (section->model, section->changed_tag);
		g_object_unref (section->model);
	}

	if (section->original_model) {
		g_object_unref (section->original_model);
	}

	g_free (section);
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

/* ESelectNamesManagerEntry routines */

static ESelectNamesManagerEntry *
get_entry_info (EEntry *entry)
{
	g_return_val_if_fail (E_IS_ENTRY (entry), NULL);
	return (ESelectNamesManagerEntry *) g_object_get_data (G_OBJECT (entry), "entry_info");
}

static void
populate_popup_cb (EEntry *eentry, GdkEventButton *ev, gint pos, GtkWidget *menu, gpointer user_data)
{
	ESelectNamesTextModel *text_model;

	g_object_get (eentry,
		      "model", &text_model,
		      NULL);
	g_assert (E_IS_SELECT_NAMES_TEXT_MODEL (text_model));

	e_select_names_populate_popup (menu, text_model, ev, pos, GTK_WIDGET (eentry));
}

#if 0
static gboolean
clean_cb (gpointer ptr)
{
	ESelectNamesManagerEntry *entry = ptr;

	e_select_names_model_clean (entry->model, TRUE);
	entry->cleaning_tag = 0;
	return FALSE;
}
#endif

static gint
focus_in_cb (GtkWidget *w, GdkEventFocus *ev, gpointer user_data)
{
	ESelectNamesManagerEntry *entry = user_data;

	if (entry->cleaning_tag) {
		g_source_remove (entry->cleaning_tag);
		entry->cleaning_tag = 0;
	}

	e_select_names_model_cancel_all_contact_load (entry->model);

	return FALSE;
}

static gint
focus_out_cb (GtkWidget *w, GdkEventFocus *ev, gpointer user_data)
{
#if 0
	/* XXX fix me */
	ESelectNamesManagerEntry *entry = user_data;
	gboolean visible = e_entry_completion_popup_is_visible (entry->entry);

	if (! visible) {
		e_select_names_model_load_all_contacts (entry->model, entry->manager->completion_book, 100);
		if (entry->cleaning_tag == 0)
			entry->cleaning_tag = gtk_timeout_add (100, clean_cb, entry);
	}
#endif
	return FALSE;
}

static void
completion_popup_cb (EEntry *w, gint visible, gpointer user_data)
{
#if 0
	/* XXX fix me */
	ESelectNamesManagerEntry *entry = user_data;

	if (!visible && !GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (entry->entry->canvas)))
		e_select_names_model_load_all_contacts (entry->model, entry->manager->completion_book, 0);
#endif
}

static void
completion_handler (EEntry *entry, ECompletionMatch *match)
{
	ESelectNamesManagerEntry *mgr_entry;
	ESelectNamesTextModel *text_model;
	EABDestination *dest;
	gint i, pos, start_pos, len;

	if (match == NULL || match->user_data == NULL)
		return;

	mgr_entry = get_entry_info (entry);
	dest = EAB_DESTINATION (match->user_data);

	/* Sometimes I really long for garbage collection.  Reference
           counting makes you feel 31337, but sometimes it is just a
           bitch. */
	g_object_ref (dest);

	g_object_get (entry,
		      "model", &text_model,
		      NULL);
	g_assert (E_IS_SELECT_NAMES_TEXT_MODEL (text_model));

	pos = e_entry_get_position (entry);
	e_select_names_model_text_pos (mgr_entry->model, text_model->seplen, pos, &i, NULL, NULL);
	e_select_names_model_replace (mgr_entry->model, i, dest);
	e_select_names_model_name_pos (mgr_entry->model, text_model->seplen, i, &start_pos, &len);
	e_entry_set_position (entry, start_pos+len);
}

static ESelectNamesManagerEntry *
e_select_names_manager_entry_new (ESelectNamesManager *manager, ESelectNamesModel *model, const gchar *id)
{
	ESelectNamesManagerEntry *entry;
	ETextModel *text_model;
	GList *l;

	g_return_val_if_fail (E_IS_SELECT_NAMES_MANAGER (manager), NULL);
	g_return_val_if_fail (E_IS_SELECT_NAMES_MODEL (model), NULL);

	entry = g_new0 (ESelectNamesManagerEntry, 1);

	entry->id = g_strdup (id);

	entry->entry = E_ENTRY (e_entry_new ());
	text_model = e_select_names_text_model_new (model);
	g_object_set(entry->entry,
		     "model",          text_model, /* The entry takes ownership of the text model */
		     "editable",       TRUE,
		     "use_ellipsis",   TRUE,
		     "allow_newlines", FALSE,
		     NULL);

	g_object_ref (entry->entry);

	entry->comp = e_select_names_completion_new (E_SELECT_NAMES_TEXT_MODEL (text_model));

	for (l = manager->completion_books; l; l = l->next) {
		EBook *book = l->data;
		e_select_names_completion_add_book (E_SELECT_NAMES_COMPLETION(entry->comp), book);
	}

	e_select_names_completion_set_minimum_query_length (E_SELECT_NAMES_COMPLETION(entry->comp),
							    manager->minimum_query_length);

	e_entry_enable_completion_full (entry->entry, entry->comp, 100, completion_handler);
		
	entry->manager = manager;

	entry->model = model;
	g_object_ref (model);
	
	g_signal_connect (entry->entry,
			  "populate_popup",
			  G_CALLBACK (populate_popup_cb),
			  entry);
			
	g_signal_connect (entry->entry->canvas,
			  "focus_in_event",
			  G_CALLBACK (focus_in_cb),
			  entry);
	
	g_signal_connect (entry->entry->canvas,
			  "focus_out_event",
			  G_CALLBACK (focus_out_cb),
			  entry);

	g_signal_connect (entry->entry,
			  "completion_popup",
			  G_CALLBACK (completion_popup_cb),
			  entry);

	g_object_set_data (G_OBJECT (entry->entry), "entry_info", entry);
	g_object_set_data (G_OBJECT (entry->entry), "select_names_model", model);
	g_object_set_data (G_OBJECT (entry->entry), "select_names_text_model", text_model);
	g_object_set_data (G_OBJECT (entry->entry), "completion_handler", entry->comp);

	return entry;
}

static void
e_select_names_manager_entry_free (ESelectNamesManagerEntry *entry)
{
	if (entry == NULL)
		return;

	g_free (entry->id);
	g_object_unref (entry->model);
	g_object_unref (entry->entry);

	if (entry->cleaning_tag)
		g_source_remove (entry->cleaning_tag);

	g_free (entry);
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

static void
e_select_names_manager_save_models (ESelectNamesManager *manager)
{
	GList *iter;
	
	for (iter = manager->sections; iter != NULL; iter = g_list_next (iter)) {
		ESelectNamesManagerSection *section = iter->data;

		if (section->original_model == NULL && section->model != NULL)
			section->original_model = e_select_names_model_duplicate (section->model);

	}
}

static void
e_select_names_manager_revert_to_saved_models (ESelectNamesManager *manager)
{
	GList *iter;

	for (iter = manager->sections; iter != NULL; iter = g_list_next (iter)) {
		ESelectNamesManagerSection *section = iter->data;
		if (section->model && section->original_model) {
			e_select_names_model_overwrite_copy (section->model, section->original_model);
			g_object_unref (section->original_model);
			section->original_model = NULL;
		}
	}
}

static void
e_select_names_manager_discard_saved_models (ESelectNamesManager *manager)
{
	GList *iter;

	for (iter = manager->sections; iter != NULL; iter = g_list_next (iter)) {
		ESelectNamesManagerSection *section = iter->data;
		if (section->original_model) {
			g_object_unref (section->original_model);
			section->original_model = NULL;
		}
	}
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */


static void
open_book_cb (EBook *book, EBookStatus status, ESelectNamesManager *manager)
{
	if (status == E_BOOK_ERROR_OK) {
		GList *l;
		for (l = manager->entries; l; l = l->next) {
			ESelectNamesManagerEntry *entry = l->data;
			e_select_names_completion_add_book (E_SELECT_NAMES_COMPLETION(entry->comp), book);
		}

		manager->completion_books = g_list_append (manager->completion_books, book);
		g_object_ref (book);
	}

	g_object_unref (manager); /* unref ourself (matches ref before the load_uri call below) */
}

static void
load_completion_books (ESelectNamesManager *manager)
{
	EFolderListItem *folders = e_folder_list_parse_xml (manager->cached_folder_list);
	EFolderListItem *f;

	for (f = folders; f && f->physical_uri; f++) {
		EBook *book = e_book_new ();
		g_object_ref (manager); /* ref ourself before our async call */

		addressbook_load_uri (book, f->physical_uri, (EBookCallback)open_book_cb, manager);
	}
	e_folder_list_free_items (folders);
}

static void
read_completion_settings_from_db (ESelectNamesManager *manager, EConfigListener *db)
{
	char *val;
	long ival;

	val = e_config_listener_get_string (db, "/apps/evolution/addressbook/completion/uris");

	if (val) {
		g_free (manager->cached_folder_list);
		manager->cached_folder_list = val;
		load_completion_books(manager);
	}

	ival = e_config_listener_get_long (db, "/apps/evolution/addressbook/completion/minimum_query_length");
	if (ival <= 0) ival = DEFAULT_MINIMUM_QUERY_LENGTH;

	manager->minimum_query_length = ival;
}

static void
db_listener (EConfigListener *db, const char *key,
	     ESelectNamesManager *manager)
{
	GList *l;

	if (!strcmp (key, "/apps/evolution/addressbook/completion/uris")) {
		char *val = e_config_listener_get_string (db, key);

		if (!val)
			return;

		if (!manager->cached_folder_list || strcmp (val, manager->cached_folder_list)) {
			for (l = manager->entries; l; l = l->next) {
				ESelectNamesManagerEntry *entry = l->data;
				e_select_names_completion_clear_books (E_SELECT_NAMES_COMPLETION (entry->comp));
			}

			g_list_foreach (manager->completion_books, (GFunc)g_object_unref, NULL);
			g_list_free (manager->completion_books);
			manager->completion_books = NULL;

			g_free (manager->cached_folder_list);
			manager->cached_folder_list = val;
			load_completion_books (manager);
		}
	}
	else if (!strcmp (key, "/apps/evolution/addressbook/completion/minimum_query_length")) {
		long ival = e_config_listener_get_long (db, key);

		if (ival <= 0)
			ival = DEFAULT_MINIMUM_QUERY_LENGTH;

		manager->minimum_query_length = ival;

		for (l = manager->entries; l; l = l->next) {
			ESelectNamesManagerEntry *entry = l->data;
			e_select_names_completion_set_minimum_query_length (E_SELECT_NAMES_COMPLETION(entry->comp),
									    manager->minimum_query_length);
		}
	}
}

/**
 * e_select_names_manager_new:
 *
 * Returns: a new #ESelectNamesManager
 */
ESelectNamesManager *
e_select_names_manager_new (void)
{
	ESelectNamesManager *manager = g_object_new (E_TYPE_SELECT_NAMES_MANAGER, NULL);
	EConfigListener *db;

	db = eab_get_config_database();

	manager->listener_id = g_signal_connect (db,
						 "key_changed",
						 G_CALLBACK (db_listener), manager);

	read_completion_settings_from_db (manager, db);

	return manager;
}


/*
 * ESelectNamesManager lifecycle management and vcard loading/saving.
 */


void
e_select_names_manager_add_section (ESelectNamesManager *manager,
				    const char *id,
				    const char *title)
{
	g_return_if_fail (E_IS_SELECT_NAMES_MANAGER (manager));
	g_return_if_fail (id != NULL);
	g_return_if_fail (title != NULL);
	
	e_select_names_manager_add_section_with_limit (manager, id, title, -1);
}

void
e_select_names_manager_add_section_with_limit (ESelectNamesManager *manager,
					       const char *id,
					       const char *title,
					       gint limit)
{
	ESelectNamesManagerSection *section;
	ESelectNamesModel *model;

	g_return_if_fail (E_IS_SELECT_NAMES_MANAGER (manager));
	g_return_if_fail (id != NULL);
	g_return_if_fail (title != NULL);
	
	model = e_select_names_model_new ();
	e_select_names_model_set_limit (model, limit);
	
	section = e_select_names_manager_section_new (manager, id, title, model);

	manager->sections = g_list_append (manager->sections, section);

	g_object_unref (model);
}

ESelectNamesModel *
e_select_names_manager_get_source (ESelectNamesManager *manager,
				   const char *id)
{
	GList *iter;

	g_return_val_if_fail (E_IS_SELECT_NAMES_MANAGER (manager), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	for (iter = manager->sections; iter != NULL; iter = g_list_next (iter)) {
		ESelectNamesManagerSection *section = iter->data;
		if (!strcmp (section->id, id))
			return section->model;
	}
	return NULL;
}

GtkWidget *
e_select_names_manager_create_entry (ESelectNamesManager *manager, const char *id)
{
	GList *iter;

	g_return_val_if_fail (E_IS_SELECT_NAMES_MANAGER (manager), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	for (iter = manager->sections; iter != NULL; iter = g_list_next (iter)) {
		ESelectNamesManagerSection *section = iter->data;
		if (!strcmp(section->id, id)) {
			ESelectNamesManagerEntry *entry;

			entry = e_select_names_manager_entry_new (manager, section->model, section->id);
			manager->entries = g_list_append (manager->entries, entry);

			return GTK_WIDGET(entry->entry);
		}
	}

	return NULL;
}

static void
e_select_names_response(ESelectNames *dialog, gint response_id, ESelectNamesManager *manager)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));

	switch(response_id) {
	case GTK_RESPONSE_OK:
		e_select_names_manager_discard_saved_models (manager);
		g_signal_emit (manager, e_select_names_manager_signals[OK], 0);
		break;

	case GTK_RESPONSE_CANCEL:
		e_select_names_manager_revert_to_saved_models (manager);
		g_signal_emit (manager, e_select_names_manager_signals[CANCEL], 0);
		break;
	}
}

static void
clear_widget (gpointer data, GObject *where_object_was)
{
	GtkWidget **widget_ref = data;
	*widget_ref = NULL;
}

void
e_select_names_manager_activate_dialog (ESelectNamesManager *manager,
					EvolutionShellClient *shell_client,
					const char *id)
{
	g_return_if_fail (E_IS_SELECT_NAMES_MANAGER (manager));
	g_return_if_fail (id != NULL);

	if (manager->names) {
		
		g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (manager->names)));
		e_select_names_set_default (manager->names, id);
		gdk_window_show (GTK_WIDGET (manager->names)->window);
		gdk_window_raise (GTK_WIDGET (manager->names)->window);

	} else {

		GList *iter;

		manager->names = E_SELECT_NAMES (e_select_names_new (shell_client));

		for (iter = manager->sections; iter != NULL; iter = g_list_next (iter)) {
			ESelectNamesManagerSection *section = iter->data;
			e_select_names_add_section (manager->names, section->id, section->title, section->model);
		}

		e_select_names_set_default (manager->names, id);

		g_signal_connect(manager->names, 
				 "response",
				 G_CALLBACK(e_select_names_response),
				 manager);

		g_object_weak_ref (G_OBJECT (manager->names), clear_widget, &manager->names);

		gtk_widget_show(GTK_WIDGET(manager->names));
	}

	e_select_names_manager_save_models (manager);
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

static void
e_select_names_manager_init (ESelectNamesManager *manager)
{
	manager->sections = NULL;
	manager->entries  = NULL;
	manager->completion_books  = NULL;
	manager->cached_folder_list  = NULL;
}

static void
e_select_names_manager_dispose (GObject *object)
{
	ESelectNamesManager *manager;
	
	manager = E_SELECT_NAMES_MANAGER (object);

	if (manager->names) {
		gtk_widget_destroy (GTK_WIDGET (manager->names));
		manager->names = NULL;
	}

	if (manager->sections) {
		g_list_foreach (manager->sections, (GFunc) e_select_names_manager_section_free, NULL);
		g_list_free (manager->sections);
		manager->sections = NULL;
	}

	if (manager->entries) {
		g_list_foreach (manager->entries, (GFunc) e_select_names_manager_entry_free, NULL);
		g_list_free (manager->entries);
		manager->entries = NULL;
	}

	if (manager->completion_books) {
		g_list_foreach (manager->completion_books, (GFunc) g_object_unref, NULL);
		g_list_free (manager->completion_books);
		manager->completion_books = NULL;
	}

	if (manager->listener_id) {
		g_signal_handler_disconnect (eab_get_config_database(), manager->listener_id);
		manager->listener_id = 0;
	}

	if (manager->cached_folder_list) {
		g_free (manager->cached_folder_list);
		manager->cached_folder_list = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_select_names_manager_class_init (ESelectNamesManagerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = e_select_names_manager_dispose;

	e_select_names_manager_signals[CHANGED] = 
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESelectNamesManagerClass, changed),
			      NULL, NULL,
			      e_select_names_marshal_NONE__POINTER_INT,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER,
			      G_TYPE_INT);

	e_select_names_manager_signals[OK] =
		g_signal_new ("ok",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESelectNamesManagerClass, ok),
			      NULL, NULL,
			      e_select_names_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);

	e_select_names_manager_signals[CANCEL] =
		g_signal_new ("cancel",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESelectNamesManagerClass, cancel),
			      NULL, NULL,
			      e_select_names_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

/**
 * e_select_names_manager_get_type:
 * @void: 
 * 
 * Registers the &ESelectNamesManager class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &ESelectNamesManager class.
 **/
GType
e_select_names_manager_get_type (void)
{
	static GType manager_type = 0;

	if (!manager_type) {
		static const GTypeInfo manager_info =  {
			sizeof (ESelectNamesManagerClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_select_names_manager_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ESelectNamesManager),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_select_names_manager_init,
		};

		manager_type = g_type_register_static (G_TYPE_OBJECT, "ESelectNamesManager", &manager_info, 0);
	}

	return manager_type;
}



