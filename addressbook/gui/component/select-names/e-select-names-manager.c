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
#include "e-select-names-entry.h"
#include "e-select-names-model.h"
#include "e-select-names-text-model.h"
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
	return newsec;
}

static void
section_free(void *sec, void *data)
{
	ESelectNamesManagerSection *section = sec;
	g_free(section->id);
	g_free(section->title);
	g_free(section);
}

/**
 * e_select_names_manager_init:
 */
static void
e_select_names_manager_init (ESelectNamesManager *manager)
{
	manager->sections = e_list_new(section_copy, section_free, manager);
}

void                          e_select_names_manager_add_section               (ESelectNamesManager *manager,
										char *id,
										char *title)
{
	ESelectNamesManagerSection *section;
	
	section = g_new(ESelectNamesManagerSection, 1);
	section->id = g_strdup(id);
	section->title = g_strdup(title);
	e_list_append(manager->sections, section);
	section_free(section, manager);
}

GtkWidget                    *e_select_names_manager_create_entry              (ESelectNamesManager *manager,
										char *id)
{
	GtkWidget *entry;
	ETextModel *model;
	EIterator *iterator;
	iterator = e_list_get_iterator(manager->sections);
	for (; e_iterator_is_valid(iterator); e_iterator_next(iterator)) {
		const ESelectNamesManagerSection *section = e_iterator_get(iterator);
		if (!strcmp(section->id, id)) {
			entry = GTK_WIDGET(e_entry_new());
			model = e_select_names_text_model_new(section->model);
			gtk_object_set(GTK_OBJECT(entry),
				       "model", model,
				       NULL);
			return entry;
		}
	}
	return NULL;
}

void                          e_select_names_manager_activate_dialog           (ESelectNamesManager *manager,
										char *id)
{
}

/* Of type ECard */
EList                    *e_select_names_manager_get_cards                 (ESelectNamesManager *manager,
									    char *id)
{
	return NULL;
}
