/*
 * folder-browser.c: Folder browser top level component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include "folder-browser.h"

#define PARENT_TYPE (gtk_table_get_type ())

static GtkObjectClass *folder_browser_parent_class;

#define PROPERTY_FOLDER_URI "folder_uri"

static void
folder_browser_destroy (GtkObject *object)
{
	FolderBrowser *folder_browser = FOLDER_BROWSER (object);

	folder_browser_parent_class->destroy (object);
}

static void
folder_browser_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = folder_browser_destroy;

	folder_browser_parent_class = gtk_type_class (PARENT_TYPE);
}

#define EQUAL(a,b) (strcmp (a,b) == 0)

void
folder_browser_set_uri (FolderBrowser *folder_browser, const char *uri)
{
	g_free (folder_browser->uri);
	folder_browser->uri = g_strudp (uri);
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
	}
}

static void
folder_browser_init (GtkObject *object)
{
	FolderBrowser *fb = FOLDER_BROWSER (object);

	fb->properties = bonobo_property_bag_new ();

	bonobo_property_bag_add (
		fb->properties, PROPERTY_FOLDER_URI, "string",
		NULL, NULL, _("The URI that the Folder Browser will display", 0);

	gtk_signal_connect (GTK_OBJECT (fb->properties), "value_changed",
			    property_changed, fb);
}

GtkWidget *
folder_browser_new (void)
{
	FolderBrowser *folder_browser = gtk_type_new (folder_browser_get_type ());
	GtkTable *table = GTK_TABLE (folder_browser);
	
	table->homogeous = FALSE;
	gtk_table_resize (table, 1, 2);

	return GTK_WIDGET (folder_browser);
}


E_MAKE_TYPE (folder_browser, "FolderBrowser", FolderBrowser, folder_browser_class_init, folder_browser_init, PARENT_TYPE);


