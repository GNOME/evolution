/*
 * e-memo-shell-backend.c
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

#include "e-memo-shell-backend.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/libecal.h>

#include "shell/e-shell.h"
#include "shell/e-shell-backend.h"
#include "shell/e-shell-window.h"

#include "calendar/gui/comp-util.h"
#include "calendar/gui/dialogs/memo-editor.h"

#include "e-memo-shell-migrate.h"
#include "e-memo-shell-view.h"

#define E_MEMO_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_BACKEND, EMemoShellBackendPrivate))

#define E_MEMO_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_BACKEND, EMemoShellBackendPrivate))

struct _EMemoShellBackendPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	EMemoShellBackend,
	e_memo_shell_backend,
	E_TYPE_SHELL_BACKEND)

static void
memo_shell_backend_new_memo (ECalClient *cal_client,
                             EShell *shell,
                             CompEditorFlags flags)
{
	ECalComponent *comp;
	CompEditor *editor;

	comp = cal_comp_memo_new_with_defaults (cal_client);
	cal_comp_update_time_by_active_window (comp, shell);
	editor = memo_editor_new (cal_client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
}

static void
memo_shell_backend_memo_new_cb (GObject *source_object,
                                GAsyncResult *result,
                                gpointer user_data)
{
	EShell *shell = E_SHELL (user_data);
	EClient *client;
	CompEditorFlags flags = 0;
	GError *error = NULL;

	flags |= COMP_EDITOR_NEW_ITEM;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (client != NULL) {
		memo_shell_backend_new_memo (
			E_CAL_CLIENT (client), shell, flags);
		g_object_unref (client);
	} else {
		/* XXX Handle errors better. */
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_object_unref (shell);
}

static void
memo_shell_backend_memo_shared_new_cb (GObject *source_object,
                                       GAsyncResult *result,
                                       gpointer user_data)
{
	EShell *shell = E_SHELL (user_data);
	EClient *client;
	CompEditorFlags flags = 0;
	GError *error = NULL;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_IS_SHARED;
	flags |= COMP_EDITOR_USER_ORG;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (client != NULL) {
		memo_shell_backend_new_memo (
			E_CAL_CLIENT (client), shell, flags);
		g_object_unref (client);
	} else {
		/* XXX Handle errors better. */
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_object_unref (shell);
}

static void
action_memo_new_cb (GtkAction *action,
                    EShellWindow *shell_window)
{
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	EClientCache *client_cache;
	const gchar *action_name;

	/* This callback is used for both memos and shared memos. */

	shell = e_shell_window_get_shell (shell_window);
	client_cache = e_shell_get_client_cache (shell);

	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_default_memo_list (registry);

	/* Use a callback function appropriate for the action. */
	action_name = gtk_action_get_name (action);
	if (g_strcmp0 (action_name, "memo-shared-new") == 0)
		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_MEMO_LIST,
			NULL,
			memo_shell_backend_memo_shared_new_cb,
			g_object_ref (shell));
	else
		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_MEMO_LIST,
			NULL,
			memo_shell_backend_memo_new_cb,
			g_object_ref (shell));

	g_object_unref (source);
}

static void
action_memo_list_new_cb (GtkAction *action,
                         EShellWindow *shell_window)
{
	EShell *shell;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	source_type = E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
	config = e_cal_source_config_new (registry, NULL, source_type);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Memo List"));

	gtk_widget_show (dialog);
}

static GtkActionEntry item_entries[] = {

	{ "memo-new",
	  "stock_insert-note",
	  NC_("New", "Mem_o"),
	  "<Shift><Control>o",
	  N_("Create a new memo"),
	  G_CALLBACK (action_memo_new_cb) },

	{ "memo-shared-new",
	  "stock_insert-note",
	  NC_("New", "_Shared Memo"),
	  "<Shift><Control>h",
	  N_("Create a new shared memo"),
	  G_CALLBACK (action_memo_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "memo-list-new",
	  "stock_notes",
	  NC_("New", "Memo Li_st"),
	  NULL,
	  N_("Create a new memo list"),
	  G_CALLBACK (action_memo_list_new_cb) }
};

static gboolean
memo_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
                                  const gchar *uri)
{
	EShell *shell;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	EClient *client;
	EClientCache *client_cache;
	ECalComponent *comp;
	ESource *source;
	ESourceRegistry *registry;
	SoupURI *soup_uri;
	icalcomponent *icalcomp;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *comp_uid = NULL;
	gchar *comp_rid = NULL;
	gboolean handled = FALSE;
	GError *error = NULL;

	shell = e_shell_backend_get_shell (shell_backend);
	client_cache = e_shell_get_client_cache (shell);

	if (strncmp (uri, "memo:", 5) != 0)
		return FALSE;

	soup_uri = soup_uri_new (uri);

	if (soup_uri == NULL)
		return FALSE;

	cp = soup_uri_get_query (soup_uri);
	if (cp == NULL)
		goto exit;

	while (*cp != '\0') {
		gchar *header;
		gchar *content;
		gsize header_len;
		gsize content_len;

		header_len = strcspn (cp, "=&");

		/* If it's malformed, give up. */
		if (cp[header_len] != '=')
			break;

		header = (gchar *) cp;
		header[header_len] = '\0';
		cp += header_len + 1;

		content_len = strcspn (cp, "&");

		content = g_strndup (cp, content_len);
		if (g_ascii_strcasecmp (header, "source-uid") == 0)
			source_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-uid") == 0)
			comp_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-rid") == 0)
			comp_rid = g_strdup (content);
		g_free (content);

		cp += content_len;
		if (*cp == '&') {
			cp++;
			if (strcmp (cp, "amp;") == 0)
				cp += 4;
		}
	}

	if (source_uid == NULL || comp_uid == NULL)
		goto exit;

	/* URI is valid, so consider it handled.  Whether
	 * we successfully open it is another matter... */
	handled = TRUE;

	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_source (registry, source_uid);
	if (source == NULL) {
		g_printerr ("No source for UID '%s'\n", source_uid);
		goto exit;
	}

	client = e_client_cache_get_client_sync (
		client_cache, source,
		E_SOURCE_EXTENSION_MEMO_LIST,
		NULL, &error);

	/* Sanity check. */
	g_return_val_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)), FALSE);

	if (error != NULL) {
		g_warning (
			"%s: Failed to create/open client: %s",
			G_STRFUNC, error->message);
		g_object_unref (source);
		g_error_free (error);
		goto exit;
	}

	g_object_unref (source);
	source = NULL;

	/* XXX Copied from e_memo_shell_view_open_memo().
	 *     Clearly a new utility function is needed. */

	editor = comp_editor_find_instance (comp_uid);

	if (editor != NULL)
		goto present;

	e_cal_client_get_object_sync (
		E_CAL_CLIENT (client), comp_uid,
		comp_rid, &icalcomp, NULL, &error);

	if (error != NULL) {
		g_warning (
			"%s: Failed to get object: %s",
			G_STRFUNC, error->message);
		g_object_unref (client);
		g_error_free (error);
		goto exit;
	}

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		g_warning ("%s: Failed to set icalcomp to comp\n", G_STRFUNC);
		icalcomponent_free (icalcomp);
		icalcomp = NULL;
	}

	if (e_cal_component_has_organizer (comp))
		flags |= COMP_EDITOR_IS_SHARED;

	if (itip_organizer_is_user (registry, comp, E_CAL_CLIENT (client)))
		flags |= COMP_EDITOR_USER_ORG;

	editor = memo_editor_new (E_CAL_CLIENT (client), shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_unref (comp);

present:
	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (client);

exit:
	g_free (source_uid);
	g_free (comp_uid);
	g_free (comp_rid);

	soup_uri_free (soup_uri);

	return handled;
}

static void
memo_shell_backend_window_added_cb (EShellBackend *shell_backend,
                                    GtkWindow *window)
{
	const gchar *module_name;

	if (!E_IS_SHELL_WINDOW (window))
		return;

	module_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static void
memo_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (memo_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-added",
		G_CALLBACK (memo_shell_backend_window_added_cb),
		shell_backend);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_memo_shell_backend_parent_class)->constructed (object);
}

static void
e_memo_shell_backend_class_init (EMemoShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	g_type_class_add_private (class, sizeof (EMemoShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = memo_shell_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_MEMO_SHELL_VIEW;
	shell_backend_class->name = "memos";
	shell_backend_class->aliases = "";
	shell_backend_class->schemes = "memo";
	shell_backend_class->sort_order = 600;
	shell_backend_class->preferences_page = "calendar-and-tasks";
	shell_backend_class->start = NULL;
	shell_backend_class->migrate = e_memo_shell_backend_migrate;

	/* Register relevant ESource extensions. */
	E_TYPE_SOURCE_MEMO_LIST;
}

static void
e_memo_shell_backend_class_finalize (EMemoShellBackendClass *class)
{
}

static void
e_memo_shell_backend_init (EMemoShellBackend *memo_shell_backend)
{
	memo_shell_backend->priv =
		E_MEMO_SHELL_BACKEND_GET_PRIVATE (memo_shell_backend);
}

void
e_memo_shell_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_memo_shell_backend_register_type (type_module);
}
