/*
 * e-mail-folder-pane.c
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

#include "evolution-config.h"

#include "e-mail-folder-pane.h"

#include <string.h>
#include <glib/gi18n.h>

#include <shell/e-shell.h>
#include <shell/e-shell-utils.h>

#include <libemail-engine/libemail-engine.h>

#include "e-mail-reader.h"
#include "e-mail-reader-utils.h"
#include "em-folder-tree-model.h"
#include "em-composer-utils.h"
#include "em-utils.h"
#include "message-list.h"

#define E_MAIL_FOLDER_PANE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_FOLDER_PANE, EMailFolderPanePrivate))

struct _EMailFolderPanePrivate {
	gint placeholder;
};

G_DEFINE_TYPE (EMailFolderPane, e_mail_folder_pane, E_TYPE_MAIL_PANED_VIEW)

static gboolean
folder_pane_get_preview_visible (EMailView *view)
{
	return FALSE;
}

static void
folder_pane_set_preview_visible (EMailView *view,
                                 gboolean preview_visible)
{
	/* Ignore the request. */
}

static guint
mail_paned_view_open_selected_mail (EMailPanedView *view)
{
	EShell *shell;
	EMailReader *reader;
	EMailBackend *backend;
	ESourceRegistry *registry;
	GPtrArray *uids;
	gint i;
	GtkWindow *window;
	CamelFolder *folder;
	GPtrArray *views;
	guint ii, n_views = 0;

	reader = E_MAIL_READER (view);
	folder = e_mail_reader_ref_folder (reader);
	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	g_return_val_if_fail (uids != NULL, 0);

	backend = e_mail_reader_get_backend (reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));
	registry = e_shell_get_registry (shell);

	if (!em_utils_ask_open_many (window, uids->len))
		goto exit;

	if (em_utils_folder_is_drafts (registry, folder)) {
		e_mail_reader_edit_messages (reader, folder, uids, TRUE, TRUE);
		goto exit;
	}

	if (em_utils_folder_is_outbox (registry, folder)) {
		e_mail_reader_edit_messages (reader, folder, uids, TRUE, TRUE);
		goto exit;
	}

	if (em_utils_folder_is_templates (registry, folder)) {
		e_mail_reader_edit_messages (reader, folder, uids, TRUE, TRUE);
		goto exit;
	}

	views = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	/* For vfolders we need to edit the original, not the vfolder copy. */
	for (ii = 0; ii < uids->len; ii++) {
		const gchar *uid = uids->pdata[ii];
		CamelFolder *real_folder;
		CamelMessageInfo *info;
		gchar *real_uid;

		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			g_ptr_array_add (views, g_strdup (uid));
			continue;
		}

		info = camel_folder_get_message_info (folder, uid);
		if (info == NULL)
			continue;

		real_folder = camel_vee_folder_get_location (
			CAMEL_VEE_FOLDER (folder),
			(CamelVeeMessageInfo *) info, &real_uid);

		if (em_utils_folder_is_drafts (registry, real_folder) ||
			em_utils_folder_is_outbox (registry, real_folder)) {
			GPtrArray *edits;

			edits = g_ptr_array_new_with_free_func (
				(GDestroyNotify) g_free);
			g_ptr_array_add (edits, real_uid);
			e_mail_reader_edit_messages (
				reader, real_folder, edits, TRUE, TRUE);
			g_ptr_array_unref (edits);
		} else {
			g_free (real_uid);
			g_ptr_array_add (views, g_strdup (uid));
		}

		g_clear_object (&info);
	}

	n_views = views->len;

	for (i = 0; i < n_views; i++)
		g_signal_emit_by_name (view, "open-mail", views->pdata[i]);

	g_ptr_array_unref (views);

exit:
	g_clear_object (&folder);
	g_ptr_array_unref (uids);

	return n_views;
}

static void
e_mail_folder_pane_class_init (EMailFolderPaneClass *class)
{
	EMailViewClass *mail_view_class;
	EMailPanedViewClass *mail_paned_view_class;

	g_type_class_add_private (class, sizeof (EMailFolderPanePrivate));

	mail_view_class = E_MAIL_VIEW_CLASS (class);
	mail_view_class->get_preview_visible = folder_pane_get_preview_visible;
	mail_view_class->set_preview_visible = folder_pane_set_preview_visible;

	mail_paned_view_class = E_MAIL_PANED_VIEW_CLASS (class);
	mail_paned_view_class->open_selected_mail = mail_paned_view_open_selected_mail;
}

static void
e_mail_folder_pane_init (EMailFolderPane *folder_pane)
{
	folder_pane->priv = E_MAIL_FOLDER_PANE_GET_PRIVATE (folder_pane);
}

EMailView *
e_mail_folder_pane_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_FOLDER_PANE,
		"shell-view", shell_view, NULL);
}

