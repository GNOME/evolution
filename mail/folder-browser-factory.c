/*
 * folder-browser-factory.c: A Bonobo Control factory for Folder Browsers
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include <bonobo/gnome-main.h>
#include <bonobo/gnome-object.h>
#include <bonobo/gnome-generic-factory.h>

static GnomeObject *
folder_browser_factory (GnomeGenericFactory *factory, void *closure)
{
	g_error ("Fill me in!");
		
	return NULL;
}

void
folder_browser_factory_init (void)
{
	static GnomeGenericFactory *bonobo_folder_browser_factory = NULL;

	if (bonobo_folder_browser_factory != NULL)
		return;

	bonobo_folder_browser_factory =
		gnome_generic_factory_new (
			"Evolution:FolderBrowser:1.0",
			folder_browser_factory, NULL);

	if (bonobo_folder_browser_factory == NULL){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("We are sorry, Evolution's Folder Browser can not be initialized.")); 
		exit (1);
	}
}
