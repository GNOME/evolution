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
 *		Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-a11y-e-cell.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-table.h"
#include "e-tree.h"
#include "gal-a11y-e-cell-vbox.h"
#include "gal-a11y-e-table-item.h"
#include "gal-a11y-util.h"

static GObjectClass *parent_class;
#define PARENT_TYPE (atk_object_get_type ())

static void _gal_a11y_e_cell_destroy_action_info (gpointer action_info, gpointer user_data);

#if 0
static void
unref_item (gpointer user_data,
            GObject *obj_loc)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (user_data);
	a11y->item = NULL;
	g_object_unref (a11y);
}

static void
unref_cell (gpointer user_data,
            GObject *obj_loc)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (user_data);
	a11y->cell_view = NULL;
	g_object_unref (a11y);
}
#endif

static gboolean
is_valid (AtkObject *cell)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (cell);
	GalA11yETableItem *a11yItem = GAL_A11Y_E_TABLE_ITEM (a11y->parent);
	AtkStateSet *item_ss;
	gboolean ret = TRUE;

	item_ss = atk_object_ref_state_set (ATK_OBJECT (a11yItem));
	if (atk_state_set_contains_state (item_ss, ATK_STATE_DEFUNCT))
		ret = FALSE;

	g_object_unref (item_ss);

	if (ret && atk_state_set_contains_state (a11y->state_set, ATK_STATE_DEFUNCT))
		ret = FALSE;

	return ret;
}

static void
gal_a11y_e_cell_dispose (GObject *object)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (object);

#if 0
	if (a11y->item)
		g_object_unref (a11y->item);  /*, unref_item, a11y); */
	if (a11y->cell_view)
		g_object_unref (a11y->cell_view); /*, unref_cell, a11y); */
	if (a11y->parent)
		g_object_unref (a11y->parent);
#endif

	g_clear_object (&a11y->state_set);

	if (a11y->action_list) {
		g_list_foreach (a11y->action_list, _gal_a11y_e_cell_destroy_action_info, NULL);
		g_list_free (a11y->action_list);
		a11y->action_list = NULL;
	}

	if (parent_class->dispose)
		parent_class->dispose (object);

}

/* Static functions */
static const gchar *
gal_a11y_e_cell_get_name (AtkObject *a11y)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (a11y);
	ETableCol *ecol;

	if (a11y->name != NULL && strcmp (a11y->name, ""))
		return a11y->name;

	if (cell->item != NULL) {
		ecol = e_table_header_get_column (cell->item->header, cell->view_col);
		if (ecol != NULL)
			return ecol->text;
	}

	return _("Table Cell");
}

static AtkStateSet *
gal_a11y_e_cell_ref_state_set (AtkObject *accessible)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (accessible);

	g_return_val_if_fail (cell->state_set, NULL);

	g_object_ref (cell->state_set);

	return cell->state_set;
}

static AtkObject *
gal_a11y_e_cell_get_parent (AtkObject *accessible)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (accessible);
	return a11y->parent;
}

static gint
gal_a11y_e_cell_get_index_in_parent (AtkObject *accessible)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (accessible);

	if (!is_valid (accessible))
		return -1;

	return (a11y->row + 1) * a11y->item->cols + a11y->view_col;
}

/* Component IFace */
static void
gal_a11y_e_cell_get_extents (AtkComponent *component,
                             gint *x,
                             gint *y,
                             gint *width,
                             gint *height,
                             AtkCoordType coord_type)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (component);
	GtkWidget *tableOrTree;
	gint row;
	gint col;
	gint xval;
	gint yval;

	row = a11y->row;
	col = a11y->view_col;

	tableOrTree = gtk_widget_get_parent (GTK_WIDGET (a11y->item->parent.canvas));
	if (E_IS_TREE (tableOrTree)) {
		e_tree_get_cell_geometry (
			E_TREE (tableOrTree),
			row, col, &xval, &yval,
			width, height);
	} else {
		e_table_get_cell_geometry (
			E_TABLE (tableOrTree),
			row, col, &xval, &yval,
			width, height);
	}

	atk_component_get_extents (
		ATK_COMPONENT (a11y->parent),
		x, y, NULL, NULL, coord_type);
	if (x && *x != G_MININT)
		*x += xval;
	if (y && *y != G_MININT)
		*y += yval;
}

static gboolean
gal_a11y_e_cell_grab_focus (AtkComponent *component)
{
	GalA11yECell *a11y;
	gint index;
	GtkWidget *toplevel;
	GalA11yETableItem *a11yTableItem;

	a11y = GAL_A11Y_E_CELL (component);

	/* for e_cell_vbox's children, we just grab the e_cell_vbox */
	if (GAL_A11Y_IS_E_CELL_VBOX (a11y->parent)) {
		return atk_component_grab_focus (ATK_COMPONENT (a11y->parent));
	}

	a11yTableItem = GAL_A11Y_E_TABLE_ITEM (a11y->parent);
	index = atk_object_get_index_in_parent (ATK_OBJECT (a11y));

	atk_selection_clear_selection (ATK_SELECTION (a11yTableItem));
	atk_selection_add_selection (ATK_SELECTION (a11yTableItem), index);

	gtk_widget_grab_focus (
		GTK_WIDGET (GNOME_CANVAS_ITEM (a11y->item)->canvas));
	toplevel = gtk_widget_get_toplevel (
		GTK_WIDGET (GNOME_CANVAS_ITEM (a11y->item)->canvas));
	if (toplevel && gtk_widget_is_toplevel (toplevel))
		gtk_window_present (GTK_WINDOW (toplevel));

	return TRUE;
}

/* Table IFace */

static void
gal_a11y_e_cell_atk_component_iface_init (AtkComponentIface *iface)
{
	iface->get_extents = gal_a11y_e_cell_get_extents;
	iface->grab_focus = gal_a11y_e_cell_grab_focus;
}

static void
gal_a11y_e_cell_class_init (GalA11yECellClass *class)
{
	AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = gal_a11y_e_cell_dispose;

	atk_object_class->get_parent = gal_a11y_e_cell_get_parent;
	atk_object_class->get_index_in_parent = gal_a11y_e_cell_get_index_in_parent;
	atk_object_class->ref_state_set = gal_a11y_e_cell_ref_state_set;
	atk_object_class->get_name = gal_a11y_e_cell_get_name;
}

static void
gal_a11y_e_cell_init (GalA11yECell *a11y)
{
	a11y->item = NULL;
	a11y->cell_view = NULL;
	a11y->parent = NULL;
	a11y->model_col = -1;
	a11y->view_col = -1;
	a11y->row = -1;

	a11y->state_set = atk_state_set_new ();
	atk_state_set_add_state (a11y->state_set, ATK_STATE_TRANSIENT);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_ENABLED);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SENSITIVE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SELECTABLE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_SHOWING);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_FOCUSABLE);
	atk_state_set_add_state (a11y->state_set, ATK_STATE_VISIBLE);
}

static ActionInfo *
_gal_a11y_e_cell_get_action_info (GalA11yECell *cell,
                                  gint index)
{
	GList *list_node;

	g_return_val_if_fail (GAL_A11Y_IS_E_CELL (cell), NULL);
	if (cell->action_list == NULL)
		return NULL;
	list_node = g_list_nth (cell->action_list, index);
	if (!list_node)
		return NULL;
	return (ActionInfo *) (list_node->data);
}

static void
_gal_a11y_e_cell_destroy_action_info (gpointer action_info,
                                      gpointer user_data)
{
	ActionInfo *info = (ActionInfo *) action_info;

	g_return_if_fail (info != NULL);
	g_free (info->name);
	g_free (info->description);
	g_free (info->keybinding);
	g_free (info);
}

gboolean
gal_a11y_e_cell_add_action (GalA11yECell *cell,
                            const gchar *action_name,
                            const gchar *action_description,
                            const gchar *action_keybinding,
                            ACTION_FUNC action_func)
{
	ActionInfo *info;
	g_return_val_if_fail (GAL_A11Y_IS_E_CELL (cell), FALSE);
	info = g_new (ActionInfo, 1);

	if (action_name != NULL)
		info->name = g_strdup (action_name);
	else
		info->name = NULL;

	if (action_description != NULL)
		info->description = g_strdup (action_description);
	else
		info->description = NULL;
	if (action_keybinding != NULL)
		info->keybinding = g_strdup (action_keybinding);
	else
		info->keybinding = NULL;
	info->do_action_func = action_func;

	cell->action_list = g_list_append (cell->action_list, (gpointer) info);
	return TRUE;
}

static gint
gal_a11y_e_cell_action_get_n_actions (AtkAction *action)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (action);
	if (cell->action_list != NULL)
		return g_list_length (cell->action_list);
	else
		return 0;
}

static const gchar *
gal_a11y_e_cell_action_get_name (AtkAction *action,
                                 gint index)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (action);
	ActionInfo *info = _gal_a11y_e_cell_get_action_info (cell, index);

	if (info == NULL)
		return NULL;
	return info->name;
}

static const gchar *
gal_a11y_e_cell_action_get_description (AtkAction *action,
                                        gint index)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (action);
	ActionInfo *info = _gal_a11y_e_cell_get_action_info (cell, index);

	if (info == NULL)
		return NULL;
	return info->description;
}

static gboolean
gal_a11y_e_cell_action_set_description (AtkAction *action,
                                        gint index,
                                        const gchar *desc)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (action);
	ActionInfo *info = _gal_a11y_e_cell_get_action_info (cell, index);

	if (info == NULL)
		return FALSE;
	g_free (info->description);
	info->description = g_strdup (desc);
	return TRUE;
}

static const gchar *
gal_a11y_e_cell_action_get_keybinding (AtkAction *action,
                                       gint index)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (action);
	ActionInfo *info = _gal_a11y_e_cell_get_action_info (cell, index);
	if (info == NULL)
		return NULL;

	return info->keybinding;
}

static gboolean
idle_do_action (gpointer data)
{
	GalA11yECell *cell;

	cell = GAL_A11Y_E_CELL (data);

	if (!is_valid (ATK_OBJECT (cell)))
		return FALSE;

	cell->action_idle_handler = 0;
	cell->action_func (cell);
	g_object_unref (cell);

	return FALSE;
}

static gboolean
gal_a11y_e_cell_action_do_action (AtkAction *action,
                                  gint index)
{
	GalA11yECell *cell = GAL_A11Y_E_CELL (action);
	ActionInfo *info = _gal_a11y_e_cell_get_action_info (cell, index);

	if (!is_valid (ATK_OBJECT (action)))
		return FALSE;

	if (info == NULL)
		return FALSE;
	g_return_val_if_fail (info->do_action_func, FALSE);
	if (cell->action_idle_handler)
		return FALSE;
	cell->action_func = info->do_action_func;
	g_object_ref (cell);
	cell->action_idle_handler = g_idle_add (idle_do_action, cell);

	return TRUE;
}

void
gal_a11y_e_cell_atk_action_interface_init (AtkActionIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_n_actions = gal_a11y_e_cell_action_get_n_actions;
  iface->do_action = gal_a11y_e_cell_action_do_action;
  iface->get_name = gal_a11y_e_cell_action_get_name;
  iface->get_description = gal_a11y_e_cell_action_get_description;
  iface->set_description = gal_a11y_e_cell_action_set_description;
  iface->get_keybinding = gal_a11y_e_cell_action_get_keybinding;
}

gboolean
gal_a11y_e_cell_add_state (GalA11yECell *cell,
                           AtkStateType state_type,
                           gboolean emit_signal)
{
	if (!atk_state_set_contains_state (cell->state_set, state_type)) {
		gboolean rc;

		rc = atk_state_set_add_state (cell->state_set, state_type);
		/*
		 * The signal should only be generated if the value changed,
		 * not when the cell is set up.  So states that are set
		 * initially should pass FALSE as the emit_signal argument.
		 */

		if (emit_signal) {
			atk_object_notify_state_change (ATK_OBJECT (cell), state_type, TRUE);
			/* If state_type is ATK_STATE_VISIBLE, additional
			 * notification */
			if (state_type == ATK_STATE_VISIBLE)
				g_signal_emit_by_name (cell, "visible_data_changed");
		}

		return rc;
	}
	else
		return FALSE;
}

gboolean
gal_a11y_e_cell_remove_state (GalA11yECell *cell,
                              AtkStateType state_type,
                              gboolean emit_signal)
{
	if (atk_state_set_contains_state (cell->state_set, state_type)) {
		gboolean rc;

		rc = atk_state_set_remove_state (cell->state_set, state_type);
		/*
		 * The signal should only be generated if the value changed,
		 * not when the cell is set up.  So states that are set
		 * initially should pass FALSE as the emit_signal argument.
		 */

		if (emit_signal) {
			atk_object_notify_state_change (ATK_OBJECT (cell), state_type, FALSE);
			/* If state_type is ATK_STATE_VISIBLE, additional notification */
			if (state_type == ATK_STATE_VISIBLE)
				g_signal_emit_by_name (cell, "visible_data_changed");
		}

		return rc;
	}
	else
		return FALSE;
}

/**
 * gal_a11y_e_cell_get_type:
 * @void:
 *
 * Registers the &GalA11yECell class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the &GalA11yECell class.
 **/
GType
gal_a11y_e_cell_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GalA11yECellClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gal_a11y_e_cell_class_init,
			(GClassFinalizeFunc) NULL,
			NULL, /* class_data */
			sizeof (GalA11yECell),
			0,
			(GInstanceInitFunc) gal_a11y_e_cell_init,
			NULL /* value_cell */
		};

		static const GInterfaceInfo atk_component_info = {
			(GInterfaceInitFunc) gal_a11y_e_cell_atk_component_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (PARENT_TYPE, "GalA11yECell", &info, 0);
		g_type_add_interface_static (type, ATK_TYPE_COMPONENT, &atk_component_info);
	}

	return type;
}

AtkObject *
gal_a11y_e_cell_new (ETableItem *item,
                     ECellView *cell_view,
                     AtkObject *parent,
                     gint model_col,
                     gint view_col,
                     gint row)
{
	AtkObject *a11y;

	a11y = g_object_new (gal_a11y_e_cell_get_type (), NULL);

	gal_a11y_e_cell_construct (
		a11y,
		item,
		cell_view,
		parent,
		model_col,
		view_col,
		row);
	return a11y;
}

void
gal_a11y_e_cell_construct (AtkObject *object,
                           ETableItem *item,
                           ECellView *cell_view,
                           AtkObject *parent,
                           gint model_col,
                           gint view_col,
                           gint row)
{
	GalA11yECell *a11y = GAL_A11Y_E_CELL (object);
	a11y->item = item;
	a11y->cell_view = cell_view;
	a11y->parent = parent;
	a11y->model_col = model_col;
	a11y->view_col = view_col;
	a11y->row = row;
	ATK_OBJECT (a11y) ->role = ATK_ROLE_TABLE_CELL;

	if (item)
		g_object_ref (item);

#if 0
	if (parent)
		g_object_ref (parent);

	if (cell_view)
		g_object_ref (cell_view);

#endif
}
