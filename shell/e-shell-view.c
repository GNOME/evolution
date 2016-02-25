/*
 * e-shell-view.c
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

/**
 * SECTION: e-shell-view
 * @short_description: views within the main window
 * @include: shell/e-shell-view.h
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include "e-shell-searchbar.h"
#include "e-shell-window-actions.h"

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

#define SIMPLE_SEARCHBAR_WIDTH 300
#define STATE_SAVE_TIMEOUT_SECONDS 3

struct _EShellViewPrivate {
	GThread *main_thread; /* not referenced */

	gpointer shell_window;  /* weak pointer */

	GKeyFile *state_key_file;
	gpointer state_save_activity;  /* weak pointer */
	guint state_save_timeout_id;

	GalViewInstance *view_instance;
	gulong view_instance_changed_handler_id;
	gulong view_instance_loaded_handler_id;

	gchar *title;
	gchar *view_id;
	gint page_num;
	guint merge_id;

	GtkAction *action;
	GtkSizeGroup *size_group;
	GtkWidget *shell_content;
	GtkWidget *shell_sidebar;
	GtkWidget *shell_taskbar;
	GtkWidget *searchbar;

	EFilterRule *search_rule;
	guint execute_search_blocked;

	GtkWidget *preferences_window;
	gulong preferences_hide_handler_id;

	guint update_actions_idle_id;
};

enum {
	PROP_0,
	PROP_ACTION,
	PROP_PAGE_NUM,
	PROP_SEARCHBAR,
	PROP_SEARCH_RULE,
	PROP_SHELL_BACKEND,
	PROP_SHELL_CONTENT,
	PROP_SHELL_SIDEBAR,
	PROP_SHELL_TASKBAR,
	PROP_SHELL_WINDOW,
	PROP_STATE_KEY_FILE,
	PROP_TITLE,
	PROP_VIEW_ID,
	PROP_VIEW_INSTANCE
};

enum {
	TOGGLED,
	CLEAR_SEARCH,
	CUSTOM_SEARCH,
	EXECUTE_SEARCH,
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

static gpointer parent_class;
static gulong signals[LAST_SIGNAL];

static void
shell_view_init_search_context (EShellViewClass *class)
{
	EShellBackend *shell_backend;
	ERuleContext *search_context;
	const gchar *config_dir;
	gchar *system_filename;
	gchar *user_filename;

	shell_backend = class->shell_backend;

	/* Sanity check the class fields we need. */
	g_return_if_fail (class->search_rules != NULL);
	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));

	/* The basename for built-in searches is specified in the
	 * shell view class.  All built-in search rules live in the
	 * same directory. */
	system_filename = g_build_filename (
		EVOLUTION_RULEDIR, class->search_rules, NULL);

	/* The filename for custom saved searches is always of
	 * the form "$(shell_backend_config_dir)/searches.xml". */
	config_dir = e_shell_backend_get_config_dir (shell_backend);
	user_filename = g_build_filename (config_dir, "searches.xml", NULL);

	/* Create the search context instance.  Subclasses may override
	 * the GType so check that it's really an ERuleContext instance. */
	search_context = g_object_new (class->search_context_type, NULL);
	g_return_if_fail (E_IS_RULE_CONTEXT (search_context));
	class->search_context = search_context;

	e_rule_context_add_part_set (
		search_context, "partset", E_TYPE_FILTER_PART,
		e_rule_context_add_part, e_rule_context_next_part);
	e_rule_context_add_rule_set (
		search_context, "ruleset", E_TYPE_FILTER_RULE,
		e_rule_context_add_rule, e_rule_context_next_rule);
	e_rule_context_load (search_context, system_filename, user_filename);

	g_free (system_filename);
	g_free (user_filename);
}

static void
shell_view_init_view_collection (EShellViewClass *class)
{
	EShellBackend *shell_backend;
	const gchar *base_directory;
	const gchar *name;
	gchar *system_directory;
	gchar *user_directory;

	shell_backend = class->shell_backend;
	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));
	name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	base_directory = EVOLUTION_GALVIEWSDIR;
	system_directory = g_build_filename (base_directory, name, NULL);

	base_directory = e_shell_backend_get_config_dir (shell_backend);
	user_directory = g_build_filename (base_directory, "views", NULL);

	/* The view collection is never destroyed. */
	class->view_collection = gal_view_collection_new (
		system_directory, user_directory);

	g_free (system_directory);
	g_free (user_directory);
}

static void
shell_view_update_view_id (EShellView *shell_view,
                           GalViewInstance *view_instance)
{
	gchar *view_id;

	view_id = gal_view_instance_get_current_view_id (view_instance);
	e_shell_view_set_view_id (shell_view, view_id);
	g_free (view_id);
}

static void
shell_view_load_state (EShellView *shell_view)
{
	EShellBackend *shell_backend;
	GKeyFile *key_file;
	const gchar *config_dir;
	gchar *filename;
	GError *error = NULL;

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	config_dir = e_shell_backend_get_config_dir (shell_backend);
	filename = g_build_filename (config_dir, "state.ini", NULL);

	/* XXX Should do this asynchronously. */
	key_file = shell_view->priv->state_key_file;
	g_key_file_load_from_file (key_file, filename, 0, &error);

	if (error == NULL)
		goto exit;

	if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
		g_warning ("%s", error->message);

	g_error_free (error);

exit:
	g_free (filename);
}

typedef struct {
	EShellView *shell_view;
	gchar *contents;
} SaveStateData;

static void
shell_view_save_state_done_cb (GFile *file,
                               GAsyncResult *result,
                               SaveStateData *data)
{
	GError *error = NULL;

	e_file_replace_contents_finish (file, result, NULL, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_object_unref (data->shell_view);
	g_free (data->contents);
	g_slice_free (SaveStateData, data);
}

static EActivity *
shell_view_save_state (EShellView *shell_view,
                       gboolean immediately)
{
	EShellBackend *shell_backend;
	SaveStateData *data;
	EActivity *activity;
	GKeyFile *key_file;
	GFile *file;
	const gchar *config_dir;
	gchar *contents;
	gchar *path;

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	config_dir = e_shell_backend_get_config_dir (shell_backend);
	key_file = shell_view->priv->state_key_file;

	contents = g_key_file_to_data (key_file, NULL, NULL);
	g_return_val_if_fail (contents != NULL, NULL);

	path = g_build_filename (config_dir, "state.ini", NULL);
	if (immediately) {
		g_file_set_contents (path, contents, -1, NULL);

		g_free (path);
		g_free (contents);

		return NULL;
	}

	file = g_file_new_for_path (path);
	g_free (path);

	/* GIO does not copy the contents string, so we need to keep
	 * it in memory until saving is complete.  We reference the
	 * shell view to keep it from being finalized while saving. */
	data = g_slice_new (SaveStateData);
	data->shell_view = g_object_ref (shell_view);
	data->contents = contents;

	/* The returned activity is a borrowed reference. */
	activity = e_file_replace_contents_async (
		file, contents, strlen (contents), NULL,
		FALSE, G_FILE_CREATE_PRIVATE, (GAsyncReadyCallback)
		shell_view_save_state_done_cb, data);

	e_activity_set_text (
		activity, (_("Saving user interface state")));

	e_shell_backend_add_activity (shell_backend, activity);

	g_object_unref (file);

	return activity;
}

static gboolean
shell_view_state_timeout_cb (gpointer user_data)
{
	EShellView *shell_view;
	EActivity *activity;

	shell_view = E_SHELL_VIEW (user_data);

	/* If a save is still in progress, check back later. */
	if (shell_view->priv->state_save_activity != NULL)
		return TRUE;

	activity = shell_view_save_state (shell_view, FALSE);

	/* Set up a weak pointer that gets set to NULL when the
	 * activity finishes.  This will tell us if we're still
	 * busy saving state data to disk on the next timeout. */
	shell_view->priv->state_save_activity = activity;
	g_object_add_weak_pointer (
		G_OBJECT (shell_view->priv->state_save_activity),
		&shell_view->priv->state_save_activity);

	shell_view->priv->state_save_timeout_id = 0;

	return FALSE;
}

static void
shell_view_emit_toggled (EShellView *shell_view)
{
	g_signal_emit (shell_view, signals[TOGGLED], 0);
}

static void
shell_view_set_action (EShellView *shell_view,
                       GtkAction *action)
{
	gchar *label;

	g_return_if_fail (shell_view->priv->action == NULL);

	shell_view->priv->action = g_object_ref (action);

	g_object_get (action, "label", &label, NULL);
	e_shell_view_set_title (shell_view, label);
	g_free (label);

	g_signal_connect_swapped (
		action, "toggled",
		G_CALLBACK (shell_view_emit_toggled), shell_view);
}

static void
shell_view_set_shell_window (EShellView *shell_view,
                             EShellWindow *shell_window)
{
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));
	g_return_if_fail (shell_view->priv->shell_window == NULL);

	shell_view->priv->shell_window = shell_window;

	g_object_add_weak_pointer (
		G_OBJECT (shell_window),
		&shell_view->priv->shell_window);
}

static void
shell_view_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTION:
			shell_view_set_action (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_PAGE_NUM:
			e_shell_view_set_page_num (
				E_SHELL_VIEW (object),
				g_value_get_int (value));
			return;

		case PROP_SEARCH_RULE:
			e_shell_view_set_search_rule (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SHELL_WINDOW:
			shell_view_set_shell_window (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_TITLE:
			e_shell_view_set_title (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_VIEW_ID:
			e_shell_view_set_view_id (
				E_SHELL_VIEW (object),
				g_value_get_string (value));
			return;

		case PROP_VIEW_INSTANCE:
			e_shell_view_set_view_instance (
				E_SHELL_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTION:
			g_value_set_object (
				value, e_shell_view_get_action (
				E_SHELL_VIEW (object)));
			return;

		case PROP_PAGE_NUM:
			g_value_set_int (
				value, e_shell_view_get_page_num (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SEARCHBAR:
			g_value_set_object (
				value, e_shell_view_get_searchbar (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SEARCH_RULE:
			g_value_set_object (
				value, e_shell_view_get_search_rule (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_BACKEND:
			g_value_set_object (
				value, e_shell_view_get_shell_backend (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_CONTENT:
			g_value_set_object (
				value, e_shell_view_get_shell_content (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_SIDEBAR:
			g_value_set_object (
				value, e_shell_view_get_shell_sidebar (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_TASKBAR:
			g_value_set_object (
				value, e_shell_view_get_shell_taskbar (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SHELL_WINDOW:
			g_value_set_object (
				value, e_shell_view_get_shell_window (
				E_SHELL_VIEW (object)));
			return;

		case PROP_STATE_KEY_FILE:
			g_value_set_pointer (
				value, e_shell_view_get_state_key_file (
				E_SHELL_VIEW (object)));
			return;

		case PROP_TITLE:
			g_value_set_string (
				value, e_shell_view_get_title (
				E_SHELL_VIEW (object)));
			return;

		case PROP_VIEW_ID:
			g_value_set_string (
				value, e_shell_view_get_view_id (
				E_SHELL_VIEW (object)));
			return;

		case PROP_VIEW_INSTANCE:
			g_value_set_object (
				value, e_shell_view_get_view_instance (
				E_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_dispose (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	/* Expedite any pending state saves. */
	if (priv->state_save_timeout_id > 0) {
		g_source_remove (priv->state_save_timeout_id);
		priv->state_save_timeout_id = 0;
		if (priv->state_save_activity == NULL)
			shell_view_save_state (E_SHELL_VIEW (object), TRUE);
	}

	if (priv->update_actions_idle_id > 0) {
		g_source_remove (priv->update_actions_idle_id);
		priv->update_actions_idle_id = 0;
	}

	if (priv->state_save_activity != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->state_save_activity),
			&priv->state_save_activity);
		priv->state_save_activity = NULL;
	}

	if (priv->view_instance_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->view_instance,
			priv->view_instance_changed_handler_id);
		priv->view_instance_changed_handler_id = 0;
	}

	if (priv->view_instance_loaded_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->view_instance,
			priv->view_instance_loaded_handler_id);
		priv->view_instance_loaded_handler_id = 0;
	}

	if (priv->preferences_window != NULL) {
		g_signal_handler_disconnect (
			priv->preferences_window,
			priv->preferences_hide_handler_id);
		priv->preferences_hide_handler_id = 0;
	}

	if (priv->shell_window != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_window), &priv->shell_window);
		priv->shell_window = NULL;
	}

	g_clear_object (&priv->view_instance);
	g_clear_object (&priv->shell_content);
	g_clear_object (&priv->shell_sidebar);
	g_clear_object (&priv->shell_taskbar);
	g_clear_object (&priv->searchbar);
	g_clear_object (&priv->search_rule);
	g_clear_object (&priv->preferences_window);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_view_finalize (GObject *object)
{
	EShellViewPrivate *priv;

	priv = E_SHELL_VIEW_GET_PRIVATE (object);

	g_key_file_free (priv->state_key_file);

	g_free (priv->title);
	g_free (priv->view_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_view_constructed (GObject *object)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellViewClass *shell_view_class;
	GtkWidget *widget;
	gulong handler_id;

	shell_view = E_SHELL_VIEW (object);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	shell_view_load_state (shell_view);

	/* Invoke factory methods. */

	/* Create the taskbar widget first so the content and
	 * sidebar widgets can access it during construction. */
	widget = shell_view_class->new_shell_taskbar (shell_view);
	shell_view->priv->shell_taskbar = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = shell_view_class->new_shell_content (shell_view);
	shell_view->priv->shell_content = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = shell_view_class->new_shell_sidebar (shell_view);
	shell_view->priv->shell_sidebar = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	if (shell_view_class->construct_searchbar != NULL) {
		widget = shell_view_class->construct_searchbar (shell_view);
		shell_view->priv->searchbar = g_object_ref_sink (widget);
	}

	/* Size group should be safe to unreference now. */
	g_object_unref (shell_view->priv->size_group);
	shell_view->priv->size_group = NULL;

	/* Update actions whenever the Preferences window is closed. */
	widget = e_shell_get_preferences_window (shell);
	shell_view->priv->preferences_window = g_object_ref (widget);
	handler_id = g_signal_connect_swapped (
		shell_view->priv->preferences_window, "hide",
		G_CALLBACK (e_shell_view_update_actions_in_idle), shell_view);
	shell_view->priv->preferences_hide_handler_id = handler_id;

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);
}

static GtkWidget *
shell_view_construct_searchbar (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellViewClass *shell_view_class;
	GtkWidget *widget;

	shell_content = e_shell_view_get_shell_content (shell_view);

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	widget = shell_view_class->new_shell_searchbar (shell_view);
	e_shell_content_set_searchbar (shell_content, widget);
	gtk_widget_show (widget);

	return widget;
}

static gchar *
shell_view_get_search_name (EShellView *shell_view)
{
	EShellSearchbar *searchbar;
	EFilterRule *rule;
	const gchar *search_text;

	rule = e_shell_view_get_search_rule (shell_view);
	g_return_val_if_fail (E_IS_FILTER_RULE (rule), NULL);

	searchbar = E_SHELL_SEARCHBAR (shell_view->priv->searchbar);
	search_text = e_shell_searchbar_get_search_text (searchbar);

	if (search_text == NULL || *search_text == '\0')
		search_text = "''";

	return g_strdup_printf ("%s %s", rule->name, search_text);
}

static void
shell_view_toggled (EShellView *shell_view)
{
	EShellViewPrivate *priv = shell_view->priv;
	EShellViewClass *shell_view_class;
	EShellWindow *shell_window;
	GtkUIManager *ui_manager;
	const gchar *basename, *id;
	gboolean view_is_active;

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	view_is_active = e_shell_view_is_active (shell_view);
	basename = shell_view_class->ui_definition;
	id = shell_view_class->ui_manager_id;

	if (view_is_active && priv->merge_id == 0) {
		priv->merge_id = e_load_ui_manager_definition (
			ui_manager, basename);
		e_plugin_ui_enable_manager (ui_manager, id);
	} else if (!view_is_active && priv->merge_id != 0) {
		e_plugin_ui_disable_manager (ui_manager, id);
		gtk_ui_manager_remove_ui (ui_manager, priv->merge_id);
		gtk_ui_manager_ensure_update (ui_manager);
		priv->merge_id = 0;
	}

	gtk_ui_manager_ensure_update (ui_manager);
}

static void
shell_view_clear_search (EShellView *shell_view)
{
	e_shell_view_set_search_rule (shell_view, NULL);
	e_shell_view_execute_search (shell_view);
}

static void
shell_view_custom_search (EShellView *shell_view,
                          EFilterRule *custom_rule)
{
	e_shell_view_set_search_rule (shell_view, custom_rule);
	e_shell_view_execute_search (shell_view);
}

static void
shell_view_update_actions (EShellView *shell_view)
{
	EShellWindow *shell_window;
	EFocusTracker *focus_tracker;
	GtkAction *action;
	GtkActionGroup *action_group;

	g_return_if_fail (e_shell_view_is_active (shell_view));

	shell_window = e_shell_view_get_shell_window (shell_view);
	focus_tracker = e_shell_window_get_focus_tracker (shell_window);

	e_focus_tracker_update_actions (focus_tracker);

	action_group = E_SHELL_WINDOW_ACTION_GROUP_CUSTOM_RULES (shell_window);
	gtk_action_group_set_sensitive (action_group, TRUE);

	action = E_SHELL_WINDOW_ACTION_SEARCH_ADVANCED (shell_window);
	gtk_action_set_sensitive (action, TRUE);
}

static void
e_shell_view_class_init (EShellViewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_view_set_property;
	object_class->get_property = shell_view_get_property;
	object_class->dispose = shell_view_dispose;
	object_class->finalize = shell_view_finalize;
	object_class->constructed = shell_view_constructed;

	class->search_context_type = E_TYPE_RULE_CONTEXT;

	/* Default Factories */
	class->new_shell_content = e_shell_content_new;
	class->new_shell_sidebar = e_shell_sidebar_new;
	class->new_shell_taskbar = e_shell_taskbar_new;
	class->new_shell_searchbar = e_shell_searchbar_new;

	class->construct_searchbar = shell_view_construct_searchbar;
	class->get_search_name = shell_view_get_search_name;

	class->toggled = shell_view_toggled;
	class->clear_search = shell_view_clear_search;
	class->custom_search = shell_view_custom_search;
	class->update_actions = shell_view_update_actions;

	/**
	 * EShellView:action:
	 *
	 * The #GtkRadioAction registered with #EShellSwitcher.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ACTION,
		g_param_spec_object (
			"action",
			"Switcher Action",
			"The switcher action for this shell view",
			GTK_TYPE_RADIO_ACTION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:page-num
	 *
	 * The #GtkNotebook page number of the shell view.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_PAGE_NUM,
		g_param_spec_int (
			"page-num",
			"Page Number",
			"The notebook page number of the shell view",
			-1,
			G_MAXINT,
			-1,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:search-rule
	 *
	 * Criteria for the current search results.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SEARCH_RULE,
		g_param_spec_object (
			"search-rule",
			"Search Rule",
			"Criteria for the current search results",
			E_TYPE_FILTER_RULE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:shell-backend
	 *
	 * The #EShellBackend for this shell view.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_BACKEND,
		g_param_spec_object (
			"shell-backend",
			"Shell Backend",
			"The EShellBackend for this shell view",
			E_TYPE_SHELL_BACKEND,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:shell-content
	 *
	 * The content widget appears in an #EShellWindow<!-- -->'s
	 * right pane.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_CONTENT,
		g_param_spec_object (
			"shell-content",
			"Shell Content Widget",
			"The content widget appears in "
			"a shell window's right pane",
			E_TYPE_SHELL_CONTENT,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:shell-sidebar
	 *
	 * The sidebar widget appears in an #EShellWindow<!-- -->'s
	 * left pane.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_SIDEBAR,
		g_param_spec_object (
			"shell-sidebar",
			"Shell Sidebar Widget",
			"The sidebar widget appears in "
			"a shell window's left pane",
			E_TYPE_SHELL_SIDEBAR,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:shell-taskbar
	 *
	 * The taskbar widget appears at the bottom of an #EShellWindow.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_TASKBAR,
		g_param_spec_object (
			"shell-taskbar",
			"Shell Taskbar Widget",
			"The taskbar widget appears at "
			"the bottom of a shell window",
			E_TYPE_SHELL_TASKBAR,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:shell-window
	 *
	 * The #EShellWindow to which the shell view belongs.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_WINDOW,
		g_param_spec_object (
			"shell-window",
			"Shell Window",
			"The window to which the shell view belongs",
			E_TYPE_SHELL_WINDOW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:state-key-file
	 *
	 * The #GKeyFile holding widget state data.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_STATE_KEY_FILE,
		g_param_spec_pointer (
			"state-key-file",
			"State Key File",
			"The key file holding widget state data",
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:title
	 *
	 * The title of the shell view.  Also serves as the #EShellWindow
	 * title when the shell view is active.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TITLE,
		g_param_spec_string (
			"title",
			"Title",
			"The title of the shell view",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:view-id
	 *
	 * The current #GalView ID.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_VIEW_ID,
		g_param_spec_string (
			"view-id",
			"Current View ID",
			"The current GAL view ID",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:view-instance:
	 *
	 * The current #GalViewInstance.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_VIEW_INSTANCE,
		g_param_spec_object (
			"view-instance",
			"View Instance",
			"The current view instance",
			GAL_TYPE_VIEW_INSTANCE,
			G_PARAM_READWRITE));

	/**
	 * EShellView::toggled
	 * @shell_view: the #EShellView which emitted the signal
	 *
	 * Emitted when @shell_view is activated or deactivated.
	 * Use e_shell_view_is_active() to find out which event has
	 * occurred.  The shell view being deactivated is always
	 * notified before the shell view being activated.
	 *
	 * By default, #EShellView adds the UI definition file
	 * given in the <structfield>ui_definition</structfield>
	 * field of #EShellViewClass on activation, and removes the
	 * UI definition on deactivation.
	 **/
	signals[TOGGLED] = g_signal_new (
		"toggled",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellViewClass, toggled),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EShellView::clear-search
	 * @shell_view: the #EShellView which emitted the signal
	 *
	 * Clears the current search.  See e_shell_view_clear_search() for
	 * details.
	 **/
	signals[CLEAR_SEARCH] = g_signal_new (
		"clear-search",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EShellViewClass, clear_search),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EShellView::custom-search
	 * @shell_view: the #EShellView which emitted the signal
	 * @custom_rule: criteria for the custom search
	 *
	 * Emitted when an advanced or saved search is about to be executed.
	 * See e_shell_view_custom_search() for details.
	 **/
	signals[CUSTOM_SEARCH] = g_signal_new (
		"custom-search",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EShellViewClass, custom_search),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_FILTER_RULE);

	/**
	 * EShellView::execute-search
	 * @shell_view: the #EShellView which emitted the signal
	 *
	 * #EShellView subclasses should override the
	 * <structfield>execute_search</structfield> method in
	 * #EShellViewClass to execute the current search conditions.
	 **/
	signals[EXECUTE_SEARCH] = g_signal_new (
		"execute-search",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellViewClass, execute_search),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EShellView::update-actions
	 * @shell_view: the #EShellView which emitted the signal
	 *
	 * #EShellView subclasses should override the
	 * <structfield>update_actions</structfield> method in
	 * #EShellViewClass to update sensitivities, labels, or any
	 * other aspect of the #GtkAction<!-- -->s they have registered.
	 *
	 * Plugins can also connect to this signal to be notified
	 * when to update their own #GtkAction<!-- -->s.
	 **/
	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellViewClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_shell_view_init (EShellView *shell_view,
                   EShellViewClass *class)
{
	GtkSizeGroup *size_group;

	/* XXX Our use of GInstanceInitFunc's 'class' parameter
	 *     prevents us from using G_DEFINE_ABSTRACT_TYPE. */

	if (class->search_context == NULL)
		shell_view_init_search_context (class);

	if (class->view_collection == NULL)
		shell_view_init_view_collection (class);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	shell_view->priv = E_SHELL_VIEW_GET_PRIVATE (shell_view);
	shell_view->priv->main_thread = g_thread_self ();
	shell_view->priv->state_key_file = g_key_file_new ();
	shell_view->priv->size_group = size_group;
}

GType
e_shell_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) e_shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) e_shell_view_init,
			NULL   /* value_table */
		};

		const GInterfaceInfo extensible_info = {
			(GInterfaceInitFunc) NULL,
			(GInterfaceFinalizeFunc) NULL,
			NULL   /* interface_data */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EShellView",
			&type_info, G_TYPE_FLAG_ABSTRACT);

		g_type_add_interface_static (
			type, E_TYPE_EXTENSIBLE, &extensible_info);
	}

	return type;
}

/**
 * e_shell_view_get_name:
 * @shell_view: an #EShellView
 *
 * Returns the view name for @shell_view, which is also the name of
 * the corresponding #EShellBackend (see the <structfield>name</structfield>
 * field in #EShellBackendInfo).
 *
 * Returns: the view name for @shell_view
 **/
const gchar *
e_shell_view_get_name (EShellView *shell_view)
{
	GtkAction *action;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	action = e_shell_view_get_action (shell_view);

	/* Switcher actions have a secret "view-name" data value.
	 * This gets set in e_shell_window_create_switcher_actions(). */
	return g_object_get_data (G_OBJECT (action), "view-name");
}

/**
 * e_shell_view_get_action:
 * @shell_view: an #EShellView
 *
 * Returns the switcher action for @shell_view.
 *
 * An #EShellWindow creates a #GtkRadioAction for each registered subclass
 * of #EShellView.  This action gets passed to the #EShellSwitcher, which
 * displays a button that proxies the action.  The icon at the top of the
 * sidebar also proxies the action.  When @shell_view is active, the
 * action's icon becomes the #EShellWindow icon.
 *
 * Returns: the switcher action for @shell_view
 **/
GtkAction *
e_shell_view_get_action (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->action;
}

/**
 * e_shell_view_get_title:
 * @shell_view: an #EShellView
 *
 * Returns the title for @shell_view.  When @shell_view is active, the
 * shell view's title becomes the #EShellWindow title.
 *
 * Returns: the title for @shell_view
 **/
const gchar *
e_shell_view_get_title (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->title;
}

/**
 * e_shell_view_set_title:
 * @shell_view: an #EShellView
 * @title: a title for @shell_view
 *
 * Sets the title for @shell_view.  When @shell_view is active, the
 * shell view's title becomes the #EShellWindow title.
 **/
void
e_shell_view_set_title (EShellView *shell_view,
                        const gchar *title)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (title == NULL)
		title = E_SHELL_VIEW_GET_CLASS (shell_view)->label;

	if (g_strcmp0 (shell_view->priv->title, title) == 0)
		return;

	g_free (shell_view->priv->title);
	shell_view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (shell_view), "title");
}

/**
 * e_shell_view_get_view_id:
 * @shell_view: an #EShellView
 *
 * Returns the ID of the currently selected #GalView.
 *
 * #EShellView subclasses are responsible for keeping this property in
 * sync with their #GalViewInstance.  #EShellView itself just provides
 * a place to store the view ID, and emits a #GObject::notify signal
 * when the property changes.
 *
 * Returns: the ID of the current #GalView
 **/
const gchar *
e_shell_view_get_view_id (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->view_id;
}

/**
 * e_shell_view_set_view_id:
 * @shell_view: an #EShellView
 * @view_id: a #GalView ID
 *
 * Selects the #GalView whose ID is equal to @view_id.
 *
 * #EShellView subclasses are responsible for keeping this property in
 * sync with their #GalViewInstance.  #EShellView itself just provides
 * a place to store the view ID, and emits a #GObject::notify signal
 * when the property changes.
 **/
void
e_shell_view_set_view_id (EShellView *shell_view,
                          const gchar *view_id)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (g_strcmp0 (shell_view->priv->view_id, view_id) == 0)
		return;

	g_free (shell_view->priv->view_id);
	shell_view->priv->view_id = g_strdup (view_id);

	g_object_notify (G_OBJECT (shell_view), "view-id");
}

/**
 * e_shell_view_new_view_instance:
 * @shell_view: an #EShellView
 * @instance_id: a name for the #GalViewInstance
 *
 * Convenience function creates a new #GalViewInstance from the
 * #GalViewCollection in @shell_view's #EShellViewClass.
 *
 * Returns: a new #GalViewInstance
 **/
GalViewInstance *
e_shell_view_new_view_instance (EShellView *shell_view,
                                const gchar *instance_id)
{
	EShellViewClass *class;
	GalViewCollection *view_collection;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);

	view_collection = class->view_collection;

	return gal_view_instance_new (view_collection, instance_id);
}

/**
 * e_shell_view_get_view_instance:
 * @shell_view: an #EShellView
 *
 * Returns the current #GalViewInstance for @shell_view.
 *
 * #EShellView subclasses are responsible for creating and configuring a
 * #GalViewInstance and handing it off so the @shell_view can monitor it
 * and perform common actions on it.
 *
 * Returns: a #GalViewInstance, or %NULL
 **/
GalViewInstance *
e_shell_view_get_view_instance (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->view_instance;
}

/**
 * e_shell_view_set_view_instance:
 * @shell_view: an #EShellView
 * @view_instance: a #GalViewInstance, or %NULL
 *
 * Sets the current #GalViewInstance for @shell_view.
 *
 * #EShellView subclasses are responsible for creating and configuring a
 * #GalViewInstance and handing it off so the @shell_view can monitor it
 * and perform common actions on it.
 **/
void
e_shell_view_set_view_instance (EShellView *shell_view,
                                GalViewInstance *view_instance)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (view_instance != NULL) {
		g_return_if_fail (GAL_IS_VIEW_INSTANCE (view_instance));
		g_object_ref (view_instance);
	}

	if (shell_view->priv->view_instance_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			shell_view->priv->view_instance,
			shell_view->priv->view_instance_changed_handler_id);
		shell_view->priv->view_instance_changed_handler_id = 0;
	}

	if (shell_view->priv->view_instance_loaded_handler_id > 0) {
		g_signal_handler_disconnect (
			shell_view->priv->view_instance,
			shell_view->priv->view_instance_loaded_handler_id);
		shell_view->priv->view_instance_loaded_handler_id = 0;
	}

	g_clear_object (&shell_view->priv->view_instance);

	shell_view->priv->view_instance = view_instance;

	if (view_instance != NULL) {
		gulong handler_id;

		handler_id = g_signal_connect_swapped (
			view_instance, "changed",
			G_CALLBACK (shell_view_update_view_id), shell_view);
		shell_view->priv->view_instance_changed_handler_id = handler_id;

		handler_id = g_signal_connect_swapped (
			view_instance, "loaded",
			G_CALLBACK (shell_view_update_view_id), shell_view);
		shell_view->priv->view_instance_loaded_handler_id = handler_id;
	}

	g_object_notify (G_OBJECT (shell_view), "view-instance");
}

/**
 * e_shell_view_get_shell_window:
 * @shell_view: an #EShellView
 *
 * Returns the #EShellWindow to which @shell_view belongs.
 *
 * Returns: the #EShellWindow to which @shell_view belongs
 **/
EShellWindow *
e_shell_view_get_shell_window (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_WINDOW (shell_view->priv->shell_window);
}

/**
 * e_shell_view_is_active:
 * @shell_view: an #EShellView
 *
 * Returns %TRUE if @shell_view is active.  That is, if it's currently
 * visible in its #EShellWindow.  An #EShellWindow can only display one
 * shell view at a time.
 *
 * Technically this just checks the #GtkToggleAction:active property of
 * the shell view's switcher action.  See e_shell_view_get_action().
 *
 * Returns: %TRUE if @shell_view is active
 **/
gboolean
e_shell_view_is_active (EShellView *shell_view)
{
	GtkAction *action;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	action = e_shell_view_get_action (shell_view);

	return gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
}

/**
 * e_shell_view_get_page_num:
 * @shell_view: an #EShellView
 *
 * This function is only interesting to #EShellWindow.  It returns the
 * #GtkNotebook page number for @shell_view.  The rest of the application
 * should have no need for this.
 *
 * Returns: the notebook page number for @shell_view
 **/
gint
e_shell_view_get_page_num (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), -1);

	return shell_view->priv->page_num;
}

/**
 * e_shell_view_set_page_num:
 * @shell_view: an #EShellView
 * @page_num: a notebook page number
 *
 * This function is only interesting to #EShellWindow.  It sets the
 * #GtkNotebook page number for @shell_view.  The rest of the application
 * must never call this because it could mess up shell view switching.
 **/
void
e_shell_view_set_page_num (EShellView *shell_view,
                           gint page_num)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->page_num == page_num)
		return;

	shell_view->priv->page_num = page_num;

	g_object_notify (G_OBJECT (shell_view), "page-num");
}

/**
 * e_shell_view_get_search_name:
 * @shell_view: an #EShellView
 *
 * Returns a newly-allocated string containing a suitable name for the
 * current search criteria.  This is used as the suggested name in the
 * Save Search dialog.  Free the returned string with g_free().
 *
 * Returns: a name for the current search criteria
 **/
gchar *
e_shell_view_get_search_name (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (class->get_search_name != NULL, NULL);

	return class->get_search_name (shell_view);
}

/**
 * e_shell_view_get_search_rule:
 * @shell_view: an #EShellView
 *
 * Returns the search criteria used to generate the current search results.
 *
 * Returns: the current search criteria
 **/
EFilterRule *
e_shell_view_get_search_rule (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->search_rule;
}

/**
 * e_shell_view_get_searchbar:
 * @shell_view: an #EShellView
 *
 * Returns the searchbar widget for @shell_view.
 *
 * Returns: the searchbar widget for @shell_view
 **/
GtkWidget *
e_shell_view_get_searchbar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->searchbar;
}

/**
 * e_shell_view_set_search_rule:
 * @shell_view: an #EShellView
 * @search_rule: an #EFilterRule
 *
 * Sets the search criteria used to generate the current search results.
 * Note that this will not trigger a search.  e_shell_view_execute_search()
 * must be called explicitly.
 **/
void
e_shell_view_set_search_rule (EShellView *shell_view,
                              EFilterRule *search_rule)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->search_rule == search_rule)
		return;

	if (search_rule != NULL) {
		g_return_if_fail (E_IS_FILTER_RULE (search_rule));
		g_object_ref (search_rule);
	}

	if (shell_view->priv->search_rule != NULL)
		g_object_unref (shell_view->priv->search_rule);

	shell_view->priv->search_rule = search_rule;

	g_object_notify (G_OBJECT (shell_view), "search-rule");
}

/**
 * e_shell_view_get_search_query:
 * @shell_view: an #EShellView
 *
 * Converts the #EShellView:search-rule property to a newly-allocated
 * S-expression string.  If the #EShellView:search-rule property is %NULL
 * the function returns %NULL.
 *
 * Returns: an S-expression string, or %NULL
 **/
gchar *
e_shell_view_get_search_query (EShellView *shell_view)
{
	EFilterRule *rule;
	GString *string;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	rule = e_shell_view_get_search_rule (shell_view);
	if (rule == NULL)
		return NULL;

	string = g_string_sized_new (1024);
	e_filter_rule_build_code (rule, string);

	return g_string_free (string, FALSE);
}

/**
 * e_shell_view_get_size_group:
 * @shell_view: an #EShellView
 *
 * Returns a #GtkSizeGroup that #EShellContent and #EShellSidebar use
 * to keep the search bar and sidebar banner vertically aligned.  The
 * rest of the application should have no need for this.
 *
 * Note, this is only available during #EShellView construction.
 *
 * Returns: a #GtkSizeGroup for internal use
 **/
GtkSizeGroup *
e_shell_view_get_size_group (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->size_group;
}

/**
 * e_shell_view_get_shell_backend:
 * @shell_view: an #EShellView
 *
 * Returns the corresponding #EShellBackend for @shell_view.
 *
 * Returns: the corresponding #EShellBackend for @shell_view
 **/
EShellBackend *
e_shell_view_get_shell_backend (EShellView *shell_view)
{
	EShellViewClass *class;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (class->shell_backend != NULL, NULL);

	return class->shell_backend;
}

/**
 * e_shell_view_get_shell_content:
 * @shell_view: an #EShellView
 *
 * Returns the #EShellContent instance for @shell_view.
 *
 * By default, #EShellView creates a plain #EShellContent during
 * initialization.  But #EShellView subclasses can override the
 * <structfield>new_shell_content</structfield> factory method
 * in #EShellViewClass to create a custom #EShellContent.
 *
 * Returns: the #EShellContent instance for @shell_view
 **/
EShellContent *
e_shell_view_get_shell_content (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_CONTENT (shell_view->priv->shell_content);
}

/**
 * e_shell_view_get_shell_sidebar:
 * @shell_view: an #EShellView
 *
 * Returns the #EShellSidebar instance for @shell_view.
 *
 * By default, #EShellView creates a plain #EShellSidebar during
 * initialization.  But #EShellView subclasses can override the
 * <structfield>new_shell_sidebar</structfield> factory method
 * in #EShellViewClass to create a custom #EShellSidebar.
 *
 * Returns: the #EShellSidebar instance for @shell_view
 **/
EShellSidebar *
e_shell_view_get_shell_sidebar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_SIDEBAR (shell_view->priv->shell_sidebar);
}

/**
 * e_shell_view_get_shell_taskbar:
 * @shell_view: an #EShellView
 *
 * Returns the #EShellTaskbar instance for @shell_view.
 *
 * By default, #EShellView creates a plain #EShellTaskbar during
 * initialization.  But #EShellView subclasses can override the
 * <structfield>new_shell_taskbar</structfield> factory method
 * in #EShellViewClass to create a custom #EShellTaskbar.
 *
 * Returns: the #EShellTaskbar instance for @shell_view
 **/
EShellTaskbar *
e_shell_view_get_shell_taskbar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return E_SHELL_TASKBAR (shell_view->priv->shell_taskbar);
}

/**
 * e_shell_view_get_state_key_file:
 * @shell_view: an #EShellView
 *
 * Returns the #GKeyFile holding widget state data for @shell_view.
 *
 * Returns: the #GKeyFile for @shell_view
 **/
GKeyFile *
e_shell_view_get_state_key_file (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->state_key_file;
}

/**
 * e_shell_view_set_state_dirty:
 * @shell_view: an #EShellView
 *
 * Marks the widget state data as modified (or "dirty") and schedules it
 * to be saved to disk after a short delay.  The delay caps the frequency
 * of saving to disk.
 **/
void
e_shell_view_set_state_dirty (EShellView *shell_view)
{
	guint source_id;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	/* If a timeout is already scheduled, do nothing. */
	if (shell_view->priv->state_save_timeout_id > 0)
		return;

	source_id = e_named_timeout_add_seconds (
		STATE_SAVE_TIMEOUT_SECONDS,
		shell_view_state_timeout_cb, shell_view);

	shell_view->priv->state_save_timeout_id = source_id;
}

/**
 * e_shell_view_clear_search:
 * @shell_view: an #EShellView
 *
 * Emits the #EShellView::clear-search signal.
 *
 * The default method sets the #EShellView:search-rule property to
 * %NULL and then emits the #EShellView::execute-search signal.
 **/
void
e_shell_view_clear_search (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	g_signal_emit (shell_view, signals[CLEAR_SEARCH], 0);
}

/**
 * e_shell_view_custom_search:
 * @shell_view: an #EShellView
 * @custom_rule: an #EFilterRule
 *
 * Emits the #EShellView::custom-search signal to indicate an advanced
 * or saved search is about to be executed.
 *
 * The default method sets the #EShellView:search-rule property to
 * @custom_rule and then emits the #EShellView::execute-search signal.
 **/
void
e_shell_view_custom_search (EShellView *shell_view,
                            EFilterRule *custom_rule)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_FILTER_RULE (custom_rule));

	g_signal_emit (shell_view, signals[CUSTOM_SEARCH], 0, custom_rule);
}

/**
 * e_shell_view_execute_search:
 * @shell_view: an #EShellView
 *
 * Emits the #EShellView::execute-search signal.
 *
 * #EShellView subclasses should implement the
 * <structfield>execute_search</structfield> method in #EShellViewClass
 * to execute a search based on the current search conditions.
 **/
void
e_shell_view_execute_search (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (!e_shell_view_is_execute_search_blocked (shell_view))
		g_signal_emit (shell_view, signals[EXECUTE_SEARCH], 0);
}

/**
 * e_shell_view_block_execute_search:
 * @shell_view: an #EShellView
 *
 * Blocks e_shell_view_execute_search() in a way it does nothing.
 * Pair function for this is e_shell_view_unblock_execute_search().
 **/
void
e_shell_view_block_execute_search (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (shell_view->priv->execute_search_blocked + 1 != 0);

	shell_view->priv->execute_search_blocked++;
}

/**
 * e_shell_view_unblock_execute_search:
 * @shell_view: an #EShellView
 *
 * Unblocks previously blocked e_shell_view_execute_search() with
 * function e_shell_view_block_execute_search().
 **/
void
e_shell_view_unblock_execute_search (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (shell_view->priv->execute_search_blocked > 0);

	shell_view->priv->execute_search_blocked--;
}

/**
 * e_shell_view_is_execute_search_blocked:
 * @shell_view: an #EShellView
 *
 * Returns whether e_shell_view_execute_search() is blocked as a result
 * of previous e_shell_view_block_execute_search() calls.
 *
 * Returns: %TRUE if e_shell_view_execute_search() is blocked
 **/
gboolean
e_shell_view_is_execute_search_blocked (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->execute_search_blocked > 0;
}

/**
 * e_shell_view_update_actions:
 * @shell_view: an #EShellView
 *
 * Emits the #EShellView::update-actions signal.
 *
 * #EShellView subclasses should implement the
 * <structfield>update_actions</structfield> method in #EShellViewClass
 * to update the various #GtkAction<!-- -->s based on the current
 * #EShellSidebar and #EShellContent selections.  The
 * #EShellView::update-actions signal is typically emitted just before
 * showing a popup menu or just after the user selects an item in the
 * shell view.
 **/
void
e_shell_view_update_actions (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (e_shell_view_is_active (shell_view)) {
		if (shell_view->priv->update_actions_idle_id > 0) {
			g_source_remove (shell_view->priv->update_actions_idle_id);
			shell_view->priv->update_actions_idle_id = 0;
		}

		g_signal_emit (shell_view, signals[UPDATE_ACTIONS], 0);
	}
}

static gboolean
shell_view_call_update_actions_idle (gpointer user_data)
{
	EShellView *shell_view = user_data;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	shell_view->priv->update_actions_idle_id = 0;
	e_shell_view_update_actions (shell_view);

	return FALSE;
}

/**
 * e_shell_view_update_actions_in_idle:
 * @shell_view: an #EShellView
 *
 * Schedules e_shell_view_update_actions() call on idle.
 *
 * Since: 3.10
 **/
void
e_shell_view_update_actions_in_idle (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (!e_shell_view_is_active (shell_view))
		return;

	if (shell_view->priv->update_actions_idle_id == 0)
		shell_view->priv->update_actions_idle_id = g_idle_add (
			shell_view_call_update_actions_idle, shell_view);
}

/**
 * e_shell_view_show_popup_menu:
 * @shell_view: an #EShellView
 * @widget_path: path in the UI definition
 * @button_event: a #GdkEvent, or %NULL
 *
 * Displays a context-sensitive (or "popup") menu that is described in
 * the UI definition loaded into @shell_view<!-- -->'s user interface
 * manager.  The menu will be shown at the current mouse cursor position.
 *
 * The #EShellView::update-actions signal is emitted just prior to
 * showing the menu to give @shell_view and any plugins that extend
 * @shell_view a chance to update the menu's actions.
 *
 * Returns: the popup menu being displayed
 **/
GtkWidget *
e_shell_view_show_popup_menu (EShellView *shell_view,
                              const gchar *widget_path,
                              GdkEvent *button_event)
{
	EShellWindow *shell_window;
	GtkWidget *menu;
	guint event_button = 0;
	guint32 event_time;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	e_shell_view_update_actions (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	menu = e_shell_window_get_managed_widget (shell_window, widget_path);
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	if (button_event != NULL) {
		gdk_event_get_button (button_event, &event_button);
		event_time = gdk_event_get_time (button_event);
	} else {
		event_time = gtk_get_current_event_time ();
	}

	if (!gtk_menu_get_attach_widget (GTK_MENU (menu)))
		gtk_menu_attach_to_widget (GTK_MENU (menu),
					   GTK_WIDGET (shell_window),
					   NULL);
	gtk_menu_popup (
		GTK_MENU (menu),
		NULL, NULL, NULL, NULL,
		event_button, event_time);

	return menu;
}

/**
 * e_shell_view_write_source:
 * @shell_view: an #EShellView
 * @source: an #ESource
 *
 * Submits the current contents of @source to the D-Bus service to be
 * written to disk and broadcast to other clients.
 *
 * This function does not block: @shell_view will dispatch the operation
 * asynchronously and handle any errors.
 **/
void
e_shell_view_write_source (EShellView *shell_view,
                           ESource *source)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	EShellBackend *shell_backend;
	EShellContent *shell_content;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_SOURCE (source));

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	alert_sink = E_ALERT_SINK (shell_content);
	activity = e_source_util_write (source, alert_sink);
	e_shell_backend_add_activity (shell_backend, activity);
}

/**
 * e_shell_view_remove_source:
 * @shell_view: an #EShellView
 * @source: the #ESource to be removed
 *
 * Requests the D-Bus service to delete the key files for @source and all of
 * its descendants and broadcast their removal to all clients.
 *
 * This function does not block: @shell_view will dispatch the operation
 * asynchronously and handle any errors.
 **/
void
e_shell_view_remove_source (EShellView *shell_view,
                            ESource *source)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	EShellBackend *shell_backend;
	EShellContent *shell_content;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_SOURCE (source));

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	alert_sink = E_ALERT_SINK (shell_content);
	activity = e_source_util_remove (source, alert_sink);
	e_shell_backend_add_activity (shell_backend, activity);
}

/**
 * e_shell_view_remote_delete_source:
 * @shell_view: an #EShellView
 * @source: an #ESource
 *
 * Deletes the resource represented by @source from a remote server.
 * The @source must be #ESource:remote-deletable.  This will also delete
 * the key file for @source and broadcast its removal to all clients,
 * similar to e_shell_view_remove_source().
 *
 * This function does not block; @shell_view will dispatch the operation
 * asynchronously and handle any errors.
 **/
void
e_shell_view_remote_delete_source (EShellView *shell_view,
                                   ESource *source)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	EShellBackend *shell_backend;
	EShellContent *shell_content;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_SOURCE (source));

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	alert_sink = E_ALERT_SINK (shell_content);
	activity = e_source_util_remote_delete (source, alert_sink);
	e_shell_backend_add_activity (shell_backend, activity);
}

/**
 * e_shell_view_submit_thread_job:
 * @shell_view: an #EShellView instance
 * @description: user-friendly description of the job, to be shown in UI
 * @alert_ident: in case of an error, this alert identificator is used
 *    for EAlert construction
 * @alert_arg_0: (allow-none): in case of an error, use this string as
 *    the first argument to the EAlert construction; the second argument
 *    is the actual error message; can be #NULL, in which case only
 *    the error message is passed to the EAlert construction
 * @func: function to be run in a dedicated thread
 * @user_data: (allow-none): custom data passed into @func; can be #NULL
 * @free_user_data: (allow-none): function to be called on @user_data,
 *   when the job is over; can be #NULL
 *
 * Runs the @func in a dedicated thread. Any error is propagated to UI.
 * The cancellable passed into the @func is a #CamelOperation, thus
 * the caller can overwrite progress and description message on it.
 *
 * Returns: (transfer full): Newly created #EActivity on success.
 *   The caller is responsible to g_object_unref() it when done with it.
 *
 * Note: The @free_user_data, if set, is called in the main thread.
 *
 * Note: This function can be called only from the main thread.
 **/
EActivity *
e_shell_view_submit_thread_job (EShellView *shell_view,
				const gchar *description,
				const gchar *alert_ident,
				const gchar *alert_arg_0,
				EAlertSinkThreadJobFunc func,
				gpointer user_data,
				GDestroyNotify free_user_data)
{
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EActivity *activity;
	EAlertSink *alert_sink;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (description != NULL, NULL);
	g_return_val_if_fail (func != NULL, NULL);
	g_return_val_if_fail (g_thread_self () == shell_view->priv->main_thread, NULL);

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	alert_sink = E_ALERT_SINK (shell_content);

	activity = e_alert_sink_submit_thread_job (
		alert_sink,
		description,
		alert_ident,
		alert_arg_0,
		func,
		user_data,
		free_user_data);

	if (activity)
		e_shell_backend_add_activity (shell_backend, activity);

	return activity;
}
