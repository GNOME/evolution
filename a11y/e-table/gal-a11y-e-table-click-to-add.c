/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 2002 Ximian, Inc.
 */

#include <config.h>
#include "gal-a11y-util.h"
#include "gal-a11y-e-table-click-to-add.h"
#include <gal/e-table/e-table-group.h>
#include <gal/e-table/e-table-group-leaf.h>
#include <gal/e-table/e-table-click-to-add.h>
#include <atk/atkcomponent.h>
#include <atk/atkaction.h>

static AtkObjectClass *parent_class;
static GType parent_type;
static gint priv_offset;
#define GET_PRIVATE(object) ((GalA11yETableClickToAddPrivate *) (((char *) object) + priv_offset))
#define PARENT_TYPE (parent_type)

struct _GalA11yETableClickToAddPrivate {
	gpointer rect;
	gpointer row;
};


static gint
etcta_get_n_actions (AtkAction *action)
{
	return 1;
}

static G_CONST_RETURN gchar*
etcta_get_description (AtkAction *action,
                             gint      i)
{
	if (i == 0)
		return "click to add";

	return NULL;
}

static G_CONST_RETURN gchar*
etcta_action_get_name (AtkAction *action, gint      i)
{
	if (i == 0)
		return "click";

	return NULL;
}


static gboolean
idle_do_action (gpointer data)
{
        GdkEventButton event;
	ETableClickToAdd * etcta;
        gint finished;

	g_return_val_if_fail ( data!= NULL, FALSE);

	etcta = E_TABLE_CLICK_TO_ADD (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (data)));
	g_return_val_if_fail (etcta, FALSE);

	event.x = 0;
	event.y = 0;
                                                                                
        event.type = GDK_BUTTON_PRESS;
        event.window = GTK_LAYOUT(GNOME_CANVAS_ITEM(etcta)->canvas)->bin_window;
        event.button = 1;
        event.send_event = TRUE;
        event.time = GDK_CURRENT_TIME;
        event.axes = NULL;
                                                                                
        g_signal_emit_by_name (etcta, "event", &event, &finished);

	return FALSE;
}

static gboolean
etcta_do_action (AtkAction * action, gint i)
{
	g_return_val_if_fail (i == 0, FALSE);

	g_idle_add (idle_do_action, action);

	return TRUE;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);
                                                                                
	iface->do_action = etcta_do_action;
	iface->get_n_actions = etcta_get_n_actions;
	iface->get_description = etcta_get_description;
	iface->get_name = etcta_action_get_name;
}


static G_CONST_RETURN gchar *
etcta_get_name (AtkObject *obj)
{
	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD (obj), NULL);

	return "click to add";
}

static gint
etcta_get_n_children (AtkObject *accessible)
{
	return 1;
}

static AtkObject*
etcta_ref_child (AtkObject *accessible,
		 gint i)
{
	AtkObject * atk_obj = NULL;
	ETableClickToAdd * etcta;

	if ( i != 0 )
		return NULL;

	etcta  = E_TABLE_CLICK_TO_ADD(atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible)));

	g_return_val_if_fail (etcta, NULL);

	if (etcta->rect) {
		atk_obj = atk_gobject_accessible_for_object (G_OBJECT(etcta->rect));
	} else if (etcta->row) {
		atk_obj = atk_gobject_accessible_for_object (G_OBJECT(etcta->row));
	}

	g_object_ref (atk_obj);

	return atk_obj;
}

static void
etcta_class_init (GalA11yETableClickToAddClass *klass)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	atk_object_class->get_name = etcta_get_name;
        atk_object_class->get_n_children = etcta_get_n_children;
        atk_object_class->ref_child = etcta_ref_child;
}

static void
etcta_init (GalA11yETableClickToAdd *a11y)
{
}

/**
 * gal_a11y_e_table_click_to_add_get_type:
 * @void: 
 * 
 * Registers the &GalA11yETableClickToAdd class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the &GalA11yETableClickToAdd class.
 **/
GType
gal_a11y_e_table_click_to_add_get_type (void)
{
	static GType type = 0;

	if (!type) {
		AtkObjectFactory *factory;

		GTypeInfo info = {
			sizeof (GalA11yETableClickToAddClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) etcta_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yETableClickToAdd),
			0,
			(GInstanceInitFunc) etcta_init,
			NULL /* value_table */
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

                factory = atk_registry_get_factory (atk_get_default_registry (), GNOME_TYPE_CANVAS_ITEM);

		parent_type = atk_object_factory_get_accessible_type (factory);
                type = gal_a11y_type_register_static_with_private (PARENT_TYPE,
				"GalA11yETableClickToAdd", &info, 0, 
				sizeof(GalA11yETableClickToAddPrivate), &priv_offset);
                                                                                
                g_type_add_interface_static (type, ATK_TYPE_ACTION, &atk_action_info);

	}

	return type;
}

static gboolean
etcta_event (GnomeCanvasItem *item, GdkEvent *e, gpointer data)
{
        ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);
	GalA11yETableClickToAdd *a11y;
	GalA11yETableClickToAddPrivate *priv;
	
	g_return_val_if_fail (item, TRUE);

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD(data), FALSE);
	a11y = GAL_A11Y_E_TABLE_CLICK_TO_ADD (data);

	priv = GET_PRIVATE (a11y);

	/* rect replaced by row. */
	if (etcta->rect == NULL && priv->rect != NULL) {
		g_signal_emit_by_name (a11y, "children_changed::remove", 0, NULL, NULL);

	}
	/* row inserted, and/or replaced by a new row. */
	if (etcta->row != NULL && priv->row == NULL) {
		g_signal_emit_by_name (a11y, "children_changed::add", 0, NULL, NULL);
	} else if (etcta->row != NULL && priv->row != NULL && etcta->row != priv->row) {
		g_signal_emit_by_name (a11y, "children_changed::remove", 0, NULL, NULL);
		g_signal_emit_by_name (a11y, "children_changed::add", 0, NULL, NULL);
	}


	priv->rect = etcta->rect;
	priv->row = etcta->row;

	return TRUE;
}

AtkObject *
gal_a11y_e_table_click_to_add_new (GObject *widget)
{
	GalA11yETableClickToAdd *a11y;
	ETableClickToAdd * etcta;
	GalA11yETableClickToAddPrivate *priv;

	g_return_val_if_fail (widget != NULL, NULL);

	a11y = g_object_new (gal_a11y_e_table_click_to_add_get_type (), NULL);
	priv = GET_PRIVATE (a11y);

	etcta = E_TABLE_CLICK_TO_ADD(widget);


	atk_object_initialize (ATK_OBJECT (a11y), etcta);

	priv->rect = etcta->rect;
	priv->row = etcta->row;


	g_signal_connect_after (G_OBJECT(widget), "event",
	    			G_CALLBACK (etcta_event), a11y);

	return ATK_OBJECT (a11y);
}
