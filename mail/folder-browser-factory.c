/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Miguel de Icaza <miguel@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <string.h>

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
	
	control_frame = bonobo_control_get_control_frame (control, NULL);

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
	GNOME_Evolution_ShellView svi;

	container = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, container, NULL);
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
	
	svi = fb_get_svi (control);
	folder_browser_set_shell_view (fb, svi);
	bonobo_object_release_unref (svi, NULL);
	
	folder_browser_reload (fb);
	
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
	
	if (fb->message_list)
		message_list_save_state (fb->message_list);
	
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
control_destroy_cb (GtkObject *fb, GObject *control)
{
	e_list_remove (control_list, control);
}

BonoboControl *
folder_browser_factory_new_control (const char *uri,
				    const GNOME_Evolution_Shell shell)
{
	BonoboControl *control;
	GtkWidget *fb;
	
	if (!(fb = folder_browser_new (shell, uri)))
		return NULL;
	
	FOLDER_BROWSER (fb)->pref_master = TRUE; /* save UI settings changed in this FB */
	
	gtk_widget_show (fb);
	
	control = bonobo_control_new (fb);
	
	if (control == NULL) {
		g_object_unref (fb);
		return NULL;
	}
	
	g_signal_connect (control, "activate", G_CALLBACK (control_activate_cb), fb);
	
	g_object_weak_ref (G_OBJECT(control), (GWeakNotify) control_destroy_cb, fb);
	
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

FolderBrowser *
folder_browser_factory_get_browser(const char *uri)
{
	EList *controls;
	EIterator *it;
	BonoboControl *control;
	FolderBrowser *fb = NULL;
	
	if (control_list == NULL)
		return NULL;
	
	controls = folder_browser_factory_get_control_list ();
	
	it = e_list_get_iterator (controls);
	while (e_iterator_is_valid (it)) {
		control = BONOBO_CONTROL (e_iterator_get (it));
		fb = FOLDER_BROWSER (bonobo_control_get_widget (control));
		if (fb->uri && strcmp (fb->uri, uri) == 0)
			break;
		fb = NULL;
		
		e_iterator_next (it);
	}
	
	g_object_unref (it);
	
	return fb;
}
