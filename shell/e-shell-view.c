/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * E-shell-view.c: Implements a Shell View of Evolution
 *
 * Authors:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "shortcut-bar/e-shortcut-bar.h"
#include "e-util/e-util.h"
#include "e-shell-view.h"
#include "e-shell-view-menu.h"
#include "e-shell-shortcut.h"
#include "Evolution.h"

#include <bonobo.h>
#include <libgnorba/gnorba.h>

#define PARENT_TYPE gnome_app_get_type ()

static GtkObjectClass *parent_class;

struct _EShellViewPrivate 
{
	/* a hashtable of e-folders -> widgets */
	GHashTable *folder_views;
	GtkWidget *notebook;
};


static void
esv_destroy (GtkObject *object)
{
	EShellView *eshell_view = E_SHELL_VIEW (object);

	e_shell_unregister_view (eshell_view->eshell, eshell_view);

	g_hash_table_destroy (eshell_view->priv->folder_views);	
	g_free (eshell_view->priv);	
	parent_class->destroy (object);
}

static void
e_shell_view_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = esv_destroy;

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
e_shell_view_setup (EShellView *eshell_view)
{
	/*
	 * FIXME, should load the config if (load_config)....
	 */
	gtk_window_set_default_size (GTK_WINDOW (eshell_view), 600, 400);
}

static void
e_shell_view_setup_shortcut_display (EShellView *eshell_view)
{
	eshell_view->shortcut_bar = e_shortcut_bar_view_new (eshell_view->eshell->shortcut_bar);
	
	eshell_view->shortcut_hpaned = gtk_hpaned_new ();
	gtk_widget_show (eshell_view->shortcut_hpaned);
	gtk_paned_set_position (GTK_PANED (eshell_view->shortcut_hpaned), 100);

	gtk_paned_pack1 (GTK_PANED (eshell_view->shortcut_hpaned),
			 eshell_view->shortcut_bar, FALSE, TRUE);
	gtk_widget_show (eshell_view->shortcut_bar);

	gnome_app_set_contents (GNOME_APP (eshell_view), eshell_view->shortcut_hpaned);

	gtk_signal_connect (
		GTK_OBJECT (eshell_view->shortcut_bar), "item_selected",
		GTK_SIGNAL_FUNC (shortcut_bar_item_selected), eshell_view);
}

static GtkWidget *
get_view (EShellView *eshell_view, EFolder *efolder, Bonobo_UIHandler uih)
{
  	GtkWidget *w = NULL;
	Evolution_Shell corba_shell = CORBA_OBJECT_NIL;
	EShell *shell_model = eshell_view->eshell;

	/* This type could be E_FOLDER_MAIL, E_FOLDER_CONTACTS, etc */
	EFolderType e_folder_type;

	g_assert (efolder);
	g_assert (eshell_view);
	
	e_folder_type = e_folder_get_folder_type (efolder);
	
	if (shell_model)
		corba_shell = bonobo_object_corba_objref (
			BONOBO_OBJECT (shell_model));
	else 
		g_warning ("The shell Bonobo object does not have "
			   "an associated CORBA object\n");
	
	/* depending on the type of folder, 
	 * we launch a different bonobo component */
	switch (e_folder_type) {

	case E_FOLDER_MAIL :
		w = bonobo_widget_new_control ("control:evolution-mail", uih);
		break;

	case E_FOLDER_CONTACTS :
		w = bonobo_widget_new_control ("control:addressbook", uih);
		break;

	case E_FOLDER_CALENDAR :
	case E_FOLDER_TASKS :
	case E_FOLDER_OTHER :		
	default : 
		printf ("%s: %s: No bonobo component associated with %s\n",
			__FILE__,
			__FUNCTION__,
			e_folder_get_description (efolder));
		return NULL;
	}

	if (w)
	{
		Evolution_ServiceRepository corba_sr;
		BonoboObjectClient *server =
			bonobo_widget_get_server (BONOBO_WIDGET (w));
		BonoboControlFrame *control_frame =
			bonobo_widget_get_control_frame (BONOBO_WIDGET (w));

		bonobo_control_frame_set_autoactivate (control_frame, FALSE);
		bonobo_control_frame_control_activate (control_frame);

		/* Does this control have the "ServiceRepository" interface? */
		corba_sr = (Evolution_ServiceRepository) 
			bonobo_object_client_query_interface (
				server,
				"IDL:Evolution/ServiceRepository:1.0",
				NULL);

		/* If it does, pass our shell interface to it */
		if (corba_sr != CORBA_OBJECT_NIL) {

			CORBA_Environment ev;
			CORBA_exception_init (&ev);
			Evolution_ServiceRepository_set_shell (corba_sr,
							       corba_shell,
							       &ev);
			CORBA_exception_free (&ev);
	
		} else {
			
			g_print ("The bonobo component for \"%s\" doesn't "
				 "seem to implement the "
				 "Evolution::ServiceRepository interface\n",
				 e_folder_get_description (efolder));
		}

		gtk_widget_show (w);
	}
	
	return w;
}



void
e_shell_view_set_view (EShellView *eshell_view, EFolder *efolder)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (eshell_view->priv->notebook);
	GtkWidget *folder_view = g_hash_table_lookup (
		eshell_view->priv->folder_views, efolder);
	int current_page = gtk_notebook_get_current_page (notebook);

	g_assert (eshell_view);
	g_assert (efolder);

	if (current_page != -1) {
		GtkWidget *current;

		current = gtk_notebook_get_nth_page (notebook, current_page);
		bonobo_control_frame_control_deactivate (bonobo_widget_get_control_frame (BONOBO_WIDGET (current)));
	}

	/* if we found a notebook page in our hash, that represents
	   this efolder, switch to it */
	if (folder_view) {
		int notebook_page = gtk_notebook_page_num (notebook,
							   folder_view);
		g_assert (notebook_page != -1);

		gtk_notebook_set_page (notebook, notebook_page);
		bonobo_control_frame_control_activate (bonobo_widget_get_control_frame (BONOBO_WIDGET (folder_view)));
	}
	else {
		/* get a new control that represents this efolder,
		 * append it to our notebook, and put it in our hash */
		Bonobo_UIHandler uih =
			bonobo_object_corba_objref (
				BONOBO_OBJECT (eshell_view->uih));

		GtkWidget *w = get_view (eshell_view, efolder, uih);
		int new_page_index;

		if (!w) return;
		
		gtk_notebook_append_page (notebook, w, NULL);

		new_page_index = gtk_notebook_page_num (notebook,
							folder_view);

		g_hash_table_insert (eshell_view->priv->folder_views,
				     efolder, w);
		gtk_notebook_set_page (notebook, new_page_index);
	}
}

GtkWidget *
e_shell_view_new (EShell *eshell, EFolder *efolder, gboolean show_shortcut_bar)
{
	EShellView *eshell_view;

	g_return_val_if_fail (eshell != NULL, NULL);
	g_return_val_if_fail (efolder != NULL, NULL);	
	
	eshell_view = gtk_type_new (e_shell_view_get_type ());

	eshell_view->priv = g_new (EShellViewPrivate, 1);
	eshell_view->priv->folder_views =
		g_hash_table_new (g_direct_hash, g_direct_equal);
	eshell_view->priv->notebook = NULL;

	gnome_app_construct (GNOME_APP (eshell_view),
			     "Evolution", "Evolution");

	eshell_view->eshell = eshell;
	e_shell_view_setup (eshell_view);
	e_shell_view_setup_menus (eshell_view);

	e_shell_register_view (eshell, eshell_view);
	eshell_view->shortcut_displayed = show_shortcut_bar;
	e_shell_view_setup_shortcut_display (eshell_view);

	/* create our notebook, if it hasn't been created already */
	if (!eshell_view->priv->notebook) {
		eshell_view->priv->notebook = gtk_notebook_new();

		gtk_notebook_set_show_tabs (
			GTK_NOTEBOOK (eshell_view->priv->notebook),
			FALSE);

		gtk_widget_show (eshell_view->priv->notebook);
		
		if (eshell_view->shortcut_displayed){
			gtk_paned_pack2 (
				GTK_PANED (eshell_view->shortcut_hpaned),
				eshell_view->priv->notebook, FALSE, TRUE);
		}
		else {
			gnome_app_set_contents (GNOME_APP (eshell_view),
						eshell_view->priv->notebook);
		}
	}

	e_shell_view_set_view (eshell_view, efolder);
	
	return (GtkWidget *) eshell_view;
}

void
e_shell_view_display_shortcut_bar (EShellView *eshell_view, gboolean display)
{
	g_return_if_fail (eshell_view != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (eshell_view));

	g_error ("Switching code for the shortcut bar is not written yet");
}

E_MAKE_TYPE (e_shell_view, "EShellView", EShellView, e_shell_view_class_init, NULL, PARENT_TYPE);

void
e_shell_view_new_folder (EShellView *esv)
{
	g_return_if_fail (esv != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (esv));
}

void
e_shell_view_new_shortcut (EShellView *esv)
{
	g_return_if_fail (esv != NULL);
	g_return_if_fail (E_IS_SHELL_VIEW (esv));
}
