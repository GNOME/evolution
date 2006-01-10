/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* memos-component.c
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
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-source-selector.h>
#include <shell/e-user-creatable-items-handler.h>
#include <shell/e-component-view.h>
#include "e-cal-model.h"
#include "e-memos.h"
#include "memos-component.h"
#include "memos-control.h"
#include "e-comp-editor-registry.h"
#include "migration.h"
#include "comp-util.h"
#include "calendar-config.h"
#include "e-cal-popup.h"
#include "common/authentication.h"
#include "dialogs/calendar-setup.h"
#include "dialogs/comp-editor.h"
#include "dialogs/copy-source-dialog.h"
#include "dialogs/memo-editor.h"
#include "widgets/misc/e-info-label.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-error.h"
#include "calendar-component.h"

#define CREATE_MEMO_ID               "memo"
#define CREATE_MEMO_LIST_ID          "memo-list"

enum DndTargetType {
	DND_TARGET_TYPE_CALENDAR_LIST,
};
#define CALENDAR_TYPE "text/calendar"
#define XCALENDAR_TYPE "text/x-calendar"
#define WEB_BASE_URI "webcal://"
#define PERSONAL_RELATIVE_URI "system"

static GtkTargetEntry drag_types[] = {
	{ CALENDAR_TYPE, 0, DND_TARGET_TYPE_CALENDAR_LIST },
	{ XCALENDAR_TYPE, 0, DND_TARGET_TYPE_CALENDAR_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

/* Memos should have their own registry */
extern ECompEditorRegistry *comp_editor_registry;


typedef struct _MemosComponentView
{
	ESourceList *source_list;
	
	GSList *source_selection;
	
	EMemos *memos;
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
} MemosComponentView;

struct _MemosComponentPrivate {
	char *base_directory;
	char *config_directory;

	ESourceList *source_list;
	GSList *source_selection;

	GList *views;
	
	ECal *create_ecal;
	
	GList *notifications;
};

/* #define d(x) x */
#define d(x)

static void
ensure_sources (MemosComponent *component)
{
	GSList *groups;
	ESourceList *source_list;
	ESourceGroup *group;
	ESourceGroup *on_this_computer;
	ESourceGroup *on_the_web;
	ESource *personal_source;
	char *base_uri, *base_uri_proto;

	on_this_computer = NULL;
	on_the_web = NULL;
	personal_source = NULL;

	if (!e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_JOURNAL, NULL)) {
		g_warning ("Could not get memo source list from GConf!");
		return;
	}

	base_uri = g_build_filename (memos_component_peek_base_directory (component),
				     "memos", "local",
				     NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			/* compare only file:// part. If user home dir name changes we do not want to create 
			   one more group  */

			if (!on_this_computer && !strncmp (base_uri_proto, e_source_group_peek_base_uri (group), 7))
				on_this_computer = group;
			else if (!on_the_web && !strcmp (WEB_BASE_URI, e_source_group_peek_base_uri (group)))
				on_the_web = group;
		}
	}

	if (on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			if (!strcmp (PERSONAL_RELATIVE_URI, e_source_peek_relative_uri (source))) {
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
	else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		on_this_computer = group;
	}

	if (!personal_source) {
		/* Create the default Person addressbook */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);

		if (!calendar_config_get_primary_memos () && !calendar_config_get_memos_selected ()) {
			GSList selected;

			calendar_config_set_primary_memos (e_source_peek_uid (source));

			selected.data = (gpointer)e_source_peek_uid (source);
			selected.next = NULL;
			calendar_config_set_memos_selected (&selected);
		}

		e_source_set_color (source, 0xBECEDD);
		personal_source = source;
	}

	if (!on_the_web) {
		/* Create the LDAP source group */
		group = e_source_group_new (_("On The Web"), WEB_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		on_the_web = group;
	}

	component->priv->source_list = source_list;
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
		const char *uid = l->data;
		
		if (!strcmp (uid, e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
update_uris_for_selection (MemosComponentView *component_view)
{
	GSList *selection, *l, *uids_selected = NULL;
	
	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	
	for (l = component_view->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			e_memos_remove_memo_source (component_view->memos, old_selected_source);
	}	
	
	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;
		
		e_memos_add_memo_source (component_view->memos, selected_source);
		uids_selected = g_slist_append (uids_selected, (char *)e_source_peek_uid (selected_source));
	}

	e_source_selector_free_selection (component_view->source_selection);
	component_view->source_selection = selection;

	/* Save the selection for next time we start up */
	calendar_config_set_memos_selected (uids_selected);
	g_slist_free (uids_selected);
}

static void
update_uri_for_primary_selection (MemosComponentView *component_view)
{
	ESource *source;
	EMemoTable *cal_table;
	ETable *etable;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!source)
		return;

	/* Set the default */
	e_memos_set_default_source (component_view->memos, source);

	cal_table = e_memos_get_calendar_table (component_view->memos);
	etable = e_memo_table_get_table (cal_table);

	memos_control_sensitize_commands (component_view->view_control, component_view->memos, e_table_selected_count (etable));
	
	/* Save the selection for next time we start up */
	calendar_config_set_primary_memos (e_source_peek_uid (source));
}

static void
update_selection (MemosComponentView *component_view)
{
	GSList *selection, *uids_selected, *l;
	
	d(g_message("memos-component.c: update_selection called");)

	/* Get the selection in gconf */
	uids_selected = calendar_config_get_memos_selected ();

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
update_primary_selection (MemosComponentView *component_view)
{
	ESource *source = NULL;
	char *uid;

	uid = calendar_config_get_primary_memos ();
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
/* TODO: doesn't work! */
static void
copy_memo_list_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	MemosComponentView *component_view = data;
	ESource *selected_source;
	
	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	copy_source_dialog (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)), selected_source, E_CAL_SOURCE_TYPE_JOURNAL);
}

static void
delete_memo_list_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	MemosComponentView *component_view = data;
	ESource *selected_source;
	ECal *cal;
	char *uri;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	if (e_error_run((GtkWindow *)gtk_widget_get_toplevel(ep->target->widget),
			"calendar:prompt-delete-memo-list", e_source_peek_name(selected_source)) != GTK_RESPONSE_YES)
		return;

	/* first, ask the backend to remove the memo list */
	uri = e_source_get_uri (selected_source);
	cal = e_cal_model_get_client_for_uri (
		e_memo_table_get_model (E_MEMO_TABLE (e_memos_get_calendar_table (component_view->memos))),
		uri);
	if (!cal)
		cal = e_cal_new_from_uri (uri, E_CAL_SOURCE_TYPE_JOURNAL);
	g_free (uri);
	if (cal) {
		if (e_cal_remove (cal, NULL)) {
			if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (component_view->source_selector),
								  selected_source)) {
				e_memos_remove_memo_source (component_view->memos, selected_source);
				e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector),
								   selected_source);
			}
			
			e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);
			e_source_list_sync (component_view->source_list, NULL);
		}
	}
}

static void
new_memo_list_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	calendar_setup_new_memo_list (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)));
}

static void
edit_memo_list_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	MemosComponentView *component_view = data;
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (component_view->source_selector));
	if (!selected_source)
		return;

	calendar_setup_edit_memo_list (GTK_WINDOW (gtk_widget_get_toplevel(ep->target->widget)), selected_source);
}

static EPopupItem emc_source_popups[] = {
	{ E_POPUP_ITEM, "10.new", N_("New Memo List"), new_memo_list_cb, NULL, "stock_notes", 0, 0 },
	{ E_POPUP_ITEM, "15.copy", N_("Copy"), copy_memo_list_cb, NULL, "stock_folder-copy", 0, E_CAL_POPUP_SOURCE_PRIMARY },
	{ E_POPUP_ITEM, "20.delete", N_("Delete"), delete_memo_list_cb, NULL, "stock_delete", 0, E_CAL_POPUP_SOURCE_USER|E_CAL_POPUP_SOURCE_PRIMARY },
	{ E_POPUP_ITEM, "30.properties", N_("Properties..."), edit_memo_list_cb, NULL, "stock_folder-properties", 0, E_CAL_POPUP_SOURCE_PRIMARY },
};

static void
emc_source_popup_free(EPopup *ep, GSList *list, void *data)
{
	g_slist_free(list);
}

static gboolean
popup_event_cb(ESourceSelector *selector, ESource *insource, GdkEventButton *event, MemosComponentView *component_view)
{
	ECalPopup *ep;
	ECalPopupTargetSource *t;
	GSList *menus = NULL;
	int i;
	GtkMenu *menu;

	/** @HookPoint-ECalPopup: Memos Source Selector Context Menu
	 * @Id: org.gnome.evolution.memos.source.popup
	 * @Class: org.gnome.evolution.calendar.popup:1.0
	 * @Target: ECalPopupTargetSource
	 *
	 * The context menu on the source selector in the memos window.
	 */
	ep = e_cal_popup_new("org.gnome.evolution.memos.source.popup");
	t = e_cal_popup_target_new_source(ep, selector);
	t->target.widget = (GtkWidget *)component_view->memos;

	for (i=0;i<sizeof(emc_source_popups)/sizeof(emc_source_popups[0]);i++)
		menus = g_slist_prepend(menus, &emc_source_popups[i]);

	e_popup_add_items((EPopup *)ep, menus, NULL,emc_source_popup_free, component_view);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event?event->button:0, event?event->time:gtk_get_current_event_time());

	return TRUE;
}

static void
source_selection_changed_cb (ESourceSelector *selector, MemosComponentView *component_view)
{
	update_uris_for_selection (component_view);
}

static void
primary_source_selection_changed_cb (ESourceSelector *selector, MemosComponentView *component_view)
{
	update_uri_for_primary_selection (component_view);
}

static void
source_added_cb (EMemos *memos, ESource *source, MemosComponentView *component_view)
{
	e_source_selector_select_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
}

static void
source_removed_cb (EMemos *memos, ESource *source, MemosComponentView *component_view)
{
	e_source_selector_unselect_source (E_SOURCE_SELECTOR (component_view->source_selector), source);
}

static void
set_info (MemosComponentView *component_view)
{
	GString *message = g_string_new ("");
	int rows, selected_rows;
	
	rows = e_table_model_row_count (component_view->model);
	selected_rows =  e_table_selected_count (component_view->table);

	g_string_append_printf(message, ngettext("%d memo", "%d memos", rows), rows);
	if (selected_rows > 0)
		g_string_append_printf(message, ngettext(", %d selected", ", %d selected", selected_rows), selected_rows);

	e_info_label_set_info (component_view->info_label, _("Memos"), message->str);

	g_string_free (message, TRUE);
}

static void
table_selection_change_cb (ETableModel *etm, MemosComponentView *component_view)
{
	set_info (component_view);
}

static void
model_changed_cb (ETableModel *etm, MemosComponentView *component_view)
{
	set_info (component_view);
}

static void
model_rows_inserted_cb (ETableModel *etm, int row, int count, MemosComponentView *component_view)
{
	set_info (component_view);
}

static void
model_rows_deleted_cb (ETableModel *etm, int row, int count, MemosComponentView *component_view)
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
	MemosComponent *component = MEMOS_COMPONENT (bonobo_object_from_servant (servant));

	if (!migrate_memos(component, major, minor, revision, &err)) {
		GNOME_Evolution_Component_UpgradeFailed *failedex;

		failedex = GNOME_Evolution_Component_UpgradeFailed__alloc();
		failedex->what = CORBA_string_dup(_("Failed upgrading memos."));
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

	d(g_message("memos-component.c: update_single_object called");)

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
	
	d(g_message("memos-component.c: update_objects called");)

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VJOURNAL_COMPONENT)
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
		} else if (kind == ICAL_VJOURNAL_COMPONENT) {
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
						   E_CAL_SOURCE_TYPE_JOURNAL);
		
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
	MemosComponentView *component_view = data;

	if (activate) {
		BonoboUIComponent *uic;
		uic = bonobo_control_get_ui_component (component_view->view_control);
		
		e_user_creatable_items_handler_activate (component_view->creatable_items_handler, uic);
	}	
}

static void
config_create_ecal_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{	
	MemosComponent *component = data;
	MemosComponentPrivate *priv;
	
	priv = component->priv;

	g_object_unref (priv->create_ecal);
	priv->create_ecal = NULL;
	
	priv->notifications = g_list_remove (priv->notifications, GUINT_TO_POINTER (id));
}

static ECal *
setup_create_ecal (MemosComponent *component, MemosComponentView *component_view) 
{
	MemosComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;
	guint not;
	
	priv = component->priv;

	if (component_view) {
		ECal *default_ecal;

		default_ecal = e_memos_get_default_client (component_view->memos);
		if (default_ecal)
			return default_ecal;
	}
	
	if (priv->create_ecal)
		return priv->create_ecal; 
	
	/* Get the current primary calendar, or try to set one if it doesn't already exist */
	uid = calendar_config_get_primary_memos ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);

		priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	}

	if (!priv->create_ecal) {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	}
		
	if (priv->create_ecal) {

		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;
			
			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the memo list '%s' for creating events and meetings"), 
							   e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return NULL;
		}

	} else {
		GtkWidget *dialog;
			
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 _("There is no calendar available for creating memos"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}		

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_memos (config_create_ecal_changed_cb, 
							      component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_memos (e_source_peek_uid (source));

	return priv->create_ecal ;
}

static gboolean
create_new_memo (MemosComponent *memo_component, gboolean is_assigned, MemosComponentView *component_view)
{
	ECal *ecal;
	MemosComponentPrivate *priv;
	ECalComponent *comp;
	MemoEditor *editor;
	
	priv = memo_component->priv;
	
	ecal = setup_create_ecal (memo_component, component_view);
	if (!ecal)
		return FALSE;

	editor = memo_editor_new (ecal);
	comp = cal_comp_memo_new_with_defaults (ecal);

	comp_editor_edit_comp (COMP_EDITOR (editor), comp);
	comp_editor_focus (COMP_EDITOR (editor));

	e_comp_editor_registry_add (comp_editor_registry, COMP_EDITOR (editor), TRUE);

	return TRUE;
}

static void
create_local_item_cb (EUserCreatableItemsHandler *handler, const char *item_type_name, void *data)
{
	MemosComponent *memos_component = data;
	MemosComponentPrivate *priv;
	MemosComponentView *component_view = NULL;
	GList *l;
	
	priv = memos_component->priv;
	
	for (l = priv->views; l; l = l->next) {
		component_view = l->data;

		if (component_view->creatable_items_handler == handler)
			break;
		
		component_view = NULL;
	}
	
	if (strcmp (item_type_name, CREATE_MEMO_ID) == 0) {
		create_new_memo (memos_component, FALSE, component_view);
	}
	else if (strcmp (item_type_name, CREATE_MEMO_LIST_ID) == 0) {
		calendar_setup_new_memo_list (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (component_view->memos))));
	}
}

static MemosComponentView *
create_component_view (MemosComponent *memos_component)
{
	MemosComponentPrivate *priv;
	MemosComponentView *component_view;
	GtkWidget *selector_scrolled_window, *vbox;
	GtkWidget *statusbar_widget;
	AtkObject *a11y;
	
	priv = memos_component->priv;

	/* Create the calendar component view */
	component_view = g_new0 (MemosComponentView, 1);
	
	/* Add the source lists */
	component_view->source_list = g_object_ref (priv->source_list);
	
	/* Create sidebar selector */
	component_view->source_selector = e_source_selector_new (memos_component->priv->source_list);
	e_source_selector_set_select_new ((ESourceSelector *)component_view->source_selector, TRUE);
	a11y = gtk_widget_get_accessible (GTK_WIDGET (component_view->source_selector));
	atk_object_set_name (a11y, _("Memo Source Selector"));

	g_signal_connect (component_view->source_selector, "drag-motion", G_CALLBACK (selector_tree_drag_motion), 
			  memos_component);
	g_signal_connect (component_view->source_selector, "drag-leave", G_CALLBACK (selector_tree_drag_leave), 
			  memos_component);
	g_signal_connect (component_view->source_selector, "drag-drop", G_CALLBACK (selector_tree_drag_drop), 
			  memos_component);
	g_signal_connect (component_view->source_selector, "drag-data-received", 
			  G_CALLBACK (selector_tree_drag_data_received), memos_component);

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

	component_view->info_label = (EInfoLabel *)e_info_label_new("stock_insert-note");
	e_info_label_set_info(component_view->info_label, _("Memos"), "");
	gtk_widget_show (GTK_WIDGET (component_view->info_label));

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), GTK_WIDGET (component_view->info_label), FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), selector_scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	component_view->sidebar_control = bonobo_control_new (vbox);

	/* Create main view */
	component_view->view_control = memos_control_new ();
	if (!component_view->view_control) {
		/* FIXME free memory */

		return NULL;
	}

	component_view->memos = (EMemos *) bonobo_control_get_widget (component_view->view_control);
	component_view->table = e_memo_table_get_table (e_memos_get_calendar_table (component_view->memos));
	component_view->model = E_TABLE_MODEL (e_memo_table_get_model (e_memos_get_calendar_table (component_view->memos)));

	/* This signal is thrown if backends die - we update the selector */
	g_signal_connect (component_view->memos, "source_added", 
			  G_CALLBACK (source_added_cb), component_view);
	g_signal_connect (component_view->memos, "source_removed", 
			  G_CALLBACK (source_removed_cb), component_view);

	/* Create status bar */
	statusbar_widget = e_task_bar_new ();
	component_view->activity_handler = e_activity_handler_new ();
	e_activity_handler_attach_task_bar (component_view->activity_handler, E_TASK_BAR (statusbar_widget));
	gtk_widget_show (statusbar_widget);

	component_view->statusbar_control = bonobo_control_new (statusbar_widget);
	
	e_memo_table_set_activity_handler (e_memos_get_calendar_table (component_view->memos), component_view->activity_handler);
	
	/* connect after setting the initial selections, or we'll get unwanted calls
	   to calendar_control_sensitize_calendar_commands */
	g_signal_connect (component_view->source_selector, "selection_changed",
			  G_CALLBACK (source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "primary_selection_changed",
			  G_CALLBACK (primary_source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "popup_event",
			  G_CALLBACK (popup_event_cb), component_view);

	/* Set up the "new" item handler */
	component_view->creatable_items_handler = e_user_creatable_items_handler_new ("memos", create_local_item_cb, memos_component);
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
destroy_component_view (MemosComponentView *component_view)
{	
	GList *l;
	
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
	MemosComponent *memos_component = data;
	MemosComponentPrivate *priv;
	GList *l;
	
	priv = memos_component->priv;

	for (l = priv->views; l; l = l->next) {
		MemosComponentView *component_view = l->data;
		
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
		 CORBA_Environment *ev)
{
	MemosComponent *component = MEMOS_COMPONENT (bonobo_object_from_servant (servant));
	MemosComponentPrivate *priv;
	MemosComponentView *component_view;
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
	
	/* TODO: Make CalendarComponentView just subclass EComponentView */
	ecv = e_component_view_new_controls (parent, "memos", component_view->sidebar_control,
					     component_view->view_control, component_view->statusbar_control);

	return BONOBO_OBJREF(ecv);
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 2;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = CREATE_MEMO_ID;
	list->_buffer[0].description = _("New memo");
	list->_buffer[0].menuDescription = _("_Memo");
	list->_buffer[0].tooltip = _("Create a new memo");
	list->_buffer[0].menuShortcut = 'o';
	list->_buffer[0].iconName = "stock_insert-note";
	list->_buffer[0].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[1].id = CREATE_MEMO_LIST_ID;
	list->_buffer[1].description = _("New memo list");
	list->_buffer[1].menuDescription = _("Memo l_ist");
	list->_buffer[1].tooltip = _("Create a new memo list");
	list->_buffer[1].menuShortcut = 'i';
	list->_buffer[1].iconName = "stock_notes";
	list->_buffer[1].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	MemosComponent *memos_component = MEMOS_COMPONENT (bonobo_object_from_servant (servant));
	MemosComponentPrivate *priv;
	
	priv = memos_component->priv;	
	
	if (strcmp (item_type_name, CREATE_MEMO_ID) == 0) {
		if (!create_new_memo (memos_component, FALSE, NULL))
			bonobo_exception_set (ev, ex_GNOME_Evolution_Component_Failed);
	}
	else if (strcmp (item_type_name, CREATE_MEMO_LIST_ID) == 0) {
		/* FIXME Should we use the last opened window? */
		calendar_setup_new_memo_list (NULL);
	}
	else {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Component_UnknownType);
	}
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	MemosComponent *memos_component = MEMOS_COMPONENT (object);
	MemosComponentPrivate *priv = memos_component->priv;
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
		MemosComponentView *component_view = l->data;
	
		g_object_weak_unref (G_OBJECT (component_view->view_control), view_destroyed_cb, memos_component);
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
	MemosComponentPrivate *priv = MEMOS_COMPONENT (object)->priv;
	GList *l;
	
	for (l = priv->views; l; l = l->next) {
		MemosComponentView *component_view = l->data;
		
		destroy_component_view (component_view);
	}
	g_list_free (priv->views);

	g_free (priv->base_directory);
	g_free (priv->config_directory);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
memos_component_class_init (MemosComponentClass *klass)
{
	POA_GNOME_Evolution_Component__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->createView		     = impl_createView;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
memos_component_init (MemosComponent *component, MemosComponentClass *klass)
{
	MemosComponentPrivate *priv;

	priv = g_new0 (MemosComponentPrivate, 1);
	
	priv->base_directory = g_build_filename (g_get_home_dir (), ".evolution", NULL);
	priv->config_directory = g_build_filename (g_get_home_dir (),
						   ".evolution", "memos", "config",
						   NULL);

	component->priv = priv;
	ensure_sources (component);
}

/* Public API */

MemosComponent *
memos_component_peek (void)
{
	static MemosComponent *component = NULL;

	if (component == NULL) {
		component = g_object_new (memos_component_get_type (), NULL);

		if (e_util_mkdir_hier (component->priv->config_directory, 0777) != 0) {
			g_warning (G_STRLOC ": Cannot create directory %s: %s",
				   component->priv->config_directory, g_strerror (errno));
			g_object_unref (component);
			component = NULL;
		}
	}

	return component;
}

const char *
memos_component_peek_base_directory (MemosComponent *component)
{
	return component->priv->base_directory;
}

const char *
memos_component_peek_config_directory (MemosComponent *component)
{
	return component->priv->config_directory;
}

ESourceList *
memos_component_peek_source_list (MemosComponent *component)
{
	return component->priv->source_list;	
}

BONOBO_TYPE_FUNC_FULL (MemosComponent, GNOME_Evolution_Component, PARENT_TYPE, memos_component)
