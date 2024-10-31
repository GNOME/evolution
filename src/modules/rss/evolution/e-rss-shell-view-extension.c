/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "mail/e-mail-reader-utils.h"
#include "mail/e-mail-view.h"
#include "mail/em-folder-tree.h"
#include "shell/e-shell-content.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-window.h"

#include "../camel-rss-store-summary.h"

#include "module-rss.h"

#define E_TYPE_RSS_SHELL_VIEW_EXTENSION (e_rss_shell_view_extension_get_type ())

GType e_rss_shell_view_extension_get_type (void);

typedef struct _ERssShellViewExtension {
	EExtension parent;
} ERssShellViewExtension;

typedef struct _ERssShellViewExtensionClass {
	EExtensionClass parent_class;
} ERssShellViewExtensionClass;

G_DEFINE_DYNAMIC_TYPE (ERssShellViewExtension, e_rss_shell_view_extension, E_TYPE_EXTENSION)

static gboolean
e_rss_check_rss_folder_selected (EShellView *shell_view,
				 CamelStore **pstore,
				 gchar **pfolder_path)
{
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	gchar *selected_path = NULL;
	CamelStore *selected_store = NULL;
	gboolean is_rss_folder = FALSE;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	if (em_folder_tree_get_selected (folder_tree, &selected_store, &selected_path)) {
		if (selected_store) {
			is_rss_folder = g_strcmp0 (camel_service_get_uid (CAMEL_SERVICE (selected_store)), "rss") == 0 &&
					g_strcmp0 (selected_path, CAMEL_VJUNK_NAME) != 0 &&
					g_strcmp0 (selected_path, CAMEL_VTRASH_NAME) != 0;

			if (is_rss_folder) {
				if (pstore)
					*pstore = g_object_ref (selected_store);

				if (pfolder_path)
					*pfolder_path = selected_path;
				else
					g_free (selected_path);

				selected_path = NULL;
			}

			g_object_unref (selected_store);
		}

		g_free (selected_path);
	}

	g_object_unref (folder_tree);

	return is_rss_folder;
}

static void
e_rss_mail_folder_reload_got_folder_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	EShellView *shell_view = user_data;
	CamelFolder *folder;
	GError *error = NULL;

	folder = camel_store_get_folder_finish (CAMEL_STORE (source_object), result, &error);

	if (folder) {
		EShellContent *shell_content;
		EMailView *mail_view = NULL;

		shell_content = e_shell_view_get_shell_content (shell_view);
		g_object_get (shell_content, "mail-view", &mail_view, NULL);

		if (mail_view)
			e_mail_reader_refresh_folder (E_MAIL_READER (mail_view), folder);

		g_clear_object (&mail_view);
		g_object_unref (folder);
	} else {
		g_warning ("%s: Failed to get folder: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}
}

static void
action_rss_mail_folder_reload_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EShellView *shell_view = user_data;
	CamelStore *store = NULL;
	CamelRssStoreSummary *store_summary = NULL;
	gchar *folder_path = NULL;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (!e_rss_check_rss_folder_selected (shell_view, &store, &folder_path))
		return;

	g_object_get (store, "summary", &store_summary, NULL);

	camel_rss_store_summary_set_last_updated (store_summary, folder_path, 0);
	camel_rss_store_summary_set_last_etag (store_summary, folder_path, NULL);
	camel_rss_store_summary_set_last_modified (store_summary, folder_path, NULL);

	camel_store_get_folder (store, folder_path, CAMEL_STORE_FOLDER_NONE, G_PRIORITY_DEFAULT, NULL,
		e_rss_mail_folder_reload_got_folder_cb, shell_view);

	g_clear_object (&store_summary);
	g_clear_object (&store);
	g_free (folder_path);
}

static void
e_rss_shell_view_update_actions_cb (EShellView *shell_view)
{
	CamelStore *store = NULL;
	EUIAction *action;
	gboolean is_rss_folder = FALSE;

	is_rss_folder = e_rss_check_rss_folder_selected (shell_view, &store, NULL);

	action = e_shell_view_get_action (shell_view, "e-rss-mail-folder-reload-action");

	if (action) {
		e_ui_action_set_visible (action, is_rss_folder);

		if (store) {
			CamelSession *session;

			session = camel_service_ref_session (CAMEL_SERVICE (store));
			e_ui_action_set_sensitive (action, session && camel_session_get_online (session));
			g_clear_object (&session);
		} else {
			e_ui_action_set_sensitive (action, FALSE);
		}
	}

	g_clear_object (&store);
}

static void
e_rss_shell_view_extension_constructed (GObject *object)
{
	EShellViewClass *shell_view_class;
	EShellView *shell_view;

	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_rss_shell_view_extension_parent_class)->constructed (object);

	shell_view = E_SHELL_VIEW (e_extension_get_extensible (E_EXTENSION (object)));
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_if_fail (shell_view_class != NULL);

	if (g_strcmp0 (shell_view_class->ui_manager_id, "org.gnome.evolution.mail") == 0) {
		static const gchar *eui =
			"<eui>"
			  "<menu id='mail-folder-popup'>"
			    "<placeholder id='mail-folder-popup-actions'>"
			      "<item action='e-rss-mail-folder-reload-action'/>"
			     "</placeholder>"
			  "</menu>"
			"</eui>";

		static const EUIActionEntry entries[] = {
			{ "e-rss-mail-folder-reload-action",
			  NULL,
			  N_("Re_load feed articles"),
			  NULL,
			  N_("Reload all feed articles from the server, updating existing and adding any missing"),
			  action_rss_mail_folder_reload_cb, NULL, NULL, NULL }
		};

		e_ui_manager_add_actions_with_eui_data (e_shell_view_get_ui_manager (shell_view), "mail", GETTEXT_PACKAGE,
			entries, G_N_ELEMENTS (entries), shell_view, eui);

		g_signal_connect (shell_view, "update-actions",
			G_CALLBACK (e_rss_shell_view_update_actions_cb), NULL);
	}
}

static void
e_rss_shell_view_extension_class_init (ERssShellViewExtensionClass *klass)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_rss_shell_view_extension_constructed;

	extension_class = E_EXTENSION_CLASS (klass);
	extension_class->extensible_type = E_TYPE_SHELL_VIEW;
}

static void
e_rss_shell_view_extension_class_finalize (ERssShellViewExtensionClass *klass)
{
}

static void
e_rss_shell_view_extension_init (ERssShellViewExtension *extension)
{
}

void
e_rss_shell_view_extension_type_register (GTypeModule *type_module)
{
	e_rss_shell_view_extension_register_type (type_module);
}
