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
#include "e-config-listener.h"

#include <libxml/tree.h>
#include <libxml/parser.h>

#define PARENT_TYPE e_categories_master_list_array_get_type ()

#define d(x)

struct _ECategoriesMasterListWombatPriv {
	EConfigListener *listener;
	guint listener_id;
};

static ECategoriesMasterListArrayClass *parent_class;

static void
ecmlw_load (ECategoriesMasterListWombat *ecmlw)
{
	char *string;
	gboolean def;

	string = e_config_listener_get_string_with_default (ecmlw->priv->listener,
							    "/apps/evolution/general/category_master_list",
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

	string = e_categories_master_list_array_to_string (E_CATEGORIES_MASTER_LIST_ARRAY (ecmlw));

	d(g_print ("save: %s\n", string));

	e_config_listener_set_string (ecmlw->priv->listener,
				      "/apps/evolution/general/category_master_list",
				      string);

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
ecmlw_dispose (GObject *object)
{
	ECategoriesMasterListWombat *ecmlw = E_CATEGORIES_MASTER_LIST_WOMBAT (object);

	if (ecmlw->priv) {
		/* remove the listener */
		g_signal_handler_disconnect (ecmlw->priv->listener,
					     ecmlw->priv->listener_id);				     

		g_object_unref (ecmlw->priv->listener);

		g_free (ecmlw->priv);
		ecmlw->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}


static void
ecmlw_class_init (GObjectClass *object_class)
{
	ECategoriesMasterListClass *ecml_class = E_CATEGORIES_MASTER_LIST_CLASS(object_class);

	parent_class          = g_type_class_ref (PARENT_TYPE);

	ecml_class->commit    = ecmlw_commit;

	ecml_class->reset     = ecmlw_reset ;

	object_class->dispose = ecmlw_dispose;
}

static void 
property_change_cb (EConfigListener   *listener,
		    char              *key,
		    gpointer           user_data)
{
	ecmlw_load (user_data);
}

static void
ecmlw_init (ECategoriesMasterListWombat *ecmlw)
{
	ecmlw->priv = g_new (ECategoriesMasterListWombatPriv, 1);
	ecmlw->priv->listener = e_config_listener_new ();

	/* add a listener */
	ecmlw->priv->listener_id =
		g_signal_connect (ecmlw->priv->listener,
				  "key_changed",
				  G_CALLBACK (property_change_cb),
				  ecmlw);

	ecmlw_load (ecmlw);
}

GType
e_categories_master_list_wombat_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (ECategoriesMasterListWombatClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) ecmlw_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECategoriesMasterListWombat),
			0,             /* n_preallocs */
			(GInstanceInitFunc) ecmlw_init,
		};

		type = g_type_register_static (PARENT_TYPE, "ECategoriesMasterListWombat", &info, 0);
	}

	return type;
}

ECategoriesMasterList *
e_categories_master_list_wombat_new       (void)
{
	return E_CATEGORIES_MASTER_LIST (g_object_new (E_TYPE_CATEGORIES_MASTER_LIST_WOMBAT, NULL));
}
