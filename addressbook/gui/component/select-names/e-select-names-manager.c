/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gal/e-text/e-entry.h>

#include "e-select-names-manager.h"
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"
#include "e-select-names.h"
#include "e-select-names-completion.h"
#include "e-select-names-popup.h"
#include <addressbook/backend/ebook/e-destination.h>
#include <addressbook/gui/component/addressbook.h>
#include <bonobo-conf/bonobo-config-database.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-moniker-util.h>

/* Object argument IDs */
enum {
	ARG_0,
	ARG_CARD,
};

enum {
	CHANGED,
	OK,
	LAST_SIGNAL
};

static guint e_select_names_manager_signals[LAST_SIGNAL] = { 0 };


typedef struct {
	char *id;
	char *title;
	ESelectNamesModel *model;
	ESelectNamesModel *original_model;
	ESelectNamesManager *manager;
	guint changed_handler;
} ESelectNamesManagerSection;

typedef struct {
	char *id;
	EEntry *entry;
} ESelectNamesManagerEntry;

static void e_select_names_manager_init (ESelectNamesManager *manager);
static void e_select_names_manager_class_init (ESelectNamesManagerClass *klass);

static void e_select_names_manager_destroy (GtkObject *object);
static void e_select_names_manager_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_manager_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

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

static void
open_book_cb (EBook *book, EBookStatus status, ESelectNamesManager *manager)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		gtk_object_unref (GTK_OBJECT (book));
		manager->completion_book = NULL;
	}

	gtk_object_unref (GTK_OBJECT (manager)); /* unref ourself (matches ref before the load_uri call below) */
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
	CORBA_Environment ev;
	char *val;

	CORBA_exception_init (&ev);

	db = addressbook_config_database (&ev);

	val = bonobo_config_get_string (db, "/Addressbook/Completion/uri", &ev);

	CORBA_exception_free (&ev);

	if (val) {
		manager->completion_book = e_book_new ();
		gtk_object_ref (GTK_OBJECT (manager)); /* ref ourself before our async call */
		addressbook_load_uri (manager->completion_book, val, (EBookCallback)open_book_cb, manager);
		g_free (val);
	}
	else
		manager->completion_book = NULL;

	return manager;
}

static void
e_select_names_manager_class_init (ESelectNamesManagerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS(klass);

	gtk_object_add_arg_type ("ESelectNamesManager::card",
				 GTK_TYPE_OBJECT, GTK_ARG_READWRITE, ARG_CARD);

	object_class->destroy = e_select_names_manager_destroy;
	object_class->get_arg = e_select_names_manager_get_arg;
	object_class->set_arg = e_select_names_manager_set_arg;

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

	gtk_object_class_add_signals (object_class, e_select_names_manager_signals, LAST_SIGNAL);
}

/*
 * ESelectNamesManager lifecycle management and vcard loading/saving.
 */

static void
e_select_names_manager_destroy (GtkObject *object)
{
	ESelectNamesManager *manager;
	
	manager = E_SELECT_NAMES_MANAGER (object);

	gtk_object_unref(GTK_OBJECT(manager->sections));
	gtk_object_unref(GTK_OBJECT(manager->entries));

	if (manager->names) {
		gtk_widget_destroy (GTK_WIDGET (manager->names));
		manager->names = NULL;
	}
}


/* Set_arg handler for the manager */
static void
e_select_names_manager_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesManager *manager;
	
	manager = E_SELECT_NAMES_MANAGER (object);

	switch (arg_id) {
	case ARG_CARD:
		break;
	default:
		return;
	}
}

/* Get_arg handler for the manager */
static void
e_select_names_manager_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesManager *manager;

	manager = E_SELECT_NAMES_MANAGER (object);

	switch (arg_id) {
	case ARG_CARD:
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void *
section_copy(const void *sec, void *data)
{
	const ESelectNamesManagerSection *section = sec;
	ESelectNamesManagerSection *newsec;

	static void section_model_changed_cb (ESelectNamesModel *, gpointer);

	newsec = g_new(ESelectNamesManagerSection, 1);
	newsec->id = g_strdup(section->id);
	newsec->title = g_strdup(section->title);
	newsec->model = section->model;
	newsec->original_model = section->original_model;
	newsec->manager = section->manager;
	newsec->changed_handler = gtk_signal_connect (GTK_OBJECT (newsec->model),
						      "changed",
						      GTK_SIGNAL_FUNC (section_model_changed_cb),
						      newsec);

	if (newsec->model)
		gtk_object_ref(GTK_OBJECT(newsec->model));
	if (newsec->original_model)
		gtk_object_ref(GTK_OBJECT(newsec->original_model));
	
	return newsec;
}

static void
section_free(void *sec, void *data)
{
	ESelectNamesManagerSection *section = sec;
	if (section->manager && section->changed_handler) {
		gtk_signal_disconnect (GTK_OBJECT (section->model), section->changed_handler);
	}
	g_free(section->id);
	g_free(section->title);
	if (section->model)
		gtk_object_unref (GTK_OBJECT(section->model));
	if (section->original_model)
		gtk_object_unref (GTK_OBJECT (section->original_model));

	g_free(section);
}

static void *
entry_copy(const void *ent, void *data)
{
	const ESelectNamesManagerEntry *entry = ent;
	ESelectNamesManagerEntry *newent;
	
	newent = g_new(ESelectNamesManagerEntry, 1);
	newent->id = g_strdup(entry->id);
	newent->entry = entry->entry;
	if (newent->entry)
		gtk_object_ref(GTK_OBJECT(newent->entry));
	return newent;
}

static void
entry_free(void *ent, void *data)
{
	ESelectNamesManagerEntry *entry = ent;
	g_free(entry->id);
	if (entry->entry)
		gtk_object_unref(GTK_OBJECT(entry->entry));
	g_free(entry);
}

/**
 * e_select_names_manager_init:
 */
static void
e_select_names_manager_init (ESelectNamesManager *manager)
{
	manager->sections = e_list_new(section_copy, section_free, manager);
	manager->entries  = e_list_new(entry_copy, entry_free, manager);
}

static void
section_model_changed_cb (ESelectNamesModel *model, gpointer closure)
{
	ESelectNamesManagerSection *section = closure;
	gtk_signal_emit (GTK_OBJECT (section->manager),
			 e_select_names_manager_signals[CHANGED],
			 section->id,
			 FALSE);
}

static void
section_model_working_copy_changed_cb (ESelectNamesModel *model, gpointer closure)
{
	ESelectNamesManagerSection *section = closure;
	gtk_signal_emit (GTK_OBJECT (section->manager),
			 e_select_names_manager_signals[CHANGED],
			 section->id,
			 TRUE);
}

void
e_select_names_manager_add_section (ESelectNamesManager *manager,
				    const char *id,
				    const char *title)
{
	e_select_names_manager_add_section_with_limit (manager, id, title, -1);
}

void
e_select_names_manager_add_section_with_limit (ESelectNamesManager *manager,
					       const char *id,
					       const char *title,
					       gint limit)
{
	ESelectNamesManagerSection *section;
	
	section = g_new(ESelectNamesManagerSection, 1);
	section->id = g_strdup(id);
	section->title = g_strdup(title);

	section->model = e_select_names_model_new();
	e_select_names_model_set_limit (section->model, limit);

	section->original_model = NULL;

	section->manager = manager;

	section->changed_handler = gtk_signal_connect (GTK_OBJECT (section->model),
						       "changed",
						       GTK_SIGNAL_FUNC (section_model_changed_cb),
						       section);

	e_list_append(manager->sections, section);
	section_free(section, manager);
}

ESelectNamesModel *
e_select_names_manager_get_source (ESelectNamesManager *manager, const char *id)
{
	EIterator *iterator;

	g_return_val_if_fail (manager && E_IS_SELECT_NAMES_MANAGER (manager), NULL);
	g_return_val_if_fail (id, NULL);

	iterator = e_list_get_iterator (manager->sections);
	for (e_iterator_reset (iterator); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
		const ESelectNamesManagerSection *section = e_iterator_get (iterator);
		if (!strcmp (section->id, id)) {
			return section->model;
		}
	}

	return NULL;
}

static void
entry_destroyed(EEntry *entry, ESelectNamesManager *manager)
{
	if(!GTK_OBJECT_DESTROYED(manager)) {
		EIterator *iterator = e_list_get_iterator(manager->entries);
		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			const ESelectNamesManagerEntry *this_entry = e_iterator_get(iterator);
			if(entry == this_entry->entry) {
				e_iterator_delete(iterator);
				break;
			}
		}
	}
}

static void
completion_handler (EEntry *entry, ECompletionMatch *match)
{
	ESelectNamesModel *snm;
	EDestination *dest;
	gint i, pos, start_pos, len;

	if (match == NULL || match->user_data == NULL)
		return;


	snm = E_SELECT_NAMES_MODEL (gtk_object_get_data (GTK_OBJECT (entry), "select_names_model"));
	dest = E_DESTINATION (match->user_data);

	/* Sometimes I really long for garbage collection.  Reference
           counting makes you feel 31337, but sometimes it is just a
           bitch. */
	gtk_object_ref (GTK_OBJECT (dest));

	pos = e_entry_get_position (entry);
	e_select_names_model_text_pos (snm, pos, &i, NULL, NULL);
	e_select_names_model_replace (snm, i, dest);
	e_select_names_model_name_pos (snm, i, &start_pos, &len);
	e_entry_set_position (entry, start_pos+len);
}

static void
popup_cb (EEntry *entry, GdkEventButton *ev, gint pos, ESelectNamesModel *model)
{
	e_select_names_popup (model, ev, pos);
}

static gint
focus_in_cb (GtkWidget *w, GdkEventFocus *ev, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);
	ESelectNamesModel *model = E_SELECT_NAMES_MODEL (gtk_object_get_data (GTK_OBJECT (entry), "select_names_model"));

	e_select_names_model_cancel_cardify_all (model);

	return FALSE;
}

static gint
focus_out_cb (GtkWidget *w, GdkEventFocus *ev, gpointer user_data)
{
	EEntry *entry = E_ENTRY (user_data);
	ESelectNamesModel *model = E_SELECT_NAMES_MODEL (gtk_object_get_data (GTK_OBJECT (entry), "select_names_model"));
	ESelectNamesManager *manager = E_SELECT_NAMES_MANAGER (gtk_object_get_data (GTK_OBJECT (entry), "select_names_manager"));

	e_select_names_model_clean (model);

	if (!e_entry_completion_popup_is_visible (entry))
		e_select_names_model_cardify_all (model, manager->completion_book, 100);

	return FALSE;
}

static void
completion_popup_cb (EEntry *entry, gint visible, gpointer user_data)
{
	ESelectNamesModel *model = E_SELECT_NAMES_MODEL (gtk_object_get_data (GTK_OBJECT (entry), "select_names_model"));
	ESelectNamesManager *manager = E_SELECT_NAMES_MANAGER (gtk_object_get_data (GTK_OBJECT (entry), "select_names_manager"));

	if (!visible && !GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (entry->canvas)))
		e_select_names_model_cardify_all (model, manager->completion_book, 0);
}

GtkWidget *
e_select_names_manager_create_entry (ESelectNamesManager *manager, const char *id)
{
	ETextModel *model;
	EIterator *iterator;
	iterator = e_list_get_iterator(manager->sections);
	for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const ESelectNamesManagerSection *section = e_iterator_get(iterator);
		if (!strcmp(section->id, id)) {
			ESelectNamesManagerEntry *entry;
			EEntry *eentry;
			ECompletion *comp;

			eentry = E_ENTRY (e_entry_new ());
			gtk_object_set_data (GTK_OBJECT (eentry), "select_names_model", section->model);
			gtk_object_set_data (GTK_OBJECT (eentry), "select_names_manager", manager);

			gtk_signal_connect (GTK_OBJECT (eentry),
					    "popup",
					    GTK_SIGNAL_FUNC (popup_cb),
					    section->model);
			
			gtk_signal_connect (GTK_OBJECT (eentry->canvas),
					    "focus_in_event",
					    GTK_SIGNAL_FUNC (focus_in_cb),
					    eentry);
			gtk_signal_connect (GTK_OBJECT (eentry->canvas),
					    "focus_out_event",
					    GTK_SIGNAL_FUNC (focus_out_cb),
					    eentry);
			gtk_signal_connect (GTK_OBJECT (eentry),
					    "completion_popup",
					    GTK_SIGNAL_FUNC (completion_popup_cb),
					    NULL);

			entry = g_new (ESelectNamesManagerEntry, 1);
			entry->entry = eentry;
			entry->id = (char *)id;

			gtk_object_ref (GTK_OBJECT (entry->entry));

			model = e_select_names_text_model_new (section->model);
			e_list_append (manager->entries, entry);
			g_free(entry);

			comp = e_select_names_completion_new (NULL, section->model);
			if (manager->completion_book)
				e_select_names_completion_add_book (E_SELECT_NAMES_COMPLETION (comp),
								    manager->completion_book);
			e_entry_enable_completion_full (eentry, comp, 50, completion_handler);

			gtk_object_set_data (GTK_OBJECT (eentry), "completion_handler", comp);

			gtk_object_set(GTK_OBJECT(eentry),
				       "model", model,
				       "editable", TRUE,
				       "use_ellipsis", TRUE,
				       "allow_newlines", FALSE,
				       NULL);

			gtk_signal_connect(GTK_OBJECT(eentry), "destroy",
					   GTK_SIGNAL_FUNC(entry_destroyed), manager);

			return GTK_WIDGET(eentry);
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
		/* We don't need to do much if they click on OK */

		gtk_signal_emit (GTK_OBJECT (manager), e_select_names_manager_signals[OK]);
		break;

	case 1: {
		EList *list = manager->sections;
		EIterator *iterator = e_list_get_iterator(list);

		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			ESelectNamesManagerSection *section = (void *) e_iterator_get(iterator);
			e_select_names_model_overwrite_copy (section->model, section->original_model);
		}
		
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	}
	}
}

void
e_select_names_manager_activate_dialog (ESelectNamesManager *manager,
					const char *id)
{
	EIterator *iterator;
	
	if (manager->names) {
		g_assert (GTK_WIDGET_REALIZED (GTK_WIDGET (manager->names)));
		e_select_names_set_default(manager->names, id);
		gdk_window_show (GTK_WIDGET (manager->names)->window);
		gdk_window_raise (GTK_WIDGET (manager->names)->window);
	} else {
		manager->names = E_SELECT_NAMES (e_select_names_new ());

		iterator = e_list_get_iterator(manager->sections);
		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			ESelectNamesManagerSection *section = (ESelectNamesManagerSection *) e_iterator_get(iterator);
			if (section->original_model != NULL)
				gtk_object_unref (GTK_OBJECT (section->original_model));
			section->original_model = e_select_names_model_duplicate (section->model);
			e_select_names_add_section (manager->names, section->id, section->title, section->model);
			gtk_signal_connect (GTK_OBJECT (section->model),
					    "changed",
					    GTK_SIGNAL_FUNC (section_model_working_copy_changed_cb),
					    (gpointer)section); /* casting out const to avoid compiler warning */
		}
		e_select_names_set_default(manager->names, id);
		gtk_signal_connect(GTK_OBJECT(manager->names), "clicked",
				   GTK_SIGNAL_FUNC(e_select_names_clicked), manager);
		gtk_signal_connect(GTK_OBJECT(manager->names), "destroy",
				   GTK_SIGNAL_FUNC(gtk_widget_destroyed), 
				   &manager->names);
		gtk_widget_show(GTK_WIDGET(manager->names));
	}
}

