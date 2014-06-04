/*
 * e-task-shell-content.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-task-shell-content.h"

#include <glib/gi18n.h>

#include "shell/e-shell-utils.h"

#include "calendar/gui/comp-util.h"
#include "calendar/gui/e-cal-component-preview.h"
#include "calendar/gui/e-cal-model-tasks.h"

#define E_TASK_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TASK_SHELL_CONTENT, ETaskShellContentPrivate))

struct _ETaskShellContentPrivate {
	GtkWidget *paned;
	GtkWidget *task_table;
	GtkWidget *preview_pane;

	ECalModel *task_model;
	GtkOrientation orientation;

	gchar *current_uid;

	guint preview_visible : 1;
};

enum {
	PROP_0,
	PROP_MODEL,
	PROP_ORIENTATION,
	PROP_PREVIEW_VISIBLE
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	ETaskShellContent,
	e_task_shell_content,
	E_TYPE_SHELL_CONTENT,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		GTK_TYPE_ORIENTABLE, NULL))

static void
task_shell_content_display_view_cb (ETaskShellContent *task_shell_content,
                                    GalView *gal_view)
{
	ETaskTable *task_table;

	if (!GAL_IS_VIEW_ETABLE (gal_view))
		return;

	task_table = e_task_shell_content_get_task_table (task_shell_content);

	gal_view_etable_attach_table (
		GAL_VIEW_ETABLE (gal_view), E_TABLE (task_table));
}

static void
task_shell_content_table_foreach_cb (gint model_row,
                                     gpointer user_data)
{
	ECalModelComponent *comp_data;
	icalcomponent *clone;
	icalcomponent *vcal;
	gchar *string;

	struct {
		ECalModel *model;
		GSList *list;
	} *foreach_data = user_data;

	comp_data = e_cal_model_get_component_at (
		foreach_data->model, model_row);

	vcal = e_cal_util_new_top_level ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_util_add_timezones_from_component (vcal, comp_data->icalcomp);
	icalcomponent_add_component (vcal, clone);

	/* String is owned by libical; do not free. */
	string = icalcomponent_as_ical_string (vcal);
	if (string != NULL) {
		ESource *source;
		const gchar *source_uid;

		source = e_client_get_source (E_CLIENT (comp_data->client));
		source_uid = e_source_get_uid (source);

		foreach_data->list = g_slist_prepend (
			foreach_data->list,
			g_strdup_printf ("%s\n%s", source_uid, string));
	}

	icalcomponent_free (vcal);
}

static void
task_shell_content_table_drag_data_get_cb (ETaskShellContent *task_shell_content,
                                           gint row,
                                           gint col,
                                           GdkDragContext *context,
                                           GtkSelectionData *selection_data,
                                           guint info,
                                           guint time)
{
	ETaskTable *task_table;
	GdkAtom target;

	struct {
		ECalModel *model;
		GSList *list;
	} foreach_data;

	/* Sanity check the selection target. */
	target = gtk_selection_data_get_target (selection_data);
	if (!e_targets_include_calendar (&target, 1))
		return;

	task_table = e_task_shell_content_get_task_table (task_shell_content);

	foreach_data.model = e_task_table_get_model (task_table);
	foreach_data.list = NULL;

	e_table_selected_row_foreach (
		E_TABLE (task_table),
		task_shell_content_table_foreach_cb,
		&foreach_data);

	if (foreach_data.list != NULL) {
		cal_comp_selection_set_string_list (
			selection_data, foreach_data.list);
		g_slist_foreach (foreach_data.list, (GFunc) g_free, NULL);
		g_slist_free (foreach_data.list);
	}
}

static void
task_shell_content_table_drag_data_delete_cb (ETaskShellContent *task_shell_content,
                                              gint row,
                                              gint col,
                                              GdkDragContext *context)
{
	/* Moved components are deleted from source immediately when moved,
	 * because some of them can be part of destination source, and we
	 * don't want to delete not-moved tasks.  There is no such information
	 * which event has been moved and which not, so skip this method. */
}

static void
task_shell_content_cursor_change_cb (ETaskShellContent *task_shell_content,
                                     gint row,
                                     ETable *table)
{
	ECalComponentPreview *task_preview;
	ECalModel *task_model;
	ECalModelComponent *comp_data;
	EPreviewPane *preview_pane;
	EWebView *web_view;
	const gchar *uid;

	task_model = e_task_shell_content_get_task_model (task_shell_content);
	preview_pane = e_task_shell_content_get_preview_pane (task_shell_content);

	web_view = e_preview_pane_get_web_view (preview_pane);
	task_preview = E_CAL_COMPONENT_PREVIEW (web_view);

	if (e_table_selected_count (table) != 1) {
		if (task_shell_content->priv->preview_visible)
			e_cal_component_preview_clear (task_preview);
		return;
	}

	row = e_table_get_cursor_row (table);
	comp_data = e_cal_model_get_component_at (task_model, row);

	if (task_shell_content->priv->preview_visible) {
		ECalComponent *comp;

		comp = e_cal_component_new_from_icalcomponent (
				icalcomponent_new_clone (comp_data->icalcomp));

		e_cal_component_preview_display (
			task_preview, comp_data->client, comp,
			e_cal_model_get_timezone (task_model),
			e_cal_model_get_use_24_hour_format (task_model));

		g_object_unref (comp);
	}

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	g_free (task_shell_content->priv->current_uid);
	task_shell_content->priv->current_uid = g_strdup (uid);
}

static void
task_shell_content_selection_change_cb (ETaskShellContent *task_shell_content,
                                        ETable *table)
{
	ECalComponentPreview *task_preview;
	EPreviewPane *preview_pane;
	EWebView *web_view;

	preview_pane = e_task_shell_content_get_preview_pane (task_shell_content);

	web_view = e_preview_pane_get_web_view (preview_pane);
	task_preview = E_CAL_COMPONENT_PREVIEW (web_view);

	if (e_table_selected_count (table) != 1)
		e_cal_component_preview_clear (task_preview);
}

static void
task_shell_content_model_row_changed_cb (ETaskShellContent *task_shell_content,
                                         gint row,
                                         ETableModel *model)
{
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	const gchar *current_uid;
	const gchar *uid;

	current_uid = task_shell_content->priv->current_uid;
	if (current_uid == NULL)
		return;

	comp_data = e_cal_model_get_component_at (E_CAL_MODEL (model), row);
	if (comp_data == NULL)
		return;

	uid = icalcomponent_get_uid (comp_data->icalcomp);
	if (g_strcmp0 (uid, current_uid) != 0)
		return;

	task_table = e_task_shell_content_get_task_table (task_shell_content);

	task_shell_content_cursor_change_cb (
		task_shell_content, 0, E_TABLE (task_table));
}

static void
task_shell_content_restore_state_cb (EShellWindow *shell_window,
                                     EShellView *shell_view,
                                     EShellContent *shell_content)
{
	ETaskShellContentPrivate *priv;
	GSettings *settings;

	priv = E_TASK_SHELL_CONTENT_GET_PRIVATE (shell_content);

	/* Bind GObject properties to settings keys. */

	settings = g_settings_new ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "task-hpane-position",
		priv->paned, "hposition",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "task-vpane-position",
		priv->paned, "vposition",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);
}

static void
task_shell_content_is_editing_changed_cb (ETaskTable *task_table,
                                          GParamSpec *param,
                                          EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	e_shell_view_update_actions (shell_view);
}

static GtkOrientation
task_shell_content_get_orientation (ETaskShellContent *task_shell_content)
{
	return task_shell_content->priv->orientation;
}

static void
task_shell_content_set_orientation (ETaskShellContent *task_shell_content,
                                    GtkOrientation orientation)
{
	if (task_shell_content->priv->orientation == orientation)
		return;

	task_shell_content->priv->orientation = orientation;

	g_object_notify (G_OBJECT (task_shell_content), "orientation");
}

static void
task_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ORIENTATION:
			task_shell_content_set_orientation (
				E_TASK_SHELL_CONTENT (object),
				g_value_get_enum (value));
			return;

		case PROP_PREVIEW_VISIBLE:
			e_task_shell_content_set_preview_visible (
				E_TASK_SHELL_CONTENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			g_value_set_object (
				value,
				e_task_shell_content_get_task_model (
				E_TASK_SHELL_CONTENT (object)));
			return;

		case PROP_ORIENTATION:
			g_value_set_enum (
				value,
				task_shell_content_get_orientation (
				E_TASK_SHELL_CONTENT (object)));
			return;

		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value,
				e_task_shell_content_get_preview_visible (
				E_TASK_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
task_shell_content_dispose (GObject *object)
{
	ETaskShellContentPrivate *priv;

	priv = E_TASK_SHELL_CONTENT_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->task_table != NULL) {
		g_object_unref (priv->task_table);
		priv->task_table = NULL;
	}

	if (priv->preview_pane != NULL) {
		g_object_unref (priv->preview_pane);
		priv->preview_pane = NULL;
	}

	if (priv->task_model != NULL) {
		g_object_unref (priv->task_model);
		priv->task_model = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_task_shell_content_parent_class)->dispose (object);
}

static void
task_shell_content_finalize (GObject *object)
{
	ETaskShellContentPrivate *priv;

	priv = E_TASK_SHELL_CONTENT_GET_PRIVATE (object);

	g_free (priv->current_uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_task_shell_content_parent_class)->finalize (object);
}

static void
task_shell_content_constructed (GObject *object)
{
	ETaskShellContentPrivate *priv;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellTaskbar *shell_taskbar;
	ESourceRegistry *registry;
	GalViewInstance *view_instance;
	GtkTargetList *target_list;
	GtkTargetEntry *targets;
	GtkWidget *container;
	GtkWidget *widget;
	gint n_targets;

	priv = E_TASK_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_task_shell_content_parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	priv->task_model = e_cal_model_tasks_new (registry);

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	g_object_bind_property (
		object, "orientation",
		widget, "orientation",
		G_BINDING_SYNC_CREATE);

	container = priv->paned;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	gtk_widget_show (widget);

	container = widget;

	widget = e_task_table_new (shell_view, priv->task_model);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->task_table = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->paned;

	widget = e_cal_component_preview_new ();
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "status-message",
		G_CALLBACK (e_shell_taskbar_set_message),
		shell_taskbar);

	widget = e_preview_pane_new (E_WEB_VIEW (widget));
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	priv->preview_pane = g_object_ref (widget);
	gtk_widget_show (widget);

	g_object_bind_property (
		object, "preview-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	targets = gtk_target_table_new_from_list (target_list, &n_targets);

	e_table_drag_source_set (
		E_TABLE (priv->task_table),
		GDK_BUTTON1_MASK, targets, n_targets,
		GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (target_list);

	g_signal_connect_swapped (
		priv->task_table, "table-drag-data-get",
		G_CALLBACK (task_shell_content_table_drag_data_get_cb),
		object);

	g_signal_connect_swapped (
		priv->task_table, "table-drag-data-delete",
		G_CALLBACK (task_shell_content_table_drag_data_delete_cb),
		object);

	g_signal_connect_swapped (
		priv->task_table, "cursor-change",
		G_CALLBACK (task_shell_content_cursor_change_cb),
		object);

	g_signal_connect_swapped (
		priv->task_table, "selection-change",
		G_CALLBACK (task_shell_content_selection_change_cb),
		object);

	e_signal_connect_notify (
		priv->task_table, "notify::is-editing",
		G_CALLBACK (task_shell_content_is_editing_changed_cb), shell_view);

	g_signal_connect_swapped (
		priv->task_model, "model-row-changed",
		G_CALLBACK (task_shell_content_model_row_changed_cb),
		object);

	/* Load the view instance. */

	view_instance = e_shell_view_new_view_instance (shell_view, NULL);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (task_shell_content_display_view_cb),
		object);
	e_shell_view_set_view_instance (shell_view, view_instance);
	gal_view_instance_load (view_instance);
	g_object_unref (view_instance);

	/* Restore pane positions from the last session once
	 * the shell view is fully initialized and visible. */
	g_signal_connect (
		shell_window, "shell-view-created::tasks",
		G_CALLBACK (task_shell_content_restore_state_cb),
		shell_content);
}

static guint32
task_shell_content_check_state (EShellContent *shell_content)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	GSList *list, *iter;
	gboolean assignable = TRUE;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gint n_selected;
	gint n_complete = 0;
	gint n_incomplete = 0;
	guint32 state = 0;

	task_shell_content = E_TASK_SHELL_CONTENT (shell_content);
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	n_selected = e_table_selected_count (E_TABLE (task_table));

	list = e_task_table_get_selected (task_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		icalproperty *prop;
		const gchar *cap;
		gboolean read_only;

		read_only = e_client_is_readonly (E_CLIENT (comp_data->client));
		editable &= !read_only;

		cap = CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT;
		if (e_client_check_capability (E_CLIENT (comp_data->client), cap))
			assignable = FALSE;

		cap = CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK;
		if (e_client_check_capability (E_CLIENT (comp_data->client), cap))
			assignable = FALSE;

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_URL_PROPERTY);
		has_url |= (prop != NULL);

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
		if (prop != NULL)
			n_complete++;
		else
			n_incomplete++;
	}
	g_slist_free (list);

	if (n_selected == 1)
		state |= E_TASK_SHELL_CONTENT_SELECTION_SINGLE;
	if (n_selected > 1)
		state |= E_TASK_SHELL_CONTENT_SELECTION_MULTIPLE;
	if (assignable)
		state |= E_TASK_SHELL_CONTENT_SELECTION_CAN_ASSIGN;
	if (editable)
		state |= E_TASK_SHELL_CONTENT_SELECTION_CAN_EDIT;
	if (n_complete > 0)
		state |= E_TASK_SHELL_CONTENT_SELECTION_HAS_COMPLETE;
	if (n_incomplete > 0)
		state |= E_TASK_SHELL_CONTENT_SELECTION_HAS_INCOMPLETE;
	if (has_url)
		state |= E_TASK_SHELL_CONTENT_SELECTION_HAS_URL;

	return state;
}

static void
task_shell_content_focus_search_results (EShellContent *shell_content)
{
	ETaskShellContentPrivate *priv;

	priv = E_TASK_SHELL_CONTENT_GET_PRIVATE (shell_content);

	gtk_widget_grab_focus (priv->task_table);
}

static void
e_task_shell_content_class_init (ETaskShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	g_type_class_add_private (class, sizeof (ETaskShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = task_shell_content_set_property;
	object_class->get_property = task_shell_content_get_property;
	object_class->dispose = task_shell_content_dispose;
	object_class->finalize = task_shell_content_finalize;
	object_class->constructed = task_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = task_shell_content_check_state;
	shell_content_class->focus_search_results =
		task_shell_content_focus_search_results;

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			"The task table model",
			E_TYPE_CAL_MODEL,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			"Preview is Visible",
			"Whether the preview pane is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_override_property (
		object_class, PROP_ORIENTATION, "orientation");
}

static void
e_task_shell_content_class_finalize (ETaskShellContentClass *class)
{
}

static void
e_task_shell_content_init (ETaskShellContent *task_shell_content)
{
	task_shell_content->priv =
		E_TASK_SHELL_CONTENT_GET_PRIVATE (task_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

void
e_task_shell_content_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_task_shell_content_register_type (type_module);
}

GtkWidget *
e_task_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_TASK_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

ECalModel *
e_task_shell_content_get_task_model (ETaskShellContent *task_shell_content)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_CONTENT (task_shell_content), NULL);

	return task_shell_content->priv->task_model;
}

ETaskTable *
e_task_shell_content_get_task_table (ETaskShellContent *task_shell_content)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_CONTENT (task_shell_content), NULL);

	return E_TASK_TABLE (task_shell_content->priv->task_table);
}

EPreviewPane *
e_task_shell_content_get_preview_pane (ETaskShellContent *task_shell_content)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_CONTENT (task_shell_content), NULL);

	return E_PREVIEW_PANE (task_shell_content->priv->preview_pane);
}

gboolean
e_task_shell_content_get_preview_visible (ETaskShellContent *task_shell_content)
{
	g_return_val_if_fail (
		E_IS_TASK_SHELL_CONTENT (task_shell_content), FALSE);

	return task_shell_content->priv->preview_visible;
}

void
e_task_shell_content_set_preview_visible (ETaskShellContent *task_shell_content,
                                          gboolean preview_visible)
{
	g_return_if_fail (E_IS_TASK_SHELL_CONTENT (task_shell_content));

	if (task_shell_content->priv->preview_visible == preview_visible)
		return;

	task_shell_content->priv->preview_visible = preview_visible;

	if (preview_visible && task_shell_content->priv->preview_pane) {
		task_shell_content_cursor_change_cb (
			task_shell_content, 0,
			E_TABLE (task_shell_content->priv->task_table));
	}

	g_object_notify (G_OBJECT (task_shell_content), "preview-visible");
}

EShellSearchbar *
e_task_shell_content_get_searchbar (ETaskShellContent *task_shell_content)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GtkWidget *widget;

	g_return_val_if_fail (
		E_IS_TASK_SHELL_CONTENT (task_shell_content), NULL);

	shell_content = E_SHELL_CONTENT (task_shell_content);
	shell_view = e_shell_content_get_shell_view (shell_content);
	widget = e_shell_view_get_searchbar (shell_view);

	return E_SHELL_SEARCHBAR (widget);
}

