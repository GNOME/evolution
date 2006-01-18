/*
 * Copyright (C) 2004 Novell, Inc.
 *
 * Author(s): Chenthill Palanisamy <pchenthill@novell.com>
 *	      Parthasarathi Susarla <sparthasarathi@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
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


void org_gnome_track_status (void *ep, EMPopupTargetSelect *t) ;
void add_recipient (GtkTable *table, char *recp, int row) ;
int add_detail (GtkTable *table, char *label, char *value, int row) ;

void
add_recipient (GtkTable *table, char *recp, int row)
{
	GtkWidget *widget ;

	widget = gtk_label_new (recp) ;
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (table, widget , 0, 1, row,  row + 1, GTK_FILL, 0, 0, 0);
}

int
add_detail (GtkTable *table, char *label, char *value, int row)
{
	GtkWidget *widget;
	time_t time;
	time_t actual_time;
	char *str;

	time = e_gw_connection_get_date_from_string (value);
	actual_time = camel_header_decode_date (ctime(&time), NULL);
	*str = ctime (&actual_time);

	str [strlen(str)-1] = '\0';

	widget = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (table, widget , 1, 2, row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new (str);
	gtk_table_attach (table, widget , 2, 3, row,  row + 1, GTK_FILL, 0, 0, 0);
	row++;

	return row ;
}

static void 
track_status (EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetSelect *t = (EMPopupTargetSelect *)data;
	CamelMimeMessage *msg = NULL ;
	const CamelInternetAddress *from ;
	const char *namep, *addp ;
	
	GtkDialog *d ;
	GtkTable *table ;
	GtkWidget *widget;
	GtkScrolledWindow *win;
	GtkVBox *vbox;
	
	time_t time ;
	char *time_str ;
	
	const char *status = NULL ;
	char **temp1 = NULL, **temp2 = NULL , **ptr = NULL, *str = NULL ;

	int row = 0;

	/*check if it is a groupwise account*/
	/*Get message*/
	msg = camel_folder_get_message (t->folder, g_ptr_array_index (t->uids, 0), NULL);
	if (!msg) {
		g_print ("Error!! No message\n") ;
		return ;
	}
	status = camel_medium_get_header ( CAMEL_MEDIUM(msg), "X-gw-status-opt") ;
	if (!status) {
		g_print ("Error!! No header\n") ;
		return ;
	} 
	
	/*Create the dialog*/
	d = (GtkDialog *) gtk_dialog_new ();
	gtk_dialog_add_button (d, GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_window_set_title (GTK_WINDOW (d), "Message Status");

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
	widget = gtk_label_new ("<b>Subject</b> :");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 0, 1, row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new (camel_mime_message_get_subject(msg));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (table, widget , 1, 2, row,  row + 1, GTK_FILL, 0, 0, 0);
	row++;

	/*From*/
	from = camel_mime_message_get_from (msg) ;
	camel_internet_address_get (from, 0, &namep, &addp) ;
	widget = gtk_label_new ("<b>From</b> :");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 0, 1,  row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new (namep);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (table, widget , 1, 2, row,  row + 1, GTK_FILL, 0, 0, 0);
	row++;
	
	/*creation date*/
	time = camel_mime_message_get_date (msg, NULL) ;
	time_str = ctime (&time) ;
	time_str[strlen(time_str)-1] = '\0' ;
	widget = gtk_label_new ("<b>Creation date</b> :");
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
	widget = gtk_label_new ("<b>Recipients </b>");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 0, 1, row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new ("<b>Action</b>");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 1, 2, row,  row + 1, GTK_FILL, 0, 0, 0);
	widget = gtk_label_new ("<b>Date and Time</b>");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_table_attach (table, widget , 2, 3,  row,  row + 1, GTK_FILL, 0, 0, 0);
	row++;

	
	temp1 = g_strsplit (status, "::", -1) ;
	ptr = temp1 ;
	str = *ptr ;
	while (str) {
		temp2 = g_strsplit (str, ";", -1) ;
		if (*temp2) {
			if (strlen(temp2[0]));
			if (strlen(temp2[1]))
				add_recipient (table, temp2[1], row++);
			/*we will decrement the row if there is info to be displayed in the same line*/
			if (strlen(temp2[2]));
			if (strlen(temp2[3]))
				row = add_detail (table,"delivered" , temp2[3], --row);
			if (strlen(temp2[4]))
				row = add_detail (table,"opened" , temp2[3], row) ;
			if (strlen(temp2[5]))
				row = add_detail (table,"accepted" , temp2[3], row) ;
			if (strlen(temp2[6]))
				row = add_detail (table,"deleted" , temp2[3], row) ;
			if (strlen(temp2[7]))
				row = add_detail (table,"declined" , temp2[3], row) ;
			if (strlen(temp2[8]))
				row = add_detail (table,"completed" , temp2[3], row) ;
			if (strlen(temp2[9]))
				row = add_detail (table,"undelivered" , temp2[3], --row);
		}
		str = *(++ptr) ;
		g_strfreev (temp2) ;
	}

	/*set size and display the dialog*/
	gtk_widget_set_usize (GTK_WIDGET (win), 400, 300);
	gtk_widget_show_all (GTK_WIDGET (d));
	if (gtk_dialog_run (d) == GTK_RESPONSE_OK)
		gtk_widget_destroy (GTK_WIDGET (d));
	else
		gtk_widget_destroy (GTK_WIDGET (d));

	
	g_strfreev (temp1) ;
	
}

/*
 * The format for the options is:
 *			    0        1   2      3          4     5         6      7         8         9
 *     X-gw-status-opt: /TO/CC/BCC;name;email;delivered;opened;accepted;deleted;declined;completed;undelivered::
 */

static EPopupItem popup_items[] = {
{ E_POPUP_ITEM, "20.emfv.02", N_("Track Message Status..."), track_status, NULL, NULL, 0, EM_POPUP_SELECT_ONE|EM_FOLDER_VIEW_SELECT_LISTONLY}
};

static void 
popup_free (EPopup *ep, GSList *items, void *data)
{
	g_slist_free (items);
}

void org_gnome_track_status (void *ep, EMPopupTargetSelect *t)
{
	GSList *menus = NULL;
	
	int i = 0;
	static int first = 0;

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
