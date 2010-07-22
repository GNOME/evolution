/*
 * e-shell-view.c
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

/**
 * SECTION: e-shell-view
 * @short_description: views within the main window
 * @include: shell/e-shell-view.h
 **/

#include "e-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-binding.h"
#include "e-util/e-extensible.h"
#include "e-util/e-file-utils.h"
#include "e-util/e-plugin-ui.h"
#include "e-util/e-ui-manager.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"
#include "filter/e-rule-context.h"

#include "e-shell-searchbar.h"
#include "e-shell-window-actions.h"

#define E_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL_VIEW, EShellViewPrivate))

#define SIMPLE_SEARCHBAR_WIDTH 300
#define STATE_SAVE_TIMEOUT_SECONDS 3

struct _EShellViewPrivate {

	gpointer shell_window;  /* weak pointer */

	GKeyFile *state_key_file;
	gpointer state_save_activity;  /* weak pointer */
	guint state_save_timeout_id;

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

	guint update_actions_blocked;
	gboolean update_actions_called;
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
	PROP_VIEW_ID
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
	EFilterRule *rule;
	EFilterPart *part;
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

	rule = e_filter_rule_new ();
	part = e_rule_context_next_part (search_context, NULL);
	if (part == NULL)
		g_warning (
			"Could not load %s search: no parts",
			G_OBJECT_CLASS_NAME (class));
	else
		e_filter_rule_add_part (rule, e_filter_part_clone (part));

	g_free (system_filename);
	g_free (user_filename);
}

static void
shell_view_init_view_collection (EShellViewClass *class)
{
	EShellBackend *shell_backend;
	const gchar *base_dir;
	const gchar *backend_name;
	gchar *system_dir;
	gchar *local_dir;

	shell_backend = class->shell_backend;
	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));
	backend_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	base_dir = EVOLUTION_GALVIEWSDIR;
	system_dir = g_build_filename (base_dir, backend_name, NULL);

	base_dir = e_shell_backend_get_config_dir (shell_backend);
	local_dir = g_build_filename (base_dir, "views", NULL);

	/* The view collection is never destroyed. */
	class->view_collection = gal_view_collection_new ();

	gal_view_collection_set_title (
		class->view_collection, class->label);

	gal_view_collection_set_storage_directories (
		class->view_collection, system_dir, local_dir);

	g_free (system_dir);
	g_free (local_dir);

	/* This is all we can do.  It's up to the subclasses to
	 * add the appropriate factories to the view collection. */
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
shell_view_save_state (EShellView *shell_view)
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

#if 0  /* FIXME Enable this for 2.31 and mark the string for translation. */
	e_activity_set_primary_text (
		activity, ("Saving user interface state"));
#endif

	e_shell_backend_add_activity (shell_backend, activity);

	g_object_unref (file);

	return activity;
}

static gboolean
shell_view_state_timeout_cb (EShellView *shell_view)
{
	EActivity *activity;

	/* If a save is still in progress, check back later. */
	if (shell_view->priv->state_save_activity != NULL)
		return TRUE;

	activity = shell_view_save_state (shell_view);

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
			shell_view_save_state (E_SHELL_VIEW (object));
	}

	if (priv->state_save_activity != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->state_save_activity),
			&priv->state_save_activity);
		priv->state_save_activity = NULL;
	}

	if (priv->shell_window != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_window), &priv->shell_window);
		priv->shell_window = NULL;
	}

	if (priv->shell_content != NULL) {
		g_object_unref (priv->shell_content);
		priv->shell_content = NULL;
	}

	if (priv->shell_sidebar != NULL) {
		g_object_unref (priv->shell_sidebar);
		priv->shell_sidebar = NULL;
	}

	if (priv->shell_taskbar != NULL) {
		g_object_unref (priv->shell_taskbar);
		priv->shell_taskbar = NULL;
	}

	if (priv->searchbar != NULL) {
		g_object_unref (priv->searchbar);
		priv->searchbar = NULL;
	}

	if (priv->search_rule != NULL) {
		g_object_unref (priv->search_rule);
		priv->search_rule = NULL;
	}

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
	EShellViewClass *shell_view_class;
	EShellView *shell_view;
	GtkWidget *widget;

	shell_view = E_SHELL_VIEW (object);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);

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

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static GtkWidget *
shell_view_construct_searchbar (EShellView *shell_view)
{
	EShell *shell;
	EShellWindow *shell_window;
	EShellContent *shell_content;
	EShellSearchbar *shell_searchbar;
	GtkToolItem *item;
	GtkAction *action;
	GtkWidget *main_toolbar;
	GtkWidget *widget;

	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	widget = e_shell_searchbar_new (shell_view);

	/* In normal mode, we hand the searchbar off to EShellContent. */
	if (!e_shell_get_express_mode (shell)) {
		e_shell_content_set_searchbar (shell_content, widget);
		gtk_widget_show (widget);
		return widget;
	}

	/* Express mode is more complicated.  We append a heavily simplified
	 * version of it to the main toolbar, but only show it when this shell
	 * view is active.  So each view still gets its own searchbar. */

	shell_searchbar = E_SHELL_SEARCHBAR (widget);
	e_shell_searchbar_set_express_mode (shell_searchbar, TRUE);

	/* XXX Hardcoded sizes are evil, but what should the width be
	 *     relative to.  Window width?  The other toolbar width? */
	gtk_widget_set_size_request (widget, SIMPLE_SEARCHBAR_WIDTH, -1);

	main_toolbar = e_shell_window_get_managed_widget (
		shell_window, "/search-toolbar");

	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), widget);
	gtk_widget_show (GTK_WIDGET (item));

	action = e_shell_view_get_action (shell_view);
	e_binding_new (action, "active", widget, "visible");

	gtk_toolbar_insert (GTK_TOOLBAR (main_toolbar), item, -1);

	if (e_shell_get_small_screen_mode (shell)) {
		GtkWidget *image;
		GtkWidget *container;
		GtkAction *action;
		GtkToolItem *item;

		action = e_shell_window_get_action (
			shell_window, "close-window");
		image = gtk_image_new_from_icon_name (
			"window-close-hover", GTK_ICON_SIZE_DIALOG);
		item = gtk_tool_button_new (
			image, gtk_action_get_label (action));
		gtk_widget_set_name (GTK_WIDGET (item), "MeeGoCloseButton");
		gtk_activatable_set_related_action (
			GTK_ACTIVATABLE (item), action);
		container = e_shell_window_get_menu_bar_box (shell_window);
		gtk_box_pack_start (
			GTK_BOX (container),
			GTK_WIDGET (item), FALSE, FALSE, 0);
		gtk_widget_show_all (GTK_WIDGET (item));
	}

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
		priv->merge_id = e_ui_manager_add_ui_from_file (
			E_UI_MANAGER (ui_manager), basename);
		e_plugin_ui_enable_manager (ui_manager, id);

	} else if (!view_is_active && priv->merge_id != 0) {
		e_plugin_ui_disable_manager (ui_manager, id);
		gtk_ui_manager_remove_ui (ui_manager, priv->merge_id);
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

	g_return_if_fail (!shell_view->priv->update_actions_blocked);
	g_return_if_fail (e_shell_view_is_active (shell_view));

	shell_window = e_shell_view_get_shell_window (shell_view);
	focus_tracker = e_shell_window_get_focus_tracker (shell_window);

	e_focus_tracker_update_actions (focus_tracker);
}

static void
shell_view_class_init (EShellViewClass *class)
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
			G_PARAM_CONSTRUCT_ONLY));

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
			G_PARAM_READWRITE));

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
			G_PARAM_READWRITE));

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
			G_PARAM_READABLE));

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
			G_PARAM_READABLE));

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
			G_PARAM_READABLE));

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
			G_PARAM_READABLE));

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
			G_PARAM_CONSTRUCT_ONLY));

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
			G_PARAM_READABLE));

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
			G_PARAM_READWRITE));

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
shell_view_init (EShellView *shell_view,
                 EShellViewClass *class)
{
	GtkSizeGroup *size_group;

	if (class->search_context == NULL)
		shell_view_init_search_context (class);

	if (class->view_collection == NULL)
		shell_view_init_view_collection (class);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	shell_view->priv = E_SHELL_VIEW_GET_PRIVATE (shell_view);
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
			(GClassInitFunc) shell_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShellView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_view_init,
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

	source_id = g_timeout_add_seconds (
		STATE_SAVE_TIMEOUT_SECONDS, (GSourceFunc)
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

	if (!shell_view->priv->execute_search_blocked)
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
 *
 * Emission can be blocked by e_shell_view_block_update_actions().
 **/
void
e_shell_view_update_actions (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (!e_shell_view_is_active (shell_view))
		return;

	if (shell_view->priv->update_actions_blocked > 0) {
		shell_view->priv->update_actions_called = TRUE;
	} else {
		shell_view->priv->update_actions_called = FALSE;
		g_signal_emit (shell_view, signals[UPDATE_ACTIONS], 0);
	}
}

/**
 * e_shell_view_block_update_actions:
 * @shell_view: an #EShellView
 *
 * Block emission of #EShellView::update-actions signal through
 * e_shell_view_update_actions(). The emission si blocked until
 * e_shell_view_unblock_update_actions() is called same times as
 * this function.
 **/
void
e_shell_view_block_update_actions (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (shell_view->priv->update_actions_blocked + 1 != 0);

	shell_view->priv->update_actions_blocked++;
	if (shell_view->priv->update_actions_blocked == 1)
		shell_view->priv->update_actions_called = FALSE;
}

/**
 * e_shell_view_unblock_update_actions:
 * @shell_view: an #EShellView
 *
 * Unblock emission of #EShellView::update-actions signal through
 * e_shell_view_update_actions(), previously blocked by function
 * e_shell_view_block_update_actions().
 **/
void
e_shell_view_unblock_update_actions (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (shell_view->priv->update_actions_blocked > 0);

	shell_view->priv->update_actions_blocked--;

	if (!shell_view->priv->update_actions_blocked &&
		shell_view->priv->update_actions_called) {

		shell_view->priv->update_actions_called = FALSE;
		e_shell_view_update_actions (shell_view);
	}
}

/**
 * e_shell_view_show_popup_menu:
 * @shell_view: an #EShellView
 * @widget_path: path in the UI definition
 * @event: a #GdkEventButton
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
                              GdkEventButton *event)
{
	EShellWindow *shell_window;
	GtkWidget *menu;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	e_shell_view_update_actions (shell_view);

	shell_window = e_shell_view_get_shell_window (shell_view);
	menu = e_shell_window_get_managed_widget (shell_window, widget_path);
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());

	return menu;
}

/**
 * e_shell_view_new_view_instance:
 * @shell_view: an #EShellView
 * @instance_id: a name for the #GalViewInstance
 *
 * Creates a new #GalViewInstance and configures it to keep
 * @shell_view<!-- -->'s #EShellView:view-id property up-to-date.
 *
 * Returns: a new #GalViewInstance
 **/
GalViewInstance *
e_shell_view_new_view_instance (EShellView *shell_view,
                                const gchar *instance_id)
{
	EShellViewClass *class;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	class = E_SHELL_VIEW_GET_CLASS (shell_view);

	view_collection = class->view_collection;
	view_instance = gal_view_instance_new (view_collection, instance_id);

	g_signal_connect_swapped (
		view_instance, "changed",
		G_CALLBACK (shell_view_update_view_id), shell_view);

	g_signal_connect_swapped (
		view_instance, "loaded",
		G_CALLBACK (shell_view_update_view_id), shell_view);

	return view_instance;
}
