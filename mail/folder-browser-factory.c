/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser-factory.c: A Bonobo Control factory for Folder Browsers
 *
 * Author:
 *   Miguel de Icaza (miguel@ximian.com)
 *
 * (C) 2000 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include "widgets/menus/gal-view-menus.h"

#include <gal/menus/gal-view-factory-etable.h>
#include <gal/menus/gal-view-etable.h>

#include "folder-browser-factory.h"

#include "folder-browser.h"
#include "folder-browser-ui.h"
#include "mail.h"
#include "mail-callbacks.h"
#include "shell/Evolution.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail-session.h"
#include "mail-folder-cache.h"

#include "evolution-shell-component-utils.h"

/* The FolderBrowser BonoboControls we have.  */
static EList *control_list = NULL;

/* copied from mail-display.c for now.... */
static GNOME_Evolution_ShellView
fb_get_svi (BonoboControl *control)
{
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;
	
	control_frame = bonobo_control_get_control_frame (control);

	if (control_frame == NULL)
		return CORBA_OBJECT_NIL;

	CORBA_exception_init (&ev);
	shell_view_interface = Bonobo_Unknown_queryInterface (control_frame,
							      "IDL:GNOME/Evolution/ShellView:1.0",
							      &ev);
	CORBA_exception_free (&ev);

	if (shell_view_interface == CORBA_OBJECT_NIL)
		g_warning ("Control frame doesn't have Evolution/ShellView.");

	return shell_view_interface;
}

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *uic,
		  FolderBrowser     *fb)
{
	GtkWidget *folder_browser;
	Bonobo_UIContainer container;

	container = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, container);
	bonobo_object_release_unref (container, NULL);

	g_assert (container == bonobo_ui_component_get_container (uic));
	g_return_if_fail (container != CORBA_OBJECT_NIL);
		
	folder_browser = bonobo_control_get_widget (control);
	folder_browser_set_ui_component (FOLDER_BROWSER (folder_browser), uic);

	/*bonobo_ui_component_freeze (uic, NULL);*/

	folder_browser_ui_add_global (fb);
	folder_browser_ui_add_list (fb);
	folder_browser_ui_add_message (fb);

	/*bonobo_ui_component_thaw (uic, NULL);*/

	folder_browser_set_shell_view(fb, fb_get_svi (control));

	if (fb->folder)
		mail_refresh_folder (fb->folder, NULL, NULL);

	e_search_bar_set_ui_component (E_SEARCH_BAR (fb->search), uic);
}

static void
control_deactivate (BonoboControl     *control,
		    BonoboUIComponent *uic,
		    FolderBrowser     *fb)
{
	/*bonobo_ui_component_freeze (uic, NULL);*/

	folder_browser_ui_rm_list (fb);
	folder_browser_ui_rm_all (fb);

	/*bonobo_ui_component_thaw (uic, NULL);*/
	
	if (fb->folder)
		mail_sync_folder (fb->folder, NULL, NULL);
	
	folder_browser_set_ui_component (fb, NULL);
	folder_browser_set_shell_view (fb, CORBA_OBJECT_NIL);

	e_search_bar_set_ui_component (E_SEARCH_BAR (fb->search), NULL);
}

static void
control_activate_cb (BonoboControl *control, 
		     gboolean activate, 
		     gpointer user_data)
{
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (activate)
		control_activate (control, uic, user_data);
	else
		control_deactivate (control, uic, user_data);
}

static void
control_destroy_cb (BonoboControl *control,
		    GtkObject     *folder_browser)
{
	gtk_object_destroy (folder_browser);
}

static void
browser_destroy_cb (FolderBrowser *fb,
		    BonoboControl *control)
{
	EIterator *it;

	/* We do this from browser_destroy_cb rather than
	 * control_destroy_cb because currently, the controls
	 * don't seem to all get destroyed properly at quit
	 * time (but the widgets get destroyed by X). FIXME.
	 */

	for (it = e_list_get_iterator (control_list); e_iterator_is_valid (it); e_iterator_next (it)) {
		if (e_iterator_get (it) == control) {
			e_iterator_delete (it);
			break;
		}
	}
	gtk_object_unref (GTK_OBJECT (it));
}

BonoboControl *
folder_browser_factory_new_control (const char *uri,
				    const GNOME_Evolution_Shell shell)
{
	BonoboControl *control;
	GtkWidget *folder_browser;

	folder_browser = folder_browser_new (shell, uri);
	if (folder_browser == NULL)
		return NULL;

	FOLDER_BROWSER (folder_browser)->pref_master = TRUE; /* save UI settings changed in this FB */

	gtk_widget_show (folder_browser);
	
	control = bonobo_control_new (folder_browser);
	
	if (control == NULL) {
		gtk_object_unref (GTK_OBJECT (folder_browser));
		return NULL;
	}
	
	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, folder_browser);

	gtk_signal_connect (GTK_OBJECT (control), "destroy",
			    control_destroy_cb, folder_browser);
	gtk_signal_connect (GTK_OBJECT (folder_browser), "destroy",
			    browser_destroy_cb, control);

	if (!control_list)
		control_list = e_list_new (NULL, NULL, NULL);

	e_list_append (control_list, control);

	return control;
}

EList *
folder_browser_factory_get_control_list (void)
{
	if (!control_list)
		control_list = e_list_new (NULL, NULL, NULL);
	return control_list;
}
