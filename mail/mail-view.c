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

#include <mail.h>
#include <camel.h>

static void
on_close (GtkWidget *menuitem, gpointer user_data)
{
	GtkWidget *view;
	
	view = gtk_widget_get_toplevel (menuitem);
	
	gtk_widget_destroy (view);
}

static GnomeUIInfo mail_view_toolbar [] = {
	
	/*GNOMEUIINFO_ITEM_STOCK (N_("Save"), N_("Save this message"),
	  save_msg, GNOME_STOCK_PIXMAP_SAVE),*/
	
	GNOMEUIINFO_ITEM_STOCK (N_("Reply"), N_("Reply to the sender of this message"),
				reply_to_sender, GNOME_STOCK_PIXMAP_MAIL_RPL),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Reply to All"), N_("Reply to all recipients of this message"),
				reply_to_all, GNOME_STOCK_PIXMAP_MAIL_RPL),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Forward"), N_("Forward this message"), forward_msg, GNOME_STOCK_PIXMAP_MAIL_FWD),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK (N_("Print"), N_("Print the selected message"), print_msg, GNOME_STOCK_PIXMAP_PRINT),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Delete"), N_("Delete this message"), delete_msg, GNOME_STOCK_PIXMAP_TRASH),
	
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

GtkWidget *
mail_view_create (FolderBrowser *folder_browser)
{
	CamelMimeMessage *msg;
	GtkWidget *window;
	GtkWidget *toolbar;
	GtkWidget *mail_display;
	char *subject;
	
	msg = folder_browser->mail_display->current_message;
	
	subject = (char *) camel_mime_message_get_subject (msg);
	if (!subject)
		subject = "";
	
	window = gnome_app_new ("Evolution", subject);
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	
	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  mail_view_toolbar,
					  NULL, folder_browser);
	
	gnome_app_set_toolbar (GNOME_APP (window), GTK_TOOLBAR (toolbar));
	
	gnome_app_create_menus (GNOME_APP (window), mail_view_menubar);
	
	gtk_widget_ref (mail_view_menubar[0].widget);
	gtk_object_set_data_full (GTK_OBJECT (window), "file",
				  mail_view_menubar[0].widget,
				  (GtkDestroyNotify) gtk_widget_unref);
	
	gtk_widget_ref (file_menu[0].widget);
	gtk_object_set_data_full (GTK_OBJECT (window), "close",
				  file_menu[0].widget,
				  (GtkDestroyNotify) gtk_widget_unref);
	
	gtk_widget_ref (mail_view_menubar[1].widget);
	gtk_object_set_data_full (GTK_OBJECT (window), "view",
				  mail_view_menubar[1].widget,
				  (GtkDestroyNotify) gtk_widget_unref);
	
	mail_display = folder_browser->mail_display;
	gtk_widget_set_usize (mail_display, 600, 600);
	
	gnome_app_set_contents (GNOME_APP (window), mail_display);
	
	return window;
}
