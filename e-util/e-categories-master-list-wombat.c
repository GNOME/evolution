/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-categories-master-list.c: the master list of categories.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 1999, 2000 Ximian, Inc.
 */
#include <config.h>

#include "e-categories-master-list-wombat.h"
#include "e-categories-config.h"

#include <tree.h>
#include <parser.h>
#include <gal/util/e-i18n.h>
#include <gal/util/e-xml-utils.h>

#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-event-source.h>

#define PARENT_TYPE e_categories_master_list_array_get_type ()

#define d(x)

struct _ECategoriesMasterListWombatPriv {
	Bonobo_ConfigDatabase db;
	Bonobo_EventSource_ListenerId listener_id;
};

static ECategoriesMasterListArrayClass *parent_class;

static void
ecmlw_load (ECategoriesMasterListWombat *ecmlw)
{
	char *string;
	gboolean def;

	string = bonobo_config_get_string_with_default
		(ecmlw->priv->db,
		 "General/CategoryMasterList",
		 NULL,
		 &def);

	/* parse the XML string */
	if (!def) {
		e_categories_master_list_array_from_string (E_CATEGORIES_MASTER_LIST_ARRAY (ecmlw),
							    string);
	}

	d(g_print ("load: %s\n", string?string:"(nil)"));

	g_free (string);
}

static void
ecmlw_save (ECategoriesMasterListWombat *ecmlw)
{
	char *string;
	CORBA_Environment ev;

	string = e_categories_master_list_array_to_string (E_CATEGORIES_MASTER_LIST_ARRAY (ecmlw));

	d(g_print ("save: %s\n", string));

	CORBA_exception_init (&ev);

	bonobo_config_set_string (ecmlw->priv->db,
				  "General/CategoryMasterList",
				  string,
				  &ev);

	CORBA_exception_free (&ev);

	g_free (string);
}

/**
 * ecmlw_commit:
 * @ecml: the master list to remove from.
 */
static void
ecmlw_commit (ECategoriesMasterList *ecml)
{
	ECategoriesMasterListWombat *ecmlw = E_CATEGORIES_MASTER_LIST_WOMBAT (ecml);

	((ECategoriesMasterListClass *) parent_class)->commit (ecml);

	ecmlw_save (ecmlw);
}


/**
 * ecmlw_reset:
 * @ecml: the master list to reset.
 */
static void
ecmlw_reset (ECategoriesMasterList *ecml)
{
	ECategoriesMasterListWombat *ecmlw = E_CATEGORIES_MASTER_LIST_WOMBAT (ecml);

	((ECategoriesMasterListClass *) parent_class)->reset (ecml);

	ecmlw_save (ecmlw);
}

static void
ecmlw_destroy (GtkObject *object)
{
	ECategoriesMasterListWombat *ecmlw = E_CATEGORIES_MASTER_LIST_WOMBAT (object);
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	/* remove the listener */

	bonobo_event_source_client_remove_listener (ecmlw->priv->db,
						    ecmlw->priv->listener_id,
						    &ev);
	bonobo_object_release_unref (ecmlw->priv->db, &ev);

	CORBA_exception_free (&ev);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
ecmlw_class_init (GtkObjectClass *object_class)
{
	ECategoriesMasterListClass *ecml_class = E_CATEGORIES_MASTER_LIST_CLASS(object_class);

	parent_class          = gtk_type_class (PARENT_TYPE);

	ecml_class->commit    = ecmlw_commit;

	ecml_class->reset     = ecmlw_reset ;

	object_class->destroy = ecmlw_destroy;
}

static void 
property_change_cb (BonoboListener    *listener,
		    char              *event_name, 
		    CORBA_any         *any,
		    CORBA_Environment *ev,
		    gpointer           user_data)
{
	ecmlw_load (user_data);
}

static void
ecmlw_init (ECategoriesMasterListWombat *ecmlw)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	ecmlw->priv = g_new (ECategoriesMasterListWombatPriv, 1);
	ecmlw->priv->db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);

	/* add a listener */
	ecmlw->priv->listener_id =
		bonobo_event_source_client_add_listener (ecmlw->priv->db, property_change_cb,
							 NULL, &ev, ecmlw);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	ecmlw_load (ecmlw);
}

guint
e_categories_master_list_wombat_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo info = {
			"ECategoriesMasterListWombat",
			sizeof (ECategoriesMasterListWombat),
			sizeof (ECategoriesMasterListWombatClass),
			(GtkClassInitFunc) ecmlw_class_init,
			(GtkObjectInitFunc) ecmlw_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

ECategoriesMasterList *
e_categories_master_list_wombat_new       (void)
{
	return E_CATEGORIES_MASTER_LIST (gtk_type_new (e_categories_master_list_wombat_get_type ()));
}
