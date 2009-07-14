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
 *		Chenthill Palanisamy <pchenthill@novell.com>
 *	    Parthasarathi Susarla <sparthasarathi@novell.com>
 *	    Sankar P <psankar@novell.com>
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
#include <time.h>
#include <gtk/gtk.h>

#include "camel/camel-folder.h"
#include "camel/camel-mime-utils.h"
#include "camel/camel-medium.h"
#include "camel/camel-mime-message.h"
#include <mail/em-popup.h>
#include <mail/em-folder-view.h>

#include <e-gw-connection.h>
#include "share-folder.h"

void org_gnome_track_status (gpointer ep, EMPopupTargetSelect *t);

static gchar *
format_date (const gchar * value)
{
	time_t time;
	gchar *str;

	time = e_gw_connection_get_date_from_string (value);
	str = ctime (&time);

	str [strlen(str)-1] = '\0';
	return str;
}

static void
track_status (EPopup *ep, EPopupItem *item, gpointer data)
{
	EMPopupTargetSelect *t = (EMPopupTargetSelect *)data;
	CamelMimeMessage *msg = NULL;
	const CamelInternetAddress *from;
	const gchar *namep, *addp;

	GtkDialog *d;
	GtkTable *table;
	GtkWidget *widget;
	GtkScrolledWindow *win;
	GtkVBox *vbox;

	time_t time;
	gchar *time_str;

	gchar *boldmsg;

	gint row = 0;

	EGwConnection *cnc;
	EGwItem *gwitem;

	/*Get message*/
	msg = camel_folder_get_message (t->folder, g_ptr_array_index (t->uids, 0), NULL);
	if (!msg) {
		g_print ("Error!! No message\n");
		return;
	}

	/*Create the dialog*/
	d = (GtkDialog *) gtk_dialog_new ();
	gtk_dialog_add_button (d, GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_window_set_title (GTK_WINDOW (d), _("Message Status"));

	table = (GtkTable *) gtk_table_new (1, 2, FALSE);
	win = (GtkScrolledWindow *) gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(d)->vbox), GTK_WIDGET (win));
	vbox = (GtkVBox *) gtk_vbox_new (FALSE, 12);
	gtk_scrolled_window_add_with_viewport (win, GTK_WIDGET(vbox));
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), FALSE, TRUE, 0);
	gtk_scrolled_window_set_policy (win, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_table_set_col_spacings (table ,12);
	gtk_table_set_row_spacings (table, 6);

	/*Subject*/
	boldmsg = g_strdup_printf ("<b>%s</b>", _("Subject:"));
	widget = gtk_label_new (boldmsg);
	g_free (boldmsg);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 0, 1, row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new (camel_mime_message_get_subject(msg));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (table, widget , 1, 2, row,  row + 1, GTK_FILL, 0, 0, 0);
	row++;

	/*From*/
	from = camel_mime_message_get_from (msg);
	camel_internet_address_get (from, 0, &namep, &addp);
	boldmsg = g_strdup_printf ("<b>%s</b>", _("From:"));
	widget = gtk_label_new (boldmsg);
	g_free (boldmsg);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 0, 1,  row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new (namep);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (table, widget , 1, 2, row,  row + 1, GTK_FILL, 0, 0, 0);
	row++;

	/*creation date*/
	time = camel_mime_message_get_date (msg, NULL);
	time_str = ctime (&time);
	time_str[strlen(time_str)-1] = '\0' ;
	boldmsg = g_strdup_printf ("<b>%s</b>", _("Creation date:"));
	widget = gtk_label_new (boldmsg);
	g_free (boldmsg);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 0, 1, row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new (time_str);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (table, widget , 1, 2, row,  row + 1, GTK_FILL, 0, 0, 0);
	row++;

	/*spacing*/
	widget = gtk_label_new ("");
	gtk_table_attach (table, widget, 0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
	row++;

	/*Table headers*/
	row = 0;
	table = (GtkTable *) gtk_table_new (1, 3, FALSE);
	gtk_table_set_col_spacings (table ,12);
	gtk_table_set_row_spacings (table, 6);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (table), FALSE, TRUE, 0);
	cnc = get_cnc (t->folder->parent_store);

	if (E_IS_GW_CONNECTION(cnc)) {
		GSList *recipient_list;
		e_gw_connection_get_item (cnc, get_container_id (cnc, "Sent Items"), g_ptr_array_index (t->uids, 0), "distribution recipientStatus", &gwitem);
		recipient_list = e_gw_item_get_recipient_list (gwitem);
		for (;recipient_list != NULL;  recipient_list = recipient_list->next)
		{
			EGwItemRecipient *recipient;
			GString *label = NULL;
			GtkLabel *detail;

			label = g_string_new("");
			recipient = recipient_list->data;

			if (recipient->display_name) {
				label = g_string_append (label, "<b>");
				label = g_string_append (label, _("Recipient: "));
				label = g_string_append (label, recipient->display_name);
				label = g_string_append (label, "</b>");
				label = g_string_append_c (label, '\n');
			}

			if (recipient->delivered_date) {
				label = g_string_append (label, _("Delivered: "));
				label = g_string_append (label, format_date(recipient->delivered_date));
				label = g_string_append_c (label, '\n');
			}

			if (recipient->opened_date) {
				label = g_string_append (label, _("Opened: "));
				label = g_string_append (label, format_date(recipient->opened_date));
				label = g_string_append_c (label, '\n');
			}
			if (recipient->accepted_date) {
				label = g_string_append (label, _("Accepted: "));
				label = g_string_append (label, format_date(recipient->accepted_date));
				label = g_string_append_c (label, '\n');
			}
			if (recipient->deleted_date) {
				label = g_string_append (label, _("Deleted: "));
				label = g_string_append (label, format_date(recipient->deleted_date));
				label = g_string_append_c (label, '\n');
			}
			if (recipient->declined_date) {
				label = g_string_append (label, _("Declined: "));
				label = g_string_append (label, format_date(recipient->declined_date));
				label = g_string_append_c (label, '\n');
			}
			if (recipient->completed_date) {
				label = g_string_append (label, _("Completed: "));
				label = g_string_append (label, format_date(recipient->completed_date));
				label = g_string_append_c (label, '\n');
			}
			if (recipient->undelivered_date) {
				label = g_string_append (label, _("Undelivered: "));
				label = g_string_append (label, format_date(recipient->undelivered_date));
				label = g_string_append_c (label, '\n');
			}

			detail = GTK_LABEL(gtk_label_new (label->str));
			g_string_free (label, TRUE);
			gtk_label_set_selectable (detail, TRUE);
			gtk_label_set_use_markup (detail, TRUE);
			gtk_table_attach (table, GTK_WIDGET(detail) , 1, 2, row,  row+1, GTK_FILL, 0, 0, 0);
			row++;
		}
	}

	/*set size and display the dialog*/
	gtk_widget_set_size_request (GTK_WIDGET (win), 400, 300);
	gtk_widget_show_all (GTK_WIDGET (d));
	if (gtk_dialog_run (d) == GTK_RESPONSE_OK)
		gtk_widget_destroy (GTK_WIDGET (d));
	else
		gtk_widget_destroy (GTK_WIDGET (d));
}

static EPopupItem popup_items[] = {
	{ E_POPUP_ITEM, (gchar * ) "20.emfv.02", (gchar *) N_("Track Message Status..."), track_status, NULL, NULL, 0, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY}
};

static void
popup_free (EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free (items);
}

void org_gnome_track_status (gpointer ep, EMPopupTargetSelect *t)
{
	GSList *menus = NULL;

	gint i = 0;
	static gint first = 0;

	if (! g_strrstr (t->uri, "groupwise://") || g_ascii_strncasecmp ((t->folder)->full_name, "Sent Items", 10))
		return;

	/* for translation*/
	if (!first) {
		popup_items[0].label =  _(popup_items[0].label);

	}

	first++;

	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);

	e_popup_add_items (t->target.popup, menus, NULL, popup_free, t);

}
