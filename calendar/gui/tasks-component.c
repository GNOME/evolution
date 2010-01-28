/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-url.h>
#include <libedataserverui/e-source-selector.h>
#include <shell/e-user-creatable-items-handler.h>
#include <shell/e-component-view.h>
#include "e-cal-model.h"
#include "e-tasks.h"
#include "tasks-component.h"
#include "tasks-control.h"
#include "e-comp-editor-registry.h"
#include "migration.h"
#include "comp-util.h"
#include "calendar-config.h"
#include "e-cal-popup.h"
#include "common/authentication.h"
#include "dialogs/calendar-setup.h"
#include "dialogs/comp-editor.h"
#include "dialogs/copy-source-dialog.h"
#include "dialogs/task-editor.h"
#include "misc/e-info-label.h"
#include "e-util/e-error.h"

#define CREATE_TASK_ID               "task"
#define CREATE_TASK_ASSIGNED_ID      "task-assigned"
#define CREATE_TASK_LIST_ID          "task-list"

enum DndTargetType {
	DND_TARGET_TYPE_CALENDAR_LIST
};
#define CALENDAR_TYPE "text/calendar"
#define XCALENDAR_TYPE "text/x-calendar"
#define WEB_BASE_URI "webcal://"
#define PERSONAL_RELATIVE_URI "system"

static GtkTargetEntry drag_types[] = {
	{ (gchar *) CALENDAR_TYPE, 0, DND_TARGET_TYPE_CALENDAR_LIST },
	{ (gchar *) XCALENDAR_TYPE, 0, DND_TARGET_TYPE_CALENDAR_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

/* Tasks should have their own registry */
#ifdef G_OS_WIN32
const ECompEditorRegistry * const comp_editor_get_registry();
#define comp_editor_registry comp_editor_get_registry()
#else
extern ECompEditorRegistry *comp_editor_registry;
#endif

typedef struct
{
	ESourceList *source_list;

	GSList *source_selection;

	ETasks *tasks;
	ETable *table;
	ETableModel *model;

	EInfoLabel *info_label;
	GtkWidget *source_selector;

	BonoboControl *view_control;
	BonoboControl *sidebar_control;
	BonoboControl *statusbar_control;

	GList *notifications;

	EUserCreatableItemsHandler *creatable_items_handler;

	EActivityHandler *activity_handler;
} TasksComponentView;

struct _TasksComponentPrivate {
	gchar *base_directory;
	gchar *config_directory;

	ESourceList *source_list;
	GSList *source_selection;

	GList *views;

	ECal *create_ecal;

	GList *notifications;
};

static void
ensure_sources (TasksComponent *component)
{
	ESourceList *source_list;
	ESourceGroup *on_this_computer;
	ESource *personal_source;
	gchar *base_uri, *base_uri_proto, base_uri_proto_seventh;
	const gchar *base_dir;

	personal_source = NULL;

	if (!e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_TODO, NULL)) {
		g_warning ("Could not get task source list from GConf!");
		return;
	}

	base_dir = tasks_component_peek_base_directory (component);
	base_uri = g_build_filename (base_dir, "local", NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);
	if (strlen (base_uri_proto) > 7) {
		/* compare only file:// part. If user home dir name changes we do not want to create
		   one more group  */
		base_uri_proto_seventh = base_uri_proto[7];
		base_uri_proto[7] = 0;
	} else {
		base_uri_proto_seventh = -1;
	}

	on_this_computer = e_source_list_ensure_group (source_list, _("On This Computer"), base_uri_proto, TRUE);
	e_source_list_ensure_group (source_list, _("On The Web"), WEB_BASE_URI, FALSE);

	if (base_uri_proto_seventh != -1) {
		base_uri_proto[7] = base_uri_proto_seventh;
	}

	if (on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;
			if (!strcmp (PERSONAL_RELATIVE_URI, relative_uri)) {
				personal_source = source;
				break;
			}
		}
		/* Make sure we have the correct base uri. This can change when user's
		   homedir name changes */
		if (strcmp (base_uri_proto, e_source_group_peek_base_uri (on_this_computer))) {
		    e_source_group_set_base_uri (on_this_computer, base_uri_proto);

		    /* *sigh* . We shouldn't  need this sync call here as set_base_uri
		       call results in synching to gconf, but that happens in idle loop
		       and too late to prevent user seeing "Can not Open ... because of invalid uri" error.*/
		    e_source_list_sync (source_list,NULL);
		}
	}

	if (personal_source) {
		/* ensure the source name is in current locale, not read from configuration */
		e_source_set_name (personal_source, _("Personal"));
	} else {
		GSList *tasks_selected;
		/* Create the default Person addressbook */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);
		g_object_unref (source);

		tasks_selected = calendar_config_get_tasks_selected ();

		if (!calendar_config_get_primary_tasks () && !tasks_selected) {
			GSList selected;

			calendar_config_set_primary_tasks (e_source_peek_uid (source));

			selected.data = (gpointer)e_source_peek_uid (source);
			selected.next = NULL;
			calendar_config_set_tasks_selected (&selected);
		}

		if (tasks_selected) {
			g_slist_foreach (tasks_selected, (GFunc) g_free, NULL);
			g_slist_free (tasks_selected);
		}

		e_source_set_color_spec (source, "#BECEDD");
		personal_source = source;
	}

	component->priv->source_list = source_list;

	g_object_unref (on_this_computer);
	g_free (base_uri_proto);
	g_free (base_uri);
}

/* Utility functions.  */
/* FIXME Some of these are duplicated from calendar-component.c */
static gboolean
is_in_selection (GSList *selection, ESource *source)
{
	GSList *l;

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;

		if (!strcmp (e_source_peek_uid (selected_source), e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static gboolean
is_in_uids (GSList *uids, ESource *source)
{
	GSList *l;

	for (l = uids; l; l = l->next) {
		const gchar *uid = l->data;

		if (!strcmp (uid, e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
update_uris_for_selection (TasksComponentView *component_view)
{
	GSList *selection, *l, *uids_selected = NULL;

	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));

	for (l = component_view->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			e_tasks_remove_todo_source (component_view->tasks, old_selected_source);
	}

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;

		e_tasks_add_todo_source (component_view->tasks, selected_source);
		uids_selected = g_slist_append (uids_selected, (gchar *)e_source_peek_uid (selected_source));
	}

	e_source_selector_free_selection (component_view->source_selection);
	component_view->source_selection = selection;

	/* Save the selection for next time we start up */
	calendar_config_set_tasks_selected (uids_selected);
	g_slist_free (uids_selected);
}

static void
update_uri_for_primary_selection (TasksComponentView *component_view)
{
	ESource *source;
	ECalendarTable *cal_table;
	ETable *etable;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!source)
		return;

	/* Set the default */
	e_tasks_set_default_source (component_view->tasks, source);

	cal_table = e_tasks_get_calendar_table (component_view->tasks);
	etable = e_calendar_table_get_table (cal_table);

	tasks_control_sensitize_commands (component_view->view_control, component_view->tasks, e_table_selected_count (etable));

	/* Save the selection for next time we start up */
	calendar_config_set_primary_tasks (e_source_peek_uid (source));
}

static void
update_selection (TasksComponentView *component_view)
{
	GSList *selection, *uids_selected, *l;

	/* Get the selection in gconf */
	uids_selected = calendar_config_get_tasks_selected ();

	/* Remove any that aren't there any more */
	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));

	for (l = selection; l; l = l->next) {
		ESource *source = l->data;

		if (!is_in_uids (uids_selected, source))
			e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
	}

	e_source_selector_free_selection (selection);

	/* Make sure the whole selection is there */
	for (l = uids_selected; l; l = l->next) {
		gchar *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (component_view->source_list, uid);
		if (source)
			e_source_selector_select_source (E_SOURCE_SELECTOR (component_view->source_selector), source);

		g_free (uid);
	}
	g_slist_free (uids_selected);
}

static void
update_primary_selection (TasksComponentView *component_view)
{
	ESource *source = NULL;
	gchar *uid;

	uid = calendar_config_get_primary_tasks ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (component_view->source_list, uid);
		g_free (uid);
	}

	if (source) {
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector), source);
	} else {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (component_view->source_list);
		if (source)
			e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector), source);
	}
}

/* Callbacks.  */
static void
copy_task_list_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	TasksComponentView *component_view = data;
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	copy_source_dialog (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)), selected_source, E_CAL_SOURCE_TYPE_TODO);
}

static void
delete_task_list_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	TasksComponentView *component_view = data;
	ESource *selected_source;
	ECal *cal;
	gchar *uri;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	if (e_error_run((GtkWindow *)gtk_widget_get_toplevel(ep->target->widget),
			"calendar:prompt-delete-task-list", e_source_peek_name(selected_source), NULL) != GTK_RESPONSE_YES)
		return;

	/* first, ask the backend to remove the task list */
	uri = e_source_get_uri (selected_source);
	cal = e_cal_model_get_client_for_uri (
		e_calendar_table_get_model (E_CALENDAR_TABLE (e_tasks_get_calendar_table (component_view->tasks))),
		uri);
	if (!cal)
		cal = e_cal_new_from_uri (uri, E_CAL_SOURCE_TYPE_TODO);
	g_free (uri);
	if (cal) {
		if (e_cal_remove (cal, NULL)) {
			if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (component_view->source_selector),
								  selected_source)) {
				e_tasks_remove_todo_source (component_view->tasks, selected_source);
				e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector),
								   selected_source);
			}

			e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);
			e_source_list_sync (component_view->source_list, NULL);
		}
	}
}

static void
new_task_list_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	calendar_setup_new_task_list (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)));
}

static void
rename_task_list_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	TasksComponentView *component_view = data;
	ESourceSelector *selector;

	selector = E_SOURCE_SELECTOR (component_view->source_selector);
	e_source_selector_edit_primary_selection (selector);
}

static void
edit_task_list_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	TasksComponentView *component_view = data;
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	calendar_setup_edit_task_list (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)), selected_source);
}

static void
set_offline_availability (EPopup *ep, EPopupItem *pitem, gpointer data, const gchar *value)
{
	TasksComponentView *component_view = data;
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	e_source_set_property (selected_source, "offline_sync", value);
}

static void
mark_no_offline_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	set_offline_availability (ep, pitem, data, "0");
}

static void
mark_offline_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	set_offline_availability (ep, pitem, data, "1");
}

static EPopupItem etc_source_popups[] = {
	{ E_POPUP_ITEM, (gchar *) "10.new", (gchar *) N_("_New Task List"), new_task_list_cb, NULL, (gchar *) "stock_todo", 0, 0 },
	{ E_POPUP_ITEM, (gchar *) "15.copy", (gchar *) N_("_Copy..."), copy_task_list_cb, NULL, (gchar *) "edit-copy", 0, E_CAL_POPUP_SOURCE_PRIMARY },
	{ E_POPUP_ITEM, (gchar *) "18.rename", (gchar *) N_("_Rename..."), rename_task_list_cb, NULL, NULL, 0, E_CAL_POPUP_SOURCE_PRIMARY|E_CAL_POPUP_SOURCE_DELETE },

	{ E_POPUP_BAR, (gchar *) "20.bar" },
	{ E_POPUP_ITEM, (gchar *) "20.delete", (gchar *) N_("_Delete"), delete_task_list_cb, NULL, (gchar *) "edit-delete", 0, E_CAL_POPUP_SOURCE_USER|E_CAL_POPUP_SOURCE_PRIMARY|E_CAL_POPUP_SOURCE_DELETE },
	{ E_POPUP_ITEM, (gchar *) "30.mark_tasks_offline", (gchar *) N_("_Make available for offline use"), mark_offline_cb, NULL, (gchar *) "stock_disconnect", E_CAL_POPUP_SOURCE_OFFLINE, E_CAL_POPUP_SOURCE_USER|E_CAL_POPUP_SOURCE_PRIMARY|E_CAL_POPUP_SOURCE_OFFLINE },
	{ E_POPUP_ITEM, (gchar *) "40.mark_tasks_no_offline", (gchar *) N_("_Do not make available for offline use"), mark_no_offline_cb, NULL, (gchar *) "stock_connect", E_CAL_POPUP_SOURCE_NO_OFFLINE, E_CAL_POPUP_SOURCE_USER|E_CAL_POPUP_SOURCE_PRIMARY|E_CAL_POPUP_SOURCE_NO_OFFLINE },

	{ E_POPUP_BAR, (gchar *) "99.bar" },
	{ E_POPUP_ITEM, (gchar *) "99.properties", (gchar *) N_("_Properties"), edit_task_list_cb, NULL, (gchar *) "document-properties", 0, E_CAL_POPUP_SOURCE_PRIMARY },
};

static void
etc_source_popup_free(EPopup *ep, GSList *list, gpointer data)
{
	g_slist_free(list);
}

static gboolean
popup_event_cb(ESourceSelector *selector, ESource *insource, GdkEventButton *event, TasksComponentView *component_view)
{
	ECalPopup *ep;
	ECalPopupTargetSource *t;
	GSList *menus = NULL;
	gint i;
	GtkMenu *menu;

	/** @HookPoint-ECalPopup: Tasks Source Selector Context Menu
	 * @Id: org.gnome.evolution.tasks.source.popup
	 * @Class: org.gnome.evolution.calendar.popup:1.0
	 * @Target: ECalPopupTargetSource
	 *
	 * The context menu on the source selector in the tasks window.
	 */
	ep = e_cal_popup_new("org.gnome.evolution.tasks.source.popup");
	t = e_cal_popup_target_new_source(ep, selector);
	t->target.widget = (GtkWidget *)component_view->tasks;

	for (i=0;i<sizeof(etc_source_popups)/sizeof(etc_source_popups[0]);i++)
		menus = g_slist_prepend(menus, &etc_source_popups[i]);

	e_popup_add_items((EPopup *)ep, menus, NULL,etc_source_popup_free, component_view);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event?event->button:0, event?event->time:gtk_get_current_event_time());

	return TRUE;
}

static void
source_selection_changed_cb (ESourceSelector *selector, TasksComponentView *component_view)
{
	update_uris_for_selection (component_view);
}

static void
primary_source_selection_changed_cb (ESourceSelector *selector, TasksComponentView *component_view)
{
	update_uri_for_primary_selection (component_view);
}

static void
source_added_cb (ETasks *tasks, ESource *source, TasksComponentView *component_view)
{
	e_source_selector_select_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
}

static void
source_removed_cb (ETasks *tasks, ESource *source, TasksComponentView *component_view)
{
	e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
}

static void
set_info (TasksComponentView *component_view)
{
	GString *message = g_string_new (NULL);
	gint rows, selected_rows;

	rows = e_table_model_row_count (component_view->model);
	selected_rows =  e_table_selected_count (component_view->table);

	g_string_append_printf(message, ngettext("%d task", "%d tasks", rows), rows);
	if (selected_rows > 0)
		g_string_append_printf(message, ngettext(", %d selected", ", %d selected", selected_rows), selected_rows);

	e_info_label_set_info (component_view->info_label, _("Tasks"), message->str);

	g_string_free (message, TRUE);
}

static void
table_selection_change_cb (ETableModel *etm, TasksComponentView *component_view)
{
	set_info (component_view);
}

static void
model_changed_cb (ETableModel *etm, TasksComponentView *component_view)
{
	set_info (component_view);
}

static void
model_rows_inserted_cb (ETableModel *etm, gint row, gint count, TasksComponentView *component_view)
{
	set_info (component_view);
}

static void
model_rows_deleted_cb (ETableModel *etm, gint row, gint count, TasksComponentView *component_view)
{
	set_info (component_view);
}

/* Evolution::Component CORBA methods */

static void
impl_upgradeFromVersion (PortableServer_Servant servant,
			 CORBA_short major,
			 CORBA_short minor,
			 CORBA_short revision,
			 CORBA_Environment *ev)
{
	GError *err = NULL;
	TasksComponent *component = TASKS_COMPONENT (bonobo_object_from_servant (servant));

	if (!migrate_tasks(component, major, minor, revision, &err)) {
		GNOME_Evolution_Component_UpgradeFailed *failedex;

		failedex = GNOME_Evolution_Component_UpgradeFailed__alloc();
		failedex->what = CORBA_string_dup(_("Failed upgrading tasks."));
		failedex->why = CORBA_string_dup(err->message);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UpgradeFailed, failedex);
	}

	if (err)
		g_error_free(err);
}

static gboolean
selector_tree_data_dropped (ESourceSelector *selector,
                            GtkSelectionData *data,
                            ESource *destination,
                            GdkDragAction action,
                            guint info,
                            TasksComponent *component)
{
	gboolean success = FALSE;
	icalcomponent *icalcomp = NULL;
	ECal *client = NULL;
	GSList *components, *p;

	client = auth_new_cal_from_source (
		destination, E_CAL_SOURCE_TYPE_TODO);

	if (!client || !e_cal_open (client, TRUE, NULL))
		goto  finish;

	components = cal_comp_selection_get_string_list (data);
	success = components != NULL;
	for (p = components; p && success; p = p->next) {
		gchar *comp_str; /* do not free this! */

		/* p->data is "source_uid\ncomponent_string" */
		comp_str = strchr (p->data, '\n');
		if (!comp_str)
			continue;

		comp_str [0] = 0;
		comp_str++;
		icalcomp = icalparser_parse_string (comp_str);

		if (!icalcomp)
			continue;

		success = cal_comp_process_source_list_drop (client, icalcomp, action, p->data, component->priv->source_list);
		icalcomponent_free (icalcomp);
	}
	g_slist_foreach (components, (GFunc)g_free, NULL);
	g_slist_free (components);

 finish:
	if (client)
		g_object_unref (client);

	return success;
}

static void
control_activate_cb (BonoboControl *control, gboolean activate, gpointer data)
{
	TasksComponentView *component_view = data;

	if (activate) {
		BonoboUIComponent *uic;
		uic = bonobo_control_get_ui_component (component_view->view_control);

		e_user_creatable_items_handler_activate (component_view->creatable_items_handler, uic);
	}
}

static void
config_create_ecal_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	TasksComponent *component = data;
	TasksComponentPrivate *priv;

	priv = component->priv;

	g_object_unref (priv->create_ecal);
	priv->create_ecal = NULL;

	priv->notifications = g_list_remove (priv->notifications, GUINT_TO_POINTER (id));
}

static ECal *
setup_create_ecal (TasksComponent *component, TasksComponentView *component_view)
{
	TasksComponentPrivate *priv;
	ESource *source = NULL;
	gchar *uid;
	guint not;

	priv = component->priv;

	if (component_view) {
		ECal *default_ecal;

		default_ecal = e_tasks_get_default_client (component_view->tasks);
		if (default_ecal)
			return default_ecal;
	}

	if (priv->create_ecal)
		return priv->create_ecal;

	/* Get the current primary calendar, or try to set one if it doesn't already exist */
	uid = calendar_config_get_primary_tasks ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);

		priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
	}

	if (!priv->create_ecal) {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
	}

	if (priv->create_ecal) {
		icaltimezone *zone;

		zone = calendar_config_get_icaltimezone ();
		e_cal_set_default_timezone (priv->create_ecal, zone, NULL);

		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the task list '%s' for creating events and meetings"),
							 e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return NULL;
		}

	} else {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 "%s", _("There is no calendar available for creating tasks"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_tasks (config_create_ecal_changed_cb,
							      component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_tasks (e_source_peek_uid (source));

	return priv->create_ecal;
}

/* Ensures the calendar is selected */
static void
object_created_cb (CompEditor *ce, ECalendarTable *cal_table)
{
	g_return_if_fail (cal_table != NULL);

	cal_table->user_created_cal = comp_editor_get_client (ce);
	g_signal_emit_by_name (cal_table, "user_created");
	cal_table->user_created_cal = NULL;
}

static gboolean
create_new_todo (TasksComponent *task_component, gboolean is_assigned, TasksComponentView *component_view)
{
	ECal *ecal;
	ECalComponent *comp;
	CompEditor *editor;
	guint32 flags = 0;

	ecal = setup_create_ecal (task_component, component_view);
	if (!ecal)
		return FALSE;

	if (is_assigned)
		flags |= COMP_EDITOR_IS_ASSIGNED;

	flags |= COMP_EDITOR_NEW_ITEM | COMP_EDITOR_USER_ORG;

	editor = task_editor_new (ecal, flags);
	comp = cal_comp_task_new_with_defaults (ecal);

	if (component_view)
		g_signal_connect (editor, "object_created", G_CALLBACK (object_created_cb), e_tasks_get_calendar_table (component_view->tasks));

	comp_editor_edit_comp (editor, comp);
	if (is_assigned)
		task_editor_show_assignment (TASK_EDITOR (editor));
	gtk_window_present (GTK_WINDOW (editor));

	e_comp_editor_registry_add (comp_editor_registry, editor, TRUE);

	return TRUE;
}

static void
create_local_item_cb (EUserCreatableItemsHandler *handler, const gchar *item_type_name, gpointer data)
{
	TasksComponent *tasks_component = data;
	TasksComponentPrivate *priv;
	TasksComponentView *component_view = NULL;
	GList *l;

	priv = tasks_component->priv;

	for (l = priv->views; l; l = l->next) {
		component_view = l->data;

		if (component_view->creatable_items_handler == handler)
			break;

		component_view = NULL;
	}

	if (strcmp (item_type_name, CREATE_TASK_ID) == 0) {
		create_new_todo (tasks_component, FALSE, component_view);
	} else if (strcmp (item_type_name, CREATE_TASK_ASSIGNED_ID) == 0) {
		create_new_todo (tasks_component, TRUE, component_view);
	} else if (strcmp (item_type_name, CREATE_TASK_LIST_ID) == 0) {
		calendar_setup_new_task_list (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (component_view->tasks))));
	}
}

static TasksComponentView *
create_component_view (TasksComponent *tasks_component)
{
	TasksComponentPrivate *priv;
	TasksComponentView *component_view;
	GtkWidget *selector_scrolled_window, *vbox;
	GtkWidget *statusbar_widget;
	AtkObject *a11y;

	priv = tasks_component->priv;

	/* Create the calendar component view */
	component_view = g_new0 (TasksComponentView, 1);

	/* Add the source lists */
	component_view->source_list = g_object_ref (priv->source_list);

	/* Create sidebar selector */
	component_view->source_selector = e_source_selector_new (tasks_component->priv->source_list);
	e_source_selector_set_select_new ((ESourceSelector *)component_view->source_selector, TRUE);
	a11y = gtk_widget_get_accessible (GTK_WIDGET (component_view->source_selector));
	atk_object_set_name (a11y, _("Task Source Selector"));

	g_signal_connect (
		component_view->source_selector, "data-dropped",
		G_CALLBACK (selector_tree_data_dropped), tasks_component);

	gtk_drag_dest_set(component_view->source_selector, GTK_DEST_DEFAULT_ALL, drag_types,
			  num_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	gtk_widget_show (component_view->source_selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), component_view->source_selector);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					     GTK_SHADOW_IN);
	gtk_widget_show (selector_scrolled_window);

	component_view->info_label = (EInfoLabel *)e_info_label_new("evolution-tasks");
	e_info_label_set_info(component_view->info_label, _("Tasks"), "");
	gtk_widget_show (GTK_WIDGET (component_view->info_label));

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), GTK_WIDGET (component_view->info_label), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), selector_scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	component_view->sidebar_control = bonobo_control_new (vbox);

	/* Create main view */
	component_view->view_control = tasks_control_new ();
	if (!component_view->view_control) {
		/* FIXME free memory */

		return NULL;
	}

	component_view->tasks = (ETasks *) bonobo_control_get_widget (component_view->view_control);
	component_view->table = e_calendar_table_get_table (e_tasks_get_calendar_table (component_view->tasks));
	component_view->model = E_TABLE_MODEL (e_calendar_table_get_model (e_tasks_get_calendar_table (component_view->tasks)));

	/* This signal is thrown if backends die - we update the selector */
	g_signal_connect (component_view->tasks, "source_added",
			  G_CALLBACK (source_added_cb), component_view);
	g_signal_connect (component_view->tasks, "source_removed",
			  G_CALLBACK (source_removed_cb), component_view);

	/* Create status bar */
	statusbar_widget = e_task_bar_new ();
	component_view->activity_handler = e_activity_handler_new ();
	e_activity_handler_attach_task_bar (component_view->activity_handler, E_TASK_BAR (statusbar_widget));
	gtk_widget_show (statusbar_widget);

	component_view->statusbar_control = bonobo_control_new (statusbar_widget);

	e_calendar_table_set_activity_handler (e_tasks_get_calendar_table (component_view->tasks), component_view->activity_handler);

	/* connect after setting the initial selections, or we'll get unwanted calls
	   to calendar_control_sensitize_calendar_commands */
	g_signal_connect (component_view->source_selector, "selection_changed",
			  G_CALLBACK (source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "primary_selection_changed",
			  G_CALLBACK (primary_source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "popup_event",
			  G_CALLBACK (popup_event_cb), component_view);

	/* Set up the "new" item handler */
	component_view->creatable_items_handler = e_user_creatable_items_handler_new ("tasks", create_local_item_cb, tasks_component);
	g_signal_connect (component_view->view_control, "activate", G_CALLBACK (control_activate_cb), component_view);

	/* We use this to update the component information */
	set_info (component_view);
	g_signal_connect (component_view->table, "selection_change",
			  G_CALLBACK (table_selection_change_cb), component_view);
	g_signal_connect (component_view->model, "model_changed",
			  G_CALLBACK (model_changed_cb), component_view);
	g_signal_connect (component_view->model, "model_rows_inserted",
			  G_CALLBACK (model_rows_inserted_cb), component_view);
	g_signal_connect (component_view->model, "model_rows_deleted",
			  G_CALLBACK (model_rows_deleted_cb), component_view);

	/* Load the selection from the last run */
	update_selection (component_view);
	update_primary_selection (component_view);

	return component_view;
}

static void
destroy_component_view (TasksComponentView *component_view)
{
	GList *l;

	g_signal_handlers_disconnect_matched (component_view->model, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, component_view);

	if (component_view->source_list)
		g_object_unref (component_view->source_list);

	if (component_view->source_selection)
		e_source_selector_free_selection (component_view->source_selection);

	for (l = component_view->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (component_view->notifications);

	if (component_view->creatable_items_handler)
		g_object_unref (component_view->creatable_items_handler);

	if (component_view->activity_handler)
		g_object_unref (component_view->activity_handler);

	g_free (component_view);
}

static void
view_destroyed_cb (gpointer data, GObject *where_the_object_was)
{
	TasksComponent *tasks_component = data;
	TasksComponentPrivate *priv;
	GList *l;

	priv = tasks_component->priv;

	for (l = priv->views; l; l = l->next) {
		TasksComponentView *component_view = l->data;

		if (G_OBJECT (component_view->view_control) == where_the_object_was) {
			priv->views = g_list_remove (priv->views, component_view);
			destroy_component_view (component_view);

			break;
		}
	}
}

static GNOME_Evolution_ComponentView
impl_createView (PortableServer_Servant servant,
		 GNOME_Evolution_ShellView parent,
		 CORBA_boolean select_item,
		 CORBA_Environment *ev)
{
	TasksComponent *component = TASKS_COMPONENT (bonobo_object_from_servant (servant));
	TasksComponentPrivate *priv;
	TasksComponentView *component_view;
	EComponentView *ecv;

	priv = component->priv;

	/* Create the calendar component view */
	component_view = create_component_view (component);
	if (!component_view) {
		/* FIXME Should we describe the problem in a control? */
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);

		return CORBA_OBJECT_NIL;
	}

	g_object_weak_ref (G_OBJECT (component_view->view_control), view_destroyed_cb, component);
	priv->views = g_list_append (priv->views, component_view);

	/* TODO: Make TasksComponentView just subclass EComponentView */
	ecv = e_component_view_new_controls (parent, "tasks", component_view->sidebar_control,
					     component_view->view_control, component_view->statusbar_control);

	return BONOBO_OBJREF(ecv);
}

static void
impl_handleURI (PortableServer_Servant servant, const gchar *uri, CORBA_Environment *ev)
{
	TasksComponent *tasks_component = TASKS_COMPONENT (bonobo_object_from_servant (servant));
	TasksComponentPrivate *priv;
	GList *l;
	TasksComponentView *view = NULL;

	gchar *src_uid = NULL;
	gchar *uid = NULL;
	gchar *rid = NULL;

	priv = tasks_component->priv;

	l = g_list_last (priv->views);
	if (!l)
		return;

	view = l->data;

	if (!strncmp (uri, "task:", 5)) {
		EUri *euri = e_uri_new (uri);
		const gchar *p;
		gchar *header, *content;
		gsize len, clen;

		p = euri->query;
		if (p) {
			while (*p) {
				len = strcspn (p, "=&");

				/* If it's malformed, give up. */
				if (p[len] != '=')
					break;

				header = (gchar *) p;
				header[len] = '\0';
				p += len + 1;

				clen = strcspn (p, "&");

				content = g_strndup (p, clen);
				if (!g_ascii_strcasecmp (header, "source-uid")) {
					src_uid = g_strdup (content);
				} else if (!g_ascii_strcasecmp (header, "comp-uid")) {
					uid = g_strdup (content);
				} else if (!g_ascii_strcasecmp (header, "comp-rid")) {
					rid = g_strdup (content);
				}
				g_free (content);

				p += clen;
				if (*p == '&') {
					p++;
					if (!strcmp (p, "amp;"))
						p += 4;
				}
			}

			if (uid && src_uid) {
				e_tasks_open_task_id (view->tasks, src_uid, uid, rid);
			}

			g_free (src_uid);
			g_free (uid);
			g_free (rid);
		}
		e_uri_free (euri);
	}

	return;
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 3;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = (gchar *) CREATE_TASK_ID;
	list->_buffer[0].description = (gchar *) _("New task");
	list->_buffer[0].menuDescription = (gchar *) C_("New", "_Task");
	list->_buffer[0].tooltip = (gchar *) _("Create a new task");
	list->_buffer[0].menuShortcut = 't';
	list->_buffer[0].iconName = (gchar *) "stock_task";
	list->_buffer[0].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[1].id = (gchar *) CREATE_TASK_ASSIGNED_ID;
	list->_buffer[1].description = (gchar *) _("New assigned task");
	list->_buffer[1].menuDescription = (gchar *) C_("New", "Assigne_d Task");
	list->_buffer[1].tooltip = (gchar *) _("Create a new assigned task");
	list->_buffer[1].menuShortcut = '\0';
	list->_buffer[1].iconName = (gchar *) "stock_task";
	list->_buffer[1].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[2].id = (gchar *) CREATE_TASK_LIST_ID;
	list->_buffer[2].description = (gchar *) _("New task list");
	list->_buffer[2].menuDescription = (gchar *) C_("New", "Tas_k list");
	list->_buffer[2].tooltip = (gchar *) _("Create a new task list");
	list->_buffer[2].menuShortcut = '\0';
	list->_buffer[2].iconName = (gchar *) "stock_todo";
	list->_buffer[2].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	TasksComponent *tasks_component = TASKS_COMPONENT (bonobo_object_from_servant (servant));

	if (strcmp (item_type_name, CREATE_TASK_ID) == 0) {
		if (!create_new_todo (tasks_component, FALSE, NULL))
			bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
	} else if (strcmp (item_type_name, CREATE_TASK_ASSIGNED_ID) == 0) {
		if (!create_new_todo (tasks_component, TRUE, NULL))
			bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
	} else if (strcmp (item_type_name, CREATE_TASK_LIST_ID) == 0) {
		/* FIXME Should we use the last opened window? */
		calendar_setup_new_task_list (NULL);
	} else {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);
	}
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	TasksComponent *tasks_component = TASKS_COMPONENT (object);
	TasksComponentPrivate *priv = tasks_component->priv;
	GList *l;

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}
	if (priv->source_selection != NULL) {
		e_source_selector_free_selection (priv->source_selection);
		priv->source_selection = NULL;
	}

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}

	for (l = priv->views; l; l = l->next) {
		TasksComponentView *component_view = l->data;

		g_object_weak_unref (G_OBJECT (component_view->view_control), view_destroyed_cb, tasks_component);
	}
	g_list_free (priv->views);
	priv->views = NULL;

	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (priv->notifications);
	priv->notifications = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	TasksComponentPrivate *priv = TASKS_COMPONENT (object)->priv;
	GList *l;

	for (l = priv->views; l; l = l->next) {
		TasksComponentView *component_view = l->data;

		destroy_component_view (component_view);
	}
	g_list_free (priv->views);

	g_free (priv->base_directory);
	g_free (priv->config_directory);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
tasks_component_class_init (TasksComponentClass *klass)
{
	POA_GNOME_Evolution_Component__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	parent_class = g_type_class_peek_parent (klass);

	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->createView		     = impl_createView;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;
	epv->handleURI               = impl_handleURI;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
tasks_component_init (TasksComponent *component, TasksComponentClass *klass)
{
	TasksComponentPrivate *priv;

	priv = g_new0 (TasksComponentPrivate, 1);

	priv->base_directory = g_build_filename (e_get_user_data_dir (), "tasks", NULL);
	priv->config_directory = g_build_filename (priv->base_directory, "config", NULL);

	component->priv = priv;
	ensure_sources (component);
}

/* Public API */

TasksComponent *
tasks_component_peek (void)
{
	static TasksComponent *component = NULL;

	if (component == NULL) {
		component = g_object_new (tasks_component_get_type (), NULL);

		if (g_mkdir_with_parents (component->priv->config_directory, 0777) != 0) {
			g_warning (G_STRLOC ": Cannot create directory %s: %s",
				   component->priv->config_directory, g_strerror (errno));
			g_object_unref (component);
			component = NULL;
		}
	}

	return component;
}

const gchar *
tasks_component_peek_base_directory (TasksComponent *component)
{
	return component->priv->base_directory;
}

const gchar *
tasks_component_peek_config_directory (TasksComponent *component)
{
	return component->priv->config_directory;
}

ESourceList *
tasks_component_peek_source_list (TasksComponent *component)
{
	return component->priv->source_list;
}

BONOBO_TYPE_FUNC_FULL (TasksComponent, GNOME_Evolution_Component, PARENT_TYPE, tasks_component)
