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
#include "widgets/misc/e-source-selector.h"
#include "widgets/misc/e-info-label.h"
#include "e-util/e-icon-factory.h"

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

struct _CalendarComponentPrivate {
	char *base_directory;
	char *config_directory;

	GConfClient *gconf_client;
	int gconf_notify_id;

	ESourceList *source_list;
	GSList *source_selection;

	ESourceList *task_source_list;
	GSList *task_source_selection;
	
	GnomeCalendar *calendar;
	GtkWidget *source_selector;

	BonoboControl *view_control;

	ECal *create_ecal;

	GList *notifications;

	EActivityHandler *activity_handler;
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
update_uris_for_selection (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	GSList *selection, *l, *uids_selected = NULL;
	
	priv = calendar_component->priv;
	
	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (priv->source_selector));

	for (l = priv->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			gnome_calendar_remove_source (priv->calendar, E_CAL_SOURCE_TYPE_EVENT, old_selected_source);
	}

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		if (gnome_calendar_add_source (priv->calendar, E_CAL_SOURCE_TYPE_EVENT, selected_source))
			uids_selected = g_slist_append (uids_selected, (char *) e_source_peek_uid (selected_source));
	}
	
	e_source_selector_free_selection (priv->source_selection);
	priv->source_selection = selection;

	/* Save the selection for next time we start up */
	calendar_config_set_calendars_selected (uids_selected);
	g_slist_free (uids_selected);
}

static void
update_uri_for_primary_selection (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	ESource *source;

	priv = calendar_component->priv;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!source)
		return;

	/* Set the default */
	gnome_calendar_set_default_source (priv->calendar, E_CAL_SOURCE_TYPE_EVENT, source);

	/* Make sure we are embedded first */
	calendar_control_sensitize_calendar_commands (priv->view_control, priv->calendar, TRUE);

	/* Save the selection for next time we start up */
	calendar_config_set_primary_calendar (e_source_peek_uid (source));
}

static void
update_selection (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	GSList *selection, *uids_selected, *l;

	priv = calendar_component->priv;

	/* Get the selection in gconf */
	uids_selected = calendar_config_get_calendars_selected ();

	/* Remove any that aren't there any more */
	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (priv->source_selector));

	for (l = selection; l; l = l->next) {
		ESource *source = l->data;

		if (!is_in_uids (uids_selected, source)) 
			e_source_selector_unselect_source (E_SOURCE_SELECTOR (priv->source_selector), source);
	}
	
	e_source_selector_free_selection (selection);

	/* Make sure the whole selection is there */
	for (l = uids_selected; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		if (source) 
			e_source_selector_select_source (E_SOURCE_SELECTOR (priv->source_selector), source);
		
		g_free (uid);
	}
	g_slist_free (uids_selected);
}

static void
update_task_selection (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	GSList *uids_selected, *l;

	priv = calendar_component->priv;

	/* Get the selection in gconf */
	uids_selected = calendar_config_get_tasks_selected ();

	/* Remove any that aren't there any more */
	for (l = priv->task_source_selection; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (priv->task_source_list, uid);
		if (!source)
			gnome_calendar_remove_source_by_uid (priv->calendar, E_CAL_SOURCE_TYPE_TODO, uid);
		else if (!is_in_uids (uids_selected, source))
			gnome_calendar_remove_source (priv->calendar, E_CAL_SOURCE_TYPE_TODO, source);
		
		g_free (uid);
	}
	g_slist_free (priv->task_source_selection);

	/* Make sure the whole selection is there */
	for (l = uids_selected; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (priv->task_source_list, uid);
		if (!gnome_calendar_add_source (priv->calendar, E_CAL_SOURCE_TYPE_TODO, source))
			/* FIXME do something */;
	}

	priv->task_source_selection = uids_selected;
}

static void
update_primary_selection (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;

	priv = calendar_component->priv;

	uid = calendar_config_get_primary_calendar ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);
	}
	
	if (source) {
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (priv->source_selector), source);
	} else {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (priv->source_selector), source);
	}
}

static void
update_primary_task_selection (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;

	priv = calendar_component->priv;

	uid = calendar_config_get_primary_tasks ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->task_source_list, uid);
		g_free (uid);
	}
	
	if (source)
		gnome_calendar_set_default_source (priv->calendar, E_CAL_SOURCE_TYPE_TODO, source);
}

/* Callbacks.  */
static void
add_popup_menu_item (GtkMenu *menu, const char *label, const char *icon_name,
		     GCallback callback, gpointer user_data, gboolean sensitive)
{
	GtkWidget *item, *image;
	GdkPixbuf *pixbuf;

	if (icon_name) {
		item = gtk_image_menu_item_new_with_label (label);

		/* load the image */
		pixbuf = e_icon_factory_get_icon (icon_name, 16);
		image = gtk_image_new_from_pixbuf (pixbuf);

		if (image) {
			gtk_widget_show (image);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		}
	} else {
		item = gtk_menu_item_new_with_label (label);
	}

	if (callback)
		g_signal_connect (G_OBJECT (item), "activate", callback, user_data);

	if (!sensitive)
		gtk_widget_set_sensitive (item, FALSE);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static void
copy_calendar_cb (GtkWidget *widget, CalendarComponent *comp)
{
	ESource *selected_source;
	CalendarComponentPrivate *priv;

	priv = comp->priv;
	
	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!selected_source)
		return;

	copy_source_dialog (GTK_WINDOW (gtk_widget_get_toplevel (widget)), selected_source, E_CAL_SOURCE_TYPE_EVENT);
}

static void
delete_calendar_cb (GtkWidget *widget, CalendarComponent *comp)
{
	ESource *selected_source;
	CalendarComponentPrivate *priv;
	GtkWidget *dialog;

	priv = comp->priv;
	
	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!selected_source)
		return;

	/* create the confirmation dialog */
	dialog = gtk_message_dialog_new (
		GTK_WINDOW (gtk_widget_get_toplevel (widget)),
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO,
		_("Calendar '%s' will be removed. Are you sure you want to continue?"),
		e_source_peek_name (selected_source));
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
		ECal *cal;
		char *uri;

		/* first, ask the backend to remove the calendar */
		uri = e_source_get_uri (selected_source);
		cal = e_cal_model_get_client_for_uri (gnome_calendar_get_calendar_model (priv->calendar), uri);
		if (!cal)
			cal = e_cal_new_from_uri (uri, E_CAL_SOURCE_TYPE_EVENT);
		g_free (uri);
		if (cal) {
			if (e_cal_remove (cal, NULL)) {
				if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (priv->source_selector),
									  selected_source)) {
					gnome_calendar_remove_source (priv->calendar, E_CAL_SOURCE_TYPE_EVENT, selected_source);
					e_source_selector_unselect_source (E_SOURCE_SELECTOR (priv->source_selector),
									   selected_source);
				}
		
				e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);
				e_source_list_sync (priv->source_list, NULL);
			}
		}
	}

	gtk_widget_destroy (dialog);
}

static void
new_calendar_cb (GtkWidget *widget, CalendarComponent *comp)
{
	calendar_setup_new_calendar (GTK_WINDOW (gtk_widget_get_toplevel (widget)));
}

static void
edit_calendar_cb (GtkWidget *widget, CalendarComponent *comp)
{
	CalendarComponentPrivate *priv;
	ESource *selected_source;

	priv = comp->priv;
	
	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!selected_source)
		return;

	calendar_setup_edit_calendar (GTK_WINDOW (gtk_widget_get_toplevel (widget)), selected_source);
}

static void
fill_popup_menu_cb (ESourceSelector *selector, GtkMenu *menu, CalendarComponent *comp)
{
	gboolean sensitive;

	sensitive = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (comp->priv->source_selector)) ?
		TRUE : FALSE;

	add_popup_menu_item (menu, _("New Calendar"), "stock_calendar",
			     G_CALLBACK (new_calendar_cb), comp, TRUE);
	add_popup_menu_item (menu, _("Copy"), "stock_folder-copy",
			     G_CALLBACK (copy_calendar_cb), comp, sensitive);
	add_popup_menu_item (menu, _("Delete"), "stock_delete", G_CALLBACK (delete_calendar_cb), comp, sensitive);
	add_popup_menu_item (menu, _("Properties..."), NULL, G_CALLBACK (edit_calendar_cb), comp, sensitive);
}

static void
source_selection_changed_cb (ESourceSelector *selector, 
			     CalendarComponent *calendar_component)
{
	update_uris_for_selection (calendar_component);
}

static void
primary_source_selection_changed_cb (ESourceSelector *selector,
				     CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv = calendar_component->priv;

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}

	update_uri_for_primary_selection (calendar_component);
}

static void
config_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_selection (data);
}


static void
config_primary_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_primary_selection (data);
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

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	CalendarComponentPrivate *priv = CALENDAR_COMPONENT (object)->priv;
	GList *l;
	
	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->source_selection != NULL) {
		e_source_selector_free_selection (priv->source_selection);
		priv->source_selection = NULL;
	}
	
	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}
		
	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (priv->notifications);
	priv->notifications = NULL;

	if (priv->activity_handler != NULL) {
		g_object_unref (priv->activity_handler);
		priv->activity_handler = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	CalendarComponentPrivate *priv = CALENDAR_COMPONENT (object)->priv;

	g_free (priv->base_directory);
	g_free (priv->config_directory);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Evolution::Component CORBA methods.  */

static CORBA_boolean
impl_upgradeFromVersion (PortableServer_Servant servant,
			 CORBA_short major,
			 CORBA_short minor,
			 CORBA_short revision,
			 CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));

	migrate_calendars (calendar_component, major, minor, revision);

	return CORBA_TRUE;
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
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     Bonobo_Control *corba_statusbar_control,
		     CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));
	CalendarComponentPrivate *priv;
	GtkWidget *selector_scrolled_window, *vbox, *info;
	GtkWidget *statusbar_widget;
	BonoboControl *sidebar_control;
	BonoboControl *statusbar_control;
	guint not;
	
	priv = calendar_component->priv;
	
	/* Create sidebar selector */
	priv->source_selector = e_source_selector_new (calendar_component->priv->source_list);

	g_signal_connect (priv->source_selector, "drag-motion", G_CALLBACK (selector_tree_drag_motion), 
			  calendar_component);
	g_signal_connect (priv->source_selector, "drag-leave", G_CALLBACK (selector_tree_drag_leave), 
			  calendar_component);
	g_signal_connect (priv->source_selector, "drag-drop", G_CALLBACK (selector_tree_drag_drop), 
			  calendar_component);
	g_signal_connect (priv->source_selector, "drag-data-received", 
			  G_CALLBACK (selector_tree_drag_data_received), calendar_component);

	gtk_drag_dest_set(priv->source_selector, GTK_DEST_DEFAULT_ALL, drag_types,
			  num_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	gtk_widget_show (priv->source_selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), priv->source_selector);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					     GTK_SHADOW_IN);
	gtk_widget_show (selector_scrolled_window);

	info = e_info_label_new("stock_calendar");
	e_info_label_set_info((EInfoLabel *)info, _("Calendars"), "");
	gtk_widget_show (info);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), info, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), selector_scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	sidebar_control = bonobo_control_new (vbox);

	/* Create main calendar view */
	/* FIXME Instead of returning, we should make a control with a
	 * label describing the problem */
	priv->view_control = control_factory_new_control ();
	if (!priv->view_control) {
		g_warning (G_STRLOC ": could not create the control!");
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
		return;
	}

	priv->calendar = (GnomeCalendar *) bonobo_control_get_widget (priv->view_control);

	statusbar_widget = e_task_bar_new ();
	e_activity_handler_attach_task_bar (priv->activity_handler, E_TASK_BAR (statusbar_widget));
	gtk_widget_show (statusbar_widget);
	statusbar_control = bonobo_control_new (statusbar_widget);
	
	/* connect after setting the initial selections, or we'll get unwanted calls
	   to calendar_control_sensitize_calendar_commands */
	g_signal_connect_object (priv->source_selector, "selection_changed",
				 G_CALLBACK (source_selection_changed_cb), 
				 G_OBJECT (calendar_component), 0);
	g_signal_connect_object (priv->source_selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_cb), 
				 G_OBJECT (calendar_component), 0);
	g_signal_connect_object (priv->source_selector, "fill_popup_menu",
				 G_CALLBACK (fill_popup_menu_cb),
				 G_OBJECT (calendar_component), 0);

	/* Load the selection from the last run */
	update_selection (calendar_component);	
	update_primary_selection (calendar_component);
	update_task_selection (calendar_component);
	update_primary_task_selection (calendar_component);
	
	/* If the selection changes elsewhere, update it */
	not = calendar_config_add_notification_calendars_selected (config_selection_changed_cb, 
								   calendar_component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	not = calendar_config_add_notification_primary_calendar (config_primary_selection_changed_cb, 
								 calendar_component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	not = calendar_config_add_notification_tasks_selected (config_tasks_selection_changed_cb, 
							       calendar_component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	not = calendar_config_add_notification_primary_tasks (config_primary_tasks_selection_changed_cb, 
							      calendar_component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Return the controls */
	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (priv->view_control), ev);
	*corba_statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (statusbar_control), ev);
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
	list->_buffer[2].menuDescription = _("All _Day Appointment");
	list->_buffer[2].tooltip = _("Create a new all-day appointment");
	list->_buffer[2].menuShortcut = 'd';
	list->_buffer[2].iconName = "stock_new-24h-appointment";
	list->_buffer[2].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[3].id = CREATE_CALENDAR_ID;
	list->_buffer[3].description = _("New calendar");
	list->_buffer[3].menuDescription = _("C_alendar");
	list->_buffer[3].tooltip = _("Create a new calendar");
	list->_buffer[3].menuShortcut = 'a';
	list->_buffer[3].iconName = "stock_calendar";
	list->_buffer[3].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
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

static gboolean
setup_create_ecal (CalendarComponent *calendar_component) 
{
	CalendarComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;
	guint not;
	
	priv = calendar_component->priv;

	if (priv->create_ecal)
		return TRUE; 

	/* Try to use the client from the calendar first to avoid re-opening things */
	if (priv->calendar) {
		ECal *default_ecal;
		
		default_ecal = gnome_calendar_get_default_client (priv->calendar);
		if (default_ecal) {
			priv->create_ecal = g_object_ref (default_ecal);
			return TRUE;
		}
	}
	
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
		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;
			
			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the calendar '%s' for creating events and meetings"), 
							   e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return FALSE;
		}
	} else {
		GtkWidget *dialog;
			
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 _("There is no calendar available for creating events and meetings"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return FALSE;
	}		

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_calendar (config_create_ecal_changed_cb, 
								 calendar_component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_calendar (e_source_peek_uid (source));

	return TRUE;
}

static void
create_new_event (CalendarComponent *calendar_component, gboolean is_allday, gboolean is_meeting, CORBA_Environment *ev)
{
	CalendarComponentPrivate *priv = calendar_component->priv;
	gboolean read_only;
	ECalendarView *view;

	if (!setup_create_ecal (calendar_component)) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
		return;
	}
	if (!e_cal_is_read_only (priv->create_ecal, &read_only, NULL) || read_only) {
		GtkWidget *dialog;
			
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
						 _("Selected calendar is read-only, events cannot be created. Please select a read-write calendar."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}
	
	if (priv->calendar && (view = E_CALENDAR_VIEW (gnome_calendar_get_current_view_widget (priv->calendar))))
		e_calendar_view_new_appointment_full (view, is_allday, is_meeting);
	else {
		ECalComponent *comp;
		EventEditor *editor;

		editor = event_editor_new (priv->create_ecal);
		comp = cal_comp_event_new_with_current_time (priv->create_ecal, is_allday);

		comp_editor_edit_comp (COMP_EDITOR (editor), comp);
		if (is_meeting)
			event_editor_show_meeting (editor);
		comp_editor_focus (COMP_EDITOR (editor));

		e_comp_editor_registry_add (comp_editor_registry, COMP_EDITOR (editor), TRUE);
	}
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));
	
	if (strcmp (item_type_name, CREATE_EVENT_ID) == 0)
		create_new_event (calendar_component, FALSE, FALSE, ev);
 	else if (strcmp (item_type_name, CREATE_ALLDAY_EVENT_ID) == 0)
		create_new_event (calendar_component, TRUE, FALSE, ev);
	else if (strcmp (item_type_name, CREATE_MEETING_ID) == 0)
		create_new_event (calendar_component, FALSE, TRUE, ev);
	else if (strcmp (item_type_name, CREATE_CALENDAR_ID) == 0)
		calendar_setup_new_calendar (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (calendar_component->priv->calendar))));
	else
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);
}

/* Initialization.  */

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

	priv = g_new0 (CalendarComponentPrivate, 1);

	priv->base_directory = g_build_filename (g_get_home_dir (), ".evolution", NULL);
	priv->config_directory = g_build_filename (g_get_home_dir (),
						   ".evolution", "calendar", "config",
						   NULL);

	/* EPFIXME: Should use a custom one instead?  Also we should add
	 * calendar_component_peek_gconf_client().  */
	priv->gconf_client = gconf_client_get_default ();

	/* FIXME Use ecal convenience functions */
	priv->source_list = e_source_list_new_for_gconf (priv->gconf_client,
							 "/apps/evolution/calendar/sources");
	priv->task_source_list = e_source_list_new_for_gconf (priv->gconf_client,
							 "/apps/evolution/tasks/sources");

	priv->activity_handler = e_activity_handler_new ();

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

ESourceSelector *
calendar_component_peek_source_selector (CalendarComponent *component)
{
	return E_SOURCE_SELECTOR (component->priv->source_selector);
}

EActivityHandler *
calendar_component_peek_activity_handler (CalendarComponent *component)
{
	return component->priv->activity_handler;
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
