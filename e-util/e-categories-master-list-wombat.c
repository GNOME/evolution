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
#include <bonobo-conf/bonobo-config-database.h>
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
		xmlDocPtr doc;
		xmlNodePtr node;
		xmlNodePtr children;
		char *string_copy;

		string_copy = g_strdup (string);
		doc = xmlParseMemory (string_copy, strlen (string_copy));
		node = xmlDocGetRootElement (doc);
		g_free (string_copy);

		/* add categories and their associated icons/colors */
		for (children = node->xmlChildrenNode;
		     children != NULL;
		     children = children->next) {
			char *category;
			char *icon;
			char *color;

			category = e_xml_get_string_prop_by_name (children, "a");
			icon = (char *) e_categories_config_get_icon_file_for (category);
			color = (char *) e_categories_config_get_color_for (category);

			e_categories_master_list_append (
				E_CATEGORIES_MASTER_LIST (ecmlw),
				category,
				color,
				icon);
		}

		xmlFreeDoc (doc);
	}

	g_print ("load: %s\n", string);

	g_free (string);
}

static void
ecmlw_save (ECategoriesMasterListWombat *ecmlw)
{
	char *string;
	int i;
	int count;
	CORBA_Environment ev;

	string = e_categories_master_list_array_to_string (E_CATEGORIES_MASTER_LIST_ARRAY (ecmlw));

	g_print ("save: %s\n", string);

	CORBA_exception_init (&ev);

	bonobo_config_set_string (ecmlw->priv->db,
				  "General/CategoryMasterList",
				  string,
				  &ev);

	/* now save all icons and colors for each category */
	count = e_categories_master_list_count (E_CATEGORIES_MASTER_LIST (ecmlw));
	for (i = 0; i < count; i++) {
		gchar *category;
		gchar *icon;
		gchar *color;

		category = e_categories_master_list_nth (E_CATEGORIES_MASTER_LIST (ecmlw), i);
		icon = e_categories_master_list_nth_icon (E_CATEGORIES_MASTER_LIST (ecmlw), i);
		color = e_categories_master_list_nth_color (E_CATEGORIES_MASTER_LIST (ecmlw), i);

		e_categories_config_set_icon_for (category, icon);
		e_categories_config_set_color_for (category, color);
	}

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
