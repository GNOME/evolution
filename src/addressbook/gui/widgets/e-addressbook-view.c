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
 *		Chris Lahey <clahey@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <ctype.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-addressbook-view.h"

#include "e-util/e-util.h"
#include "shell/e-shell-sidebar.h"

#include "addressbook/printing/e-contact-print.h"
#include "ea-addressbook.h"

#include "e-card-view.h"
#include "gal-view-minicard.h"

#include "e-addressbook-model.h"
#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "e-addressbook-table-adapter.h"
#include "eab-contact-merging.h"

#define d(x)

static void	e_addressbook_view_delete_selection
						(EAddressbookView *view,
						 gboolean is_delete);
static void	search_result			(EAddressbookView *view,
						 const GError *error);
static void	stop_state_changed		(GObject *object,
						 EAddressbookView *view);
static void	command_state_change		(EAddressbookView *view);

struct _EAddressbookViewPrivate {
	gpointer shell_view;  /* weak pointer */

	EAddressbookModel *model;
	EActivity *activity;

	ESource *source;

	GObject *object;

	GalViewInstance *view_instance;

	/* stored search setup for this view */
	gint filter_id;
	gchar *search_text;
	gint search_id;
	EFilterRule *advanced_search;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	GSList *previous_selection; /* EContact * */
	EContact *cursor_contact;
	gint cursor_col;
	gboolean awaiting_search_start;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_PASTE_TARGET_LIST,
	PROP_SHELL_VIEW,
	PROP_SOURCE
};

enum {
	OPEN_CONTACT,
	POPUP_EVENT,
	COMMAND_STATE_CHANGE,
	SELECTION_CHANGE,
	STATUS_MESSAGE,
	LAST_SIGNAL
};

enum {
	DND_TARGET_TYPE_SOURCE_VCARD,
	DND_TARGET_TYPE_VCARD
};

static GtkTargetEntry drag_types[] = {
	{ (gchar *) "text/x-source-vcard", 0, DND_TARGET_TYPE_SOURCE_VCARD },
	{ (gchar *) "text/x-vcard", 0, DND_TARGET_TYPE_VCARD }
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_addressbook_view_selectable_init
					(ESelectableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EAddressbookView, e_addressbook_view, GTK_TYPE_SCROLLED_WINDOW,
	G_ADD_PRIVATE (EAddressbookView)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, e_addressbook_view_selectable_init))

static ESelectionModel *
e_addressbook_view_get_selection_model (EAddressbookView *view)
{
	GalView *gal_view;
	GalViewInstance *view_instance;
	ESelectionModel *model = NULL;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	if (GAL_IS_VIEW_ETABLE (gal_view)) {
		GtkWidget *child;

		child = gtk_bin_get_child (GTK_BIN (view));
		model = e_table_get_selection_model (E_TABLE (child));

	} else if (GAL_IS_VIEW_MINICARD (gal_view)) {
		g_warn_if_reached ();
	}

	return model;
}

static void
addressbook_view_emit_open_contact (EAddressbookView *view,
                                    EContact *contact,
                                    gboolean is_new_contact)
{
	g_signal_emit (view, signals[OPEN_CONTACT], 0, contact, is_new_contact);
}

static void
addressbook_view_emit_popup_event (EAddressbookView *view,
                                   GdkEvent *event)
{
	GalView *gal_view;
	GalViewInstance *view_instance;
	GtkWidget *focused_widget = NULL;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
	if (GTK_IS_WINDOW (toplevel))
		focused_widget = gtk_window_get_focus (GTK_WINDOW (toplevel));

	/* Grab focus so that EFocusTracker asks us to update the
	 * selection-related actions before showing the popup menu.
	 * Apparently ETable doesn't automatically grab focus on
	 * right-clicks (is that a bug?). */
	gtk_widget_grab_focus (GTK_WIDGET (view));

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	/* Restore focus for the minicard view */
	if (GAL_IS_VIEW_MINICARD (gal_view) && focused_widget)
		gtk_widget_grab_focus (focused_widget);

	if (view->priv->shell_view)
		e_shell_view_update_actions (view->priv->shell_view);

	g_signal_emit (view, signals[POPUP_EVENT], 0, event);
}

static void
addressbook_view_emit_selection_change (EAddressbookView *view)
{
	if (!view->priv->awaiting_search_start &&
	    e_addressbook_view_get_n_selected (view) > 0) {
		g_slist_free_full (view->priv->previous_selection, g_object_unref);
		view->priv->previous_selection = NULL;

		g_clear_object (&view->priv->cursor_contact);
	}

	g_signal_emit (view, signals[SELECTION_CHANGE], 0);
}

static void
addressbook_view_child_activated_got_contacts_cb (GObject *source_object,
						  GAsyncResult *result,
						  gpointer user_data)
{
	EAddressbookView *view = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_contact_card_box_dup_contacts_finish (E_CONTACT_CARD_BOX (source_object), result, &error);
	if (!contacts && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to get activated child: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else if (contacts) {
		if (contacts->len == 1)
			addressbook_view_emit_open_contact (view, g_ptr_array_index (contacts, 0), FALSE);
		else
			g_warning ("%s: Expected 1 contact to be retrieved, but received %u instead", G_STRFUNC, contacts->len);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);
	g_object_unref (view);
}

static void
addressbook_view_child_activated_cb (EContactCardBox *box,
				     guint child_index,
				     gpointer user_data)
{
	EAddressbookView *view = user_data;
	EContact *contact;

	contact = e_contact_card_box_peek_contact (box, child_index);

	if (!contact) {
		GPtrArray *indexes;

		indexes = g_ptr_array_sized_new (1);
		g_ptr_array_add (indexes, GUINT_TO_POINTER (child_index));

		e_contact_card_box_dup_contacts	(box, indexes, NULL, addressbook_view_child_activated_got_contacts_cb, g_object_ref (view));

		g_ptr_array_unref (indexes);

		return;
	}

	addressbook_view_emit_open_contact (view, contact, FALSE);
	g_clear_object (&contact);
}

static gboolean
addressbook_view_card_event_cb (EContactCardBox *box,
				guint child_index,
				GdkEvent *event,
				gpointer user_data)
{
	EAddressbookView *view = user_data;
	guint event_button = 0;

	switch (event->type) {
	case GDK_2BUTTON_PRESS:
		gdk_event_get_button (event, &event_button);
		if (event_button == GDK_BUTTON_PRIMARY) {
			if (e_addressbook_view_get_editable (view)) {
				if (child_index < e_contact_card_box_get_n_items (box)) {
					addressbook_view_child_activated_cb (box, child_index, view);
				} else {
					EContact *contact;

					contact = e_contact_new ();
					addressbook_view_emit_open_contact (view, contact, TRUE);
					g_object_unref (contact);
				}
			}
			return TRUE;
		}
		break;
	case GDK_BUTTON_PRESS:
		gdk_event_get_button (event, &event_button);
		if (event_button == GDK_BUTTON_SECONDARY) {
			if (child_index != G_MAXUINT && !e_contact_card_box_get_selected (box, child_index)) {
				e_contact_card_box_set_selected_all (box, FALSE);
				e_contact_card_box_set_selected (box, child_index, TRUE);
				e_contact_card_box_set_focused_index (box, child_index);
			}
			addressbook_view_emit_popup_event (view, event);
			return TRUE;
		}
		break;
	case GDK_KEY_PRESS:
		if (((event->key.state & GDK_SHIFT_MASK) != 0 && event->key.keyval == GDK_KEY_F10) ||
		    ((event->key.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0 && event->key.keyval == GDK_KEY_Menu)) {
			addressbook_view_emit_popup_event (view, event);
			return TRUE;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

static gboolean
addressbook_view_card_popup_menu_cb (EContactCardBox *box,
				     guint child_index,
				     gpointer user_data)
{
	EAddressbookView *view = user_data;

	addressbook_view_emit_popup_event (view, NULL);

	return TRUE;
}

static void
card_view_double_click_cb (ECardView *card_view,
			   gpointer user_data)
{
	EAddressbookView *self = user_data;
	EContact *contact;

	contact = e_contact_new ();
	addressbook_view_emit_open_contact (self, contact, TRUE);
	g_object_unref (contact);
}

static void
addressbook_view_update_folder_bar_message (EAddressbookView *view)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	const gchar *display_name, *message;
	gchar *tmp = NULL;
	guint n_total;

	if (!view->priv->source)
		return;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	n_total = e_addressbook_view_get_n_total (view);

	switch (n_total) {
	case 0:
		message = _("No contacts");
		break;
	default:
		tmp = g_strdup_printf (ngettext ("%u contact", "%u contacts", n_total), n_total);
		message = tmp;
		break;
	}

	display_name = e_source_get_display_name (view->priv->source);
	e_shell_sidebar_set_primary_text (shell_sidebar, display_name);
	e_shell_sidebar_set_secondary_text (shell_sidebar, message);

	g_free (tmp);
}

static void
table_double_click (ETable *table,
                    gint row,
                    gint col,
                    GdkEvent *event,
                    EAddressbookView *view)
{
	EAddressbookModel *model;
	EContact *contact;

	if (!E_IS_ADDRESSBOOK_TABLE_ADAPTER (view->priv->object))
		return;

	model = view->priv->model;
	contact = e_addressbook_model_get_contact (model, row);
	addressbook_view_emit_open_contact (view, contact, FALSE);
	g_object_unref (contact);
}

static gint
table_right_click (ETable *table,
                   gint row,
                   gint col,
                   GdkEvent *event,
                   EAddressbookView *view)
{
	addressbook_view_emit_popup_event (view, event);

	return TRUE;
}

static gboolean
addressbook_view_popup_menu_cb (GtkWidget *widget,
				EAddressbookView *view)
{
	addressbook_view_emit_popup_event (view, NULL);

	return TRUE;
}

static gint
table_white_space_event (ETable *table,
                         GdkEvent *event,
                         EAddressbookView *view)
{
	guint event_button = 0;

	gdk_event_get_button (event, &event_button);

	if (event->type == GDK_BUTTON_PRESS && event_button == 3) {
		addressbook_view_emit_popup_event (view, event);
		return TRUE;
	}

	return FALSE;
}

static void
table_drag_data_get (ETable *table,
                     gint row,
                     gint col,
                     GdkDragContext *context,
                     GtkSelectionData *selection_data,
                     guint info,
                     guint time,
                     gpointer user_data)
{
	EAddressbookView *view = user_data;
	EBookClient *book_client;
	GPtrArray *contacts;
	GdkAtom target;
	gchar *value;

	if (!E_IS_ADDRESSBOOK_TABLE_ADAPTER (view->priv->object))
		return;

	contacts = e_addressbook_view_peek_selected_contacts (view);
	g_return_if_fail (contacts != NULL);

	book_client = e_addressbook_view_get_client (view);
	target = gtk_selection_data_get_target (selection_data);

	switch (info) {
		case DND_TARGET_TYPE_VCARD:
			value = eab_contact_array_to_string (contacts);

			gtk_selection_data_set (
				selection_data, target, 8,
				(guchar *) value, strlen (value));

			g_free (value);
			break;

		case DND_TARGET_TYPE_SOURCE_VCARD:
			value = eab_book_and_contact_array_to_string (
				book_client, contacts);

			gtk_selection_data_set (
				selection_data, target, 8,
				(guchar *) value, strlen (value));

			g_free (value);
			break;
	}

	g_ptr_array_unref (contacts);
}

static void
addressbook_view_create_table_view (EAddressbookView *view,
                                    GalViewEtable *gal_view)
{
	ETableModel *adapter;
	ETableExtras *extras;
	ETableSpecification *specification;
	ECell *cell;
	GtkWidget *widget;
	gchar *etspecfile;
	GError *local_error = NULL;

	adapter = e_addressbook_table_adapter_new (view->priv->model);

	extras = e_table_extras_new ();

	/* Set proper format component for a default 'date' cell renderer. */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "addressbook");

	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "e-addressbook-view.etspec", NULL);
	specification = e_table_specification_new (etspecfile, &local_error);

	/* Failure here is fatal. */
	if (local_error != NULL) {
		g_error ("%s: %s", etspecfile, local_error->message);
		g_return_if_reached ();
	}

	/* Here we create the table.  We give it the three pieces of
	 * the table we've created, the header, the model, and the
	 * initial layout.  It does the rest.  */
	widget = e_table_new (adapter, extras, specification);
	g_object_set (G_OBJECT (widget), "uniform-row-height", TRUE, NULL);
	gtk_container_add (GTK_CONTAINER (view), widget);

	g_object_unref (specification);
	g_object_unref (extras);
	g_free (etspecfile);

	view->priv->object = G_OBJECT (adapter);

	g_signal_connect (
		widget, "double_click",
		G_CALLBACK (table_double_click), view);
	g_signal_connect (
		widget, "right_click",
		G_CALLBACK (table_right_click), view);
	g_signal_connect (
		widget, "popup-menu",
		G_CALLBACK (addressbook_view_popup_menu_cb), view);
	g_signal_connect (
		widget, "white_space_event",
		G_CALLBACK (table_white_space_event), view);
	g_signal_connect_swapped (
		widget, "selection_change",
		G_CALLBACK (addressbook_view_emit_selection_change), view);
	g_signal_connect_object (
		adapter, "model-row-changed",
		G_CALLBACK (addressbook_view_emit_selection_change), view, G_CONNECT_SWAPPED);

	e_table_drag_source_set (
		E_TABLE (widget), GDK_BUTTON1_MASK,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect (
		E_TABLE (widget), "table_drag_data_get",
		G_CALLBACK (table_drag_data_get), view);

	gtk_widget_show (widget);

	gal_view_etable_attach_table (gal_view, E_TABLE (widget));
}

static void
card_view_status_message_cb (EAddressbookModel *model,
			     const gchar *message,
			     gint percentage,
			     gpointer user_data)
{
	EAddressbookView *self = user_data;

	g_signal_emit (self, signals[STATUS_MESSAGE], 0, message, percentage);
}

static void
addressbook_view_create_minicard_view (EAddressbookView *view,
                                       GalViewMinicard *gal_view)
{
	GtkWidget *card_view;
	EContactCardBox *card_box;

	card_view = e_card_view_new ();
	card_box = e_card_view_get_card_box (E_CARD_VIEW (card_view));

	g_signal_connect_object (
		card_box, "child-activated",
		G_CALLBACK (addressbook_view_child_activated_cb), view, 0);

	g_signal_connect_object (
		card_box, "selected-children-changed",
		G_CALLBACK (addressbook_view_emit_selection_change), view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		card_box, "count-changed",
		G_CALLBACK (addressbook_view_emit_selection_change), view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		card_box, "count-changed",
		G_CALLBACK (addressbook_view_update_folder_bar_message), view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		card_box, "card-event",
		G_CALLBACK (addressbook_view_card_event_cb), view, 0);

	g_signal_connect_object (
		card_box, "card-popup-menu",
		G_CALLBACK (addressbook_view_card_popup_menu_cb), view, 0);

	g_signal_connect_object (
		card_view, "status-message",
		G_CALLBACK (card_view_status_message_cb), view, 0);

	g_signal_connect_object (
		card_view, "double-click",
		G_CALLBACK (card_view_double_click_cb), view, 0);

	g_signal_connect_object (
		card_view, "popup-menu",
		G_CALLBACK (addressbook_view_popup_menu_cb), view, 0);

	view->priv->object = G_OBJECT (card_view);

	gtk_container_add (GTK_CONTAINER (view), card_view);
	gtk_widget_show (card_view);

	gal_view_minicard_attach (gal_view, view);
}

static void
addressbook_view_set_query (EAddressbookView *view,
			    const gchar *query)
{
	if (E_IS_CARD_VIEW (view->priv->object))
		e_card_view_set_query (E_CARD_VIEW (view->priv->object), query);
	else
		e_addressbook_model_set_query (view->priv->model, query);
}

static void
addressbook_view_display_view_cb (GalViewInstance *view_instance,
                                  GalView *gal_view,
                                  EAddressbookView *view)
{
	EShellView *shell_view;
	EBookClient *book_client;
	GtkWidget *child;
	gchar *query;

	query = g_strdup (e_addressbook_view_get_search_query (view));
	book_client = e_addressbook_view_get_client (view);
	if (book_client)
		g_object_ref (book_client);

	child = gtk_bin_get_child (GTK_BIN (view));
	if (child != NULL)
		gtk_container_remove (GTK_CONTAINER (view), child);
	view->priv->object = NULL;

	if (GAL_IS_VIEW_ETABLE (gal_view))
		addressbook_view_create_table_view (
			view, GAL_VIEW_ETABLE (gal_view));
	else if (GAL_IS_VIEW_MINICARD (gal_view))
		addressbook_view_create_minicard_view (
			view, GAL_VIEW_MINICARD (gal_view));

	shell_view = e_addressbook_view_get_shell_view (view);
	e_shell_view_set_view_instance (shell_view, view_instance);

	if (book_client) {
		e_addressbook_view_set_client (view, book_client);
		addressbook_view_set_query (view, query);
	}

	command_state_change (view);

	g_clear_object (&book_client);
	g_free (query);
}

static void
add_to_list (gint model_row,
             gpointer closure)
{
	GSList **list = closure;
	*list = g_slist_prepend (*list, GINT_TO_POINTER (model_row));
}

static void
addressbook_view_model_before_search_cb (EAddressbookModel *model,
					 gpointer user_data)
{
	EAddressbookView *view = user_data;
	ESelectionModel *selection_model;
	GSList *link;
	gint cursor_row;

	selection_model = e_addressbook_view_get_selection_model (view);

	g_slist_free_full (view->priv->previous_selection, g_object_unref);
	view->priv->previous_selection = NULL;

	e_selection_model_foreach (selection_model, add_to_list, &view->priv->previous_selection);
	for (link = view->priv->previous_selection; link; link = g_slist_next (link)) {
		link->data = e_addressbook_model_get_contact (model, GPOINTER_TO_INT (link->data));
	}
	view->priv->previous_selection = g_slist_reverse (view->priv->previous_selection);

	g_clear_object (&view->priv->cursor_contact);

	cursor_row = e_selection_model_cursor_row (selection_model);

	if (cursor_row >= 0 && cursor_row < e_addressbook_model_contact_count (model))
		view->priv->cursor_contact = g_object_ref (e_addressbook_model_contact_at (model, cursor_row));

	view->priv->cursor_col = e_selection_model_cursor_col (selection_model);
	view->priv->awaiting_search_start = TRUE;
}

static void
addressbook_view_model_search_started_cb (EAddressbookModel *model,
					  gpointer user_data)
{
	EAddressbookView *view = user_data;

	view->priv->awaiting_search_start = FALSE;
}

static void
addressbook_view_model_search_result_cb (EAddressbookModel *model,
					 const GError *error,
					 gpointer user_data)
{
	EAddressbookView *view = user_data;
	ESelectionModel *selection_model;
	EContact *cursor_contact;
	GSList *previous_selection, *link;
	gint row;

	view->priv->awaiting_search_start = FALSE;

	if (!view->priv->previous_selection && !view->priv->cursor_contact)
		return;

	/* This can change selection, which frees the 'previous_selection', thus take
	   ownership of it. */
	previous_selection = view->priv->previous_selection;
	view->priv->previous_selection = NULL;

	cursor_contact = view->priv->cursor_contact;
	view->priv->cursor_contact = NULL;

	selection_model = e_addressbook_view_get_selection_model (view);

	if (cursor_contact) {
		row = e_addressbook_model_find (model, cursor_contact);

		if (row >= 0) {
			e_selection_model_change_cursor (selection_model, row, view->priv->cursor_col);
			e_selection_model_cursor_changed (selection_model, row, view->priv->cursor_col);
		}
	}

	for (link = previous_selection; link; link = g_slist_next (link)) {
		EContact *contact = link->data;

		row = e_addressbook_model_find (model, contact);

		if (row >= 0)
			e_selection_model_change_one_row (selection_model, row, TRUE);
	}

	g_slist_free_full (previous_selection, g_object_unref);
	g_clear_object (&cursor_contact);

	e_selection_model_selection_changed (selection_model);
}

static gboolean
address_book_view_focus_in_cb (EAddressbookView *view,
			       GdkEvent *event)
{
	GtkWidget *child;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), FALSE);

	child = gtk_bin_get_child (GTK_BIN (view));
	if (child)
		gtk_widget_grab_focus (child);

	return child != NULL;
}

static void
addressbook_view_set_shell_view (EAddressbookView *view,
                                 EShellView *shell_view)
{
	g_return_if_fail (view->priv->shell_view == NULL);

	view->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&view->priv->shell_view);
}

static void
addressbook_view_set_source (EAddressbookView *view,
                             ESource *source)
{
	g_return_if_fail (view->priv->source == NULL);

	view->priv->source = g_object_ref (source);
}

static void
addressbook_view_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			addressbook_view_set_shell_view (
				E_ADDRESSBOOK_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			addressbook_view_set_source (
				E_ADDRESSBOOK_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_view_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value,
				e_addressbook_view_get_copy_target_list (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value,
				e_addressbook_view_get_paste_target_list (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value,
				e_addressbook_view_get_shell_view (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_SOURCE:
			g_value_set_object (
				value,
				e_addressbook_view_get_source (
				E_ADDRESSBOOK_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_view_dispose (GObject *object)
{
	EAddressbookView *self = E_ADDRESSBOOK_VIEW (object);

	if (self->priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->shell_view),
			&self->priv->shell_view);
		self->priv->shell_view = NULL;
	}

	if (self->priv->model != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->model, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->model);
	}

	if (self->priv->activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_set_state (self->priv->activity, E_ACTIVITY_COMPLETED);
		g_clear_object (&self->priv->activity);
	}

	g_clear_object (&self->priv->source);
	g_clear_object (&self->priv->view_instance);

	self->priv->filter_id = 0;
	self->priv->search_id = 0;

	g_clear_pointer (&self->priv->search_text, g_free);
	g_clear_object (&self->priv->advanced_search);
	g_clear_pointer (&self->priv->copy_target_list, gtk_target_list_unref);
	g_clear_pointer (&self->priv->paste_target_list, gtk_target_list_unref);

	g_slist_free_full (self->priv->previous_selection, g_object_unref);
	self->priv->previous_selection = NULL;

	g_clear_object (&self->priv->cursor_contact);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_addressbook_view_parent_class)->dispose (object);
}

static void
addressbook_view_constructed (GObject *object)
{
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (object);
	GalViewInstance *view_instance;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EClientCache *client_cache;
	ESource *source;
	const gchar *uid;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);
	client_cache = e_shell_get_client_cache (shell);

	source = e_addressbook_view_get_source (view);
	uid = e_source_get_uid (source);

	view->priv->model = e_addressbook_model_new (client_cache);

	g_signal_connect_object (view->priv->model, "before-search",
		G_CALLBACK (addressbook_view_model_before_search_cb), view, 0);

	g_signal_connect_object (view->priv->model, "search-started",
		G_CALLBACK (addressbook_view_model_search_started_cb), view, 0);

	g_signal_connect_object (view->priv->model, "search-result",
		G_CALLBACK (addressbook_view_model_search_result_cb), view, 0);

	view_instance = e_shell_view_new_view_instance (shell_view, uid);
	g_signal_connect (
		view_instance, "display-view",
		G_CALLBACK (addressbook_view_display_view_cb), view);
	view->priv->view_instance = view_instance;

	/* Do not call gal_view_instance_load() here.  EBookShellContent
	 * must first obtain a reference to this EAddressbookView so that
	 * e_book_shell_content_get_current_view() returns the correct
	 * view in GalViewInstance::loaded signal handlers. */

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_addressbook_view_parent_class)->constructed (object);

	g_signal_connect (object, "focus-in-event", G_CALLBACK (address_book_view_focus_in_cb), NULL);
}

static void
addressbook_view_update_actions (ESelectable *selectable,
                                 EFocusTracker *focus_tracker,
                                 GdkAtom *clipboard_targets,
                                 gint n_clipboard_targets)
{
	EAddressbookView *view;
	EUIAction *action;
	GtkTargetList *target_list;
	gboolean can_paste = FALSE;
	gboolean source_is_editable;
	gboolean sensitive;
	const gchar *tooltip;
	guint n_contacts;
	guint n_selected;
	gint ii;

	view = E_ADDRESSBOOK_VIEW (selectable);

	source_is_editable = e_addressbook_view_get_editable (view);
	n_contacts = e_addressbook_view_get_n_total (view);
	n_selected = e_addressbook_view_get_n_selected (view);

	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++)
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = source_is_editable && (n_selected > 0);
	tooltip = _("Cut selected contacts to the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0);
	tooltip = _("Copy selected contacts to the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = source_is_editable && can_paste;
	tooltip = _("Paste contacts from the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	sensitive = source_is_editable && (n_selected > 0);
	tooltip = _("Delete selected contacts");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	sensitive = (n_contacts > 0);
	tooltip = _("Select all visible contacts");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);
}

static void
addressbook_view_fill_clipboard_run (EAddressbookView *view,
				     GPtrArray *contacts,
				     gboolean is_cut)
{
	GtkClipboard *clipboard;
	gchar *str;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	str = eab_contact_array_to_string (contacts);
	e_clipboard_set_directory (clipboard, str, -1);
	g_free (str);

	if (is_cut)
		e_addressbook_view_delete_selection (view, FALSE);
}

static void
addressbook_view_fill_clipboard_got_selected_cb (GObject *source_object,
						 GAsyncResult *result,
						 gpointer user_data)
{
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (source_object);
	gboolean is_cut = GPOINTER_TO_INT (user_data) != 0;
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_addressbook_view_dup_selected_contacts_finish (view, result, &error);
	if (!contacts) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Faield to get selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else {
		addressbook_view_fill_clipboard_run (view, contacts, is_cut);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);
}

static void
addressbook_view_fill_clipboard (EAddressbookView *view,
				 gboolean is_cut)
{
	GPtrArray *contacts;

	contacts = e_addressbook_view_peek_selected_contacts (view);
	if (!contacts) {
		e_addressbook_view_dup_selected_contacts (view, NULL, addressbook_view_fill_clipboard_got_selected_cb, GINT_TO_POINTER (is_cut ? 1 : 0));
		return;
	}

	addressbook_view_fill_clipboard_run (view, contacts, is_cut);

	g_clear_pointer (&contacts, g_ptr_array_unref);
}

static void
addressbook_view_cut_clipboard (ESelectable *selectable)
{
	addressbook_view_fill_clipboard (E_ADDRESSBOOK_VIEW (selectable), TRUE);
}

static void
addressbook_view_copy_clipboard (ESelectable *selectable)
{
	addressbook_view_fill_clipboard (E_ADDRESSBOOK_VIEW (selectable), FALSE);
}

static void
addressbook_view_paste_clipboard (ESelectable *selectable)
{
	EBookClient *book_client;
	EAddressbookView *view;
	ESourceRegistry *registry;
	GtkClipboard *clipboard;
	GSList *contact_list, *iter;
	gchar *string;

	view = E_ADDRESSBOOK_VIEW (selectable);
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	if (!e_clipboard_wait_is_directory_available (clipboard))
		return;

	book_client = e_addressbook_view_get_client (view);

	string = e_clipboard_wait_for_directory (clipboard);
	contact_list = eab_contact_list_from_string (string);
	g_free (string);

	registry = e_shell_get_registry (e_shell_backend_get_shell (e_shell_view_get_shell_backend (e_addressbook_view_get_shell_view (view))));

	for (iter = contact_list; iter != NULL; iter = iter->next) {
		EContact *contact = iter->data;

		eab_merging_book_add_contact (
			registry, book_client, contact, NULL, NULL, TRUE);
	}

	g_object_unref (registry);

	g_slist_free_full (contact_list, (GDestroyNotify) g_object_unref);
}

static void
addressbook_view_delete_selection (ESelectable *selectable)
{
	EAddressbookView *view;

	view = E_ADDRESSBOOK_VIEW (selectable);

	e_addressbook_view_delete_selection (view, TRUE);
}

static void
addressbook_view_select_all (ESelectable *selectable)
{
	EAddressbookView *view;
	ESelectionModel *selection_model;

	view = E_ADDRESSBOOK_VIEW (selectable);

	if (E_IS_CARD_VIEW (view->priv->object)) {
		e_contact_card_box_set_selected_all (e_card_view_get_card_box (E_CARD_VIEW (view->priv->object)), TRUE);
		return;
	}

	selection_model = e_addressbook_view_get_selection_model (view);

	if (selection_model != NULL)
		e_selection_model_select_all (selection_model);
}

static void
e_addressbook_view_class_init (EAddressbookViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = addressbook_view_set_property;
	object_class->get_property = addressbook_view_get_property;
	object_class->dispose = addressbook_view_dispose;
	object_class->constructed = addressbook_view_constructed;

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			"Shell View",
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[OPEN_CONTACT] = g_signal_new (
		"open-contact",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, open_contact),
		NULL, NULL,
		e_marshal_VOID__OBJECT_BOOLEAN,
		G_TYPE_NONE, 2,
		E_TYPE_CONTACT,
		G_TYPE_BOOLEAN);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[COMMAND_STATE_CHANGE] = g_signal_new (
		"command-state-change",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, command_state_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SELECTION_CHANGE] = g_signal_new (
		"selection-change",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, selection_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, status_message),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_INT);

	/* init the accessibility support for e_addressbook_view */
	eab_view_a11y_init ();
}

static void
e_addressbook_view_init (EAddressbookView *view)
{
	GtkTargetList *target_list;

	view->priv = e_addressbook_view_get_instance_private (view);

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_directory_targets (target_list, 0);
	view->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_directory_targets (target_list, 0);
	view->priv->paste_target_list = target_list;

	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (view),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
}

static void
e_addressbook_view_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = addressbook_view_update_actions;
	iface->cut_clipboard = addressbook_view_cut_clipboard;
	iface->copy_clipboard = addressbook_view_copy_clipboard;
	iface->paste_clipboard = addressbook_view_paste_clipboard;
	iface->delete_selection = addressbook_view_delete_selection;
	iface->select_all = addressbook_view_select_all;
}

static void
update_empty_message (EAddressbookView *view)
{
	GtkWidget *widget;

	widget = gtk_bin_get_child (GTK_BIN (view));

	if (E_IS_TABLE (widget)) {
		const gchar *msg = NULL;

		if (view->priv->model && e_addressbook_model_can_stop (view->priv->model) &&
		    !e_addressbook_model_contact_count (view->priv->model))
			msg = _("Searching for the Contacts…");

		e_table_set_info_message (E_TABLE (widget), msg);
	}
}

static void
model_status_message_cb (EAddressbookModel *model,
			 const gchar *message,
			 gint percent,
			 gpointer user_data)
{
	EAddressbookView *view = user_data;

	g_signal_emit (view, signals[STATUS_MESSAGE], 0, message, percent);
}

GtkWidget *
e_addressbook_view_new (EShellView *shell_view,
                        ESource *source)
{
	GtkWidget *widget;
	EAddressbookView *view;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	widget = g_object_new (
		E_TYPE_ADDRESSBOOK_VIEW, "shell-view",
		shell_view, "source", source, NULL);

	view = E_ADDRESSBOOK_VIEW (widget);

	g_signal_connect_swapped (
		view->priv->model, "search_result",
		G_CALLBACK (search_result), view);
	g_signal_connect_swapped (
		view->priv->model, "count-changed",
		G_CALLBACK (addressbook_view_update_folder_bar_message), view);
	g_signal_connect (
		view->priv->model, "stop_state_changed",
		G_CALLBACK (stop_state_changed), view);
	g_signal_connect_swapped (
		view->priv->model, "writable-status",
		G_CALLBACK (command_state_change), view);
	g_signal_connect_object (
		view->priv->model, "contact-added",
		G_CALLBACK (update_empty_message), view, G_CONNECT_SWAPPED | G_CONNECT_AFTER);
	g_signal_connect_object (
		view->priv->model, "contacts-removed",
		G_CALLBACK (update_empty_message), view, G_CONNECT_SWAPPED | G_CONNECT_AFTER);
	g_signal_connect_object (
		view->priv->model, "status-message",
		G_CALLBACK (model_status_message_cb), view, 0);

	return widget;
}

EBookClient *
e_addressbook_view_get_client (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	if (E_IS_CARD_VIEW (view->priv->object))
		return e_card_view_get_book_client (E_CARD_VIEW (view->priv->object));

	return e_addressbook_model_get_client (view->priv->model);
}

void
e_addressbook_view_set_client (EAddressbookView *view,
			       EBookClient *book_client)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	if (E_IS_CARD_VIEW (view->priv->object)) {
		e_card_view_set_book_client (E_CARD_VIEW (view->priv->object), book_client);
		e_addressbook_model_set_client (view->priv->model, NULL);
	} else {
		e_addressbook_model_set_client (view->priv->model, book_client);
	}

	addressbook_view_update_folder_bar_message (view);
}

gboolean
e_addressbook_view_get_editable (EAddressbookView *view)
{
	EBookClient *book;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), FALSE);

	book = e_addressbook_view_get_client (view);

	return book && !e_client_is_readonly (E_CLIENT (book));
}

void
e_addressbook_view_force_folder_bar_message (EAddressbookView *view)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	addressbook_view_update_folder_bar_message (view);
}

typedef struct _PeekSelectedContactsData {
	EAddressbookModel *model;
	GPtrArray *contacts;
} PeekSelectedContactsData;

static void
addressbook_view_add_to_array_cb (gint row,
				  gpointer user_data)
{
	PeekSelectedContactsData *pscd = user_data;
	EContact *contact;

	g_return_if_fail (pscd != NULL);

	contact = e_addressbook_model_get_contact (pscd->model, row);
	if (contact)
		g_ptr_array_add (pscd->contacts, contact);
}

GPtrArray * /* (transfer container) */
e_addressbook_view_peek_selected_contacts (EAddressbookView *view)
{
	GPtrArray *contacts;
	PeekSelectedContactsData pscd;
	guint n_selected;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	n_selected = e_addressbook_view_get_n_selected (view);

	if (!n_selected)
		return g_ptr_array_new_with_free_func (g_object_unref);

	if (E_IS_CARD_VIEW (view->priv->object)) {
		EContactCardBox *box;
		GPtrArray *indexes;

		box = e_card_view_get_card_box (E_CARD_VIEW (view->priv->object));
		indexes = e_contact_card_box_dup_selected_indexes (box);
		if (indexes) {
			contacts = e_contact_card_box_peek_contacts (box, indexes);
			g_ptr_array_unref (indexes);
		} else {
			contacts = g_ptr_array_new_with_free_func (g_object_unref);
		}
	} else {
		contacts = g_ptr_array_new_full (n_selected, g_object_unref);

		pscd.model = view->priv->model;
		pscd.contacts = contacts;

		e_selection_model_foreach (e_addressbook_view_get_selection_model (view), addressbook_view_add_to_array_cb, &pscd);
	}

	return contacts;
}

static void
addressbook_view_got_selected_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	GTask *task = user_data;
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_contact_card_box_dup_contacts_finish (E_CONTACT_CARD_BOX (source_object), result, &error);

	if (contacts)
		g_task_return_pointer (task, contacts, (GDestroyNotify) g_ptr_array_unref);
	else if (error)
		g_task_return_error (task, error);
	else
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", _("Failed to get contacts with unknown error"));

	g_object_unref (task);
}

void
e_addressbook_view_dup_selected_contacts (EAddressbookView *view,
					  GCancellable *cancellable,
					  GAsyncReadyCallback cb,
					  gpointer user_data)
{
	EContactCardBox *box;
	GTask *task;
	GPtrArray *array;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	task = g_task_new (view, cancellable, cb, user_data);
	g_task_set_source_tag (task, e_addressbook_view_dup_selected_contacts);

	array = e_addressbook_view_peek_selected_contacts (view);

	if (array) {
		g_task_return_pointer (task, array, (GDestroyNotify) g_ptr_array_unref);
		g_object_unref (task);
		return;
	}

	if (!view->priv->object ||
	    !E_IS_CARD_VIEW (view->priv->object)) {
		if (view->priv->object) {
			/* Should not get here with the table view */
			g_warn_if_reached ();
		}

		g_task_return_pointer (task, g_ptr_array_new_with_free_func (g_object_unref), (GDestroyNotify) g_ptr_array_unref);
		g_object_unref (task);
		return;
	}

	box = e_card_view_get_card_box (E_CARD_VIEW (view->priv->object));
	array = e_contact_card_box_dup_selected_indexes (box);

	if (!array || array->len == 0) {
		g_task_return_pointer (task, g_ptr_array_new_with_free_func (g_object_unref), (GDestroyNotify) g_ptr_array_unref);
		g_object_unref (task);
	} else {
		e_contact_card_box_dup_contacts	(box, array, cancellable, addressbook_view_got_selected_cb, task);
	}

	g_clear_pointer (&array, g_ptr_array_unref);
}

GPtrArray * /* (transfer container) */
e_addressbook_view_dup_selected_contacts_finish (EAddressbookView *view,
						 GAsyncResult *result,
						 GError **error)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);
	g_return_val_if_fail (g_task_is_valid (result, view), NULL);
	g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == e_addressbook_view_dup_selected_contacts, NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

guint
e_addressbook_view_get_n_total (EAddressbookView *view)
{
	ESelectionModel *selection_model;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), 0);

	if (E_IS_CARD_VIEW (view->priv->object))
		return e_contact_card_box_get_n_items (e_card_view_get_card_box (E_CARD_VIEW (view->priv->object)));

	selection_model = e_addressbook_view_get_selection_model (view);

	return selection_model ? e_selection_model_row_count (selection_model) : 0;
}

guint
e_addressbook_view_get_n_selected (EAddressbookView *view)
{
	ESelectionModel *selection_model;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), 0);

	if (E_IS_CARD_VIEW (view->priv->object))
		return e_contact_card_box_get_n_selected (e_card_view_get_card_box (E_CARD_VIEW (view->priv->object)));

	selection_model = e_addressbook_view_get_selection_model (view);

	return selection_model ? e_selection_model_selected_count (selection_model) : 0;
}

GalViewInstance *
e_addressbook_view_get_view_instance (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->view_instance;
}

GObject *
e_addressbook_view_get_content_object (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->object;
}

EShellView *
e_addressbook_view_get_shell_view (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->shell_view;
}

ESource *
e_addressbook_view_get_source (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->source;
}

GtkTargetList *
e_addressbook_view_get_copy_target_list (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->copy_target_list;
}

GtkTargetList *
e_addressbook_view_get_paste_target_list (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->paste_target_list;
}

static void
search_result (EAddressbookView *view,
               const GError *error)
{
	EShellView *shell_view;
	EAlertSink *alert_sink;

	shell_view = e_addressbook_view_get_shell_view (view);
	alert_sink = E_ALERT_SINK (e_shell_view_get_shell_content (shell_view));

	eab_search_result_dialog (alert_sink, error);
}

static void
stop_state_changed (GObject *object,
                    EAddressbookView *view)
{
	command_state_change (view);
}

static void
command_state_change (EAddressbookView *view)
{
	g_signal_emit (view, signals[COMMAND_STATE_CHANGE], 0);

	update_empty_message (view);
}

static void
contact_print_button_draw_page (GtkPrintOperation *operation,
                                GtkPrintContext *context,
                                gint page_nr,
                                EPrintable *printable)
{
	GtkPageSetup *setup;
	gdouble top_margin, page_width;
	cairo_t *cr;

	setup = gtk_print_context_get_page_setup (context);
	top_margin = gtk_page_setup_get_top_margin (setup, GTK_UNIT_POINTS);
	page_width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);

	cr = gtk_print_context_get_cairo_context (context);

	e_printable_reset (printable);

	while (e_printable_data_left (printable)) {
		cairo_save (cr);
		contact_page_draw_footer (operation,context,page_nr++);
		e_printable_print_page (
			printable, context, page_width - 16, top_margin + 10, TRUE);
		cairo_restore (cr);
	}
}

static void
e_contact_print_button (EPrintable *printable,
                        GtkPrintOperationAction action)
{
	GtkPrintOperation *operation;

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect (
		operation, "draw_page",
		G_CALLBACK (contact_print_button_draw_page), printable);

	gtk_print_operation_run (operation, action, NULL, NULL);

	g_object_unref (operation);
}

static void
addressbook_view_print_got_selection_cb (GObject *source_object,
					 GAsyncResult *result,
					 gpointer user_data)
{
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (source_object);
	GtkPrintOperationAction action = GPOINTER_TO_INT (user_data);
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_addressbook_view_dup_selected_contacts_finish (view, result, &error);
	if (!contacts) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Faield to get selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else {
		e_contact_print (NULL, NULL, contacts, action);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);
}

void
e_addressbook_view_print (EAddressbookView *view,
                          gboolean selection_only,
                          GtkPrintOperationAction action)
{
	GalView *gal_view;
	GalViewInstance *view_instance;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	/* Print the selected contacts. */
	if (GAL_IS_VIEW_MINICARD (gal_view) && selection_only) {
		GPtrArray *contacts;

		contacts = e_addressbook_view_peek_selected_contacts (view);
		if (contacts) {
			e_contact_print (NULL, NULL, contacts, action);
			g_ptr_array_unref (contacts);
		} else {
			e_addressbook_view_dup_selected_contacts (view, NULL,
				addressbook_view_print_got_selection_cb, GINT_TO_POINTER (action));
		}

	/* Print the latest query results. */
	} else if (GAL_IS_VIEW_MINICARD (gal_view)) {
		EBookClient *book_client;
		EBookQuery *query;
		const gchar *query_string;

		book_client = e_addressbook_view_get_client (view);
		query_string = e_addressbook_view_get_search_query (view);

		if (query_string != NULL)
			query = e_book_query_from_string (query_string);
		else
			query = NULL;

		e_contact_print (book_client, query, NULL, action);

		if (query != NULL)
			e_book_query_unref (query);

	/* XXX Does this print the entire table or just selected? */
	} else if (GAL_IS_VIEW_ETABLE (gal_view)) {
		EPrintable *printable;
		GtkWidget *widget;

		widget = gtk_bin_get_child (GTK_BIN (view));
		printable = e_table_get_printable (E_TABLE (widget));
		g_object_ref_sink (printable);

		e_contact_print_button (printable, action);

		g_object_unref (printable);
	}
}

static void
report_and_free_error_if_any (GError *error)
{
	if (!error)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;
	}

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_PERMISSION_DENIED)) {
		e_alert_run_dialog_for_args (
			e_shell_get_active_window (NULL),
			"addressbook:contact-delete-error-perm", NULL);
	} else {
		eab_error_dialog (NULL, NULL, _("Failed to delete contact"), error);
	}

	g_error_free (error);
}

/* callback function to handle removal of contacts for
 * which a user doesn't have write permission
 */
static void
remove_contacts_cb (GObject *source_object,
                    GAsyncResult *result,
                    gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	GError *error = NULL;

	e_book_client_remove_contacts_finish (book_client, result, &error);

	report_and_free_error_if_any (error);
}

static void
remove_contact_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	GError *error = NULL;

	e_book_client_remove_contact_finish (book_client, result, &error);

	report_and_free_error_if_any (error);
}

static gboolean
addressbook_view_confirm_delete (GtkWindow *parent,
                                 gboolean plural,
                                 gboolean is_list,
                                 const gchar *name)
{
	GtkWidget *dialog;
	gchar *message;
	gint response;

	if (is_list) {
		if (plural) {
			message = g_strdup (
				_("Are you sure you want to "
				"delete these contact lists?"));
		} else if (name == NULL) {
			message = g_strdup (
				_("Are you sure you want to "
				"delete this contact list?"));
		} else {
			message = g_strdup_printf (
				_("Are you sure you want to delete "
				"this contact list (%s)?"), name);
		}
	} else {
		if (plural) {
			message = g_strdup (
				_("Are you sure you want to "
				"delete these contacts?"));
		} else if (name == NULL) {
			message = g_strdup (
				_("Are you sure you want to "
				"delete this contact?"));
		} else {
			message = g_strdup_printf (
				_("Are you sure you want to delete "
				"this contact (%s)?"), name);
		}
	}

	dialog = gtk_message_dialog_new (
		parent, 0, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE, "%s", message);
	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Delete"), GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (message);

	return (response == GTK_RESPONSE_ACCEPT);
}

static void
e_addressbook_view_delete_selection_run (EAddressbookView *view,
					 gboolean is_delete,
					 GPtrArray *contacts)
{
	gboolean plural = FALSE, is_list = FALSE;
	EContact *contact;
	ETable *etable = NULL;
	EBookClient *book_client;
	EContactCardBox *card_box = NULL;
	GalViewInstance *view_instance;
	GalView *gal_view;
	GtkWidget *widget;
	gchar *name = NULL;
	gint row = 0, select;
	guint ii;

	if (!contacts || !contacts->len)
		return;

	book_client = e_addressbook_view_get_client (view);

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	contact = g_ptr_array_index (contacts, 0);

	if (contacts->len > 1)
		plural = TRUE;
	else
		name = e_contact_get (contact, E_CONTACT_FILE_AS);

	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		is_list = TRUE;

	widget = gtk_bin_get_child (GTK_BIN (view));

	if (GAL_IS_VIEW_MINICARD (gal_view)) {
		card_box = e_card_view_get_card_box (E_CARD_VIEW (view->priv->object));
		row = e_contact_card_box_get_focused_index (card_box);
	} else if (GAL_IS_VIEW_ETABLE (gal_view)) {
		etable = E_TABLE (widget);
		row = e_table_get_cursor_row (E_TABLE (etable));
	}

	/* confirm delete */
	if (is_delete && !addressbook_view_confirm_delete (
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
			plural, is_list, name)) {
		g_free (name);
		return;
	}

	if (e_client_check_capability (E_CLIENT (book_client), "bulk-remove")) {
		GSList *ids = NULL;

		for (ii = 0; ii < contacts->len; ii++) {
			const gchar *uid;

			contact = g_ptr_array_index (contacts, ii);

			uid = e_contact_get_const (contact, E_CONTACT_UID);
			ids = g_slist_prepend (ids, (gpointer) uid);
		}

		/* Remove the cards all at once. */
		e_book_client_remove_contacts (
			book_client, ids, E_BOOK_OPERATION_FLAG_NONE, NULL, remove_contacts_cb, NULL);

		g_slist_free (ids);
	} else {
		for (ii = 0; ii < contacts->len; ii++) {
			contact = g_ptr_array_index (contacts, ii);

			/* Remove the card. */
			e_book_client_remove_contact (
				book_client, contact, E_BOOK_OPERATION_FLAG_NONE, NULL,
				remove_contact_cb, NULL);
		}
	}

	/* Sets the cursor, at the row after the deleted row */
	if (card_box && row != 0) {
		guint n_items = e_contact_card_box_get_n_items (card_box);
		if (n_items > 0) {
			if (row >= n_items)
				row = n_items - 1;
			e_contact_card_box_set_focused_index (card_box, row);
			e_contact_card_box_set_selected_all (card_box, FALSE);
			e_contact_card_box_set_selected (card_box, row, TRUE);
		}
	}

	/* Sets the cursor, at the row after the deleted row */
	else if (GAL_IS_VIEW_ETABLE (gal_view) && row != 0) {
		select = e_table_model_to_view_row (E_TABLE (etable), row);

		/* Sets the cursor, before the deleted row if its the last row */
		if (select == e_table_model_row_count (E_TABLE (etable)->model) - 1)
			select = select - 1;
		else
			select = select + 1;

		row = e_table_view_to_model_row (E_TABLE (etable), select);
		e_table_set_cursor_row (E_TABLE (etable), row);
	}

	g_free (name);
}

static void
addressbook_view_delete_selection_got_selected_cb (GObject *source_object,
						   GAsyncResult *result,
						   gpointer user_data)
{
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (source_object);
	gboolean is_delete = GPOINTER_TO_INT (user_data) != 0;
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_addressbook_view_dup_selected_contacts_finish (view, result, &error);
	if (!contacts) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Faield to get selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else {
		e_addressbook_view_delete_selection_run (view, is_delete, contacts);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);
}

static void
e_addressbook_view_delete_selection (EAddressbookView *view,
				     gboolean is_delete)
{
	GPtrArray *contacts;

	contacts = e_addressbook_view_peek_selected_contacts (view);

	if (!contacts) {
		e_addressbook_view_dup_selected_contacts (view, NULL, addressbook_view_delete_selection_got_selected_cb,
			GINT_TO_POINTER (is_delete ? 1 : 0));
		return;
	}

	e_addressbook_view_delete_selection_run (view, is_delete, contacts);

	g_ptr_array_unref (contacts);
}

static void
addressbook_view_view_run (EAddressbookView *view,
			   GPtrArray *contacts)
{
	gint response;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (contacts != NULL);

	response = GTK_RESPONSE_YES;

	if (contacts->len > 5) {
		GtkWidget *dialog;

		/* XXX Use e_alert_new(). */
		/* XXX Provide a parent window. */
		dialog = gtk_message_dialog_new (
			NULL, 0,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, ngettext (
			/* Translators: This is shown for > 5 contacts. */
			"Opening %d contacts will open %d new windows "
			"as well.\nDo you really want to display all of "
			"these contacts?",
			"Opening %d contacts will open %d new windows "
			"as well.\nDo you really want to display all of "
			"these contacts?", contacts->len), contacts->len, contacts->len);
		gtk_dialog_add_buttons (
			GTK_DIALOG (dialog),
			_("_Don’t Display"), GTK_RESPONSE_NO,
			_("Display _All Contacts"), GTK_RESPONSE_YES,
			NULL);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	if (response == GTK_RESPONSE_YES) {
		guint ii;

		for (ii = 0; ii < contacts->len; ii++) {
			EContact *contact = g_ptr_array_index (contacts, ii);

			addressbook_view_emit_open_contact (view, contact, FALSE);
		}
	}
}

static void
addressbook_view_view_got_selected_cb (GObject *source_object,
				       GAsyncResult *result,
				       gpointer user_data)
{
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (source_object);
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_addressbook_view_dup_selected_contacts_finish (view, result, &error);
	if (!contacts) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Faield to get selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else {
		addressbook_view_view_run (view, contacts);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);
}

void
e_addressbook_view_view (EAddressbookView *view)
{
	GPtrArray *contacts;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	contacts = e_addressbook_view_peek_selected_contacts (view);

	if (!contacts) {
		e_addressbook_view_dup_selected_contacts (view, NULL, addressbook_view_view_got_selected_cb, NULL);
		return;
	}

	addressbook_view_view_run (view, contacts);

	g_ptr_array_unref (contacts);
}

gboolean
e_addressbook_view_can_stop (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), FALSE);

	return !E_IS_CARD_VIEW (view->priv->object) && e_addressbook_model_can_stop (view->priv->model);
}

void
e_addressbook_view_stop (EAddressbookView *view)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	e_addressbook_model_stop (view->priv->model);
}

struct TransferContactsData
{
	gboolean delete_from_source;
	EAddressbookView *view;
};

static void
all_contacts_ready_cb (GObject *source_object,
                       GAsyncResult *result,
                       gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	struct TransferContactsData *tcd = user_data;
	EShellView *shell_view;
	EShellContent *shell_content;
	EAlertSink *alert_sink;
	GSList *contacts = NULL;
	GError *error = NULL;

	g_return_if_fail (book_client != NULL);
	g_return_if_fail (tcd != NULL);

	e_book_client_get_contacts_finish (
		book_client, result, &contacts, &error);

	shell_view = e_addressbook_view_get_shell_view (tcd->view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	alert_sink = E_ALERT_SINK (shell_content);

	if (error != NULL) {
		e_alert_submit (
			alert_sink, "addressbook:search-error",
			error->message, NULL);
		g_error_free (error);

	} else if (contacts != NULL) {
		ESourceRegistry *registry;

		registry = e_shell_get_registry (e_shell_backend_get_shell (e_shell_view_get_shell_backend (shell_view)));

		eab_transfer_contacts (
			registry, book_client, contacts,
			tcd->delete_from_source, alert_sink);
	}

	g_object_unref (tcd->view);
	g_slice_free (struct TransferContactsData, tcd);
}

static void
view_transfer_contacts_run (EAddressbookView *view,
			    gboolean delete_from_source,
			    GPtrArray *contacts)
{
	GSList *contacts_list = NULL;
	EShellView *shell_view;
	EShellContent *shell_content;
	EAlertSink *alert_sink;
	ESourceRegistry *registry;
	guint ii;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	alert_sink = E_ALERT_SINK (shell_content);

	registry = e_shell_get_registry (e_shell_backend_get_shell (e_shell_view_get_shell_backend (shell_view)));

	for (ii = 0; contacts && ii < contacts->len; ii++) {
		EContact *contact = g_ptr_array_index (contacts, contacts->len - ii - 1);

		contacts_list = g_slist_prepend (contacts_list, g_object_ref (contact));
	}

	eab_transfer_contacts (
		registry, e_addressbook_view_get_client (view), contacts_list,
		delete_from_source, alert_sink);
}

static void
view_transfer_contacts_got_selected_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	gboolean delete_from_source = GPOINTER_TO_INT (user_data) != 0;
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (source_object);
	GPtrArray *contacts;
	GError *error = NULL;

	contacts = e_addressbook_view_dup_selected_contacts_finish (view, result, &error);
	if (!contacts) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Faield to get selected contacts: %s", G_STRFUNC, error ? error->message : "Unknown error");
	} else {
		view_transfer_contacts_run (view, delete_from_source, contacts);
	}

	g_clear_pointer (&contacts, g_ptr_array_unref);
	g_clear_error (&error);
}

static void
view_transfer_contacts (EAddressbookView *view,
                        gboolean delete_from_source,
                        gboolean all)
{
	EBookClient *book_client;

	book_client = e_addressbook_view_get_client (view);

	if (all) {
		EBookQuery *query;
		gchar *query_str;
		struct TransferContactsData *tcd;

		query = e_book_query_any_field_contains ("");
		query_str = e_book_query_to_string (query);
		e_book_query_unref (query);

		tcd = g_slice_new0 (struct TransferContactsData);
		tcd->delete_from_source = delete_from_source;
		tcd->view = g_object_ref (view);

		e_book_client_get_contacts (
			book_client, query_str, NULL,
			all_contacts_ready_cb, tcd);
	} else {
		GPtrArray *contacts;

		contacts = e_addressbook_view_peek_selected_contacts (view);

		if (!contacts) {
			e_addressbook_view_dup_selected_contacts (view, NULL, view_transfer_contacts_got_selected_cb,
				GINT_TO_POINTER (delete_from_source ? 1 : 0));
			return;
		}

		view_transfer_contacts_run (view, delete_from_source, contacts);

		g_ptr_array_unref (contacts);
	}
}

void
e_addressbook_view_copy_to_folder (EAddressbookView *view,
                                   gboolean all)
{
	view_transfer_contacts (view, FALSE, all);
}

void
e_addressbook_view_move_to_folder (EAddressbookView *view,
                                   gboolean all)
{
	view_transfer_contacts (view, TRUE, all);
}

void
e_addressbook_view_set_search (EAddressbookView *view,
			       const gchar *query,
                               gint filter_id,
                               gint search_id,
                               const gchar *search_text,
                               EFilterRule *advanced_search)
{
	EAddressbookViewPrivate *priv;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	priv = view->priv;
	g_free (priv->search_text);
	if (priv->advanced_search)
		g_object_unref (priv->advanced_search);

	priv->filter_id = filter_id;
	priv->search_id = search_id;
	priv->search_text = g_strdup (search_text);

	if (advanced_search != NULL)
		priv->advanced_search = e_filter_rule_clone (advanced_search);
	else
		priv->advanced_search = NULL;

	addressbook_view_set_query (view, query);
}

const gchar *
e_addressbook_view_get_search_query (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	if (E_IS_CARD_VIEW (view->priv->object))
		return e_card_view_get_query (E_CARD_VIEW (view->priv->object));

	return e_addressbook_model_get_query (view->priv->model);
}

/* Free returned values for search_text and advanced_search,
 * if not NULL, as these are new copies. */
void
e_addressbook_view_get_search (EAddressbookView *view,
                               gint *filter_id,
                               gint *search_id,
                               gchar **search_text,
                               EFilterRule **advanced_search)
{
	EAddressbookViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (filter_id != NULL);
	g_return_if_fail (search_id != NULL);
	g_return_if_fail (search_text != NULL);
	g_return_if_fail (advanced_search != NULL);

	priv = view->priv;

	*filter_id = priv->filter_id;
	*search_id = priv->search_id;
	*search_text = g_strdup (priv->search_text);

	if (priv->advanced_search != NULL)
		*advanced_search = e_filter_rule_clone (priv->advanced_search);
	else
		*advanced_search = NULL;
}
