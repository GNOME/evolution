/*
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
 * Authors:
 *		Sankar P <psankar@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <e-gw-connection.h>

#include <e-util/e-alert-dialog.h>

#include <mail/e-mail-reader.h>
#include <mail/em-utils.h>
#include <mail/message-list.h>

#include "gw-ui.h"
#include "share-folder.h"

static gboolean
get_selected_info (EShellView *shell_view, CamelFolder **folder, gchar **selected_uid)
{
	EShellContent *shell_content;
	EMailReader *reader;
	GPtrArray *uids;

	shell_content = e_shell_view_get_shell_content (shell_view);

	reader = E_MAIL_READER (shell_content);
	uids = e_mail_reader_get_selected_uids (reader);

	if (uids && uids->len == 1) {
		*folder = e_mail_reader_get_folder (reader);
		*selected_uid = g_strdup (g_ptr_array_index (uids, 0));
	}

	em_utils_uids_free (uids);

	return *selected_uid != NULL;
}

void
gw_retract_mail_cb (GtkAction *action, EShellView *shell_view)
{
	EGwConnection *cnc;
	CamelFolder *folder;
	CamelStore *parent_store;
	gchar *id = NULL;
	GtkWidget *confirm_dialog, *confirm_warning;
	GtkWidget *content_area;
	gint n;

	g_return_if_fail (get_selected_info (shell_view, &folder, &id));
	g_return_if_fail (folder != NULL);

	parent_store = camel_folder_get_parent_store (folder);

	cnc = get_cnc (parent_store);

	if (cnc && E_IS_GW_CONNECTION(cnc)) {
		confirm_dialog = gtk_dialog_new_with_buttons (
			_("Message Retract"),
			GTK_WINDOW (e_shell_view_get_shell_window (shell_view)),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_YES, GTK_RESPONSE_YES,
			GTK_STOCK_NO, GTK_RESPONSE_NO, NULL);

		content_area = gtk_dialog_get_content_area (
			GTK_DIALOG (confirm_dialog));

		confirm_warning = gtk_label_new (_("Retracting a message may remove it from the recipient's mailbox. Are you sure you want to do this?"));
		gtk_label_set_line_wrap (GTK_LABEL (confirm_warning), TRUE);
		gtk_label_set_selectable (GTK_LABEL (confirm_warning), TRUE);

		gtk_container_add (GTK_CONTAINER (content_area), confirm_warning);
		gtk_widget_set_size_request (confirm_dialog, 400, 100);
		gtk_widget_show_all (confirm_dialog);

		n =gtk_dialog_run (GTK_DIALOG (confirm_dialog));

		gtk_widget_destroy (confirm_warning);
		gtk_widget_destroy (confirm_dialog);

		if (n == GTK_RESPONSE_YES) {

			if (e_gw_connection_retract_request (cnc, id, NULL, FALSE, FALSE) != E_GW_CONNECTION_STATUS_OK )
				e_alert_run_dialog_for_args (GTK_WINDOW (e_shell_view_get_shell_window (shell_view)),
							     "org.gnome.evolution.message.retract:retract-failure",
							     NULL);
			else {
				GtkWidget *dialog;
				dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("Message retracted successfully"));
				gtk_dialog_run (GTK_DIALOG(dialog));
				gtk_widget_destroy (dialog);
			}
		}
	}

	g_free (id);
}
