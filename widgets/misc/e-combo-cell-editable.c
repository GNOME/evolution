/*
 * e-combo-cell-editable.c
 *
 * Author: Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 2003 Ximian Inc.
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
 */

#include <config.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcelleditable.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkmain.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkwindow.h>

#include "e-combo-cell-editable.h"

struct _EComboCellEditablePriv {
	GtkEntry *entry;
	GtkWidget *popup;
	GtkTreeView *tree_view;
	gboolean cancelled;
	GList *list;
};

#define GRAB_MASK  (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK)

static GtkEventBoxClass *parent_class;

static void
kill_popup (EComboCellEditable *ecce)
{
	gtk_grab_remove (GTK_WIDGET (ecce->priv->tree_view));
	gtk_widget_destroy (ecce->priv->popup);

	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (ecce));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (ecce));
}
	 
static gboolean
popup_key_press_cb (GtkWidget *widget, GdkEventKey *event, EComboCellEditable *ecce)
{
	switch (event->keyval) {
	case GDK_Escape:
		ecce->priv->cancelled = TRUE;
		kill_popup (ecce);
		break;

	case GDK_Return:
	case GDK_KP_Enter:
		ecce->priv->cancelled = FALSE;
		kill_popup (ecce);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

static gboolean
popup_button_press_cb (GtkWidget *widget, GdkEventButton *event, EComboCellEditable *ecce)
{
	GtkAllocation alloc;
	gdouble rel_x, rel_y;
	gint win_x, win_y;

	if (event->button != 1)
		return FALSE;
	
	gdk_window_get_root_origin (widget->window, &win_x, &win_y);
	alloc = ecce->priv->popup->allocation;

	rel_x = event->x_root - win_x - alloc.x;
	rel_y = event->y_root - win_y - alloc.y;
	
	if (rel_x > 0 && rel_x < alloc.width && rel_y > 0 && rel_y < alloc.height)
		return FALSE;
	
	ecce->priv->cancelled = TRUE;
	kill_popup (ecce);
	
	return FALSE;
}

static gboolean
tree_button_release_cb (GtkWidget *widget, GdkEventButton *event, EComboCellEditable *ecce)
{
	kill_popup (ecce);
	return TRUE;
}
	
static void 
selection_changed_cb (GtkTreeSelection *selection, EComboCellEditable *ecce)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text;

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, 0, &text, -1);
	e_combo_cell_editable_set_text (ecce, text);
	g_free (text);
}

static void
build_popup (EComboCellEditable *ecce)
{
	GtkWidget *frame;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *l;

	ecce->priv->popup = gtk_window_new (GTK_WINDOW_POPUP);
	
	g_signal_connect (ecce->priv->popup, "button-press-event", G_CALLBACK (popup_button_press_cb), ecce);
	g_signal_connect (ecce->priv->popup, "key-press-event", G_CALLBACK (popup_key_press_cb), ecce);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (ecce->priv->popup), frame);

	ecce->priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));

	for (l = ecce->priv->list; l; l = l->next) {
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, l->data, -1);
	}
	
	gtk_tree_view_set_model (ecce->priv->tree_view, model);
	g_object_unref (model);
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (ecce->priv->tree_view));
	
	gtk_tree_view_set_headers_visible (ecce->priv->tree_view, FALSE);

	gtk_tree_view_insert_column_with_attributes (ecce->priv->tree_view, 0, NULL,
						     gtk_cell_renderer_text_new (),
						     "text", 0,
						     NULL);

	g_signal_connect (ecce->priv->tree_view, "button-release-event", G_CALLBACK (tree_button_release_cb), ecce);

	selection = gtk_tree_view_get_selection (ecce->priv->tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	g_signal_connect (selection, "changed", G_CALLBACK (selection_changed_cb), ecce);
	
	gtk_widget_show (GTK_WIDGET (ecce->priv->tree_view));
}

static gint
lookup_row (GList *list, const gchar *text)
{
	GList *l;
	gint result = 0;

	for (l = list; l; l = l->next, result++)
		if (!g_utf8_collate (text, (char *) l->data))
			break;

	return result;
}

static void 
set_cursor (GtkTreeView *tree_view, gint index)
{
	GtkTreePath *path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, index);

	gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
	
	gtk_tree_path_free (path);
}

static void
grab_popup (GdkWindow *popup)
{
	gdk_pointer_grab (popup, TRUE, GRAB_MASK, NULL, NULL, gtk_get_current_event_time ());
	gdk_keyboard_grab (popup, TRUE, gtk_get_current_event_time ());
}

static void
position_popup (EComboCellEditable *ecce, gint x, gint y, gint offset) 
{
	GtkRequisition req;

	gtk_widget_realize (ecce->priv->popup);
	gtk_widget_size_request (ecce->priv->popup, &req);
	
	if (req.height > gdk_screen_height () - y) {
		y -= (offset + req.height);
		if (y < 0)
			y = 0;
	}

	gtk_window_move (GTK_WINDOW (ecce->priv->popup), x, y);
	gtk_widget_show (ecce->priv->popup);
}

static void
show_popup (EComboCellEditable *ecce)
{
	gint row;
	GtkAllocation  alloc;
	gint x, y;

	if (!ecce->priv->list)
		return;

	build_popup (ecce);
	row = lookup_row (ecce->priv->list, e_combo_cell_editable_get_text (ecce));
	set_cursor (ecce->priv->tree_view, row);

	gtk_editable_select_region (GTK_EDITABLE (ecce->priv->entry), 0, 0);
	gdk_window_get_origin (GTK_WIDGET (ecce)->window, &x, &y);
	alloc = GTK_WIDGET (ecce)->allocation;

	position_popup (ecce, x, y + alloc.height, alloc.height);

	gtk_grab_add (GTK_WIDGET (ecce->priv->popup));
	gtk_widget_grab_focus (GTK_WIDGET (ecce->priv->tree_view));
	grab_popup (GTK_WIDGET (ecce->priv->popup)->window);
}

static void
button_clicked_cb (GtkButton *btn, EComboCellEditable *ecce)
{
	if (ecce->priv->popup) {
		kill_popup (ecce);
		return;
	}

	show_popup (ecce);
}

static void
entry_activated_cb (GtkEntry *entry, EComboCellEditable *widget)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (widget));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (widget));
}

static gboolean
entry_key_press_event_cb (GtkEntry *entry, GdkEventKey *key_event, EComboCellEditable *ecce)
{
	if (key_event->keyval == GDK_Escape) {
		ecce->priv->cancelled = TRUE;
		gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (ecce));
		gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (ecce));
		return TRUE;
	}

	if (key_event->state & GDK_MOD1_MASK
		&& key_event->keyval == GDK_Down) {
		if (!ecce->priv->popup)
			show_popup (ecce);

		return TRUE;
	}

	return FALSE;
}

static void
ecce_start_editing (GtkCellEditable *cell_editable, GdkEvent *event)
{
	EComboCellEditable *ecce = E_COMBO_CELL_EDITABLE (cell_editable);

	gtk_editable_select_region (GTK_EDITABLE (ecce->priv->entry), 0, -1);
}

static void
ecce_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = ecce_start_editing;
}

static void
ecce_finalize (GObject *obj)
{
	EComboCellEditable *ecce = (EComboCellEditable *) obj;

	g_free (ecce->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
ecce_init (EComboCellEditable *ecce)
{
	GtkWidget *entry, *btn, *box;

	box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (box);
	gtk_container_add (GTK_CONTAINER (ecce), box);
	
	ecce->priv = g_new0 (EComboCellEditablePriv, 1);

	entry = gtk_entry_new ();
	ecce->priv->entry = GTK_ENTRY (entry);
	gtk_entry_set_has_frame (ecce->priv->entry, FALSE);
	gtk_entry_set_editable (ecce->priv->entry, FALSE);
	g_signal_connect (entry, "activate", G_CALLBACK (entry_activated_cb), ecce);
	g_signal_connect (entry, "key_press_event", G_CALLBACK (entry_key_press_event_cb), ecce);
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);

	btn = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (btn), gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT));
	g_signal_connect (btn, "clicked", (GCallback) button_clicked_cb, ecce);
	gtk_widget_show_all (btn);
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, TRUE, 0);
}

static void
ecce_grab_focus (GtkWidget *widget)
{
	EComboCellEditable *ecce = E_COMBO_CELL_EDITABLE (widget);

	gtk_widget_grab_focus (GTK_WIDGET (ecce->priv->entry));
}

static void
ecce_class_init (GObjectClass *klass)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

	klass->finalize = ecce_finalize;
	
	widget_class->grab_focus = ecce_grab_focus;
	
	parent_class = GTK_EVENT_BOX_CLASS (g_type_class_peek_parent (klass));
}

GType
e_combo_cell_editable_get_type (void)
{
	static GType ecce_type = 0;
	
	if (!ecce_type) {
		static const GTypeInfo ecce_info = {
			sizeof (EComboCellEditableClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) ecce_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (EComboCellEditable),
			0,              /* n_preallocs */
			(GInstanceInitFunc) ecce_init,
		};

		static const GInterfaceInfo cell_editable_info = {
			(GInterfaceInitFunc) ecce_cell_editable_init,
			NULL, 
			NULL 
		};
      
		ecce_type = g_type_register_static (GTK_TYPE_EVENT_BOX, "EComboCellEditable", &ecce_info, 0);
		
		g_type_add_interface_static (ecce_type, GTK_TYPE_CELL_EDITABLE, &cell_editable_info);
	}
	
	return ecce_type;
}

GtkCellEditable *
e_combo_cell_editable_new ()
{
	return GTK_CELL_EDITABLE (g_object_new (E_TYPE_COMBO_CELL_EDITABLE, NULL));
}

const GList *
e_combo_cell_editable_get_list (EComboCellEditable *ecce)
{
	g_return_val_if_fail (E_COMBO_CELL_EDITABLE (ecce), NULL);

	return ecce->priv->list;
}

void
e_combo_cell_editable_set_list (EComboCellEditable *ecce, GList *list)
{
	g_return_if_fail (E_IS_COMBO_CELL_EDITABLE (ecce));

	ecce->priv->list = list;
}

const gchar *
e_combo_cell_editable_get_text (EComboCellEditable *ecce)
{
	g_return_val_if_fail (E_COMBO_CELL_EDITABLE (ecce), NULL);

	return gtk_entry_get_text (ecce->priv->entry);
}

void
e_combo_cell_editable_set_text (EComboCellEditable *ecce, const gchar *text)
{
	g_return_if_fail (E_IS_COMBO_CELL_EDITABLE (ecce));

	gtk_entry_set_text (ecce->priv->entry, text ? text : "");
}

gboolean
e_combo_cell_editable_cancelled (EComboCellEditable *ecce)
{
	g_return_val_if_fail (E_IS_COMBO_CELL_EDITABLE (ecce), FALSE);

	return ecce->priv->cancelled;
}

