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

#include <mail/em-popup.h>
#include <mail/em-folder-view.h>
#include <glib/gi18n-lib.h>
#include <share-folder.h>
#include <e-gw-connection.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <e-util/e-error.h>

void org_gnome_retract_message (EPlugin *ep, EMPopupTargetSelect *t);

static void retract_mail_settings (EPopup *ep, EPopupItem *item, gpointer data)
{
	EGwConnection *cnc;
	CamelFolder *folder = (CamelFolder *)data;
	CamelStore *store = folder->parent_store;
	gchar *id;
	GtkWidget *confirm_dialog, *confirm_warning;
	gint n;

	cnc = get_cnc (store);

	if (cnc && E_IS_GW_CONNECTION(cnc)) {
		id = (gchar *)item->user_data;

		confirm_dialog = gtk_dialog_new_with_buttons (_("Message Retract"), NULL,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_YES, GTK_RESPONSE_YES,
				GTK_STOCK_NO, GTK_RESPONSE_NO, NULL);

		confirm_warning = gtk_label_new (_("Retracting a message may remove it from the recipient's mailbox. Are you sure you want to do this ?"));
		gtk_label_set_line_wrap (GTK_LABEL (confirm_warning), TRUE);
		gtk_label_set_selectable (GTK_LABEL (confirm_warning), TRUE);

		gtk_container_add (GTK_CONTAINER ((GTK_DIALOG(confirm_dialog))->vbox), confirm_warning);
		gtk_widget_set_size_request (confirm_dialog, 400, 100);
		gtk_widget_show_all (confirm_dialog);

		n =gtk_dialog_run (GTK_DIALOG (confirm_dialog));

		gtk_widget_destroy (confirm_warning);
		gtk_widget_destroy (confirm_dialog);

		if (n == GTK_RESPONSE_YES) {

			if (e_gw_connection_retract_request (cnc, id, NULL, FALSE, FALSE) != E_GW_CONNECTION_STATUS_OK )
				e_error_run (NULL, "org.gnome.evolution.message.retract:retract-failure", NULL);
			else {
				GtkWidget *dialog;
				dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("Message retracted successfully"));
				gtk_dialog_run (GTK_DIALOG(dialog));
				gtk_widget_destroy (dialog);
			}
		}
	}
}

static EPopupItem popup_items[] = {
	{ E_POPUP_BAR,  (gchar *) "20.emfv.03" },
	{ E_POPUP_ITEM, (gchar *) "20.emfv.04", (gchar *) N_("Retract Mail"), retract_mail_settings, NULL, NULL, 0, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY}
};

static void popup_free (EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free (items);
}

void org_gnome_retract_message (EPlugin *ep, EMPopupTargetSelect *t)
{
	GSList *menus = NULL;
	GPtrArray *uids;
	gint i = 0;
	static gint first = 0;

	uids = t->uids;
	if (g_strrstr (t->uri, "groupwise://") && !g_ascii_strcasecmp((t->folder)->full_name, "Sent Items")) {

		/* for translation*/
		if (!first) {
			popup_items[1].label =  _(popup_items[1].label);
			popup_items[1].user_data = g_strdup((gchar *) g_ptr_array_index(uids, 0));
		}

		first++;

		for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
			menus = g_slist_prepend (menus, &popup_items[i]);

		e_popup_add_items (t->target.popup, menus, NULL, popup_free, t->folder);
	}
	return;
}
