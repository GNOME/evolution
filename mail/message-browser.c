/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <gal/util/e-util.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-util.h>

#include "message-browser.h"

#include "mail.h"
#include "mail-callbacks.h"
#include "mail-tools.h"
#include "message-list.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-mt.h"

#include "mail-local.h"
#include "mail-config.h"

#include "folder-browser-ui.h"

#define d(x) 

#define MINIMUM_WIDTH  600
#define MINIMUM_HEIGHT 400

#define PARENT_TYPE BONOBO_TYPE_WINDOW

/* Size of the window last time it was changed.  */
static GtkAllocation last_allocation = { 0, 0 };

static BonoboWindowClass *message_browser_parent_class;

static void
message_browser_destroy (GtkObject *object)
{
	MessageBrowser *message_browser;
	
	message_browser = MESSAGE_BROWSER (object);
	
	if (message_browser->ml_built_id) {
		g_signal_handler_disconnect (message_browser->fb->message_list, message_browser->ml_built_id);
		message_browser->ml_built_id = 0;
	}
	
	if (message_browser->loaded_id) {
		g_signal_handler_disconnect (message_browser->fb, message_browser->loaded_id);
		message_browser->loaded_id = 0;
	}
	
	if (message_browser->fb) {
		g_object_unref (message_browser->fb);
		message_browser->fb = NULL;
	}
	
	if (GTK_OBJECT_CLASS (message_browser_parent_class)->destroy)
		(GTK_OBJECT_CLASS (message_browser_parent_class)->destroy) (object);
}

static void
message_browser_class_init (GObjectClass *object_class)
{
	((GtkObjectClass *)object_class)->destroy = message_browser_destroy;
	
	message_browser_parent_class = g_type_class_ref (PARENT_TYPE);
}

static void
message_browser_init (GtkObject *object)
{
	
}

static void
transfer_msg_done (gboolean ok, void *data)
{
	MessageBrowser *mb = data;
	
	gtk_widget_destroy ((GtkWidget *) mb);
	
	g_object_unref (mb);
}

static void
transfer_msg (MessageBrowser *mb, int del)
{
	const char *allowed_types[] = { "mail/*", "vtrash", NULL };
	extern EvolutionShellClient *global_shell_client;
	GNOME_Evolution_Folder *folder;
	static char *last_uri = NULL;
	GPtrArray *uids;
	char *desc;
	
/*	if (GTK_OBJECT_DESTROYED(mb))
	return;*/
	
	if (last_uri == NULL)
		last_uri = g_strdup ("");
	
	if (del)
		desc = _("Move message(s) to");
	else
		desc = _("Copy message(s) to");
	
	evolution_shell_client_user_select_folder (global_shell_client,
						   GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (mb))),
						   desc, last_uri, allowed_types, &folder);
	if (!folder)
		return;
	
	if (strcmp (last_uri, folder->evolutionUri) != 0) {
		g_free (last_uri);
		last_uri = g_strdup (folder->evolutionUri);
	}
	
	uids = g_ptr_array_new ();
	message_list_foreach (mb->fb->message_list, enumerate_msg, uids);
	
	if (del) {
		g_object_ref (mb);
		mail_transfer_messages (mb->fb->folder, uids, del,
					folder->physicalUri, 0, transfer_msg_done, mb);
	} else {
		mail_transfer_messages (mb->fb->folder, uids, del,
					folder->physicalUri, 0, NULL, NULL);
	}
	
	CORBA_free (folder);
}


/* UI callbacks */

static void
message_browser_close (BonoboUIComponent *uih, void *user_data, const char *path)
{
	gtk_widget_destroy (GTK_WIDGET (user_data));
}

static void
message_browser_move (BonoboUIComponent *uih, void *user_data, const char *path)
{
	transfer_msg (user_data, TRUE);
}

static void
message_browser_copy (BonoboUIComponent *uih, void *user_data, const char *path)
{
	transfer_msg (user_data, FALSE);
}

static void
message_browser_delete (BonoboUIComponent *uih, void *user_data, const char *path)
{
	MessageBrowser *mb = user_data;
	GPtrArray *uids;
	int i;
	
	uids = g_ptr_array_new ();
	message_list_foreach (mb->fb->message_list, enumerate_msg, uids);
	camel_folder_freeze (mb->fb->folder);
	for (i = 0; i < uids->len; i++) {
		camel_folder_set_message_flags (mb->fb->folder, uids->pdata[i],
						CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
						CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);
		g_free (uids->pdata[i]);
	}
	
	camel_folder_thaw (mb->fb->folder);
	
	g_ptr_array_free (uids, TRUE);
	
	gtk_widget_destroy ((GtkWidget *) mb);
}

static BonoboUIVerb 
browser_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("MessageBrowserClose", message_browser_close),
	BONOBO_UI_UNSAFE_VERB ("MessageMove", message_browser_move),
	BONOBO_UI_UNSAFE_VERB ("MessageCopy", message_browser_copy),
	BONOBO_UI_UNSAFE_VERB ("MessageDelete", message_browser_delete),
	BONOBO_UI_VERB_END
};

/* FB message loading hookups */

static void
message_browser_message_loaded (FolderBrowser *fb, const char *uid, MessageBrowser *mb)
{
	CamelMimeMessage *message;
	char *subject = NULL;
	char *title;
	
	folder_browser_ui_message_loaded(fb);
	
	message = fb->mail_display->current_message;
	
	if (message)
		subject = (char *) camel_mime_message_get_subject (message);
	
	if (subject == NULL)
		subject = _("(No subject)");
	
	title = g_strdup_printf (_("%s - Message"), subject);
	
	gtk_window_set_title (GTK_WINDOW (mb), title);
	
	g_free (title);
}

static void
message_browser_message_list_built (MessageList *ml, MessageBrowser *mb)
{
	const char *uid = g_object_get_data (G_OBJECT (mb), "uid");
	
	g_signal_handler_disconnect (ml, mb->ml_built_id);
	mb->ml_built_id = 0;
	
	message_list_select_uid (ml, uid);
}

static void
message_browser_folder_loaded (FolderBrowser *fb, const char *uri, MessageBrowser *mb)
{
	g_signal_handler_disconnect (fb, mb->loaded_id);
	mb->loaded_id = 0;
	
	mb->ml_built_id = g_signal_connect (fb->message_list, "message_list_built",
					    G_CALLBACK (message_browser_message_list_built), mb);
}

static void
message_browser_size_allocate_cb (GtkWidget *widget,
				  GtkAllocation *allocation)
{
	last_allocation = *allocation;
}

/* Construction */

static void
set_default_size (GtkWidget *widget)
{
	int width, height;
	
	width  = MAX (MINIMUM_WIDTH, last_allocation.width);
	height = MAX (MINIMUM_HEIGHT, last_allocation.height);
	
	gtk_window_set_default_size (GTK_WINDOW (widget), width, height);
}

static void 
set_bonobo_ui (GtkWidget *widget, FolderBrowser *fb)
{
	BonoboUIContainer *uicont;
	BonoboUIComponent *uic;
	CORBA_Environment ev;
	
	uicont = bonobo_window_get_ui_container (BONOBO_WINDOW (widget));
	
	uic = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (uic, BONOBO_OBJREF (uicont), NULL);
	folder_browser_set_ui_component (fb, uic);
	
	/* Load our UI */
	
	/*bonobo_ui_component_freeze (uic, NULL);*/
	bonobo_ui_util_set_ui (uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-mail-messagedisplay.xml",
			       "evolution-mail", NULL);
	
	/* Load the appropriate UI stuff from the folder browser */
	
	folder_browser_ui_add_message (fb);
	
	/* We just opened the message! We don't need to open it again. */
	
	CORBA_exception_init (&ev);
	/* remove the broken menus and toolbar items */
	bonobo_ui_component_rm (uic, "/menu/File/FileOps/MessageOpen", &ev);
	bonobo_ui_component_rm (uic, "/menu/Actions/ComponentActionsPlaceholder/MailMessageActions/GoTo", &ev);
	bonobo_ui_component_rm (uic, "/menu/Tools", &ev);
	bonobo_ui_component_rm (uic, "/Toolbar/MailNextButtons", &ev);
	CORBA_exception_free (&ev);
	
	/* Hack around the move/copy/delete commands api's */
	bonobo_ui_component_remove_listener (uic, "MessageCopy");
	bonobo_ui_component_remove_listener (uic, "MessageMove");
	bonobo_ui_component_remove_listener (uic, "MessageDelete");
	
	/* Add the Close & Move/Copy/Delete items */
	
	bonobo_ui_component_add_verb_list_with_data (uic, browser_verbs, widget);
	
	/* Done */
	
	/*bonobo_ui_component_thaw (uic, NULL);*/
}

static int
on_key_press (GtkWidget *widget, GdkEventKey *key, gpointer data)
{
	MessageBrowser *mb = data;
	
	if (key->state & GDK_CONTROL_MASK)
		return FALSE;
	
	switch (key->keyval) {
	case GDK_Delete:
	case GDK_KP_Delete:
		message_browser_delete (NULL, mb, NULL);
		return TRUE;
	case GDK_Escape:
		message_browser_close (NULL, mb, NULL);
		return TRUE;	
	default:
	}
	
	return FALSE;
}

GtkWidget *
message_browser_new (const GNOME_Evolution_Shell shell, const char *uri, const char *uid)
{
	GtkWidget *vbox;
	MessageBrowser *new;
	FolderBrowser *fb;
	
	new = g_object_new (MESSAGE_BROWSER_TYPE, "title", "Ximian Evolution", NULL);
	
	g_object_set_data_full (G_OBJECT (new), "uid", g_strdup (uid), g_free);
	
	fb = FOLDER_BROWSER (folder_browser_new (shell, uri));
	g_object_ref (fb);
	gtk_object_sink ((GtkObject *) fb);
	
	new->fb = fb;
	
	set_bonobo_ui (GTK_WIDGET (new), fb);
	
	/* some evil hackery action... */
	vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_ref (GTK_WIDGET (fb->mail_display));
	gtk_widget_reparent (GTK_WIDGET (fb->mail_display), vbox);
	/* Note: normally we'd unref the fb->mail_display now, except
           that if we do then our refcounts will not be in
           harmony... both the fb *and* the message-browser need to
           own a ref on the mail_display. */
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (vbox);
	
	g_signal_connect (new, "size-allocate", G_CALLBACK (message_browser_size_allocate_cb), NULL);
	
	bonobo_window_set_contents (BONOBO_WINDOW (new), vbox);
	gtk_widget_grab_focus (GTK_WIDGET (MAIL_DISPLAY (fb->mail_display)->html));
	
	set_default_size (GTK_WIDGET (new));
	
	/* more evil hackery... */
	new->loaded_id = g_signal_connect (fb, "folder_loaded", G_CALLBACK (message_browser_folder_loaded), new);
	g_signal_connect (fb, "message_loaded", G_CALLBACK (message_browser_message_loaded), new);
	
	g_signal_connect (new, "key_press_event", G_CALLBACK (on_key_press), new);
	
	return GTK_WIDGET (new);
}

/* Fin */

E_MAKE_TYPE (message_browser, "MessageBrowser", MessageBrowser, message_browser_class_init,
	     message_browser_init, PARENT_TYPE);
