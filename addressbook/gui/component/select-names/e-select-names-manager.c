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

/* Object argument IDs */
enum {
	ARG_0,
	ARG_CARD,
};


static void e_select_names_manager_init (ESelectNamesManager *manager);
static void e_select_names_manager_class_init (ESelectNamesManagerClass *klass);

static void e_select_names_manager_destroy (GtkObject *object);
static void e_select_names_manager_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_select_names_manager_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static void fill_in_info(ESelectNamesManager *manager);

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
	int i;
	
	manager = E_SELECT_NAMES_MANAGER (object);
}


/* Set_arg handler for the manager */
static void
e_select_names_manager_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	ESelectNamesManager *manager;
	
	manager = E_SELECT_NAMES_MANAGER (object);

	switch (arg_id) {
	case ARG_CARD:
		fill_in_info(manager);
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
		e_select_names_manager_sync_card(manager);
		if (manager->card)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(manager->card);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}


/**
 * e_select_names_manager_init:
 */
static void
e_select_names_manager_init (ESelectNamesManager *manager)
{
}

static void
fill_in_info(ESelectNamesManager *manager)
{
	ECard *card = manager->card;
	if (card) {

	}
}

void
e_select_names_manager_sync_card(ESelectNamesManager *manager)
{
	ECard *card = manager->card;
	if (card) {
		fill_in_info(manager);
	}
}
