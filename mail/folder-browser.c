/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser.c: Folder browser top level component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "e-util/e-util.h"
#include "camel/camel-exception.h"
#include "folder-browser.h"
#include "session.h"
#include "message-list.h"


#define PARENT_TYPE (gtk_table_get_type ())

static GtkObjectClass *folder_browser_parent_class;


#define PROPERTY_FOLDER_URI      "folder_uri"
#define PROPERTY_MESSAGE_PREVIEW "message_preview"




static void
folder_browser_destroy (GtkObject *object)
{
	FolderBrowser *folder_browser = FOLDER_BROWSER (object);

	if (folder_browser->uri)
		g_free (folder_browser->uri);

	if (folder_browser->folder)
		gtk_object_unref (GTK_OBJECT (folder_browser->folder));
	
	if (folder_browser->message_list)
		bonobo_object_unref (BONOBO_OBJECT (folder_browser->message_list));

	folder_browser_parent_class->destroy (object);
}

static void
folder_browser_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = folder_browser_destroy;

	folder_browser_parent_class = gtk_type_class (PARENT_TYPE);
}

static gboolean
folder_browser_load_folder (FolderBrowser *fb, const char *name)
{
	CamelFolder *new_folder;
	CamelException ex;
	gboolean new_folder_exists = FALSE;

	camel_exception_init (&ex);
	new_folder = camel_store_get_folder (default_session->store, name, &ex);

	if (camel_exception_get_id (&ex)){
		printf ("Unable to get folder %s : %s\n",
			name,
			ex.desc?ex.desc:"unknown reason");
		return FALSE;
	}
	
	/* if the folder does not exist, we don't want to show it */
	new_folder_exists = camel_folder_exists (new_folder, &ex);	
	if (camel_exception_get_id (&ex)) {
	      printf ("Unable to test for folder existence: %s\n",
		      ex.desc?ex.desc:"unknown reason");
	      return FALSE;
	}
		
	if (!new_folder_exists) {
		gtk_object_unref (GTK_OBJECT (new_folder));
		return FALSE;
	}

	
	if (fb->folder)
		gtk_object_unref (GTK_OBJECT (fb->folder));
	
	fb->folder = new_folder;
	
	message_list_set_folder (fb->message_list, new_folder);

	return TRUE;
}

#define EQUAL(a,b) (strcmp (a,b) == 0)

void
folder_browser_set_uri (FolderBrowser *folder_browser, const char *uri)
{
	/* FIXME: hardcoded uri */
	if (!folder_browser_load_folder (folder_browser, "inbox"))
		return;
	
	if (folder_browser->uri)
		g_free (folder_browser->uri);

	folder_browser->uri = g_strdup (uri);
}

void
folder_browser_set_message_preview (FolderBrowser *folder_browser, gboolean show_message_preview)
{
	if (folder_browser->preview_shown == show_message_preview)
		return;

	g_warning ("FIXME: implement me");
}
	
static void
folder_browser_property_changed (BonoboPropertyBag *properties,
				 const char *name,
				 const char *type,
				 gpointer old_value,
				 gpointer new_value,
				 gpointer user_data)
{
	FolderBrowser *folder_browser = FOLDER_BROWSER (user_data);

	if (EQUAL (name, PROPERTY_FOLDER_URI)){
		folder_browser_set_uri (folder_browser, new_value);
		return;
	} else if (EQUAL (name, PROPERTY_MESSAGE_PREVIEW)){
		folder_browser_set_message_preview (folder_browser, *((gboolean *)new_value));
	}
}

static void
folder_browser_properties_init (FolderBrowser *fb)
{
	fb->properties = bonobo_property_bag_new ();

	bonobo_property_bag_add (
		fb->properties, PROPERTY_FOLDER_URI, "string",
		NULL, NULL, _("The URI that the Folder Browser will display"), 0);
	bonobo_property_bag_add (
		fb->properties, PROPERTY_MESSAGE_PREVIEW, "boolean",
		NULL, NULL, _("Whether a message preview should be shown"), 0);

	gtk_signal_connect (GTK_OBJECT (fb->properties), "value_changed",
			    folder_browser_property_changed, fb);
}

static void
folder_browser_gui_init (FolderBrowser *fb)
{
	/*
	 * The panned container
	 */
	fb->vpaned = gtk_vpaned_new ();
	gtk_widget_show (fb->vpaned);

	gtk_table_attach (
		GTK_TABLE (fb), fb->vpaned,
		0, 1, 0, 2,
		GTK_FILL | GTK_EXPAND,
		GTK_FILL | GTK_EXPAND,
		0, 0);

	fb->message_list_w = message_list_get_widget (fb->message_list);
	gtk_paned_add1 (GTK_PANED (fb->vpaned), fb->message_list_w);
	gtk_widget_show (fb->message_list_w);

	gtk_paned_add2 (GTK_PANED (fb->vpaned), GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb));

}

static void
folder_browser_init (GtkObject *object)
{
}

static void
my_folder_browser_init (GtkObject *object)
{
	FolderBrowser *fb = FOLDER_BROWSER (object);

	/*
	 * Setup parent class fields.
	 */ 
	GTK_TABLE (fb)->homogeneous = FALSE;
	gtk_table_resize (GTK_TABLE (fb), 1, 2);

	/*
	 * Our instance data
	 */
	fb->message_list = MESSAGE_LIST (message_list_new (fb));
	fb->mail_display = MAIL_DISPLAY (mail_display_new (fb));

	folder_browser_properties_init (fb);
	folder_browser_gui_init (fb);
}

GtkWidget *
folder_browser_new (void)
{
	FolderBrowser *folder_browser = gtk_type_new (folder_browser_get_type ());

	my_folder_browser_init (GTK_OBJECT (folder_browser));

	printf ("Widget started\n");
	return GTK_WIDGET (folder_browser);
}


E_MAKE_TYPE (folder_browser, "FolderBrowser", FolderBrowser, folder_browser_class_init, folder_browser_init, PARENT_TYPE);


