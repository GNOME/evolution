/*
 * e-summary-factory.c: Executive Summary Factory.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors:  Iain Holmes <iain@ximian.com>
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

BonoboUIVerb verbs[] = {
	BONOBO_UI_VERB ("PrintMyEvolution", e_summary_print),
	BONOBO_UI_VERB_END
};

#if 0
static EPixmap pixmaps [] = {
	E_PIXMAP ("/menu/File/Print/Print", "print.xpm"),
	E_PIXMAP_END
};
#endif

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
/*  	e_pixmaps_update (ui_component, pixmaps); */

	bonobo_ui_component_thaw (ui_component, NULL);
}

static void
control_deactivate (BonoboControl *control,
		    BonoboUIComponent *ui_component,
		    ESummary *summary)
{
	bonobo_ui_component_unset_container (ui_component);
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
			       const GNOME_Evolution_Shell shell)
{
	BonoboControl *control;
	GtkWidget *summary;

	summary = e_summary_new (shell);
	if (summary == NULL) {
		return NULL;
	}

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
