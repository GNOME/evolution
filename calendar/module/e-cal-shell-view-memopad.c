/*
 * e-cal-shell-view-memopad.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-cal-shell-view-private.h"

/* Much of this file is based on e-memo-shell-view-actions.c. */

static void
action_calendar_memopad_clipboard_copy_cb (GtkAction *action,
                                           ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	e_memo_table_copy_clipboard (memo_table);
}

static void
action_calendar_memopad_clipboard_cut_cb (GtkAction *action,
                                          ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	e_memo_table_cut_clipboard (memo_table);
}

static void
action_calendar_memopad_clipboard_paste_cb (GtkAction *action,
                                            ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	e_memo_table_paste_clipboard (memo_table);
}

static void
action_calendar_memopad_delete_cb (GtkAction *action,
                                   ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	e_cal_shell_view_memopad_set_status_message (
		cal_shell_view, _("Deleting selected memos..."), -1.0);
	e_memo_table_delete_selected (memo_table);
	e_cal_shell_view_memopad_set_status_message (
		cal_shell_view, NULL, -1.0);
}

static void
action_calendar_memopad_forward_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	icalcomponent *clone;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only forward the first selected memo. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);
	itip_send_comp (
		E_CAL_COMPONENT_METHOD_PUBLISH, comp,
		comp_data->client, NULL, NULL, NULL, TRUE);
	g_object_unref (comp);
}

static void
action_calendar_memopad_new_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECal *client;
	ECalComponent *comp;
	CompEditor *editor;
	GSList *list;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	client = comp_data->client;
	editor = memo_editor_new (client, shell, COMP_EDITOR_NEW_ITEM);
	comp = cal_comp_memo_new_with_defaults (client);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
	g_object_unref (client);
}

static void
action_calendar_memopad_open_cb (GtkAction *action,
                                 ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected memo. */
	e_cal_shell_view_memopad_open_memo (cal_shell_view, comp_data);
}

static void
action_calendar_memopad_open_url_cb (GtkAction *action,
                                     ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	icalproperty *prop;
	const gchar *uri;
	GSList *list;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the URI of the first selected memo. */
	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_URL_PROPERTY);
	g_return_if_fail (prop == NULL);

	uri = icalproperty_get_url (prop);
	e_show_uri (GTK_WINDOW (shell_window), uri);
}

static void
action_calendar_memopad_print_cb (GtkAction *action,
                                  ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	icalcomponent *clone;
	GtkPrintOperationAction print_action;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only print the first selected memo. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_cal_component_set_icalcomponent (comp, clone);
	print_comp (comp, comp_data->client, print_action);
	g_object_unref (comp);
}

static void
action_calendar_memopad_save_as_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EMemoTable *memo_table;
	ECalModelComponent *comp_data;
	GSList *list;
	gchar *filename;
	gchar *string;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	list = e_memo_table_get_selected (memo_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	filename = e_file_dialog_save (_("Save as..."), NULL);
	if (filename == NULL)
		return;

	/* XXX We only save the first selected memo. */
	string = e_cal_get_component_as_string (
		comp_data->client, comp_data->icalcomp);
	if (string == NULL) {
		g_warning ("Could not convert memo to a string.");
		return;
	}

	e_write_file_uri (filename, string);

	g_free (filename);
	g_free (string);
}

static GtkActionEntry calendar_memopad_entries[] = {

	{ "calendar-memopad-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy selected memo"),
	  G_CALLBACK (action_calendar_memopad_clipboard_copy_cb) },

	{ "calendar-memopad-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut selected memo"),
	  G_CALLBACK (action_calendar_memopad_clipboard_cut_cb) },

	{ "calendar-memopad-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste memo from the clipboard"),
	  G_CALLBACK (action_calendar_memopad_clipboard_paste_cb) },

	{ "calendar-memopad-delete",
	  GTK_STOCK_DELETE,
	  N_("_Delete Memo"),
	  NULL,
	  N_("Delete selected memos"),
	  G_CALLBACK (action_calendar_memopad_delete_cb) },

	{ "calendar-memopad-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_memopad_forward_cb) },

	{ "calendar-memopad-new",
	  "stock_insert-note",
	  N_("New _Memo"),
	  NULL,
	  N_("Create a new memo"),
	  G_CALLBACK (action_calendar_memopad_new_cb) },

	{ "calendar-memopad-open",
	  GTK_STOCK_OPEN,
	  N_("_Open Memo"),
	  NULL,
	  N_("View the selected memo"),
	  G_CALLBACK (action_calendar_memopad_open_cb) },

	{ "calendar-memopad-open-url",
	  "applications-internet",
	  N_("Open _Web Page"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_memopad_open_url_cb) },

	{ "calendar-memopad-save-as",
	  GTK_STOCK_SAVE_AS,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_memopad_save_as_cb) }
};

static GtkActionEntry lockdown_printing_entries[] = {

	{ "calendar-memopad-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the selected memo"),
	  G_CALLBACK (action_calendar_memopad_print_cb) }
};

void
e_cal_shell_view_memopad_actions_init (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	/* Calendar Actions */
	action_group = ACTION_GROUP (CALENDAR);
	gtk_action_group_add_actions (
		action_group, calendar_memopad_entries,
		G_N_ELEMENTS (calendar_memopad_entries), cal_shell_view);

	/* Lockdown Printing Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);
	gtk_action_group_add_actions (
		action_group, lockdown_printing_entries,
		G_N_ELEMENTS (lockdown_printing_entries), cal_shell_view);
}

void
e_cal_shell_view_memopad_actions_update (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EMemoTable *memo_table;
	ETable *table;
	GtkAction *action;
	GSList *list, *iter;
	const gchar *label;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gboolean sensitive;
	gint n_selected;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	memo_table = e_cal_shell_content_get_memo_table (cal_shell_content);

	table = e_memo_table_get_table (memo_table);
	n_selected = e_table_selected_count (table);

	list = e_memo_table_get_selected (memo_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		icalproperty *prop;
		gboolean read_only;

		e_cal_is_read_only (comp_data->client, &read_only, NULL);
		editable &= !read_only;

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_URL_PROPERTY);
		has_url |= (prop != NULL);
	}
	g_slist_free (list);

	action = ACTION (CALENDAR_MEMOPAD_CLIPBOARD_COPY);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_MEMOPAD_CLIPBOARD_CUT);
	sensitive = (n_selected > 0) && editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_MEMOPAD_CLIPBOARD_PASTE);
	sensitive = editable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_MEMOPAD_DELETE);
	sensitive = (n_selected > 0) && editable;
	gtk_action_set_sensitive (action, sensitive);
	label = ngettext ("Delete Memo", "Delete Memos", n_selected);
	g_object_set (action, "label", label, NULL);

	action = ACTION (CALENDAR_MEMOPAD_FORWARD);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_MEMOPAD_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_MEMOPAD_OPEN_URL);
	sensitive = (n_selected == 1) && has_url;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_MEMOPAD_PRINT);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_MEMOPAD_SAVE_AS);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);
}

void
e_cal_shell_view_memopad_open_memo (ECalShellView *cal_shell_view,
                                    ECalModelComponent *comp_data)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalComponent *comp;
	icalcomponent *clone;
	const gchar *uid;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	editor = comp_editor_find_instance (uid);

	if (editor != NULL)
		goto exit;

	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	if (e_cal_component_has_organizer (comp))
		flags |= COMP_EDITOR_IS_SHARED;

	if (itip_organizer_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	editor = memo_editor_new (comp_data->client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_unref (comp);

exit:
	gtk_window_present (GTK_WINDOW (editor));
}

void
e_cal_shell_view_memopad_set_status_message (ECalShellView *cal_shell_view,
                                             const gchar *status_message,
                                             gdouble percent)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	activity = cal_shell_view->priv->memopad_activity;

	if (status_message == NULL || *status_message == '\0') {
		if (activity != NULL) {
			e_activity_complete (activity);
			g_object_unref (activity);
			activity = NULL;
		}

	} else if (activity == NULL) {
		activity = e_activity_new (status_message);
		e_activity_set_percent (activity, percent);
		e_shell_backend_add_activity (shell_backend, activity);

	} else {
		e_activity_set_percent (activity, percent);
		e_activity_set_primary_text (activity, status_message);
	}

	cal_shell_view->priv->memopad_activity = activity;
}
