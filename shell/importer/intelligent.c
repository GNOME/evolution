/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* intelligent.c
 * 
 * Authors: 
 *    Iain Holmes  <iain@ximian.com>
 *
 * Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "intelligent.h"

#include <glib.h>

#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkobject.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkclist.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>

#include <libgnome/gnome-config.h>
/*#include <libgnome/gnome-util.h>*/
#include <libgnome/gnome-i18n.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-widget.h>

#include "intelligent.h"
#include "GNOME_Evolution_Importer.h"

/* Prototypes */

void intelligent_importer_init (void);

/* End prototypes */

typedef struct {
	CORBA_Object object;
	Bonobo_Control control;
	GtkWidget *widget;

	char *name;
	char *blurb;
	char *iid;
} IntelligentImporterData;

typedef struct {
	GtkWidget *dialog;
	GtkWidget *placeholder;
	GtkWidget *clist;
	BonoboWidget *current;

	GList *importers;

	int running;
} IntelligentImporterDialog;

typedef struct {
	CORBA_Object importer;
	char *iid;
} SelectedImporterData;

static void
free_importer_dialog (IntelligentImporterDialog *d)
{
	GList *l;

	for (l = d->importers; l; l = l->next) {
		CORBA_Environment ev;
		IntelligentImporterData *data;

		data = l->data;

		CORBA_exception_init (&ev);
		if (data->object != CORBA_OBJECT_NIL) 
			bonobo_object_release_unref (data->object, &ev);

		g_free (data->iid);
		g_free (data->name);
		g_free (data->blurb);
		g_free (data);
	}

	g_list_free (d->importers);
	gtk_widget_destroy (d->dialog);
	g_free (d);
}

static void
start_importers (GList *selected)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	for (; selected; selected = selected->next) {
		SelectedImporterData *selection = selected->data;

		GNOME_Evolution_IntelligentImporter_importData (selection->importer, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Error importing %s\n%s", selection->iid,
				   CORBA_exception_id (&ev));
		}
	}
	CORBA_exception_free (&ev);
}

static GList *
get_intelligent_importers (void)
{
	Bonobo_ServerInfoList *info_list;
	GList *iids_ret = NULL;
	CORBA_Environment ev;
	int i;

	CORBA_exception_init (&ev);
	info_list = bonobo_activation_query ("repo_ids.has ('IDL:GNOME/Evolution/IntelligentImporter:" BASE_VERSION "')", NULL, &ev);
	CORBA_exception_free (&ev);

	for (i = 0; i < info_list->_length; i++) {
		const Bonobo_ServerInfo *info;
		
		info = info_list->_buffer + i;
		iids_ret = g_list_prepend (iids_ret, g_strdup (info->iid));
	}

	return iids_ret;
}

static void
select_row_cb (GtkCList *clist,
	       int row,
	       int column,
	       GdkEvent *ev,
	       IntelligentImporterDialog *d)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (d->placeholder), row);
}

static void
unselect_row_cb (GtkCList *clist,
		 int row,
		 int column,
		 GdkEvent *ev,
		 IntelligentImporterDialog *d)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (d->placeholder), d->running);
}

static IntelligentImporterDialog *
create_gui (GList *importers)
{
	GtkWidget *dialog, *clist, *sw, *label;
	GtkWidget *hbox, *vbox, *dummy;
	IntelligentImporterDialog *d;
	GList *l;
	int running = 0;

	d = g_new (IntelligentImporterDialog, 1);
	d->dialog = dialog = gtk_dialog_new();
	gtk_dialog_set_has_separator ((GtkDialog *) dialog, FALSE);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *)dialog)->vbox, 0);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *)dialog)->action_area, 12);

	gtk_window_set_title((GtkWindow *)dialog, _("Importers"));
	dummy = gtk_button_new_from_stock(GTK_STOCK_CONVERT);
	gtk_button_set_label((GtkButton *)dummy, _("Import"));
	gtk_dialog_add_action_widget((GtkDialog *)dialog, dummy, GTK_RESPONSE_ACCEPT);

	dummy = gtk_button_new_from_stock(GTK_STOCK_NO);
	gtk_button_set_label((GtkButton *)dummy, _("Don't import"));
	gtk_dialog_add_action_widget((GtkDialog *)dialog, dummy, GTK_RESPONSE_REJECT);

	dummy = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_button_set_label((GtkButton *)dummy, _("Don't ask me again"));
	gtk_dialog_add_action_widget((GtkDialog *)dialog, dummy, GTK_RESPONSE_CANCEL);
	d->importers = NULL;
	d->current = NULL;

	d->clist = clist = gtk_clist_new (1);
	gtk_clist_set_selection_mode (GTK_CLIST (d->clist), GTK_SELECTION_MULTIPLE);
	
	label = gtk_label_new (_("Evolution can import data from the following files:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label,
			    TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    TRUE, TRUE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(sw), 
					GTK_POLICY_AUTOMATIC, 
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_widget_set_size_request (sw, 300, 150);
	gtk_container_add (GTK_CONTAINER (sw), clist);
	gtk_box_pack_start (GTK_BOX (hbox), sw, TRUE, TRUE, 0);
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	d->placeholder = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (d->placeholder), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), d->placeholder, TRUE, TRUE, 0);

	for (l = importers; l; l = l->next) {
		IntelligentImporterData *data;
		CORBA_Environment ev;
		gboolean dontaskagain, can_run;
		char *text[1], *prefix;

		/* Check if we want to show this one again */
		prefix = g_strdup_printf ("=%s/evolution/config/Shell=/intelligent-importers/", g_get_home_dir ());
		gnome_config_push_prefix (prefix);
		g_free (prefix);
		
		dontaskagain = gnome_config_get_bool (l->data);
		gnome_config_pop_prefix ();
		
		if (dontaskagain)
			continue;

		data = g_new0 (IntelligentImporterData, 1);
		data->iid = g_strdup (l->data);

		CORBA_exception_init (&ev);
		data->object = bonobo_activation_activate_from_id ((char *) data->iid, 0, 
						     NULL, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Could not start %s: %s", data->iid,
				   CORBA_exception_id (&ev));
			CORBA_exception_free (&ev);

			/* Clean up the IntelligentImporterData */
			g_free (data->iid);
			g_free (data);
			continue;
		}

		CORBA_exception_free (&ev);
		if (data->object == CORBA_OBJECT_NIL) {
			g_warning ("Could not activate_component %s", data->iid);
			g_free (data->iid);
			g_free (data);
			continue;
		}

		CORBA_exception_init (&ev);
		can_run = GNOME_Evolution_IntelligentImporter_canImport (data->object,
									 &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Could not get canImport(%s): %s", 
				   data->iid, CORBA_exception_id (&ev));
			bonobo_object_release_unref (data->object, &ev);
			CORBA_exception_free (&ev);
			g_free (data->iid);
			g_free (data);
			continue;
		}
		CORBA_exception_free (&ev);
		
		if (can_run == FALSE) {
			CORBA_exception_init (&ev);
			bonobo_object_release_unref (data->object, &ev);
			CORBA_exception_free (&ev);
			g_free (data->iid);
			g_free (data);
			continue;
		}

		running++;

		data->name = g_strdup (GNOME_Evolution_IntelligentImporter__get_importername (data->object, &ev));
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Could not get name(%s): %s", 
				   data->iid, CORBA_exception_id (&ev));
			bonobo_object_release_unref (data->object, &ev);
			CORBA_exception_free (&ev);
			g_free (data->iid);
			g_free (data);
			continue;
		}

		data->blurb = g_strdup (GNOME_Evolution_IntelligentImporter__get_message (data->object, &ev));
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Could not get message(%s): %s", 
				   data->iid, CORBA_exception_id (&ev));
			bonobo_object_release_unref (data->object, &ev);
			CORBA_exception_free (&ev);
			g_free (data->iid);
			g_free (data->name);
			g_free (data);
			continue;
		}

		data->control = Bonobo_Unknown_queryInterface (data->object,
							      "IDL:Bonobo/Control:1.0", &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Could not QI for Bonobo/Control:1.0 %s:%s",
				   data->iid, CORBA_exception_id (&ev));
			bonobo_object_release_unref (data->object, &ev);
			CORBA_exception_free (&ev);
			g_free (data->iid);
			g_free (data->name);
			g_free (data->blurb);
			continue;
		}
		if (data->control != CORBA_OBJECT_NIL) {
			data->widget = bonobo_widget_new_control_from_objref (data->control, CORBA_OBJECT_NIL);
			/* Ref this widget so even if we remove it from the
			   containers it will always have an extra ref. */
			gtk_widget_show (data->widget);
			gtk_widget_ref (data->widget);
		} else {
			data->widget = gtk_label_new ("");
		}

		CORBA_exception_free (&ev);

		d->importers = g_list_prepend (d->importers, data);
		gtk_notebook_prepend_page (GTK_NOTEBOOK (d->placeholder),
					   data->widget, NULL);
		text[0] = data->name;
		gtk_clist_prepend (GTK_CLIST (clist), text);
	}
	
	d->running = running;
	dummy = gtk_drawing_area_new ();
	gtk_widget_show (dummy);
	gtk_notebook_append_page (GTK_NOTEBOOK (d->placeholder),
				  dummy, NULL);
	/* Set the start to the blank page */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (d->placeholder), running);

	g_signal_connect((clist), "select-row", 
			    G_CALLBACK (select_row_cb), d);
	g_signal_connect((clist), "unselect-row",
			    G_CALLBACK (unselect_row_cb), d);

	gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);
	return d;
}

void
intelligent_importer_init (void)
{
	GList *importers, *l, *selected = NULL;
	IntelligentImporterDialog *d;
	char *prefix;
	gboolean dontaskagain;
	int resp;

	prefix = g_strdup_printf ("=%s/evolution/config/Shell=/intelligent-importers/", g_get_home_dir());
	gnome_config_push_prefix (prefix);
	g_free (prefix);
	
	dontaskagain = gnome_config_get_bool ("Dontaskagain=False");
	gnome_config_pop_prefix ();
	
	if (dontaskagain) {
		return;
	}

	importers = get_intelligent_importers ();
	if (importers == NULL)
		return; /* No intelligent importers. Easy :) */

	d = create_gui (importers);
	if (d->running == 0) {
		free_importer_dialog (d);
		return; /* No runnable intelligent importers. */
	}

	resp = gtk_dialog_run((GtkDialog *)d->dialog);
	gtk_widget_destroy(d->dialog);
	switch (resp) {
	case GTK_RESPONSE_ACCEPT:
		/* Make a list of the importers */

		/* FIXME: Sort this list and don't do it a slow way */
		for (l = GTK_CLIST (d->clist)->selection; l; l = l->next) {
			IntelligentImporterData *data;
			SelectedImporterData *new_data;
			CORBA_Environment ev;
			char *iid;

			data = g_list_nth_data (d->importers, GPOINTER_TO_INT (l->data));
			iid = g_strdup (data->iid);

			new_data = g_new (SelectedImporterData, 1);
			new_data->iid = iid;

			/* Reference the remote object, and duplicate the 
			   local one. */
			CORBA_exception_init (&ev);
			new_data->importer = bonobo_object_dup_ref (data->object, &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				g_warning ("Error duplicating %s\n%s", iid,
					   CORBA_exception_id (&ev));
				g_free (iid);
				CORBA_exception_free (&ev);
				g_free (new_data);
				continue;
			}
			CORBA_exception_free (&ev);

			selected = g_list_prepend (selected, new_data);
		}

		/* Now destroy all the importers, as we've kept references to 
		   the ones we need */
		free_importer_dialog (d);

		if (selected != NULL) {
			/* Restart the selected ones */
			start_importers (selected);
			
			/* Free the selected list */
			for (l = selected; l; l = l->next) {
				CORBA_Environment ev;
				SelectedImporterData *selection = l->data;

				CORBA_exception_init (&ev);
				bonobo_object_release_unref (selection->importer, &ev);
				CORBA_exception_free (&ev);

				g_free (selection->iid);
				g_free (selection);
			}
			g_list_free (selected);
		}

		break;
		
	case GTK_RESPONSE_CANCEL: /* Dont ask again */
		prefix = g_strdup_printf ("=%s/evolution/config/Shell=/intelligent-importers/", g_get_home_dir());
		gnome_config_push_prefix (prefix);
		g_free (prefix);
		
		gnome_config_set_bool ("Dontaskagain", TRUE);
		gnome_config_pop_prefix ();

		gnome_config_sync ();
		gnome_config_drop_all ();
		g_print ("Not asking again");
		free_importer_dialog (d);
		break;

	default:
	case GTK_RESPONSE_REJECT: /* No button */
		free_importer_dialog (d);
		break;
	}

	g_list_free (importers);
}
