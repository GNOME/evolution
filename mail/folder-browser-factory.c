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
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h> 
#include "e-util/e-util.h"
#include "e-util/e-gui-utils.h"
#include "folder-browser.h"
#include "main.h"

/*
 * Creates the Folder Browser, wraps it in a Bonobo Control, and
 * sets the Bonobo Control properties to point to the Folder Browser
 * Properties
 */
static BonoboObject *
folder_browser_factory (BonoboGenericFactory *factory, void *closure)
{
	BonoboControl *control;
	GtkWidget *folder_browser;

	folder_browser = folder_browser_new ();
	if (folder_browser == NULL)
		return NULL;

	folder_browser_set_uri (FOLDER_BROWSER (folder_browser), "inbox");

	gtk_widget_show(folder_browser);
	
	control = bonobo_control_new (folder_browser);
	
	if (control == NULL){
		gtk_object_destroy (GTK_OBJECT (folder_browser));
		return NULL;
	}
	
	
	/*bonobo_control_set_property_bag (
		control,
		FOLDER_BROWSER (folder_browser)->properties);*/

	return BONOBO_OBJECT (control);

}

void
folder_browser_factory_init (void)
{
	static BonoboGenericFactory *bonobo_folder_browser_factory = NULL;

	if (bonobo_folder_browser_factory != NULL)
		return;

	bonobo_folder_browser_factory =
		bonobo_generic_factory_new (
			"GOADID:Evolution:FolderBrowserFactory:1.0",
			folder_browser_factory, NULL);

	if (bonobo_folder_browser_factory == NULL){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("We are sorry, Evolution's Folder Browser can not be initialized.")); 
		exit (1);
	}
}
