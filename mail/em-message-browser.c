/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>

#include <gconf/gconf-client.h>

#include <camel/camel-folder.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>

#include "em-format-html-display.h"
#include "em-message-browser.h"

#include "evolution-shell-component-utils.h" /* Pixmap stuff, sigh */


#define DEFAULT_WIDTH  600
#define DEFAULT_HEIGHT 400

struct _EMMessageBrowserPrivate {
	GtkWidget *preview;	/* container for message display */
};

static void emmb_set_message(EMFolderView *emfv, const char *uid, int nomarkseen);
static void emmb_activate(EMFolderView *emfv, BonoboUIComponent *uic, int state);

static EMFolderViewClass *emmb_parent;

static void
emmb_init(GObject *o)
{
	EMMessageBrowser *emmb = (EMMessageBrowser *)o;
	struct _EMMessageBrowserPrivate *p;

	p = emmb->priv = g_malloc0(sizeof(struct _EMMessageBrowserPrivate));

	((EMFolderView *)emmb)->preview_active = TRUE;

	g_slist_free(emmb->view.ui_files);
	emmb->view.ui_files = g_slist_append(NULL, EVOLUTION_UIDIR "/evolution-mail-messagedisplay.xml");
	emmb->view.ui_files = g_slist_append(emmb->view.ui_files, EVOLUTION_UIDIR "/evolution-mail-message.xml");

	/* currently: just use a scrolledwindow for preview widget */
	p->preview = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow *)p->preview, GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)p->preview, GTK_SHADOW_IN);
	gtk_widget_show(p->preview);

	gtk_container_add((GtkContainer *)p->preview, (GtkWidget *)emmb->view.preview->formathtml.html);
	gtk_widget_show((GtkWidget *)emmb->view.preview->formathtml.html);

	gtk_widget_show(p->preview);

	gtk_box_pack_start_defaults((GtkBox *)emmb, p->preview);
}

static void
emmb_finalise(GObject *o)
{
	EMMessageBrowser *emmb = (EMMessageBrowser *)o;

	g_free(emmb->priv);
	((GObjectClass *)emmb_parent)->finalize(o);
}

static void
emmb_destroy(GtkObject *o)
{
	EMMessageBrowser *emmb = (EMMessageBrowser *)o;

	if (emmb->view.list) {
		gtk_widget_destroy((GtkWidget *)emmb->view.list);
		emmb->view.list = NULL;
	}

	((GtkObjectClass *)emmb_parent)->destroy(o);
}

static void
emmb_class_init(GObjectClass *klass)
{
	klass->finalize = emmb_finalise;
	
	((GtkObjectClass *)klass)->destroy = emmb_destroy;
	
	((EMFolderViewClass *) klass)->update_message_style = FALSE;
	
	((EMFolderViewClass *)klass)->set_message = emmb_set_message;
	((EMFolderViewClass *)klass)->activate = emmb_activate;
}

GType
em_message_browser_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMMessageBrowserClass),
			NULL, NULL,
			(GClassInitFunc)emmb_class_init,
			NULL, NULL,
			sizeof(EMMessageBrowser), 0,
			(GInstanceInitFunc)emmb_init
		};
		emmb_parent = g_type_class_ref(em_folder_view_get_type());
		type = g_type_register_static(em_folder_view_get_type(), "EMMessageBrowser", &info, 0);
	}

	return type;
}

static GtkAllocation window_size = { 0, 0, 0, 0 };

static void
window_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GConfClient *gconf;
	
	/* save to in-memory variable for current session access */
	window_size = *allocation;
	
	/* save the setting across sessions */
	gconf = gconf_client_get_default ();
	gconf_client_set_int (gconf, "/apps/evolution/mail/message_window/width", window_size.width, NULL);
	gconf_client_set_int (gconf, "/apps/evolution/mail/message_window/height", window_size.height, NULL);
	g_object_unref (gconf);
}

static void
emmb_list_message_selected (struct _MessageList *ml, const char *uid, EMMessageBrowser *emmb)
{
	EMFolderView *emfv = (EMFolderView *) emmb;
	CamelMessageInfo *info;
	
	if (uid && (info = camel_folder_get_message_info (emfv->folder, uid))) {
		gtk_window_set_title ((GtkWindow *) emmb->window, camel_message_info_subject (info));
		gtk_widget_grab_focus ((GtkWidget *) (emmb->view.preview->formathtml.html));
		camel_folder_free_message_info (emfv->folder, info);
	}
}

GtkWidget *em_message_browser_new(void)
{
	EMMessageBrowser *emmb = g_object_new(em_message_browser_get_type(), 0);

	return (GtkWidget *)emmb;
}

GtkWidget *em_message_browser_window_new(void)
{
	EMMessageBrowser *emmb;
	BonoboUIContainer *uicont;
	BonoboUIComponent *uic;

	emmb = (EMMessageBrowser *)em_message_browser_new();
	gtk_widget_show((GtkWidget *)emmb);
	/* FIXME: title set elsewhere? */
	emmb->window = g_object_new(bonobo_window_get_type(), "title", "Ximian Evolution", NULL);
	bonobo_window_set_contents((BonoboWindow *)emmb->window, (GtkWidget *)emmb);

	uicont = bonobo_window_get_ui_container((BonoboWindow *)emmb->window);
	uic = bonobo_ui_component_new_default();
	bonobo_ui_component_set_container(uic, BONOBO_OBJREF(uicont), NULL);

	em_folder_view_activate((EMFolderView *)emmb, uic, TRUE);
	
	if (window_size.width == 0) {
		/* initialize @window_size with the previous session's size */
		GConfClient *gconf;
		GError *err = NULL;
		
		gconf = gconf_client_get_default ();
		
		window_size.width = gconf_client_get_int (gconf, "/apps/evolution/mail/message_window/width", &err);
		if (err != NULL) {
			window_size.width = DEFAULT_WIDTH;
			g_clear_error (&err);
		}
		
		window_size.height = gconf_client_get_int (gconf, "/apps/evolution/mail/message_window/height", &err);
		if (err != NULL) {
			window_size.height = DEFAULT_HEIGHT;
			g_clear_error (&err);
		}
		
		g_object_unref (gconf);
	}
	
	gtk_window_set_default_size ((GtkWindow *) emmb->window, window_size.width, window_size.height);
	g_signal_connect (emmb->window, "size-allocate", G_CALLBACK (window_size_allocate), NULL);
	g_signal_connect (((EMFolderView *) emmb)->list, "message_selected", G_CALLBACK (emmb_list_message_selected), emmb);
	
	/* cleanup? */

	return (GtkWidget *)emmb;
}

/* ********************************************************************** */

static void
emmb_set_message(EMFolderView *emfv, const char *uid, int nomarkseen)
{
	EMMessageBrowser *emmb = (EMMessageBrowser *) emfv;
	CamelMessageInfo *info;
	
	emmb_parent->set_message(emfv, uid, nomarkseen);
	
	if (uid == NULL)
		return;
	
	if ((info = camel_folder_get_message_info (emfv->folder, uid))) {
		gtk_window_set_title ((GtkWindow *) emmb->window, camel_message_info_subject (info));
		camel_folder_free_message_info (emfv->folder, info);
	}
	
	/* Well we don't know if it got displayed (yet) ... but whatever ... */
	if (!nomarkseen)
		camel_folder_set_message_flags(emfv->folder, uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
}

static void
emmb_close(BonoboUIComponent *uid, void *data, const char *path)
{
	EMMessageBrowser *emmb = data;

	gtk_widget_destroy(gtk_widget_get_toplevel((GtkWidget *)emmb));
}

static BonoboUIVerb emmb_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("MessageBrowserClose", emmb_close),
	BONOBO_UI_VERB_END
};

static void
emmb_activate(EMFolderView *emfv, BonoboUIComponent *uic, int state)
{
	if (state) {
		emmb_parent->activate(emfv, uic, state);

		bonobo_ui_component_add_verb_list_with_data(uic, emmb_verbs, emfv);
		bonobo_ui_component_set_prop(uic, "/commands/EditPaste", "sensitive", "0", NULL);
	} else {
		const BonoboUIVerb *v;
		
		for (v = &emmb_verbs[0]; v->cname; v++)
			bonobo_ui_component_remove_verb(uic, v->cname);

		emmb_parent->activate(emfv, uic, state);
	}
}
