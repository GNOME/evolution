/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gal/util/e-util.h>

#include "message-browser.h"

#include "mail.h"
#include "mail-callbacks.h"
#include "mail-tools.h"
#include "message-list.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-mlist-magic.h"
#include "mail-mt.h"

#include "mail-local.h"
#include "mail-config.h"

#define d(x) x

#define MINIMUM_WIDTH  600
#define MINIMUM_HEIGHT 400

#define PARENT_TYPE (gnome_app_get_type ())

/* Size of the window last time it was changed.  */
static GtkAllocation last_allocation = { 0, 0 };

static GnomeAppClass *message_browser_parent_class;

static void
message_browser_destroy (GtkObject *object)
{
	MessageBrowser *message_browser;
	
	message_browser = MESSAGE_BROWSER (object);
	
	gtk_object_unref (GTK_OBJECT (message_browser->fb));
	
	gtk_widget_destroy (GTK_WIDGET (message_browser));
}

static void
message_browser_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = message_browser_destroy;
	
	message_browser_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
message_browser_init (GtkObject *object)
{
	
}

static void
message_browser_close (GtkWidget *menuitem, gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (user_data));
}

static void
message_browser_reply_to_sender (GtkWidget *widget, gpointer user_data)
{
	MessageBrowser *mb = MESSAGE_BROWSER (user_data);
	
	reply_to_sender (NULL, mb->fb);
}

static void
message_browser_reply_to_all (GtkWidget *widget, gpointer user_data)
{
	MessageBrowser *mb = MESSAGE_BROWSER (user_data);
	
	reply_to_all (NULL, mb->fb);
}

static void
message_browser_forward_msg (GtkWidget *widget, gpointer user_data)
{
	MessageBrowser *mb = MESSAGE_BROWSER (user_data);
	
	forward_attached (NULL, mb->fb);
}

static void
message_browser_print_msg (GtkWidget *widget, gpointer user_data)
{
	MessageBrowser *mb = MESSAGE_BROWSER (user_data);
	
	mail_print_msg (mb->fb->mail_display);
}

static void
message_browser_delete_msg (GtkWidget *button, gpointer user_data)
{
	MessageBrowser *mb = MESSAGE_BROWSER (user_data);
	
	delete_msg (NULL, mb->fb);
}

static void
message_browser_next_msg (GtkWidget *button, gpointer user_data)
{
	MessageBrowser *mb = MESSAGE_BROWSER (user_data);
	
	next_msg (NULL, mb->fb);
}

static void
message_browser_prev_msg (GtkWidget *button, gpointer user_data)
{
	MessageBrowser *mb = MESSAGE_BROWSER (user_data);
	
	previous_msg (NULL, mb->fb);
}

static void
message_browser_message_selected (MessageList *ml, const char *uid, MessageBrowser *mb)
{
	CamelMimeMessage *message;
	char *subject = NULL;
	
	g_warning ("got 'message_selected' event");
	
	message = mb->fb->mail_display->current_message;
	
	if (message)
		subject = (char *) camel_mime_message_get_subject (message);
	
	gtk_window_set_title (GTK_WINDOW (mb), subject ? subject : "");
}

static void
message_browser_folder_loaded (FolderBrowser *fb, const char *uri, MessageBrowser *mb)
{
	const char *uid = gtk_object_get_data (GTK_OBJECT (mb), "uid");
	
	g_warning ("got 'folder_loaded' event");
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list), "message_selected",
			    message_browser_message_selected, mb);
	
	message_list_select_uid (fb->message_list, uid);
}

static GnomeUIInfo message_browser_toolbar [] = {
	
	/*GNOMEUIINFO_ITEM_STOCK (N_("Save"), N_("Save this message"),
	  save_msg, GNOME_STOCK_PIXMAP_SAVE),*/
	
	GNOMEUIINFO_ITEM_STOCK (N_("Reply"), N_("Reply to the sender of this message"),
				message_browser_reply_to_sender, GNOME_STOCK_PIXMAP_MAIL_RPL),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Reply to All"), N_("Reply to all recipients of this message"),
				message_browser_reply_to_all, GNOME_STOCK_PIXMAP_MAIL_RPL),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Forward"), N_("Forward this message"),
				message_browser_forward_msg, GNOME_STOCK_PIXMAP_MAIL_FWD),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK (N_("Print"), N_("Print the selected message"),
				message_browser_print_msg, GNOME_STOCK_PIXMAP_PRINT),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Delete"), N_("Delete this message"),
				message_browser_delete_msg, GNOME_STOCK_PIXMAP_TRASH),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK (N_("Previous"), N_("Previous message"),
				message_browser_prev_msg, GNOME_STOCK_PIXMAP_BACK),
	
	GNOMEUIINFO_ITEM_STOCK (N_("Next"), N_("Next message"),
				message_browser_next_msg, GNOME_STOCK_PIXMAP_FORWARD),
	
	GNOMEUIINFO_END
};

static GnomeUIInfo file_menu[] = {
	/*GNOMEUIINFO_MENU_SAVE_ITEM (save, NULL),*/
	/*GNOMEUIINFO_MENU_SAVE_AS_ITEM (save_as, NULL),*/
	/*GNOMEUIINFO_SEPARATOR,*/
	GNOMEUIINFO_MENU_CLOSE_ITEM (message_browser_close, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo view_menu[] =
{
	GNOMEUIINFO_END
};

static GnomeUIInfo message_browser_menubar[] =
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
message_browser_new (const GNOME_Evolution_Shell shell, const char *uri, const char *uid)
{
	GtkWidget *toolbar, *vbox;
	MessageBrowser *new;
	FolderBrowser *fb;
	
	new = gtk_type_new (message_browser_get_type ());
	
	gnome_app_construct (GNOME_APP (new), "Evolution", "");
	
	gtk_object_set_data_full (GTK_OBJECT (new), "uid", g_strdup (uid), g_free);
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar),
					  message_browser_toolbar,
					  NULL, new);
	
	gnome_app_set_toolbar (GNOME_APP (new), GTK_TOOLBAR (toolbar));
	gnome_app_create_menus (GNOME_APP (new), message_browser_menubar);
	
	fb = FOLDER_BROWSER (folder_browser_new (shell));
	new->fb = fb;
	
	/* some evil hackery action... */
	vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_ref (GTK_WIDGET (fb->mail_display));
	gtk_widget_reparent (GTK_WIDGET (fb->mail_display), vbox);
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (vbox);
	
	gnome_app_set_contents (GNOME_APP (new), vbox);
	gtk_widget_grab_focus (GTK_WIDGET (MAIL_DISPLAY (fb->mail_display)->html));
	
	set_default_size (GTK_WIDGET (new));
	
	/* more evil hackery... */
	gtk_signal_connect (GTK_OBJECT (fb), "folder_loaded",
			    message_browser_folder_loaded, new);
	
	folder_browser_set_uri (fb, uri);
	
	return GTK_WIDGET (new);
}


E_MAKE_TYPE (message_browser, "MessageBrowser", MessageBrowser, message_browser_class_init,
	     message_browser_init, PARENT_TYPE);
