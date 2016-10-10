/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Yuedong Du <yuedong.du@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-table-click-to-add.h"

#include <atk/atk.h>
#include <glib/gi18n.h>

#include "e-table-click-to-add.h"
#include "e-table-group-leaf.h"
#include "e-table-group.h"
#include "gal-a11y-e-table-click-to-add-factory.h"
#include "gal-a11y-util.h"

static AtkObjectClass *parent_class;
static GType parent_type;
static gint priv_offset;
#define GET_PRIVATE(object) \
	((GalA11yETableClickToAddPrivate *) \
	(((gchar *) object) + priv_offset))
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

static const gchar *
etcta_get_description (AtkAction *action,
                       gint i)
{
	if (i == 0)
		return _("click to add");

	return NULL;
}

static const gchar *
etcta_action_get_name (AtkAction *action,
                       gint i)
{
	if (i == 0)
		return _("click");

	return NULL;
}

static gboolean
idle_do_action (gpointer data)
{
	GtkLayout *layout;
	GdkEventButton event;
	ETableClickToAdd * etcta;
	gint finished;

	g_return_val_if_fail (data!= NULL, FALSE);

	etcta = E_TABLE_CLICK_TO_ADD (
		atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (data)));
	g_return_val_if_fail (etcta, FALSE);

	layout = GTK_LAYOUT (GNOME_CANVAS_ITEM (etcta)->canvas);

	event.x = 0;
	event.y = 0;
	event.type = GDK_BUTTON_PRESS;
	event.window = gtk_layout_get_bin_window (layout);
	event.button = 1;
	event.send_event = TRUE;
	event.time = GDK_CURRENT_TIME;
	event.axes = NULL;

	g_signal_emit_by_name (etcta, "event", &event, &finished);

	return FALSE;
}

static gboolean
etcta_do_action (AtkAction *action,
                 gint i)
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

static const gchar *
etcta_get_name (AtkObject *obj)
{
	ETableClickToAdd * etcta;

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD (obj), NULL);

	etcta = E_TABLE_CLICK_TO_ADD (
		atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (obj)));
	if (etcta && etcta->message != NULL)
		return etcta->message;

	return _("click to add");
}

static gint
etcta_get_n_children (AtkObject *accessible)
{
	ETableClickToAdd * etcta;

	etcta = E_TABLE_CLICK_TO_ADD (
		atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (accessible)));

	return (etcta->rect || etcta->row) ? 1 : 0;
}

static AtkObject *
etcta_ref_child (AtkObject *accessible,
                 gint i)
{
	AtkObject * atk_obj = NULL;
	ETableClickToAdd * etcta;

	if (i != 0)
		return NULL;

	etcta = E_TABLE_CLICK_TO_ADD (
		atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (accessible)));

	g_return_val_if_fail (etcta, NULL);

	if (etcta->rect) {
		atk_obj = atk_gobject_accessible_for_object (
			G_OBJECT (etcta->rect));
	} else if (etcta->row) {
		atk_obj = atk_gobject_accessible_for_object (
			G_OBJECT (etcta->row));
	}

	g_object_ref (atk_obj);

	return atk_obj;
}

static AtkStateSet *
etcta_ref_state_set (AtkObject *accessible)
{
	AtkStateSet * state_set = NULL;

	state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (accessible);
	if (state_set != NULL) {
		atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
		atk_state_set_add_state (state_set, ATK_STATE_SHOWING);
	}

	return state_set;
}

static void
etcta_class_init (GalA11yETableClickToAddClass *class)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	atk_object_class->get_name = etcta_get_name;
	atk_object_class->get_n_children = etcta_get_n_children;
	atk_object_class->ref_child = etcta_ref_child;
	atk_object_class->ref_state_set = etcta_ref_state_set;
}

static void
etcta_init (GalA11yETableClickToAdd *a11y)
{
}

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

		factory = atk_registry_get_factory (
			atk_get_default_registry (),
			GNOME_TYPE_CANVAS_ITEM);

		parent_type = atk_object_factory_get_accessible_type (factory);
		type = gal_a11y_type_register_static_with_private (
			PARENT_TYPE, "GalA11yETableClickToAdd", &info, 0,
			sizeof (GalA11yETableClickToAddPrivate), &priv_offset);

		g_type_add_interface_static (type, ATK_TYPE_ACTION, &atk_action_info);

	}

	return type;
}

static gboolean
etcta_event (GnomeCanvasItem *item,
             GdkEvent *e,
             gpointer data)
{
	ETableClickToAdd *etcta = E_TABLE_CLICK_TO_ADD (item);
	GalA11yETableClickToAdd *a11y;
	GalA11yETableClickToAddPrivate *priv;

	g_return_val_if_fail (item, TRUE);

	g_return_val_if_fail (GAL_A11Y_IS_E_TABLE_CLICK_TO_ADD (data), FALSE);
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

	return FALSE;
}

static void
etcta_selection_cursor_changed (ESelectionModel *esm,
                                gint row,
                                gint col,
                                GalA11yETableClickToAdd *a11y)
{
	ETableClickToAdd *etcta;
	AtkObject *row_a11y;

	etcta = E_TABLE_CLICK_TO_ADD (
		atk_gobject_accessible_get_object (
		ATK_GOBJECT_ACCESSIBLE (a11y)));

	if (etcta == NULL || etcta->row == NULL)
		return;

	row_a11y = atk_gobject_accessible_for_object (G_OBJECT (etcta->row));
	if (row_a11y) {
		AtkObject *cell_a11y;

		cell_a11y = g_object_get_data (
			G_OBJECT (row_a11y), "gail-focus-object");
		if (cell_a11y) {
			atk_object_notify_state_change (cell_a11y, ATK_STATE_FOCUSED, TRUE);
		}
	}
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

	etcta = E_TABLE_CLICK_TO_ADD (widget);

	atk_object_initialize (ATK_OBJECT (a11y), etcta);

	priv->rect = etcta->rect;
	priv->row = etcta->row;

	g_signal_connect_after (
		widget, "event",
		G_CALLBACK (etcta_event), a11y);

	g_signal_connect (
		etcta->selection, "cursor_changed",
		G_CALLBACK (etcta_selection_cursor_changed), a11y);

	return ATK_OBJECT (a11y);
}

void
gal_a11y_e_table_click_to_add_init (void)
{
	if (atk_get_root ())
		atk_registry_set_factory_type (
			atk_get_default_registry (),
			E_TYPE_TABLE_CLICK_TO_ADD,
			gal_a11y_e_table_click_to_add_factory_get_type ());
}

