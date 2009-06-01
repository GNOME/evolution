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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <gdk/gdkkeysyms.h>

#include <gconf/gconf-client.h>

#include <camel/camel-folder.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>

#include "e-util/e-util-private.h"

#include "e-mail-search-bar.h"
#include "em-format-html-display.h"
#include "em-message-browser.h"
#include "em-menu.h"

#include "evolution-shell-component-utils.h" /* Pixmap stuff, sigh */

#define EM_MESSAGE_BROWSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_MESSAGE_BROWSER, EMMessageBrowserPrivate))

#define DEFAULT_WIDTH  600
#define DEFAULT_HEIGHT 400

struct _EMMessageBrowserPrivate {
	GtkWidget *preview;	/* container for message display */
	GtkWidget *search_bar;
};

static gpointer parent_class;
static GtkAllocation window_size = { 0, 0, 0, 0 };

static void
emmb_close (BonoboUIComponent *uid,
            gpointer data,
            const gchar *path)
{
	EMMessageBrowser *emmb = data;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (emmb));
	gtk_widget_destroy (toplevel);
}

static BonoboUIVerb emmb_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("MessageBrowserClose", emmb_close),
	BONOBO_UI_VERB_END
};

static void
emmb_set_message (EMFolderView *emfv,
                  const gchar *uid,
                  gint nomarkseen)
{
	EMMessageBrowser *emmb = EM_MESSAGE_BROWSER (emfv);
	EMFolderViewClass *folder_view_class;
	CamelMessageInfo *info;

	/* Chain up to parent's set_message() method. */
	folder_view_class = EM_FOLDER_VIEW_CLASS (parent_class);
	folder_view_class->set_message (emfv, uid, nomarkseen);

	if (uid == NULL) {
		gtk_widget_destroy (GTK_WIDGET (emfv));
		return;
	}

	info = camel_folder_get_message_info (emfv->folder, uid);

	if (info != NULL) {
		gtk_window_set_title (
			GTK_WINDOW (emmb->window),
			camel_message_info_subject (info));
		camel_folder_free_message_info (emfv->folder, info);
	}

	/* Well we don't know if it got displayed (yet) ... but whatever ... */
	if (!nomarkseen)
		camel_folder_set_message_flags (
			emfv->folder, uid,
			CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
}

static void
emmb_activate (EMFolderView *emfv,
               BonoboUIComponent *uic,
               gint state)
{
	EMFolderViewClass *folder_view_class;

	folder_view_class = EM_FOLDER_VIEW_CLASS (parent_class);

	if (state) {
		/* Chain up to parent's activate() method. */
		folder_view_class->activate (emfv, uic, state);

		bonobo_ui_component_add_verb_list_with_data (
			uic, emmb_verbs, emfv);
		bonobo_ui_component_set_prop(
			uic, "/commands/EditPaste", "sensitive", "0", NULL);
	} else {
		const BonoboUIVerb *verb;

		for (verb = &emmb_verbs[0]; verb->cname; verb++)
			bonobo_ui_component_remove_verb (uic, verb->cname);

		/* Chain up to parent's activate() method. */
		folder_view_class->activate (emfv, uic, state);
	}
}

static void
emmb_list_message_selected_cb (struct _MessageList *ml,
                               const gchar *uid,
                               EMMessageBrowser *emmb)
{
	EMFolderView *emfv = EM_FOLDER_VIEW (emmb);
	CamelMessageInfo *info;

	if (uid == NULL)
		return;

	info = camel_folder_get_message_info (emfv->folder, uid);
	if (info == NULL)
		return;

	gtk_window_set_title (
		GTK_WINDOW (emmb->window),
		camel_message_info_subject (info));
	gtk_widget_grab_focus (
		GTK_WIDGET (emfv->preview->formathtml.html));

	camel_folder_free_message_info (emfv->folder, info);
}

static void
emmb_window_size_allocate (GtkWidget *widget,
                           GtkAllocation *allocation)
{
	GConfClient *client;

	/* FIXME Have GConfBridge handle this. */

	/* save to in-memory variable for current session access */
	window_size = *allocation;

	/* save the setting across sessions */
	client = gconf_client_get_default ();
	gconf_client_set_int (
		client, "/apps/evolution/mail/message_window/width",
		window_size.width, NULL);
	gconf_client_set_int (
		client, "/apps/evolution/mail/message_window/height",
		window_size.height, NULL);
	g_object_unref (client);
}

static gint
emmb_key_press_event_cb (EMMessageBrowser *emmb,
                         GdkEventKey *event)
{
	if (event->keyval == GDK_Escape) {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (emmb));
		gtk_widget_destroy (toplevel);
		g_signal_stop_emission_by_name (emmb, "key-press-event");

		return TRUE;
	}

	return FALSE;
}

static void
emmb_destroy (GtkObject *gtk_object)
{
	EMFolderView *emfv = EM_FOLDER_VIEW (gtk_object);

	if (emfv->list) {
		gtk_widget_destroy (GTK_WIDGET (emfv->list));
		emfv->list = NULL;
	}

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (parent_class)->destroy (gtk_object);
}

static void
emmb_show_search_bar (EMFolderView *folder_view)
{
	EMMessageBrowser *browser = EM_MESSAGE_BROWSER (folder_view);

	gtk_widget_show (browser->priv->search_bar);
}

static void
emmb_class_init (EMMessageBrowserClass *class)
{
	GtkObjectClass *gtk_object_class;
	EMFolderViewClass *folder_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMMessageBrowserPrivate));

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = emmb_destroy;

	folder_view_class = EM_FOLDER_VIEW_CLASS (class);
	folder_view_class->update_message_style = FALSE;
	folder_view_class->set_message = emmb_set_message;
	folder_view_class->activate = emmb_activate;
	folder_view_class->show_search_bar = emmb_show_search_bar;
}

static void
emmb_init (EMMessageBrowser *emmb)
{
	EMFolderView *emfv = EM_FOLDER_VIEW (emmb);
	GtkWidget *container;
	GtkWidget *widget;
	gchar *filename;

	emmb->priv = EM_MESSAGE_BROWSER_GET_PRIVATE (emmb);

	emfv->preview_active = TRUE;

	g_slist_foreach (emfv->ui_files, (GFunc) g_free, NULL);
	g_slist_free (emfv->ui_files);
	emfv->ui_files = NULL;

	filename = g_build_filename (
		EVOLUTION_UIDIR, "evolution-mail-messagedisplay.xml", NULL);
	emfv->ui_files = g_slist_append (emfv->ui_files, filename);

	filename = g_build_filename (
		EVOLUTION_UIDIR, "evolution-mail-message.xml", NULL);
	emfv->ui_files = g_slist_append (emfv->ui_files, filename);

	gtk_box_set_spacing (GTK_BOX (emmb), 1);

	container = GTK_WIDGET (emmb);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	emmb->priv->preview = widget;
	gtk_widget_show (widget);

	widget = e_mail_search_bar_new (emfv->preview->formathtml.html);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	emmb->priv->search_bar = widget;
	gtk_widget_hide (widget);

	g_signal_connect_swapped (
		widget, "changed",
		G_CALLBACK (em_format_redraw), emfv->preview);

	container = emmb->priv->preview;

	widget = GTK_WIDGET (emfv->preview->formathtml.html);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	/** @HookPoint-EMMenu: Standalone Message View Menu
	 * @Id: org.gnome.evolution.mail.messagebrowser
	 * @Class: org.gnome.evolution.mail.bonobomenu:1.0
	 * @Target: EMMenuTargetSelect
	 *
	 * The main menu of standalone message viewer.
	 */
	EM_FOLDER_VIEW (emmb)->menu =
		em_menu_new ("org.gnome.evolution.mail.messagebrowser");
}

GType
em_message_browser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMMessageBrowserClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) emmb_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMMessageBrowser),
			0,     /* n_preallocs */
			(GInstanceInitFunc) emmb_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			EM_TYPE_FOLDER_VIEW, "EMMessageBrowser",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
em_message_browser_new (void)
{
	return g_object_new (EM_TYPE_MESSAGE_BROWSER, NULL);
}

GtkWidget *
em_message_browser_window_new (void)
{
	EMMessageBrowser *emmb;
	BonoboUIContainer *uicont;
	BonoboUIComponent *uic;

	emmb = (EMMessageBrowser *) em_message_browser_new ();
	gtk_widget_show (GTK_WIDGET (emmb));

	/* FIXME: title set elsewhere? */
	emmb->window = g_object_new (
		BONOBO_TYPE_WINDOW, "title", "Evolution", NULL);
	bonobo_window_set_contents (
		BONOBO_WINDOW (emmb->window), GTK_WIDGET (emmb));

	uic = bonobo_ui_component_new_default ();
	uicont = bonobo_window_get_ui_container (BONOBO_WINDOW (emmb->window));
	bonobo_ui_component_set_container (uic, BONOBO_OBJREF (uicont), NULL);

	em_folder_view_activate (EM_FOLDER_VIEW (emmb), uic, TRUE);

	if (window_size.width == 0) {
		/* initialize @window_size with the previous session's size */

		/* FIXME Have GConfBridge handle this. */

		GConfClient *client;
		GError *error = NULL;

		client = gconf_client_get_default ();

		window_size.width = gconf_client_get_int (
			client, "/apps/evolution/mail/message_window/width",
			&error);
		if (error != NULL) {
			window_size.width = DEFAULT_WIDTH;
			g_clear_error (&error);
		}

		window_size.height = gconf_client_get_int (
			client, "/apps/evolution/mail/message_window/height",
			&error);
		if (error != NULL) {
			window_size.height = DEFAULT_HEIGHT;
			g_clear_error (&error);
		}

		g_object_unref (client);
	}

	gtk_window_set_default_size (
		GTK_WINDOW (emmb->window),
		window_size.width, window_size.height);

	g_signal_connect (
		emmb->window, "size-allocate",
		G_CALLBACK (emmb_window_size_allocate), NULL);
	g_signal_connect (
		EM_FOLDER_VIEW (emmb)->list, "message_selected",
		G_CALLBACK (emmb_list_message_selected_cb), emmb);
	g_signal_connect (
		emmb, "key-press-event",
		G_CALLBACK (emmb_key_press_event_cb), NULL);

	/* cleanup? */

	return GTK_WIDGET (emmb);
}
