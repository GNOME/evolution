/*
 *
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <e-util/e-config.h>
#include <mail/em-popup.h>
#include <mail/mail-ops.h>
#include <mail/mail-mt.h>
#include <camel/camel-vee-folder.h>
#include "e-util/e-error.h"

#define PRIMARY_TEXT \
	N_("Also mark messages in subfolders?")
#define SECONDARY_TEXT \
	N_("Do you want to mark messages as read in the current folder " \
	   "only, or in the current folder as well as all subfolders?")

void org_gnome_mark_all_read (EPlugin *ep, EMPopupTargetFolder *target);
static void mar_got_folder (gchar *uri, CamelFolder *folder, gpointer data);
static void mar_all_sub_folders (CamelStore *store, CamelFolderInfo *fi, CamelException *ex);

static void
button_clicked_cb (GtkButton *button,
                   GtkDialog *dialog)
{
	gpointer response;

	response = g_object_get_data (G_OBJECT (button), "response");
	gtk_dialog_response (dialog, GPOINTER_TO_INT (response));
}

static void
box_mapped_cb (GtkWidget *box,
               GtkWidget *label)
{
	GtkRequisition requisition;

	/* In order to get decent line wrapping we need to wait until the
	 * box containing the buttons is mapped, and then resize the label
	 * to the same width as the box. */
	gtk_widget_size_request (box, &requisition);
	gtk_widget_set_size_request (label, requisition.width, -1);
}

static gint
prompt_user (void)
{
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *table;
	GtkWidget *widget;
	GtkWidget *vbox;
	gchar *markup;
	gint response;

	dialog = gtk_dialog_new ();
	gtk_widget_hide (GTK_DIALOG (dialog)->action_area);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), "");
	g_signal_connect (
		dialog, "map",
		G_CALLBACK (gtk_widget_queue_resize), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
	vbox = GTK_DIALOG (dialog)->vbox;

	/* Table */
	widget = gtk_table_new (3, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (widget), 12);
	gtk_table_set_col_spacings (GTK_TABLE (widget), 12);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	table = widget;

	/* Question Icon */
	widget = gtk_image_new_from_stock (
		GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.5, 0.0);
	gtk_table_attach (
		GTK_TABLE (table), widget, 0, 1, 0, 3,
		0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);

	/* Primary Text */
	markup = g_markup_printf_escaped (
		"<big><b>%s</b></big>", gettext (PRIMARY_TEXT));
	widget = gtk_label_new (markup);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 0, 1,
		0, 0, 0, 0);
	gtk_widget_show (widget);
	g_free (markup);
	label1 = widget;

	/* Secondary Text */
	widget = gtk_label_new (gettext (SECONDARY_TEXT));
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 1, 2,
		0, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);
	label2 = widget;

	/* Action Area */
	widget = gtk_hbox_new (FALSE, 6);
	g_signal_connect (
		widget, "map",
		G_CALLBACK (box_mapped_cb), label1);
	g_signal_connect (
		widget, "map",
		G_CALLBACK (box_mapped_cb), label2);
	gtk_table_attach (
		GTK_TABLE (table), widget, 1, 2, 2, 3,
		GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_widget_show (widget);

	container = widget;

	/* Cancel button */
	widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_object_set_data (
		G_OBJECT (widget), "response",
		GINT_TO_POINTER (GTK_RESPONSE_CANCEL));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (button_clicked_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* To Translators: It's a response button caption on a question
	   "Do you want to mark messages as read in the current folder
	   only, or in the current folder as well as all subfolders?" */
	widget = gtk_button_new_with_mnemonic (
		_("In Current Folder and _Subfolders"));
	g_object_set_data (
		G_OBJECT (widget), "response",
		GINT_TO_POINTER (GTK_RESPONSE_YES));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (button_clicked_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* To Translators: It's a response button caption on a question
	   "Do you want to mark messages as read in the current folder
	   only, or in the current folder as well as all subfolders?" */
	widget = gtk_button_new_with_mnemonic (
		_("In Current _Folder Only"));
	g_object_set_data (
		G_OBJECT (widget), "response",
		GINT_TO_POINTER (GTK_RESPONSE_NO));
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (button_clicked_cb), dialog);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return response;
}

void
org_gnome_mark_all_read (EPlugin *ep, EMPopupTargetFolder *t)
{
	if (t->uri == NULL) {
		return;
	}

	mail_get_folder(t->uri, 0, mar_got_folder, NULL, mail_msg_unordered_push);
}

static void
mark_all_as_read (CamelFolder *folder)
{
	GPtrArray *uids;
	gint i;

	uids =  camel_folder_get_uids (folder);
	camel_folder_freeze(folder);
	for (i=0;i<uids->len;i++)
		camel_folder_set_message_flags(folder, uids->pdata[i], CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	camel_folder_thaw(folder);
	camel_folder_free_uids (folder, uids);
}

static void
mar_got_folder (gchar *uri, CamelFolder *folder, gpointer data)
{
	CamelFolderInfo *info;
	CamelStore *store;
	CamelException ex;
	gint response;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST;

	/* FIXME we have to disable the menu item */
	if (!folder)
		return;

	camel_exception_init (&ex);

	store = folder->parent_store;
	info = camel_store_get_folder_info (store, folder->full_name, flags, &ex);

	if (camel_exception_is_set (&ex))
		goto out;

	if (info && (info->child || info->next))
		response = prompt_user ();
	else
		response = GTK_RESPONSE_NO;

	if (response == GTK_RESPONSE_NO)
		mark_all_as_read (folder);
	else if (response == GTK_RESPONSE_YES)
		mar_all_sub_folders (store, info, &ex);
out:
	camel_store_free_folder_info(store, info);
}

static void
mar_all_sub_folders (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
	while (fi) {
		CamelFolder *folder;

		if (fi->child) {
			mar_all_sub_folders (store, fi->child, ex);
			if (camel_exception_is_set (ex))
				return;
		}

		if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
			return;

		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			mark_all_as_read (folder);
		}

		fi = fi->next;
	}
}
