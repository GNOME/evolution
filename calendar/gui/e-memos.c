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
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <table/e-table-scrolled.h>
#include <widgets/menus/gal-view-instance.h>
#include <widgets/menus/gal-view-factory-etable.h>
#include <widgets/menus/gal-view-etable.h>

#include "e-util/e-error.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-util-private.h"
#include "shell/e-user-creatable-items-handler.h"
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-categories.h>
#include "widgets/menus/gal-view-menus.h"
#include "dialogs/delete-error.h"
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
	gchar *sexp;

	/* View instance and the view menus handler */
	GalViewInstance *view_instance;
	GalViewMenus *view_menus;

	GList *notifications;
};

static void setup_widgets (EMemos *memos);
static void e_memos_destroy (GtkObject *object);
static void update_view (EMemos *memos);

static void categories_changed_cb (gpointer object, gpointer user_data);
static void backend_error_cb (ECal *client, const gchar *message, gpointer data);

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
	{ (gchar *) "text/calendar",   0, TARGET_VCALENDAR },
	{ (gchar *) "text/x-calendar", 0, TARGET_VCALENDAR }
};
static const gint num_list_drag_types = sizeof (list_drag_types) / sizeof (list_drag_types[0]);

static guint e_memos_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EMemos, e_memos, GTK_TYPE_TABLE)

/* Callback used when the cursor changes in the table */
static void
table_cursor_change_cb (ETable *etable, gint row, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;
	ECalModel *model;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	const gchar *uid;

	gint n_selected;

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
	gint n_selected;

	memos = E_MEMOS (data);

	n_selected = e_table_selected_count (etable);
	g_signal_emit (memos, e_memos_signals[SELECTION_CHANGED], 0, n_selected);

	if (n_selected != 1)
		e_cal_component_memo_preview_clear (E_CAL_COMPONENT_MEMO_PREVIEW (memos->priv->preview));

}

static void
user_created_cb (GtkWidget *view, EMemos *memos)
{
	EMemosPrivate *priv;
	EMemoTable *memo_table;
	ECal *ecal;

	priv = memos->priv;
	memo_table = E_MEMO_TABLE (priv->memos_view);

	if (memo_table->user_created_cal)
		ecal = memo_table->user_created_cal;
	else {
		ECalModel *model;

		model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
		ecal = e_cal_model_get_default_client (model);
	}

	e_memos_add_memo_source (memos, e_cal_get_source (ecal));
}

/* Callback used when the sexp in the search bar changes */
static void
search_bar_sexp_changed_cb (CalSearchBar *cal_search, const gchar *sexp, gpointer data)
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
search_bar_category_changed_cb (CalSearchBar *cal_search, const gchar *category, gpointer data)
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

	priv = memos->priv;

	model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));

	e_cal_model_set_search_query (model, priv->sexp);

	e_cal_component_memo_preview_clear (E_CAL_COMPONENT_MEMO_PREVIEW (priv->preview));
}

static void
model_row_changed_cb (ETableModel *etm, gint row, gpointer data)
{
	EMemos *memos;
	EMemosPrivate *priv;
	ECalModelComponent *comp_data;

	memos = E_MEMOS (data);
	priv = memos->priv;

	if (priv->current_uid) {
		const gchar *uid;

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

struct AffectedComponents {
	EMemoTable *memo_table;
	GSList *components; /* contains pointers to ECalModelComponent */
};

/**
 * get_selected_components_cb
 * Helper function to fill list of selected components in EMemoTable.
 * This function is called from e_table_selected_row_foreach.
 **/
static void
get_selected_components_cb (gint model_row, gpointer data)
{
	struct AffectedComponents *ac = (struct AffectedComponents *) data;

	if (!ac || !ac->memo_table)
		return;

	ac->components = g_slist_prepend (ac->components, e_cal_model_get_component_at (E_CAL_MODEL (e_memo_table_get_model (ac->memo_table)), model_row));
}

/**
 * do_for_selected_components
 * Calls function func for all selected components in memo_table.
 *
 * @param memo_table Table with selected components of our interest.
 * @param func Function to be called on each selected component from cal_table.
 *        The first parameter of this function is a pointer to ECalModelComponent and
 *        the second parameter of this function is pointer to cal_table
 * @param user_data User data, will be passed to func.
 **/
static void
do_for_selected_components (EMemoTable *memo_table, GFunc func, gpointer user_data)
{
	ETable *etable;
	struct AffectedComponents ac;

	g_return_if_fail (memo_table != NULL);

	ac.memo_table = memo_table;
	ac.components = NULL;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (memo_table->etable));
	e_table_selected_row_foreach (etable, get_selected_components_cb, &ac);

	g_slist_foreach (ac.components, func, user_data);
	g_slist_free (ac.components);
}

/**
 * obtain_list_of_components
 * As a callback function to convert each ECalModelComponent to string
 * of format "source_uid\ncomponent_str" and add this newly allocated
 * string to the list of components. Strings should be freed with g_free.
 *
 * @param data ECalModelComponent object.
 * @param user_data Pointer to GSList list, where to put new strings.
 **/
static void
obtain_list_of_components (gpointer data, gpointer user_data)
{
	GSList **list;
	ECalModelComponent *comp_data;

	list = (GSList **) user_data;
	comp_data = (ECalModelComponent *) data;

	if (list && comp_data) {
		gchar *comp_str;
		icalcomponent *vcal;

		vcal = e_cal_util_new_top_level ();
		e_cal_util_add_timezones_from_component (vcal, comp_data->icalcomp);
		icalcomponent_add_component (vcal, icalcomponent_new_clone (comp_data->icalcomp));

		comp_str = icalcomponent_as_ical_string_r (vcal);
		if (comp_str) {
			ESource *source = e_cal_get_source (comp_data->client);
			const gchar *source_uid = e_source_peek_uid (source);

			*list = g_slist_prepend (*list, g_strdup_printf ("%s\n%s", source_uid, comp_str));

			/* do not free this pointer, it owns libical */
			/* g_free (comp_str); */
		}

		icalcomponent_free (vcal);
		g_free (comp_str);
	}
}

static void
table_drag_data_get (ETable             *table,
		     gint                 row,
		     gint                 col,
		     GdkDragContext     *context,
		     GtkSelectionData   *selection_data,
		     guint               info,
		     guint               time,
		     EMemos             *memos)
{
	EMemosPrivate *priv;

	priv = memos->priv;

	if (info == TARGET_VCALENDAR) {
		/* we will pass an icalcalendar component for both types */
		GSList *components = NULL;

		do_for_selected_components (E_MEMO_TABLE (priv->memos_view), obtain_list_of_components, &components);

		if (components) {
			cal_comp_selection_set_string_list (selection_data, components);

			g_slist_foreach (components, (GFunc)g_free, NULL);
			g_slist_free (components);
		}
	}
}

static void
table_drag_data_delete (ETable         *table,
			gint             row,
			gint             col,
			GdkDragContext *context,
			EMemos         *memos)
{
	/* Moved components are deleted from source immediately when moved,
	   because some of them can be part of destination source, and we
	   don't want to delete not-moved tasks. There is no such information
	   which event has been moved and which not, so skip this method.
	*/
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

	priv->search_bar = cal_search_bar_new (CAL_SEARCH_MEMOS_DEFAULT);
	g_signal_connect (priv->search_bar, "sexp_changed",
			  G_CALLBACK (search_bar_sexp_changed_cb), memos);
	g_signal_connect (priv->search_bar, "category_changed",
			  G_CALLBACK (search_bar_category_changed_cb), memos);

	categories_changed_cb (NULL, memos);

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
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMemosClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	e_memos_signals[SOURCE_ADDED] =
		g_signal_new ("source_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMemosClass, source_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	e_memos_signals[SOURCE_REMOVED] =
		g_signal_new ("source_removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMemosClass, source_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	object_class->destroy = e_memos_destroy;

	klass->selection_changed = NULL;
	klass->source_added = NULL;
	klass->source_removed = NULL;
}

static void
categories_changed_cb (gpointer object, gpointer user_data)
{
	GList *cat_list;
	GPtrArray *cat_array;
	EMemosPrivate *priv;
	EMemos *memos = user_data;

	priv = memos->priv;

	cat_array = g_ptr_array_new ();
	cat_list = e_categories_get_list ();
	while (cat_list != NULL) {
		if (e_categories_is_searchable ((const gchar *) cat_list->data))
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

	e_categories_register_change_listener (G_CALLBACK (categories_changed_cb), memos);

	priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->query = NULL;
	priv->view_instance = NULL;
	priv->view_menus = NULL;
	priv->current_uid = NULL;
	priv->sexp = g_strdup ("#t");
	priv->default_client = NULL;
	update_view (memos);
}

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

		e_categories_unregister_change_listener (G_CALLBACK (categories_changed_cb), memos);

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
set_status_message (EMemos *memos, const gchar *message, ...)
{
	EMemosPrivate *priv;
	va_list args;
	gchar sz[2048], *msg_string = NULL;

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
backend_error_cb (ECal *client, const gchar *message, gpointer data)
{
	EMemos *memos;
	GtkWidget *dialog;
	gchar *urinopwd;

	memos = E_MEMOS (data);

	urinopwd = get_uri_without_password (e_cal_get_uri (client));

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (memos))),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_OK,
		_("Error on %s:\n %s"), urinopwd, message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

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

	g_signal_emit (memos, e_memos_signals[SOURCE_REMOVED], 0, source);

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

	if (status == E_CALENDAR_STATUS_AUTHENTICATION_FAILED || status == E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED)
		auth_cal_forget_password (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, client_cal_opened_cb, NULL);

		set_status_message (memos, _("Loading memos"));
		model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));
		e_cal_model_add_client (model, ecal);

		set_timezone (memos);
		set_status_message (memos, NULL);
		break;
	case E_CALENDAR_STATUS_AUTHENTICATION_FAILED:
		/* try to reopen calendar - it'll ask for a password once again */
		e_cal_open_async (ecal, FALSE);
		return;
	case E_CALENDAR_STATUS_BUSY :
		break;
	case E_CALENDAR_STATUS_REPOSITORY_OFFLINE:
		e_error_run (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (memos))), "calendar:prompt-no-contents-offline-memos", NULL);
	default :
		/* Make sure the source doesn't disappear on us */
		g_object_ref (source);

		priv->clients_list = g_list_remove (priv->clients_list, ecal);
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, memos);

		/* Do this last because it unrefs the client */
		g_hash_table_remove (priv->clients,  e_source_peek_uid (source));

		g_signal_emit (memos, e_memos_signals[SOURCE_REMOVED], 0, source);

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

	if (status == E_CALENDAR_STATUS_AUTHENTICATION_FAILED || status == E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED)
		auth_cal_forget_password (ecal);

	switch (status) {
	case E_CALENDAR_STATUS_OK :
		g_signal_handlers_disconnect_matched (ecal, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, default_client_cal_opened_cb, NULL);
		model = e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view));

		set_timezone (memos);
		e_cal_model_set_default_client (model, ecal);
		set_status_message (memos, NULL);
		break;
	case E_CALENDAR_STATUS_AUTHENTICATION_FAILED:
		/* try to reopen calendar - it'll ask for a password once again */
		e_cal_open_async (ecal, FALSE);
		return;
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

		g_signal_emit (memos, e_memos_signals[SOURCE_REMOVED], 0, source);

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
	const gchar *uid;

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
	g_signal_connect (G_OBJECT (client), "backend_died", G_CALLBACK (backend_died_cb), memos);

	/* add the client to internal structure */
	g_hash_table_insert (priv->clients, g_strdup (uid) , client);
	priv->clients_list = g_list_prepend (priv->clients_list, client);

	g_signal_emit (memos, e_memos_signals[SOURCE_ADDED], 0, source);

	open_ecal (memos, client, FALSE, client_cal_opened_cb);

	return TRUE;
}

gboolean
e_memos_remove_memo_source (EMemos *memos, ESource *source)
{
	EMemosPrivate *priv;
	ECal *client;
	ECalModel *model;
	const gchar *uid;

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

	g_signal_emit (memos, e_memos_signals[SOURCE_REMOVED], 0, source);

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
	gchar *dir0, *dir1, *filename;
	static GalViewCollection *collection = NULL;

	g_return_if_fail (memos != NULL);
	g_return_if_fail (E_IS_MEMOS (memos));
	g_return_if_fail (uic != NULL);
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (uic));

	priv = memos->priv;

	g_return_if_fail (priv->view_instance == NULL);

	g_return_if_fail (priv->view_instance == NULL);
	g_return_if_fail (priv->view_menus == NULL);

	/* Create the view instance */

	if (collection == NULL) {
		collection = gal_view_collection_new ();

		gal_view_collection_set_title (collection, _("Memos"));

		dir0 = g_build_filename (EVOLUTION_GALVIEWSDIR,
					 "memos",
					 NULL);
		dir1 = g_build_filename (memos_component_peek_base_directory (memos_component_peek ()),
					 "views", NULL);
		gal_view_collection_set_storage_directories (collection,
							     dir0,
							     dir1);
		g_free (dir1);
		g_free (dir0);

		/* Create the views */

		spec = e_table_specification_new ();
		filename = g_build_filename (EVOLUTION_ETSPECDIR,
					     "e-memo-table.etspec",
					     NULL);
		if (!e_table_specification_load_from_file (spec, filename))
			g_error ("Unable to load ETable specification file "
				 "for memos");
		g_free (filename);

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

	g_return_if_fail (priv->view_instance != NULL);
	g_return_if_fail (priv->view_menus != NULL);

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

/**
 * e_memos_get_preview:
 * @memos: A memos widget.
 *
 * Queries the #ECalComponentMemoPreview contained in a memos widget.
 **/
GtkWidget *
e_memos_get_preview (EMemos *memos)
{
	g_return_val_if_fail (memos != NULL, NULL);
	g_return_val_if_fail (E_IS_MEMOS (memos), NULL);

	return memos->priv->preview;
}
