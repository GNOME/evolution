/*
 * e-mail-shell-backend.c
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

#include "e-mail-shell-backend.h"

#include <glib/gi18n.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>

#include <composer/e-msg-composer.h>

#include <mail/e-mail-browser.h>
#include <mail/e-mail-config-assistant.h>
#include <mail/e-mail-config-window.h>
#include <mail/e-mail-folder-create-dialog.h>
#include <mail/e-mail-reader.h>
#include <mail/em-composer-utils.h>
#include <mail/em-utils.h>
#include <mail/mail-send-recv.h>
#include <mail/mail-vfolder-ui.h>
#include <mail/importers/mail-importer.h>
#include <mail/e-mail-ui-session.h>

#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>

#include "e-mail-shell-sidebar.h"
#include "e-mail-shell-view.h"
#include "em-account-prefs.h"
#include "em-composer-prefs.h"
#include "em-mailer-prefs.h"

#define E_MAIL_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackendPrivate))

#define E_MAIL_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_BACKEND, EMailShellBackendPrivate))

#define BACKEND_NAME "mail"

struct _EMailShellBackendPrivate {
	gint mail_sync_in_progress;
	guint mail_sync_source_id;
	gpointer assistant; /* weak pointer, when adding new mail account */
	gpointer editor;    /* weak pointer, when editing a mail account */
};

G_DEFINE_DYNAMIC_TYPE (
	EMailShellBackend,
	e_mail_shell_backend,
	E_TYPE_MAIL_BACKEND)

/* utility functions for mbox importer */
static void
mbox_create_preview_cb (GObject *preview,
                        GtkWidget **preview_widget)
{
	EMailDisplay *display;
	EMailBackend *mail_backend;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (preview_widget != NULL);

	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (e_shell_get_default (), BACKEND_NAME));
	g_return_if_fail (mail_backend != NULL);

	display = E_MAIL_DISPLAY (e_mail_display_new (e_mail_backend_get_remote_content (mail_backend)));
	g_object_set_data_full (
		preview, "mbox-imp-display",
		g_object_ref (display), g_object_unref);

	*preview_widget = GTK_WIDGET (display);
}

static void
message_parsed_cb (GObject *source_object,
                   GAsyncResult *res,
                   gpointer user_data)
{
	EMailParser *parser = E_MAIL_PARSER (source_object);
	EMailPartList *parts_list;
	GObject *preview = user_data;
	EMailDisplay *display;
	CamelFolder *folder;
	const gchar *message_uid;

	display = g_object_get_data (preview, "mbox-imp-display");

	parts_list = e_mail_parser_parse_finish (parser, res, NULL);
	if (!parts_list)
		return;

	folder = e_mail_part_list_get_folder (parts_list);
	message_uid = e_mail_part_list_get_message_uid (parts_list);
	if (message_uid) {
		CamelObjectBag *parts_registry;
		EMailPartList *reserved_parts_list;
		gchar *mail_uri;

		mail_uri = e_mail_part_build_uri (folder, message_uid, NULL, NULL);
		parts_registry = e_mail_part_list_get_registry ();

		reserved_parts_list = camel_object_bag_reserve (parts_registry, mail_uri);
		g_clear_object (&reserved_parts_list);

		camel_object_bag_add (parts_registry, mail_uri, parts_list);

		g_free (mail_uri);
	}

	e_mail_display_set_part_list (display, parts_list);
	e_mail_display_load (display, NULL);

	g_object_unref (parts_list);
}

static void
mbox_fill_preview_cb (GObject *preview,
                      CamelMimeMessage *msg)
{
	EShell *shell;
	EMailDisplay *display;
	EMailParser *parser;
	EMailSession *mail_session;
	ESourceRegistry *registry;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (msg != NULL);

	display = g_object_get_data (preview, "mbox-imp-display");
	g_return_if_fail (display != NULL);

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	mail_session = e_mail_session_new (registry);

	parser = e_mail_parser_new (CAMEL_SESSION (mail_session));
	e_mail_parser_parse (
		parser, NULL, msg->message_id, msg,
		message_parsed_cb, NULL, preview);

	g_object_unref (mail_session);
}

static void
mail_shell_backend_init_importers (void)
{
	EImportClass *import_class;
	EImportImporter *importer;

	import_class = g_type_class_ref (e_import_get_type ());

	importer = mbox_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);
	mbox_importer_set_preview_funcs (
		mbox_create_preview_cb, mbox_fill_preview_cb);

	importer = elm_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = pine_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);
}

static void
mail_shell_backend_mail_icon_cb (EShellWindow *shell_window,
                                 const gchar *icon_name)
{
	GtkAction *action;

	action = e_shell_window_get_shell_view_action (
		shell_window, BACKEND_NAME);
	gtk_action_set_icon_name (action, icon_name);
}

static void
mail_shell_backend_folder_created_cb (EMailFolderCreateDialog *dialog,
                                      CamelStore *store,
                                      const gchar *folder_name,
                                      GWeakRef *folder_tree_weak_ref)
{
	EMFolderTree *folder_tree;

	folder_tree = g_weak_ref_get (folder_tree_weak_ref);

	if (folder_tree != NULL) {
		gchar *folder_uri;

		/* Select the newly created folder. */
		folder_uri = e_mail_folder_uri_build (store, folder_name);
		em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);
		g_free (folder_uri);

		g_object_unref (folder_tree);
	}
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	EMFolderTree *folder_tree = NULL;
	EMailShellSidebar *mail_shell_sidebar;
	EMailSession *session;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	GtkWidget *dialog;
	const gchar *view_name;

	/* Take care not to unnecessarily load the mail shell view. */
	view_name = e_shell_window_get_active_view (shell_window);
	if (g_strcmp0 (view_name, BACKEND_NAME) != 0) {
		EShell *shell;
		EShellBackend *shell_backend;
		EMailBackend *backend;

		shell = e_shell_window_get_shell (shell_window);

		shell_backend =
			e_shell_get_backend_by_name (shell, BACKEND_NAME);
		g_return_if_fail (E_IS_MAIL_BACKEND (shell_backend));

		backend = E_MAIL_BACKEND (shell_backend);
		session = e_mail_backend_get_session (backend);

		goto exit;
	}

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	session = em_folder_tree_get_session (folder_tree);

exit:
	dialog = e_mail_folder_create_dialog_new (
		GTK_WINDOW (shell_window),
		E_MAIL_UI_SESSION (session));

	if (folder_tree != NULL) {
		g_signal_connect_data (
			dialog, "folder-created",
			G_CALLBACK (mail_shell_backend_folder_created_cb),
			e_weak_ref_new (folder_tree),
			(GClosureNotify) e_weak_ref_free, 0);
	}

	gtk_widget_show (GTK_WIDGET (dialog));
}

static void
action_mail_account_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	EShell *shell;
	EShellBackend *shell_backend;

	g_return_if_fail (shell_window != NULL);

	shell = e_shell_window_get_shell (shell_window);
	shell_backend = e_shell_get_backend_by_name (shell, BACKEND_NAME);
	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (shell_backend));

	e_mail_shell_backend_new_account (
		E_MAIL_SHELL_BACKEND (shell_backend),
		GTK_WINDOW (shell_window));
}

static void
action_mail_message_new_composer_created_cb (GObject *source_object,
					     GAsyncResult *result,
					     gpointer user_data)
{
	CamelFolder *folder = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	if (folder)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		em_utils_compose_new_message (composer, folder);
	}

	g_clear_object (&folder);
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	EMailShellSidebar *mail_shell_sidebar;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	EShell *shell;
	ESourceRegistry *registry;
	EMFolderTree *folder_tree;
	CamelFolder *folder = NULL;
	CamelStore *store;
	GList *list;
	const gchar *extension_name;
	const gchar *view_name;
	gboolean no_transport_defined;
	gchar *folder_name;

	shell = e_shell_window_get_shell (shell_window);
	registry = e_shell_get_registry (shell);

	extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;
	list = e_source_registry_list_sources (registry, extension_name);
	no_transport_defined = (list == NULL);
	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (no_transport_defined)
		return;

	/* Take care not to unnecessarily load the mail shell view. */
	view_name = e_shell_window_get_active_view (shell_window);
	if (g_strcmp0 (view_name, BACKEND_NAME) != 0)
		goto exit;

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	if (em_folder_tree_get_selected (folder_tree, &store, &folder_name)) {

		/* FIXME This blocks and is not cancellable. */
		folder = camel_store_get_folder_sync (
			store, folder_name, 0, NULL, NULL);

		g_object_unref (store);
		g_free (folder_name);
	}

 exit:
	e_msg_composer_new (shell, action_mail_message_new_composer_created_cb,
		folder ? g_object_ref (folder) : NULL);
}

static GtkActionEntry item_entries[] = {

	{ "mail-message-new",
	  "mail-message-new",
	  NC_("New", "_Mail Message"),
	  "<Shift><Control>m",
	  N_("Compose a new mail message"),
	  G_CALLBACK (action_mail_message_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "mail-account-new",
	  "evolution-mail",
	  NC_("New", "Mail Acco_unt"),
	  NULL,
	  N_("Create a new mail account"),
	  G_CALLBACK (action_mail_account_new_cb) },

	{ "mail-folder-new",
	  "folder-new",
	  NC_("New", "Mail _Folder"),
	  NULL,
	  N_("Create a new mail folder"),
	  G_CALLBACK (action_mail_folder_new_cb) }
};

static void
mail_shell_backend_sync_store_done_cb (CamelStore *store,
                                       gpointer user_data)
{
	EMailShellBackend *mail_shell_backend = user_data;

	mail_shell_backend->priv->mail_sync_in_progress--;
}

static gboolean
mail_shell_backend_mail_sync (gpointer user_data)
{
	EMailShellBackend *mail_shell_backend;
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	GList *list, *link;

	mail_shell_backend = E_MAIL_SHELL_BACKEND (user_data);

	shell_backend = E_SHELL_BACKEND (mail_shell_backend);
	shell = e_shell_backend_get_shell (shell_backend);

	/* Obviously we can only sync in online mode. */
	if (!e_shell_get_online (shell))
		goto exit;

	/* If a sync is still in progress, skip this round. */
	if (mail_shell_backend->priv->mail_sync_in_progress)
		goto exit;

	backend = E_MAIL_BACKEND (mail_shell_backend);
	session = e_mail_backend_get_session (backend);

	list = camel_session_list_services (CAMEL_SESSION (session));

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelService *service;

		service = CAMEL_SERVICE (link->data);

		if (!CAMEL_IS_STORE (service))
			continue;

		mail_shell_backend->priv->mail_sync_in_progress++;

		mail_sync_store (
			CAMEL_STORE (service), FALSE,
			mail_shell_backend_sync_store_done_cb,
			mail_shell_backend);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

exit:
	return TRUE;
}

static gboolean
mail_shell_backend_handle_uri_cb (EShell *shell,
                                  const gchar *uri,
                                  EMailShellBackend *mail_shell_backend)
{
	gboolean handled = FALSE;

	if (g_str_has_prefix (uri, "mailto:")) {
		em_utils_compose_new_message_with_mailto (shell, uri, NULL);
		handled = TRUE;
	}

	return handled;
}

static void
mail_shell_backend_prepare_for_quit_cb (EShell *shell,
                                        EActivity *activity,
                                        EShellBackend *shell_backend)
{
	EMailShellBackendPrivate *priv;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	/* Prevent a sync from starting while trying to shutdown. */
	if (priv->mail_sync_source_id > 0) {
		g_source_remove (priv->mail_sync_source_id);
		priv->mail_sync_source_id = 0;
	}
}

static void
mail_shell_backend_window_weak_notify_cb (EShell *shell,
                                          GObject *where_the_object_was)
{
	g_signal_handlers_disconnect_by_func (
		shell, mail_shell_backend_mail_icon_cb,
		where_the_object_was);
}

static void
mail_shell_backend_window_added_cb (GtkApplication *application,
                                    GtkWindow *window,
                                    EShellBackend *shell_backend)
{
	EShell *shell = E_SHELL (application);
	EMailBackend *backend;
	EMailSession *session;
	EHTMLEditor *editor = NULL;
	const gchar *backend_name;

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	if (E_IS_MSG_COMPOSER (window))
		editor = e_msg_composer_get_editor (E_MSG_COMPOSER (window));

	if (E_IS_MAIL_SIGNATURE_EDITOR (window))
		editor = e_mail_signature_editor_get_editor (
			E_MAIL_SIGNATURE_EDITOR (window));

	/* This applies to both the composer and signature editor. */
	if (editor != NULL) {
		EContentEditor *cnt_editor;
		GSettings *settings;
		gboolean active = TRUE;

		cnt_editor = e_html_editor_get_content_editor (editor);

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		active = g_settings_get_boolean (
			settings, "composer-send-html");

		g_object_unref (settings);

		e_content_editor_set_html_mode (cnt_editor, active);
	}

	if (E_IS_MSG_COMPOSER (window)) {
		/* Start the mail backend if it isn't already.  This
		 * may be necessary when opening a new composer window
		 * from a shell view other than mail. */
		e_shell_backend_start (shell_backend);

		/* Integrate the new composer into the mail module. */
		em_configure_new_composer (
			E_MSG_COMPOSER (window), session);
		return;
	}

	if (!E_IS_SHELL_WINDOW (window))
		return;

	backend_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), backend_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), backend_name,
		source_entries, G_N_ELEMENTS (source_entries));

	g_signal_connect_swapped (
		shell, "event::mail-icon",
		G_CALLBACK (mail_shell_backend_mail_icon_cb), window);

	g_object_weak_ref (
		G_OBJECT (window), (GWeakNotify)
		mail_shell_backend_window_weak_notify_cb, shell);
}

static void
mail_shell_backend_disconnect_done_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	CamelService *service;
	EActivity *activity;
	EAlertSink *alert_sink;
	GError *error = NULL;

	service = CAMEL_SERVICE (source_object);
	activity = E_ACTIVITY (user_data);

	alert_sink = e_activity_get_alert_sink (activity);

	camel_service_disconnect_finish (service, result, &error);

	if (e_activity_handle_cancellation (activity, error)) {
		g_error_free (error);

	} else if (error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail:disconnect",
			camel_service_get_display_name (service),
			error->message, NULL);
		g_error_free (error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	g_object_unref (activity);
}

static void
mail_shell_backend_changes_committed_cb (EMailConfigWindow *window,
                                         EMailShellBackend *mail_shell_backend)
{
	EMailSession *session;
	EShell *shell;
	EShellBackend *shell_backend;
	ESource *original_source;
	CamelService *service;
	EActivity *activity;
	GCancellable *cancellable;
	GList *list, *link;
	const gchar *uid;

	session = e_mail_config_window_get_session (window);
	original_source = e_mail_config_window_get_original_source (window);

	uid = e_source_get_uid (original_source);
	service = camel_session_ref_service (CAMEL_SESSION (session), uid);
	g_return_if_fail (service != NULL);

	shell_backend = E_SHELL_BACKEND (mail_shell_backend);

	shell = e_shell_backend_get_shell (shell_backend);
	list = gtk_application_get_windows (GTK_APPLICATION (shell));

	activity = e_activity_new ();

	/* Find an EShellWindow to serve as an EAlertSink. */
	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkWindow *window = GTK_WINDOW (link->data);

		if (E_IS_SHELL_WINDOW (window)) {
			EAlertSink *alert_sink = E_ALERT_SINK (window);
			e_activity_set_alert_sink (activity, alert_sink);
		}
	}

	cancellable = camel_operation_new ();
	e_activity_set_cancellable (activity, cancellable);

	e_shell_backend_add_activity (shell_backend, activity);

	camel_service_disconnect (
		service, TRUE, G_PRIORITY_DEFAULT, cancellable,
		mail_shell_backend_disconnect_done_cb, activity);

	g_object_unref (cancellable);

	g_object_unref (service);
}

static gboolean
network_monitor_gio_name_to_active_id (GBinding *binding,
				       const GValue *from_value,
				       GValue *to_value,
				       gpointer user_data)
{
	const gchar *gio_name_value;

	gio_name_value = g_value_get_string (from_value);

	if (g_strcmp0 (gio_name_value, E_NETWORK_MONITOR_ALWAYS_ONLINE_NAME) == 0) {
		g_value_set_string (to_value, gio_name_value);
	} else {
		ENetworkMonitor *network_monitor;
		GSList *gio_names, *link;

		network_monitor = E_NETWORK_MONITOR (e_network_monitor_get_default ());
		gio_names = e_network_monitor_list_gio_names (network_monitor);
		for (link = gio_names; link; link = g_slist_next (link)) {
			const gchar *gio_name = link->data;

			g_warn_if_fail (gio_name != NULL);

			if (g_strcmp0 (gio_name_value, gio_name) == 0)
				break;
		}
		g_slist_free_full (gio_names, g_free);

		/* Stopped before checked all the gio_names, thus found a match */
		if (link)
			g_value_set_string (to_value, gio_name_value);
		else
			g_value_set_string (to_value, "default");
	}

	return TRUE;
}

static GtkWidget *
mail_shell_backend_create_network_page (EPreferencesWindow *window)
{
	EShell *shell;
	ESourceRegistry *registry;
	GtkBox *vbox, *hbox;
	GtkWidget *widget, *label;
	PangoAttrList *bold;
	ENetworkMonitor *network_monitor;
	GSList *gio_names, *link;

	const gchar *known_gio_names[] = {
		/* Translators: One of the known implementation names of the GNetworkMonitor. Either translate
		    it to some user-frienly form, or keep it as is. */
		NC_("NetworkMonitor", "base"),
		/* Translators: One of the known implementation names of the GNetworkMonitor. Either translate
		    it to some user-frienly form, or keep it as is. */
		NC_("NetworkMonitor", "netlink"),
		/* Translators: One of the known implementation names of the GNetworkMonitor. Either translate
		    it to some user-frienly form, or keep it as is. */
		NC_("NetworkMonitor", "networkmanager")
	};

	/* To quiet a gcc warning about unused variable */
	known_gio_names[0] = known_gio_names[1];

	shell = e_preferences_window_get_shell (window);
	registry = e_shell_get_registry (shell);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 4));

	widget = gtk_label_new (_("General"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"attributes", bold,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

	hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4));
	#if GTK_CHECK_VERSION(3,12,0)
	gtk_widget_set_margin_start (GTK_WIDGET (hbox), 12);
	#else
	gtk_widget_set_margin_left (GTK_WIDGET (hbox), 12);
	#endif

	label = gtk_label_new_with_mnemonic (C_("NetworkMonitor", "Method to detect _online state:"));
	gtk_box_pack_start (hbox, label, FALSE, FALSE, 0);

	widget = gtk_combo_box_text_new ();
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	/* Always as the first */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "default", C_("NetworkMonitor", "Default"));

	network_monitor = E_NETWORK_MONITOR (e_network_monitor_get_default ());
	gio_names = e_network_monitor_list_gio_names (network_monitor);
	for (link = gio_names; link; link = g_slist_next (link)) {
		const gchar *gio_name = link->data;

		g_warn_if_fail (gio_name != NULL);

		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), gio_name, g_dpgettext2 (NULL, "NetworkMonitor", gio_name));
	}
	g_slist_free_full (gio_names, g_free);

	/* Always as the last */
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), E_NETWORK_MONITOR_ALWAYS_ONLINE_NAME, C_("NetworkMonitor", "Always Online"));

	e_binding_bind_property_full (
		network_monitor, "gio-name",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
		network_monitor_gio_name_to_active_id,
		NULL,
		NULL, NULL);

	gtk_widget_show_all (GTK_WIDGET (hbox));
	gtk_box_pack_start (vbox, GTK_WIDGET (hbox), FALSE, FALSE, 0);

	widget = e_proxy_preferences_new (registry);
	gtk_widget_show (widget);
	gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

	return GTK_WIDGET (vbox);
}

static void
mail_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *mail_session;
	CamelService *vstore;
	GtkWidget *preferences_window;
	GSettings *settings;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_shell_backend_parent_class)->constructed (object);

	mail_shell_backend_init_importers ();

	g_signal_connect (
		shell, "handle-uri",
		G_CALLBACK (mail_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (mail_shell_backend_prepare_for_quit_cb),
		shell_backend);

	g_signal_connect (
		shell, "window-added",
		G_CALLBACK (mail_shell_backend_window_added_cb),
		shell_backend);

	/* Setup preference widget factories */
	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"mail-accounts",
		"preferences-mail-accounts",
		_("Mail Accounts"),
		"mail-account-management",
		em_account_prefs_new,
		100);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"mail",
		"preferences-mail",
		_("Mail Preferences"),
		"index#mail-basic",
		em_mailer_prefs_new,
		300);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"composer",
		"preferences-composer",
		_("Composer Preferences"),
		"index#mail-composing",
		em_composer_prefs_new,
		400);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"system-network-proxy",
		"preferences-system-network-proxy",
		_("Network Preferences"),
		NULL,
		mail_shell_backend_create_network_page,
		500);

	mail_session = e_mail_backend_get_session (E_MAIL_BACKEND (object));
	vstore = camel_session_ref_service (
		CAMEL_SESSION (mail_session), E_MAIL_SESSION_VFOLDER_UID);
	g_return_if_fail (vstore != NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "enable-unmatched",
		vstore, "unmatched-enabled",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	g_object_unref (vstore);
}

static void
mail_shell_backend_start (EShellBackend *shell_backend)
{
	EMailShellBackendPrivate *priv;
	EMailBackend *backend;
	EMailSession *session;
	EMailAccountStore *account_store;
	GError *error = NULL;

	priv = E_MAIL_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (
		E_MAIL_UI_SESSION (session));

	/* XXX Should we be calling this unconditionally? */
	vfolder_load_storage (session);

	if (!e_mail_account_store_load_sort_order (account_store, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	if (g_getenv ("CAMEL_FLUSH_CHANGES") != NULL) {
		priv->mail_sync_source_id = e_named_timeout_add_seconds (
			mail_config_get_sync_timeout (),
			mail_shell_backend_mail_sync,
			shell_backend);
	}
}

static gboolean
mail_shell_backend_delete_junk_policy_decision (EMailBackend *backend)
{
	GSettings *settings;
	gboolean delete_junk;
	gint empty_date = 0;
	gint empty_days = 0;
	gint now;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	now = time (NULL) / 60 / 60 / 24;

	delete_junk = g_settings_get_boolean (settings, "junk-empty-on-exit");

	if (delete_junk) {
		empty_days = g_settings_get_int (
			settings, "junk-empty-on-exit-days");
		empty_date = g_settings_get_int (
			settings, "junk-empty-date");
	}

	delete_junk = delete_junk && (
		(empty_days == 0) ||
		(empty_days > 0 && empty_date + empty_days <= now));

	if (delete_junk)
		g_settings_set_int (settings, "junk-empty-date", now);

	g_object_unref (settings);

	return delete_junk;
}

static gboolean
mail_shell_backend_empty_trash_policy_decision (EMailBackend *backend)
{
	GSettings *settings;
	gboolean empty_trash;
	gint empty_date = 0;
	gint empty_days = 0;
	gint now;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	now = time (NULL) / 60 / 60 / 24;

	empty_trash = g_settings_get_boolean (settings, "trash-empty-on-exit");

	if (empty_trash) {
		empty_days = g_settings_get_int (
			settings, "trash-empty-on-exit-days");
		empty_date = g_settings_get_int (
			settings, "trash-empty-date");
	}

	empty_trash = empty_trash && (
		(empty_days == 0) ||
		(empty_days > 0 && empty_date + empty_days <= now));

	if (empty_trash)
		g_settings_set_int (settings, "trash-empty-date", now);

	g_object_unref (settings);

	return empty_trash;
}

static void
mail_shell_backend_dispose (GObject *object)
{
	EMailShellBackendPrivate *priv;

	priv = E_MAIL_SHELL_BACKEND (object)->priv;

	if (priv->assistant != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->assistant), &priv->assistant);
		priv->assistant = NULL;
	}

	if (priv->editor != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->editor), &priv->editor);
		priv->editor = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_shell_backend_parent_class)->dispose (object);
}

static void
e_mail_shell_backend_class_init (EMailShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;
	EMailBackendClass *mail_backend_class;

	g_type_class_add_private (class, sizeof (EMailShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_shell_backend_constructed;
	object_class->dispose = mail_shell_backend_dispose;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_MAIL_SHELL_VIEW;
	shell_backend_class->name = BACKEND_NAME;
	shell_backend_class->aliases = "";
	shell_backend_class->schemes = "mailto:email";
	shell_backend_class->sort_order = 200;
	shell_backend_class->preferences_page = "mail-accounts";
	shell_backend_class->start = mail_shell_backend_start;

	mail_backend_class = E_MAIL_BACKEND_CLASS (class);
	mail_backend_class->delete_junk_policy_decision =
		mail_shell_backend_delete_junk_policy_decision;
	mail_backend_class->empty_trash_policy_decision =
		mail_shell_backend_empty_trash_policy_decision;
}

static void
e_mail_shell_backend_class_finalize (EMailShellBackendClass *class)
{
}

static void
e_mail_shell_backend_init (EMailShellBackend *mail_shell_backend)
{
	mail_shell_backend->priv =
		E_MAIL_SHELL_BACKEND_GET_PRIVATE (mail_shell_backend);
}

void
e_mail_shell_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_shell_backend_register_type (type_module);
}

void
e_mail_shell_backend_new_account (EMailShellBackend *mail_shell_backend,
                                  GtkWindow *parent)
{
	GtkWidget *assistant;
	EMailBackend *backend;
	EMailSession *session;

	g_return_if_fail (mail_shell_backend != NULL);
	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));

	assistant = mail_shell_backend->priv->assistant;

	if (assistant != NULL) {
		gtk_window_present (GTK_WINDOW (assistant));
		return;
	}

	backend = E_MAIL_BACKEND (mail_shell_backend);
	session = e_mail_backend_get_session (backend);

	if (assistant == NULL)
		assistant = e_mail_config_assistant_new (session);

	gtk_window_set_transient_for (GTK_WINDOW (assistant), parent);
	gtk_widget_show (assistant);

	mail_shell_backend->priv->assistant = assistant;

	g_object_add_weak_pointer (
		G_OBJECT (mail_shell_backend->priv->assistant),
		&mail_shell_backend->priv->assistant);
}

void
e_mail_shell_backend_edit_account (EMailShellBackend *mail_shell_backend,
                                   GtkWindow *parent,
                                   ESource *mail_account)
{
	EMailShellBackendPrivate *priv;
	EMailBackend *backend;
	EMailSession *session;

	g_return_if_fail (E_IS_MAIL_SHELL_BACKEND (mail_shell_backend));
	g_return_if_fail (E_IS_SOURCE (mail_account));

	priv = mail_shell_backend->priv;

	backend = E_MAIL_BACKEND (mail_shell_backend);
	session = e_mail_backend_get_session (backend);

	if (priv->editor != NULL) {
		gtk_window_present (GTK_WINDOW (priv->editor));
		return;
	}

	priv->editor = e_mail_config_window_new (session, mail_account);
	gtk_window_set_transient_for (GTK_WINDOW (priv->editor), parent);
	g_object_add_weak_pointer (G_OBJECT (priv->editor), &priv->editor);

	g_signal_connect (
		priv->editor, "changes-committed",
		G_CALLBACK (mail_shell_backend_changes_committed_cb),
		mail_shell_backend);

	gtk_widget_show (priv->editor);
}

/******************* Code below here belongs elsewhere. *******************/

static GSList *
mail_labels_get_filter_options (gboolean include_none)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	EMailLabelListStore *label_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list = NULL;
	gboolean valid;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	if (include_none) {
		struct _filter_option *option;

		option = g_new0 (struct _filter_option, 1);
		/* Translators: The first item in the list, to be
		 * able to set rule: [Label] [is/is-not] [None] */
		option->title = g_strdup (C_("label", "None"));
		option->value = g_strdup ("");
		list = g_slist_prepend (list, option);
	}

	model = GTK_TREE_MODEL (label_store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		struct _filter_option *option;
		gchar *name, *tag;

		name = e_mail_label_list_store_get_name (label_store, &iter);
		tag = e_mail_label_list_store_get_tag (label_store, &iter);

		if (g_str_has_prefix (tag, "$Label")) {
			gchar *tmp = tag;

			tag = g_strdup (tag + 6);

			g_free (tmp);
		}

		option = g_new0 (struct _filter_option, 1);
		option->title = e_str_without_underscores (name);
		option->value = tag;  /* takes ownership */
		list = g_slist_prepend (list, option);

		g_free (name);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return g_slist_reverse (list);
}

GSList *
e_mail_labels_get_filter_options (void)
{
	return mail_labels_get_filter_options (TRUE);
}

GSList *
e_mail_labels_get_filter_options_without_none (void)
{
	return mail_labels_get_filter_options (FALSE);
}

static const gchar *
get_filter_option_value (EFilterPart *part,
                         const gchar *name)
{
	EFilterElement *elem;
	EFilterOption *opt;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	elem = e_filter_part_find_element (part, name);
	g_return_val_if_fail (elem != NULL, NULL);
	g_return_val_if_fail (E_IS_FILTER_OPTION (elem), NULL);

	opt = E_FILTER_OPTION (elem);
	return e_filter_option_get_current (opt);
}

static void
append_one_label_expr (GString *out,
                       const gchar *versus)
{
	GString *encoded;

	g_return_if_fail (out != NULL);
	g_return_if_fail (versus != NULL);

	encoded = g_string_new ("");
	camel_sexp_encode_string (encoded, versus);

	g_string_append_printf (
		out,
		" (= (user-tag \"label\") %s)"
		" (user-flag (+ \"$Label\" %s))"
		" (user-flag %s)",
		encoded->str, encoded->str, encoded->str);

	g_string_free (encoded, TRUE);
}

void
e_mail_labels_get_filter_code (EFilterElement *element,
                               GString *out,
                               EFilterPart *part)
{
	const gchar *label_type, *versus;
	gboolean is_not;

	label_type = get_filter_option_value (part, "label-type");
	versus = get_filter_option_value (part, "versus");

	g_return_if_fail (label_type != NULL);
	g_return_if_fail (versus != NULL);

	is_not = g_str_equal (label_type, "is-not");

	if (!g_str_equal (label_type, "is") && !is_not) {
		g_warning (
			"%s: Unknown label-type: '%s'",
			G_STRFUNC, label_type);
		return;
	}

	/* the 'None' item has 'is-not' inverted */
	if (!*versus)
		is_not = !is_not;

	g_string_append (out, " (match-all (");
	if (is_not)
		g_string_append (out, " not (");
	g_string_append (out, "or");

	/* the 'None' item; "is None" means "has not set any label" */
	if (!*versus) {
		EShell *shell;
		EShellBackend *shell_backend;
		EMailBackend *backend;
		EMailSession *session;
		EMailLabelListStore *label_store;
		GtkTreeModel *model;
		GtkTreeIter iter;
		gboolean valid;

		shell = e_shell_get_default ();
		shell_backend = e_shell_get_backend_by_name (shell, "mail");

		backend = E_MAIL_BACKEND (shell_backend);
		session = e_mail_backend_get_session (backend);
		label_store = e_mail_ui_session_get_label_store (
			E_MAIL_UI_SESSION (session));

		model = GTK_TREE_MODEL (label_store);
		valid = gtk_tree_model_get_iter_first (model, &iter);

		while (valid) {
			gchar *tag;

			tag = e_mail_label_list_store_get_tag (
				label_store, &iter);

			if (g_str_has_prefix (tag, "$Label")) {
				gchar *tmp = tag;

				tag = g_strdup (tag + 6);

				g_free (tmp);
			}

			append_one_label_expr (out, tag);

			g_free (tag);

			valid = gtk_tree_model_iter_next (model, &iter);
		}
	} else {
		append_one_label_expr (out, versus);
	}

	if (is_not)
		g_string_append (out, ")");
	g_string_append (out, " ))");
}

