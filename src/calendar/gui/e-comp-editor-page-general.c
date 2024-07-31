/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>
#include <camel/camel.h>
#include <e-util/e-util.h>

#include "comp-util.h"
#include "e-comp-editor.h"
#include "e-comp-editor-page.h"
#include "e-comp-editor-property-parts.h"
#include "e-meeting-list-view.h"
#include "itip-utils.h"

#include "e-comp-editor-page-general.h"

#define BACKEND_EMAIL_ID "backend-email-id"

struct _ECompEditorPageGeneralPrivate {
	GtkWidget *source_label;
	GtkWidget *source_combo_box;
	GtkWidget *organizer_label;
	GtkWidget *organizer_combo_box;
	GtkWidget *organizer_hbox;
	GtkWidget *attendees_button;
	GtkWidget *attendees_hbox;
	GtkWidget *attendees_list_view;
	GtkWidget *attendees_button_box;
	GtkWidget *attendees_button_add;
	GtkWidget *attendees_button_edit;
	GtkWidget *attendees_button_remove;
	ECompEditorPropertyPart *comp_color;
	gulong comp_color_changed_handler_id;
	GtkWidget *source_and_color_hbox; /* has together source_combo_box and comp_color::edit_widget */

	gint data_column_width;
	gchar *source_label_text;
	gchar *source_extension_name;
	ESource *select_source;
	gboolean show_attendees;

	EMeetingStore *meeting_store;
	GSList *orig_attendees; /* gchar *mail_addresses */
	gchar *user_delegator;
};

enum {
	PROP_0,
	PROP_DATA_COLUMN_WIDTH,
	PROP_SOURCE_LABEL,
	PROP_SOURCE_EXTENSION_NAME,
	PROP_SELECTED_SOURCE,
	PROP_SHOW_ATTENDEES
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorPageGeneral, e_comp_editor_page_general, E_TYPE_COMP_EDITOR_PAGE)

static void ecep_general_sensitize_widgets (ECompEditorPage *page,
					    gboolean force_insensitive);

static void
ecep_general_set_column_visible (ECompEditorPageGeneral *page_general,
				 EMeetingStoreColumns column,
				 gboolean visible)
{
	EMeetingListView *meeting_list_view;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	meeting_list_view = E_MEETING_LIST_VIEW (page_general->priv->attendees_list_view);
	e_meeting_list_view_column_set_visible (meeting_list_view, column, visible);
}

static void
action_view_role_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	ECompEditorPageGeneral *page_general = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	e_ui_action_set_active (action, !e_ui_action_get_active (action));

	ecep_general_set_column_visible (page_general, E_MEETING_STORE_ROLE_COL,
		e_ui_action_get_active (action));
}

static void
action_view_rsvp_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	ECompEditorPageGeneral *page_general = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	e_ui_action_set_active (action, !e_ui_action_get_active (action));

	ecep_general_set_column_visible (page_general, E_MEETING_STORE_RSVP_COL,
		e_ui_action_get_active (action));
}

static void
action_view_status_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	ECompEditorPageGeneral *page_general = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	e_ui_action_set_active (action, !e_ui_action_get_active (action));

	ecep_general_set_column_visible (page_general, E_MEETING_STORE_STATUS_COL,
		e_ui_action_get_active (action));
}

static void
action_view_type_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	ECompEditorPageGeneral *page_general = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	e_ui_action_set_active (action, !e_ui_action_get_active (action));

	ecep_general_set_column_visible (page_general, E_MEETING_STORE_TYPE_COL,
		e_ui_action_get_active (action));
}

static void
ecep_general_fill_organizer_combo_box (ECompEditorPageGeneral *page_general)
{
	GtkComboBoxText *combo_box_text;
	ECompEditor *comp_editor;
	EShell *shell;
	ESourceRegistry *registry;
	gchar **address_strings;
	gint ii;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (page_general->priv->organizer_combo_box));

	combo_box_text = GTK_COMBO_BOX_TEXT (page_general->priv->organizer_combo_box);
	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));
	shell = e_comp_editor_get_shell (comp_editor);
	registry = e_shell_get_registry (shell);
	address_strings = itip_get_user_identities (registry);

	for (ii = 0; address_strings && address_strings[ii]; ii++) {
		gtk_combo_box_text_append_text (combo_box_text, address_strings[ii]);
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box_text), 0);

	g_strfreev (address_strings);
	g_clear_object (&comp_editor);
}

static void
ecep_general_source_combo_box_changed_cb (ESourceComboBox *source_combo_box,
					  ECompEditorPageGeneral *page_general)
{
	ESource *source;

	g_return_if_fail (E_IS_SOURCE_COMBO_BOX (source_combo_box));
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	source = e_source_combo_box_ref_active (source_combo_box);
	e_comp_editor_page_general_set_selected_source (page_general, source);
	g_clear_object (&source);
}

static void
ecep_general_attendees_clicked_cb (GtkWidget *widget,
				   ECompEditorPageGeneral *page_general)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	e_meeting_list_view_invite_others_dialog (E_MEETING_LIST_VIEW (page_general->priv->attendees_list_view));
}

static void
ecep_general_attendees_add_clicked_cb (GtkButton *button,
				       ECompEditorPageGeneral *page_general)
{
	ECompEditor *comp_editor;
	EMeetingAttendee *attendee;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));
	flags = e_comp_editor_get_flags (comp_editor);

	attendee = e_meeting_store_add_attendee_with_defaults (page_general->priv->meeting_store);

	if ((flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0) {
		gchar *mailto;

		mailto = g_strdup_printf ("mailto:%s",
			page_general->priv->user_delegator ? page_general->priv->user_delegator : "");
		e_meeting_attendee_set_delfrom (attendee, mailto);
		g_free (mailto);
	}

	e_meeting_list_view_edit (E_MEETING_LIST_VIEW (page_general->priv->attendees_list_view), attendee);

	g_clear_object (&comp_editor);
}

static void
ecep_general_attendees_edit_clicked_cb (GtkButton *button,
					ECompEditorPageGeneral *page_general)
{
	GtkTreeView *tree_view;
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_col;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	tree_view = GTK_TREE_VIEW (page_general->priv->attendees_list_view);

	gtk_tree_view_get_cursor (tree_view, &path, NULL);
	g_return_if_fail (path != NULL);

	gtk_tree_view_get_cursor (tree_view, &path, &focus_col);
	gtk_tree_view_set_cursor (tree_view, path, focus_col, TRUE);
	gtk_tree_path_free (path);
}

static void
ecep_general_remove_attendee (ECompEditorPageGeneral *page_general,
			      EMeetingAttendee *attendee)
{
	ECompEditor *comp_editor;
	gint pos = 0;

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));

	/* If this was a delegatee, no longer delegate */
	if (e_meeting_attendee_is_set_delfrom (attendee)) {
		EMeetingAttendee *ib;

		ib = e_meeting_store_find_attendee (page_general->priv->meeting_store, e_meeting_attendee_get_delfrom (attendee), &pos);
		if (ib != NULL) {
			ECompEditorFlags flags;

			e_meeting_attendee_set_delto (ib, NULL);

			flags = e_comp_editor_get_flags (comp_editor);

			if (!(flags & E_COMP_EDITOR_FLAG_DELEGATE))
				e_meeting_attendee_set_edit_level (ib, E_MEETING_ATTENDEE_EDIT_FULL);
		}
	}

	/* Handle deleting all attendees in the delegation chain */
	while (attendee) {
		EMeetingAttendee *ib = NULL;

		if (e_meeting_attendee_get_delto (attendee)) {
			ib = e_meeting_store_find_attendee (page_general->priv->meeting_store,
				e_meeting_attendee_get_delto (attendee), NULL);
		}

		e_meeting_list_view_remove_attendee_from_name_selector (
			E_MEETING_LIST_VIEW (page_general->priv->attendees_list_view), attendee);
		e_meeting_store_remove_attendee (page_general->priv->meeting_store, attendee);

		attendee = ib;
	}

	ecep_general_sensitize_widgets (E_COMP_EDITOR_PAGE (page_general), FALSE);

	e_comp_editor_set_changed (comp_editor, TRUE);
	g_clear_object (&comp_editor);
}

static void
ecep_general_attendees_remove_clicked_cb (GtkButton *button,
					  ECompEditorPageGeneral *page_general)
{
	EMeetingAttendee *attendee;
	GtkTreeSelection *selection;
	GList *paths = NULL, *tmp;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	GtkTreeModel *model = NULL;
	gboolean valid_iter;
	gchar *address;
	gint failures = 0;
	GString *errors = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_general->priv->attendees_list_view));
	paths = gtk_tree_selection_get_selected_rows (selection, &model);
	g_return_if_fail (paths != NULL);

	paths = g_list_reverse (paths);

	for (tmp = paths; tmp; tmp = tmp->next) {
		path = tmp->data;

		gtk_tree_model_get_iter (model, &iter, path);

		gtk_tree_model_get (model, &iter, E_MEETING_STORE_ADDRESS_COL, &address, -1);
		attendee = e_meeting_store_find_attendee (E_MEETING_STORE (model), address, NULL);

		if (!attendee) {
			if (!errors)
				errors = g_string_new ("");
			else
				g_string_append_c (errors, '\n');
			g_string_append_printf (errors, _("Cannot find attendee “%s” in the list of attendees"), address);
			failures++;
		} else if (e_meeting_attendee_get_edit_level (attendee) != E_MEETING_ATTENDEE_EDIT_FULL) {
			if (!errors)
				errors = g_string_new ("");
			else
				g_string_append_c (errors, '\n');
			g_string_append_printf (errors, _("Not enough rights to delete attendee “%s”"), e_cal_util_strip_mailto (e_meeting_attendee_get_address (attendee)));
			failures++;
		} else {
			ecep_general_remove_attendee (page_general, attendee);
		}

		g_free (address);
	}

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (model, &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (model, &iter, path);
	}

	if (valid_iter) {
		gtk_tree_selection_unselect_all (selection);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);

	if (errors) {
		ECompEditor *comp_editor;
		EAlert *alert;

		comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));

		alert = e_comp_editor_add_error (comp_editor, g_dngettext (GETTEXT_PACKAGE,
			"Failed to delete selected attendee",
			"Failed to delete selected attendees",
			failures), errors->str);

		g_string_free (errors, TRUE);
		g_clear_object (&alert);
		g_clear_object (&comp_editor);
	}
}

static void
ecep_general_attendees_selection_changed_cb (GtkTreeSelection *selection,
					     ECompEditorPageGeneral *page_general)
{
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	ecep_general_sensitize_widgets (E_COMP_EDITOR_PAGE (page_general), FALSE);
}

static void
ecep_general_attendee_added_cb (EMeetingListView *meeting_list_view,
				EMeetingAttendee *attendee,
				ECompEditorPageGeneral *page_general)
{
	ECompEditor *comp_editor;
	ECompEditorFlags flags;
	ECalClient *client;
	gchar *mailto;

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));
	flags = e_comp_editor_get_flags (comp_editor);

	e_comp_editor_set_changed (comp_editor, TRUE);

	if (!(flags & E_COMP_EDITOR_FLAG_DELEGATE)) {
		g_clear_object (&comp_editor);
		return;
	}

	client = e_comp_editor_get_target_client (comp_editor);

	/* do not remove here, it did EMeetingListView already */
	mailto = g_strdup_printf ("mailto:%s", page_general->priv->user_delegator ? page_general->priv->user_delegator : "");
	e_meeting_attendee_set_delfrom (attendee, mailto);
	g_free (mailto);

	if (client && !e_client_check_capability (E_CLIENT (client), E_CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY)) {
		EMeetingAttendee *delegator;

		delegator = e_meeting_store_find_attendee (page_general->priv->meeting_store,
			page_general->priv->user_delegator, NULL);
		g_return_if_fail (delegator != NULL);

		e_meeting_attendee_set_delto (delegator, e_meeting_attendee_get_address (attendee));
	}

	ecep_general_sensitize_widgets (E_COMP_EDITOR_PAGE (page_general), FALSE);

	g_clear_object (&comp_editor);
}

static void
ecep_general_attendee_row_changed_cb (GtkTreeModel *model,
				      GtkTreePath *path,
				      GtkTreeIter *iter,
				      ECompEditorPageGeneral *page_general)
{
	ECompEditor *comp_editor;

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));

	if (comp_editor)
		e_comp_editor_set_changed (comp_editor, TRUE);

	g_clear_object (&comp_editor);
}

static void
ecep_general_attendee_show_address_notify_cb (GObject *store,
					      GParamSpec *param,
					      ECompEditorPageGeneral *page_general)
{
	if (gtk_widget_get_realized (GTK_WIDGET (page_general)) &&
	    gtk_widget_get_realized (page_general->priv->attendees_list_view))
		gtk_widget_queue_draw (page_general->priv->attendees_list_view);
}

static gboolean
ecep_general_get_organizer (ECompEditorPageGeneral *page_general,
			    gchar **out_name,
			    gchar **out_mailto,
			    const gchar **out_error_message)
{
	gchar *organizer_text;
	gboolean valid = FALSE;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), FALSE);

	organizer_text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (page_general->priv->organizer_combo_box));
	if (organizer_text) {
		CamelInternetAddress *address;
		const gchar *str_name, *str_address;

		address = camel_internet_address_new ();
		if (camel_address_unformat (CAMEL_ADDRESS (address), organizer_text) == 1 &&
		    camel_internet_address_get (address, 0, &str_name, &str_address)) {
			valid = TRUE;

			if (out_name)
				*out_name = g_strdup (str_name);
			if (out_mailto)
				*out_mailto = g_strconcat ("mailto:", e_cal_util_strip_mailto (str_address), NULL);
		} else if (out_error_message) {
			*out_error_message = _("Organizer address is not a valid user mail address");
		}

		g_object_unref (address);
		g_free (organizer_text);
	}

	return valid;
}

static void
ecep_general_remove_organizer_backend_address (GtkComboBox *combo_box)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (combo_box);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		gint id_column;

		id_column = gtk_combo_box_get_id_column (combo_box);

		do {
			gchar *value = NULL;

			gtk_tree_model_get (model, &iter, id_column, &value, -1);

			if (g_strcmp0 (value, BACKEND_EMAIL_ID) == 0) {
				g_free (value);
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				break;
			}

			g_free (value);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

static gboolean
ecep_general_pick_organizer_for_email_address (ECompEditorPageGeneral *page_general,
					       const gchar *email_address,
					       gboolean can_add)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkComboBox *combo_box;
	gint ii = 0, entry_text_column;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), FALSE);

	combo_box = GTK_COMBO_BOX (page_general->priv->organizer_combo_box);
	model = gtk_combo_box_get_model (combo_box);

	if (can_add)
		ecep_general_remove_organizer_backend_address (combo_box);

	email_address = e_cal_util_strip_mailto (email_address);

	if (!email_address || !*email_address) {
		if (can_add && gtk_combo_box_get_active (combo_box) == -1 &&
		    gtk_tree_model_get_iter_first (model, &iter))
			gtk_combo_box_set_active (combo_box, 0);

		return FALSE;
	}

	entry_text_column = gtk_combo_box_get_entry_text_column (combo_box);

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *value = NULL;

			gtk_tree_model_get (model, &iter, entry_text_column, &value, -1);

			if (value && g_strrstr (value, email_address)) {
				g_free (value);
				gtk_combo_box_set_active (combo_box, ii);
				return TRUE;
			}

			g_free (value);

			ii++;
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	if (can_add) {
		/* The expected address is not in the list, thus add it */
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_box), BACKEND_EMAIL_ID, email_address);
		gtk_combo_box_set_active (combo_box, ii);

		return TRUE;
	}

	return FALSE;
}

static void
ecep_general_target_client_notify_cb (ECompEditor *comp_editor,
				      GParamSpec *param,
				      ECompEditorPageGeneral *page_general)
{
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	if ((e_comp_editor_get_flags (comp_editor) & E_COMP_EDITOR_FLAG_IS_NEW) != 0 ||
	    (e_comp_editor_get_source_client (comp_editor) != NULL &&
	     e_comp_editor_get_target_client (comp_editor) != e_comp_editor_get_source_client (comp_editor))) {
		const gchar *cal_email_address;

		cal_email_address = e_comp_editor_get_cal_email_address (comp_editor);
		ecep_general_pick_organizer_for_email_address (page_general, cal_email_address, TRUE);
	}

	if (page_general->priv->comp_color) {
		ECalClient *target_client;
		gboolean supports_color = FALSE;

		target_client = e_comp_editor_get_target_client (comp_editor);
		if (target_client)
			supports_color = e_client_check_capability (E_CLIENT (target_client), E_CAL_STATIC_CAPABILITY_COMPONENT_COLOR);

		e_comp_editor_property_part_set_visible (page_general->priv->comp_color, supports_color);
	}
}

static void
ecep_general_editor_flags_notify_cb (ECompEditor *comp_editor,
				     GParamSpec *param,
				     ECompEditorPageGeneral *page_general)
{
	gboolean can_change_target;

	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	can_change_target = (e_comp_editor_get_flags (comp_editor) & E_COMP_EDITOR_FLAG_IS_NEW) != 0 ||
		!e_comp_editor_get_component (comp_editor);
	if (!can_change_target) {
		ICalComponent *icomp = e_comp_editor_get_component (comp_editor);

		/* disallow move between targets only for recurring events */
		can_change_target = i_cal_component_isa (icomp) != I_CAL_VEVENT_COMPONENT ||
			(!e_cal_util_component_is_instance (icomp) &&
			 !e_cal_util_component_has_recurrences (icomp));
	}

	gtk_widget_set_sensitive (page_general->priv->source_combo_box, can_change_target);
	e_source_combo_box_set_show_full_name (E_SOURCE_COMBO_BOX (page_general->priv->source_combo_box), !can_change_target);
}

static gboolean
ecep_general_list_view_event_cb (EMeetingListView *list_view,
				 GdkEvent *event,
				 ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_MEETING_LIST_VIEW (list_view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), FALSE);

	if (event->type == GDK_2BUTTON_PRESS && gtk_widget_get_sensitive (GTK_WIDGET (list_view)) &&
	    gtk_widget_get_sensitive (page_general->priv->attendees_button_add)) {
		EMeetingAttendee *attendee;
		ECompEditor *comp_editor;
		guint32 flags;

		attendee = e_meeting_store_add_attendee_with_defaults (page_general->priv->meeting_store);

		comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));
		flags = e_comp_editor_get_flags (comp_editor);

		if ((flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0) {
			gchar *mailto;

			mailto = g_strdup_printf ("mailto:%s", page_general->priv->user_delegator);
			e_meeting_attendee_set_delfrom (attendee, mailto);
			g_free (mailto);
		}

		g_clear_object (&comp_editor);
		e_meeting_list_view_edit (list_view, attendee);

		return TRUE;
	}

	return FALSE;
}

static gboolean
ecep_general_list_view_key_press_cb (EMeetingListView *list_view,
				     GdkEventKey *event,
				     ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_MEETING_LIST_VIEW (list_view), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), FALSE);

	if (event->keyval == GDK_KEY_Delete) {
		if (gtk_widget_get_sensitive (page_general->priv->attendees_button_remove))
			ecep_general_attendees_remove_clicked_cb (NULL, page_general);
		return TRUE;
	} else if (event->keyval == GDK_KEY_Insert) {
		if (gtk_widget_get_sensitive (page_general->priv->attendees_button_add))
			ecep_general_attendees_add_clicked_cb (NULL, page_general);
		return TRUE;
	}

	return FALSE;
}

static void
ecep_general_init_ui (ECompEditorPageGeneral *page_general,
		      ECompEditor *comp_editor)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='view-menu'>"
		      "<placeholder id='columns'>"
			"<item action='view-role'/>"
			"<item action='view-rsvp'/>"
			"<item action='view-status'/>"
			"<item action='view-type'/>"
		      "</placeholder>"
		    "</submenu>"
		    "<submenu action='options-menu'>"
		      "<placeholder id='toggles'>"
			"<item action='option-attendees' text_only='true'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry attendees_toggle_entry[] = {
		{ "option-attendees",
		  NULL,
		  N_("A_ttendees"),
		  NULL,
		  N_("Toggles whether the Attendees are displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state }
	};

	static const EUIActionEntry columns_toggle_entries[] = {
		{ "view-role",
		  NULL,
		  N_("R_ole Field"),
		  NULL,
		  N_("Toggles whether the Role field is displayed"),
		  NULL, NULL, "true", action_view_role_cb },

		{ "view-rsvp",
		  NULL,
		  N_("_RSVP"),
		  NULL,
		  N_("Toggles whether the RSVP field is displayed"),
		  NULL, NULL, "true", action_view_rsvp_cb },

		{ "view-status",
		  NULL,
		  N_("_Status Field"),
		  NULL,
		  N_("Toggles whether the Status field is displayed"),
		  NULL, NULL, "true", action_view_status_cb },

		{ "view-type",
		  NULL,
		  N_("_Type Field"),
		  NULL,
		  N_("Toggles whether the Attendee Type is displayed"),
		  NULL, NULL, "true", action_view_type_cb }
	};

	EUIManager *ui_manager;
	EUIAction *action;
	GSettings *settings;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));
	g_return_if_fail (E_IS_COMP_EDITOR (comp_editor));

	settings = e_comp_editor_get_settings (comp_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions (ui_manager, "columns", GETTEXT_PACKAGE,
		columns_toggle_entries, G_N_ELEMENTS (columns_toggle_entries), page_general);

	e_binding_bind_property (
		page_general, "show-attendees",
		e_ui_manager_get_action_group (ui_manager, "columns"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		attendees_toggle_entry, G_N_ELEMENTS (attendees_toggle_entry), page_general, eui);

	action = e_comp_editor_get_action (comp_editor, "option-attendees");
	e_binding_bind_property (
		page_general, "show-attendees",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	action = e_comp_editor_get_action (comp_editor, "view-role");
	g_settings_bind (
		settings, "editor-show-role",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);
	ecep_general_set_column_visible (page_general, E_MEETING_STORE_ROLE_COL,
		e_ui_action_get_active (action));

	action = e_comp_editor_get_action (comp_editor, "view-rsvp");
	g_settings_bind (
		settings, "editor-show-rsvp",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);
	ecep_general_set_column_visible (page_general, E_MEETING_STORE_RSVP_COL,
		e_ui_action_get_active (action));

	action = e_comp_editor_get_action (comp_editor, "view-status");
	g_settings_bind (
		settings, "editor-show-status",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);
	ecep_general_set_column_visible (page_general, E_MEETING_STORE_STATUS_COL,
		e_ui_action_get_active (action));

	action = e_comp_editor_get_action (comp_editor, "view-type");
	g_settings_bind (
		settings, "editor-show-type",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);
	ecep_general_set_column_visible (page_general, E_MEETING_STORE_TYPE_COL,
		e_ui_action_get_active (action));
}

static void
ecep_general_sensitize_widgets (ECompEditorPage *page,
				gboolean force_insensitive)
{
	ECompEditorPageGeneral *page_general;
	GtkTreeSelection *selection;
	gboolean sensitive, organizer_is_user, delegate, delegate_to_many = FALSE, read_only = TRUE, any_selected = FALSE;
	ECompEditor *comp_editor;
	ECalClient *client;
	EUIAction *action;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_general_parent_class)->sensitize_widgets (page, force_insensitive);

	page_general = E_COMP_EDITOR_PAGE_GENERAL (page);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));
	flags = e_comp_editor_get_flags (comp_editor);
	client = e_comp_editor_get_target_client (comp_editor);

	if (client) {
		EClient *cl = E_CLIENT (client);

		read_only = e_client_is_readonly (cl);
		delegate_to_many = e_client_check_capability (cl, E_CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY);
	} else {
		force_insensitive = TRUE;
	}

	delegate = (flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0;
	organizer_is_user = (flags & (E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER | E_COMP_EDITOR_FLAG_IS_NEW)) != 0 ||
		!e_comp_editor_page_general_get_show_attendees (page_general);
	sensitive = (!read_only && organizer_is_user) || delegate;

	if (!delegate)
		delegate_to_many = TRUE;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_general->priv->attendees_list_view));
	any_selected = selection && gtk_tree_selection_count_selected_rows (selection) > 0;

	gtk_widget_set_sensitive (page_general->priv->organizer_label, !force_insensitive);
	gtk_widget_set_sensitive (page_general->priv->organizer_combo_box, !read_only && !force_insensitive);
	gtk_widget_set_sensitive (page_general->priv->attendees_button, sensitive && delegate_to_many && !force_insensitive);
	gtk_widget_set_sensitive (page_general->priv->attendees_hbox, !force_insensitive);
	gtk_widget_set_sensitive (page_general->priv->attendees_button_add, sensitive && delegate_to_many && !force_insensitive);
	gtk_widget_set_sensitive (page_general->priv->attendees_button_edit, sensitive && delegate_to_many && !force_insensitive && any_selected);
	gtk_widget_set_sensitive (page_general->priv->attendees_button_remove, sensitive && !force_insensitive && any_selected);
	e_meeting_list_view_set_editable (E_MEETING_LIST_VIEW (page_general->priv->attendees_list_view), sensitive && !force_insensitive);
	gtk_widget_set_sensitive (page_general->priv->attendees_list_view, !read_only && !force_insensitive);

	action = e_comp_editor_get_action (comp_editor, "option-attendees");
	e_ui_action_set_sensitive (action, !force_insensitive && !read_only);

	if (page_general->priv->comp_color &&
	    !e_comp_editor_property_part_get_sensitize_handled (page_general->priv->comp_color)) {
		e_comp_editor_property_part_sensitize_widgets (page_general->priv->comp_color, force_insensitive || read_only);
	}

	g_clear_object (&comp_editor);
}

static void
ecep_general_fill_widgets (ECompEditorPage *page,
			   ICalComponent *component)
{
	ECompEditorPageGeneral *page_general;
	EMeetingListView *attendees_list_view;
	ICalProperty *prop;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_general_parent_class)->fill_widgets (page, component);

	page_general = E_COMP_EDITOR_PAGE_GENERAL (page);

	if (page_general->priv->comp_color)
		e_comp_editor_property_part_fill_widget (page_general->priv->comp_color, component);

	g_slist_free_full (page_general->priv->orig_attendees, g_free);
	page_general->priv->orig_attendees = NULL;

	for (prop = i_cal_component_get_first_property (component, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (component, I_CAL_ATTENDEE_PROPERTY)) {
		const gchar *address;

		address = e_cal_util_get_property_email (prop);
		if (address)
			page_general->priv->orig_attendees = g_slist_prepend (page_general->priv->orig_attendees, g_strdup (address));
	}

	page_general->priv->orig_attendees = g_slist_reverse (page_general->priv->orig_attendees);

	prop = i_cal_component_get_first_property (component, I_CAL_ORGANIZER_PROPERTY);
	if (prop) {
		ICalParameter *param;
		const gchar *organizer;

		organizer = e_cal_util_get_property_email (prop);

		if (organizer && *organizer) {
			ECompEditor *comp_editor;
			ESourceRegistry *registry;
			guint32 flags;
			gchar *value = NULL;

			comp_editor = e_comp_editor_page_ref_editor (page);
			flags = e_comp_editor_get_flags (comp_editor);
			registry = e_shell_get_registry (e_comp_editor_get_shell (comp_editor));

			flags = flags & E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;

			if (itip_address_is_user (registry, e_cal_util_strip_mailto (organizer))) {
				flags = flags | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;
			} else {
				param = i_cal_property_get_first_parameter (prop, I_CAL_SENTBY_PARAMETER);
				if (param) {
					const gchar *sentby = i_cal_parameter_get_sentby (param);

					if (sentby && *sentby &&
					    itip_address_is_user (registry, e_cal_util_strip_mailto (organizer))) {
						flags = flags | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;
					}

					g_object_unref (param);
				}
			}

			e_comp_editor_page_general_set_show_attendees (page_general, TRUE);

			param = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
			if (param) {
				const gchar *cn;

				cn = i_cal_parameter_get_cn (param);
				if (cn && *cn) {
					value = camel_internet_address_format_address (cn, e_cal_util_strip_mailto (organizer));
				}

				g_object_unref (param);
			}

			if (!value)
				value = g_strdup (e_cal_util_strip_mailto (organizer));

			if (!(flags & E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER) ||
			    !ecep_general_pick_organizer_for_email_address (page_general, organizer, FALSE)) {
				GtkComboBoxText *combo_box_text;

				combo_box_text = GTK_COMBO_BOX_TEXT (page_general->priv->organizer_combo_box);
				gtk_combo_box_text_remove_all (combo_box_text);
				gtk_combo_box_text_append_text (combo_box_text, value);
				gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box_text), 0);
			}

			e_comp_editor_set_flags (comp_editor, flags);

			g_clear_object (&comp_editor);
			g_free (value);
		}

		g_object_unref (prop);
	}

	attendees_list_view = E_MEETING_LIST_VIEW (page_general->priv->attendees_list_view);

	e_meeting_store_remove_all_attendees (page_general->priv->meeting_store);
	e_meeting_list_view_remove_all_attendees_from_name_selector (attendees_list_view);

	for (prop = i_cal_component_get_first_property (component, I_CAL_ATTENDEE_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (component, I_CAL_ATTENDEE_PROPERTY)) {
		const gchar *address;

		address = e_cal_util_get_property_email (prop);
		if (address) {
			EMeetingAttendee *attendee;
			ECalComponentAttendee *comp_attendee;

			comp_attendee = e_cal_component_attendee_new_from_property (prop);
			if (!comp_attendee) {
				g_warn_if_reached ();
				continue;
			}

			attendee = E_MEETING_ATTENDEE (e_meeting_attendee_new_from_e_cal_component_attendee (comp_attendee));

			e_cal_component_attendee_free (comp_attendee);

			e_meeting_store_add_attendee (page_general->priv->meeting_store, attendee);
			e_meeting_list_view_add_attendee_to_name_selector (attendees_list_view, attendee);

			g_object_unref (attendee);
		}
	}
}

static gboolean
ecep_general_fill_component (ECompEditorPage *page,
			     ICalComponent *component)
{
	ECompEditorPageGeneral *page_general;
	ICalProperty *prop;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	page_general = E_COMP_EDITOR_PAGE_GENERAL (page);

	if (page_general->priv->comp_color)
		e_comp_editor_property_part_fill_component (page_general->priv->comp_color, component);

	e_cal_util_component_remove_property_by_kind (component, I_CAL_ATTENDEE_PROPERTY, TRUE);

	if (e_comp_editor_page_general_get_show_attendees (page_general)) {
		const GPtrArray *attendees;
		GHashTable *known_attendees;
		ECompEditor *comp_editor;
		gchar *organizer_name = NULL, *organizer_mailto = NULL;
		guint32 flags;
		gint ii, added_attendees = 0;
		const gchar *error_message = NULL;

		comp_editor = e_comp_editor_page_ref_editor (page);
		flags = e_comp_editor_get_flags (comp_editor);

		if ((flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0 &&
		    !ecep_general_get_organizer (page_general, NULL, NULL, &error_message)) {
			e_comp_editor_set_validation_error (comp_editor, page,
				page_general->priv->organizer_combo_box,
				error_message ? error_message : _("An organizer is required."));

			g_clear_object (&comp_editor);

			return FALSE;
		}

		if (e_meeting_store_count_actual_attendees (page_general->priv->meeting_store) < 1) {
			e_comp_editor_set_validation_error (comp_editor, page,
				page_general->priv->attendees_list_view,
				_("At least one attendee is required."));

			g_clear_object (&comp_editor);

			return FALSE;
		}

		/* Organizer */
		if ((flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0 &&
		    ecep_general_get_organizer (page_general, &organizer_name, &organizer_mailto, NULL)) {
			const gchar *cal_email_address;
			ICalParameter *param;

			prop = i_cal_component_get_first_property (component, I_CAL_ORGANIZER_PROPERTY);
			if (!prop) {
				prop = i_cal_property_new_organizer (organizer_mailto ? organizer_mailto : organizer_name);
				i_cal_component_take_property (component, prop);
				prop = i_cal_component_get_first_property (component, I_CAL_ORGANIZER_PROPERTY);
			} else {
				i_cal_property_set_organizer (prop, organizer_mailto ? organizer_mailto : organizer_name);
			}

			param = i_cal_property_get_first_parameter (prop, I_CAL_CN_PARAMETER);
			if (organizer_name && *organizer_name) {
				if (!param) {
					param = i_cal_parameter_new_cn (organizer_name);
					i_cal_property_add_parameter (prop, param);
				} else {
					i_cal_parameter_set_cn (param, organizer_name);
				}
			} else if (param) {
				i_cal_property_remove_parameter_by_kind (prop, I_CAL_CN_PARAMETER);
			}
			g_clear_object (&param);

			param = i_cal_property_get_first_parameter (prop, I_CAL_SENTBY_PARAMETER);
			cal_email_address = e_comp_editor_get_cal_email_address (comp_editor);
			if (cal_email_address && *cal_email_address) {
				gchar *sentby;
				gboolean differs;

				sentby = g_strconcat ("mailto:", cal_email_address, NULL);
				differs = !organizer_mailto || g_ascii_strcasecmp (sentby, organizer_mailto) != 0;

				if (differs) {
					if (!param) {
						param = i_cal_parameter_new_sentby (sentby);
						i_cal_property_add_parameter (prop, param);
					} else {
						i_cal_parameter_set_sentby (param, sentby);
					}
				} else if (param) {
					i_cal_property_remove_parameter_by_kind (prop, I_CAL_SENTBY_PARAMETER);
				}

				g_free (sentby);
			} else if (param) {
				i_cal_property_remove_parameter_by_kind (prop, I_CAL_SENTBY_PARAMETER);
			}
			g_clear_object (&param);

			g_object_unref (prop);
		}

		/* Attendees */
		known_attendees = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
		attendees = e_meeting_store_get_attendees (page_general->priv->meeting_store);

		for (ii = 0; ii < attendees->len; ii++) {
			EMeetingAttendee *attendee = g_ptr_array_index (attendees, ii);
			const gchar *address;

			address = e_cal_util_strip_mailto (e_meeting_attendee_get_address (attendee));
			if (address) {
				ICalParameter *param;

				if ((flags & E_COMP_EDITOR_FLAG_DELEGATE) != 0 &&
				    (e_meeting_attendee_is_set_delfrom (attendee) || e_meeting_attendee_is_set_delto (attendee)) &&
				    g_hash_table_contains (known_attendees, address))
					continue;

				g_hash_table_insert (known_attendees, (gpointer) address, GINT_TO_POINTER (1));

				prop = i_cal_property_new_attendee (e_meeting_attendee_get_address (attendee));

				added_attendees++;

				if (e_meeting_attendee_is_set_member (attendee)) {
					param = i_cal_parameter_new_member (e_meeting_attendee_get_member (attendee));
					i_cal_property_take_parameter (prop, param);
				}

				param = i_cal_parameter_new_cutype (e_meeting_attendee_get_cutype (attendee));
				if (param)
					i_cal_property_take_parameter (prop, param);

				param = i_cal_parameter_new_role (e_meeting_attendee_get_role (attendee));
				if (param)
					i_cal_property_take_parameter (prop, param);

				param = i_cal_parameter_new_partstat (e_meeting_attendee_get_partstat (attendee));
				if (param)
					i_cal_property_take_parameter (prop, param);

				param = i_cal_parameter_new_rsvp (e_meeting_attendee_get_rsvp (attendee) ? I_CAL_RSVP_TRUE : I_CAL_RSVP_FALSE);
				i_cal_property_take_parameter (prop, param);

				if (e_meeting_attendee_is_set_delfrom (attendee)) {
					param = i_cal_parameter_new_delegatedfrom (e_meeting_attendee_get_delfrom (attendee));
					i_cal_property_take_parameter (prop, param);
				}
				if (e_meeting_attendee_is_set_delto (attendee)) {
					param = i_cal_parameter_new_delegatedto (e_meeting_attendee_get_delto (attendee));
					i_cal_property_take_parameter (prop, param);
				}
				if (e_meeting_attendee_is_set_sentby (attendee)) {
					param = i_cal_parameter_new_sentby (e_meeting_attendee_get_sentby (attendee));
					i_cal_property_take_parameter (prop, param);
				}
				if (e_meeting_attendee_is_set_cn (attendee)) {
					param = i_cal_parameter_new_cn (e_meeting_attendee_get_cn (attendee));
					i_cal_property_take_parameter (prop, param);
				}
				if (e_meeting_attendee_is_set_language (attendee)) {
					param = i_cal_parameter_new_language (e_meeting_attendee_get_language (attendee));
					i_cal_property_take_parameter (prop, param);
				}

				e_cal_component_parameter_bag_fill_property (e_meeting_attendee_get_parameter_bag (attendee), prop);

				i_cal_component_take_property (component, prop);
			}
		}

		g_hash_table_destroy (known_attendees);

		g_free (organizer_name);
		g_free (organizer_mailto);

		if (!added_attendees) {
			e_comp_editor_set_validation_error (comp_editor, page,
				page_general->priv->attendees_list_view,
				_("At least one attendee is required."));

			g_clear_object (&comp_editor);

			return FALSE;
		}

		g_clear_object (&comp_editor);
	} else {
		e_cal_util_component_remove_property_by_kind (component, I_CAL_ORGANIZER_PROPERTY, TRUE);
	}

	return E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_general_parent_class)->fill_component (page, component);
}

static void
ecep_general_set_property (GObject *object,
			   guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DATA_COLUMN_WIDTH:
			e_comp_editor_page_general_set_data_column_width (
				E_COMP_EDITOR_PAGE_GENERAL (object),
				g_value_get_int (value));
			return;

		case PROP_SOURCE_LABEL:
			e_comp_editor_page_general_set_source_label (
				E_COMP_EDITOR_PAGE_GENERAL (object),
				g_value_get_string (value));
			return;

		case PROP_SOURCE_EXTENSION_NAME:
			e_comp_editor_page_general_set_source_extension_name (
				E_COMP_EDITOR_PAGE_GENERAL (object),
				g_value_get_string (value));
			return;

		case PROP_SELECTED_SOURCE:
			e_comp_editor_page_general_set_selected_source (
				E_COMP_EDITOR_PAGE_GENERAL (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_ATTENDEES:
			e_comp_editor_page_general_set_show_attendees (
				E_COMP_EDITOR_PAGE_GENERAL (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ecep_general_get_property (GObject *object,
			   guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DATA_COLUMN_WIDTH:
			g_value_set_int (
				value,
				e_comp_editor_page_general_get_data_column_width (
				E_COMP_EDITOR_PAGE_GENERAL (object)));
			return;

		case PROP_SOURCE_LABEL:
			g_value_set_string (
				value,
				e_comp_editor_page_general_get_source_label (
				E_COMP_EDITOR_PAGE_GENERAL (object)));
			return;

		case PROP_SOURCE_EXTENSION_NAME:
			g_value_set_string (
				value,
				e_comp_editor_page_general_get_source_extension_name (
				E_COMP_EDITOR_PAGE_GENERAL (object)));
			return;

		case PROP_SELECTED_SOURCE:
			g_value_take_object (
				value,
				e_comp_editor_page_general_ref_selected_source (
				E_COMP_EDITOR_PAGE_GENERAL (object)));
			return;

		case PROP_SHOW_ATTENDEES:
			g_value_set_boolean (
				value,
				e_comp_editor_page_general_get_show_attendees (
				E_COMP_EDITOR_PAGE_GENERAL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
ecep_general_constructed (GObject *object)
{
	ECompEditor *comp_editor;
	ECompEditorPageGeneral *page_general;
	ECompEditorPropertyPart *part;
	GtkWidget *widget, *scrolled_window;
	GtkTreeSelection *selection;
	GtkGrid *grid;
	EShell *shell;

	page_general = E_COMP_EDITOR_PAGE_GENERAL (object);

	G_OBJECT_CLASS (e_comp_editor_page_general_parent_class)->constructed (object);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));
	g_return_if_fail (comp_editor != NULL);

	page_general->priv->meeting_store = E_MEETING_STORE (e_meeting_store_new ());

	grid = GTK_GRID (page_general);

	widget = gtk_label_new_with_mnemonic (_("Or_ganizer:"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	gtk_widget_hide (widget);

	page_general->priv->organizer_label = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_grid_attach (grid, widget, 1, 0, page_general->priv->data_column_width, 1);
	gtk_widget_hide (widget);

	page_general->priv->organizer_hbox = widget;

	widget = e_ellipsized_combo_box_text_new (FALSE);
	e_ellipsized_combo_box_text_set_max_natural_width (E_ELLIPSIZED_COMBO_BOX_TEXT (widget), 100);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"width-request", 100,
		NULL);
	gtk_box_pack_start (GTK_BOX (page_general->priv->organizer_hbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	page_general->priv->organizer_combo_box = widget;

	ecep_general_fill_organizer_combo_box (page_general);

	g_signal_connect_swapped (page_general->priv->organizer_combo_box, "changed",
		G_CALLBACK (e_comp_editor_ensure_changed), comp_editor);

	widget = gtk_label_new_with_mnemonic (page_general->priv->source_label_text);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_box_pack_start (GTK_BOX (page_general->priv->organizer_hbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	page_general->priv->source_label = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_box_pack_start (GTK_BOX (page_general->priv->organizer_hbox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	page_general->priv->source_and_color_hbox = widget;

	shell = e_comp_editor_get_shell (comp_editor);
	widget = e_source_combo_box_new (
		e_shell_get_registry (shell),
		page_general->priv->source_extension_name);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"width-request", 100,
		"max-natural-width", 100,
		"show-colors", TRUE,
		NULL);
	gtk_box_pack_start (GTK_BOX (page_general->priv->source_and_color_hbox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	page_general->priv->source_combo_box = widget;

	gtk_label_set_mnemonic_widget (GTK_LABEL (page_general->priv->source_label),
		page_general->priv->source_combo_box);
	g_signal_connect (page_general->priv->source_combo_box, "changed",
		G_CALLBACK (ecep_general_source_combo_box_changed_cb), page_general);

	part = e_comp_editor_property_part_color_new ();
	widget = e_comp_editor_property_part_get_edit_widget (part);

	if (widget) {
		const gchar *tooltip;

		gtk_box_pack_start (GTK_BOX (page_general->priv->source_and_color_hbox), widget, FALSE, FALSE, 0);

		if (g_strcmp0 (page_general->priv->source_extension_name, E_SOURCE_EXTENSION_CALENDAR) == 0) {
			tooltip = _("Override color of the event. If not set, then color of the calendar is used.");
		} else if (g_strcmp0 (page_general->priv->source_extension_name, E_SOURCE_EXTENSION_MEMO_LIST) == 0) {
			tooltip = _("Override color of the memo. If not set, then color of the memo list is used.");
		} else { /* E_SOURCE_EXTENSION_TASK_LIST */
			tooltip = _("Override color of the task. If not set, then color of the task list is used.");
		}

		gtk_widget_set_tooltip_text (widget, tooltip);
	}

	page_general->priv->comp_color_changed_handler_id = g_signal_connect_swapped (part, "changed",
		G_CALLBACK (e_comp_editor_page_emit_changed), page_general);

	page_general->priv->comp_color = part;

	widget = gtk_button_new_with_mnemonic (C_("ECompEditor", "Atte_ndees…"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);
	gtk_widget_hide (widget);

	page_general->priv->attendees_button = widget;

	g_signal_connect (widget, "clicked", G_CALLBACK (ecep_general_attendees_clicked_cb), page_general);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_grid_attach (grid, widget, 1, 1, page_general->priv->data_column_width, 1);
	gtk_widget_hide (widget);

	page_general->priv->attendees_hbox = widget;

	widget = GTK_WIDGET (e_meeting_list_view_new (page_general->priv->meeting_store));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_widget_show (scrolled_window);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);
	gtk_box_pack_start (GTK_BOX (page_general->priv->attendees_hbox), scrolled_window, TRUE, TRUE, 0);

	page_general->priv->attendees_list_view = widget;

	g_signal_connect_object (page_general->priv->attendees_list_view, "attendee-added",
		G_CALLBACK (ecep_general_attendee_added_cb), page_general, 0);

	g_signal_connect_object (page_general->priv->meeting_store, "row-changed",
		G_CALLBACK (ecep_general_attendee_row_changed_cb), page_general, 0);

	e_signal_connect_notify_object (page_general->priv->meeting_store, "notify::show-address",
		G_CALLBACK (ecep_general_attendee_show_address_notify_cb), page_general, 0);

	g_signal_connect_object (page_general->priv->attendees_list_view, "event",
		G_CALLBACK (ecep_general_list_view_event_cb), page_general, 0);

	g_signal_connect_object (page_general->priv->attendees_list_view, "key_press_event",
		G_CALLBACK (ecep_general_list_view_key_press_cb), page_general, 0);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_box_pack_start (GTK_BOX (page_general->priv->attendees_hbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	page_general->priv->attendees_button_box = widget;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_general->priv->attendees_list_view));
	g_signal_connect (selection, "changed", G_CALLBACK (ecep_general_attendees_selection_changed_cb), page_general);

	widget = gtk_button_new_with_mnemonic (_("_Add"));
	gtk_box_pack_start (GTK_BOX (page_general->priv->attendees_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	page_general->priv->attendees_button_add = widget;

	g_signal_connect (widget, "clicked", G_CALLBACK (ecep_general_attendees_add_clicked_cb), page_general);

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (GTK_BOX (page_general->priv->attendees_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	page_general->priv->attendees_button_edit = widget;

	g_signal_connect (widget, "clicked", G_CALLBACK (ecep_general_attendees_edit_clicked_cb), page_general);

	widget = gtk_button_new_with_mnemonic (_("_Remove"));
	gtk_box_pack_start (GTK_BOX (page_general->priv->attendees_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	page_general->priv->attendees_button_remove = widget;

	g_signal_connect (widget, "clicked", G_CALLBACK (ecep_general_attendees_remove_clicked_cb), page_general);

	e_signal_connect_notify_object (comp_editor, "notify::target-client", G_CALLBACK (ecep_general_target_client_notify_cb), page_general, 0);
	e_signal_connect_notify_object (comp_editor, "notify::flags", G_CALLBACK (ecep_general_editor_flags_notify_cb), page_general, 0);

	ecep_general_editor_flags_notify_cb (comp_editor, NULL, page_general);

	ecep_general_init_ui (page_general, comp_editor);

	g_clear_object (&comp_editor);
}

static void
ecep_general_finalize (GObject *object)
{
	ECompEditorPageGeneral *page_general;

	page_general = E_COMP_EDITOR_PAGE_GENERAL (object);

	g_free (page_general->priv->source_label_text);
	page_general->priv->source_label_text = NULL;

	g_free (page_general->priv->source_extension_name);
	page_general->priv->source_extension_name = NULL;

	g_free (page_general->priv->user_delegator);
	page_general->priv->user_delegator = NULL;

	if (page_general->priv->comp_color && page_general->priv->comp_color_changed_handler_id) {
		g_signal_handler_disconnect (page_general->priv->comp_color, page_general->priv->comp_color_changed_handler_id);
		page_general->priv->comp_color_changed_handler_id = 0;
	}

	g_clear_object (&page_general->priv->comp_color);
	g_clear_object (&page_general->priv->select_source);
	g_clear_object (&page_general->priv->meeting_store);

	g_slist_free_full (page_general->priv->orig_attendees, g_free);
	page_general->priv->orig_attendees = NULL;

	G_OBJECT_CLASS (e_comp_editor_page_general_parent_class)->finalize (object);
}

static void
e_comp_editor_page_general_init (ECompEditorPageGeneral *page_general)
{
	page_general->priv = e_comp_editor_page_general_get_instance_private (page_general);
}

static void
e_comp_editor_page_general_class_init (ECompEditorPageGeneralClass *klass)
{
	ECompEditorPageClass *page_class;
	GObjectClass *object_class;

	page_class = E_COMP_EDITOR_PAGE_CLASS (klass);
	page_class->sensitize_widgets = ecep_general_sensitize_widgets;
	page_class->fill_widgets = ecep_general_fill_widgets;
	page_class->fill_component = ecep_general_fill_component;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = ecep_general_set_property;
	object_class->get_property = ecep_general_get_property;
	object_class->constructed = ecep_general_constructed;
	object_class->finalize = ecep_general_finalize;

	g_object_class_install_property (
		object_class,
		PROP_DATA_COLUMN_WIDTH,
		g_param_spec_int (
			"data-column-width",
			"Data Column Width",
			"How many columns should the data column occupy",
			1, G_MAXINT, 1,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LABEL,
		g_param_spec_string (
			"source-label",
			"Source Label",
			"Label to use for the source selector",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_EXTENSION_NAME,
		g_param_spec_string (
			"source-extension-name",
			"Source Extension Name",
			"Extension name to use for the source selector",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SELECTED_SOURCE,
		g_param_spec_object (
			"selected-source",
			"Selected Source",
			"Which source is currently selected in the source selector",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_ATTENDEES,
		g_param_spec_boolean (
			"show-attendees",
			"Show Attendees",
			"Whether to show also attendees",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

ECompEditorPage *
e_comp_editor_page_general_new (ECompEditor *editor,
				const gchar *source_label,
				const gchar *source_extension_name,
				ESource *select_source,
				gboolean show_attendees,
				gint data_column_width)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (editor), NULL);
	g_return_val_if_fail (source_label != NULL, NULL);
	g_return_val_if_fail (source_extension_name != NULL, NULL);

	if (select_source)
		g_return_val_if_fail (E_IS_SOURCE (select_source), NULL);

	return g_object_new (E_TYPE_COMP_EDITOR_PAGE_GENERAL,
		"editor", editor,
		"source-label", source_label,
		"source-extension-name", source_extension_name,
		"selected-source", select_source,
		"show-attendees", show_attendees,
		"data-column-width", data_column_width,
		NULL);
}

const gchar *
e_comp_editor_page_general_get_source_label (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);

	if (page_general->priv->source_label)
		return gtk_label_get_text (GTK_LABEL (page_general->priv->source_label));

	return page_general->priv->source_label_text;
}

void
e_comp_editor_page_general_set_source_label (ECompEditorPageGeneral *page_general,
					     const gchar *source_label)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));
	g_return_if_fail (source_label != NULL);

	if (page_general->priv->source_label) {
		if (g_strcmp0 (source_label, gtk_label_get_text (GTK_LABEL (page_general->priv->source_label))) != 0) {
			gtk_label_set_text (GTK_LABEL (page_general->priv->source_label), source_label);

			g_object_notify (G_OBJECT (page_general), "source-label");
		}
	} else {
		g_free (page_general->priv->source_label_text);
		page_general->priv->source_label_text = g_strdup (source_label);

		g_object_notify (G_OBJECT (page_general), "source-label");
	}
}

const gchar *
e_comp_editor_page_general_get_source_extension_name (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);

	if (page_general->priv->source_combo_box)
		return e_source_combo_box_get_extension_name (E_SOURCE_COMBO_BOX (page_general->priv->source_combo_box));

	return page_general->priv->source_extension_name;
}

void
e_comp_editor_page_general_set_source_extension_name (ECompEditorPageGeneral *page_general,
						      const gchar *source_extension_name)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	if (g_strcmp0 (page_general->priv->source_extension_name, source_extension_name) == 0)
		return;

	g_free (page_general->priv->source_extension_name);
	page_general->priv->source_extension_name = g_strdup (source_extension_name);

	g_object_notify (G_OBJECT (page_general), "source-extension-name");

	if (page_general->priv->source_combo_box) {
		e_source_combo_box_set_extension_name (
			E_SOURCE_COMBO_BOX (page_general->priv->source_combo_box),
			source_extension_name);
	}
}

ESource *
e_comp_editor_page_general_ref_selected_source (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);
	g_return_val_if_fail (page_general->priv->source_combo_box != NULL, NULL);

	return e_source_combo_box_ref_active (E_SOURCE_COMBO_BOX (page_general->priv->source_combo_box));
}

void
e_comp_editor_page_general_set_selected_source (ECompEditorPageGeneral *page_general,
						ESource *source)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));
	if (source)
		g_return_if_fail (E_IS_SOURCE (source));

	if (!page_general->priv->source_combo_box) {
		g_clear_object (&page_general->priv->select_source);
		page_general->priv->select_source = g_object_ref (source);

		g_object_notify (G_OBJECT (page_general), "selected-source");

		return;
	}

	if (source)
		e_source_combo_box_set_active (E_SOURCE_COMBO_BOX (page_general->priv->source_combo_box), source);

	g_object_notify (G_OBJECT (page_general), "selected-source");
}

gboolean
e_comp_editor_page_general_get_show_attendees (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), FALSE);

	return page_general->priv->show_attendees;
}

void
e_comp_editor_page_general_set_show_attendees (ECompEditorPageGeneral *page_general,
					       gboolean show_attendees)
{
	ECompEditor *comp_editor;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	if ((show_attendees ? 1 : 0) == (page_general->priv->show_attendees ? 1 : 0))
		return;

	page_general->priv->show_attendees = show_attendees;

	g_object_notify (G_OBJECT (page_general), "show-attendees");

	e_comp_editor_page_general_update_view (page_general);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_general));
	if (comp_editor)
		e_comp_editor_set_changed (comp_editor, TRUE);
	g_clear_object (&comp_editor);
}

gint
e_comp_editor_page_general_get_data_column_width (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), 0);

	return page_general->priv->data_column_width;
}

void
e_comp_editor_page_general_set_data_column_width (ECompEditorPageGeneral *page_general,
						  gint data_column_width)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	if (data_column_width == page_general->priv->data_column_width)
		return;

	page_general->priv->data_column_width = data_column_width;

	g_object_notify (G_OBJECT (page_general), "data-column-width");

	e_comp_editor_page_general_update_view (page_general);
}

void
e_comp_editor_page_general_update_view (ECompEditorPageGeneral *page_general)
{
	GtkContainer *grid;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general));

	if (!page_general->priv->source_label)
		return;

	grid = GTK_CONTAINER (page_general);

	gtk_container_child_set (grid, page_general->priv->organizer_hbox,
		"left-attach", 1, "width", page_general->priv->data_column_width, NULL);
	gtk_container_child_set (grid, page_general->priv->attendees_hbox,
		"width", page_general->priv->data_column_width, NULL);

	if (page_general->priv->show_attendees) {
		if (gtk_widget_get_parent (page_general->priv->source_label) == GTK_WIDGET (grid)) {

			g_object_ref (page_general->priv->source_label);
			g_object_ref (page_general->priv->source_and_color_hbox);

			gtk_container_remove (grid, page_general->priv->source_label);
			gtk_container_remove (grid, page_general->priv->source_and_color_hbox);

			gtk_box_pack_start (GTK_BOX (page_general->priv->organizer_hbox),
				page_general->priv->source_label, FALSE, FALSE, 0);
			gtk_box_pack_start (GTK_BOX (page_general->priv->organizer_hbox),
				page_general->priv->source_and_color_hbox, TRUE, TRUE, 0);

			g_object_unref (page_general->priv->source_label);
			g_object_unref (page_general->priv->source_and_color_hbox);
		}

		gtk_container_child_set (grid, page_general->priv->organizer_label,
			"left-attach", 0, NULL);

		gtk_widget_show (page_general->priv->organizer_label);
		gtk_widget_show (page_general->priv->organizer_hbox);
		gtk_widget_show (page_general->priv->attendees_button);
		gtk_widget_show (page_general->priv->attendees_hbox);
		gtk_widget_show (page_general->priv->attendees_list_view);
		gtk_widget_show (page_general->priv->attendees_button_box);
	} else {
		if (gtk_widget_get_parent (page_general->priv->source_label) != GTK_WIDGET (grid)) {
			GtkContainer *container;
			GtkGrid *ggrid;

			container = GTK_CONTAINER (page_general->priv->organizer_hbox);
			ggrid = GTK_GRID (grid);

			g_object_ref (page_general->priv->source_label);
			g_object_ref (page_general->priv->source_and_color_hbox);

			gtk_container_remove (container, page_general->priv->source_label);
			gtk_container_remove (container, page_general->priv->source_and_color_hbox);

			gtk_grid_attach (ggrid, page_general->priv->source_label, 0, 0, 1, 1);
			gtk_grid_attach (ggrid, page_general->priv->source_and_color_hbox, 1, 0, 1, 1);

			g_object_unref (page_general->priv->source_label);
			g_object_unref (page_general->priv->source_and_color_hbox);
		}

		gtk_container_child_set (grid, page_general->priv->source_label,
			"left-attach", 0, NULL);
		gtk_container_child_set (grid, page_general->priv->source_and_color_hbox,
			"left-attach", 1, "width", page_general->priv->data_column_width, NULL);

		gtk_widget_hide (page_general->priv->organizer_label);
		gtk_widget_hide (page_general->priv->organizer_hbox);
		gtk_widget_hide (page_general->priv->attendees_button);
		gtk_widget_hide (page_general->priv->attendees_hbox);
		gtk_widget_hide (page_general->priv->attendees_list_view);
		gtk_widget_hide (page_general->priv->attendees_button_box);
	}
}

EMeetingStore *
e_comp_editor_page_general_get_meeting_store (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);

	return page_general->priv->meeting_store;
}

ENameSelector *
e_comp_editor_page_general_get_name_selector (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);

	return e_meeting_list_view_get_name_selector (E_MEETING_LIST_VIEW (page_general->priv->attendees_list_view));
}

/* Element is a string, an email address; free with g_slist_free_full (slist, g_free); */
GSList *
e_comp_editor_page_general_get_added_attendees (ECompEditorPageGeneral *page_general)
{
	const GPtrArray *attendees;
	GHashTable *orig_attendees = NULL;
	GSList *added_attendees = NULL, *link;
	gint ii;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);

	if (!page_general->priv->show_attendees)
		return NULL;

	if (page_general->priv->orig_attendees) {
		for (link = page_general->priv->orig_attendees; link; link = g_slist_next (link)) {
			const gchar *address = link->data;

			if (address) {
				if (!orig_attendees)
					orig_attendees = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
				g_hash_table_insert (orig_attendees, (gpointer) address, GINT_TO_POINTER (1));
			}
		}
	}

	attendees = e_meeting_store_get_attendees (page_general->priv->meeting_store);

	for (ii = 0; ii < attendees->len; ii++) {
		EMeetingAttendee *attendee = g_ptr_array_index (attendees, ii);
		const gchar *address;

		address = e_cal_util_strip_mailto (e_meeting_attendee_get_address (attendee));

		if (address && (!orig_attendees || !g_hash_table_contains (orig_attendees, address)))
			added_attendees = g_slist_prepend (added_attendees, g_strdup (address));
	}

	if (orig_attendees)
		g_hash_table_destroy (orig_attendees);

	return g_slist_reverse (added_attendees);
}

/* Element is a string, an email address; free with g_slist_free_full (slist, g_free); */
GSList *
e_comp_editor_page_general_get_removed_attendees (ECompEditorPageGeneral *page_general)
{
	const GPtrArray *attendees;
	GHashTable *new_attendees;
	GSList *removed_attendees = NULL, *link;
	gint ii;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);

	if (!page_general->priv->orig_attendees)
		return NULL;

	if (!page_general->priv->show_attendees) {
		GSList *copy;

		copy = g_slist_copy (page_general->priv->orig_attendees);
		for (link = copy; link; link = g_slist_next (link)) {
			link->data = g_strdup (link->data);
		}

		return copy;
	}

	new_attendees = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
	attendees = e_meeting_store_get_attendees (page_general->priv->meeting_store);

	for (ii = 0; ii < attendees->len; ii++) {
		EMeetingAttendee *attendee = g_ptr_array_index (attendees, ii);
		const gchar *address;

		address = e_cal_util_strip_mailto (e_meeting_attendee_get_address (attendee));
		if (address)
			g_hash_table_insert (new_attendees, (gpointer) address, GINT_TO_POINTER (1));
	}

	for (link = page_general->priv->orig_attendees; link; link = g_slist_next (link)) {
		const gchar *address = link->data;

		if (address && !g_hash_table_contains (new_attendees, address)) {
			removed_attendees = g_slist_prepend (removed_attendees, g_strdup (address));
		}
	}

	g_hash_table_destroy (new_attendees);

	return g_slist_reverse (removed_attendees);
}

GtkWidget *
e_comp_editor_page_general_get_source_combo_box (ECompEditorPageGeneral *page_general)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_GENERAL (page_general), NULL);

	return page_general->priv->source_combo_box;
}
