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

#include "evolution-config.h"

#include "e-shell-view.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include "e-shell-searchbar.h"
#include "e-shell-window-actions.h"
#include "e-shell-window-private.h"

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

	EUIManager *ui_manager;
	GtkWidget *headerbar;
	EMenuBar *menu_bar;
	GtkWidget *menu_button; /* owned by menu_bar */
	GtkWidget *switcher;
	GtkWidget *tooltip_label;
	GtkWidget *status;

	GMenu *new_menu;
	GMenu *gal_view_list_menu;
	GMenu *saved_searches_menu;

	guint sidebar_visible : 1;
	guint switcher_visible : 1;
	guint taskbar_visible : 1;
	guint toolbar_visible : 1;
	guint accel_group_added : 1;
	gint sidebar_width;

	gchar *title;
	gchar *view_id;
	gint page_num;
	guint merge_id;

	EUIAction *switcher_action;
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
	PROP_SWITCHER_ACTION,
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
	PROP_VIEW_INSTANCE,
	PROP_MENUBAR_VISIBLE,
	PROP_SIDEBAR_VISIBLE,
	PROP_SWITCHER_VISIBLE,
	PROP_TASKBAR_VISIBLE,
	PROP_TOOLBAR_VISIBLE,
	PROP_SIDEBAR_WIDTH
};

enum {
	CLEAR_SEARCH,
	CUSTOM_SEARCH,
	EXECUTE_SEARCH,
	UPDATE_ACTIONS,
	INIT_UI_DATA,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

static gpointer e_shell_view_parent_class = NULL;
static gint EShellView_private_offset = 0;

static inline gpointer
e_shell_view_get_instance_private (EShellView *self)
{
	return (G_STRUCT_MEMBER_P (self, EShellView_private_offset));
}

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
	EShellBackendClass *backend_class;
	const gchar *base_directory;
	const gchar *name;
	gchar *system_directory;
	gchar *user_directory;

	shell_backend = class->shell_backend;
	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));

	backend_class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_if_fail (backend_class != NULL);

	name = backend_class->name;

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

void
e_shell_view_save_state_immediately (EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->state_save_timeout_id > 0) {
		g_source_remove (shell_view->priv->state_save_timeout_id);
		shell_view->priv->state_save_timeout_id = 0;
		if (!shell_view->priv->state_save_activity)
			shell_view_save_state (shell_view, TRUE);
	}
}

static void
shell_view_online_button_clicked_cb (EOnlineButton *button,
				     gpointer user_data)
{
	EShellView *self = user_data;
	EUIAction *action;

	if (e_online_button_get_online (button))
		action = e_ui_manager_get_action (self->priv->ui_manager, "work-offline");
	else
		action = e_ui_manager_get_action (self->priv->ui_manager, "work-online");

	g_action_activate (G_ACTION (action), NULL);
}

static gint
shell_view_sort_by_action_label_cmp (gconstpointer aa,
				     gconstpointer bb)
{
	const EUIAction *action1 = *((EUIAction **) aa);
	const EUIAction *action2 = *((EUIAction **) bb);

	if (action1 == action2)
		return 0;

	return g_utf8_collate (e_ui_action_get_label ((EUIAction *) action1), e_ui_action_get_label ((EUIAction *) action2));
}

static void
shell_view_extract_actions (const gchar *for_view,
			    GPtrArray *source_array,
			    GPtrArray *destination_array)
{
	guint ii, from_index = destination_array->len;

	/* Pick out the actions from the source list that are tagged
	 * as belonging to the for_view EShellView and move them to the
	 * destination list. */

	/* Example: Suppose [A] and [C] are tagged for this EShellView.
	 *
	 *        source_list = [A] -> [B] -> [C]
	 *                       ^             ^
	 *                       |             |
	 *         match_list = [ ] --------> [ ]
	 *
	 *
	 *   destination_list = [1] -> [2]  (other actions)
	 */
	for (ii = 0; ii < source_array->len; ii++) {
		EUIAction *action = g_ptr_array_index (source_array, ii);
		const gchar *backend_name;

		backend_name = g_object_get_data (G_OBJECT (action), "backend-name");

		if (g_strcmp0 (backend_name, for_view) != 0)
			continue;

		if (g_object_get_data (G_OBJECT (action), "primary"))
			g_ptr_array_insert (destination_array, from_index++, g_object_ref (action));
		else
			g_ptr_array_add (destination_array, g_object_ref (action));

		/* destination_list = [1] -> [2] -> [A] -> [C] */
		g_ptr_array_remove_index (source_array, ii);
		ii--;
	}
}

static gboolean
shell_view_button_style_to_state_cb (GValue *value,
				     GVariant *variant,
				     gpointer user_data)
{
	GtkToolbarStyle style = -1;
	const gchar *string = g_variant_get_string (variant, NULL);

	if (string != NULL) {
		if (strcmp (string, "icons") == 0)
			style = GTK_TOOLBAR_ICONS;
		else if (strcmp (string, "text") == 0)
			style = GTK_TOOLBAR_TEXT;
		else if (strcmp (string, "both") == 0)
			style = GTK_TOOLBAR_BOTH_HORIZ;
		else
			style = -1;
	}

	g_value_set_variant (value, g_variant_new_int32 (style));

	return TRUE;
}

static GVariant *
shell_view_state_to_button_style_cb (const GValue *value,
				     const GVariantType *expected_type,
				     gpointer user_data)
{
	GVariant *var_value = g_value_get_variant (value);
	const gchar *string;

	switch (var_value ? g_variant_get_int32 (var_value) : -1) {
		case GTK_TOOLBAR_ICONS:
			string = "icons";
			break;

		case GTK_TOOLBAR_TEXT:
			string = "text";
			break;

		case GTK_TOOLBAR_BOTH:
		case GTK_TOOLBAR_BOTH_HORIZ:
			string = "both";
			break;

		default:
			string = "toolbar";
			break;
	}

	return g_variant_new_string (string);
}

static void
shell_view_add_actions_as_section (EShellView *self,
				   GMenu *new_menu,
				   GPtrArray *actions)
{
	GMenu *items_menu;
	GMenuItem *item;
	guint ii;

	if (!actions || !actions->len)
		return;

	items_menu = g_menu_new ();

	for (ii = 0; ii < actions->len; ii++) {
		EUIAction *action = g_ptr_array_index (actions, ii);

		item = g_menu_item_new (NULL, NULL);
		e_ui_manager_update_item_from_action (self->priv->ui_manager, item, action);
		g_menu_append_item (items_menu, item);
		g_clear_object (&item);
	}

	g_menu_append_section (new_menu, NULL, G_MENU_MODEL (items_menu));

	g_clear_object (&items_menu);
}

static void
shell_view_populate_new_menu (EShellView *self)
{
	EShellBackend *shell_backend;
	EShellBackendClass *shell_backend_class;
	GPtrArray *new_item_actions;
	GPtrArray *new_source_actions;
	GPtrArray *view_actions;
	const gchar *backend_name;

	shell_backend = e_shell_view_get_shell_backend (self);
	shell_backend_class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_if_fail (shell_backend_class != NULL);

	e_ui_manager_freeze (self->priv->ui_manager);

	backend_name = shell_backend_class->name;

	/* Get sorted lists of "new item" and "new source" actions. */

	new_item_actions = e_ui_action_group_list_actions (e_ui_manager_get_action_group (self->priv->ui_manager, "new-item"));
	g_ptr_array_sort (new_item_actions, shell_view_sort_by_action_label_cmp);

	new_source_actions = e_ui_action_group_list_actions (e_ui_manager_get_action_group (self->priv->ui_manager, "new-source"));
	g_ptr_array_sort (new_source_actions, shell_view_sort_by_action_label_cmp);

	/* Give priority to actions that belong to this shell view. */

	view_actions = g_ptr_array_new_with_free_func (g_object_unref);

	shell_view_extract_actions (backend_name, new_item_actions, view_actions);
	shell_view_extract_actions (backend_name, new_source_actions, view_actions);

	g_menu_remove_all (self->priv->new_menu);

	/* Construct the menu with the layout. */

	shell_view_add_actions_as_section (self, self->priv->new_menu, view_actions);
	shell_view_add_actions_as_section (self, self->priv->new_menu, new_item_actions);
	shell_view_add_actions_as_section (self, self->priv->new_menu, new_source_actions);

	g_clear_pointer (&new_item_actions, g_ptr_array_unref);
	g_clear_pointer (&new_source_actions, g_ptr_array_unref);
	g_clear_pointer (&view_actions, g_ptr_array_unref);

	e_ui_manager_thaw (self->priv->ui_manager);
}

static GtkWidget *
shell_view_construct_taskbar (EShellView *self)
{
	EShell *shell;
	GtkWidget *box;
	GtkWidget *status_area;
	GtkWidget *online_button;
	GtkWidget *tooltip_label;
	GtkStyleContext *style_context;
	gint height;

	shell = e_shell_window_get_shell (self->priv->shell_window);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_container_set_border_width (GTK_CONTAINER (box), 3);
	gtk_widget_show (box);

	status_area = gtk_frame_new (NULL);
	style_context = gtk_widget_get_style_context (status_area);
	gtk_style_context_add_class (style_context, "taskbar");
	gtk_container_add (GTK_CONTAINER (status_area), box);

	e_binding_bind_property (
		self, "taskbar-visible",
		status_area, "visible",
		G_BINDING_SYNC_CREATE);

	/* Make the status area as large as the task bar. */
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, NULL, &height);
	gtk_widget_set_size_request (status_area, -1, (height * 2) + 6);

	online_button = e_online_button_new ();
	gtk_box_pack_start (GTK_BOX (box), online_button, FALSE, TRUE, 0);
	gtk_widget_show (online_button);

	e_binding_bind_property (
		shell, "online",
		online_button, "online",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell, "network-available",
		online_button, "sensitive",
		G_BINDING_SYNC_CREATE);

	g_signal_connect (
		online_button, "clicked",
		G_CALLBACK (shell_view_online_button_clicked_cb),
		self);

	tooltip_label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (tooltip_label), 0);
	gtk_box_pack_start (GTK_BOX (box), tooltip_label, TRUE, TRUE, 0);
	self->priv->tooltip_label = g_object_ref (tooltip_label);
	gtk_widget_hide (tooltip_label);

	gtk_box_pack_start (GTK_BOX (box), self->priv->shell_taskbar, TRUE, TRUE, 0);
	gtk_widget_show (self->priv->shell_taskbar);

	e_binding_bind_property (
		self->priv->shell_taskbar, "height-request",
		self->priv->tooltip_label, "height-request",
		G_BINDING_SYNC_CREATE);

	return status_area;
}

static gboolean
e_shell_view_ui_manager_ignore_accel_cb (EUIManager *manager,
					 EUIAction *action,
					 gpointer user_data)
{
	EShellView *self = user_data;
	gboolean ignore = FALSE;

	if (e_shell_view_is_active (self)) {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		if (GTK_IS_WINDOW (toplevel))
			ignore = e_util_ignore_accel_for_focused (gtk_window_get_focus (GTK_WINDOW (toplevel)));
	} else {
		ignore = TRUE;
	}

	return ignore;
}

static void
action_custom_rule_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EShellView *self = user_data;
	EFilterRule *rule;

	rule = g_object_get_data (G_OBJECT (action), "rule");
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	e_shell_view_custom_search (self, rule);
}

static void
shell_view_update_search_menu (EShellView *self)
{
	EShellViewClass *shell_view_class;
	ERuleContext *context;
	EFilterRule *rule;
	EUIActionGroup *action_group;
	const gchar *source;
	gint ii = 0;

	g_return_if_fail (E_IS_SHELL_VIEW (self));

	shell_view_class = E_SHELL_VIEW_GET_CLASS (self);
	context = shell_view_class->search_context;

	source = E_FILTER_SOURCE_INCOMING;

	e_ui_manager_freeze (self->priv->ui_manager);

	action_group = e_ui_manager_get_action_group (self->priv->ui_manager, "custom-rules");
	e_ui_action_group_remove_all (action_group);

	g_menu_remove_all (self->priv->saved_searches_menu);
	e_ui_action_group_remove_all (action_group);

	rule = e_rule_context_next_rule (context, NULL, source);
	while (rule != NULL) {
		EUIAction *action;
		GMenuItem *menu_item;
		gchar action_name[128];
		gchar *label, *label_numbered = NULL;

		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "custom-rule-%d", ii) < sizeof (action_name));
		label = e_str_without_underscores (rule->name);
		if (ii < 10)
			label_numbered = g_strdup_printf ("_%d. %s", ++ii, label);
		else
			ii++;

		action = e_ui_action_new (e_ui_action_group_get_name (action_group), action_name, NULL);
		e_ui_action_set_label (action, label_numbered ? label_numbered : label);
		e_ui_action_set_tooltip (action, _("Execute these search parameters"));
		e_ui_action_set_usable_for_kinds (action, 0);

		e_ui_action_group_add (action_group, action);

		g_object_set_data_full (G_OBJECT (action), "rule", g_object_ref (rule), g_object_unref);

		g_signal_connect_object (action, "activate",
			G_CALLBACK (action_custom_rule_cb), self, 0);

		menu_item = g_menu_item_new (NULL, NULL);
		e_ui_manager_update_item_from_action (self->priv->ui_manager, menu_item, action);
		g_menu_append_item (self->priv->saved_searches_menu, menu_item);
		g_clear_object (&menu_item);

		g_object_unref (action);
		g_free (label_numbered);
		g_free (label);

		rule = e_rule_context_next_rule (context, rule, source);
	}

	e_ui_manager_thaw (self->priv->ui_manager);
}

static void
action_search_advanced_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EShellView *self = user_data;
	EShellContent *shell_content;

	shell_content = e_shell_view_get_shell_content (self);

	e_shell_content_run_advanced_search_dialog (shell_content);
	shell_view_update_search_menu (self);
}

static void
action_search_clear_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EShellView *self = user_data;

	e_shell_view_clear_search (self);
}

static void
action_search_edit_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EShellView *self = user_data;
	EShellContent *shell_content;

	shell_content = e_shell_view_get_shell_content (self);

	e_shell_content_run_edit_searches_dialog (shell_content);
	shell_view_update_search_menu (self);
}

static void
search_options_selection_cancel_cb (GtkMenuShell *menu,
				    EShellView *self);

static void
search_options_selection_done_cb (GtkMenuShell *menu,
				  EShellView *self)
{
	EShellSearchbar *search_bar;

	/* disconnect first */
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_done_cb, self);
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_cancel_cb, self);

	g_return_if_fail (E_IS_SHELL_VIEW (self));

	search_bar = E_SHELL_SEARCHBAR (e_shell_view_get_searchbar (self));
	e_shell_searchbar_search_entry_grab_focus (search_bar);
}

static void
search_options_selection_cancel_cb (GtkMenuShell *menu,
				    EShellView *self)
{
	/* only disconnect both functions, thus the selection-done is not called */
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_done_cb, self);
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_cancel_cb, self);
}

static void
action_search_options_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EShellView *self = user_data;
	EShellSearchbar *search_bar;
	GtkWidget *popup_menu;

	search_bar = E_SHELL_SEARCHBAR (e_shell_view_get_searchbar (self));
	if (!e_shell_searchbar_search_entry_has_focus (search_bar)) {
		e_shell_searchbar_search_entry_grab_focus (search_bar);
		return;
	}

	popup_menu = e_shell_view_show_popup_menu (self, "search-options", NULL);

	if (popup_menu) {
		g_return_if_fail (GTK_IS_MENU_SHELL (popup_menu));

		g_signal_connect_object (popup_menu, "selection-done",
			G_CALLBACK (search_options_selection_done_cb), self, 0);
		g_signal_connect_object (popup_menu, "cancel",
			G_CALLBACK (search_options_selection_cancel_cb), self, 0);
	}
}

static void
action_search_quick_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EShellView *self = user_data;

	e_shell_view_execute_search (self);
}

static void
action_search_save_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EShellView *self = user_data;
	EShellContent *shell_content;

	shell_content = e_shell_view_get_shell_content (self);

	e_shell_content_run_save_search_dialog (shell_content);
	shell_view_update_search_menu (self);
}

static void
action_gal_delete_view_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EShellView *self = user_data;
	GalViewInstance *view_instance;
	gchar *gal_view_id;
	gint index = -1;

	view_instance = e_shell_view_get_view_instance (self);
	g_return_if_fail (view_instance != NULL);

	/* XXX This is kinda cumbersome.  The view collection API
	 *     should be using only view ID's, not index numbers. */
	gal_view_id = gal_view_instance_get_current_view_id (view_instance);
	if (gal_view_id != NULL) {
		index = gal_view_collection_get_view_index_by_id (
			view_instance->collection, gal_view_id);
		g_free (gal_view_id);
	}

	gal_view_collection_delete_view (view_instance->collection, index);

	gal_view_collection_save (view_instance->collection);
}

static void shell_view_update_view_menu (EShellView *self);

static void
action_gal_view_cb (EUIAction *action,
		    GParamSpec *parm,
		    gpointer user_data)
{
	EShellView *self = user_data;
	GVariant *state;
	const gchar *view_id;

	state = g_action_get_state (G_ACTION (action));

	view_id = g_variant_get_string (state, NULL);

	e_shell_view_set_view_id (self, view_id);

	g_clear_pointer (&state, g_variant_unref);

	shell_view_update_view_menu (self);
}

static void
action_gal_save_custom_view_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EShellView *self = user_data;
	GalViewInstance *view_instance;

	view_instance = e_shell_view_get_view_instance (self);

	gal_view_instance_save_as (view_instance);
}

static void
action_gal_customize_view_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EShellView *self = user_data;
	GalViewInstance *view_instance;
	GalView *gal_view;

	view_instance = e_shell_view_get_view_instance (self);

	gal_view = gal_view_instance_get_current_view (view_instance);

	if (GAL_IS_VIEW_ETABLE (gal_view)) {
		GalViewEtable *etable_view = GAL_VIEW_ETABLE (gal_view);
		ETable *etable;

		etable = gal_view_etable_get_table (etable_view);

		if (etable) {
			e_table_customize_view (etable);
		} else {
			ETree *etree;

			etree = gal_view_etable_get_tree (etable_view);

			if (etree)
				e_tree_customize_view (etree);
		}
	}
}

static EUIAction * /* (transfer none) */
shell_view_get_prefer_new_item_action (EShellView *self)
{
	EShellBackend *shell_backend;
	EUIAction *action = NULL;
	const gchar *prefer_new_item;

	shell_backend = e_shell_view_get_shell_backend (self);
	prefer_new_item = e_shell_backend_get_prefer_new_item (shell_backend);

	if (prefer_new_item)
		action = e_shell_view_get_action (self, prefer_new_item);

	if (!action) {
		EShellBackendClass *shell_backend_class;
		GPtrArray *new_item_actions;
		const gchar *backend_name;
		guint ii;

		shell_backend_class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
		g_return_val_if_fail (shell_backend_class != NULL, NULL);

		backend_name = shell_backend_class->name;

		new_item_actions = e_ui_action_group_list_actions (e_ui_manager_get_action_group (self->priv->ui_manager, "new-item"));
		g_ptr_array_sort (new_item_actions, shell_view_sort_by_action_label_cmp);

		for (ii = 0; ii < new_item_actions->len; ii++) {
			EUIAction *new_item_action = g_ptr_array_index (new_item_actions, ii);
			const gchar *action_backend_name;

			action_backend_name = g_object_get_data (G_OBJECT (new_item_action), "backend-name");

			/* pick the primary action or the first action in the list */
			if (g_strcmp0 (action_backend_name, backend_name) == 0) {
				if (g_object_get_data (G_OBJECT (new_item_action), "primary")) {
					action = new_item_action;
					break;
				} else if (!action) {
					action = new_item_action;
				}
			}
		}

		g_clear_pointer (&new_item_actions, g_ptr_array_unref);
	}

	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

static void
action_shell_view_new_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EShellView *self = user_data;
	EUIAction *new_item_action;

	new_item_action = shell_view_get_prefer_new_item_action (self);
	g_return_if_fail (new_item_action != NULL);

	g_action_activate (G_ACTION (new_item_action), NULL);
}

static void
action_shell_view_customize_toolbar_cb (EUIAction *action,
					GVariant *parameter,
					gpointer user_data)
{
	EShellView *self = user_data;

	e_shell_view_run_ui_customize_dialog (self, NULL);
}

static void
shell_view_init_ui_data (EShellView *self)
{
	static const EUIActionEntry search_entries[] = {

		{ "search-advanced",
		  NULL,
		  N_("_Advanced Search…"),
		  NULL,
		  N_("Construct a more advanced search"),
		  action_search_advanced_cb, NULL, NULL, NULL },

		{ "search-clear",
		  "edit-clear",
		  N_("_Clear"),
		  "<Control><Shift>q",
		  N_("Clear the current search parameters"),
		  action_search_clear_cb, NULL, NULL, NULL },

		{ "search-edit",
		  NULL,
		  N_("_Edit Saved Searches…"),
		  NULL,
		  N_("Manage your saved searches"),
		  action_search_edit_cb, NULL, NULL, NULL },

		{ "search-options",
		  "edit-find",
		  N_("_Find"),
		  "<Control>f",
		  N_("Click here to change the search type"),
		  action_search_options_cb, NULL, NULL, NULL },

		{ "search-quick",
		  "edit-find",
		  N_("_Find Now"),
		  NULL,
		  N_("Execute the current search parameters"),
		  action_search_quick_cb, NULL, NULL, NULL },

		{ "search-save",
		  NULL,
		  N_("_Save Search…"),
		  NULL,
		  N_("Save the current search parameters"),
		  action_search_save_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry show_entries[] = {

		{ "show-menubar",
		  NULL,
		  N_("Show _Menu Bar"),
		  NULL,
		  N_("Show the menu bar"),
		  NULL, NULL, "true", NULL },

		{ "show-sidebar",
		  NULL,
		  N_("Show Side_bar"),
		  "F9",
		  N_("Show the sidebar"),
		  NULL, NULL, "true", NULL },

		{ "show-switcher",
		  NULL,
		  N_("Show _Buttons"),
		  NULL,
		  N_("Show the switcher buttons"),
		  NULL, NULL, "true", NULL },

		{ "show-taskbar",
		  NULL,
		  N_("Show _Status Bar"),
		  NULL,
		  N_("Show the status bar"),
		  NULL, NULL, "true", NULL },

		{ "show-toolbar",
		  NULL,
		  N_("Show _Tool Bar"),
		  NULL,
		  N_("Show the tool bar"),
		  NULL, NULL, "true", NULL }
	};

	static const EUIActionEntry custom_entries[] = {

		{ "gal-delete-view",
		  NULL,
		  N_("Delete Current View"),
		  NULL,
		  NULL,  /* Set in update_view_menu */
		  action_gal_delete_view_cb, NULL, NULL, NULL },

		{ "gal-save-custom-view",
		  NULL,
		  N_("Save Custom View…"),
		  NULL,
		  N_("Save current custom view"),
		  action_gal_save_custom_view_cb, NULL, NULL, NULL },

		{ "gal-customize-view",
		  NULL,
		  N_("Custo_mize Current View…"),
		  NULL,
		  NULL,
		  action_gal_customize_view_cb, NULL, NULL, NULL },

		{ "gal-custom-view",
		  NULL,
		  N_("Custom View"),
		  NULL,
		  N_("Current view is a customized view"),
		  NULL, "s", "''", NULL },

		{ "EShellView::new-menu",
		  "document-new",
		  N_("_New"),
		  NULL,
		  NULL,
		  action_shell_view_new_cb, NULL, NULL, NULL },

		{ "customize-ui",
		  NULL,
		  N_("Customi_ze…"),
		  NULL,
		  N_("Customize user interface, like toolbar content and shortcuts"),
		  action_shell_view_customize_toolbar_cb, NULL, NULL, NULL },

		/*** Menus ***/

		{ "gal-view-menu", NULL, N_("C_urrent View"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "saved-searches", NULL, N_("Saved searches"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EShellView::gal-view-list", NULL, N_("List of available views"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EShellView::menu-button", NULL, N_("Menu"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EShellView::saved-searches-list", NULL, N_("List of saved searches"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EShellView::switch-to-list", NULL, N_("List of views"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	static const EUIActionEnumEntry shell_switcher_style_entries[] = {

		{ "switcher-style-icons",
		  NULL,
		  N_("_Icons Only"),
		  NULL,
		  N_("Display window buttons with icons only"),
		  NULL, GTK_TOOLBAR_ICONS },

		{ "switcher-style-text",
		  NULL,
		  N_("_Text Only"),
		  NULL,
		  N_("Display window buttons with text only"),
		  NULL, GTK_TOOLBAR_TEXT },

		{ "switcher-style-both",
		  NULL,
		  N_("Icons _and Text"),
		  NULL,
		  N_("Display window buttons with icons and text"),
		  NULL, GTK_TOOLBAR_BOTH_HORIZ },

		{ "switcher-style-user",
		  NULL,
		  N_("Tool_bar Style"),
		  NULL,
		  N_("Display window buttons using the desktop toolbar setting"),
		  NULL, -1 }
	};

	EShellViewClass *shell_view_class;
	EUIActionGroup *action_group;
	EUIAction *action;
	GError *local_error = NULL;

	shell_view_class = E_SHELL_VIEW_GET_CLASS (self);
	g_return_if_fail (shell_view_class != NULL);

	e_ui_manager_add_actions (self->priv->ui_manager, "search-entries", NULL,
		search_entries, G_N_ELEMENTS (search_entries), self);
	e_ui_manager_add_actions (self->priv->ui_manager, "show-entries", NULL,
		show_entries, G_N_ELEMENTS (show_entries), self);
	e_ui_manager_add_actions (self->priv->ui_manager, "custom-entries", NULL,
		custom_entries, G_N_ELEMENTS (custom_entries), self);
	e_ui_manager_add_actions_enum (self->priv->ui_manager, "custom-entries", NULL,
		shell_switcher_style_entries, G_N_ELEMENTS (shell_switcher_style_entries), self);

	action_group = e_ui_action_group_new ("gal-view");
	e_ui_manager_add_action_group (self->priv->ui_manager, action_group);
	g_clear_object (&action_group);

	action = e_ui_manager_get_action (self->priv->ui_manager, "gal-custom-view");
	g_signal_connect_object (action, "notify::state",
		G_CALLBACK (action_gal_view_cb), self, 0);

	e_ui_manager_set_actions_usable_for_kinds (self->priv->ui_manager, E_UI_ELEMENT_KIND_MENU,
		"gal-view-menu",
		"saved-searches",
		"EShellView::gal-view-list",
		"EShellView::saved-searches-list",
		"EShellView::switch-to-list",
		NULL);

	action = e_ui_manager_get_action (self->priv->ui_manager, "EShellView::menu-button");
	e_ui_action_set_usable_for_kinds (action, E_UI_ELEMENT_KIND_HEADERBAR);

	e_shell_window_init_ui_data (self->priv->shell_window, self);
	shell_view_populate_new_menu (self);

	if (self->priv->searchbar && E_IS_SHELL_SEARCHBAR (self->priv->searchbar))
		e_shell_searchbar_init_ui_data (E_SHELL_SEARCHBAR (self->priv->searchbar));

	g_signal_emit (self, signals[INIT_UI_DATA], 0);

	/* Fine tuning. */
	e_ui_action_set_sensitive (e_ui_manager_get_action (self->priv->ui_manager, "search-quick"), FALSE);

	e_binding_bind_property (
		self, "menubar-visible",
		e_ui_manager_get_action (self->priv->ui_manager, "show-menubar"), "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		self, "sidebar-visible",
		e_ui_manager_get_action (self->priv->ui_manager, "show-sidebar"), "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		self, "switcher-visible",
		e_ui_manager_get_action (self->priv->ui_manager, "show-switcher"), "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		self, "taskbar-visible",
		e_ui_manager_get_action (self->priv->ui_manager, "show-taskbar"), "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		self, "toolbar-visible",
		e_ui_manager_get_action (self->priv->ui_manager, "show-toolbar"), "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		e_ui_manager_get_action (self->priv->ui_manager, "show-sidebar"), "active",
		e_ui_manager_get_action (self->priv->ui_manager, "show-switcher"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		e_ui_manager_get_action (self->priv->ui_manager, "show-sidebar"), "active",
		e_ui_manager_get_action (self->priv->ui_manager, "switcher-style-both"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		e_ui_manager_get_action (self->priv->ui_manager, "show-sidebar"), "active",
		e_ui_manager_get_action (self->priv->ui_manager, "switcher-style-icons"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		e_ui_manager_get_action (self->priv->ui_manager, "show-sidebar"), "active",
		e_ui_manager_get_action (self->priv->ui_manager, "switcher-style-text"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		e_ui_manager_get_action (self->priv->ui_manager, "show-sidebar"), "active",
		e_ui_manager_get_action (self->priv->ui_manager, "switcher-style-user"), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		e_ui_manager_get_action (self->priv->ui_manager, "show-sidebar"), "active",
		e_ui_manager_get_action (self->priv->ui_manager, "switcher-menu"), "sensitive",
		G_BINDING_SYNC_CREATE);

	if (!e_ui_parser_merge_file (e_ui_manager_get_parser (self->priv->ui_manager), shell_view_class->ui_definition, &local_error))
		g_warning ("%s: Failed to read %s file: %s", G_STRFUNC, shell_view_class->ui_definition, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);

	g_signal_connect_object (self->priv->shell_window, "update-new-menu",
		G_CALLBACK (shell_view_populate_new_menu), self, G_CONNECT_SWAPPED);
}

static void
shell_view_menubar_deactivate_cb (GtkWidget *menu_bar,
				  gpointer user_data)
{
	EShellView *self = user_data;

	g_return_if_fail (E_IS_SHELL_VIEW (self));

	if (!e_shell_view_get_menubar_visible (self))
		gtk_widget_hide (menu_bar);
}

static void
shell_view_set_switcher_action (EShellView *shell_view,
				EUIAction *action)
{
	g_return_if_fail (shell_view->priv->switcher_action == NULL);
	g_return_if_fail (E_IS_UI_ACTION (action));

	shell_view->priv->switcher_action = g_object_ref (action);

	e_shell_view_set_title (shell_view, e_ui_action_get_label (action));
}

static void
shell_view_switcher_style_cb (EUIAction *action,
			      GParamSpec *param,
			      gpointer user_data)
{
	EShellSwitcher *switcher = user_data;
	GtkToolbarStyle style;
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	style = g_variant_get_int32 (state);
	g_clear_pointer (&state, g_variant_unref);

	switch (style) {
		case GTK_TOOLBAR_ICONS:
		case GTK_TOOLBAR_TEXT:
		case GTK_TOOLBAR_BOTH:
		case GTK_TOOLBAR_BOTH_HORIZ:
			e_shell_switcher_set_style (switcher, style);
			break;

		default:
			e_shell_switcher_unset_style (switcher);
			break;
	}
}

static EUIAction *
shell_view_get_new_menu_first_action (GMenuModel *new_menu,
				      EUIManager *manager)
{
	EUIAction *action = NULL;
	gint ii, n_items;

	if (!new_menu)
		return NULL;

	n_items = g_menu_model_get_n_items (new_menu);

	for (ii = 0; ii < n_items && !action; ii++) {
		GVariant *value;

		value = g_menu_model_get_item_attribute_value (new_menu, ii, G_MENU_ATTRIBUTE_ACTION, G_VARIANT_TYPE_STRING);
		if (value) {
			const gchar *action_full_name = g_variant_get_string (value, NULL);

			if (action_full_name) {
				action = e_ui_manager_get_action (manager, action_full_name);

				if (!action) {
					const gchar *dot = strchr (action_full_name, '.');

					if (dot)
						action = e_ui_manager_get_action (manager, dot + 1);
				}
			}
		}

		g_clear_pointer (&value, g_variant_unref);

		if (!action) {
			GMenuLinkIter *iter;

			iter = g_menu_model_iterate_item_links (new_menu, ii);
			if (iter) {
				GMenuModel *submenu = NULL;

				while (!action && g_menu_link_iter_get_next (iter, NULL, &submenu)) {
					if (submenu)
						action = shell_view_get_new_menu_first_action (submenu, manager);

					g_clear_object (&submenu);
				}

				g_clear_object (&iter);
			}
		}
	}

	return action;
}

static void
shell_view_update_toolbar_new_menu_fallback_action_cb (GMenuModel *new_menu,
						       gint position,
						       gint removed,
						       gint added,
						       gpointer user_data)
{
	EMenuToolButton *tool_button = user_data;
	EUIManager *ui_manager = NULL;
	EUIAction *action;

	g_object_get (tool_button, "ui-manager", &ui_manager, NULL);

	if (!ui_manager)
		return;

	action = shell_view_get_new_menu_first_action (new_menu, ui_manager);
	e_menu_tool_button_set_fallback_action (tool_button, action);

	g_clear_object (&ui_manager);
}

static void
shell_view_update_headerbar_new_menu_action_cb (GMenuModel *new_menu,
						gint position,
						gint removed,
						gint added,
						gpointer user_data)
{
	EHeaderBarButton *header_bar_button = user_data;
	EUIManager *ui_manager = NULL;
	EUIAction *action;

	g_object_get (header_bar_button, "ui-manager", &ui_manager, NULL);

	if (!ui_manager)
		return;

	action = shell_view_get_new_menu_first_action (new_menu, ui_manager);
	g_object_set (header_bar_button, "action", action, NULL);

	g_clear_object (&ui_manager);
}

static gboolean
shell_view_ui_manager_create_item_cb (EUIManager *manager,
				      EUIElement *elem,
				      EUIAction *action,
				      EUIElementKind for_kind,
				      GObject **out_item,
				      gpointer user_data)
{
	EShellView *self = user_data;
	const gchar *name;

	g_return_val_if_fail (E_IS_SHELL_VIEW (self), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EShellView::"))
		return FALSE;

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		if (is_action ("EShellView::new-menu")) {
			*out_item = G_OBJECT (g_menu_item_new_submenu (e_ui_action_get_label (action), G_MENU_MODEL (self->priv->new_menu)));
			g_menu_item_set_attribute (G_MENU_ITEM (*out_item), "icon", "s", e_ui_action_get_icon_name (action));
		} else if (is_action ("EShellView::gal-view-list")) {
			*out_item = G_OBJECT (g_menu_item_new_section (NULL, G_MENU_MODEL (self->priv->gal_view_list_menu)));
		} else if (is_action ("EShellView::saved-searches-list")) {
			*out_item = G_OBJECT (g_menu_item_new_section (NULL, G_MENU_MODEL (self->priv->saved_searches_menu)));
		} else if (is_action ("EShellView::switch-to-list")) {
			GMenuModel *menu_model = self->priv->shell_window ? e_shell_window_ref_switch_to_menu_model (self->priv->shell_window) : NULL;
			if (menu_model)
				*out_item = G_OBJECT (g_menu_item_new_section (NULL, menu_model));
		} else {
			g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
		}
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		GtkWidget *widget = NULL;
		gboolean claimed = FALSE;

		if (is_action ("EShellView::new-menu")) {
			EShellBackend *shell_backend;
			EUIAction *fallback_action;
			GtkToolItem *tool_item;
			GtkWidget *menu;

			fallback_action = shell_view_get_new_menu_first_action (G_MENU_MODEL (self->priv->new_menu), manager);

			menu = gtk_menu_new_from_model (G_MENU_MODEL (self->priv->new_menu));
			tool_item = e_menu_tool_button_new (C_("toolbar-button", "New"), manager);
			e_menu_tool_button_set_fallback_action (E_MENU_TOOL_BUTTON (tool_item), fallback_action);
			gtk_tool_item_set_is_important (tool_item, TRUE);
			gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (tool_item), menu);
			gtk_widget_set_visible (GTK_WIDGET (tool_item), TRUE);

			shell_backend = e_shell_view_get_shell_backend (self);

			e_binding_bind_property (shell_backend, "prefer-new-item",
				tool_item, "prefer-item",
				G_BINDING_SYNC_CREATE);

			g_signal_connect_object (self->priv->new_menu, "items-changed",
				G_CALLBACK (shell_view_update_toolbar_new_menu_fallback_action_cb), tool_item, 0);

			*out_item = G_OBJECT (tool_item);
		} else {
			g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
			claimed = TRUE;
		}

		if (widget) {
			GtkToolItem *tool_item;

			tool_item = gtk_tool_item_new ();
			gtk_container_add (GTK_CONTAINER (tool_item), widget);
			gtk_widget_show_all (GTK_WIDGET (tool_item));

			*out_item = G_OBJECT (tool_item);
		} else if (!claimed && !*out_item) {
			g_warning ("%s: Did not get toolbar widget for '%s'", G_STRFUNC, name);
		}
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		if (is_action ("EShellView::new-menu")) {
			GtkWidget *button;
			GtkWidget *menu;
			EShellBackend *shell_backend;
			EUIAction *fallback_action;

			fallback_action = shell_view_get_new_menu_first_action (G_MENU_MODEL (self->priv->new_menu), manager);

			menu = gtk_menu_new_from_model (G_MENU_MODEL (self->priv->new_menu));
			button = e_header_bar_button_new (C_("toolbar-button", "New"), fallback_action, manager);
			e_header_bar_button_take_menu (E_HEADER_BAR_BUTTON (button), menu);
			gtk_widget_set_visible (GTK_WIDGET (button), TRUE);

			shell_backend = e_shell_view_get_shell_backend (self);

			e_binding_bind_property (shell_backend, "prefer-new-item",
				button, "prefer-item",
				G_BINDING_SYNC_CREATE);

			g_signal_connect_object (self->priv->new_menu, "items-changed",
				G_CALLBACK (shell_view_update_headerbar_new_menu_action_cb), button, 0);

			*out_item = G_OBJECT (button);
		} else if (is_action ("EShellView::menu-button")) {
			*out_item = G_OBJECT (g_object_ref (self->priv->menu_button));
		} else {
			g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
		}
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

static void
shell_view_ui_manager_changed_cb (EUIManager *ui_manager,
				  gpointer user_data)
{
	EShellView *self = user_data;

	shell_view_populate_new_menu (self);
}

static void
shell_view_update_view_menu (EShellView *self)
{
	EShellViewClass *shell_view_class;
	EUIActionGroup *action_group;
	EUIAction *action;
	GPtrArray *radio_group;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	gboolean visible;
	const gchar *view_id;
	gchar *delete_tooltip = NULL;
	gboolean view_exists = FALSE;
	gboolean delete_visible = FALSE;
	gint count, ii;

	shell_view_class = E_SHELL_VIEW_GET_CLASS (self);
	g_return_if_fail (shell_view_class != NULL);

	view_collection = shell_view_class->view_collection;
	view_id = e_shell_view_get_view_id (self);
	g_return_if_fail (view_collection != NULL);

	action_group = e_ui_manager_get_action_group (self->priv->ui_manager, "gal-view");

	e_ui_manager_freeze (self->priv->ui_manager);

	g_menu_remove_all (self->priv->gal_view_list_menu);
	e_ui_action_group_remove_all (action_group);

	/* We have a view ID, so forge ahead. */
	count = gal_view_collection_get_count (view_collection);

	radio_group = g_ptr_array_sized_new (1 + count);

	/* Prevent spurious activations. */
	action = e_ui_manager_get_action (self->priv->ui_manager, "gal-custom-view");
	g_signal_handlers_block_matched (
		action, G_SIGNAL_MATCH_FUNC, 0, 0,
		NULL, action_gal_view_cb, NULL);

	/* first unset, because overriding the group is considered a code error */
	e_ui_action_set_radio_group (action, NULL);
	e_ui_action_set_radio_group (action, radio_group);

	/* Add a menu item for each view collection item. */
	for (ii = 0; ii < count; ii++) {
		GalViewCollectionItem *item;
		GMenuItem *menu_item;
		gchar action_name[128];
		gchar *tooltip, *title;

		item = gal_view_collection_get_view_item (view_collection, ii);

		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "gal-view-%d", ii) < sizeof (action_name));
		title = e_str_without_underscores (item->title);
		tooltip = g_strdup_printf (_("Select view: %s"), title);

		action = e_ui_action_new_stateful (e_ui_action_group_get_name (action_group), action_name,
			G_VARIANT_TYPE_STRING, g_variant_new_string (item->id));
		e_ui_action_set_label (action, title);
		e_ui_action_set_tooltip (action, tooltip);
		e_ui_action_set_usable_for_kinds (action, 0);
		if (item->built_in && item->accelerator)
			e_ui_action_set_accel (action, item->accelerator);
		e_ui_action_set_radio_group (action, radio_group);

		if (g_strcmp0 (item->id, view_id) == 0) {
			view_exists = TRUE;
			g_free (delete_tooltip);
			delete_tooltip = g_strdup_printf (_("Delete view: %s"), title);
			delete_visible = (!item->built_in);
		}

		e_ui_action_group_add (action_group, action);

		menu_item = g_menu_item_new (NULL, NULL);
		e_ui_manager_update_item_from_action (self->priv->ui_manager, menu_item, action);
		g_menu_append_item (self->priv->gal_view_list_menu, menu_item);
		g_clear_object (&menu_item);

		g_free (tooltip);
		g_free (title);
	}

	action = e_ui_manager_get_action (self->priv->ui_manager, "gal-custom-view");
	e_ui_action_set_state (action, g_variant_new_string (view_exists ? view_id : ""));
	visible = e_ui_action_get_active (action);
	e_ui_action_set_visible (action, visible);

	g_signal_handlers_unblock_matched (
		action, G_SIGNAL_MATCH_FUNC, 0, 0,
		NULL, action_gal_view_cb, NULL);

	action = e_ui_manager_get_action (self->priv->ui_manager, "gal-save-custom-view");
	e_ui_action_set_visible (action, visible);

	view_instance = e_shell_view_get_view_instance (self);
	visible = view_instance && gal_view_instance_get_current_view (view_instance) &&
		GAL_IS_VIEW_ETABLE (gal_view_instance_get_current_view (view_instance));
	action = e_ui_manager_get_action (self->priv->ui_manager, "gal-customize-view");
	e_ui_action_set_visible (action, visible);

	action = e_ui_manager_get_action (self->priv->ui_manager, "gal-delete-view");
	e_ui_action_set_tooltip (action, delete_tooltip);
	e_ui_action_set_visible (action, delete_visible);

	e_ui_manager_thaw (self->priv->ui_manager);

	g_ptr_array_unref (radio_group);
	g_free (delete_tooltip);
}

static void
shell_view_notify_active_view_cb (GObject *object,
				  GParamSpec *param,
				  gpointer user_data)
{
	EShellView *self = user_data;
	GtkAccelGroup *accel_group;

	if (!self->priv->ui_manager)
		return;

	accel_group = e_ui_manager_get_accel_group (self->priv->ui_manager);

	/* Different views can use the same accelerator for their actions.
	   The gtk+ finds the first accel group with the accelerator and
	   activates it, but it does not go to other groups, if the chosen
	   one cannot be activated due to the view being inactive (in
	   the EUIManager::ignore-accel callback). */
	if (e_shell_view_is_active (self)) {
		if (!self->priv->accel_group_added) {
			self->priv->accel_group_added = TRUE;
			gtk_window_add_accel_group (GTK_WINDOW (object), accel_group);
		}
	} else if (self->priv->accel_group_added) {
		self->priv->accel_group_added = FALSE;
		gtk_window_remove_accel_group (GTK_WINDOW (object), accel_group);
	}
}

/**
 * e_shell_view_run_ui_customize_dialog:
 * @self: an #EShellView
 * @id: (nullable): optional ID of the part to be preselected, or %NULL
 *
 * Runs an #EUICustomizeDialog with optionally preselected part @id.
 *
 * Since: 3.56
 **/
void
e_shell_view_run_ui_customize_dialog (EShellView *self,
				      const gchar *id)
{
	EShellViewClass *klass;
	EShellWindow *shell_window;
	EUICustomizeDialog *dialog;

	g_return_if_fail (E_IS_SHELL_VIEW (self));

	klass = E_SHELL_VIEW_GET_CLASS (self);
	g_return_if_fail (klass != NULL);

	shell_window = e_shell_view_get_shell_window (self);
	dialog = e_ui_customize_dialog_new (shell_window ? GTK_WINDOW (shell_window) : NULL);

	e_ui_customize_dialog_add_customizer (dialog, e_ui_manager_get_customizer (self->priv->ui_manager));

	if (klass->add_ui_customizers)
		klass->add_ui_customizers (self, dialog);

	e_ui_customize_dialog_run (dialog, id);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
e_shell_view_customize_toolbar_activate_cb (GtkWidget *toolbar,
					    const gchar *id,
					    gpointer user_data)
{
	EShellView *self = user_data;

	g_return_if_fail (E_IS_SHELL_VIEW (self));

	e_shell_view_run_ui_customize_dialog (self, id);
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
		case PROP_SWITCHER_ACTION:
			shell_view_set_switcher_action (
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

		case PROP_MENUBAR_VISIBLE:
			e_shell_view_set_menubar_visible (
				E_SHELL_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SIDEBAR_VISIBLE:
			e_shell_view_set_sidebar_visible (
				E_SHELL_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SWITCHER_VISIBLE:
			e_shell_view_set_switcher_visible (
				E_SHELL_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_TASKBAR_VISIBLE:
			e_shell_view_set_taskbar_visible (
				E_SHELL_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_TOOLBAR_VISIBLE:
			e_shell_view_set_toolbar_visible (
				E_SHELL_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SIDEBAR_WIDTH:
			e_shell_view_set_sidebar_width (
				E_SHELL_VIEW (object),
				g_value_get_int (value));
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
		case PROP_SWITCHER_ACTION:
			g_value_set_object (
				value, e_shell_view_get_switcher_action (
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

		case PROP_MENUBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_view_get_menubar_visible (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SIDEBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_view_get_sidebar_visible (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SWITCHER_VISIBLE:
			g_value_set_boolean (
				value, e_shell_view_get_switcher_visible (
				E_SHELL_VIEW (object)));
			return;

		case PROP_TASKBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_view_get_taskbar_visible (
				E_SHELL_VIEW (object)));
			return;

		case PROP_TOOLBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_view_get_toolbar_visible (
				E_SHELL_VIEW (object)));
			return;

		case PROP_SIDEBAR_WIDTH:
			g_value_set_int (
				value, e_shell_view_get_sidebar_width (
				E_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_view_dispose (GObject *object)
{
	EShellView *self = E_SHELL_VIEW (object);

	/* Expedite any pending state saves. */
	e_shell_view_save_state_immediately (self);

	if (self->priv->update_actions_idle_id > 0) {
		g_source_remove (self->priv->update_actions_idle_id);
		self->priv->update_actions_idle_id = 0;
	}

	if (self->priv->state_save_activity != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->state_save_activity),
			&self->priv->state_save_activity);
		self->priv->state_save_activity = NULL;
	}

	if (self->priv->view_instance_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->view_instance,
			self->priv->view_instance_changed_handler_id);
		self->priv->view_instance_changed_handler_id = 0;
	}

	if (self->priv->view_instance_loaded_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->view_instance,
			self->priv->view_instance_loaded_handler_id);
		self->priv->view_instance_loaded_handler_id = 0;
	}

	if (self->priv->preferences_window != NULL) {
		g_signal_handler_disconnect (
			self->priv->preferences_window,
			self->priv->preferences_hide_handler_id);
		self->priv->preferences_hide_handler_id = 0;
	}

	if (self->priv->shell_window != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->shell_window), &self->priv->shell_window);
		self->priv->shell_window = NULL;
	}

	g_clear_object (&self->priv->view_instance);
	g_clear_object (&self->priv->shell_content);
	g_clear_object (&self->priv->shell_sidebar);
	g_clear_object (&self->priv->shell_taskbar);
	g_clear_object (&self->priv->searchbar);
	g_clear_object (&self->priv->search_rule);
	g_clear_object (&self->priv->preferences_window);
	g_clear_object (&self->priv->new_menu);
	g_clear_object (&self->priv->gal_view_list_menu);
	g_clear_object (&self->priv->saved_searches_menu);
	g_clear_object (&self->priv->headerbar);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_view_parent_class)->dispose (object);
}

static void
shell_view_finalize (GObject *object)
{
	EShellView *self = E_SHELL_VIEW (object);

	g_key_file_free (self->priv->state_key_file);

	g_clear_object (&self->priv->ui_manager);
	g_free (self->priv->title);
	g_free (self->priv->view_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_shell_view_parent_class)->finalize (object);
}

static void
shell_view_constructed (GObject *object)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellViewClass *shell_view_class;
	EUIAction *action;
	EUICustomizer *customizer;
	GtkWidget *widget;
	GtkPaned *paned;
	GSettings *settings;
	GObject *ui_item;
	gulong handler_id;
	const gchar *toolbar_id;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_shell_view_parent_class)->constructed (object);

	shell_view = E_SHELL_VIEW (object);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_if_fail (shell_view_class != NULL);

	e_ui_manager_freeze (shell_view->priv->ui_manager);

	customizer = e_ui_manager_get_customizer (shell_view->priv->ui_manager);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (shell_view), GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (shell_view), 0);

	g_signal_connect (shell_view->priv->ui_manager, "ignore-accel",
		G_CALLBACK (e_shell_view_ui_manager_ignore_accel_cb), shell_view);

	e_ui_manager_set_action_groups_widget (shell_view->priv->ui_manager, GTK_WIDGET (shell_view));

	e_signal_connect_notify_object (shell_view->priv->shell_window, "notify::active-view",
		G_CALLBACK (shell_view_notify_active_view_cb), shell_view, 0);

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	shell_view_load_state (shell_view);

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

	shell_view_init_ui_data (shell_view);

	ui_item = e_ui_manager_create_item (shell_view->priv->ui_manager, "main-menu");
	widget = gtk_menu_bar_new_from_model (G_MENU_MODEL (ui_item));
	g_clear_object (&ui_item);

	e_ui_customizer_register (customizer, "main-menu", NULL);

	shell_view->priv->menu_bar = e_menu_bar_new (GTK_MENU_BAR (widget), GTK_WINDOW (shell_view->priv->shell_window), &shell_view->priv->menu_button);
	gtk_box_pack_start (GTK_BOX (shell_view), widget, FALSE, FALSE, 0);

	g_signal_connect_object (widget, "deactivate",
		G_CALLBACK (shell_view_menubar_deactivate_cb), shell_view, 0);

	if (e_util_get_use_header_bar ()) {
		ui_item = e_ui_manager_create_item (shell_view->priv->ui_manager, "main-headerbar");
		shell_view->priv->headerbar = g_object_ref_sink (GTK_WIDGET (ui_item));

		e_ui_customizer_register (customizer, "main-headerbar", NULL);

		toolbar_id = "main-toolbar-with-headerbar";
	} else {
		toolbar_id = "main-toolbar-without-headerbar";
	}

	ui_item = e_ui_manager_create_item (shell_view->priv->ui_manager, toolbar_id);
	widget = GTK_WIDGET (ui_item);
	gtk_box_pack_start (GTK_BOX (shell_view), widget, FALSE, FALSE, 0);

	e_ui_customizer_register (customizer, toolbar_id, NULL);
	e_ui_customizer_util_attach_toolbar_context_menu (widget, toolbar_id,
		e_shell_view_customize_toolbar_activate_cb, shell_view);

	e_binding_bind_property (
		shell_view, "toolbar-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	widget = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (shell_view), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	paned = GTK_PANED (widget);

	e_binding_bind_property (shell_view, "sidebar-width",
		paned, "position",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	widget = shell_view_construct_taskbar (shell_view);
	gtk_box_pack_start (GTK_BOX (shell_view), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_shell_switcher_new ();
	shell_view->priv->switcher = g_object_ref_sink (widget);

	e_binding_bind_property (
		shell_view, "sidebar-visible",
		shell_view->priv->switcher, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		shell_view, "switcher-visible",
		shell_view->priv->switcher, "toolbar-visible",
		G_BINDING_SYNC_CREATE);

	gtk_container_add (GTK_CONTAINER (shell_view->priv->switcher), shell_view->priv->shell_sidebar);

	g_object_set (shell_view->priv->shell_content,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);

	gtk_paned_pack1 (paned, widget, FALSE, FALSE);
	gtk_paned_pack2 (paned, shell_view->priv->shell_content, TRUE, FALSE);

	e_shell_window_fill_switcher_actions (shell_view->priv->shell_window, shell_view->priv->ui_manager,
		E_SHELL_SWITCHER (shell_view->priv->switcher));

	shell_view_update_view_menu (shell_view);
	shell_view_update_search_menu (shell_view);

	/* Update actions whenever the Preferences window is closed. */
	widget = e_shell_get_preferences_window (shell);
	shell_view->priv->preferences_window = g_object_ref (widget);
	handler_id = g_signal_connect_swapped (
		shell_view->priv->preferences_window, "hide",
		G_CALLBACK (e_shell_view_update_actions_in_idle), shell_view);
	shell_view->priv->preferences_hide_handler_id = handler_id;

	/* use the first action in the group, to not have the value reset later */
	action = e_ui_manager_get_action (shell_view->priv->ui_manager, "switcher-style-both");
	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_settings_bind_with_mapping (settings, "buttons-style",
		action, "state",
		G_SETTINGS_BIND_DEFAULT,
		shell_view_button_style_to_state_cb,
		shell_view_state_to_button_style_cb,
		NULL, NULL);
	g_clear_object (&settings);

	g_signal_connect_object (action, "notify::state",
		G_CALLBACK (shell_view_switcher_style_cb), shell_view->priv->switcher, 0);
	shell_view_switcher_style_cb (action, NULL, shell_view->priv->switcher);

	e_signal_connect_notify (
		shell_view, "notify::view-id",
		G_CALLBACK (shell_view_update_view_menu), NULL);

	/* Register the EUIManager ID for the shell view. */
	e_plugin_ui_register_manager (shell_view->priv->ui_manager, shell_view_class->ui_manager_id, shell_view);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	if (shell_view->priv->headerbar)
		e_ui_manager_add_action_groups_to_widget (shell_view->priv->ui_manager, shell_view->priv->headerbar);

	e_ui_manager_thaw (shell_view->priv->ui_manager);
}

static GtkWidget *
shell_view_construct_searchbar (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellViewClass *shell_view_class;
	GtkWidget *widget;

	shell_content = e_shell_view_get_shell_content (shell_view);

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (shell_view_class != NULL, NULL);

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
	EUIAction *action;
	EUIActionGroup *action_group;

	g_return_if_fail (e_shell_view_is_active (shell_view));

	shell_window = e_shell_view_get_shell_window (shell_view);
	focus_tracker = e_shell_window_get_focus_tracker (shell_window);

	e_focus_tracker_update_actions (focus_tracker);

	action_group = e_ui_manager_get_action_group (shell_view->priv->ui_manager, "custom-rules");
	e_ui_action_group_set_sensitive (action_group, TRUE);

	action = e_ui_manager_get_action (shell_view->priv->ui_manager, "search-advanced");
	e_ui_action_set_sensitive (action, TRUE);
}

static void
e_shell_view_class_init (EShellViewClass *class)
{
	GObjectClass *object_class;

	e_shell_view_parent_class = g_type_class_peek_parent (class);
	if (EShellView_private_offset != 0)
		g_type_class_adjust_private_offset (class, &EShellView_private_offset);

	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "EShellView");

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

	class->clear_search = shell_view_clear_search;
	class->custom_search = shell_view_custom_search;
	class->update_actions = shell_view_update_actions;

	/**
	 * EShellView:switcher-action:
	 *
	 * The #EUIAction registered with #EShellSwitcher.
	 *
	 * Since: 3.56
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SWITCHER_ACTION,
		g_param_spec_object (
			"switcher-action",
			"Switcher Action",
			"The switcher action for this shell view",
			E_TYPE_UI_ACTION,
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
	 * EShellView:menubar-visible
	 *
	 * Whether the menu bar is visible.
	 *
	 * Since: 3.56
	 **/
	g_object_class_install_property (
		object_class,
		PROP_MENUBAR_VISIBLE,
		g_param_spec_boolean (
			"menubar-visible",
			"Menubar Visible",
			"Whether the shell view's menu bar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:sidebar-visible
	 *
	 * Whether the sidebar is visible.
	 *
	 * Since: 3.56
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SIDEBAR_VISIBLE,
		g_param_spec_boolean (
			"sidebar-visible",
			"Sidebar Visible",
			"Whether the shell view's sidebar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:switcher-visible
	 *
	 * Whether the switcher buttons are visible.
	 *
	 * Since: 3.56
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SWITCHER_VISIBLE,
		g_param_spec_boolean (
			"switcher-visible",
			"Switcher Visible",
			"Whether the shell view's switcher buttons are visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:taskbar-visible
	 *
	 * Whether the task bar is visible.
	 *
	 * Since: 3.56
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TASKBAR_VISIBLE,
		g_param_spec_boolean (
			"taskbar-visible",
			"Taskbar Visible",
			"Whether the shell view's task bar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:toolbar-visible
	 *
	 * Whether the tool bar is visible.
	 *
	 * Since: 3.56
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_VISIBLE,
		g_param_spec_boolean (
			"toolbar-visible",
			"Toolbar Visible",
			"Whether the shell view's tool bar is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellView:sidebar-width
	 *
	 * Width of the sidebar, in pixels.
	 *
	 * Since: 3.56
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SIDEBAR_WIDTH,
		g_param_spec_int (
			"sidebar-width",
			"Sidebar Width",
			"Width of the sidebar, in pixels",
			0, G_MAXINT, 128,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

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
		G_SIGNAL_RUN_LAST,
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
	 * other aspect of the actions they have registered.
	 *
	 * Plugins can also connect to this signal to be notified
	 * when to update their own actions.
	 **/
	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellViewClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/**
	 * EShellView::init-ui-data
	 * @shell_view: the #EShellView which emitted the signal
	 *
	 * #EShellView subclasses should override the
	 * <structfield>init_ui_data</structfield> method in
	 * #EShellViewClass to add any actions and UI definitions.
	 * The @shell_view automatically adds UI definition from
	 * <structfield>ui_definition</structfield> class property
	 * after this signal is emitted.
	 *
	 * Since: 3.56
	 **/
	signals[INIT_UI_DATA] = g_signal_new (
		"init-ui-data",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EShellViewClass, init_ui_data),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_shell_view_init (EShellView *shell_view,
                   EShellViewClass *class)
{
	EShellBackendClass *backend_class;
	GtkStyleContext *style_context;
	GtkCssProvider *provider;
	GError *local_error = NULL;

	backend_class = E_SHELL_BACKEND_GET_CLASS (class->shell_backend);
	g_return_if_fail (backend_class != NULL);

	/* XXX Our use of GInstanceInitFunc's 'class' parameter
	 *     prevents us from using G_DEFINE_ABSTRACT_TYPE. */

	if (class->search_context == NULL)
		shell_view_init_search_context (class);

	if (class->view_collection == NULL)
		shell_view_init_view_collection (class);

	shell_view->priv = e_shell_view_get_instance_private (shell_view);
	shell_view->priv->main_thread = g_thread_self ();
	shell_view->priv->state_key_file = g_key_file_new ();
	shell_view->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
	shell_view->priv->ui_manager = e_ui_manager_new (e_ui_customizer_util_dup_filename_for_component (backend_class->name));
	shell_view->priv->new_menu = g_menu_new ();
	shell_view->priv->gal_view_list_menu = g_menu_new ();
	shell_view->priv->saved_searches_menu = g_menu_new ();

	provider = gtk_css_provider_new ();

	if (!gtk_css_provider_load_from_data (provider, "EShellView { padding:0px; margin:0px; border:0px; }", -1, &local_error))
		g_critical ("%s: Failed to load CSS data: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);

	style_context = gtk_widget_get_style_context (GTK_WIDGET (shell_view));
	gtk_style_context_add_provider (style_context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_clear_object (&provider);

	gtk_widget_set_visible (GTK_WIDGET (shell_view), TRUE);

	g_signal_connect (shell_view->priv->ui_manager, "create-item",
		G_CALLBACK (shell_view_ui_manager_create_item_cb), shell_view);
	g_signal_connect (shell_view->priv->ui_manager, "changed",
		G_CALLBACK (shell_view_ui_manager_changed_cb), shell_view);
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
			GTK_TYPE_BOX, "EShellView",
			&type_info, G_TYPE_FLAG_ABSTRACT);

		EShellView_private_offset = g_type_add_instance_private (type, sizeof (EShellViewPrivate));

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
	EUIAction *action;
	GVariant *target;
	const gchar *view_name;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	action = e_shell_view_get_switcher_action (shell_view);
	target = e_ui_action_ref_target (action);
	view_name = g_variant_get_string (target, NULL);
	/* technically speaking, the `view_name` can vanish after the following line, but
	   the `target` is left unchanged and alive together with the `action`, which
	   the `shell_view` owns */
	g_clear_pointer (&target, g_variant_unref);

	return view_name;
}

/**
 * e_shell_view_get_ui_manager:
 * @shell_view: an #EShellView
 *
 * Returns the view's #EUIManager. It can be used to add UI elemenrs
 * related to the @shell_view itself.
 *
 * Returns: (transfer none): the view's UI manager
 *
 * Since: 3.56
 **/
EUIManager *
e_shell_view_get_ui_manager (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->ui_manager;
}

/**
 * e_shell_view_get_action:
 * @shell_view: an #EShellView
 * @name: action name to get
 *
 * Returns an @EUIAction named @name.
 *
 * Returns: (transfer none) (nullable): an @EUIAction named @name, or %NULL,
 *    when no such exists
 *
 * Since: 3.56
 **/
EUIAction *
e_shell_view_get_action (EShellView *shell_view,
			 const gchar *name)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return e_ui_manager_get_action (shell_view->priv->ui_manager, name);
}

/**
 * e_shell_view_get_switcher_action:
 * @shell_view: an #EShellView
 *
 * Returns the switcher action for @shell_view.
 *
 * An #EShellWindow creates an #EUIAction for each registered subclass
 * of #EShellView.  This action gets passed to the #EShellSwitcher, which
 * displays a button that proxies the action.  The icon at the top of the
 * sidebar also proxies the action.  When @shell_view is active, the
 * action's icon becomes the #EShellWindow icon.
 *
 * Returns: (transfer none): the switcher action for @shell_view
 *
 * Since: 3.56
 **/
EUIAction *
e_shell_view_get_switcher_action (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->switcher_action;
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

	if (!title) {
		EShellViewClass *klass = E_SHELL_VIEW_GET_CLASS (shell_view);
		g_return_if_fail (klass != NULL);

		title = klass->label;
	}

	if (g_strcmp0 (shell_view->priv->title, title) == 0)
		return;

	g_free (shell_view->priv->title);
	shell_view->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (shell_view), "title");
}

static void
shell_view_menubar_info_response_cb (EAlert *alert,
				     gint response_id,
				     gpointer user_data)
{
	GWeakRef *weakref = user_data;

	g_return_if_fail (weakref != NULL);

	if (response_id == GTK_RESPONSE_APPLY) {
		EShellView *shell_view;

		shell_view = g_weak_ref_get (weakref);
		if (shell_view) {
			e_shell_view_set_menubar_visible (shell_view, TRUE);
			g_object_unref (shell_view);
		}
	}
}

/**
 * e_shell_view_get_menubar_visible:
 * @shell_view: an #EShellView
 *
 * Returns %TRUE if @shell_view<!-- -->'s menu bar is visible.
 *
 * Returns: %TRUE is the menu bar is visible
 *
 * Since: 3.56
 **/
gboolean
e_shell_view_get_menubar_visible (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->menu_bar &&
		e_menu_bar_get_visible (shell_view->priv->menu_bar);
}

/**
 * e_shell_view_set_menubar_visible:
 * @shell_view: an #EShellView
 * @menubar_visible: whether the menu bar should be visible
 *
 * Makes @shell_view<!-- -->'s menu bar visible or invisible.
 *
 * Since: 3.56
 **/
void
e_shell_view_set_menubar_visible (EShellView *shell_view,
				  gboolean menubar_visible)
{
	GSettings *settings;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if ((e_shell_view_get_menubar_visible (shell_view) ? 1 : 0) == (menubar_visible ? 1 : 0))
		return;

	e_menu_bar_set_visible (shell_view->priv->menu_bar, menubar_visible);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	if (!menubar_visible &&
	    g_settings_get_boolean (settings, e_shell_window_is_main_instance (shell_view->priv->shell_window) ? "menubar-visible" : "menubar-visible-sub")) {
		/* The menu bar had been just hidden. Show a hint how to enable it. */
		EShellContent *shell_content;
		EAlert *alert;

		shell_content = e_shell_view_get_shell_content (shell_view);

		alert = e_alert_new ("shell:menubar-hidden", NULL);

		g_signal_connect_data (alert, "response", G_CALLBACK (shell_view_menubar_info_response_cb),
			e_weak_ref_new (shell_view), (GClosureNotify) e_weak_ref_free, 0);

		e_alert_sink_submit_alert (E_ALERT_SINK (shell_content), alert);
		e_alert_start_timer (alert, 30);
		g_object_unref (alert);
	}
	g_object_unref (settings);

	g_object_notify (G_OBJECT (shell_view), "menubar-visible");
}

/**
 * e_shell_view_get_sidebar_visible:
 * @shell_view: an #EShellView
 *
 * Returns %TRUE if @shell_view<!-- -->'s sidebar is visible.
 *
 * Returns: %TRUE is the sidebar is visible
 *
 * Since: 3.56
 **/
gboolean
e_shell_view_get_sidebar_visible (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->sidebar_visible;
}

/**
 * e_shell_view_set_sidebar_visible:
 * @shell_view: an #EShellView
 * @sidebar_visible: whether the sidebar should be visible
 *
 * Makes @shell_view<!-- -->'s sidebar visible or invisible.
 *
 * Since: 3.56
 **/
void
e_shell_view_set_sidebar_visible (EShellView *shell_view,
				  gboolean sidebar_visible)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->sidebar_visible == sidebar_visible)
		return;

	shell_view->priv->sidebar_visible = sidebar_visible;

	g_object_notify (G_OBJECT (shell_view), "sidebar-visible");
}

/**
 * e_shell_view_get_switcher_visible:
 * @shell_view: an #EShellView
 *
 * Returns %TRUE if @shell_view<!-- -->'s switcher buttons are visible.
 *
 * Returns: %TRUE is the switcher buttons are visible
 *
 * Since: 3.56
 **/
gboolean
e_shell_view_get_switcher_visible (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->switcher_visible;
}

/**
 * e_shell_view_set_switcher_visible:
 * @shell_view: an #EShellView
 * @switcher_visible: whether the switcher buttons should be visible
 *
 * Makes @shell_view<!-- -->'s switcher buttons visible or invisible.
 *
 * Since: 3.56
 **/
void
e_shell_view_set_switcher_visible (EShellView *shell_view,
				   gboolean switcher_visible)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->switcher_visible == switcher_visible)
		return;

	shell_view->priv->switcher_visible = switcher_visible;

	g_object_notify (G_OBJECT (shell_view), "switcher-visible");
}

/**
 * e_shell_view_get_taskbar_visible:
 * @shell_view: an #EShellView
 *
 * Returns %TRUE if @shell_view<!-- -->'s task bar is visible.
 *
 * Returns: %TRUE is the task bar is visible
 *
 * Since: 3.56
 **/
gboolean
e_shell_view_get_taskbar_visible (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->taskbar_visible;
}

/**
 * e_shell_view_set_taskbar_visible:
 * @shell_view: an #EShellView
 * @taskbar_visible: whether the task bar should be visible
 *
 * Makes @shell_view<!-- -->'s task bar visible or invisible.
 *
 * Since: 3.56
 **/
void
e_shell_view_set_taskbar_visible (EShellView *shell_view,
				  gboolean taskbar_visible)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->taskbar_visible == taskbar_visible)
		return;

	shell_view->priv->taskbar_visible = taskbar_visible;

	g_object_notify (G_OBJECT (shell_view), "taskbar-visible");
}

/**
 * e_shell_view_get_toolbar_visible:
 * @shell_view: an #EShellView
 *
 * Returns %TRUE if @shell_view<!-- -->'s tool bar is visible.
 *
 * Returns: %TRUE if the tool bar is visible
 *
 * Since: 3.56
 **/
gboolean
e_shell_view_get_toolbar_visible (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	return shell_view->priv->toolbar_visible;
}

/**
 * e_shell_view_set_toolbar_visible:
 * @shell_view: an #EShellView
 * @toolbar_visible: whether the tool bar should be visible
 *
 * Makes @shell_view<!-- -->'s tool bar visible or invisible.
 *
 * Since: 3.56
 **/
void
e_shell_view_set_toolbar_visible (EShellView *shell_view,
				  gboolean toolbar_visible)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->toolbar_visible == toolbar_visible)
		return;

	shell_view->priv->toolbar_visible = toolbar_visible;

	g_object_notify (G_OBJECT (shell_view), "toolbar-visible");
}

/**
 * e_shell_view_get_sidebar_width:
 * @shell_view: an #EShellView
 *
 * Gets width of the sidebar, in pixels.
 *
 * Returns: width of the sidebar, in pixels
 *
 * Since: 3.56
 **/
gint
e_shell_view_get_sidebar_width (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), 0);

	return shell_view->priv->sidebar_width;
}

/**
 * e_shell_view_set_sidebar_width:
 * @shell_view: an #EShellView
 * @width: width in pixels
 *
 * Sets width of the sidebar, in pixels.
 *
 * Since: 3.56
 **/
void
e_shell_view_set_sidebar_width (EShellView *shell_view,
				gint width)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (shell_view->priv->sidebar_width == width)
		return;

	shell_view->priv->sidebar_width = width;

	g_object_notify (G_OBJECT (shell_view), "sidebar-width");
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
	g_return_val_if_fail (class != NULL, NULL);

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
 * e_shell_view_get_headerbar:
 * @shell_view: an #EShellView
 *
 * Returns a header bar widget for the @shell_view.
 *
 * Returns: (transfer none) (nullable): a header bar widget for the @shell_view,
 *    or #NULL, when none is needed for it.
 *
 * Since: 3.56
 **/
GtkWidget *
e_shell_view_get_headerbar (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return shell_view->priv->headerbar;
}

/**
 * e_shell_view_is_active:
 * @shell_view: an #EShellView
 *
 * Returns %TRUE if @shell_view is active.  That is, if it's currently
 * visible in its #EShellWindow.  An #EShellWindow can only display one
 * shell view at a time.
 *
 * Technically this just checks the shell view's switcher action "active" property.
 * See e_shell_view_get_switcher_action().
 *
 * Returns: %TRUE if @shell_view is active
 **/
gboolean
e_shell_view_is_active (EShellView *shell_view)
{
	EUIAction *action;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), FALSE);

	action = e_shell_view_get_switcher_action (shell_view);

	return e_ui_action_get_active (action);
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
	g_return_val_if_fail (class != NULL, NULL);
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
	g_return_val_if_fail (class != NULL, NULL);
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
 * to update the various actions based on the current
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

		e_ui_manager_freeze (shell_view->priv->ui_manager);

		g_signal_emit (shell_view, signals[UPDATE_ACTIONS], 0);

		e_ui_manager_thaw (shell_view->priv->ui_manager);
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
 * @menu_name: name of the menu in the UI definition
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
 * The returned #GtkMenu is automatically destroyed once the user
 * either selects an item from it or when it's dismissed.
 *
 * Returns: (transfer none): the popup menu being displayed
 *
 * Since: 3.56
 **/
GtkWidget *
e_shell_view_show_popup_menu (EShellView *shell_view,
                              const gchar *menu_name,
                              GdkEvent *button_event)
{
	GObject *ui_item;
	GtkWidget *widget;
	GtkMenu *menu;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	e_shell_view_update_actions (shell_view);

	ui_item = e_ui_manager_create_item (shell_view->priv->ui_manager, menu_name);

	if (!ui_item) {
		g_warning ("%s: Cannot find menu '%s' in %s", G_STRFUNC, menu_name, G_OBJECT_TYPE_NAME (shell_view));
		return NULL;
	}

	if (!G_IS_MENU_MODEL (ui_item)) {
		g_warning ("%s: Object '%s' is not a GMenuItem, but %s instead", G_STRFUNC, menu_name, G_OBJECT_TYPE_NAME (ui_item));
		g_clear_object (&ui_item);
		return NULL;
	}

	widget = gtk_menu_new_from_model (G_MENU_MODEL (ui_item));
	g_clear_object (&ui_item);

	menu = GTK_MENU (widget);

	gtk_menu_attach_to_widget (menu, GTK_WIDGET (shell_view), NULL);
	e_util_connect_menu_detach_after_deactivate (menu);

	gtk_menu_popup_at_pointer (menu, button_event);

	return widget;
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

gboolean
e_shell_view_util_layout_to_state_cb (GValue *value,
				      GVariant *variant,
				      gpointer user_data)
{
	g_value_set_variant (value, variant);
	return TRUE;
}

GVariant *
e_shell_view_util_state_to_layout_cb (const GValue *value,
				      const GVariantType *expected_type,
				      gpointer user_data)
{
	GVariant *var = g_value_get_variant (value);
	if (var)
		g_variant_ref_sink (var);
	return var;
}
