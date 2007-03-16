/*
 * Authors: Harry Lu  <harry.lu@sun.com>
 *
 * Copyright (C) 2004 Ximian, Inc.
 */

#include <config.h>
#include "ea-combo-button.h"
#include <gtk/gtkbutton.h>
#include <gtk/gtklabel.h>
#include <glib/gi18n.h>

static AtkObjectClass *parent_class;
static GType parent_type;

/*Action IDs */
enum {
	ACTIVATE_DEFAULT,
	POPUP_MENU,	
	LAST_ACTION
};

/* Static functions */
static G_CONST_RETURN gchar*
ea_combo_button_get_name (AtkObject *a11y)
{
	GtkWidget *widget;
	GtkWidget *label;
	EComboButton *button;

	widget = GTK_ACCESSIBLE (a11y)->widget;
	if (!widget)
		return NULL;

	button = E_COMBO_BUTTON (widget);
	label = e_combo_button_get_label (button);
	if (label)
		return gtk_label_get_text (GTK_LABEL (label));

	return _("Combo Button");
}

/* Action interface */
static G_CONST_RETURN gchar *
ea_combo_button_action_get_name (AtkAction *action, gint i)
{
	switch (i)
	{
		case ACTIVATE_DEFAULT:
			return _("Activate Default");
		case POPUP_MENU:
			return _("Popup Menu");
		default:
			return NULL;
	}
}

static gboolean
ea_combo_button_do_action (AtkAction *action,
                       gint      i)
{
	GtkWidget *widget;
	EComboButton *button;

	widget = GTK_ACCESSIBLE (action)->widget;
	if (!widget || !GTK_WIDGET_IS_SENSITIVE (widget) || !GTK_WIDGET_VISIBLE (widget))
		return FALSE;

	button = E_COMBO_BUTTON (widget);

	switch (i)
	{
		case ACTIVATE_DEFAULT:
			g_signal_emit_by_name (button, "activate_default");
			return TRUE;
		case POPUP_MENU:
			return e_combo_button_popup_menu (button);
		default:
			return FALSE;
	}
}

static gint
ea_combo_button_get_n_actions (AtkAction *action)
{
  return LAST_ACTION;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
	g_return_if_fail (iface != NULL);	

	iface->do_action = ea_combo_button_do_action;
	iface->get_n_actions = ea_combo_button_get_n_actions;
	iface->get_name = ea_combo_button_action_get_name;
}

static void
ea_combo_button_class_init (EaComboButtonClass *klass)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

	parent_class  = g_type_class_ref (parent_type);

	atk_object_class->get_name = ea_combo_button_get_name;
}

static void
ea_combo_button_init (EaComboButton *a11y)
{
	/* Empty for now */
}

GType
ea_combo_button_get_type (void)
{
	static GType type = 0;

	if (!type) {
		AtkObjectFactory *factory;
		GTypeQuery query;

		GTypeInfo info = {
			sizeof (EaComboButtonClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ea_combo_button_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (EaComboButton),
			0,
			(GInstanceInitFunc) ea_combo_button_init,
			NULL /* value_tree */
		};

		static const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc) atk_action_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		factory = atk_registry_get_factory (atk_get_default_registry (), GTK_TYPE_BUTTON);
		parent_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (parent_type, &query);

		info.class_size = query.class_size;
		info.instance_size = query.instance_size;

		type = g_type_register_static (parent_type, "EaComboButton", &info, 0);
		g_type_add_interface_static (type, ATK_TYPE_ACTION,
					     &atk_action_info);

	}

	return type;
}

AtkObject *
ea_combo_button_new (GtkWidget *widget)
{
	EaComboButton *a11y;

	a11y = g_object_new (ea_combo_button_get_type (), NULL);

	GTK_ACCESSIBLE (a11y)->widget = GTK_WIDGET (widget);
	ATK_OBJECT (a11y)->role = ATK_ROLE_PUSH_BUTTON;

	return ATK_OBJECT (a11y);
}
