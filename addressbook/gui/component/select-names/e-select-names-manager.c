/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: 
 *   Chris Lahey     <clahey@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "e-select-names-manager.h"
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"
#include "e-select-names.h"
#include "widgets/e-text/e-entry.h"

/* Object argument IDs */
enum {
	ARG_0,
	ARG_CARD,
};


typedef struct {
	char *id;
	char *title;
	ESelectNamesModel *model;
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
	
	newsec = g_new(ESelectNamesManagerSection, 1);
	newsec->id = g_strdup(section->id);
	newsec->title = g_strdup(section->title);
	newsec->model = section->model;
	if (newsec->model)
		gtk_object_ref(GTK_OBJECT(newsec->model));
	return newsec;
}

static void
section_free(void *sec, void *data)
{
	ESelectNamesManagerSection *section = sec;
	g_free(section->id);
	g_free(section->title);
	if (section->model)
		gtk_object_unref(GTK_OBJECT(section->model));
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

void                          e_select_names_manager_add_section               (ESelectNamesManager *manager,
										const char *id,
										const char *title)
{
	ESelectNamesManagerSection *section;
	
	section = g_new(ESelectNamesManagerSection, 1);
	section->id = g_strdup(id);
	section->title = g_strdup(title);
	section->model = e_select_names_model_new();
	e_list_append(manager->sections, section);
	section_free(section, manager);
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
	gtk_object_unref(GTK_OBJECT(manager));
}

GtkWidget                    *e_select_names_manager_create_entry              (ESelectNamesManager *manager,
										const char *id)
{
	ETextModel *model;
	EIterator *iterator;
	iterator = e_list_get_iterator(manager->sections);
	for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const ESelectNamesManagerSection *section = e_iterator_get(iterator);
		if (!strcmp(section->id, id)) {
			ESelectNamesManagerEntry *entry;
			EEntry *eentry;
			eentry = E_ENTRY(e_entry_new());

			entry = g_new(ESelectNamesManagerEntry, 1);
			entry->entry = eentry;
			entry->id = (char *)id;
			model = e_select_names_text_model_new(section->model);
			e_list_append(manager->entries, entry);
			g_free(entry);

			gtk_object_set(GTK_OBJECT(eentry),
				       "model", model,
				       "editable", TRUE,
				       "use_ellipsis", TRUE,
				       NULL);
			gtk_signal_connect(GTK_OBJECT(eentry), "destroy",
					   GTK_SIGNAL_FUNC(entry_destroyed), manager);
			gtk_object_ref(GTK_OBJECT(manager));
			return GTK_WIDGET(eentry);
		}
	}
	return NULL;
}

static void
e_select_names_clicked(ESelectNames *dialog, gint button, ESelectNamesManager *manager)
{
	switch(button) {
	case 0: {
		EList *list = manager->sections;
		EIterator *iterator = e_list_get_iterator(list);
		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			ESelectNamesManagerSection *section = (void *) e_iterator_get(iterator);
			ESelectNamesModel *source = e_select_names_get_source(dialog, section->id);
			if (section->model)
				gtk_object_unref(GTK_OBJECT(section->model));
			section->model = source;
			/* Don't ref because get_source returns a conceptual ref_count of 1. */
		}
		gtk_object_unref(GTK_OBJECT(iterator));

		list = manager->entries;
		iterator = e_list_get_iterator(list);
		for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
			ESelectNamesManagerEntry *entry = (void *) e_iterator_get(iterator);
			ESelectNamesModel *source = e_select_names_get_source(dialog, entry->id);
			if (source) {
				ETextModel *model = e_select_names_text_model_new(source);
				if (model) {
					gtk_object_set(GTK_OBJECT(entry->entry),
						       "model", model,
						       NULL);
					gtk_object_unref(GTK_OBJECT(source));
				}
				gtk_object_unref(GTK_OBJECT(model));
			}
		}
		gtk_object_unref(GTK_OBJECT(iterator));
		break;
	}
	}
	gnome_dialog_close(GNOME_DIALOG(dialog));
}

void                          e_select_names_manager_activate_dialog           (ESelectNamesManager *manager,
										const char *id)
{
	ESelectNames *names = E_SELECT_NAMES(e_select_names_new());
	EIterator *iterator;
	iterator = e_list_get_iterator(manager->sections);
	for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const ESelectNamesManagerSection *section = e_iterator_get(iterator);
		ESelectNamesModel *newmodel = e_select_names_model_duplicate(section->model);
		e_select_names_add_section(names, section->id, section->title, newmodel);
		gtk_object_unref(GTK_OBJECT(newmodel));
	}
	gtk_signal_connect(GTK_OBJECT(names), "clicked",
			   GTK_SIGNAL_FUNC(e_select_names_clicked), manager);
	gtk_widget_show(GTK_WIDGET(names));
}

/* Of type ECard */
EList                    *e_select_names_manager_get_cards                 (ESelectNamesManager *manager,
									    const char *id)
{
	EIterator *iterator;
	iterator = e_list_get_iterator(manager->sections);
	for (e_iterator_reset(iterator); e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const ESelectNamesManagerSection *section = e_iterator_get(iterator);
		if (!strcmp(section->id, id)) {
			return e_select_names_model_get_cards(section->model);
		}
	}
	return NULL;
}
