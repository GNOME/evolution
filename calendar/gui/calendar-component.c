/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-component.c
 *
 * Copyright (C) 2003  Novell, Inc.
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
 * Authors: Ettore Perazzoli <ettore@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <libical/icalvcal.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserverui/e-source-selector.h>
#include <shell/e-user-creatable-items-handler.h>
#include "e-pub-utils.h"
#include "e-calendar-view.h"
#include "calendar-config-keys.h"
#include "calendar-config.h"
#include "calendar-component.h"
#include "calendar-commands.h"
#include "control-factory.h"
#include "gnome-cal.h"
#include "migration.h"
#include "e-comp-editor-registry.h"
#include "comp-util.h"
#include "common/authentication.h"
#include "dialogs/calendar-setup.h"
#include "dialogs/comp-editor.h"
#include "dialogs/copy-source-dialog.h"
#include "dialogs/event-editor.h"
#include "widgets/misc/e-info-label.h"
#include "widgets/misc/e-error.h"
#include "e-util/e-icon-factory.h"
#include "e-cal-menu.h"
#include "e-cal-popup.h"

/* IDs for user creatable items */
#define CREATE_EVENT_ID        "event"
#define CREATE_MEETING_ID      "meeting"
#define CREATE_ALLDAY_EVENT_ID "allday-event"
#define CREATE_CALENDAR_ID      "calendar"

enum DndTargetType {
	DND_TARGET_TYPE_CALENDAR_LIST,
};
#define CALENDAR_TYPE "text/calendar"
#define XCALENDAR_TYPE "text/x-calendar"
static GtkTargetEntry drag_types[] = {
	{ CALENDAR_TYPE, 0, DND_TARGET_TYPE_CALENDAR_LIST },
	{ XCALENDAR_TYPE, 0, DND_TARGET_TYPE_CALENDAR_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

typedef struct 
{
	ESourceList *source_list;
	ESourceList *task_source_list;
	
	GSList *source_selection;
	GSList *task_source_selection;
	
	GnomeCalendar *calendar;

	EInfoLabel *info_label;
	GtkWidget *source_selector;

	BonoboControl *view_control;
	BonoboControl *sidebar_control;
	BonoboControl *statusbar_control;

	GList *notifications;

	EUserCreatableItemsHandler *creatable_items_handler;

	EActivityHandler *activity_handler;
} CalendarComponentView;

struct _CalendarComponentPrivate {
	char *base_directory;
	char *config_directory;

	GConfClient *gconf_client;
	int gconf_notify_id;

	ESourceList *source_list;
	ESourceList *task_source_list;

	GList *views;

	ECal *create_ecal;

	GList *notifications;
};

/* FIXME This should be gnome cal likely */
extern ECompEditorRegistry *comp_editor_registry;

/* Utility functions.  */

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
		const char *uid = l->data;
		
		if (!strcmp (uid, e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
update_uris_for_selection (CalendarComponentView *component_view)
{
	GSList *selection, *l, *uids_selected = NULL;
	
	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));

	for (l = component_view->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			gnome_calendar_remove_source (component_view->calendar, E_CAL_SOURCE_TYPE_EVENT, old_selected_source);
	}

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		if (gnome_calendar_add_source (component_view->calendar, E_CAL_SOURCE_TYPE_EVENT, selected_source))
			uids_selected = g_slist_append (uids_selected, (char *) e_source_peek_uid (selected_source));
	}
	
	e_source_selector_free_selection (component_view->source_selection);
	component_view->source_selection = selection;

	/* Save the selection for next time we start up */
	calendar_config_set_calendars_selected (uids_selected);
	g_slist_free (uids_selected);
}

static void
update_uri_for_primary_selection (CalendarComponentView *component_view)
{
	ESource *source;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!source)
		return;

	/* Set the default */
	gnome_calendar_set_default_source (component_view->calendar, E_CAL_SOURCE_TYPE_EVENT, source);

	/* Make sure we are embedded first */
	calendar_control_sensitize_calendar_commands (component_view->view_control, component_view->calendar, TRUE);

	/* Save the selection for next time we start up */
	calendar_config_set_primary_calendar (e_source_peek_uid (source));
}

static void
update_selection (CalendarComponentView *component_view)
{
	GSList *selection, *uids_selected, *l;

	/* Get the selection in gconf */
	uids_selected = calendar_config_get_calendars_selected ();

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
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (component_view->source_list, uid);
		if (source) 
			e_source_selector_select_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
		
		g_free (uid);
	}
	g_slist_free (uids_selected);
}

static void
update_task_selection (CalendarComponentView *component_view)
{
	GSList *uids_selected, *l;

	/* Get the selection in gconf */
	uids_selected = calendar_config_get_tasks_selected ();

	/* Remove any that aren't there any more */
	for (l = component_view->task_source_selection; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (component_view->task_source_list, uid);
		if (!source)
			gnome_calendar_remove_source_by_uid (component_view->calendar, E_CAL_SOURCE_TYPE_TODO, uid);
		else if (!is_in_uids (uids_selected, source))
			gnome_calendar_remove_source (component_view->calendar, E_CAL_SOURCE_TYPE_TODO, source);
		
		g_free (uid);
	}
	g_slist_free (component_view->task_source_selection);

	/* Make sure the whole selection is there */
	for (l = uids_selected; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (component_view->task_source_list, uid);
		if (!gnome_calendar_add_source (component_view->calendar, E_CAL_SOURCE_TYPE_TODO, source))
			/* FIXME do something */;
	}

	component_view->task_source_selection = uids_selected;
}

static void
update_primary_selection (CalendarComponentView *component_view)
{
	ESource *source = NULL;
	char *uid;

	uid = calendar_config_get_primary_calendar ();
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

static void
update_primary_task_selection (CalendarComponentView *component_view)
{
	ESource *source = NULL;
	char *uid;

	uid = calendar_config_get_primary_tasks ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (component_view->task_source_list, uid);

		g_free (uid);
	}
	
	if (source)
		gnome_calendar_set_default_source (component_view->calendar, E_CAL_SOURCE_TYPE_TODO, source);
}

/* Callbacks.  */
static void
copy_calendar_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	CalendarComponentView *component_view = data;
	ESource *selected_source;
	
	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	copy_source_dialog (GTK_WINDOW (gtk_widget_get_toplevel (ep->target->widget)), selected_source, E_CAL_SOURCE_TYPE_EVENT);
}

static void
delete_calendar_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	CalendarComponentView *component_view = data;
	ESource *selected_source;
	ECal *cal;
	char *uri;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	if (e_error_run((GtkWindow *)gtk_widget_get_toplevel(ep->target->widget),
			"calendar:prompt-delete-calendar", e_source_peek_name(selected_source)) != GTK_RESPONSE_YES)
		return;

	/* first, ask the backend to remove the calendar */
	uri = e_source_get_uri (selected_source);
	cal = e_cal_model_get_client_for_uri (gnome_calendar_get_calendar_model (component_view->calendar), uri);
	if (!cal)
		cal = e_cal_new_from_uri (uri, E_CAL_SOURCE_TYPE_EVENT);
	g_free (uri);
	if (cal) {
		if (e_cal_remove (cal, NULL)) {
			if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (component_view->source_selector),
								  selected_source)) {
				gnome_calendar_remove_source (component_view->calendar, E_CAL_SOURCE_TYPE_EVENT, selected_source);
				e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector),
								   selected_source);
			}
			
			e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);
			e_source_list_sync (component_view->source_list, NULL);
		}
	}
}

static void
new_calendar_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	calendar_setup_new_calendar (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)));
}

static void
edit_calendar_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	CalendarComponentView *component_view = data;
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	calendar_setup_edit_calendar (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)), selected_source);
}

static EPopupItem ecc_source_popups[] = {
	{ E_POPUP_ITEM, "10.new", N_("New Calendar"), new_calendar_cb, NULL, "stock_calendar", 0, 0 },
	{ E_POPUP_ITEM, "15.copy", N_("Copy"), copy_calendar_cb, NULL, "stock_folder-copy", 0, E_CAL_POPUP_SOURCE_PRIMARY },
	{ E_POPUP_ITEM, "20.delete", N_("Delete"), delete_calendar_cb, NULL, "stock_delete", 0, E_CAL_POPUP_SOURCE_USER|E_CAL_POPUP_SOURCE_PRIMARY },
	{ E_POPUP_ITEM, "30.properties", N_("Properties..."), edit_calendar_cb, NULL, "stock_folder-properties", 0, E_CAL_POPUP_SOURCE_PRIMARY },
};

static void
ecc_source_popup_free(EPopup *ep, GSList *list, void *data)
{
	g_slist_free(list);
}

static gboolean
popup_event_cb(ESourceSelector *selector, ESource *insource, GdkEventButton *event, CalendarComponentView *component_view)
{
	ECalPopup *ep;
	ECalPopupTargetSource *t;
	GSList *menus = NULL;
	int i;
	GtkMenu *menu;

	/** @HookPoint-ECalPopup: Calendar Source Selector Context Menu
	 * @Id: org.gnome.evolution.calendar.source.popup
	 * @Class: org.gnome.evolution.calendar.popup:1.0
	 * @Target: ECalPopupTargetSource
	 *
	 * The context menu on the source selector in the calendar window.
	 */
	ep = e_cal_popup_new("org.gnome.evolution.calendar.source.popup");
	t = e_cal_popup_target_new_source(ep, selector);
	t->target.widget = (GtkWidget *)component_view->calendar;

	for (i=0;i<sizeof(ecc_source_popups)/sizeof(ecc_source_popups[0]);i++)
		menus = g_slist_prepend(menus, &ecc_source_popups[i]);

	e_popup_add_items((EPopup *)ep, menus, ecc_source_popup_free, component_view);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event?event->button:0, event?event->time:gtk_get_current_event_time());

	return TRUE;
}

static void
source_selection_changed_cb (ESourceSelector *selector, CalendarComponentView *component_view)
{
	update_uris_for_selection (component_view);
}

static void
primary_source_selection_changed_cb (ESourceSelector *selector, CalendarComponentView *component_view)
{
	update_uri_for_primary_selection (component_view);
}

static void
source_added_cb (GnomeCalendar *calendar, ECalSourceType source_type, ESource *source, CalendarComponentView *component_view)
{
	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		e_source_selector_select_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
		break;
	default:
		break;
	}
}

static void
source_removed_cb (GnomeCalendar *calendar, ECalSourceType source_type, ESource *source, CalendarComponentView *component_view)
{
	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
		break;
	default:
		break;
	}
}

static void
set_info (CalendarComponentView *component_view)
{
	icaltimezone *zone;
	struct icaltimetype start_tt, end_tt;
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	char buffer[512], end_buffer[256];
	GnomeCalendarViewType view;

	gnome_calendar_get_visible_time_range (component_view->calendar, &start_time, &end_time);
	zone = gnome_calendar_get_timezone (component_view->calendar);

	start_tt = icaltime_from_timet_with_zone (start_time, FALSE, zone);
	start_tm.tm_year = start_tt.year - 1900;
	start_tm.tm_mon = start_tt.month - 1;
	start_tm.tm_mday = start_tt.day;
	start_tm.tm_hour = start_tt.hour;
	start_tm.tm_min = start_tt.minute;
	start_tm.tm_sec = start_tt.second;
	start_tm.tm_isdst = -1;
	start_tm.tm_wday = time_day_of_week (start_tt.day, start_tt.month - 1,
					     start_tt.year);

	/* Take one off end_time so we don't get an extra day. */
	end_tt = icaltime_from_timet_with_zone (end_time - 1, FALSE, zone);
	end_tm.tm_year = end_tt.year - 1900;
	end_tm.tm_mon = end_tt.month - 1;
	end_tm.tm_mday = end_tt.day;
	end_tm.tm_hour = end_tt.hour;
	end_tm.tm_min = end_tt.minute;
	end_tm.tm_sec = end_tt.second;
	end_tm.tm_isdst = -1;
	end_tm.tm_wday = time_day_of_week (end_tt.day, end_tt.month - 1,
					   end_tt.year);

	view = gnome_calendar_get_view (component_view->calendar);

	switch (view) {
	case GNOME_CAL_DAY_VIEW:
	case GNOME_CAL_WORK_WEEK_VIEW:
	case GNOME_CAL_WEEK_VIEW:
		if (start_tm.tm_year == end_tm.tm_year
		    && start_tm.tm_mon == end_tm.tm_mon
		    && start_tm.tm_mday == end_tm.tm_mday) {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%A %d %b %Y"), &start_tm);
		} else if (start_tm.tm_year == end_tm.tm_year) {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%a %d %b"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer),
				  _("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		} else {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%a %d %b %Y"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer),
				  _("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	case GNOME_CAL_MONTH_VIEW:
	case GNOME_CAL_LIST_VIEW:
		if (start_tm.tm_year == end_tm.tm_year) {
			if (start_tm.tm_mon == end_tm.tm_mon) {
				e_utf8_strftime (buffer, sizeof (buffer),
					  "%d", &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer),
					  _("%d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			} else {
				e_utf8_strftime (buffer, sizeof (buffer),
					  _("%d %b"), &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer),
					  _("%d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
		} else {
			e_utf8_strftime (buffer, sizeof (buffer),
				  _("%d %b %Y"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer),
				  _("%d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	default:
		g_assert_not_reached ();
	}

	e_info_label_set_info (component_view->info_label, _("Calendar"), buffer);
}

static void
calendar_dates_changed_cb (GnomeCalendar *calendar, CalendarComponentView *component_view)
{
	set_info (component_view);
}

static void
config_primary_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	CalendarComponent *calendar_component = data;
	CalendarComponentPrivate *priv = calendar_component->priv;
	
	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}
}

static void
config_tasks_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_task_selection (data);
}


static void
config_primary_tasks_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_primary_task_selection (data);
}

static gboolean
init_calendar_publishing_cb (gpointer data)
{	
	/* Publish if it is time to publish again */
	e_pub_publish (FALSE);

	return FALSE;
}

static void
conf_changed_callback (GConfClient *client,
		       unsigned int connection_id,
		       GConfEntry *entry,
		       void *user_data)
{
	/* publish config changed, so publish */
	e_pub_publish (TRUE);
}



/* Evolution::Component CORBA methods.  */

static void
impl_upgradeFromVersion (PortableServer_Servant servant,
			 CORBA_short major,
			 CORBA_short minor,
			 CORBA_short revision,
			 CORBA_Environment *ev)
{
	GError *err = NULL;
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));

	if (!migrate_calendars (calendar_component, major, minor, revision, &err)) {
		GNOME_Evolution_Component_UpgradeFailed *failedex;

		failedex = GNOME_Evolution_Component_UpgradeFailed__alloc();
		failedex->what = CORBA_string_dup(_("Failed upgrading calendars."));
		failedex->why = CORBA_string_dup(err->message);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UpgradeFailed, failedex);
	}

	if (err)
		g_error_free(err);
}

static gboolean
selector_tree_drag_drop (GtkWidget *widget, 
			 GdkDragContext *context, 
			 int x, 
			 int y, 
			 guint time, 
			 CalendarComponent *component)
{
	GtkTreeViewColumn *column;
	int cell_x;
	int cell_y;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer data;
	
	if (!gtk_tree_view_get_path_at_pos  (GTK_TREE_VIEW (widget), x, y, &path, 
					     &column, &cell_x, &cell_y))
		return FALSE;
	
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, 0, &data, -1);
	
	if (E_IS_SOURCE_GROUP (data)) {
		g_object_unref (data);
		gtk_tree_path_free (path);
		return FALSE;
	}
	
	gtk_tree_path_free (path);
	return TRUE;
}
	
static gboolean
selector_tree_drag_motion (GtkWidget *widget,
			   GdkDragContext *context,
			   int x,
			   int y,
			   guint time,
			   gpointer user_data)
{
	GtkTreePath *path = NULL;
	gpointer data = NULL;
	GtkTreeViewDropPosition pos;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkDragAction action = GDK_ACTION_DEFAULT;
	
	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;
	
	gtk_tree_model_get (model, &iter, 0, &data, -1);

	if (E_IS_SOURCE_GROUP (data) || e_source_get_readonly (data))
		goto finish;
	
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
	action = context->suggested_action;

 finish:
	if (path)
		gtk_tree_path_free (path);
	if (data)
		g_object_unref (data);

	gdk_drag_status (context, action, time);
	return TRUE;
}

static gboolean
update_single_object (ECal *client, icalcomponent *icalcomp)
{
	char *uid;
	icalcomponent *tmp_icalcomp;

	uid = (char *) icalcomponent_get_uid (icalcomp);
	
	if (e_cal_get_object (client, uid, NULL, &tmp_icalcomp, NULL))
		return e_cal_modify_object (client, icalcomp, CALOBJ_MOD_ALL, NULL);

	return e_cal_create_object (client, icalcomp, &uid, NULL);	
}

static gboolean
update_objects (ECal *client, icalcomponent *icalcomp)
{
	icalcomponent *subcomp;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VTODO_COMPONENT || kind == ICAL_VEVENT_COMPONENT)
		return update_single_object (client, icalcomp);
	else if (kind != ICAL_VCALENDAR_COMPONENT)
		return FALSE;

	subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
	while (subcomp) {
		gboolean success;
		
		kind = icalcomponent_isa (subcomp);
		if (kind == ICAL_VTIMEZONE_COMPONENT) {
			icaltimezone *zone;

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);

			success = e_cal_add_timezone (client, zone, NULL);
			icaltimezone_free (zone, 1);
			if (!success)
				return success;
		} else if (kind == ICAL_VTODO_COMPONENT ||
			   kind == ICAL_VEVENT_COMPONENT) {
			success = update_single_object (client, subcomp);
			if (!success)
				return success;
		}

		subcomp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);
	}

	return TRUE;
}

static void
selector_tree_drag_data_received (GtkWidget *widget, 
				  GdkDragContext *context, 
				  gint x, 
				  gint y, 
				  GtkSelectionData *data,
				  guint info,
				  guint time,
				  gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	gpointer source = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean success = FALSE;
	icalcomponent *icalcomp = NULL;
	ECal *client = NULL;

	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;
       
	
	gtk_tree_model_get (model, &iter, 0, &source, -1);

	if (E_IS_SOURCE_GROUP (source) || e_source_get_readonly (source))
		goto finish;

	icalcomp = icalparser_parse_string (data->data);
	
	if (icalcomp) {
		char * uid;

		/* FIXME deal with GDK_ACTION_ASK */
		if (context->action == GDK_ACTION_COPY) {
			uid = e_cal_component_gen_uid ();
			icalcomponent_set_uid (icalcomp, uid);
		}

		client = auth_new_cal_from_source (source, 
						   E_CAL_SOURCE_TYPE_EVENT);
		
		if (client) {
			if (e_cal_open (client, TRUE, NULL)) {
				success = TRUE;
				update_objects (client, icalcomp);
			}
			
			g_object_unref (client);
		}
		
		icalcomponent_free (icalcomp);
	}

 finish:
	if (source)
		g_object_unref (source);
	if (path)
		gtk_tree_path_free (path);

	gtk_drag_finish (context, success, context->action == GDK_ACTION_MOVE, time);
}	

static void
selector_tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), 
					NULL, GTK_TREE_VIEW_DROP_BEFORE);
}

static void
control_activate_cb (BonoboControl *control, gboolean activate, gpointer data)
{
	CalendarComponentView *component_view = data;

	if (activate) {
		BonoboUIComponent *uic;
		uic = bonobo_control_get_ui_component (component_view->view_control);
		
		e_user_creatable_items_handler_activate (component_view->creatable_items_handler, uic);
	}	
}


static void
config_create_ecal_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{	
	CalendarComponent *calendar_component = data;
	CalendarComponentPrivate *priv;
	
	priv = calendar_component->priv;

	g_object_unref (priv->create_ecal);
	priv->create_ecal = NULL;
	
	priv->notifications = g_list_remove (priv->notifications, GUINT_TO_POINTER (id));
}

static ECal *
setup_create_ecal (CalendarComponent *calendar_component, CalendarComponentView *component_view) 
{
	CalendarComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;
	guint not;
	
	priv = calendar_component->priv;

	/* Try to use the client from the calendar first to avoid re-opening things */
	if (component_view) {
		ECal *default_ecal;
		
		default_ecal = gnome_calendar_get_default_client (component_view->calendar);
		if (default_ecal)
			return default_ecal;
	}
	
	/* If there is an existing fall back, use that */
	if (priv->create_ecal)
		return priv->create_ecal; 

	/* Get the current primary calendar, or try to set one if it doesn't already exist */		
	uid = calendar_config_get_primary_calendar ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);

		priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	} 

	if (!priv->create_ecal) {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	}
		
	if (priv->create_ecal) {
		icaltimezone *zone;

		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;
			
			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the calendar '%s' for creating events and meetings"), 
							   e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return NULL;
		}

		zone = calendar_config_get_icaltimezone ();
		e_cal_set_default_timezone (priv->create_ecal, zone, NULL);

	} else {
		GtkWidget *dialog;
			
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 _("There is no calendar available for creating events and meetings"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}		

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_calendar (config_create_ecal_changed_cb, 
								 calendar_component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_calendar (e_source_peek_uid (source));

	return priv->create_ecal;
}

static gboolean
create_new_event (CalendarComponent *calendar_component, CalendarComponentView *component_view, gboolean is_allday, gboolean is_meeting)
{
	ECal *ecal;
	ECalendarView *view;

	ecal = setup_create_ecal (calendar_component, component_view);
	if (!ecal)
		return FALSE;

	if (component_view && (view = E_CALENDAR_VIEW (gnome_calendar_get_current_view_widget (component_view->calendar)))) {
		GnomeCalendarViewType view_type;

		/* Force all for these view types because thats what's selected and it mimics a double click */
		view_type = gnome_calendar_get_view (component_view->calendar);
		if (view_type == GNOME_CAL_WEEK_VIEW 
		    || view_type == GNOME_CAL_MONTH_VIEW
		    || view_type == GNOME_CAL_LIST_VIEW)
			is_allday = TRUE;

		e_calendar_view_new_appointment_full (view, is_allday, is_meeting);
	} else {
		ECalComponent *comp;
		EventEditor *editor;

		editor = event_editor_new (ecal, is_meeting);
		comp = cal_comp_event_new_with_current_time (ecal, is_allday);
		e_cal_component_commit_sequence (comp);

		comp_editor_edit_comp (COMP_EDITOR (editor), comp);
		if (is_meeting)
			event_editor_show_meeting (editor);
		comp_editor_focus (COMP_EDITOR (editor));

		e_comp_editor_registry_add (comp_editor_registry, COMP_EDITOR (editor), TRUE);
	}

	return TRUE;	
}

static void
create_local_item_cb (EUserCreatableItemsHandler *handler, const char *item_type_name, void *data)
{
	CalendarComponent *calendar_component = data;
	CalendarComponentPrivate *priv;
	CalendarComponentView *component_view = NULL;
	GList *l;
	
	priv = calendar_component->priv;
	
	for (l = priv->views; l; l = l->next) {
		component_view = l->data;

		if (component_view->creatable_items_handler == handler)
			break;
		
		component_view = NULL;
	}
	
	if (strcmp (item_type_name, CREATE_EVENT_ID) == 0)
		create_new_event (calendar_component, component_view, FALSE, FALSE);
 	else if (strcmp (item_type_name, CREATE_ALLDAY_EVENT_ID) == 0)
		create_new_event (calendar_component, component_view, TRUE, FALSE);
	else if (strcmp (item_type_name, CREATE_MEETING_ID) == 0)
		create_new_event (calendar_component, component_view, FALSE, TRUE);
	else if (strcmp (item_type_name, CREATE_CALENDAR_ID) == 0)
		calendar_setup_new_calendar (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (component_view->calendar))));
}

static CalendarComponentView *
create_component_view (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	CalendarComponentView *component_view;
	GtkWidget *selector_scrolled_window, *vbox;
	GtkWidget *statusbar_widget;
	guint not;
	
	priv = calendar_component->priv;

	/* Create the calendar component view */
	component_view = g_new0 (CalendarComponentView, 1);
	
	/* Add the source lists */
	component_view->source_list = g_object_ref (priv->source_list);
	component_view->task_source_list = g_object_ref (priv->task_source_list);
	
	/* Create sidebar selector */
	component_view->source_selector = e_source_selector_new (calendar_component->priv->source_list);
	e_source_selector_set_select_new ((ESourceSelector *)component_view->source_selector, TRUE);

	g_signal_connect (component_view->source_selector, "drag-motion", G_CALLBACK (selector_tree_drag_motion), 
			  calendar_component);
	g_signal_connect (component_view->source_selector, "drag-leave", G_CALLBACK (selector_tree_drag_leave), 
			  calendar_component);
	g_signal_connect (component_view->source_selector, "drag-drop", G_CALLBACK (selector_tree_drag_drop), 
			  calendar_component);
	g_signal_connect (component_view->source_selector, "drag-data-received", 
			  G_CALLBACK (selector_tree_drag_data_received), calendar_component);

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

	component_view->info_label = (EInfoLabel *)e_info_label_new("stock_calendar");
	e_info_label_set_info (component_view->info_label, _("Calendars"), "");
	gtk_widget_show (GTK_WIDGET (component_view->info_label));

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), GTK_WIDGET (component_view->info_label), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), selector_scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	component_view->sidebar_control = bonobo_control_new (vbox);

	/* Create main view */
	component_view->view_control = control_factory_new_control ();
	if (!component_view->view_control) {
		/* FIXME free memory */

		return NULL;
	}

	component_view->calendar = (GnomeCalendar *) bonobo_control_get_widget (component_view->view_control);

	/* This signal is thrown if backends die - we update the selector */
	g_signal_connect (component_view->calendar, "source_added", 
			  G_CALLBACK (source_added_cb), component_view);
	g_signal_connect (component_view->calendar, "source_removed", 
			  G_CALLBACK (source_removed_cb), component_view);

	/* Create status bar */
	statusbar_widget = e_task_bar_new ();
	component_view->activity_handler = e_activity_handler_new ();
	e_activity_handler_attach_task_bar (component_view->activity_handler, E_TASK_BAR (statusbar_widget));
	gtk_widget_show (statusbar_widget);

	component_view->statusbar_control = bonobo_control_new (statusbar_widget);
	
	gnome_calendar_set_activity_handler (component_view->calendar, component_view->activity_handler);
	
	/* connect after setting the initial selections, or we'll get unwanted calls
	   to calendar_control_sensitize_calendar_commands */
	g_signal_connect (component_view->source_selector, "selection_changed",
			  G_CALLBACK (source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "primary_selection_changed",
			  G_CALLBACK (primary_source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "popup_event",
			  G_CALLBACK (popup_event_cb), component_view);

	/* Set up the "new" item handler */
	component_view->creatable_items_handler = e_user_creatable_items_handler_new ("calendar", create_local_item_cb, calendar_component);
	g_signal_connect (component_view->view_control, "activate", G_CALLBACK (control_activate_cb), component_view);

	/* We use this to update the component information */
	set_info (component_view);
	g_signal_connect (component_view->calendar, "dates_shown_changed",
			  G_CALLBACK (calendar_dates_changed_cb), component_view);

	/* Load the selection from the last run */
	update_selection (component_view);	
	update_primary_selection (component_view);
	update_task_selection (component_view);
	update_primary_task_selection (component_view);

	/* If the tasks selection changes elsewhere, update it for the mini
	   mini tasks view sidebar */
	not = calendar_config_add_notification_tasks_selected (config_tasks_selection_changed_cb, 
							       component_view);
	component_view->notifications = g_list_prepend (component_view->notifications, GUINT_TO_POINTER (not));

	not = calendar_config_add_notification_primary_tasks (config_primary_tasks_selection_changed_cb, 
							      component_view);
	component_view->notifications = g_list_prepend (component_view->notifications, GUINT_TO_POINTER (not));

	return component_view;
}

static void
destroy_component_view (CalendarComponentView *component_view)
{	
	GList *l;
	
	if (component_view->source_list)
		g_object_unref (component_view->source_list);

	if (component_view->task_source_list)
		g_object_unref (component_view->task_source_list);

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
	CalendarComponent *calendar_component = data;
	CalendarComponentPrivate *priv;
	GList *l;
	
	priv = calendar_component->priv;

	for (l = priv->views; l; l = l->next) {
		CalendarComponentView *component_view = l->data;
		
		if (G_OBJECT (component_view->view_control) == where_the_object_was) {
			priv->views = g_list_remove (priv->views, component_view);
			destroy_component_view (component_view);

			break;
		}
	}
}

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     Bonobo_Control *corba_statusbar_control,
		     CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));
	CalendarComponentPrivate *priv;
	CalendarComponentView *component_view;
	
	priv = calendar_component->priv;

	/* Create the calendar component view */
	component_view = create_component_view (calendar_component);
	if (!component_view) {
		/* FIXME Should we describe the problem in a control? */
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);

		return;
	}

	g_object_weak_ref (G_OBJECT (component_view->view_control), view_destroyed_cb, calendar_component);
	priv->views = g_list_append (priv->views, component_view);
	
	/* Return the controls */
	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (component_view->sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (component_view->view_control), ev);
	*corba_statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (component_view->statusbar_control), ev);
}


static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 4;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = CREATE_EVENT_ID;
	list->_buffer[0].description = _("New appointment");
	list->_buffer[0].menuDescription = _("_Appointment");
	list->_buffer[0].tooltip = _("Create a new appointment");
	list->_buffer[0].menuShortcut = 'a';
	list->_buffer[0].iconName = "stock_new-appointment";
	list->_buffer[0].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[1].id = CREATE_MEETING_ID;
	list->_buffer[1].description = _("New meeting");
	list->_buffer[1].menuDescription = _("M_eeting");
	list->_buffer[1].tooltip = _("Create a new meeting request");
	list->_buffer[1].menuShortcut = 'e';
	list->_buffer[1].iconName = "stock_new-meeting";
	list->_buffer[1].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[2].id = CREATE_ALLDAY_EVENT_ID;
	list->_buffer[2].description = _("New all day appointment");
	list->_buffer[2].menuDescription = _("All Day A_ppointment");
	list->_buffer[2].tooltip = _("Create a new all-day appointment");
	list->_buffer[2].menuShortcut = 'p';
	list->_buffer[2].iconName = "stock_new-24h-appointment";
	list->_buffer[2].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[3].id = CREATE_CALENDAR_ID;
	list->_buffer[3].description = _("New calendar");
	list->_buffer[3].menuDescription = _("Cale_ndar");
	list->_buffer[3].tooltip = _("Create a new calendar");
	list->_buffer[3].menuShortcut = 'n';
	list->_buffer[3].iconName = "stock_calendar";
	list->_buffer[3].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));
	gboolean result = FALSE;
	
	if (strcmp (item_type_name, CREATE_EVENT_ID) == 0)
		result = create_new_event (calendar_component, NULL, FALSE, FALSE);
 	else if (strcmp (item_type_name, CREATE_ALLDAY_EVENT_ID) == 0)
		result = create_new_event (calendar_component, NULL, TRUE, FALSE);
	else if (strcmp (item_type_name, CREATE_MEETING_ID) == 0)
		result = create_new_event (calendar_component, NULL, FALSE, TRUE);
	else if (strcmp (item_type_name, CREATE_CALENDAR_ID) == 0)
		/* FIXME Should we use the last opened window? */
		calendar_setup_new_calendar (NULL);
	else
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);

	if (!result)
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (object);
	CalendarComponentPrivate *priv = calendar_component->priv;
	GList *l;
	
	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}

	for (l = priv->views; l; l = l->next) {
		CalendarComponentView *component_view = l->data;
	
		g_object_weak_unref (G_OBJECT (component_view->view_control), view_destroyed_cb, calendar_component);
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
	CalendarComponentPrivate *priv = CALENDAR_COMPONENT (object)->priv;
	GList *l;
	
	for (l = priv->views; l; l = l->next) {
		CalendarComponentView *component_view = l->data;
		
		destroy_component_view (component_view);
	}
	g_list_free (priv->views);
	
	g_free (priv->base_directory);
	g_free (priv->config_directory);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
calendar_component_class_init (CalendarComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->createControls          = impl_createControls;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
calendar_component_init (CalendarComponent *component)
{
	CalendarComponentPrivate *priv;
	guint not;
	
	priv = g_new0 (CalendarComponentPrivate, 1);

	priv->base_directory = g_build_filename (g_get_home_dir (), ".evolution", NULL);
	priv->config_directory = g_build_filename (g_get_home_dir (),
						   ".evolution", "calendar", "config",
						   NULL);

	/* EPFIXME: Should use a custom one instead?  Also we should add
	 * calendar_component_peek_gconf_client().  */
	priv->gconf_client = gconf_client_get_default ();

	
	if (!e_cal_get_sources (&priv->source_list, E_CAL_SOURCE_TYPE_EVENT, NULL))
		;
	
	if (!e_cal_get_sources (&priv->task_source_list, E_CAL_SOURCE_TYPE_TODO, NULL))
		;

	not = calendar_config_add_notification_primary_calendar (config_primary_selection_changed_cb, 
								 component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));


	component->priv = priv;
}


/* Public API.  */

CalendarComponent *
calendar_component_peek (void)
{
	static CalendarComponent *component = NULL;

	if (component == NULL) {
		component = g_object_new (calendar_component_get_type (), NULL);

		if (e_mkdir_hier (calendar_component_peek_config_directory (component), 0777) != 0) {
			g_warning (G_STRLOC ": Cannot create directory %s: %s",
				   calendar_component_peek_config_directory (component),
				   g_strerror (errno));
			g_object_unref (component);
			component = NULL;
		}
	}

	return component;
}

const char *
calendar_component_peek_base_directory (CalendarComponent *component)
{
	return component->priv->base_directory;
}

const char *
calendar_component_peek_config_directory (CalendarComponent *component)
{
	return component->priv->config_directory;
}

ESourceList *
calendar_component_peek_source_list (CalendarComponent *component)
{
	return component->priv->source_list;
}

void
calendar_component_init_publishing (void)
{
	guint idle_id = 0;
	CalendarComponent *calendar_component;
	CalendarComponentPrivate *priv;
	
	calendar_component = calendar_component_peek ();
	
	priv = calendar_component->priv;
	
	gconf_client_add_dir (priv->gconf_client, CALENDAR_CONFIG_PUBLISH, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	priv->gconf_notify_id
		= gconf_client_notify_add (priv->gconf_client, CALENDAR_CONFIG_PUBLISH,
					   (GConfClientNotifyFunc) conf_changed_callback, NULL,
					   NULL, NULL);
	
	idle_id = g_idle_add ((GSourceFunc) init_calendar_publishing_cb, GINT_TO_POINTER (idle_id));
}

BONOBO_TYPE_FUNC_FULL (CalendarComponent, GNOME_Evolution_Component, PARENT_TYPE, calendar_component)
