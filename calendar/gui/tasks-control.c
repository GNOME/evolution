/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* tasks-control.c
 *
 * Copyright (C) 2000, 2001, 2002, 2003  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-paper.h>
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libgnomeprintui/gnome-print-paper-selector.h>
#include <libgnomeprintui/gnome-print-preview.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-ui-util.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-print.h>
#include "dialogs/cal-prefs-dialog.h"
#include "calendar-config.h"
#include "calendar-commands.h"
#include "e-tasks.h"
#include "e-calendar-table.h"
#include "print.h"
#include "tasks-control.h"
#include "evolution-shell-component-utils.h"
#include "e-util/e-menu.h"
#include "e-cal-menu.h"
#include "e-util/e-menu.h"
#include "itip-utils.h"

#define FIXED_MARGIN                            .05


static void tasks_control_activate_cb		(BonoboControl		*control,
						 gboolean		 activate,
						 gpointer		 user_data);
static void tasks_control_open_task_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
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
static void tasks_control_purge_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void tasks_control_print_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void tasks_control_print_preview_cmd	(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path);
static void tasks_control_assign_cmd           (BonoboUIComponent      *uic,
                                                gpointer               data,
                                                const char             *path);

static void tasks_control_forward_cmd          (BonoboUIComponent      *uic,
                                                gpointer               data,
                                                const char             *path);

	  
BonoboControl *
tasks_control_new (void)
{
	BonoboControl *control;
	GtkWidget *tasks;

	tasks = e_tasks_new ();
	if (!tasks)
		return NULL;
	gtk_widget_show (tasks);

	control = bonobo_control_new (tasks);
	if (!control) {
		gtk_widget_destroy (tasks);
		g_message ("control_factory_fn(): could not create the control!");
		return NULL;
	}

	g_signal_connect (control, "activate", G_CALLBACK (tasks_control_activate_cb), tasks);

	return control;
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

struct _tasks_sensitize_item {
	char *command;
	guint32 enable;
};

static void
sensitize_items(BonoboUIComponent *uic, struct _tasks_sensitize_item *items, guint32 mask)
{
	while (items->command) {
		char command[32];

		g_assert(strlen(items->command)<21);
		sprintf(command, "/commands/%s", items->command);

		bonobo_ui_component_set_prop (uic, command, "sensitive",
					      (items->enable & mask) == 0 ? "1" : "0",
					      NULL);
		items++;
	}
}

static struct _tasks_sensitize_item tasks_sensitize_table[] = {
	{ "TasksOpenTask", E_CAL_MENU_SELECT_ONE },
	{ "TasksCut", E_CAL_MENU_SELECT_ANY | E_CAL_MENU_SELECT_EDITABLE },
	{ "TasksCopy", E_CAL_MENU_SELECT_ANY },
	{ "TasksPaste", E_CAL_MENU_SELECT_EDITABLE },
	{ "TasksDelete", E_CAL_MENU_SELECT_ANY | E_CAL_MENU_SELECT_EDITABLE },
	{ "TasksMarkComplete", E_CAL_MENU_SELECT_ANY | E_CAL_MENU_SELECT_EDITABLE },
	{ "TasksPurge", E_CAL_MENU_SELECT_EDITABLE },
	{ "TasksAssign", E_CAL_MENU_SELECT_ONE | E_CAL_MENU_SELECT_EDITABLE | E_CAL_MENU_SELECT_ASSIGNABLE },
	{ "TasksForward", E_CAL_MENU_SELECT_ONE },
	{ 0 }
};

/* Sensitizes the UI Component menu/toolbar commands based on the number of
 * selected tasks.
 */
void
tasks_control_sensitize_commands (BonoboControl *control, ETasks *tasks, int n_selected)
{
	BonoboUIComponent *uic;
	gboolean read_only = TRUE;
	ECal *ecal;
	ECalModel *model;
	ECalMenu *menu;
	ECalMenuTargetSelect *t;
	GPtrArray *events;
	GSList *selected = NULL, *l = NULL;
	ECalendarTable *cal_table;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	if (bonobo_ui_component_get_container (uic) == CORBA_OBJECT_NIL)
		return;

	menu = e_tasks_get_tasks_menu (tasks);
	cal_table = e_tasks_get_calendar_table (tasks);
	model = e_calendar_table_get_model (cal_table);
	events = g_ptr_array_new ();
	selected = e_calendar_table_get_selected (cal_table);

	for (l = selected;l;l = g_slist_next (l)) {
		g_ptr_array_add (events, e_cal_model_copy_component_data ((ECalModelComponent *)l->data));
	}

	g_slist_free (selected);

	t = e_cal_menu_target_new_select (menu, model, events);

	ecal = e_cal_model_get_default_client (model);

	if (ecal) 
		e_cal_is_read_only (ecal, &read_only, NULL);
		
	sensitize_items (uic, tasks_sensitize_table, t->target.mask);
	e_menu_update_target ((EMenu *)menu, (EMenuTarget *)t);
}

/* Callback used when the selection in the table changes */
static void
selection_changed_cb (ETasks *tasks, int n_selected, gpointer data)
{
	BonoboControl *control;

	control = BONOBO_CONTROL (data);

	tasks_control_sensitize_commands (control, tasks, n_selected);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_VERB ("TasksOpenTask", tasks_control_open_task_cmd),
	BONOBO_UI_VERB ("TasksNewTask", tasks_control_new_task_cmd),
	BONOBO_UI_VERB ("TasksCut", tasks_control_cut_cmd),
	BONOBO_UI_VERB ("TasksCopy", tasks_control_copy_cmd),
	BONOBO_UI_VERB ("TasksPaste", tasks_control_paste_cmd),
	BONOBO_UI_VERB ("TasksDelete", tasks_control_delete_cmd),
	BONOBO_UI_VERB ("TasksMarkComplete", tasks_control_complete_cmd),
	BONOBO_UI_VERB ("TasksPurge", tasks_control_purge_cmd),
	BONOBO_UI_VERB ("TasksPrint", tasks_control_print_cmd),
	BONOBO_UI_VERB ("TasksPrintPreview", tasks_control_print_preview_cmd),
	BONOBO_UI_VERB ("TasksAssign", tasks_control_assign_cmd),
        BONOBO_UI_VERB ("TasksForward", tasks_control_forward_cmd),
     
	BONOBO_UI_VERB_END
};

void
tasks_control_activate (BonoboControl *control, ETasks *tasks)
{
	Bonobo_UIContainer remote_uih;
	BonoboUIComponent *uic;
	int n_selected;
	ECalendarTable *cal_table;
	ETable *etable;

	uic = bonobo_control_get_ui_component (control);
	g_assert (uic != NULL);

	remote_uih = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_uih, NULL);
	bonobo_object_release_unref (remote_uih, NULL);

	e_tasks_set_ui_component (tasks, uic);

	bonobo_ui_component_add_verb_list_with_data (uic, verbs, tasks);

	bonobo_ui_component_freeze (uic, NULL);

	bonobo_ui_util_set_ui (uic, PREFIX,
			       EVOLUTION_UIDIR "/evolution-tasks.xml",
			       "evolution-tasks",
			       NULL);

	e_tasks_setup_view_menus (tasks, uic);

	/* Signals from the tasks widget; also sensitize the menu items as appropriate */

	g_signal_connect (tasks, "selection_changed", G_CALLBACK (selection_changed_cb), control);

	e_menu_activate ((EMenu *)e_tasks_get_tasks_menu (tasks), uic, 1);
	cal_table = e_tasks_get_calendar_table (tasks);
	etable = e_calendar_table_get_table (cal_table);
	n_selected = e_table_selected_count (etable);

	tasks_control_sensitize_commands (control, tasks, n_selected);

	bonobo_ui_component_thaw (uic, NULL);

	/* Show the dialog for setting the timezone if the user hasn't chosen
	   a default timezone already. This is done in the startup wizard now,
	   so we don't do it here. */
#if 0
	calendar_config_check_timezone_set ();
#endif
}


void
tasks_control_deactivate (BonoboControl *control, ETasks *tasks)
{
	BonoboUIComponent *uic = bonobo_control_get_ui_component (control);

	g_assert (uic != NULL);

	e_menu_activate ((EMenu *)e_tasks_get_tasks_menu (tasks), uic, 0);
	e_tasks_set_ui_component (tasks, NULL);

	e_tasks_discard_view_menus (tasks);

	/* Stop monitoring the "selection_changed" signal */
	g_signal_handlers_disconnect_matched (tasks, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, control);

	bonobo_ui_component_rm (uic, "/", NULL);
 	bonobo_ui_component_unset_container (uic, NULL);
}

static void tasks_control_open_task_cmd		(BonoboUIComponent	*uic,
						 gpointer		 data,
						 const char		*path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);
	e_tasks_open_task (tasks);
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
confirm_purge (ETasks *tasks)
{
	GtkWidget *dialog, *checkbox, *parent;
	int button;
	
	if (!calendar_config_get_confirm_purge ())
		return TRUE;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (tasks));
	dialog = gtk_message_dialog_new (
		(GtkWindow *)parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_YES_NO,
		_("This operation will permanently erase all tasks marked as completed. If you continue, you will not be able to recover these tasks.\n\nReally erase these tasks?"));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);

	checkbox = gtk_check_button_new_with_label (_("Do not ask me again."));
	gtk_widget_show (checkbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), checkbox, TRUE, TRUE, 6);
	
	button = gtk_dialog_run (GTK_DIALOG (dialog));	
	if (button == GTK_RESPONSE_YES && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox)))
		calendar_config_set_confirm_purge (FALSE);
	gtk_widget_destroy (dialog);
	
	return button == GTK_RESPONSE_YES ? TRUE : FALSE;
}

static void
tasks_control_purge_cmd	(BonoboUIComponent	*uic,
			 gpointer		 data,
			 const char		*path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);
	
	if (confirm_purge (tasks))
	    e_tasks_delete_completed (tasks);
}


static void
print_tasks (ETasks *tasks, gboolean preview)
{
	ECalendarTable *cal_table;
	ETable *etable;

	cal_table = e_tasks_get_calendar_table (tasks);
	etable = e_calendar_table_get_table (E_CALENDAR_TABLE (cal_table));

	print_table (etable, _("Print Tasks"), _("Tasks"), preview);
}

/* File/Print callback */
static void
tasks_control_print_cmd (BonoboUIComponent *uic,
			 gpointer data,
			 const char *path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);

	print_tasks (tasks, FALSE);
}

static void
tasks_control_print_preview_cmd (BonoboUIComponent *uic,
				 gpointer data,
				 const char *path)
{
	ETasks *tasks;

	tasks = E_TASKS (data);

	print_tasks (tasks, TRUE);
}

static void
tasks_control_assign_cmd (BonoboUIComponent *uic,
                         gpointer data,
                         const char *path)
{
              ETasks *tasks;
              ECalendarTable *cal_table;
              ECalModelComponent *comp_data;

              tasks = E_TASKS (data);
              cal_table = e_tasks_get_calendar_table (tasks);
              comp_data = e_calendar_table_get_selected_comp (cal_table);
 	       if (comp_data)
              e_calendar_table_open_task (cal_table, comp_data, TRUE);
}

static void
tasks_control_forward_cmd (BonoboUIComponent *uic,
                         gpointer data,
                          const char *path)
{
	        ETasks *tasks;
               ECalendarTable *cal_table;
               ECalModelComponent *comp_data;
	
               tasks = E_TASKS (data);
               cal_table = e_tasks_get_calendar_table (tasks);
               comp_data = e_calendar_table_get_selected_comp (cal_table);
               if (comp_data) {
                       ECalComponent *comp;
                       comp = e_cal_component_new ();
                       e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
                       itip_send_comp (E_CAL_COMPONENT_METHOD_PUBLISH, comp, comp_data->client, NULL, NULL);
                       g_object_unref (comp);
	       }
}	       
