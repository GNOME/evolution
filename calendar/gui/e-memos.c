/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memos.c
 *
 * Copyright (C) 2001-2003  Ximian, Inc.
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
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <table/e-table-scrolled.h>
#include <widgets/menus/gal-view-instance.h>
#include <widgets/menus/gal-view-factory-etable.h>
#include <widgets/menus/gal-view-etable.h>

#include "e-util/e-error.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-config-listener.h"
#include "e-util/e-time-utils.h"
#include "shell/e-user-creatable-items-handler.h"
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-categories.h>
#include "widgets/menus/gal-view-menus.h"
#include "dialogs/delete-error.h"
#include "e-calendar-marshal.h"
#include "calendar-config.h"
#include "cal-search-bar.h"
#include "calendar-component.h"
#include "comp-util.h"
#include "e-memo-table-config.h"
#include "misc.h"
#include "memos-component.h"
#include "e-cal-component-memo-preview.h"
#include "e-memos.h"
#include "common/authentication.h"


/* Private part of the GnomeCalendar structure */
struct _EMemosPrivate {
	/* The memo lists for display */
	GHashTable *clients;
	GList *clients_list;
	ECal *default_client;

	ECalView *query;
	
	EConfigListener *config_listener;
	
	/* The EMemoTable showing the memos. */
	GtkWidget   *memos_view;
	EMemoTableConfig *memos_view_config;
	
	/* Calendar search bar for memos */
	GtkWidget *search_bar;

	/* Paned widget */
	GtkWidget *paned;

	/* The preview */
	GtkWidget *preview;
	
	gchar *current_uid;
	char *sexp;
	guint update_timeout;
	
	/* View instance and the view menus handler */
	GalViewInstance *view_instance;
	GalViewMenus *view_menus;

	GList *notifications;
};

static void setup_widgets (EMemos *memos);
static void e_memos_destroy (GtkObject *object);
static void update_view (EMemos *memos);

static void config_categories_changed_cb (EConfigListener *config_listener, const char *key, gpointer user_data);
static void backend_error_cb (ECal *client, const char *message, gpointer data);

/* Signal IDs */
enum {
	SELECTION_CHANGED,
	SOURCE_ADDED,
	SOURCE_REMOVED,
	LAST_SIGNAL
};

enum DndTargetType {
	TARGET_VCALENDAR
};

static GtkTargetEntry list_drag_types[] = {
	{ "text/calendar",                0, TARGET_VCALENDAR },
	{ "text/x-calendar",              0, TARGET_VCALENDAR }
};
static const int num_list_drag_types = sizeof (list_drag_types) / sizeof (list_drag_types[0]);

static guint e_memos_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EMemos, e_memos, GTK_TYPE_TABLE);

/* Callback used when the cursor changes in the table */
static void
table_cursor_change_cb (ETable *etable, int row, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;
	ECalModel *model;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	const char *uid;

	int n_selected;

	memos = E_MEMOS (data);
	priv = memos->priv;

	n_selected = e_table_selected_count (etable);
	
	/* update the HTML widget */
	if (n_selected != 1) {
		e_cal_component_memo_preview_clear (E_CAL_COMPONENT_MEMO_PREVIEW (priv->preview));

		return;
	}
	
	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
	
	comp_data = e_cal_model_get_component_at (model, e_table_get_cursor_row (etable));
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));

	e_cal_component_memo_preview_display (E_CAL_COMPONENT_MEMO_PREVIEW (priv->preview), comp_data->client, comp);
	
	e_cal_component_get_uid (comp, &uid);
	if (priv->current_uid)
		g_free (priv->current_uid);
	priv->current_uid = g_strdup (uid);
	
	g_object_unref (comp);
}

/* Callback used when the selection changes in the table. */
static void
table_selection_change_cb (ETable *etable, gpointer data)
{
	EMemos *memos;
	int n_selected;

	memos = E_MEMOS (data);

	n_selected = e_table_selected_count (etable);
	gtk_signal_emit (GTK_OBJECT (memos), e_memos_signals[SELECTION_CHANGED],
			 n_selected);
}

static void
user_created_cb (GtkWidget *view, EMemos *memos)
{
	EMemosPrivate *priv;	
	ECal *ecal;
	ECalModel *model;
	
	priv = memos->priv;

	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
	ecal = e_cal_model_get_default_client (model);

	e_memos_add_memo_source (memos, e_cal_get_source (ecal));
}

/* Callback used when the sexp in the search bar changes */
static void
search_bar_sexp_changed_cb (CalSearchBar *cal_search, const char *sexp, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;

	memos = E_MEMOS (data);
	priv = memos->priv;

	if (priv->sexp)
		g_free (priv->sexp);
	
	priv->sexp = g_strdup (sexp);
	
	update_view (memos);
}

/* Callback used when the selected category in the search bar changes */
static void
search_bar_category_changed_cb (CalSearchBar *cal_search, const char *category, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;
	ECalModel *model;

	memos = E_MEMOS (data);
	priv = memos->priv;

	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
	e_cal_model_set_default_category (model, category);
}

static gboolean
vpaned_resized_cb (GtkWidget *widget, GdkEventButton *event, EMemos *memos)
{
	calendar_config_set_task_vpane_pos (gtk_paned_get_position (GTK_PANED (widget)));

	return FALSE;
}

static void
set_timezone (EMemos *memos) 
{
	EMemosPrivate *priv;
	icaltimezone *zone;
	GList *l;
	
	priv = memos->priv;
	
	zone = calendar_config_get_icaltimezone ();
	for (l = priv->clients_list; l != NULL; l = l->next) {
		ECal *client = l->data;

		if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED)
			/* FIXME Error checking */
			e_cal_set_default_timezone (client, zone, NULL);
	}

	if (priv->default_client && e_cal_get_load_state (priv->default_client) == E_CAL_LOAD_LOADED)
		/* FIXME Error checking */
		e_cal_set_default_timezone (priv->default_client, zone, NULL);

	if (priv->preview)
		e_cal_component_memo_preview_set_default_timezone (E_CAL_COMPONENT_MEMO_PREVIEW (priv->preview), zone);
}

static void
timezone_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EMemos *memos = data;
	
	set_timezone (memos);
}

static void
update_view (EMemos *memos)
{
	EMemosPrivate *priv;
	ECalModel *model;
	char *real_sexp = NULL;
	char *new_sexp = NULL;
	
	priv = memos->priv;

	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
		
	e_cal_model_set_search_query (model, priv->sexp);
	
	e_cal_component_memo_preview_clear (E_CAL_COMPONENT_MEMO_PREVIEW (priv->preview));
}

static gboolean
update_view_cb (EMemos *memos)
{	
	update_view (memos);

	return TRUE;
}

static void
model_row_changed_cb (ETableModel *etm, int row, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;
	ECalModelComponent *comp_data;

	memos = E_MEMOS (data);
	priv = memos->priv;

	if (priv->current_uid) {
		const char *uid;

		comp_data = e_cal_model_get_component_at (E_CAL_MODEL (etm), row);
		if (comp_data) {
			uid = icalcomponent_get_uid (comp_data->icalcomp);
			if (!strcmp (uid ? uid : "", priv->current_uid)) {
				ETable *etable;

				etable = e_table_scrolled_get_table (
					E_TABLE_SCROLLED (E_MEMO_TABLE (priv->memos_view)->etable));
				table_cursor_change_cb (etable, 0, memos);
			}
		}
	}
}

static void
setup_config (EMemos *memos)
{
	EMemosPrivate *priv;
	guint not;

	priv = memos->priv;
	
	/* Timezone */
	set_timezone (memos);
	
	not = calendar_config_add_notification_timezone (timezone_changed_cb, memos);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
}

static void
table_drag_data_get (ETable             *table,
		     int                 row,
		     int                 col,
		     GdkDragContext     *context,
		     GtkSelectionData   *selection_data,
		     guint               info,
		     guint               time,
		     EMemos             *memos)
{
	EMemosPrivate *priv;
	ECalModelComponent *comp_data;

	priv = memos->priv;

	if (priv->current_uid) {
		ECalModel *model;

		model = e_memo_table_get_model (
			E_MEMO_TABLE (priv->memos_view));

		comp_data = e_cal_model_get_component_at (model, row);

		if (info == TARGET_VCALENDAR) {
			/* we will pass an icalcalendar component for both types */
			char *comp_str;
			icalcomponent *vcal;
		
			vcal = e_cal_util_new_top_level ();
			e_cal_util_add_timezones_from_component (vcal, comp_data->icalcomp);
			icalcomponent_add_component (
			        vcal,
				icalcomponent_new_clone (comp_data->icalcomp));

			comp_str = icalcomponent_as_ical_string (vcal);
			if (comp_str) {
				gtk_selection_data_set (selection_data, selection_data->target,
							8, comp_str, strlen (comp_str));
			}
			icalcomponent_free (vcal);
		}
	}
}

static void 
table_drag_data_delete (ETable         *table,
			int             row,
			int             col,
			GdkDragContext *context,
			EMemos         *memos)
{
	EMemosPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model;
	gboolean read_only = TRUE;
	
	priv = memos->priv;
	
	model = e_memo_table_get_model (
		E_MEMO_TABLE (priv->memos_view));
	comp_data = e_cal_model_get_component_at (model, row);

	e_cal_is_read_only (comp_data->client, &read_only, NULL);
	if (read_only)
		return;

	e_cal_remove_object (comp_data->client, icalcomponent_get_uid (comp_data->icalcomp), NULL);
}

#define E_MEMOS_TABLE_DEFAULT_STATE					\
	"<?xml version=\"1.0\"?>"					\
	"<ETableState>"							\
	"<column source=\"1\"/>"					\
	"<column source=\"0\"/>"					\
	"<column source=\"2\"/>"					\
	"<grouping></grouping>"						\
	"</ETableState>"

static void
pane_realized (GtkWidget *widget, EMemos *memos)
{
	gtk_paned_set_position ((GtkPaned *)widget, calendar_config_get_task_vpane_pos ());
}

static void
setup_widgets (EMemos *memos)
{
	EMemosPrivate *priv;
	ETable *etable;
	ECalModel *model;

	priv = memos->priv;

	priv->search_bar = cal_search_bar_new (CAL_SEARCH_TASKS_DEFAULT);
	g_signal_connect (priv->search_bar, "sexp_changed",
			  G_CALLBACK (search_bar_sexp_changed_cb), memos);
	g_signal_connect (priv->search_bar, "category_changed",
			  G_CALLBACK (search_bar_category_changed_cb), memos);
	
	/* TODO Why doesn't this work?? */
	config_categories_changed_cb (priv->config_listener, "/apps/evolution/general/category_master_list", memos);

	gtk_table_attach (GTK_TABLE (memos), priv->search_bar, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
	gtk_widget_show (priv->search_bar);

	/* add the paned widget for the memo list and memo detail areas */
	priv->paned = gtk_vpaned_new ();
	g_signal_connect (priv->paned, "realize", G_CALLBACK (pane_realized), memos);

	g_signal_connect (G_OBJECT (priv->paned), "button_release_event",
			  G_CALLBACK (vpaned_resized_cb), memos);
	gtk_table_attach (GTK_TABLE (memos), priv->paned, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (priv->paned);

	/* create the memo list */
	priv->memos_view = e_memo_table_new ();
	priv->memos_view_config = e_memo_table_config_new (E_MEMO_TABLE (priv->memos_view));
	
	g_signal_connect (priv->memos_view, "user_created", G_CALLBACK (user_created_cb), memos);

	etable = e_table_scrolled_get_table (
		E_TABLE_SCROLLED (E_MEMO_TABLE (priv->memos_view)->etable));
	e_table_set_state (etable, E_MEMOS_TABLE_DEFAULT_STATE);
	
	gtk_paned_add1 (GTK_PANED (priv->paned), priv->memos_view);
	gtk_widget_show (priv->memos_view);


	e_table_drag_source_set (etable, GDK_BUTTON1_MASK,
				 list_drag_types, num_list_drag_types,
				 GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_ASK);
	
	g_signal_connect (etable, "table_drag_data_get",
			  G_CALLBACK(table_drag_data_get), memos);
	g_signal_connect (etable, "table_drag_data_delete",
			  G_CALLBACK(table_drag_data_delete), memos);

	g_signal_connect (etable, "cursor_change", G_CALLBACK (table_cursor_change_cb), memos);
	g_signal_connect (etable, "selection_change", G_CALLBACK (table_selection_change_cb), memos);

	/* Timeout check to hide completed items */
	priv->update_timeout = g_timeout_add_full (G_PRIORITY_LOW, 60000, (GSourceFunc) update_view_cb, memos, NULL);

	/* create the memo detail */
	priv->preview = e_cal_component_memo_preview_new ();
	e_cal_component_memo_preview_set_default_timezone (E_CAL_COMPONENT_MEMO_PREVIEW (priv->preview), calendar_config_get_icaltimezone ());
	gtk_paned_add2 (GTK_PANED (priv->paned), priv->preview);
	gtk_widget_show (priv->preview);

	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
	g_signal_connect (G_OBJECT (model), "model_row_changed",
			  G_CALLBACK (model_row_changed_cb), memos);
}

/* Class initialization function for the gnome calendar */
static void
e_memos_class_init (EMemosClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	e_memos_signals[SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_LAST,
				G_TYPE_FROM_CLASS (object_class), 
				GTK_SIGNAL_OFFSET (EMemosClass, selection_changed),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_INT);

	e_memos_signals[SOURCE_ADDED] =
		g_signal_new ("source_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMemosClass, source_added),
			      NULL, NULL,
			      e_calendar_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	e_memos_signals[SOURCE_REMOVED] =
		g_signal_new ("source_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMemosClass, source_removed),
			      NULL, NULL,
			      e_calendar_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	object_class->destroy = e_memos_destroy;

	klass->selection_changed = NULL;
	klass->source_added = NULL;
	klass->source_removed = NULL;
}


static void
config_categories_changed_cb (EConfigListener *config_listener, const char *key, gpointer user_data)
{
	GList *cat_list;
	GPtrArray *cat_array;
	EMemosPrivate *priv;
	EMemos *memos = user_data;

	priv = memos->priv;

	cat_array = g_ptr_array_new ();
	cat_list = e_categories_get_list ();
	while (cat_list != NULL) {
		if (e_categories_is_searchable ((const char *) cat_list->data))
			g_ptr_array_add (cat_array, cat_list->data);
		cat_list = g_list_remove (cat_list, cat_list->data);
	}

	cal_search_bar_set_categories (CAL_SEARCH_BAR(priv->search_bar), cat_array);

	g_ptr_array_free (cat_array, TRUE);
}

/* Object initialization function for the gnome calendar */
static void
e_memos_init (EMemos *memos)
{
	EMemosPrivate *priv;
	
	priv = g_new0 (EMemosPrivate, 1);
	memos->priv = priv;

	setup_config (memos);
	setup_widgets (memos);
	
	priv->config_listener = e_config_listener_new ();
	g_signal_connect (priv->config_listener, "key_changed", G_CALLBACK (config_categories_changed_cb), memos);

	priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);	
	priv->query = NULL;
	priv->view_instance = NULL;
	priv->view_menus = NULL;
	priv->current_uid = NULL;
	priv->sexp = g_strdup ("#t");
	priv->default_client = NULL;
	update_view (memos);
}

/* Callback used when the set of categories changes in the calendar client */
/* TODO this was actually taken out of tasks in 2.3.x
 *      config_categories doesn't work, but this may - trying with memos*/
/*
static void
client_categories_changed_cb (ECal *client, GPtrArray *categories, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;

	memos = E_MEMOS (data);
	priv = memos->priv;

	cal_search_bar_set_categories (CAL_SEARCH_BAR (priv->search_bar), categories);
}
*/

GtkWidget *
e_memos_new (void)
{
	EMemos *memos;

	memos = g_object_new (e_memos_get_type (), NULL);

	return GTK_WIDGET (memos);
}


void
e_memos_set_ui_component (EMemos *memos,
			  BonoboUIComponent *ui_component)
{
	g_return_if_fail (E_IS_MEMOS (memos));
	g_return_if_fail (ui_component == NULL || BONOBO_IS_UI_COMPONENT (ui_component));

	e_search_bar_set_ui_component (E_SEARCH_BAR (memos->priv->search_bar), ui_component);
}


static void
e_memos_destroy (GtkObject *object)
{
	EMemos *memos;
	EMemosPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MEMOS (object));

	memos = E_MEMOS (object);
	priv = memos->priv;

	if (priv) {
		GList *l;

		/* unset the config listener */
		if (priv->config_listener) {
			g_signal_handlers_disconnect_matched (priv->config_listener,
							      G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, memos);
			g_object_unref (priv->config_listener);
			priv->config_listener = NULL;
		}
		
		/* disconnect from signals on all the clients */
		for (l = priv->clients_list; l != NULL; l = l->next) {
			g_signal_handlers_disconnect_matched (l->data, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, memos);
		}

		g_hash_table_destroy (priv->clients);
		g_list_free (priv->clients_list);

		if (priv->default_client)
			g_object_unref (priv->default_client);
		priv->default_client = NULL;

		if (priv->current_uid) {
			g_free (priv->current_uid);
			priv->current_uid = NULL;
		}
	
		if (priv->sexp) {
			g_free (priv->sexp);
			priv->sexp = NULL;
		}

		if (priv->update_timeout) {
			g_source_remove (priv->update_timeout);
			priv->update_timeout = 0;
		}
	
		if (priv->memos_view_config) {
			g_object_unref (priv->memos_view_config);
			priv->memos_view_config = NULL;
		}
		
		for (l = priv->notifications; l; l = l->next)
			calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
		priv->notifications = NULL;

		g_free (priv);
		memos->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (e_memos_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (e_memos_parent_class)->destroy) (object);
}

static void
set_status_message (EMemos *memos, const char *message, ...)
{
	EMemosPrivate *priv;
	va_list args;
	char sz[2048], *msg_string = NULL;

	if (message) {
		va_start (args, message);
		vsnprintf (sz, sizeof sz, message, args);
		va_end (args);
		msg_string = sz;
	}

	priv = memos->priv;
	
	e_memo_table_set_status_message (E_MEMO_TABLE (priv->memos_view), msg_string);
}

/* Callback from the calendar client when an error occurs in the backend */
static void
backend_error_cb (ECal *client, const char *message, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;
	char *errmsg;
	char *urinopwd;

	memos = E_MEMOS (data);
	priv = memos->priv;

	urinopwd = get_uri_without_password (e_cal_get_uri (client));
	errmsg = g_strdup_printf (_("Error on %s:\n %s"), urinopwd, message);
	gnome_error_dialog_parented (errmsg, GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (memos))));
	g_free (errmsg);
	g_free (urinopwd);
}

/* Callback from the calendar client when the backend dies */
static void
backend_died_cb (ECal *client, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;
	ESource *source;
	
	memos = E_MEMOS (data);
	priv = memos->priv;

	source = g_object_ref (e_cal_get_source (client));

	priv->clients_list = g_list_remove (priv->clients_list, client);
	g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

	gtk_signal_emit (GTK_OBJECT (memos), e_memos_signals[SOURCE_REMOVED], source);

	e_memo_table_set_status_message (E_MEMO_TABLE (e_memos_get_calendar_table (memos)), NULL);
	
	e_error_run (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (memos))),
		     "calendar:memos-crashed", NULL);
	
	g_object_unref (source);
}

/* Callback from the calendar client when the calendar is opened */
static void
client_cal_opened_cb (ECal *ecal, ECalendarStatus status, EMemos *memos)
{
	ECalModel *model;
	ESource *source;
	EMemosPrivate *priv;

	priv = memos->priv;

	source = e_cal_get_source (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, client_cal_opened_cb, NULL);

		set_status_message (memos, _("Loading memos"));
		model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
		e_cal_model_add_client (model, ecal);

		set_timezone (memos);
		set_status_message (memos, NULL);
		break;
	case E_CALENDAR_STATUS_BUSY :
		break;
	case E_CALENDAR_STATUS_REPOSITORY_OFFLINE:
		e_error_run (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (memos))), "calendar:prompt-no-contents-offline-memos", NULL);
		break;
	default :
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);

		priv->clients_list = g_list_remove (priv->clients_list, ecal);
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, memos);

		/* Do this last because it unrefs the client */
		g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

		gtk_signal_emit (GTK_OBJECT (memos), e_memos_signals[SOURCE_REMOVED], source);

		set_status_message (memos, NULL);
		g_object_unref (source);

		break;
	}
}

static void
default_client_cal_opened_cb (ECal *ecal, ECalendarStatus status, EMemos *memos)
{
	ECalModel *model;
	ESource *source;
	EMemosPrivate *priv;

	priv = memos->priv;

	source = e_cal_get_source (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, default_client_cal_opened_cb, NULL);
		model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
		
		set_timezone (memos);
		e_cal_model_set_default_client (model, ecal);
		set_status_message (memos, NULL);
		break;
	case E_CALENDAR_STATUS_BUSY:
		break;
	default :
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);

		priv->clients_list = g_list_remove (priv->clients_list, ecal);
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, memos);

		/* Do this last because it unrefs the client */
		g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

		gtk_signal_emit (GTK_OBJECT (memos), e_memos_signals[SOURCE_REMOVED], source);

		set_status_message (memos, NULL);
		g_object_unref (priv->default_client);
		priv->default_client = NULL;
		g_object_unref (source);

		break;
	}
}

typedef void (*open_func) (ECal *, ECalendarStatus, EMemos *);

static gboolean
open_ecal (EMemos *memos, ECal *cal, gboolean only_if_exists, open_func of)
{
	EMemosPrivate *priv;

	priv = memos->priv;
	
	set_status_message (memos, _("Opening memos at %s"), e_cal_get_uri (cal));

	g_signal_connect (G_OBJECT (cal), "cal_opened", G_CALLBACK (of), memos);
	e_cal_open_async (cal, only_if_exists);

	return TRUE;
}

void
e_memos_open_memo			(EMemos		*memos)
{
	EMemoTable *cal_table;
                                                                                
	cal_table = e_memos_get_calendar_table (memos);
	e_memo_table_open_selected (cal_table);
}

void
e_memos_new_memo (EMemos *memos)
{
	/* used for click_to_add ?? Can't figure out anything else it's used for */
}

gboolean
e_memos_add_memo_source (EMemos *memos, ESource *source)
{
	EMemosPrivate *priv;
	ECal *client;
	const char *uid;

	g_return_val_if_fail (memos != NULL, FALSE);
	g_return_val_if_fail (E_IS_MEMOS (memos), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = memos->priv;

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (priv->clients, uid);
	if (client) {
		/* We already have it */

		return TRUE;
	} else {
		ESource *default_source;
		
		if (priv->default_client) {
			default_source = e_cal_get_source (priv->default_client);
		
			/* We don't have it but the default client is it */
			if (!strcmp (e_source_peek_uid (default_source), uid))
				client = g_object_ref (priv->default_client);
		}

		/* Create a new one */
		if (!client) {
			client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
			if (!client)
				return FALSE;
		}
	}

	g_signal_connect (G_OBJECT (client), "backend_error", G_CALLBACK (backend_error_cb), memos);
/*	g_signal_connect (G_OBJECT (client), "categories_changed", G_CALLBACK (client_categories_changed_cb), memos); */
	g_signal_connect (G_OBJECT (client), "backend_died", G_CALLBACK (backend_died_cb), memos);

	/* add the client to internal structure */	
	g_hash_table_insert (priv->clients, g_strdup (uid) , client);
	priv->clients_list = g_list_prepend (priv->clients_list, client);

	gtk_signal_emit (GTK_OBJECT (memos), e_memos_signals[SOURCE_ADDED], source);

	open_ecal (memos, client, FALSE, client_cal_opened_cb);

	return TRUE;
}

gboolean
e_memos_remove_memo_source (EMemos *memos, ESource *source)
{
	EMemosPrivate *priv;
	ECal *client;
	ECalModel *model;
	const char *uid;

	g_return_val_if_fail (memos != NULL, FALSE);
	g_return_val_if_fail (E_IS_MEMOS (memos), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = memos->priv;

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (priv->clients, uid);
	if (!client) 
		return TRUE;
	

	priv->clients_list = g_list_remove (priv->clients_list, client);
	g_signal_handlers_disconnect_matched (client, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, memos);	

	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
	e_cal_model_remove_client (model, client);

	g_hash_table_remove (priv->clients, uid);
       

	gtk_signal_emit (GTK_OBJECT (memos), e_memos_signals[SOURCE_REMOVED], source);

	return TRUE;
}

gboolean
e_memos_set_default_source (EMemos *memos, ESource *source)
{
	EMemosPrivate *priv;
	ECal *ecal;
	
	g_return_val_if_fail (memos != NULL, FALSE);
	g_return_val_if_fail (E_IS_MEMOS (memos), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	priv = memos->priv;

	ecal = g_hash_table_lookup (priv->clients, e_source_peek_uid (source));

	if (priv->default_client)
		g_object_unref (priv->default_client);

	if (ecal) {
		priv->default_client = g_object_ref (ecal);
	} else {
		priv->default_client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
		if (!priv->default_client)
			return FALSE;
	}

	open_ecal (memos, priv->default_client, FALSE, default_client_cal_opened_cb);

	return TRUE;
}

ECal *
e_memos_get_default_client (EMemos *memos)
{
	EMemosPrivate *priv;
	
	g_return_val_if_fail (memos != NULL, NULL);
	g_return_val_if_fail (E_IS_MEMOS (memos), NULL);

	priv = memos->priv;

	return e_cal_model_get_default_client (e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view)));	
}


/**
 * e_memos_delete_selected:
 * @memos: A memos control widget.
 * 
 * Deletes the selected memos in the memo list.
 **/
void
e_memos_delete_selected (EMemos *memos)
{
	EMemosPrivate *priv;
	EMemoTable *cal_table;

	g_return_if_fail (memos != NULL);
	g_return_if_fail (E_IS_MEMOS (memos));

	priv = memos->priv;

	cal_table = E_MEMO_TABLE (priv->memos_view);
	set_status_message (memos, _("Deleting selected objects..."));
	e_memo_table_delete_selected (cal_table);
	set_status_message (memos, NULL);

	e_cal_component_memo_preview_clear (E_CAL_COMPONENT_MEMO_PREVIEW (priv->preview));
}


/* Callback used from the view collection when we need to display a new view */
static void
display_view_cb (GalViewInstance *instance, GalView *view, gpointer data)
{
	EMemos *memos;

	memos = E_MEMOS (data);

	if (GAL_IS_VIEW_ETABLE (view)) {
		gal_view_etable_attach_table (GAL_VIEW_ETABLE (view), e_table_scrolled_get_table (E_TABLE_SCROLLED (E_MEMO_TABLE (memos->priv->memos_view)->etable)));
	}

	gtk_paned_set_position ((GtkPaned *)memos->priv->paned, calendar_config_get_task_vpane_pos ());
}

/**
 * e_memos_setup_view_menus:
 * @memos: A memos widget.
 * @uic: UI controller to use for the menus.
 * 
 * Sets up the #GalView menus for a memos control.  This function should be
 * called from the Bonobo control activation callback for this memos control.
 * Also, the menus should be discarded using e_memos_discard_view_menus().
 */
void
e_memos_setup_view_menus (EMemos *memos, BonoboUIComponent *uic)
{
	EMemosPrivate *priv;
	GalViewFactory *factory;
	ETableSpecification *spec;
	char *dir;
	static GalViewCollection *collection = NULL;

	g_return_if_fail (memos != NULL);
	g_return_if_fail (E_IS_MEMOS (memos));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	priv = memos->priv;

	g_return_if_fail (priv->view_instance == NULL);

	g_assert (priv->view_instance == NULL);
	g_assert (priv->view_menus == NULL);

	/* Create the view instance */

	if (collection == NULL) {
		collection = gal_view_collection_new ();

		gal_view_collection_set_title (collection, _("Memos"));

		dir = g_build_filename (memos_component_peek_base_directory (memos_component_peek ()), 
					"memos", "views", NULL);
	
		gal_view_collection_set_storage_directories (collection,
							     EVOLUTION_GALVIEWSDIR "/memos/",
							     dir);
		g_free (dir);

		/* Create the views */

		spec = e_table_specification_new ();
		e_table_specification_load_from_file (spec, 
						      EVOLUTION_ETSPECDIR "/e-memo-table.etspec");

		factory = gal_view_factory_etable_new (spec);
		g_object_unref (spec);
		gal_view_collection_add_factory (collection, factory);
		g_object_unref (factory);

		/* Load the collection and create the menus */

		gal_view_collection_load (collection);
	}

	priv->view_instance = gal_view_instance_new (collection, NULL);

	priv->view_menus = gal_view_menus_new (priv->view_instance);
	gal_view_menus_apply (priv->view_menus, uic, NULL);
	g_signal_connect (priv->view_instance, "display_view", G_CALLBACK (display_view_cb), memos);
	display_view_cb (priv->view_instance, gal_view_instance_get_current_view (priv->view_instance), memos);
}

/**
 * e_memos_discard_view_menus:
 * @memos: A memos widget.
 * 
 * Discards the #GalView menus used by a memos control.  This function should be
 * called from the Bonobo control deactivation callback for this memos control.
 * The menus should have been set up with e_memos_setup_view_menus().
 **/
void
e_memos_discard_view_menus (EMemos *memos)
{
	EMemosPrivate *priv;

	g_return_if_fail (memos != NULL);
	g_return_if_fail (E_IS_MEMOS (memos));

	priv = memos->priv;

	g_return_if_fail (priv->view_instance != NULL);

	g_assert (priv->view_instance != NULL);
	g_assert (priv->view_menus != NULL);

	g_object_unref (priv->view_instance);
	priv->view_instance = NULL;

	g_object_unref (priv->view_menus);
	priv->view_menus = NULL;
}

/**
 * e_memos_get_calendar_table:
 * @memos: A memos widget.
 * 
 * Queries the #EMemoTable contained in a memos widget.
 * 
 * Return value: The #EMemoTable that the memos widget uses to display its
 * information.
 **/
EMemoTable *
e_memos_get_calendar_table (EMemos *memos)
{
	EMemosPrivate *priv;

	g_return_val_if_fail (memos != NULL, NULL);
	g_return_val_if_fail (E_IS_MEMOS (memos), NULL);

	priv = memos->priv;
	return E_MEMO_TABLE (priv->memos_view);
}
