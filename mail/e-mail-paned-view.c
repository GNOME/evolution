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

#include "e-mail-paned-view.h"

#include <config.h>
#include <glib/gi18n.h>

#include <shell/e-shell-window-actions.h>

#include <libemail-engine/libemail-engine.h>

#include "em-utils.h"
#include "message-list.h"
#include "e-mail-reader-utils.h"

#define E_MAIL_PANED_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PANED_VIEW, EMailPanedViewPrivate))

#define E_SHELL_WINDOW_ACTION_GROUP_MAIL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail")

struct _EMailPanedViewPrivate {
	GtkWidget *paned;
	GtkWidget *scrolled_window;
	GtkWidget *message_list;
	GtkWidget *preview_pane;
	GtkWidget *search_bar;

	EMailDisplay *display;
	GalViewInstance *view_instance;

	/* ETable scrolling hack */
	gdouble default_scrollbar_position;

	guint paned_binding_id;

	/* Signal handler IDs */
	guint message_list_built_id;
};

enum {
	PROP_0,
	PROP_FORWARD_STYLE,
	PROP_GROUP_BY_THREADS,
	PROP_REPLY_STYLE,
	PROP_MARK_SEEN_ALWAYS
};

#define STATE_KEY_GROUP_BY_THREADS	"GroupByThreads"
#define STATE_KEY_SELECTED_MESSAGE	"SelectedMessage"
#define STATE_KEY_PREVIEW_VISIBLE	"PreviewVisible"
#define STATE_GROUP_GLOBAL_FOLDER	"GlobalFolder"

/* Forward Declarations */
static void e_mail_paned_view_reader_init (EMailReaderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EMailPanedView, e_mail_paned_view, E_TYPE_MAIL_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_READER, e_mail_paned_view_reader_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

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

static void
mail_paned_view_message_list_built_cb (EMailView *view,
                                       MessageList *message_list)
{
	EMailPanedViewPrivate *priv;
	EShellView *shell_view;
	EShellWindow *shell_window;
	CamelFolder *folder;
	GKeyFile *key_file;

	priv = E_MAIL_PANED_VIEW_GET_PRIVATE (view);

	folder = message_list_ref_folder (message_list);

	g_signal_handler_disconnect (
		message_list, priv->message_list_built_id);
	priv->message_list_built_id = 0;

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
		const gchar *key;
		gchar *folder_uri;
		gchar *group_name;
		gchar *uid;

		folder_uri = e_mail_folder_uri_from_folder (folder);

		key = STATE_KEY_SELECTED_MESSAGE;
		group_name = g_strdup_printf ("Folder %s", folder_uri);
		uid = g_key_file_get_string (key_file, group_name, key, NULL);
		g_free (group_name);

		g_free (folder_uri);

		if (!message_list_contains_uid (message_list, uid))
			e_mail_reader_unset_folder_just_selected (E_MAIL_READER (view));

		/* Use selection fallbacks if UID is not found. */
		message_list_select_uid (message_list, uid, TRUE);

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

	if (message_uid != NULL)
		g_key_file_set_string (key_file, group_name, key, message_uid);
	else
		g_key_file_remove_key (key_file, group_name, key, NULL);
	e_shell_view_set_state_dirty (shell_view);

	g_free (group_name);
	g_free (folder_uri);

	g_object_unref (folder);
}

static void
mail_paned_view_restore_state_cb (EShellWindow *shell_window,
                                  EShellView *shell_view,
                                  EMailPanedView *view)
{
	EMailPanedViewPrivate *priv;
	GSettings *settings;

	priv = E_MAIL_PANED_VIEW (view)->priv;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (e_shell_window_is_main_instance (shell_window)) {
		g_settings_bind (
			settings, "hpaned-size",
			priv->paned, "hposition",
			G_SETTINGS_BIND_DEFAULT);

		g_settings_bind (
			settings, "paned-size",
			priv->paned, "vposition",
			G_SETTINGS_BIND_DEFAULT);
	} else {
		g_settings_bind (
			settings, "hpaned-size-sub",
			priv->paned, "hposition",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);

		g_settings_bind (
			settings, "paned-size-sub",
			priv->paned, "vposition",
			G_SETTINGS_BIND_DEFAULT |
			G_SETTINGS_BIND_GET_NO_CHANGES);
	}

	g_object_unref (settings);
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_paned_view_dispose (GObject *object)
{
	EMailPanedViewPrivate *priv;

	priv = E_MAIL_PANED_VIEW_GET_PRIVATE (object);

	if (priv->paned != NULL) {
		g_object_unref (priv->paned);
		priv->paned = NULL;
	}

	if (priv->scrolled_window != NULL) {
		g_object_unref (priv->scrolled_window);
		priv->scrolled_window = NULL;
	}

	if (priv->message_list != NULL) {
		g_object_unref (priv->message_list);
		priv->message_list = NULL;
	}

	if (priv->preview_pane != NULL) {
		g_object_unref (priv->preview_pane);
		priv->preview_pane = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_paned_view_parent_class)->dispose (object);
}

static GtkActionGroup *
mail_paned_view_get_action_group (EMailReader *reader,
                                  EMailReaderActionGroup group)
{
	EMailView *view;
	EShellView *shell_view;
	EShellWindow *shell_window;
	const gchar *group_name;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	switch (group) {
		case E_MAIL_READER_ACTION_GROUP_STANDARD:
			group_name = "mail";
			break;
		case E_MAIL_READER_ACTION_GROUP_SEARCH_FOLDERS:
			group_name = "search-folders";
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	return e_shell_window_get_action_group (shell_window, group_name);
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

static GtkMenu *
mail_paned_view_get_popup_menu (EMailReader *reader)
{
	EMailView *view;
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkUIManager *ui_manager;
	GtkWidget *widget;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	widget = gtk_ui_manager_get_widget (ui_manager, "/mail-preview-popup");

	return GTK_MENU (widget);
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
	EMailPanedViewPrivate *priv;
	EMailView *view;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	GSettings *settings;
	EMailReaderInterface *default_interface;
	GtkWidget *message_list;
	GKeyFile *key_file;
	gchar *folder_uri;
	gchar *group_name;
	const gchar *key;
	gboolean value, global_view_setting;
	GError *error = NULL;

	priv = E_MAIL_PANED_VIEW_GET_PRIVATE (reader);

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);

	/* Can be NULL, if the shell window was closed meanwhile */
	if (!shell_view)
		return;

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

	/* This is a one-time-only callback. */
	if (MESSAGE_LIST (message_list)->cursor_uid == NULL &&
		priv->message_list_built_id == 0)
		priv->message_list_built_id = g_signal_connect_swapped (
			message_list, "message-list-built",
			G_CALLBACK (mail_paned_view_message_list_built_cb),
			reader);

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
	EMailPanedViewPrivate *priv;
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

	priv = E_MAIL_PANED_VIEW_GET_PRIVATE (object);

	view = E_MAIL_VIEW (object);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	priv->display = g_object_new (E_TYPE_MAIL_DISPLAY,
		"headers-collapsable", TRUE,
		"remote-content", e_mail_backend_get_remote_content (backend),
		NULL);

	/* FIXME This should be an EMailPanedView property, so
	 *       it can be configured from the settings module. */

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "headers-collapsed",
		priv->display, "headers-collapsed",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = e_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->paned = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		object, "orientation",
		widget, "orientation",
		G_BINDING_SYNC_CREATE);

	container = priv->paned;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	priv->scrolled_window = g_object_ref (widget);
	gtk_paned_pack1 (GTK_PANED (container), widget, TRUE, FALSE);
	gtk_widget_show (widget);

	container = widget;

	widget = message_list_new (session);
	gtk_container_add (GTK_CONTAINER (container), widget);
	priv->message_list = g_object_ref (widget);
	gtk_widget_show (widget);

	container = priv->paned;

	widget = e_preview_pane_new (E_WEB_VIEW (priv->display));
	gtk_paned_pack2 (GTK_PANED (container), widget, FALSE, FALSE);
	priv->preview_pane = g_object_ref (widget);
	gtk_widget_show (GTK_WIDGET (priv->display));
	gtk_widget_show (widget);

	e_binding_bind_property (
		object, "preview-visible",
		widget, "visible",
		G_BINDING_SYNC_CREATE);

	/* Load the view instance. */

	e_mail_view_update_view_instance (E_MAIL_VIEW (object));

	/* Message list customizations. */

	e_mail_reader_init (E_MAIL_READER (object), FALSE, TRUE);

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

	e_mail_reader_connect_remote_content (reader);

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
	e_filename_make_safe (folder_uri);

	/* use MD5 checksum of the folder URI, to not depend on its length */
	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, (const guchar *) folder_uri, -1);

	res = g_strdup (g_checksum_get_string (checksum));

	g_checksum_free (checksum);
	g_free (folder_uri);

	return res;
}

static void
mail_paned_view_update_view_instance (EMailView *view)
{
	EMailPanedViewPrivate *priv;
	EMailReader *reader;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellViewClass *shell_view_class;
	ESourceRegistry *registry;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	CamelFolder *folder;
	GtkOrientable *orientable;
	GtkOrientation orientation;
	GSettings *settings;
	gboolean outgoing_folder;
	gboolean show_vertical_view;
	gboolean global_view_setting;
	gchar *view_id;

	priv = E_MAIL_PANED_VIEW_GET_PRIVATE (view);

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

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	view_id = empv_create_view_id (folder);
	e_filename_make_safe (view_id);

	outgoing_folder =
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

	priv->view_instance = g_object_ref (view_instance);

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
			GalView *view;

			view = gal_view_etable_new ("");

			/* XXX This only stashes the filename in the view.
			 *     The state file is not actually loaded until
			 *     the MessageList is attached to the view. */
			gal_view_load (view, state_filename);

			gal_view_instance_set_custom_view (
				view_instance, view);

			g_object_unref (view);
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

	g_type_class_add_private (class, sizeof (EMailPanedViewPrivate));

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
}

static void
e_mail_paned_view_reader_init (EMailReaderInterface *iface)
{
	iface->get_action_group = mail_paned_view_get_action_group;
	iface->get_alert_sink = mail_paned_view_get_alert_sink;
	iface->get_backend = mail_paned_view_get_backend;
	iface->get_mail_display = mail_paned_view_get_mail_display;
	iface->get_hide_deleted = mail_paned_view_get_hide_deleted;
	iface->get_message_list = mail_paned_view_get_message_list;
	iface->get_popup_menu = mail_paned_view_get_popup_menu;
	iface->get_preview_pane = mail_paned_view_get_preview_pane;
	iface->get_window = mail_paned_view_get_window;
	iface->set_folder = mail_paned_view_set_folder;
	iface->open_selected_mail = mail_paned_view_reader_open_selected_mail;
}

static void
e_mail_paned_view_init (EMailPanedView *view)
{
	view->priv = E_MAIL_PANED_VIEW_GET_PRIVATE (view);

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

