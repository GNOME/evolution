/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#include <mail/e-mail-reader.h>
#include <mail/em-folder-tree.h>
#include <mail/em-folder-tree-model.h>
#include <mail/em-utils.h>
#include <mail/message-list.h>

#include <calendar/gui/e-calendar-view.h>
#include <calendar/gui/gnome-cal.h>

#include "gw-ui.h"

gboolean gw_ui_mail_folder_popup (GtkUIManager *ui_manager, EShellView *shell_view);
gboolean gw_ui_mail_message_popup (GtkUIManager *ui_manager, EShellView *shell_view);
gboolean gw_ui_calendar_event_popup (GtkUIManager *ui_manager, EShellView *shell_view);

static gboolean
is_in_gw_account (EShellView *shell_view, gboolean *is_on_store, gchar **folder_full_name)
{
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean is_store = FALSE, res;
	gchar *uri = NULL;
	gchar *full_name = NULL;

	if (folder_full_name)
		*folder_full_name = NULL;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	g_return_val_if_fail (folder_tree != NULL, FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	g_return_val_if_fail (selection != NULL, FALSE);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (model, &iter,
		COL_STRING_FULL_NAME, &full_name,
		COL_STRING_URI, &uri,
		COL_BOOL_IS_STORE, &is_store,
		-1);

	res = uri && g_ascii_strncasecmp (uri, "groupwise://", 12) == 0;

	if (is_on_store)
		*is_on_store = is_store;

	if (!is_store) {
		if (folder_full_name)
			*folder_full_name = full_name;
		else
			g_free (full_name);

	} else {
		g_free (full_name);
	}

	g_free (uri);

	return res;
}

static void
visible_actions (GtkActionGroup *action_group, gboolean visible, const GtkActionEntry *entries, guint n_entries)
{
	gint i;

	g_return_if_fail (action_group != NULL);
	g_return_if_fail (entries != NULL);

	for (i = 0; i < n_entries; i++) {
		GtkAction *action = gtk_action_group_get_action (action_group, entries[i].name);

		g_return_if_fail (action != NULL);

		gtk_action_set_visible (action, visible);
	}
}

static GtkActionEntry mfp_entries[] = {
	{ "gw-new-shared-folder",
	  "folder-new",
	  N_("New _Shared Folder..."),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_new_shared_folder_cb) },

	{ "gw-proxy-login",
	  NULL,
	  N_("_Proxy Login..."),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_proxy_login_cb) }
};

static void
update_mfp_entries_cb (EShellView *shell_view, gpointer user_data)
{
	GtkActionGroup *action_group;
	EShellWindow *shell_window;
	gboolean is_on_store = FALSE, visible;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "mail");

	visible = is_in_gw_account (shell_view, &is_on_store, NULL);
	visible_actions (action_group, visible, mfp_entries, G_N_ELEMENTS (mfp_entries));

	if (visible && !is_on_store) {
		GtkAction *action = gtk_action_group_get_action (action_group, "gw-proxy-login");

		g_return_if_fail (action != NULL);

		gtk_action_set_visible (action, FALSE);
	}
}

gboolean
gw_ui_mail_folder_popup (GtkUIManager *ui_manager, EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;

	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "mail");

	gtk_action_group_add_actions (
		action_group, mfp_entries,
		G_N_ELEMENTS (mfp_entries), shell_view);

	g_signal_connect (shell_view, "update-actions", G_CALLBACK (update_mfp_entries_cb), NULL);

	return TRUE;
}

static GtkActionEntry mmp_entries[] = {
	{ "gw-junk-mail-settings",
	  NULL,
	  N_("Junk Mail Settings..."),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_junk_mail_settings_cb) },

	{ "gw-track-message-status",
	  NULL,
	  N_("Track Message Status..."),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_track_message_status_cb) },
	{ "gw-retract-mail",
	  NULL,
	  N_("Retract Mail"),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_retract_mail_cb) }
};

static void
update_mmp_entries_cb (EShellView *shell_view, gpointer user_data)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	gboolean visible;
	gchar *full_name = NULL, *uri = NULL;
	guint n_selected = 0;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "mail");

	visible = is_in_gw_account (shell_view, NULL, &full_name);
	if (visible) {
		EShellContent *shell_content;
		EMailReader *reader;
		GPtrArray *uids;

		shell_content = e_shell_view_get_shell_content (shell_view);

		reader = E_MAIL_READER (shell_content);
		uids = e_mail_reader_get_selected_uids (reader);

		if (uids)
			n_selected = uids->len;

		em_utils_uids_free (uids);

		visible = n_selected > 0;
	}

	visible_actions (action_group, visible, mmp_entries, G_N_ELEMENTS (mmp_entries));

	if (visible) {
		GtkAction *action;
		gboolean is_sent_items_folder = full_name && g_ascii_strncasecmp (full_name, "Sent Items", 10) == 0;

		action = gtk_action_group_get_action (action_group, "gw-track-message-status");
		g_return_if_fail (action != NULL);
		gtk_action_set_visible (action, is_sent_items_folder && n_selected == 1);

		action = gtk_action_group_get_action (action_group, "gw-retract-mail");
		g_return_if_fail (action != NULL);
		gtk_action_set_visible (action, is_sent_items_folder && n_selected == 1);
	}

	g_free (full_name);
	g_free (uri);
}

gboolean
gw_ui_mail_message_popup (GtkUIManager *ui_manager, EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;

	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "mail");

	gtk_action_group_add_actions (
		action_group, mmp_entries,
		G_N_ELEMENTS (mmp_entries), shell_view);

	g_signal_connect (shell_view, "update-actions", G_CALLBACK (update_mmp_entries_cb), NULL);

	return TRUE;
}

static icalproperty *
get_attendee_prop (icalcomponent *icalcomp, const gchar *address)
{
	icalproperty *prop;

	if (!(address && *address))
		return NULL;

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const gchar *attendee = icalproperty_get_attendee (prop);

		if (g_str_equal (itip_strip_mailto (attendee), address)) {
			return prop;
		}
	}

	return NULL;
}

static gboolean
needs_to_accept (icalcomponent *icalcomp, const gchar *user_email)
{
	icalproperty *prop;
	icalparameter *param;
	icalparameter_partstat status = ICAL_PARTSTAT_NONE;

	prop = get_attendee_prop (icalcomp, user_email);

	/* It might be a mailing list */
	if (!prop)
		return TRUE;
	param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
	if (param)
		status = icalparameter_get_partstat (param);

	if (status == ICAL_PARTSTAT_ACCEPTED || status == ICAL_PARTSTAT_TENTATIVE)
		return FALSE;

	return TRUE;
}

static gboolean
is_meeting_owner (ECalComponent *comp, ECal *client)
{
	ECalComponentOrganizer org;
	gchar *email = NULL;
	const gchar *strip = NULL;
	gboolean ret_val = FALSE;

	if (!(e_cal_component_has_attendees (comp) &&
				e_cal_get_save_schedules (client)))
		return ret_val;

	e_cal_component_get_organizer (comp, &org);
	strip = itip_strip_mailto (org.value);

	if (e_cal_get_cal_address (client, &email, NULL) && !g_ascii_strcasecmp (email, strip)) {
		ret_val = TRUE;
	}

	if (!ret_val)
		ret_val = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, strip) != NULL;

	g_free (email);
	return ret_val;
}

static GtkActionEntry cal_entries[] = {
	{ "gw-meeting-accept",
	  GTK_STOCK_APPLY,
	  N_("Accept"),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_meeting_accept_cb) },

	{ "gw-meeting-accept-tentative",
	  GTK_STOCK_DIALOG_QUESTION,
	  N_("Accept Tentatively"),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_meeting_accept_tentative_cb) },

	{ "gw-meeting-decline",
	  GTK_STOCK_CANCEL,
	  N_("Decline"),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_meeting_decline_cb) },

	{ "gw-resend-meeting",
	  GTK_STOCK_EDIT,
	  N_("Rese_nd Meeting..."),
	  NULL,
	  NULL,
	  G_CALLBACK (gw_resend_meeting_cb) }
};

static void
update_cal_entries_cb (EShellView *shell_view, gpointer user_data)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	gboolean visible = FALSE, is_unaccepted = FALSE, is_mtg_owner = FALSE;
	EShellContent *shell_content;
	GnomeCalendar *gcal = NULL;
	GnomeCalendarViewType view_type;
	ECalendarView *view;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	g_object_get (shell_content, "calendar", &gcal, NULL);

	view_type = gnome_calendar_get_view (gcal);
	view = gnome_calendar_get_calendar_view (gcal, view_type);

	if (view) {
		GList *selected;

		selected = e_calendar_view_get_selected_events (view);
		if (selected && selected->data) {
			ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
			const gchar *uri;

			uri = is_comp_data_valid (event) ? e_cal_get_uri (event->comp_data->client) : NULL;

			if (uri && g_ascii_strncasecmp (uri, "groupwise://", 12) == 0) {
				visible = e_cal_util_component_has_attendee (event->comp_data->icalcomp);
				if (visible) {
					ECalComponent *comp;

					comp = e_cal_component_new ();
					e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

					if (e_cal_get_static_capability (event->comp_data->client, CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING)) {
						gchar *user_email;

						user_email = itip_get_comp_attendee (comp, event->comp_data->client);

						is_unaccepted = needs_to_accept (event->comp_data->icalcomp, user_email);

						g_free (user_email);
					}

					is_mtg_owner = is_meeting_owner (comp, event->comp_data->client);

					g_object_unref (comp);
				}
			}
		}

		g_list_free (selected);
	}

	action_group = e_shell_window_get_action_group (shell_window, "calendar");
	visible_actions (action_group, visible, cal_entries, G_N_ELEMENTS (cal_entries));

	if (visible && !is_unaccepted) {
		GtkAction *action;

		action = gtk_action_group_get_action (action_group, "gw-meeting-accept");
		g_return_if_fail (action != NULL);
		gtk_action_set_visible (action, FALSE);

		action = gtk_action_group_get_action (action_group, "gw-meeting-accept-tentative");
		g_return_if_fail (action != NULL);
		gtk_action_set_visible (action, FALSE);
	}

	if (visible && !is_mtg_owner) {
		GtkAction *action;

		action = gtk_action_group_get_action (action_group, "gw-resend-meeting");
		g_return_if_fail (action != NULL);
		gtk_action_set_visible (action, FALSE);
	}
}

gboolean
gw_ui_calendar_event_popup (GtkUIManager *ui_manager, EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;

	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "calendar");

	gtk_action_group_add_actions (
		action_group, cal_entries,
		G_N_ELEMENTS (cal_entries), shell_view);

	g_signal_connect (shell_view, "update-actions", G_CALLBACK (update_cal_entries_cb), NULL);

	return TRUE;
}
