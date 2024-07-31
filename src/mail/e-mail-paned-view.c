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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include <shell/e-shell-window-actions.h>

#include <libemail-engine/libemail-engine.h>

#include "em-utils.h"
#include "message-list.h"
#include "e-mail-reader-utils.h"

#include "e-mail-paned-view.h"

#define E_SHELL_WINDOW_ACTION_GROUP_MAIL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail")

struct _EMailPanedViewPrivate {
	GtkWidget *paned;
	GtkWidget *scrolled_window;
	GtkWidget *message_list;
	GtkWidget *preview_pane;
	GtkWidget *preview_toolbar_box;

	EMailDisplay *display;
	GalViewInstance *view_instance;

	/* ETable scrolling hack */
	gdouble default_scrollbar_position;

	guint paned_binding_id;

	/* Signal handler IDs */
	guint message_list_built_id;

	/* TRUE when folder had been just set */
	gboolean folder_just_set;
	gchar *last_selected_uid;

	gboolean preview_toolbar_visible;
};

enum {
	PROP_0,
	PROP_FORWARD_STYLE,
	PROP_GROUP_BY_THREADS,
	PROP_REPLY_STYLE,
	PROP_MARK_SEEN_ALWAYS,
	PROP_DELETE_SELECTS_PREVIOUS,
	PROP_PREVIEW_TOOLBAR_VISIBLE
};

#define STATE_KEY_GROUP_BY_THREADS	"GroupByThreads"
#define STATE_KEY_SELECTED_MESSAGE	"SelectedMessage"
#define STATE_KEY_PREVIEW_VISIBLE	"PreviewVisible"
#define STATE_GROUP_GLOBAL_FOLDER	"GlobalFolder"

/* Forward Declarations */
static void e_mail_paned_view_reader_init (EMailReaderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailPanedView, e_mail_paned_view, E_TYPE_MAIL_VIEW,
	G_ADD_PRIVATE (EMailPanedView)
	G_IMPLEMENT_INTERFACE (E_TYPE_MAIL_READER, e_mail_paned_view_reader_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static void
mail_paned_view_save_boolean (EMailView *view,
                              const gchar *key,
                              gboolean value)
{
	EMailReader *reader;
	CamelFolder *folder;

	reader = E_MAIL_READER (view);
	folder = e_mail_reader_ref_folder (reader);

	if (folder != NULL) {
		EShellView *shell_view;
		GKeyFile *key_file;
		gchar *folder_uri;
		gchar *group_name;

		shell_view = e_mail_view_get_shell_view (view);
		key_file = e_shell_view_get_state_key_file (shell_view);

		folder_uri = e_mail_folder_uri_from_folder (folder);
		group_name = g_strdup_printf ("Folder %s", folder_uri);
		g_key_file_set_boolean (key_file, group_name, key, value);
		g_free (group_name);
		g_free (folder_uri);

		g_key_file_set_boolean (
			key_file, STATE_GROUP_GLOBAL_FOLDER, key, value);

		e_shell_view_set_state_dirty (shell_view);

		g_object_unref (folder);
	}
}

static gboolean
mail_paned_view_message_list_is_empty (MessageList *message_list)
{
	ETreeModel *model;
	ETreePath root;

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), TRUE);

	model = e_tree_get_model (E_TREE (message_list));
	if (!model)
		return TRUE;

	root = e_tree_model_get_root (model);
	if (!root)
		return TRUE;

	return !e_tree_model_node_get_first_child (model, root);
}

static void
mail_paned_view_message_list_built_cb (EMailView *view,
                                       MessageList *message_list)
{
	EMailPanedView *self = E_MAIL_PANED_VIEW (view);
	EShellView *shell_view;
	EShellWindow *shell_window;
	CamelFolder *folder;
	GKeyFile *key_file;
	gboolean ensure_message_selected;

	ensure_message_selected = self->priv->folder_just_set;
	self->priv->folder_just_set = FALSE;

	folder = message_list_ref_folder (message_list);

	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	key_file = e_shell_view_get_state_key_file (shell_view);

	if (message_list->cursor_uid != NULL)
		;  /* do nothing */

	else if (folder == NULL)
		;  /* do nothing */

	else if (e_shell_window_get_safe_mode (shell_window))
		e_shell_window_set_safe_mode (shell_window, FALSE);

	else {
		gboolean with_fallback = TRUE;
		gchar *uid = NULL;

		/* This is for regen when setting filter, or when folder changed or such */
		if (!ensure_message_selected &&
		    !message_list_selected_count (message_list) &&
		    !mail_paned_view_message_list_is_empty (message_list)) {
			ensure_message_selected = TRUE;
			with_fallback = FALSE;

			if (self->priv->last_selected_uid &&
			    message_list_contains_uid (message_list, self->priv->last_selected_uid)) {
				g_free (uid);
				uid = g_strdup (self->priv->last_selected_uid);
			}
		}

		/* This is to prefer last selected message from the previous search folder
		   over the stored message. The _set_folder() makes sure to unset
		   priv->last_selected_uid, when it's not from this folder. */
		if (ensure_message_selected && !uid && self->priv->last_selected_uid &&
		    message_list_contains_uid (message_list, self->priv->last_selected_uid)) {
			uid = g_strdup (self->priv->last_selected_uid);
		}

		if (ensure_message_selected && !uid) {
			const gchar *key;
			gchar *folder_uri;
			gchar *group_name;

			folder_uri = e_mail_folder_uri_from_folder (folder);

			key = STATE_KEY_SELECTED_MESSAGE;
			group_name = g_strdup_printf ("Folder %s", folder_uri);
			uid = g_key_file_get_string (key_file, group_name, key, NULL);
			g_free (group_name);

			g_free (folder_uri);
		}

		if (ensure_message_selected && !message_list_contains_uid (message_list, uid) &&
		    e_mail_reader_get_mark_seen_always (E_MAIL_READER (view)))
			e_mail_reader_unset_folder_just_selected (E_MAIL_READER (view));

		if (ensure_message_selected)
			message_list_select_uid (message_list, uid, with_fallback);

		g_free (uid);
	}

	g_clear_object (&folder);
}

static void
mail_paned_view_message_selected_cb (EMailView *view,
                                     const gchar *message_uid,
                                     MessageList *message_list)
{
	EShellView *shell_view;
	CamelFolder *folder;
	GKeyFile *key_file;
	const gchar *key;
	gchar *folder_uri;
	gchar *group_name;

	folder = message_list_ref_folder (message_list);

	/* This also gets triggered when selecting a store name on
	 * the sidebar such as "On This Computer", in which case
	 * 'folder' will be NULL. */
	if (folder == NULL)
		return;

	shell_view = e_mail_view_get_shell_view (view);
	key_file = e_shell_view_get_state_key_file (shell_view);

	folder_uri = e_mail_folder_uri_from_folder (folder);

	key = STATE_KEY_SELECTED_MESSAGE;
	group_name = g_strdup_printf ("Folder %s", folder_uri);

	/* Only overwrite changes, do not delete the stored selected message,
	   when the current view has nothing (or multiple messages) selected. */
	if (message_uid != NULL) {
		EMailPanedView *paned_view = E_MAIL_PANED_VIEW (view);

		g_key_file_set_string (key_file, group_name, key, message_uid);

		g_clear_pointer (&paned_view->priv->last_selected_uid, g_free);
		paned_view->priv->last_selected_uid = g_strdup (message_uid);
	}

	e_shell_view_set_state_dirty (shell_view);

	g_free (group_name);
	g_free (folder_uri);

	g_object_unref (folder);
}

/* To recognize old values from new values */
#define PROPORTION_LOWER_LIMIT 1000000

static gboolean
mail_paned_view_map_setting_to_proportion_cb (GValue *value,
					      GVariant *variant,
					      gpointer user_data)
{
	gint stored;
	gdouble proportion = 0.5;

	stored = g_variant_get_int32 (variant);

	if (stored >= PROPORTION_LOWER_LIMIT)
		proportion = (stored - PROPORTION_LOWER_LIMIT) / ((gdouble) PROPORTION_LOWER_LIMIT);

	g_value_set_double (value, proportion);

	return TRUE;
}

static GVariant *
mail_paned_view_map_proportion_to_setting_cb (const GValue *value,
					      const GVariantType *expected_type,
					      gpointer user_data)
{
	gdouble proportion;

	proportion = g_value_get_double (value);

	return g_variant_new_int32 (PROPORTION_LOWER_LIMIT + (gint32) (proportion * PROPORTION_LOWER_LIMIT));
}

static void
mail_paned_view_notify_orientation_cb (GtkWidget *paned,
				       GParamSpec *param,
				       EShellWindow *shell_window)
{
	GSettings *settings;
	const gchar *settings_key;
	guint32 add_flags = 0;

	g_return_if_fail (E_IS_PANED (paned));
	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	g_settings_unbind (paned, "proportion");

	if (e_shell_window_is_main_instance (shell_window)) {
		if (gtk_orientable_get_orientation (GTK_ORIENTABLE (paned)) == GTK_ORIENTATION_HORIZONTAL)
			settings_key = "hpaned-size";
		else
			settings_key = "paned-size";
	} else {
		if (gtk_orientable_get_orientation (GTK_ORIENTABLE (paned)) == GTK_ORIENTATION_HORIZONTAL)
			settings_key = "hpaned-size-sub";
		else
			settings_key = "paned-size-sub";

		add_flags = G_SETTINGS_BIND_GET_NO_CHANGES;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind_with_mapping (settings, settings_key,
		paned, "proportion",
		G_SETTINGS_BIND_DEFAULT | add_flags,
		mail_paned_view_map_setting_to_proportion_cb,
		mail_paned_view_map_proportion_to_setting_cb,
		NULL, NULL);

	g_object_unref (settings);
}

static void
mail_paned_view_restore_state_cb (EShellWindow *shell_window,
                                  EShellView *shell_view,
                                  EMailPanedView *view)
{
	EMailPanedViewPrivate *priv;

	priv = E_MAIL_PANED_VIEW (view)->priv;

	g_signal_connect (priv->paned, "notify::orientation",
		G_CALLBACK (mail_paned_view_notify_orientation_cb), shell_window);

	mail_paned_view_notify_orientation_cb (priv->paned, NULL, shell_window);
}

static void
mail_paned_display_view_cb (GalViewInstance *view_instance,
                            GalView *gal_view,
                            EMailView *view)
{
	EMailReader *reader;
	EShellView *shell_view;
	GtkWidget *message_list;

	shell_view = e_mail_view_get_shell_view (view);
	e_shell_view_set_view_instance (shell_view, view_instance);

	reader = E_MAIL_READER (view);
	message_list = e_mail_reader_get_message_list (reader);

	if (GAL_IS_VIEW_ETABLE (gal_view))
		gal_view_etable_attach_tree (
			GAL_VIEW_ETABLE (gal_view),
			E_TREE (message_list));
}

static void
mail_paned_view_notify_group_by_threads_cb (EMailReader *reader)
{
	gboolean group_by_threads;

	group_by_threads = e_mail_reader_get_group_by_threads (reader);

	mail_paned_view_save_boolean (
		E_MAIL_VIEW (reader),
		STATE_KEY_GROUP_BY_THREADS, group_by_threads);
}

static void
mail_paned_view_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FORWARD_STYLE:
			e_mail_reader_set_forward_style (
				E_MAIL_READER (object),
				g_value_get_enum (value));
			return;

		case PROP_GROUP_BY_THREADS:
			e_mail_reader_set_group_by_threads (
				E_MAIL_READER (object),
				g_value_get_boolean (value));
			return;

		case PROP_REPLY_STYLE:
			e_mail_reader_set_reply_style (
				E_MAIL_READER (object),
				g_value_get_enum (value));
			return;

		case PROP_MARK_SEEN_ALWAYS:
			e_mail_reader_set_mark_seen_always (
				E_MAIL_READER (object),
				g_value_get_boolean (value));
			return;

		case PROP_DELETE_SELECTS_PREVIOUS:
			e_mail_reader_set_delete_selects_previous (
				E_MAIL_READER (object),
				g_value_get_boolean (value));
			return;

		case PROP_PREVIEW_TOOLBAR_VISIBLE:
			e_mail_paned_view_set_preview_toolbar_visible (
				E_MAIL_PANED_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_paned_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_FORWARD_STYLE:
			g_value_set_enum (
				value,
				e_mail_reader_get_forward_style (
				E_MAIL_READER (object)));
			return;

		case PROP_GROUP_BY_THREADS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_group_by_threads (
				E_MAIL_READER (object)));
			return;

		case PROP_REPLY_STYLE:
			g_value_set_enum (
				value,
				e_mail_reader_get_reply_style (
				E_MAIL_READER (object)));
			return;

		case PROP_MARK_SEEN_ALWAYS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_mark_seen_always (
				E_MAIL_READER (object)));
			return;

		case PROP_DELETE_SELECTS_PREVIOUS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_delete_selects_previous (
				E_MAIL_READER (object)));
			return;

		case PROP_PREVIEW_TOOLBAR_VISIBLE:
			g_value_set_boolean (
				value,
				e_mail_paned_view_get_preview_toolbar_visible (
				E_MAIL_PANED_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_paned_view_dispose (GObject *object)
{
	EMailPanedView *self = E_MAIL_PANED_VIEW (object);

	e_mail_reader_dispose (E_MAIL_READER (object));

	g_clear_object (&self->priv->paned);
	g_clear_object (&self->priv->scrolled_window);

	if (self->priv->message_list != NULL) {
		/* It can be disconnected by EMailReader in e_mail_reader_dispose() */
		if (self->priv->message_list_built_id &&
		    g_signal_handler_is_connected (self->priv->message_list, self->priv->message_list_built_id)) {
			g_signal_handler_disconnect (self->priv->message_list, self->priv->message_list_built_id);
		}

		self->priv->message_list_built_id = 0;

		g_clear_object (&self->priv->message_list);
	}

	g_clear_object (&self->priv->preview_pane);
	g_clear_object (&self->priv->preview_toolbar_box);
	g_clear_object (&self->priv->view_instance);

	g_clear_pointer (&self->priv->last_selected_uid, g_free);

	self->priv->display = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_paned_view_parent_class)->dispose (object);
}

static EAlertSink *
mail_paned_view_get_alert_sink (EMailReader *reader)
{
	EMailView *view;
	EShellView *shell_view;
	EShellContent *shell_content;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	return E_ALERT_SINK (shell_content);
}

static EMailBackend *
mail_paned_view_get_backend (EMailReader *reader)
{
	EMailView *view;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	return E_MAIL_BACKEND (shell_backend);
}

static EMailDisplay *
mail_paned_view_get_mail_display (EMailReader *reader)
{
	EMailPanedViewPrivate *priv;

	priv = E_MAIL_PANED_VIEW (reader)->priv;

	return priv->display;
}

static gboolean
mail_paned_view_get_hide_deleted (EMailReader *reader)
{
	return !e_mail_view_get_show_deleted (E_MAIL_VIEW (reader));
}

static GtkWidget *
mail_paned_view_get_message_list (EMailReader *reader)
{
	EMailPanedView *paned_view;

	paned_view = E_MAIL_PANED_VIEW (reader);

	return paned_view->priv->message_list;
}

static EUIManager *
mail_paned_view_get_ui_manager (EMailReader *reader)
{
	EShellView *shell_view;

	shell_view = e_mail_view_get_shell_view (E_MAIL_VIEW (reader));

	return e_shell_view_get_ui_manager (shell_view);
}

static EPreviewPane *
mail_paned_view_get_preview_pane (EMailReader *reader)
{
	EMailPanedView *paned_view;

	paned_view = E_MAIL_PANED_VIEW (reader);

	return E_PREVIEW_PANE (paned_view->priv->preview_pane);
}

static GtkWindow *
mail_paned_view_get_window (EMailReader *reader)
{
	EMailView *view;
	EShellView *shell_view;
	EShellWindow *shell_window;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return GTK_WINDOW (shell_window);
}

static void
mail_paned_view_set_folder (EMailReader *reader,
                            CamelFolder *folder)
{
	EMailPanedView *self = E_MAIL_PANED_VIEW (reader);
	EMailView *view;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	GSettings *settings;
	EMailReaderInterface *default_interface;
	GtkWidget *message_list;
	GKeyFile *key_file;
	CamelFolder *previous_folder;
	gchar *folder_uri;
	gchar *group_name;
	const gchar *key;
	gboolean value, global_view_setting;
	GError *error = NULL;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);

	/* Can be NULL, if the shell window was closed meanwhile */
	if (!shell_view)
		return;

	previous_folder = e_mail_reader_ref_folder (reader);
	if (previous_folder == folder) {
		g_clear_object (&previous_folder);
		return;
	}

	if (self->priv->last_selected_uid && previous_folder && folder &&
	    CAMEL_IS_VEE_FOLDER (previous_folder)) {
		CamelFolder *real_folder = NULL;
		gchar *message_uid = NULL;

		em_utils_get_real_folder_and_message_uid (previous_folder, self->priv->last_selected_uid, &real_folder, NULL, &message_uid);

		g_clear_pointer (&self->priv->last_selected_uid, g_free);

		if (real_folder == folder && message_uid) {
			self->priv->last_selected_uid = message_uid;
			message_uid = NULL;
		}

		g_free (message_uid);
		g_clear_object (&real_folder);
	} else {
		g_clear_pointer (&self->priv->last_selected_uid, g_free);
	}

	g_clear_object (&previous_folder);

	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* FIXME This should be an EMailReader property. */
	global_view_setting = g_settings_get_boolean (
		settings, "global-view-setting");

	message_list = e_mail_reader_get_message_list (reader);

	message_list_freeze (MESSAGE_LIST (message_list));

	/* Chain up to interface's default set_folder() method. */
	default_interface = g_type_default_interface_peek (E_TYPE_MAIL_READER);
	default_interface->set_folder (reader, folder);

	if (folder == NULL)
		goto exit;

	/* Only refresh the folder if we're online. */
	if (e_shell_get_online (shell))
		e_mail_reader_refresh_folder (reader, folder);

	self->priv->folder_just_set = TRUE;

	/* Restore the folder's preview and threaded state. */

	folder_uri = e_mail_folder_uri_from_folder (folder);
	key_file = e_shell_view_get_state_key_file (shell_view);
	group_name = g_strdup_printf ("Folder %s", folder_uri);
	g_free (folder_uri);

	key = STATE_KEY_GROUP_BY_THREADS;
	value = g_key_file_get_boolean (key_file, global_view_setting ? STATE_GROUP_GLOBAL_FOLDER : group_name, key, &error);
	if (error != NULL) {
		g_clear_error (&error);

		value = !global_view_setting ||
			g_key_file_get_boolean (key_file, STATE_GROUP_GLOBAL_FOLDER, key, &error);
		if (error != NULL) {
			g_clear_error (&error);
			value = TRUE;
		}
	}

	e_mail_reader_set_group_by_threads (reader, value);

	key = STATE_KEY_PREVIEW_VISIBLE;
	value = g_key_file_get_boolean (key_file, global_view_setting ? STATE_GROUP_GLOBAL_FOLDER : group_name, key, &error);
	if (error != NULL) {
		g_clear_error (&error);

		value = !global_view_setting ||
			g_key_file_get_boolean (key_file, STATE_GROUP_GLOBAL_FOLDER, key, &error);
		if (error != NULL) {
			g_clear_error (&error);
			value = TRUE;
		}
	}

	/* XXX This is a little confusing and needs rethought.  The
	 *     EShellWindow:safe-mode property blocks automatic message
	 *     selection, but the "safe-list" setting blocks both the
	 *     preview pane and automatic message selection. */
	if (g_settings_get_boolean (settings, "safe-list")) {
		g_settings_set_boolean (settings, "safe-list", FALSE);
		e_shell_window_set_safe_mode (shell_window, TRUE);
		value = FALSE;
	}

	e_mail_view_set_preview_visible (E_MAIL_VIEW (reader), value);

	g_free (group_name);

exit:
	message_list_thaw (MESSAGE_LIST (message_list));

	g_object_unref (settings);
}

static guint
mail_paned_view_reader_open_selected_mail (EMailReader *reader)
{
	EMailPanedView *paned_view;
	EMailPanedViewClass *class;

	paned_view = E_MAIL_PANED_VIEW (reader);

	class = E_MAIL_PANED_VIEW_GET_CLASS (paned_view);
	g_return_val_if_fail (class->open_selected_mail != NULL, 0);

	return class->open_selected_mail (paned_view);
}

static void
mail_paned_view_constructed (GObject *object)
{
	EMailPanedView *self = E_MAIL_PANED_VIEW (object);
	EShellBackend *shell_backend;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GSettings *settings;
	EMailReader *reader;
	EMailBackend *backend;
	EMailSession *session;
	EMailView *view;
	GtkWidget *message_list;
	GtkWidget *container;
	GtkWidget *widget;

	view = E_MAIL_VIEW (object);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	self->priv->display = g_object_new (E_TYPE_MAIL_DISPLAY,
		"headers-collapsable", TRUE,
		"remote-content", e_mail_backend_get_remote_content (backend),
		"mail-reader", E_MAIL_READER (self),
		NULL);

	/* FIXME This should be an EMailPanedView property, so
	 *       it can be configured from the settings module. */

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "headers-collapsed",
		self->priv->display, "headers-collapsed",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	e_paned_set_fixed_resize (E_PANED (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	self->priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		object, "orientation",
		widget, "orientation",
		G_BINDING_SYNC_CREATE);

	container = self->priv->paned;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	self->priv->scrolled_window = g_object_ref (widget);
	gtk_widget_show (widget);

	container = widget;

	widget = message_list_new (session);
	gtk_container_add (GTK_CONTAINER (container), widget);
	self->priv->message_list = g_object_ref (widget);
	gtk_widget_show (widget);

	self->priv->message_list_built_id = g_signal_connect_swapped (
		self->priv->message_list, "message-list-built",
		G_CALLBACK (mail_paned_view_message_list_built_cb),
		object);

	container = self->priv->paned;

	widget = GTK_WIDGET (e_mail_display_get_attachment_view (self->priv->display));
	gtk_widget_show (widget);
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);

	e_binding_bind_property (
		object, "preview-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	container = e_attachment_bar_get_content_area (E_ATTACHMENT_BAR (widget));
	widget = e_preview_pane_new (E_WEB_VIEW (self->priv->display));

	/* cannot ask the shell_view for the "mail-preview-toolbar" now, because its EUIManager is not loaded yet;
	   postpone it to the e_mail_paned_view_take_preview_toolbar() function */
	self->priv->preview_toolbar_box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
	gtk_box_pack_start (GTK_BOX (container), GTK_WIDGET (self->priv->preview_toolbar_box), FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);

	self->priv->preview_pane = g_object_ref (widget);
	gtk_widget_show (GTK_WIDGET (self->priv->display));
	gtk_widget_show (widget);

	/* Load the view instance. */

	e_mail_view_update_view_instance (E_MAIL_VIEW (object));

	/* Message list customizations. */

	e_mail_reader_init (E_MAIL_READER (object));

	reader = E_MAIL_READER (object);
	message_list = e_mail_reader_get_message_list (reader);

	g_signal_connect_swapped (
		message_list, "message-selected",
		G_CALLBACK (mail_paned_view_message_selected_cb),
		object);

	/* Restore pane positions from the last session once
	 * the shell view is fully initialized and visible. */
	g_signal_connect (
		shell_window, "shell-view-created::mail",
		G_CALLBACK (mail_paned_view_restore_state_cb),
		object);

	/* Do this after creating the message list.  Our
	 * set_preview_visible() method relies on it. */
	e_mail_view_set_preview_visible (view, TRUE);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_paned_view_parent_class)->constructed (object);
}

static void
mail_paned_view_set_search_strings (EMailView *view,
                                    GSList *search_strings)
{
	EMailDisplay *display;
	EWebView *web_view;
	EMailReader *reader;

	reader = E_MAIL_READER (view);
	display = e_mail_reader_get_mail_display (reader);
	if (!display)
		return;

	web_view = E_WEB_VIEW (display);

	e_web_view_clear_highlights (web_view);

	while (search_strings != NULL) {
		e_web_view_add_highlight (web_view, search_strings->data);
		search_strings = g_slist_next (search_strings);
	}
}

static GalViewInstance *
mail_paned_view_get_view_instance (EMailView *view)
{
	EMailPanedView *paned_view;

	paned_view = E_MAIL_PANED_VIEW (view);

	return paned_view->priv->view_instance;
}

static gchar *
empv_create_view_id (CamelFolder *folder)
{
	GChecksum *checksum;
	gchar *res, *folder_uri;

	g_return_val_if_fail (folder != NULL, NULL);

	folder_uri = e_mail_folder_uri_from_folder (folder);
	g_return_val_if_fail (folder_uri != NULL, NULL);

	/* to be able to migrate previously saved views */
	e_util_make_safe_filename (folder_uri);

	/* use MD5 checksum of the folder URI, to not depend on its length */
	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, (const guchar *) folder_uri, -1);

	res = g_strdup (g_checksum_get_string (checksum));

	g_checksum_free (checksum);
	g_free (folder_uri);

	return res;
}

static gboolean
empv_folder_or_parent_is_outgoing (MailFolderCache *folder_cache,
				   CamelStore *store,
				   const gchar *fullname)
{
	CamelFolderInfoFlags info_flags;
	gchar *path, *dash;
	gboolean res = FALSE;

	g_return_val_if_fail (MAIL_IS_FOLDER_CACHE (folder_cache), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (fullname != NULL, FALSE);

	if (!mail_folder_cache_get_folder_info_flags (folder_cache, store, fullname, &info_flags))
		info_flags = 0;

	if ((info_flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_OUTBOX ||
	    (info_flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_SENT)
		return TRUE;

	dash = strrchr (fullname, '/');
	if (!dash)
		return FALSE;

	path = g_strdup (fullname);

	while (path && *path) {
		dash = strrchr (path, '/');
		if (!dash)
			break;

		*dash = '\0';

		if (!mail_folder_cache_get_folder_info_flags (folder_cache, store, path, &info_flags))
			continue;

		if ((info_flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_OUTBOX ||
		    (info_flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_SENT) {
			res = TRUE;
			break;
		}
	}

	g_free (path);

	return res;
}

static void
mail_paned_view_update_view_instance (EMailView *view)
{
	EMailPanedView *self = E_MAIL_PANED_VIEW (view);
	EMailReader *reader;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellViewClass *shell_view_class;
	ESourceRegistry *registry;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	MailFolderCache *folder_cache;
	CamelFolder *folder;
	GtkOrientable *orientable;
	GtkOrientation orientation;
	GSettings *settings;
	gboolean outgoing_folder;
	gboolean show_vertical_view;
	gboolean global_view_setting;
	gchar *view_id;

	shell_view = e_mail_view_get_shell_view (view);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	registry = e_shell_get_registry (shell);

	reader = E_MAIL_READER (view);
	folder = e_mail_reader_ref_folder (reader);

	/* If no folder is selected, return silently. */
	if (folder == NULL)
		return;

	g_clear_object (&self->priv->view_instance);

	view_id = empv_create_view_id (folder);
	e_util_make_safe_filename (view_id);

	folder_cache = e_mail_session_get_folder_cache (e_mail_backend_get_session (e_mail_reader_get_backend (reader)));

	outgoing_folder =
		empv_folder_or_parent_is_outgoing (folder_cache, camel_folder_get_parent_store (folder), camel_folder_get_full_name (folder)) ||
		em_utils_folder_is_drafts (registry, folder) ||
		em_utils_folder_is_outbox (registry, folder) ||
		em_utils_folder_is_sent (registry, folder);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	global_view_setting = g_settings_get_boolean (
		settings, "global-view-setting");
	g_object_unref (settings);

	if (global_view_setting) {
		if (outgoing_folder) {
			view_instance = e_shell_view_new_view_instance (
				shell_view, "global_view_sent_setting");
		} else {
			view_instance = e_shell_view_new_view_instance (
				shell_view, "global_view_setting");
		}
	} else {
		view_instance = e_shell_view_new_view_instance (
			shell_view, view_id);
	}

	self->priv->view_instance = g_object_ref (view_instance);

	orientable = GTK_ORIENTABLE (view);
	orientation = gtk_orientable_get_orientation (orientable);
	show_vertical_view =
		!global_view_setting &&
		(orientation == GTK_ORIENTATION_HORIZONTAL);

	if (show_vertical_view) {
		const gchar *user_directory;
		gchar *filename;

		/* Force the view instance into vertical view. */

		g_free (view_instance->custom_filename);
		g_free (view_instance->current_view_filename);

		user_directory = gal_view_collection_get_user_directory (
			view_collection);

		filename = g_strdup_printf (
			"custom_wide_view-%s.xml", view_id);
		view_instance->custom_filename =
			g_build_filename (user_directory, filename, NULL);
		g_free (filename);

		filename = g_strdup_printf (
			"current_wide_view-%s.xml", view_id);
		view_instance->current_view_filename =
			g_build_filename (user_directory, filename, NULL);
		g_free (filename);
	}

	g_free (view_id);

	if (outgoing_folder) {
		if (show_vertical_view)
			gal_view_instance_set_default_view (
				view_instance, "Wide_View_Sent");
		else
			gal_view_instance_set_default_view (
				view_instance, "As_Sent_Folder");
	} else if (show_vertical_view) {
		gal_view_instance_set_default_view (
			view_instance, "Wide_View_Normal");
	}

	gal_view_instance_load (view_instance);

	if (!gal_view_instance_exists (view_instance)) {
		gchar *state_filename;

		state_filename = mail_config_folder_to_cachename (
			folder, "et-header-");

		if (g_file_test (state_filename, G_FILE_TEST_IS_REGULAR)) {
			GalView *gal_view;

			gal_view = gal_view_etable_new ("");

			/* XXX This only stashes the filename in the view.
			 *     The state file is not actually loaded until
			 *     the MessageList is attached to the view. */
			gal_view_load (gal_view, state_filename);

			gal_view_instance_set_custom_view (
				view_instance, gal_view);

			g_object_unref (gal_view);
		}

		g_free (state_filename);
	}

	g_signal_connect (
		view_instance, "display-view",
		G_CALLBACK (mail_paned_display_view_cb), view);

	mail_paned_display_view_cb (
		view_instance,
		gal_view_instance_get_current_view (view_instance),
		view);

	view_id = gal_view_instance_get_current_view_id (view_instance);
	e_shell_view_set_view_id (shell_view, view_id);
	g_free (view_id);

	g_object_unref (view_instance);

	g_clear_object (&folder);
}

static void
mail_paned_view_set_preview_visible (EMailView *view,
                                     gboolean preview_visible)
{
	EMailViewClass *parent_class;

	/* If we're showing the preview, tell EMailReader to reload the
	 * selected message.  This should force it to download the full
	 * message if necessary, so we don't get an empty preview. */
	if (preview_visible) {
		EMailReader *reader;
		GtkWidget *message_list;
		const gchar *cursor_uid;

		reader = E_MAIL_READER (view);
		message_list = e_mail_reader_get_message_list (reader);
		cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;

		if (cursor_uid != NULL)
			e_mail_reader_set_message (reader, cursor_uid);
	}

	mail_paned_view_save_boolean (
		E_MAIL_VIEW (view),
		STATE_KEY_PREVIEW_VISIBLE, preview_visible);

	/* Chain up to parent's set_preview_visible() method. */
	parent_class = E_MAIL_VIEW_CLASS (e_mail_paned_view_parent_class);
	parent_class->set_preview_visible (view, preview_visible);
}

static guint
mail_paned_view_open_selected_mail (EMailPanedView *view)
{
	return e_mail_reader_open_selected (E_MAIL_READER (view));
}

static void
e_mail_paned_view_class_init (EMailPanedViewClass *class)
{
	GObjectClass *object_class;
	EMailViewClass *mail_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_paned_view_dispose;
	object_class->constructed = mail_paned_view_constructed;
	object_class->set_property = mail_paned_view_set_property;
	object_class->get_property = mail_paned_view_get_property;

	mail_view_class = E_MAIL_VIEW_CLASS (class);
	mail_view_class->set_search_strings = mail_paned_view_set_search_strings;
	mail_view_class->get_view_instance = mail_paned_view_get_view_instance;
	mail_view_class->update_view_instance = mail_paned_view_update_view_instance;

	mail_view_class->set_preview_visible = mail_paned_view_set_preview_visible;

	class->open_selected_mail = mail_paned_view_open_selected_mail;

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_FORWARD_STYLE,
		"forward-style");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_GROUP_BY_THREADS,
		"group-by-threads");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_REPLY_STYLE,
		"reply-style");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_MARK_SEEN_ALWAYS,
		"mark-seen-always");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_DELETE_SELECTS_PREVIOUS,
		"delete-selects-previous");

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_TOOLBAR_VISIBLE,
		g_param_spec_boolean (
			"preview-toolbar-visible",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_paned_view_reader_init (EMailReaderInterface *iface)
{
	iface->get_alert_sink = mail_paned_view_get_alert_sink;
	iface->get_backend = mail_paned_view_get_backend;
	iface->get_mail_display = mail_paned_view_get_mail_display;
	iface->get_hide_deleted = mail_paned_view_get_hide_deleted;
	iface->get_message_list = mail_paned_view_get_message_list;
	iface->get_ui_manager = mail_paned_view_get_ui_manager;
	iface->get_preview_pane = mail_paned_view_get_preview_pane;
	iface->get_window = mail_paned_view_get_window;
	iface->set_folder = mail_paned_view_set_folder;
	iface->open_selected_mail = mail_paned_view_reader_open_selected_mail;
}

static void
e_mail_paned_view_init (EMailPanedView *view)
{
	view->priv = e_mail_paned_view_get_instance_private (view);

	e_signal_connect_notify (
		view, "notify::group-by-threads",
		G_CALLBACK (mail_paned_view_notify_group_by_threads_cb),
		NULL);
}

GtkWidget *
e_mail_paned_view_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_PANED_VIEW,
		"shell-view", shell_view, NULL);
}

void
e_mail_paned_view_hide_message_list_pane (EMailPanedView *view,
                                          gboolean visible)
{
	g_return_if_fail (E_IS_MAIL_PANED_VIEW (view));

	if (visible)
		gtk_widget_show (view->priv->scrolled_window);
	else
		gtk_widget_hide (view->priv->scrolled_window);
}

GtkWidget *
e_mail_paned_view_get_preview (EMailPanedView *view)
{
	g_return_val_if_fail (E_IS_MAIL_PANED_VIEW (view), NULL);

	return GTK_WIDGET (mail_paned_view_get_mail_display (E_MAIL_READER (view)));
}

gboolean
e_mail_paned_view_get_preview_toolbar_visible (EMailPanedView *view)
{
	g_return_val_if_fail (E_IS_MAIL_PANED_VIEW (view), FALSE);

	return view->priv->preview_toolbar_visible;
}

void
e_mail_paned_view_set_preview_toolbar_visible (EMailPanedView *view,
					       gboolean value)
{
	g_return_if_fail (E_IS_MAIL_PANED_VIEW (view));

	if (!view->priv->preview_toolbar_visible == !value)
		return;

	view->priv->preview_toolbar_visible = value;

	gtk_widget_set_visible (view->priv->preview_toolbar_box, value);

	g_object_notify (G_OBJECT (view), "preview-toolbar-visible");
}

void
e_mail_paned_view_take_preview_toolbar (EMailPanedView *self,
					GtkWidget *toolbar)
{
	g_return_if_fail (E_IS_MAIL_PANED_VIEW (self));
	g_return_if_fail (GTK_IS_WIDGET (toolbar));

	gtk_widget_set_visible (toolbar, TRUE);

	gtk_box_pack_start (GTK_BOX (self->priv->preview_toolbar_box), toolbar, FALSE, FALSE, 0);
}
