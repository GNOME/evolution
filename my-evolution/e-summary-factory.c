/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-factory.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-ui-util.h>

#include "e-util/e-gui-utils.h"

#include "e-summary.h"
#include "e-summary-factory.h"
#include "e-summary-offline-handler.h"
#include "e-summary-preferences.h"
#include "evolution-shell-component-utils.h" /* For E_PIXMAP */

BonoboUIVerb verbs[] = {
	BONOBO_UI_VERB ("PrintMyEvolution", e_summary_print),
	BONOBO_UI_VERB ("ToolsSettings", e_summary_configure),
	BONOBO_UI_VERB ("Reload", e_summary_reload),
	BONOBO_UI_VERB_END
};


static EPixmap pixmaps [] = {
	E_PIXMAP ("/commands/PrintMyEvolution", "print.xpm"),
	E_PIXMAP ("/commands/ToolsSettings", "configure_16_mail.xpm"),
	E_PIXMAP ("/Toolbar/PrintMyEvolution", "buttons/print.png"),
	E_PIXMAP_END
};

static void
control_activate (BonoboControl *control,
		  BonoboUIComponent *ui_component,
		  ESummary *summary)
{
	Bonobo_UIContainer container;

	container = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (ui_component, container);
	bonobo_object_release_unref (container, NULL);

	bonobo_ui_component_add_verb_list_with_data (ui_component, verbs, summary);
	bonobo_ui_component_freeze (ui_component, NULL);

	bonobo_ui_util_set_ui (ui_component, EVOLUTION_DATADIR,
			       "my-evolution.xml", "my-evolution");
  	e_pixmaps_update (ui_component, pixmaps); 

	bonobo_ui_component_thaw (ui_component, NULL);
	e_summary_thaw (summary);
}

static void
control_deactivate (BonoboControl *control,
		    BonoboUIComponent *ui_component,
		    ESummary *summary)
{
	bonobo_ui_component_unset_container (ui_component);
	e_summary_freeze (summary);
}

static void
control_activate_cb (BonoboControl *control,
		     gboolean activate,
		     ESummary *summary)
{
	BonoboUIComponent *ui_component;
	
	ui_component = bonobo_control_get_ui_component (control);
	
	if (summary->shell_view_interface == NULL) {
		Bonobo_ControlFrame control_frame;
		CORBA_Environment ev;

		control_frame = bonobo_control_get_control_frame (control);
		if (control_frame == NULL) {
			goto out;
		}

		CORBA_exception_init (&ev);
		summary->shell_view_interface = Bonobo_Unknown_queryInterface (control_frame, "IDL:GNOME/Evolution/ShellView:1.0", &ev);

		if (BONOBO_EX (&ev)) {
			g_warning ("Error getting ShellView. %s", CORBA_exception_id (&ev));
			summary->shell_view_interface = CORBA_OBJECT_NIL;
		}
		CORBA_exception_free (&ev);
	}
 out:

	if (activate)
		control_activate (control, ui_component, summary);
	else
		control_deactivate (control, ui_component, summary);
}

static void
control_destroy_cb (BonoboControl *control,
		    ESummary *summary)
{
	gtk_object_destroy (GTK_OBJECT (summary));
}

BonoboControl *
e_summary_factory_new_control (const char *uri,
			       const GNOME_Evolution_Shell shell,
			       ESummaryOfflineHandler *handler)
{
	BonoboControl *control;
	GtkWidget *summary;

	summary = e_summary_new (shell);
	if (summary == NULL) {
		return NULL;
	}

	e_summary_offline_handler_add_summary (handler, E_SUMMARY (summary));
	gtk_widget_show (summary);
	
	control = bonobo_control_new (summary);

	if (control == NULL) {
		gtk_object_destroy (GTK_OBJECT (summary));
		return NULL;
	}

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    control_activate_cb, summary);
	gtk_signal_connect (GTK_OBJECT (control), "destroy",
			    control_destroy_cb, summary);

	return control;
}
