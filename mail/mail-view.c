/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>
#include "mail.h"
#include "mail-ops.h"
#include "camel/camel.h"

typedef struct mail_view_data_s {
	CamelFolder *source;
	gchar *uid;
	CamelMimeMessage *msg;
	MailDisplay *md;
} mail_view_data;

#define MINIMUM_WIDTH  600
#define MINIMUM_HEIGHT 400

/* Size of the window last time it was changed.  */
static GtkAllocation last_allocation = { 0, 0 };

static void
mail_view_data_free (gpointer mvd)
{
	mail_view_data *data = (mail_view_data *) mvd;

	if (data->uid)
		g_free (data->uid);
	if (data->msg)
		camel_object_unref (CAMEL_OBJECT (data->msg));
	if (data->source)
		camel_object_unref (CAMEL_OBJECT (data->source));

	g_free (data);
}

static mail_view_data *
mail_view_data_new (CamelFolder *source, const gchar *uid, CamelMimeMessage *msg)
{
	mail_view_data *data;

	data = g_new (mail_view_data, 1);
	data->source = source;
	camel_object_ref (CAMEL_OBJECT (data->source));
	data->msg = msg;
	camel_object_ref (CAMEL_OBJECT (data->msg));
	data->uid = g_strdup (uid);

	return data;
}

static void
on_close (GtkWidget *menuitem, gpointer user_data)
{
	GtkWidget *view_window;

	view_window = gtk_object_get_data (GTK_OBJECT (menuitem), "view-window");
	g_return_if_fail (view_window);
	gtk_widget_destroy (GTK_WIDGET (view_window));
}

static void
view_reply_to_sender (GtkWidget *widget, gpointer user_data)
{
	mail_view_data *data = (mail_view_data *) user_data;

	mail_reply (data->source, data->msg, data->uid, FALSE);
}

static void
view_reply_to_all (GtkWidget *widget, gpointer user_data)
{
	mail_view_data *data = (mail_view_data *) user_data;

	mail_reply (data->source, data->msg, data->uid, TRUE);
}

static void
view_forward_msg (GtkWidget *widget, gpointer user_data)
{
	mail_view_data *data = (mail_view_data *) user_data;

	GPtrArray *uids;
	EMsgComposer *composer;

	uids = g_ptr_array_new();
	g_ptr_array_add (uids, g_strdup (data->uid));

	composer = E_MSG_COMPOSER (e_msg_composer_new ());
	gtk_signal_connect (GTK_OBJECT (composer), "send",
			    GTK_SIGNAL_FUNC (composer_send_cb), NULL);

	mail_do_forward_message (data->msg, data->source, uids, composer);
}

static void
view_print_msg (GtkWidget *widget, gpointer user_data)
{
	mail_view_data *data = (mail_view_data *) user_data;

	mail_print_msg (data->md);
}

static void
view_delete_msg (GtkWidget *button, gpointer user_data)
{
	mail_view_data *data = (mail_view_data *) user_data;

	GPtrArray *uids;

	uids = g_ptr_array_new();
	g_ptr_array_add (uids, g_strdup (data->uid));
	mail_do_flag_messages (data->source, uids, TRUE,
			       CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
}

static void
view_size_allocate_cb (GtkWidget *widget,
		       GtkAllocation *allocation)
{
	last_allocation = *allocation;
}

static GnomeUIInfo mail_view_toolbar [] = {
	
	/*GNOMEUIINFO_ITEM_STOCK (N_("Save"), N_("Save this message"),
	  save_msg, GNOME_STOCK_PIXMAP_SAVE),*/
	
	GNOMEUIINFO_ITEM_STOCK (N_("Reply"), N_("Reply to the sender of this message"),
				view_reply_to_sender, GNOME_STOCK_PIXMAP_MAIL_RPL),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Reply to All"), N_("Reply to all recipients of this message"),
				view_reply_to_all, GNOME_STOCK_PIXMAP_MAIL_RPL),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Forward"), N_("Forward this message"), view_forward_msg, GNOME_STOCK_PIXMAP_MAIL_FWD),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK (N_("Print"), N_("Print the selected message"), view_print_msg, GNOME_STOCK_PIXMAP_PRINT),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Delete"), N_("Delete this message"), view_delete_msg, GNOME_STOCK_PIXMAP_TRASH),
	
	/*GNOMEUIINFO_SEPARATOR,*/
	
	/*GNOMEUIINFO_ITEM_STOCK (N_("Next"), N_("Next message"), mail_view_next_msg, GNOME_STOCK_PIXMAP_NEXT),
	
	  GNOMEUIINFO_ITEM_STOCK (N_("Previous"), N_("Previous message"), mail_view_prev_msg, GNOME_STOCK_PIXMAP_PREVIOUS),*/
	
	GNOMEUIINFO_END
};

static GnomeUIInfo file_menu[] = {
	/*GNOMEUIINFO_MENU_SAVE_ITEM (save, NULL),*/
	/*GNOMEUIINFO_MENU_SAVE_AS_ITEM (save_as, NULL),*/
	/*GNOMEUIINFO_SEPARATOR,*/
	GNOMEUIINFO_MENU_CLOSE_ITEM (on_close, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_menu[] =
{
	GNOMEUIINFO_END
};

static GnomeUIInfo mail_view_menubar[] =
{
	GNOMEUIINFO_MENU_FILE_TREE (file_menu),
	GNOMEUIINFO_MENU_VIEW_TREE (view_menu),
	GNOMEUIINFO_END
};

static void
set_default_size (GtkWidget *widget)
{
	int width, height;

	width  = MAX (MINIMUM_WIDTH, last_allocation.width);
	height = MAX (MINIMUM_HEIGHT, last_allocation.height);

	gtk_window_set_default_size (GTK_WINDOW (widget), width, height);
}

GtkWidget *
mail_view_create (CamelFolder *source, const char *uid, CamelMimeMessage *msg)
{
	GtkWidget *window;
	GtkWidget *toolbar;
	GtkWidget *mail_display;
	char *subject;
	mail_view_data *data;

	data = mail_view_data_new (source, uid, msg);

	subject = (char *) camel_mime_message_get_subject (msg);
	if (!subject)
		subject = "";
	
	window = gnome_app_new ("Evolution", subject);
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	
	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  mail_view_toolbar,
					  NULL, data);

	gnome_app_set_toolbar (GNOME_APP (window), GTK_TOOLBAR (toolbar));
	gnome_app_create_menus (GNOME_APP (window), mail_view_menubar);

	gtk_object_set_data_full (GTK_OBJECT (window), "mvd", data,
				  mail_view_data_free);

	gtk_widget_ref (mail_view_menubar[0].widget);
	gtk_object_set_data_full (GTK_OBJECT (window), "file",
				  mail_view_menubar[0].widget,
				  (GtkDestroyNotify) gtk_widget_unref);
	
	gtk_widget_ref (file_menu[0].widget);
	gtk_object_set_data (GTK_OBJECT (file_menu[0].widget), "view-window", window);
	gtk_object_set_data_full (GTK_OBJECT (window), "close",
				  file_menu[0].widget,
				  (GtkDestroyNotify) gtk_widget_unref);
	
	gtk_widget_ref (mail_view_menubar[1].widget);
	gtk_object_set_data_full (GTK_OBJECT (window), "view",
				  mail_view_menubar[1].widget,
				  (GtkDestroyNotify) gtk_widget_unref);
	
	mail_display = mail_display_new ();
	mail_display_set_message (MAIL_DISPLAY (mail_display), CAMEL_MEDIUM (msg));
	data->md = MAIL_DISPLAY (mail_display);
	gnome_app_set_contents (GNOME_APP (window), mail_display);

	gtk_signal_connect (GTK_OBJECT (window), "size_allocate",
			    GTK_SIGNAL_FUNC (view_size_allocate_cb), NULL);

	set_default_size (window);
	
	return window;
}

