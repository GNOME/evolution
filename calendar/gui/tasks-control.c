/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* tasks-control.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Authors: Damon Chaplin <damon@ximian.com>
 *	    Ettore Perazzoli
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-stock.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <e-util/e-dialog-utils.h>
#include "dialogs/cal-prefs-dialog.h"
#include "calendar-config.h"
#include "calendar-commands.h"
#include "e-tasks.h"
#include "tasks-control.h"
#include "evolution-shell-component-utils.h"

#define TASKS_CONTROL_PROPERTY_URI		"folder_uri"
#define TASKS_CONTROL_PROPERTY_URI_IDX		1


static void tasks_control_properties_init	(BonoboControl		*control,
						 ETasks			*tasks);
static void tasks_control_get_property		(BonoboPropertyBag	*bag,
						 BonoboArg		*arg,
						 guint			 arg_id,
						 CORBA_Environment      *ev,
						 gpointer		 user_data);
static void tasks_control_set_property		(BonoboPropertyBag	*bag,
						 const BonoboArg	*arg,
						 guint			 arg_id,
						 CORBA_Environment      *ev,
						 gpointer		 user_data);
static void tasks_control_activate_cb		(BonoboControl		*control,
						 gboolean		 activate,
						 gpointer		 user_data);
static void tasks_control_activate		(BonoboControl		*control,
						 ETasks			*tasks);
static void tasks_control_deactivate		(BonoboControl		*control,
						 ETasks			*tasks);

static void tasks_control_new_task_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void tasks_control_cut_cmd               (BonoboUIComponent      *uic,
						 gpointer                data,
						 const gchar            *path);
static void tasks_control_copy_cmd              (BonoboUIComponent      *uic,
						 gpointer                data,
						 const gchar            *path);
static void tasks_control_paste_cmd             (BonoboUIComponent      *uic,
						 gpointer                data,
						 const gchar            *path);
static void tasks_control_delete_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void tasks_control_complete_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void tasks_control_expunge_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void tasks_control_settings_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);


BonoboControl *
tasks_control_new			(void)
{
	BonoboControl *control;
	GtkWidget *tasks;

	tasks = e_tasks_new ();
	if (!tasks)
		return NULL;

	gtk_widget_show (tasks);

	control = bonobo_control_new (tasks);
	if (!control) {
		g_message ("control_factory_fn(): could not create the control!");
		return NULL;
	}

	tasks_control_properties_init (control, E_TASKS (tasks));

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    GTK_SIGNAL_FUNC (tasks_control_activate_cb),
			    tasks);

	return control;
}


/* Creates the property bag for our new control. */
static void
tasks_control_properties_init		(BonoboControl		*control,
					 ETasks			*tasks)
					 
{
	BonoboPropertyBag *pbag;

	pbag = bonobo_property_bag_new (tasks_control_get_property,
					tasks_control_set_property, tasks);

	bonobo_property_bag_add (pbag,
				 TASKS_CONTROL_PROPERTY_URI,
				 TASKS_CONTROL_PROPERTY_URI_IDX,
				 BONOBO_ARG_STRING,
				 NULL,
				 _("The URI of the tasks folder to display"),
				 0);

	bonobo_control_set_properties (control, pbag);
	bonobo_object_unref (BONOBO_OBJECT (pbag));
}


/* Gets a property of our control. FIXME: Finish. */
static void
tasks_control_get_property		(BonoboPropertyBag	*bag,
					 BonoboArg		*arg,
					 guint			 arg_id,
					 CORBA_Environment      *ev,
					 gpointer		 user_data)
{
	ETasks *tasks = user_data;
	char *uri;

	switch (arg_id) {

	case TASKS_CONTROL_PROPERTY_URI_IDX:
		uri = cal_client_get_uri (e_tasks_get_cal_client (tasks));
		BONOBO_ARG_SET_STRING (arg, uri);
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}


static void
tasks_control_set_property		(BonoboPropertyBag	*bag,
					 const BonoboArg	*arg,
					 guint			 arg_id,
					 CORBA_Environment      *ev,
					 gpointer		 user_data)
{
	ETasks *tasks = user_data;
	char *uri;

	switch (arg_id) {

	case TASKS_CONTROL_PROPERTY_URI_IDX:
		uri = BONOBO_ARG_GET_STRING (arg);
		if (!e_tasks_open (tasks, uri)) {
			char *msg;

			msg = g_strdup_printf (_("Could not load the tasks in `%s'"), uri);
			gnome_error_dialog_parented (
				msg,
				GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tasks))));
			g_free (msg);
		}
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}


static void
tasks_control_activate_cb		(BonoboControl		*control,
					 gboolean		 activate,
					 gpointer		 user_data)
{
	ETasks *tasks;

	tasks = E_TASKS (user_data);

	if (activate)
		tasks_control_activate (control, tasks);
	else
		tasks_control_deactivate (control, tasks);
}

/* Sensitizes the UI Component menu/toolbar commands based on the number of
 * selected tasks.
 */
static void
sensitize_commands (ETasks *tasks, BonoboControl *control, int n_selected)
{
	BonoboUIComponent *uic;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	bonobo_ui_component_set_prop (uic, "/commands/TasksCut", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/TasksCopy", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/TasksDelete", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
	bonobo_ui_component_set_prop (uic, "/commands/TasksMarkComplete", "sensitive",
				      n_selected == 0 ? "0" : "1",
				      NULL);
}

/* Callback used when the selection in the table changes */
static void
selection_changed_cb (ETasks *tasks, int n_selected, gpointer data)
{
	BonoboControl *control;

	control = BONOBO_CONTROL (data);

	sensitize_commands (tasks, control, n_selected);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("TasksNewTask", tasks_control_new_task_cmd),
	BONOBO_UI_VERB ("TasksCut", tasks_control_cut_cmd),
	BONOBO_UI_VERB ("TasksCopy", tasks_control_copy_cmd),
	BONOBO_UI_VERB ("TasksPaste", tasks_control_paste_cmd),
	BONOBO_UI_VERB ("TasksDelete", tasks_control_delete_cmd),
	BONOBO_UI_VERB ("TasksMarkComplete", tasks_control_complete_cmd),
	BONOBO_UI_VERB ("TasksExpunge", tasks_control_expunge_cmd),
	BONOBO_UI_VERB ("TasksSettings", tasks_control_settings_cmd),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/menu/File/New/NewFirstItem/NewTask",            "new_task-16.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/TasksCut",            "16_cut.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/TasksCopy",           "16_copy.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/TasksPaste",          "16_paste.png"),
	E_PIXMAP ("/menu/EditPlaceholder/Edit/TasksDelete",         "evolution-trash-mini.png"),
	E_PIXMAP ("/menu/Tools/ComponentPlaceholder/TasksSettings", "configure_16_calendar.xpm"),
	E_PIXMAP ("/Toolbar/New",                                   "buttons/new_task.png"),
	E_PIXMAP ("/Toolbar/Cut",                                   "buttons/cut.png"),
	E_PIXMAP ("/Toolbar/Copy",                                  "buttons/copy.png"),
	E_PIXMAP ("/Toolbar/Paste",                                 "buttons/paste.png"),
	E_PIXMAP ("/Toolbar/Delete",                                "buttons/delete-message.png"),
	E_PIXMAP_END
};

static void
tasks_control_activate (BonoboControl *control, ETasks *tasks)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;
	int n_selected;
	ECalendarTable *cal_table;
	ETable *etable;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (uic, remote_uih);
	bonobo_object_release_unref (remote_uih, NULL);

	bonobo_ui_component_add_verb_list_with_data (uic, verbs, tasks);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, EVOLUTION_DATADIR,
			       "evolution-tasks.xml",
			       "evolution-tasks");

	e_pixmaps_update (uic, pixmaps);

	e_tasks_setup_view_menus (tasks, uic);

	/* Signals from the tasks widget; also sensitize the menu items as appropriate */

	gtk_signal_connect (GTK_OBJECT (tasks), "selection_changed",
			    GTK_SIGNAL_FUNC (selection_changed_cb), control);

	cal_table = e_tasks_get_calendar_table (tasks);
	etable = e_calendar_table_get_table (cal_table);
	n_selected = e_table_selected_count (etable);

	sensitize_commands (tasks, control, n_selected);

	bonobo_ui_component_thaw (uic, NULL);

	/* Show the dialog for setting the timezone if the user hasn't chosen
	   a default timezone already. This is done in the startup wizard now,
	   so we don't do it here. */
#if 0
	calendar_config_check_timezone_set ();
#endif

	control_util_set_folder_bar_label (control, "");
}


static void
tasks_control_deactivate (BonoboControl *control, ETasks *tasks)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	e_tasks_discard_view_menus (tasks);

	/* Stop monitoring the "selection_changed" signal */
	gtk_signal_disconnect_by_data (GTK_OBJECT (tasks), control);

	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic);
}


static void
tasks_control_new_task_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const char		*path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);
	e_tasks_new_task (tasks);
}

static void
tasks_control_cut_cmd                   (BonoboUIComponent      *uic,
					 gpointer                data,
					 const char             *path)
{
	ETasks *tasks;
	ECalendarTable *cal_table;

	tasks = E_TASKS (data);
	cal_table = e_tasks_get_calendar_table (tasks);
	e_calendar_table_cut_clipboard (cal_table);
}

static void
tasks_control_copy_cmd                  (BonoboUIComponent      *uic,
					 gpointer                data,
					 const char             *path)
{
	ETasks *tasks;
	ECalendarTable *cal_table;

	tasks = E_TASKS (data);
	cal_table = e_tasks_get_calendar_table (tasks);
	e_calendar_table_copy_clipboard (cal_table);
}

static void
tasks_control_paste_cmd                 (BonoboUIComponent      *uic,
					 gpointer                data,
					 const char             *path)
{
	ETasks *tasks;
	ECalendarTable *cal_table;

	tasks = E_TASKS (data);
	cal_table = e_tasks_get_calendar_table (tasks);
	e_calendar_table_paste_clipboard (cal_table);
}

static void
tasks_control_delete_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const char		*path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);
	e_tasks_delete_selected (tasks);
}

static void
tasks_control_complete_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const char		*path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);
	e_tasks_complete_selected (tasks);
}

static gboolean
confirm_expunge (ETasks *tasks)
{
	GtkWidget *dialog, *label, *checkbox;
	int button;
	
	if (!calendar_config_get_confirm_expunge ())
		return TRUE;
	
	dialog = gnome_dialog_new (_("Warning"),
				   GNOME_STOCK_BUTTON_YES,
				   GNOME_STOCK_BUTTON_NO,
				   NULL);
	e_gnome_dialog_set_parent (GNOME_DIALOG (dialog), 
				   GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (tasks),
									GTK_TYPE_WINDOW)));
	
	label = gtk_label_new (_("This operation will permanently erase all tasks marked as completed. If you continue, you will not be able to recover these tasks.\n\nReally erase these tasks?"));
	
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 4);
	
	checkbox = gtk_check_button_new_with_label (_("Do not ask me again."));
	gtk_widget_show (checkbox);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), checkbox, TRUE, TRUE, 4);
	
	button = gnome_dialog_run (GNOME_DIALOG (dialog));	
	if (button == 0 && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox)))
		calendar_config_set_confirm_expunge (FALSE);
	gnome_dialog_close (GNOME_DIALOG (dialog));
	
	if (button == 0)
		return TRUE;
	else
		return FALSE;
}

static void
tasks_control_expunge_cmd		(BonoboUIComponent	*uic,
					 gpointer		 data,
					 const char		*path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);
	
	if (confirm_expunge (tasks))
	    e_tasks_delete_completed (tasks);
}


/* Callback used for the tasks settings command */
static void
tasks_control_settings_cmd (BonoboUIComponent *uic, gpointer data, const char *path)
{
	ETasks *tasks;
	static CalPrefsDialog *prefs_dialog = NULL;

	tasks = E_TASKS (data);

	if (!prefs_dialog)
		prefs_dialog = cal_prefs_dialog_new (CAL_PREFS_DIALOG_PAGE_TASKS);
	else
		cal_prefs_dialog_show (prefs_dialog, CAL_PREFS_DIALOG_PAGE_TASKS);
}
