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
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"
#include "e-select-names.h"
#include "e-select-names-completion.h"
#include "e-select-names-popup.h"
#include "e-folder-list.h"
#include <addressbook/backend/ebook/e-destination.h>
#include <addressbook/gui/component/addressbook.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>

enum {
	CHANGED,
	OK,
	CANCEL,
	LAST_SIGNAL
};

static guint e_select_names_manager_signals[LAST_SIGNAL] = { 0 };


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

static void e_select_names_manager_destroy (GtkObject *object);

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

/* ESelectNamesManagerSection routines */

static void
section_model_changed_cb (ESelectNamesModel *model, gpointer closure)
{
	ESelectNamesManagerSection *section = closure;
	gtk_signal_emit (GTK_OBJECT (section->manager),
			 e_select_names_manager_signals[CHANGED],
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
	gtk_object_ref (GTK_OBJECT (section->model));
	section->changed_tag =
		gtk_signal_connect (GTK_OBJECT (section->model),
				    "changed",
				    GTK_SIGNAL_FUNC (section_model_changed_cb),
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
		gtk_signal_disconnect (GTK_OBJECT (section->model), section->changed_tag);
		gtk_object_unref (GTK_OBJECT (section->model));
	}

	if (section->original_model) {
		gtk_object_unref (GTK_OBJECT (section->original_model));
	}

	g_free (section);
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

/* ESelectNamesManagerEntry routines */

static ESelectNamesManagerEntry *
get_entry_info (EEntry *entry)
{
	g_return_val_if_fail (E_IS_ENTRY (entry), NULL);
	return (ESelectNamesManagerEntry *) gtk_object_get_data (GTK_OBJECT (entry), "entry_info");
}

static void
popup_cb (EEntry *eentry, GdkEventButton *ev, gint pos, gpointer user_data)
{
	ESelectNamesTextModel *text_model;

	gtk_object_get (GTK_OBJECT (eentry),
			"model", &text_model,
			NULL);
	g_assert (E_IS_SELECT_NAMES_TEXT_MODEL (text_model));

	e_select_names_popup (text_model, ev, pos);
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
		gtk_timeout_remove (entry->cleaning_tag);
		entry->cleaning_tag = 0;
	}

	e_select_names_model_cancel_cardify_all (entry->model);

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
		e_select_names_model_cardify_all (entry->model, entry->manager->completion_book, 100);
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
		e_select_names_model_cardify_all (entry->model, entry->manager->completion_book, 0);
#endif
}

static void
completion_handler (EEntry *entry, ECompletionMatch *match)
{
	ESelectNamesManagerEntry *mgr_entry;
	ESelectNamesTextModel *text_model;
	EDestination *dest;
	gint i, pos, start_pos, len;

	if (match == NULL || match->user_data == NULL)
		return;

	mgr_entry = get_entry_info (entry);
	dest = E_DESTINATION (match->user_data);

	/* Sometimes I really long for garbage collection.  Reference
           counting makes you feel 31337, but sometimes it is just a
           bitch. */
	gtk_object_ref (GTK_OBJECT (dest));

	gtk_object_get (GTK_OBJECT (entry),
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
	gtk_object_set(GTK_OBJECT(entry->entry),
		       "model",          text_model, /* The entry takes ownership of the text model */
		       "editable",       TRUE,
		       "use_ellipsis",   TRUE,
		       "allow_newlines", FALSE,
		       NULL);

	gtk_object_ref (GTK_OBJECT (entry->entry));

	entry->comp = e_select_names_completion_new (E_SELECT_NAMES_TEXT_MODEL (text_model));

	for (l = manager->completion_books; l; l = l->next) {
		EBook *book = l->data;
		e_select_names_completion_add_book (E_SELECT_NAMES_COMPLETION(entry->comp), book);
	}

	e_entry_enable_completion_full (entry->entry, entry->comp, 100, completion_handler);
		
	entry->manager = manager;

	entry->model = model;
	gtk_object_ref (GTK_OBJECT (model));
	
	gtk_signal_connect (GTK_OBJECT (entry->entry),
			    "popup",
			    GTK_SIGNAL_FUNC (popup_cb),
			    entry);
			
	gtk_signal_connect (GTK_OBJECT (entry->entry->canvas),
			    "focus_in_event",
			    GTK_SIGNAL_FUNC (focus_in_cb),
			    entry);
	
	gtk_signal_connect (GTK_OBJECT (entry->entry->canvas),
			    "focus_out_event",
			    GTK_SIGNAL_FUNC (focus_out_cb),
			    entry);

	gtk_signal_connect (GTK_OBJECT (entry->entry),
			    "completion_popup",
			    GTK_SIGNAL_FUNC (completion_popup_cb),
			    entry);

	gtk_object_set_data (GTK_OBJECT (entry->entry), "entry_info", entry);
	gtk_object_set_data (GTK_OBJECT (entry->entry), "select_names_model", model);
	gtk_object_set_data (GTK_OBJECT (entry->entry), "select_names_text_model", text_model);
	gtk_object_set_data (GTK_OBJECT (entry->entry), "completion_handler", entry->comp);

	return entry;
}

static void
e_select_names_manager_entry_free (ESelectNamesManagerEntry *entry)
{
	if (entry == NULL)
		return;

	g_free (entry->id);
	gtk_object_unref (GTK_OBJECT (entry->model));
	gtk_object_unref (GTK_OBJECT (entry->entry));

	if (entry->cleaning_tag)
		gtk_timeout_remove (entry->cleaning_tag);

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
			gtk_object_unref (GTK_OBJECT (section->original_model));
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
			gtk_object_unref (GTK_OBJECT (section->original_model));
			section->original_model = NULL;
		}
	}
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */


static void
open_book_cb (EBook *book, EBookStatus status, ESelectNamesManager *manager)
{
	if (status == E_BOOK_STATUS_SUCCESS) {
		GList *l;
		for (l = manager->entries; l; l = l->next) {
			ESelectNamesManagerEntry *entry = l->data;
			e_select_names_completion_add_book (E_SELECT_NAMES_COMPLETION(entry->comp), book);
		}

		manager->completion_books = g_list_append (manager->completion_books, book);
		gtk_object_ref (GTK_OBJECT (book));
	}

	gtk_object_unref (GTK_OBJECT (manager)); /* unref ourself (matches ref before the load_uri call below) */
}

static void
load_completion_books (ESelectNamesManager *manager)
{
	EFolderListItem *folders = e_folder_list_parse_xml (manager->cached_folder_list);
	EFolderListItem *f;

	for (f = folders; f && f->physical_uri; f++) {
		char *uri;
		EBook *book = e_book_new ();
		gtk_object_ref (GTK_OBJECT (manager)); /* ref ourself before our async call */

		if (!strncmp (f->physical_uri, "file:", 5))
			uri = g_strdup_printf ("%s/addressbook.db", f->physical_uri);
		else
			uri = g_strdup (f->physical_uri);
		addressbook_load_uri (book, uri, (EBookCallback)open_book_cb, manager);
	}
	e_folder_list_free_items (folders);
}

static void
read_completion_books_from_db (ESelectNamesManager *manager)
{
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	char *val;

	CORBA_exception_init (&ev);

	db = addressbook_config_database (&ev);
		
	val = bonobo_config_get_string (db, "/Addressbook/Completion/uris", &ev);

	CORBA_exception_free (&ev);

	if (val) {
		g_free (manager->cached_folder_list);
		manager->cached_folder_list = val;
		load_completion_books(manager);
	}
}

static void
uris_listener (BonoboListener *listener, char *event_name, 
	       CORBA_any *any, CORBA_Environment *ev,
	       gpointer user_data)
{
	ESelectNamesManager *manager = E_SELECT_NAMES_MANAGER (user_data);
	GList *l;
	Bonobo_ConfigDatabase db;
	char *val;

	db = addressbook_config_database (NULL);
		
	val = bonobo_config_get_string (db, "/Addressbook/Completion/uris", NULL);

	if (val) {
		if (!manager->cached_folder_list || strcmp (val, manager->cached_folder_list)) {
			for (l = manager->entries; l; l = l->next) {
				ESelectNamesManagerEntry *entry = l->data;
				e_select_names_completion_clear_books (E_SELECT_NAMES_COMPLETION (entry->comp));
			}

			g_list_foreach (manager->completion_books, (GFunc)gtk_object_unref, NULL);
			g_list_free (manager->completion_books);
			manager->completion_books = NULL;

			g_free (manager->cached_folder_list);
			manager->cached_folder_list = val;
			load_completion_books (manager);
		}
	}
}

/**
 * e_select_names_manager_new:
 * @VCard: a string in vCard format
 *
 * Returns: a new #ESelectNamesManager that wraps the @VCard.
 */
ESelectNamesManager *
e_select_names_manager_new (void)
{
	ESelectNamesManager *manager = E_SELECT_NAMES_MANAGER(gtk_type_new(e_select_names_manager_get_type()));
	Bonobo_ConfigDatabase db;

	db = addressbook_config_database (NULL);

	manager->listener_id = bonobo_event_source_client_add_listener (db, uris_listener,
									"Bonobo/ConfigDatabase:change/Addressbook/Completion:",
									NULL,
									manager);

	read_completion_books_from_db (manager);

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

	gtk_object_unref (GTK_OBJECT (model));
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
e_select_names_clicked(ESelectNames *dialog, gint button, ESelectNamesManager *manager)
{
	gnome_dialog_close(GNOME_DIALOG(dialog));

	switch(button) {
	case 0:
		e_select_names_manager_discard_saved_models (manager);
		gtk_signal_emit (GTK_OBJECT (manager), e_select_names_manager_signals[OK]);
		break;

	case 1:
		e_select_names_manager_revert_to_saved_models (manager);
		gtk_signal_emit (GTK_OBJECT (manager), e_select_names_manager_signals[CANCEL]);
		break;
	}
}

void
e_select_names_manager_activate_dialog (ESelectNamesManager *manager,
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

		manager->names = E_SELECT_NAMES (e_select_names_new ());

		for (iter = manager->sections; iter != NULL; iter = g_list_next (iter)) {
			ESelectNamesManagerSection *section = iter->data;
			e_select_names_add_section (manager->names, section->id, section->title, section->model);
		}

		e_select_names_set_default (manager->names, id);

		gtk_signal_connect(GTK_OBJECT(manager->names), 
				   "clicked",
				   GTK_SIGNAL_FUNC(e_select_names_clicked),
				   manager);

		gtk_signal_connect(GTK_OBJECT(manager->names),
				   "destroy",
				   GTK_SIGNAL_FUNC(gtk_widget_destroyed), 
				   &manager->names);

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
e_select_names_manager_destroy (GtkObject *object)
{
	ESelectNamesManager *manager;
	
	manager = E_SELECT_NAMES_MANAGER (object);

	if (manager->names) {
		gtk_widget_destroy (GTK_WIDGET (manager->names));
		manager->names = NULL;
	}

	g_list_foreach (manager->sections, (GFunc) e_select_names_manager_section_free, NULL);
	g_list_free (manager->sections);
	manager->sections = NULL;

	g_list_foreach (manager->entries, (GFunc) e_select_names_manager_entry_free, NULL);
	g_list_free (manager->entries);
	manager->entries = NULL;

	g_list_foreach (manager->completion_books, (GFunc) gtk_object_unref, NULL);
	g_list_free (manager->completion_books);
	manager->completion_books = NULL;

	bonobo_event_source_client_remove_listener (addressbook_config_database (NULL), manager->listener_id, NULL);

	g_free (manager->cached_folder_list);
}

static void
e_select_names_manager_class_init (ESelectNamesManagerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	object_class->destroy = e_select_names_manager_destroy;

	e_select_names_manager_signals[CHANGED] = 
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESelectNamesManagerClass, changed),
				gtk_marshal_NONE__POINTER_INT,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_POINTER,
				GTK_TYPE_INT);

	e_select_names_manager_signals[OK] =
		gtk_signal_new ("ok",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESelectNamesManagerClass, ok),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	e_select_names_manager_signals[CANCEL] =
		gtk_signal_new ("cancel",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ESelectNamesManagerClass, cancel),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_select_names_manager_signals, LAST_SIGNAL);
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
GtkType
e_select_names_manager_get_type (void)
{
	static GtkType manager_type = 0;

	if (!manager_type) {
		GtkTypeInfo manager_info = {
			"ESelectNamesManager",
			sizeof (ESelectNamesManager),
			sizeof (ESelectNamesManagerClass),
			(GtkClassInitFunc) e_select_names_manager_class_init,
			(GtkObjectInitFunc) e_select_names_manager_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		manager_type = gtk_type_unique (gtk_object_get_type (), &manager_info);
	}

	return manager_type;
}



