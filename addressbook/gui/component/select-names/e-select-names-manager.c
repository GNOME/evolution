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
#include "e-select-names-config.h"
#include "e-select-names-manager.h"
#include "e-select-names-marshal.h"
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"
#include "e-select-names.h"
#include "e-select-names-completion.h"
#include "e-select-names-popup.h"
#include <addressbook/util/eab-book-util.h>
#include <addressbook/util/e-destination.h>
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
open_book_cb (EBook *book, EBookStatus status, gpointer user_data)
{
	ESelectNamesManager *manager = E_SELECT_NAMES_MANAGER (user_data);

	if (status == E_BOOK_ERROR_OK) {
		GList *l;
		for (l = manager->entries; l; l = l->next) {
			ESelectNamesManagerEntry *entry = l->data;
			e_select_names_completion_add_book (E_SELECT_NAMES_COMPLETION(entry->comp), book);
		}

		manager->completion_books = g_list_append (manager->completion_books, book);
		g_object_ref (book);
	}

	g_object_unref (manager); /* unref ourself (matches ref before the load_source call below) */
}

static void
update_completion_books (ESelectNamesManager *manager)
{
	GSList *groups;

	/* Add all the completion books */
	for (groups = e_source_list_peek_groups (manager->source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;

		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const char *completion = e_source_get_property (source, "completion");
			if (completion && !g_ascii_strcasecmp (completion, "true")) {
				EBook *book = e_book_new (source, NULL);
				g_object_ref (manager);
				addressbook_load (book, open_book_cb, manager);
			}
		}
	}
}

static void
source_list_changed (ESourceList *source_list, ESelectNamesManager *manager)
{
	GList *l;
	
	for (l = manager->entries; l; l = l->next) {
		ESelectNamesManagerEntry *entry = l->data;
		e_select_names_completion_clear_books (E_SELECT_NAMES_COMPLETION (entry->comp));
	}

	g_list_foreach (manager->completion_books, (GFunc)g_object_unref, NULL);
	g_list_free (manager->completion_books);
	manager->completion_books = NULL;

	update_completion_books (manager);
}

static void
config_min_query_length_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	ESelectNamesManager *manager = data;
	GList *l;
	
	manager->minimum_query_length = e_select_names_config_get_min_query_length ();
	if (manager->minimum_query_length <= 0)
		manager->minimum_query_length = DEFAULT_MINIMUM_QUERY_LENGTH;
	
	for (l = manager->entries; l; l = l->next) {
		ESelectNamesManagerEntry *entry = l->data;
		e_select_names_completion_set_minimum_query_length (E_SELECT_NAMES_COMPLETION(entry->comp),
								    manager->minimum_query_length);
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
	guint not;
	
	manager->sections = NULL;
	manager->entries  = NULL;

	manager->source_list =  e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");
	g_signal_connect (manager->source_list, "changed", G_CALLBACK (source_list_changed), manager);

	manager->completion_books  = NULL;

	manager->minimum_query_length = e_select_names_config_get_min_query_length ();

	update_completion_books (manager);

	not = e_select_names_config_add_notification_min_query_length (config_min_query_length_changed_cb, manager);
	manager->notifications = g_list_append (manager->notifications, GUINT_TO_POINTER (not));
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

	if (manager->source_list) {
		g_object_unref (manager->source_list);
		manager->source_list = NULL;
	}
	
	if (manager->completion_books) {
		g_list_foreach (manager->completion_books, (GFunc) g_object_unref, NULL);
		g_list_free (manager->completion_books);
		manager->completion_books = NULL;
	}

	if (manager->notifications) {
		GList *l;
		
		for (l = manager->notifications; l; l = l->next)
			e_select_names_config_remove_notification (GPOINTER_TO_UINT (l->data));
		g_list_free (manager->notifications);
		manager->notifications = NULL;
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



