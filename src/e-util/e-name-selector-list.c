/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Srinivasa Ragavan <sragavan@novell.com>
 * SPDX-FileContributor: Devashish Sharma  <sdevashish@novell.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>

#include "e-name-selector-list.h"
#include "e-ui-action-group.h"
#include "e-name-selector-entry.h"

#define MAX_ROW	10

struct _ENameSelectorListPrivate {
	GtkWindow *popup;
	GtkWidget *tree_view;
	GtkWidget *menu;
	gint rows;
	GdkDevice *grab_keyboard;
	GdkDevice *grab_pointer;
};

G_DEFINE_TYPE_WITH_PRIVATE (ENameSelectorList, e_name_selector_list, E_TYPE_NAME_SELECTOR_ENTRY)

/* Signals */

static void
enl_popup_size (ENameSelectorList *list)
{
	gint height = 0, count;
	GtkAllocation allocation;
	GtkTreeViewColumn *column = NULL;

	column = gtk_tree_view_get_column ( GTK_TREE_VIEW (list->priv->tree_view), 0);
	if (column)
		gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, NULL, &height);

	/* Show a maximum of 10 rows in the popup list view */
	count = list->priv->rows;
	if (count > MAX_ROW)
		count = MAX_ROW;
	if (count <= 0)
		count = 1;

	gtk_widget_get_allocation (GTK_WIDGET (list), &allocation);
	gtk_widget_set_size_request (list->priv->tree_view, allocation.width - 3 , height * count);
}

static void
enl_popup_position (ENameSelectorList *list)
{
	GtkAllocation allocation;
	GdkWindow *window;
	gint x,y;

	gtk_widget_get_allocation (GTK_WIDGET (list), &allocation);

	enl_popup_size (list);
	window = gtk_widget_get_window (GTK_WIDGET (list));
	gdk_window_get_origin (window, &x, &y);
	y = y + allocation.height;

	gtk_window_move (list->priv->popup, x, y);
}

static gboolean
popup_grab_on_window (GdkWindow *window,
                      GdkDevice *keyboard,
                      GdkDevice *pointer,
                      guint32 activate_time)
{
	if (keyboard && gdk_device_grab (keyboard, window,
			GDK_OWNERSHIP_WINDOW, TRUE,
			GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
			NULL, activate_time) != GDK_GRAB_SUCCESS)
		return FALSE;

	if (pointer && gdk_device_grab (pointer, window,
			GDK_OWNERSHIP_WINDOW, TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK,
			NULL, activate_time) != GDK_GRAB_SUCCESS) {
		if (keyboard)
			gdk_device_ungrab (keyboard, activate_time);

		return FALSE;
	}

	return TRUE;
}

static void
enl_popup_grab (ENameSelectorList *list,
                const GdkEvent *event)
{
	EDestinationStore *store;
	ENameSelectorEntry *entry;
	GdkWindow *window;
	GdkDevice *device = NULL;
	GdkDevice *keyboard, *pointer;
	gint len;

	if (list->priv->grab_pointer && list->priv->grab_keyboard)
		return;

	window = gtk_widget_get_window (GTK_WIDGET (list->priv->popup));

	if (event)
		device = gdk_event_get_device (event);
	if (!device)
		device = gtk_get_current_event_device ();
	if (!device) {
		GdkDeviceManager *device_manager;

		device_manager = gdk_display_get_device_manager (gtk_widget_get_display (GTK_WIDGET (list)));
		device = gdk_device_manager_get_client_pointer (device_manager);
	}

	if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD) {
		keyboard = device;
		pointer = gdk_device_get_associated_device (device);
	} else {
		pointer = device;
		keyboard = gdk_device_get_associated_device (device);
	}

	if (!popup_grab_on_window (window, keyboard, pointer, gtk_get_current_event_time ()))
		return;

	gtk_widget_grab_focus ((GtkWidget *) list);

	/* Build the listview from the model */
	entry = E_NAME_SELECTOR_ENTRY (list);
	store = e_name_selector_entry_peek_destination_store (entry);
	gtk_tree_view_set_model (
		GTK_TREE_VIEW (list->priv->tree_view),
		GTK_TREE_MODEL (store));

	/* If any selection of text is present, unselect it */
	len = strlen (gtk_entry_get_text (GTK_ENTRY (list)));
	gtk_editable_select_region (GTK_EDITABLE (list), len, -1);

	gtk_device_grab_add (GTK_WIDGET (list->priv->popup), pointer, TRUE);
	list->priv->grab_keyboard = keyboard;
	list->priv->grab_pointer = pointer;
}

static void
enl_popup_ungrab (ENameSelectorList *list)
{
	if (!list->priv->grab_pointer ||
	    !list->priv->grab_keyboard ||
	    !gtk_widget_has_grab (GTK_WIDGET (list->priv->popup)))
		return;

	gtk_device_grab_remove (GTK_WIDGET (list->priv->popup), list->priv->grab_pointer);
	gtk_device_grab_remove (GTK_WIDGET (list->priv->popup), list->priv->grab_keyboard);

	list->priv->grab_pointer = NULL;
	list->priv->grab_keyboard = NULL;
}

static gboolean
enl_entry_focus_out (ENameSelectorList *list,
                     GdkEventFocus *event,
                     gpointer dummy)
{
	/* When we lose focus and popup is still present hide it. Dont do it, when we click the popup. Look for grab */
	if (gtk_widget_get_visible (GTK_WIDGET (list->priv->popup))
	    && !gtk_widget_has_grab (GTK_WIDGET (list->priv->popup))) {
		enl_popup_ungrab (list);
		gtk_widget_hide ((GtkWidget *) list->priv->popup);

		return FALSE;
	}

	return FALSE;
}

static gboolean
enl_popup_button_press (GtkWidget *widget,
                        GdkEventButton *event,
                        ENameSelectorList *list)
{
	if (!gtk_widget_get_mapped (widget))
		return FALSE;

	/* if we come here, it's usually time to popdown */
	gtk_widget_hide ((GtkWidget *) list->priv->popup);

	return TRUE;
}

static gboolean
enl_popup_focus_out (GtkWidget *w,
                     GdkEventFocus *event,
                     ENameSelectorList *list)
{
	/* Just ungrab. We lose focus on button press event */
	enl_popup_ungrab (list);
	return TRUE;
}

static gboolean
enl_popup_enter_notify (GtkWidget *widget,
                        GdkEventCrossing *event,
                        ENameSelectorList *list)
{
	if (gdk_event_get_event_type ((GdkEvent *) event) == GDK_ENTER_NOTIFY && !gtk_widget_has_grab (GTK_WIDGET (list->priv->popup)))
		enl_popup_grab (list, (GdkEvent *) event);

	return TRUE;
}

static void
enl_tree_select_node (ENameSelectorList *list,
                      gint n)
{
	EDestinationStore *store;
	ENameSelectorEntry *entry;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	GtkTreePath *path;

	entry = E_NAME_SELECTOR_ENTRY (list);
	tree_view = GTK_TREE_VIEW (list->priv->tree_view);
	store = e_name_selector_entry_peek_destination_store (entry);
	selection = gtk_tree_view_get_selection (tree_view);
	iter.stamp = e_destination_store_get_stamp (store);
	iter.user_data = GINT_TO_POINTER (n - 1);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	column = gtk_tree_view_get_column (tree_view, 0);
	path = e_destination_store_get_path (GTK_TREE_MODEL (store), &iter);
	gtk_tree_view_scroll_to_cell (tree_view, path, column, FALSE, 0, 0);
	gtk_tree_view_set_cursor (tree_view, path, column, FALSE);
	gtk_widget_grab_focus (GTK_WIDGET (tree_view));
	/*Fixme: We should grab the focus to the column. How? */

	gtk_tree_path_free (path);
}

static gboolean
enl_entry_key_press_event (ENameSelectorList *list,
                           GdkEventKey *event,
                           gpointer dummy)
{
	/* Disabled for now, see https://gitlab.gnome.org/GNOME/evolution/issues/788 */
	/*if ( (event->state & GDK_CONTROL_MASK) && (event->keyval == GDK_KEY_Down)) {
		ENameSelectorEntry *entry;
		EDestinationStore *store;

		entry = E_NAME_SELECTOR_ENTRY (list);
		store = e_name_selector_entry_peek_destination_store (entry);

		enl_popup_position (list);
		gtk_widget_show_all (GTK_WIDGET (list->priv->popup));
		enl_popup_grab (list, (GdkEvent *) event);
		list->priv->rows = e_destination_store_get_destination_count (store);
		enl_popup_size (list);
		enl_tree_select_node (list, 1);
		return TRUE;
	}*/

	return FALSE;
}

static void
delete_row (GtkTreePath *path,
            ENameSelectorList *list)
{
	ENameSelectorEntry *entry;
	EDestinationStore *store;
	GtkTreeIter   iter;
	gint n, len;
	GtkTreeSelection *selection;

	entry = E_NAME_SELECTOR_ENTRY (list);
	store = e_name_selector_entry_peek_destination_store (entry);

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
		return;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->tree_view));
	len = e_destination_store_get_destination_count (store);
	n = GPOINTER_TO_INT (iter.user_data);

	e_destination_store_remove_destination_nth (store, n);

	/* If the last one is deleted select the last but one or the deleted +1 */
	if (n == len -1)
		n -= 1;

	/* We deleted the last entry */
	if (len == 1) {
		enl_popup_ungrab (list);
		if (list->priv->menu)
			gtk_menu_popdown (GTK_MENU (list->priv->menu));
		gtk_widget_hide (GTK_WIDGET (list->priv->popup));
		return;
	}

	iter.stamp = e_destination_store_get_stamp (store);
	iter.user_data = GINT_TO_POINTER (n);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	gtk_tree_path_free (path);

	list->priv->rows = e_destination_store_get_destination_count (store);
	enl_popup_size (list);
}

static void
destination_set_list (EUIAction *action,
                      GVariant *value,
                      EDestination *destination)
{
	gboolean status = g_variant_get_boolean (value);

	e_ui_action_set_state (action, value);
	e_destination_set_ignored (destination, !status);
}

static void
destination_set_email (EUIAction *action,
                       GVariant *value,
                       EDestination *destination)
{
	EContact *contact;
	gint email_num;

	contact = e_destination_get_contact (destination);
	if (!contact)
		return;

	email_num = g_variant_get_int32 (value);
	e_ui_action_set_state (action, value);
	e_destination_set_contact (destination, contact, email_num);
}

typedef struct {
	ENameSelectorList *list;
	GtkTreePath *path;
} PopupDeleteRowInfo;

static void
popup_delete_row_info_free (gpointer data)
{
	PopupDeleteRowInfo *row_info = data;

	g_clear_pointer (&row_info->path, gtk_tree_path_free);
	g_slice_free (PopupDeleteRowInfo, row_info);
}

static void
popup_delete_row (EUIAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
	PopupDeleteRowInfo *row_info = user_data;
	GtkTreePath *path = g_steal_pointer (&row_info->path);

	delete_row (path, row_info->list);
}

static void
menu_deactivate (GtkMenuShell *junk,
                 ENameSelectorList *list)
{
	enl_popup_grab (list, NULL);
}

static gboolean
enl_tree_button_press_event (GtkWidget *widget,
                             GdkEventButton *event,
                             ENameSelectorList *list)
{
	EUIActionGroup *action_group;
	EUIAction *action;
	GtkWidget *menu;
	GMenu *menu_model, *email_section, *delete_section;
	EDestination *destination;
	ENameSelectorEntry *entry;
	EDestinationStore *store;
	EContact     *contact;
	GList        *email_list = NULL, *l;
	gint          i;
	gint	      email_num, len;
	gchar         *delete_label;
	gchar         *action_name, *full_action_name;
	gboolean      is_list;
	gboolean      show_menu = FALSE;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreePath  *path;
	PopupDeleteRowInfo *row_info;
	GtkTreeIter   iter;
	guint         button = 0;
	gdouble       x = 0, y = 0;
	gint          jj;

	entry = E_NAME_SELECTOR_ENTRY (list);
	tree_view = GTK_TREE_VIEW (list->priv->tree_view);
	store = e_name_selector_entry_peek_destination_store (entry);


	gdk_event_get_button ((GdkEvent *) event, &button);
	gdk_event_get_coords ((GdkEvent *) event, &x, &y);

	if (!gtk_widget_has_grab (GTK_WIDGET (list->priv->popup)))
		enl_popup_grab (list, (GdkEvent *) event);

	if (!gtk_tree_view_get_dest_row_at_pos (
		tree_view, x, y, &path, NULL))
		return FALSE;
	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
		return FALSE;

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	if (button != GDK_BUTTON_SECONDARY) {
		return FALSE;
	}

	destination = e_destination_store_get_destination (store, &iter);

	if (!destination)
		return FALSE;

	contact = e_destination_get_contact (destination);
	if (!contact)
		return FALSE;

	if (list->priv->menu) {
		gtk_menu_popdown (GTK_MENU (list->priv->menu));
	}

	action_group = e_ui_action_group_new ("ensl");
	email_section = g_menu_new ();

	email_num = e_destination_get_email_num (destination);

	/* Addresses */
	is_list = e_contact_get (contact, E_CONTACT_IS_LIST) ? TRUE : FALSE;
	if (is_list) {
		const GList *dests = e_destination_list_get_dests (destination);
		GList *iters;
		gint length = g_list_length ((GList *) dests);

		for (iters = (GList *) dests, jj = 0; iters; iters = iters->next, jj++) {
			EDestination *dest = (EDestination *) iters->data;
			const gchar *email = e_destination_get_email (dest);

			if (!email || *email == '\0')
				continue;

			if (length > 1) {
				action_name = g_strdup_printf ("toggle-dest-%d", jj);
				full_action_name = g_strdup_printf ("ensl.%s", action_name);

				action = e_ui_action_new_stateful ("ensl", action_name, NULL,
					g_variant_new_boolean (!e_destination_is_ignored (dest)));
				e_ui_action_set_label (action, email);
				g_signal_connect (action, "change-state",
					G_CALLBACK (destination_set_list), dest);
				e_ui_action_group_add (action_group, action);
				g_object_unref (action);

				g_menu_append (email_section, email, full_action_name);

				g_free (full_action_name);
				g_free (action_name);
			} else {
				g_menu_append (email_section, email, NULL);
			}

			show_menu = TRUE;
		}

	} else {
		email_list = e_contact_get (contact, E_CONTACT_EMAIL);
		len = g_list_length (email_list);

		if (len > 1) {
			action = e_ui_action_new_stateful ("ensl", "select-email", G_VARIANT_TYPE_INT32,
				g_variant_new_int32 (email_num));
			e_ui_action_set_label (action, _("Select Email Address"));
			g_signal_connect (action, "change-state",
				G_CALLBACK (destination_set_email), destination);
			e_ui_action_group_add (action_group, action);
			g_object_unref (action);
		}

		for (l = email_list, i = 0; l; l = g_list_next (l), i++) {
			gchar *email = l->data;

			if (!email || *email == '\0')
				continue;

			if (len > 1) {
				GMenuItem *item = g_menu_item_new (email, NULL);
				g_menu_item_set_action_and_target (item, "ensl.select-email", "i", i);
				g_menu_append_item (email_section, item);
				g_object_unref (item);
			} else {
				g_menu_append (email_section, email, NULL);
			}

			show_menu = TRUE;
		}
		g_list_foreach (email_list, (GFunc) g_free, NULL);
		g_list_free (email_list);
	}

	row_info = g_slice_new (PopupDeleteRowInfo);
	row_info->list = list;
	row_info->path = path;

	delete_label = g_strdup_printf (_("_Delete %s"), (gchar *) e_contact_get_const (contact, E_CONTACT_FILE_AS));
	action = e_ui_action_new ("ensl", "delete-row", NULL);
	e_ui_action_set_label (action, delete_label);
	g_signal_connect_data (action, "activate",
		G_CALLBACK (popup_delete_row), row_info,
		(GClosureNotify) popup_delete_row_info_free, 0);
	e_ui_action_group_add (action_group, action);
	g_object_unref (action);
	delete_section = g_menu_new ();
	g_menu_append (delete_section, delete_label, "ensl.delete-row");
	g_free (delete_label);

	menu_model = g_menu_new ();
	g_menu_append_section (menu_model, NULL, G_MENU_MODEL (delete_section));
	g_object_unref (delete_section);
	if (show_menu)
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (email_section));
	g_object_unref (email_section);

	menu = gtk_menu_new_from_model (G_MENU_MODEL (menu_model));
	g_object_unref (menu_model);
	gtk_widget_insert_action_group (menu, "ensl", G_ACTION_GROUP (action_group));
	g_object_unref (action_group);

	g_signal_connect (menu, "deactivate", G_CALLBACK (menu_deactivate), list);
	list->priv->menu = menu;
	gtk_menu_attach_to_widget (GTK_MENU (menu), widget, NULL);
	gtk_menu_popup_at_pointer (GTK_MENU (menu), (const GdkEvent *) event);

	return TRUE;

}

static gboolean
enl_tree_key_press_event (GtkWidget *w,
                          GdkEventKey *event,
                          ENameSelectorList *list)
{
	guint keyval = 0;

	gdk_event_get_keyval ((GdkEvent *) event, &keyval);

	if (keyval == GDK_KEY_Escape) {
		enl_popup_ungrab (list);
		gtk_widget_hide ( GTK_WIDGET (list->priv->popup));
		return TRUE;
	} else if (keyval == GDK_KEY_Delete) {
		GtkTreeSelection *selection;
		GtkTreeView *tree_view;
		GList *paths;

		tree_view = GTK_TREE_VIEW (list->priv->tree_view);
		selection = gtk_tree_view_get_selection (tree_view);
		paths = gtk_tree_selection_get_selected_rows (selection, NULL);
		paths = g_list_reverse (paths);
		g_list_foreach (paths, (GFunc) delete_row, list);
		g_list_free (paths);
	} else if (keyval != GDK_KEY_Up && keyval != GDK_KEY_Down
		   && keyval != GDK_KEY_Shift_R && keyval != GDK_KEY_Shift_L
		   && keyval != GDK_KEY_Control_R && keyval != GDK_KEY_Control_L) {
		enl_popup_ungrab (list);
		gtk_widget_hide ( GTK_WIDGET (list->priv->popup));
		gtk_widget_event (GTK_WIDGET (list), (GdkEvent *) event);
		return TRUE;
	}

	return FALSE;
}

void
e_name_selector_list_expand_clicked (ENameSelectorList *list)
{
	ENameSelectorEntry *entry;
	EDestinationStore *store;

	entry = E_NAME_SELECTOR_ENTRY (list);
	store = e_name_selector_entry_peek_destination_store (entry);

	if (!gtk_widget_get_visible (GTK_WIDGET (list->priv->popup))) {
		enl_popup_position (list);
		gtk_widget_show_all (GTK_WIDGET (list->priv->popup));
		enl_popup_grab (list, NULL);
		list->priv->rows = e_destination_store_get_destination_count (store);
		enl_popup_size (list);
		enl_tree_select_node (list, 1);
	}
	else {
		enl_popup_ungrab (list);
		if (list->priv->menu)
			gtk_menu_popdown (GTK_MENU (list->priv->menu));
		gtk_widget_hide (GTK_WIDGET (list->priv->popup));
	}
}

static void
name_selector_list_realize (GtkWidget *widget)
{
	ENameSelectorList *list;
	ENameSelectorEntry *entry;
	EDestinationStore *store;

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_name_selector_list_parent_class)->realize (widget);

	list = E_NAME_SELECTOR_LIST (widget);
	entry = E_NAME_SELECTOR_ENTRY (widget);
	store = e_name_selector_entry_peek_destination_store (entry);

	gtk_tree_view_set_model (
		GTK_TREE_VIEW (list->priv->tree_view), GTK_TREE_MODEL (store));
}

static void
name_selector_list_dispose (GObject *object)
{
	ENameSelectorList *list;

	list = E_NAME_SELECTOR_LIST (object);

	if (list->priv->popup) {
		gtk_widget_destroy (GTK_WIDGET (list->priv->popup));
		list->priv->popup = NULL;
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_name_selector_list_parent_class)->dispose (object);
}

static void
e_name_selector_list_class_init (ENameSelectorListClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = name_selector_list_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = name_selector_list_realize;
}

static void
e_name_selector_list_init (ENameSelectorList *list)
{
	GtkCellRenderer *renderer;
	GtkWidget *scroll, *popup_frame, *vgrid;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	ENameSelectorEntry *entry;
	EDestinationStore *store;
	GtkEntryCompletion *completion;

	list->priv = e_name_selector_list_get_instance_private (list);
	list->priv->menu = NULL;

	entry = E_NAME_SELECTOR_ENTRY (list);
	store = e_name_selector_entry_peek_destination_store (entry);

	list->priv->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list->priv->tree_view), FALSE);
	gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (list->priv->tree_view), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_unselect_all (selection);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (list->priv->tree_view), FALSE);

	completion = gtk_entry_get_completion (GTK_ENTRY (list));
	gtk_entry_completion_set_inline_completion (completion, TRUE);
	gtk_entry_completion_set_popup_completion (completion, TRUE);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", E_DESTINATION_STORE_COLUMN_ADDRESS, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (list->priv->tree_view), column);
	gtk_tree_view_column_set_clickable (column, TRUE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_NONE);
	gtk_widget_set_size_request (
		gtk_scrolled_window_get_vscrollbar (
		GTK_SCROLLED_WINDOW (scroll)), -1, 0);
	gtk_widget_set_vexpand (scroll, TRUE);
	gtk_widget_set_valign (scroll, GTK_ALIGN_FILL);

	list->priv->popup = GTK_WINDOW (gtk_window_new (GTK_WINDOW_POPUP));
	gtk_window_set_resizable (GTK_WINDOW (list->priv->popup), FALSE);

	popup_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (
		GTK_FRAME (popup_frame), GTK_SHADOW_ETCHED_IN);

	gtk_container_add (GTK_CONTAINER (list->priv->popup), popup_frame);

	vgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"column-homogeneous", FALSE,
		"row-spacing", 0,
		NULL);
	gtk_container_add (GTK_CONTAINER (popup_frame), vgrid);

	gtk_container_add (GTK_CONTAINER (scroll), list->priv->tree_view);
	gtk_container_add (GTK_CONTAINER (vgrid), scroll);

	g_signal_connect (
		GTK_WIDGET (list), "focus-out-event",
		G_CALLBACK (enl_entry_focus_out), NULL);
	g_signal_connect (
		GTK_WIDGET (list), "key-press-event",
		G_CALLBACK (enl_entry_key_press_event), NULL);

	g_signal_connect_after (
		list->priv->tree_view, "key-press-event",
		G_CALLBACK (enl_tree_key_press_event), list);
	g_signal_connect (
		list->priv->tree_view, "button-press-event",
		G_CALLBACK (enl_tree_button_press_event), list);

	g_signal_connect (
		list->priv->popup, "button-press-event",
		G_CALLBACK (enl_popup_button_press), list);
	g_signal_connect (
		list->priv->popup, "focus-out-event",
		G_CALLBACK (enl_popup_focus_out), list);
	g_signal_connect (
		list->priv->popup, "enter-notify-event",
		G_CALLBACK (enl_popup_enter_notify), list);

}

GtkWidget *
e_name_selector_list_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_NAME_SELECTOR_LIST,
		"client-cache", client_cache, NULL);
}
