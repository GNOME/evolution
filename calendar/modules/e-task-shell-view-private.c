/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-shell-view-private.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-task-shell-view-private.h"

#include <widgets/menus/gal-view-factory-etable.h>

static void
task_shell_view_backend_died_cb (ETaskShellView *task_shell_view,
                                 ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	source = e_cal_get_source (client);
	uid = e_source_peek_uid (source);

	g_object_ref (source);

	g_hash_table_remove (client_table, uid);
	e_task_shell_view_set_status_message (task_shell_view, NULL);

	e_error_run (
		GTK_WINDOW (shell_window),
		"calendar:tasks-crashed", NULL);

	g_object_unref (source);
}

static void
task_shell_view_backend_error_cb (ETaskShellView *task_shell_view,
                                  const gchar *message,
                                  ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkWidget *dialog;
	const gchar *uri;
	gchar *uri_no_passwd;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	uri = e_cal_get_uri (client);
	uri_no_passwd = get_uri_without_password (uri);

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		_("Error on %s:\n%s"),
		uri_no_passwd, message);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (uri_no_passwd);
}

static void
task_shell_view_client_opened_cb (ETaskShellView *task_shell_view,
                                  ECalendarStatus status,
                                  ECal *client)
{
	/* FIXME */
}

static gboolean
task_shell_view_add_source (ETaskShellView *task_shell_view,
                            ESource *source)
{
	ETaskShellSidebar *task_shell_sidebar;
	ESourceSelector *selector;
	GHashTable *client_table;
	ECal *default_client;
	ECal *client;
	const gchar *uid;
	const gchar *uri;
	gchar *status_message;

	client_table = task_shell_view->priv->client_table;
	default_client = task_shell_view->priv->default_client;

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client != NULL)
		return TRUE;

	if (default_client != NULL) {
		ESource *default_source;
		const gchar *default_uid;

		default_source = e_cal_get_source (default_client);
		default_uid = e_source_peek_uid (default_source);

		if (strcmp (uid, default_uid) == 0)
			client = g_object_ref (default_client);
	}

	if (client == NULL)
		client = auth_new_cal_from_source (
			source, E_CAL_SOURCE_TYPE_TODO);

	if (client == NULL)
		return FALSE;

	g_signal_connect_swapped (
		client, "backend-died",
		G_CALLBACK (task_shell_view_backend_died_cb),
		task_shell_view);

	g_signal_connect_swapped (
		client, "backend-error",
		G_CALLBACK (task_shell_view_backend_error_cb),
		task_shell_view);

	g_hash_table_insert (client_table, g_strdup (uid), client);

	uri = e_cal_get_uri (client);

	status_message = g_strdup_printf (_("Opening tasks at %s"), uri);
	e_task_shell_view_set_status_message (task_shell_view, status_message);
	g_free (status_message);

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	selector = e_task_shell_sidebar_get_selector (task_shell_sidebar);
	e_source_selector_select_source (selector, source);

	g_signal_connect_swapped (
		client, "cal-opened",
		G_CALLBACK (task_shell_view_client_opened_cb),
		task_shell_view);

	e_cal_open_async (client, FALSE);

	return TRUE;
}

static void
task_shell_view_table_popup_event_cb (ETaskShellView *task_shell_view,
                                      GdkEventButton *event)
{
	EShellView *shell_view;
	const gchar *widget_path;

	shell_view = E_SHELL_VIEW (task_shell_view);
	widget_path = "/task-popup";

	e_task_shell_view_actions_update (task_shell_view);
	e_shell_view_show_popup_menu (shell_view, widget_path, event);
}

static void
task_shell_view_table_user_created (ETaskShellView *task_shell_view,
                                    ETaskTable *task_table)
{
	ECal *client;
	ESource *source;

	if (task_table->user_created_cal != NULL)
		client = task_table->user_created_cal;
	else {
		ECalModel *model;

		model = e_task_table_get_model (task_table);
		client = e_cal_model_get_default_client (model);
	}

	source = e_cal_get_source (client);
	task_shell_view_add_source (task_shell_view, source);
}

static void
task_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for tasks");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
task_shell_view_notify_view_id_cb (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	GalViewInstance *view_instance;
	const gchar *view_id;

	task_shell_content = task_shell_view->priv->task_shell_content;
	view_instance =
		e_task_shell_content_get_view_instance (task_shell_content);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (task_shell_view));

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

void
e_task_shell_view_private_init (ETaskShellView *task_shell_view,
                                EShellViewClass *shell_view_class)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	ESourceList *source_list;
	GHashTable *client_table;
	GObject *object;

	object = G_OBJECT (shell_view_class->type_module);
	source_list = g_object_get_data (object, "source-list");
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	client_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	priv->source_list = g_object_ref (source_list);
	priv->task_actions = gtk_action_group_new ("tasks");
	priv->client_table = client_table;

	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		task_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		task_shell_view, "notify::view-id",
		G_CALLBACK (task_shell_view_notify_view_id_cb), NULL);
}

void
e_task_shell_view_private_constructed (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	ETaskShellContent *task_shell_content;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	ETaskTable *task_table;
	ECalModel *model;
	ETable *table;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	/* Cache these to avoid lots of awkward casting. */
	priv->task_shell_content = g_object_ref (shell_content);
	priv->task_shell_sidebar = g_object_ref (shell_sidebar);

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_task_table_get_model (task_table);
	table = e_task_table_get_table (task_table);

	g_signal_connect_swapped (
		task_table, "open-component",
		G_CALLBACK (e_task_shell_view_open_task),
		task_shell_view);

	g_signal_connect_swapped (
		task_table, "popup-event",
		G_CALLBACK (task_shell_view_table_popup_event_cb),
		task_shell_view);

	g_signal_connect_swapped (
		task_table, "status-message",
		G_CALLBACK (e_task_shell_view_set_status_message),
		task_shell_view);

	g_signal_connect_swapped (
		task_table, "user-created",
		G_CALLBACK (task_shell_view_table_user_created_cb),
		task_shell_view);

	g_signal_connect_swapped (
		model, "model-changed",
		G_CALLBACK (e_task_shell_view_sidebar_update),
		task_shell_view);

	g_signal_connect_swapped (
		model, "model-rows-deleted",
		G_CALLBACK (e_task_shell_view_sidebar_update),
		task_shell_view);

	g_signal_connect_swapped (
		model, "model-rows-inserted",
		G_CALLBACK (e_task_shell_view_sidebar_update),
		task_shell_view);

	g_signal_connect_swapped (
		table, "selection-change",
		G_CALLBACK (e_task_shell_view_sidebar_update),
		task_shell_view);

	e_task_shell_view_actions_init (task_shell_view);
	e_task_shell_view_sidebar_update (task_shell_view);
}

void
e_task_shell_view_private_dispose (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;

	DISPOSE (priv->source_list);

	DISPOSE (priv->task_actions);

	DISPOSE (priv->task_shell_content);
	DISPOSE (priv->task_shell_sidebar);

	g_hash_table_remove_all (priv->client_table);
	DISPOSE (priv->default_client);

	if (task_shell_view->priv->activity != NULL) {
		/* XXX Activity is no cancellable. */
		e_activity_complete (task_shell_view->priv->activity);
		g_object_unref (task_shell_view->priv->activity);
		task_shell_view->priv->activity = NULL;
	}
}

void
e_task_shell_view_private_finalize (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;

	g_hash_table_destroy (priv->client_table);
}

void
e_task_shell_view_open_task (ETaskShellView *task_shell_view,
                             ECalModelComponent *comp_data)
{
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalComponent *comp;
	icalcomponent *clone;
	icalproperty *prop;
	const gchar *uid;

	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	editor = comp_editor_find_instance (uid);

	if (editor == NULL)
		goto exit;

	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (prop != NULL)
		flags |= COMP_EDITOR_IS_ASSIGNED;

	if (itip_organizer_is_user (comp, comp_data->client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!itip_organizer_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = task_editor_new (comp_data->client, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_ref (comp);

	if (flags & COMP_EDITOR_IS_ASSIGNED)
		task_editor_show_assignment (TASK_EDITOR (editor));

exit:
	gtk_window_present (GTK_WINDOW (editor));
}

void
e_task_shell_view_set_status_message (ETaskShellView *task_shell_view,
                                      const gchar *status_message)
{
	EActivity *activity;
	EShellView *shell_view;

	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));

	activity = task_shell_view->priv->activity;
	shell_view = E_SHELL_VIEW (task_shell_view);

	if (status_message == NULL || *status_message == '\0') {
		if (activity != NULL) {
			e_activity_complete (activity);
			g_object_unref (activity);
			activity = NULL;
		}

	} else if (activity == NULL) {
		activity = e_activity_new (status_message);
		e_shell_view_add_activity (shell_view, activity);

	} else
		e_activity_set_primary_text (activity, status_message);

	task_shell_view->priv->activity = activity;
}

void
e_task_shell_view_sidebar_update (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ETaskTable *task_table;
	ECalModel *model;
	ETable *table;
	GString *string;
	const gchar *format;
	gint n_rows;
	gint n_selected;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	model = e_task_table_get_model (task_table);
	table = e_task_table_get_table (task_table);

	n_rows = e_table_model_row_count (E_TABLE_MODEL (model));
	n_selected = e_table_selected_count (table);

	string = g_string_sized_new (64);

	format = ngettext ("%d task", "%d tasks", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, string->str);

	g_string_free (string, TRUE);
}
