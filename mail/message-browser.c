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

#define d(x) x

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
	
	gtk_object_unref (GTK_OBJECT (message_browser->fb));
	
	gtk_widget_destroy (GTK_WIDGET (message_browser));

	if (GTK_OBJECT_CLASS (message_browser_parent_class)->destroy)
		(GTK_OBJECT_CLASS (message_browser_parent_class)->destroy) (object);
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

/* UI callbacks */

static void
message_browser_close (BonoboUIComponent *uih, void *user_data, const char *path)
{
	gtk_widget_destroy (GTK_WIDGET (user_data));
}

static BonoboUIVerb 
browser_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("MessageBrowserClose", message_browser_close),
	BONOBO_UI_VERB_END
};

/* FB message loading hookups */

static void
message_browser_message_loaded (FolderBrowser *fb, const char *uid, MessageBrowser *mb)
{
	CamelMimeMessage *message;
	char *subject = NULL;
	
	g_warning ("got 'message_loaded' event");
	
	message = fb->mail_display->current_message;
	
	if (message)
		subject = (char *) camel_mime_message_get_subject (message);
	
	gtk_window_set_title (GTK_WINDOW (mb), subject ? subject : "");
}

static void
message_browser_message_list_built (MessageList *ml, MessageBrowser *mb)
{
	const char *uid = gtk_object_get_data (GTK_OBJECT (mb), "uid");
	
	g_warning ("got 'message_list_built' event");
	
	message_list_select_uid (ml, uid);
}

static void
message_browser_folder_loaded (FolderBrowser *fb, const char *uri, MessageBrowser *mb)
{
	g_warning ("got 'folder_loaded' event for '%s'", uri);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list), "message_list_built",
			    message_browser_message_list_built, mb);
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

	uicont = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (uicont, BONOBO_WINDOW (widget));

	uic = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (uic, BONOBO_OBJREF (uicont));
	folder_browser_set_ui_component (fb, uic);

	/* Load our UI */

	bonobo_ui_component_freeze (uic, NULL);
	bonobo_ui_util_set_ui (uic, EVOLUTION_DATADIR, "evolution-mail-messagedisplay.xml", "evolution-mail");

	/* Load the appropriate UI stuff from the folder browser */

	folder_browser_ui_add_message (fb);

	/* We just opened the message! We don't need to open it again. */

	CORBA_exception_init (&ev);
	bonobo_ui_component_rm (uic, "/menu/File/FileOps/MessageOpen", &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Couldn't remove message open item. Weird. Error: %s",
			   bonobo_exception_get_text (&ev));
	CORBA_exception_free (&ev);

	/* Customize Toolbar thingie */
	
	bonobo_ui_engine_config_set_path (bonobo_window_get_ui_engine (BONOBO_WINDOW (widget)),
					  "/evolution/UIConf/messagebrowser");

	/* Add the Close item */

	bonobo_ui_component_add_verb_list_with_data (uic, browser_verbs, widget);

	/* Done */

	bonobo_ui_component_thaw (uic, NULL);

}

GtkWidget *
message_browser_new (const GNOME_Evolution_Shell shell, const char *uri, const char *uid)
{
	GtkWidget *vbox;
	MessageBrowser *new;
	FolderBrowser *fb;
	
	new = gtk_type_new (MESSAGE_BROWSER_TYPE);
	new = (MessageBrowser *) bonobo_window_construct (BONOBO_WINDOW (new), "Evolution", "");
	if (!new) {
		g_warning ("Failed to construct Bonobo window!");
		return NULL;
	}

	gtk_object_set_data_full (GTK_OBJECT (new), "uid", g_strdup (uid), g_free);

	fb = FOLDER_BROWSER (folder_browser_new (shell));
	new->fb = fb;

	set_bonobo_ui (GTK_WIDGET (new), fb);

	/* some evil hackery action... */
	vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_ref (GTK_WIDGET (fb->mail_display));
	gtk_widget_reparent (GTK_WIDGET (fb->mail_display), vbox);
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (vbox);

	gtk_signal_connect(GTK_OBJECT(new), "size_allocate", 
			   GTK_SIGNAL_FUNC(message_browser_size_allocate_cb), NULL);
	
	bonobo_window_set_contents (BONOBO_WINDOW (new), vbox);
	gtk_widget_grab_focus (GTK_WIDGET (MAIL_DISPLAY (fb->mail_display)->html));
	
	set_default_size (GTK_WIDGET (new));
	
	/* more evil hackery... */
	gtk_signal_connect (GTK_OBJECT (fb), "folder_loaded",
			    message_browser_folder_loaded, new);
	
	gtk_signal_connect (GTK_OBJECT (fb), "message_loaded",
			    message_browser_message_loaded, new);
	
	folder_browser_set_uri (fb, uri);
	
	return GTK_WIDGET (new);
}

/* Fin */

E_MAKE_TYPE (message_browser, "MessageBrowser", MessageBrowser, message_browser_class_init,
	     message_browser_init, PARENT_TYPE);
