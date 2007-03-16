/* Evolution Accessibility: ea-expander.c
 *
 * Copyright (C) 2006 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Boby Wang <boby.wang@sun.com> Sun Microsystem Inc., 2006
 *
 */

#include <config.h>
#include "ea-expander.h"
#include <gtk/gtkbutton.h>
#include <glib/gi18n.h>

static AtkObjectClass *parent_class;
static GType parent_type;

/* Action IDs */
enum {
	ACTIVATE,
	LAST_ACTION
};

static G_CONST_RETURN gchar*
ea_expander_get_name (AtkObject *a11y)
{
	return _("Toggle Attachment Bar");
}

/* Action interface */
static G_CONST_RETURN gchar *
ea_expander_action_get_name (AtkAction *action, gint i)
{
	switch (i)
	{
		case ACTIVATE:
			return _("activate");
		default:
			return NULL;
	}
}

static gboolean
ea_expander_do_action (AtkAction *action, gint i)
{
	GtkWidget *widget;
	EExpander *expander;

	widget = GTK_ACCESSIBLE (action)->widget;
	if (!widget || !GTK_WIDGET_IS_SENSITIVE (widget) || !GTK_WIDGET_VISIBLE (widget))
		return FALSE;

	expander = E_EXPANDER (widget);

	switch (i)
	{
		case ACTIVATE:
			g_signal_emit_by_name (expander, "activate");
			return TRUE;
		default:
			return FALSE;
	}
}

static gint
ea_expander_get_n_actions (AtkAction *action)
{
	return LAST_ACTION;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);

	iface->do_action = ea_expander_do_action;
	iface->get_n_actions = ea_expander_get_n_actions;
	iface->get_name = ea_expander_action_get_name;
}

static void
ea_expander_class_init (EaExpanderClass *klass)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (parent_type);

	atk_object_class->get_name = ea_expander_get_name;
}

static void
ea_expander_init (EaExpander *a11y)
{
	/* Empty */
}

GType
ea_expander_get_type (void)
{
	static GType type = 0;

	if (!type) {
		AtkObjectFactory *factory;
		GTypeQuery query;

		GTypeInfo info = {
			sizeof (EaExpanderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ea_expander_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class data */
			sizeof (EaExpander),
			0,
			(GInstanceInitFunc) ea_expander_init,
			NULL /* value_tree */
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		
		factory = atk_registry_get_factory (atk_get_default_registry (), GTK_TYPE_BIN);
		parent_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (parent_type, &query);

		info.class_size = query.class_size;
		info.instance_size = query.instance_size;

		type = g_type_register_static (parent_type, "EaExpander", &info, 0);
		g_type_add_interface_static (type, ATK_TYPE_ACTION,
						&atk_action_info);
	}

	return type;
}

AtkObject *
ea_expander_new (GtkWidget *widget)
{
	EaExpander *a11y;

	a11y = g_object_new (ea_expander_get_type (), NULL);

	GTK_ACCESSIBLE (a11y)->widget = GTK_WIDGET (widget);
	ATK_OBJECT (a11y)->role = ATK_ROLE_TOGGLE_BUTTON;

	return ATK_OBJECT (a11y);
}

