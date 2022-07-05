/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "mail/e-mail-reader-utils.h"
#include "mail/em-folder-tree.h"
#include "shell/e-shell-content.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-window.h"

#include "../camel-rss-store-summary.h"

#include "module-rss.h"

static const gchar *mail_ui_def =
	"<popup name=\"mail-folder-popup\">\n"
	"  <placeholder name=\"mail-folder-popup-actions\">\n"
	"    <menuitem action=\"e-rss-mail-folder-reload-action\"/>\n"
	"  </placeholder>\n"
	"</popup>\n";

#define E_TYPE_RSS_SHELL_VIEW_EXTENSION (e_rss_shell_view_extension_get_type ())

GType e_rss_shell_view_extension_get_type (void);

typedef struct _ERssShellViewExtension {
	EExtension parent;
	guint current_ui_id;
	gboolean actions_added;
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

		shell_content = e_shell_view_get_shell_content (shell_view);

		e_mail_reader_refresh_folder (E_MAIL_READER (shell_content), folder);

		g_object_unref (folder);
	} else {
		g_warning ("%s: Failed to get folder: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}
}

static void
action_rss_mail_folder_reload_cb (GtkAction *action,
				  EShellView *shell_view)
{
	CamelStore *store = NULL;
	CamelRssStoreSummary *store_summary = NULL;
	gchar *folder_path = NULL;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	if (!e_rss_check_rss_folder_selected (shell_view, &store, &folder_path))
		return;

	g_object_get (store, "summary", &store_summary, NULL);

	camel_rss_store_summary_set_last_updated (store_summary, folder_path, 0);

	camel_store_get_folder (store, folder_path, CAMEL_STORE_FOLDER_NONE, G_PRIORITY_DEFAULT, NULL,
		e_rss_mail_folder_reload_got_folder_cb, shell_view);

	g_clear_object (&store_summary);
	g_clear_object (&store);
	g_free (folder_path);
}

static void
e_rss_shell_view_update_actions_cb (EShellView *shell_view,
				    GtkActionEntry *entries)
{
	CamelStore *store = NULL;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkUIManager *ui_manager;
	gboolean is_rss_folder = FALSE;

	is_rss_folder = e_rss_check_rss_folder_selected (shell_view, &store, NULL);

	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	action_group = e_lookup_action_group (ui_manager, "mail");
	action = gtk_action_group_get_action (action_group, "e-rss-mail-folder-reload-action");

	if (action) {
		gtk_action_set_visible (action, is_rss_folder);

		if (store) {
			CamelSession *session;

			session = camel_service_ref_session (CAMEL_SERVICE (store));
			gtk_action_set_sensitive (action, session && camel_session_get_online (session));
			g_clear_object (&session);
		} else {
			gtk_action_set_sensitive (action, FALSE);
		}
	}

	g_clear_object (&store);
}

static void
e_rss_shell_view_toggled_cb (EShellView *shell_view,
			     ERssShellViewExtension *extension)
{
	EShellViewClass *shell_view_class;
	EShellWindow *shell_window;
	GtkUIManager *ui_manager;
	gboolean is_active, need_update;
	GError *error = NULL;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (extension != NULL);

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_if_fail (shell_view_class != NULL);

	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);

	need_update = extension->current_ui_id != 0;

	if (extension->current_ui_id) {
		gtk_ui_manager_remove_ui (ui_manager, extension->current_ui_id);
		extension->current_ui_id = 0;
	}

	is_active = e_shell_view_is_active (shell_view);

	if (!is_active || g_strcmp0 (shell_view_class->ui_manager_id, "org.gnome.evolution.mail") != 0) {
		if (need_update)
			gtk_ui_manager_ensure_update (ui_manager);

		return;
	}

	if (!extension->actions_added) {
		GtkActionEntry mail_folder_context_entries[] = {
			{ "e-rss-mail-folder-reload-action",
			  NULL,
			  N_("Re_load feed articles"),
			  NULL,
			  N_("Reload all feed articles from the server, updating existing and adding any missing"),
			  G_CALLBACK (action_rss_mail_folder_reload_cb) }
		};

		GtkActionGroup *action_group;

		action_group = e_shell_window_get_action_group (shell_window, "mail");

		e_action_group_add_actions_localized (
			action_group, GETTEXT_PACKAGE,
			mail_folder_context_entries, G_N_ELEMENTS (mail_folder_context_entries), shell_view);

		g_signal_connect (shell_view, "update-actions",
			G_CALLBACK (e_rss_shell_view_update_actions_cb), NULL);

		extension->actions_added = TRUE;
	}

	extension->current_ui_id = gtk_ui_manager_add_ui_from_string (ui_manager, mail_ui_def, -1, &error);

	if (error) {
		g_warning ("%s: Failed to add ui definition: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	gtk_ui_manager_ensure_update (ui_manager);
}

static void
e_rss_shell_view_extension_constructed (GObject *object)
{
	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_rss_shell_view_extension_parent_class)->constructed (object);

	g_signal_connect_object (e_extension_get_extensible (E_EXTENSION (object)), "toggled",
		G_CALLBACK (e_rss_shell_view_toggled_cb), object, 0);
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
