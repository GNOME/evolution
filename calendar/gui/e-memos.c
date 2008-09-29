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
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-categories.h>
#include "widgets/menus/gal-view-menus.h"
#include "dialogs/delete-error.h"
#include "calendar-config.h"
#include "cal-search-bar.h"
#include "comp-util.h"
#include "e-memo-table-config.h"
#include "misc.h"
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

	/* The preview */
	GtkWidget *preview;

	gchar *current_uid;
	char *sexp;

	/* View instance and the view menus handler */
	GalViewInstance *view_instance;
	GalViewMenus *view_menus;

	GList *notifications;
};

static void setup_widgets (EMemos *memos);
static void e_memos_destroy (GtkObject *object);
static void update_view (EMemos *memos);

static void categories_changed_cb (gpointer object, gpointer user_data);

/* Signal IDs */
enum {
	SELECTION_CHANGED,
	SOURCE_ADDED,
	SOURCE_REMOVED,
	LAST_SIGNAL
};

static guint e_memos_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EMemos, e_memos, GTK_TYPE_TABLE)

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
get_selected_components_cb (int model_row, gpointer data)
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
		char *comp_str;
		icalcomponent *vcal;

		vcal = e_cal_util_new_top_level ();
		e_cal_util_add_timezones_from_component (vcal, comp_data->icalcomp);
		icalcomponent_add_component (vcal, icalcomponent_new_clone (comp_data->icalcomp));

		comp_str = icalcomponent_as_ical_string (vcal);
		if (comp_str) {
			ESource *source = e_cal_get_source (comp_data->client);
			const char *source_uid = e_source_peek_uid (source);

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
		     int                 row,
		     int                 col,
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

	/* create the memo list */
	priv->memos_view = e_memo_table_new ();
	priv->memos_view_config = e_memo_table_config_new (E_MEMO_TABLE (priv->memos_view));

	g_signal_connect (etable, "table_drag_data_get",
			  G_CALLBACK(table_drag_data_get), memos);
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

	e_categories_register_change_listener (G_CALLBACK (categories_changed_cb), memos);

	priv->clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->query = NULL;
	priv->view_instance = NULL;
	priv->view_menus = NULL;
	priv->current_uid = NULL;
	priv->sexp = g_strdup ("#t");
	update_view (memos);
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

ECal *
e_memos_get_default_client (EMemos *memos)
{
	EMemosPrivate *priv;

	g_return_val_if_fail (memos != NULL, NULL);
	g_return_val_if_fail (E_IS_MEMOS (memos), NULL);

	priv = memos->priv;

	return e_cal_model_get_default_client (e_memo_table_get_model (E_MEMO_TABLE (priv->memos_view)));
}
