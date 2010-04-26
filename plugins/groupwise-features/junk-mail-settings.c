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
 *		Vivek Jain <jvivek@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>

#include <mail/e-mail-reader.h>
#include <mail/message-list.h>

#include <e-gw-connection.h>

#include "gw-ui.h"
#include "share-folder.h"
#include "junk-settings.h"

static void
abort_changes (JunkSettings *js)
{
	g_object_run_dispose ((GObject *)js);
}

static void
junk_dialog_response (GtkWidget *dialog, gint response, JunkSettings *js)
{
	if (response == GTK_RESPONSE_ACCEPT) {
		commit_changes(js);
		abort_changes (js);
	}
	else
		abort_changes (js);

	gtk_widget_destroy (dialog);

}

void
gw_junk_mail_settings_cb (GtkAction *action, EShellView *shell_view)
{
	GtkWidget *dialog ,*w, *notebook, *box;
	GtkWidget *content_area;
	JunkSettings *junk_tab;
	gint page_count =0;
	EGwConnection *cnc;
	gchar *msg;
	EShellContent *shell_content;
	EMailReader *reader;
	CamelFolder *folder;

	shell_content = e_shell_view_get_shell_content (shell_view);

	reader = E_MAIL_READER (shell_content);
	folder = e_mail_reader_get_folder (reader);
	g_return_if_fail (folder != NULL);

	cnc = get_cnc (camel_folder_get_parent_store (folder));

	dialog =  gtk_dialog_new_with_buttons (_("Junk Settings"),
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL,
			GTK_RESPONSE_REJECT,
			GTK_STOCK_OK,
			GTK_RESPONSE_ACCEPT,
			NULL);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_window_set_default_size ((GtkWindow *) dialog, 292, 260);
	gtk_widget_ensure_style (dialog);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);
	box = gtk_vbox_new (FALSE, 6);
	w = gtk_label_new ("");
	msg = g_strdup_printf("<b>%s</b>", _("Junk Mail Settings"));
	gtk_label_set_markup (GTK_LABEL (w), msg);
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 6);
	g_free(msg);

	junk_tab = junk_settings_new (cnc);
	w = (GtkWidget *)junk_tab->vbox;
	gtk_box_pack_start ((GtkBox *) box, w, FALSE, FALSE, 6);

	/* We might have to add more options for settings i.e. more pages */
	while (page_count > 0 ) {
		notebook = gtk_notebook_new ();
		gtk_notebook_append_page ((GtkNotebook *)notebook, box, NULL);
		gtk_box_pack_start (
			GTK_BOX (content_area), notebook, TRUE, TRUE, 0);
	}

	if (page_count == 0)
		gtk_box_pack_start (
			GTK_BOX (content_area), box, TRUE, TRUE, 0);

	g_signal_connect (dialog, "response", G_CALLBACK (junk_dialog_response), junk_tab);
	gtk_widget_show_all (dialog);
}
