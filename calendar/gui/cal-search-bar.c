/* Evolution calendar - Search bar widget for calendar views
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "cal-search-bar.h"



/* Menu items for the ESearchBar */
static ESearchBarItem search_menu_items[] = {
	E_FILTERBAR_RESET,
	{ NULL, -1 }
};

/* IDs and option items for the ESearchBar */
enum {
	SEARCH_ANY_FIELD_CONTAINS,
	SEARCH_SUMMARY_CONTAINS,
	SEARCH_DESCRIPTION_CONTAINS,
	SEARCH_COMMENT_CONTAINS,
	SEARCH_HAS_CATEGORY
};

static ESearchBarItem search_option_items[] = {
	{ N_("Any field contains"), SEARCH_ANY_FIELD_CONTAINS },
	{ N_("Summary contains"), SEARCH_SUMMARY_CONTAINS },
	{ N_("Description contains"), SEARCH_DESCRIPTION_CONTAINS },
	{ N_("Comment contains"), SEARCH_COMMENT_CONTAINS },
	{ N_("Has category"), SEARCH_HAS_CATEGORY },
	{ NULL, -1 }
};



static void cal_search_bar_class_init (CalSearchBarClass *class);

static void cal_search_bar_query_changed (ESearchBar *search);
static void cal_search_bar_menu_activated (ESearchBar *search, int item);

/* Signal IDs */
enum {
	SEXP_CHANGED,
	LAST_SIGNAL
};

static guint cal_search_bar_signals[LAST_SIGNAL] = { 0 };



/**
 * cal_search_bar_get_type:
 * 
 * Registers the #CalSearchBar class if necessary and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #CalSearchBar class.
 **/
GtkType
cal_search_bar_get_type (void)
{
	static GtkType cal_search_bar_type = 0;

	if (!cal_search_bar_type) {
		static const GtkTypeInfo cal_search_bar_info = {
			"CalSearchBar",
			sizeof (CalSearchBar),
			sizeof (CalSearchBarClass),
			(GtkClassInitFunc) cal_search_bar_class_init,
			(GtkObjectInitFunc) NULL,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_search_bar_type = gtk_type_unique (E_SEARCH_BAR_TYPE, &cal_search_bar_info);
	}

	return cal_search_bar_type;
}

/* Class initialization function for the calendar search bar */
static void
cal_search_bar_class_init (CalSearchBarClass *class)
{
	ESearchBarClass *e_search_bar_class;
	GtkObjectClass *object_class;

	e_search_bar_class = (ESearchBarClass *) class;
	object_class = (GtkObjectClass *) class;

	cal_search_bar_signals[SEXP_CHANGED] =
		gtk_signal_new ("sexp_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalSearchBarClass, sexp_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, cal_search_bar_signals, LAST_SIGNAL);

	class->sexp_changed = NULL;

	e_search_bar_class->query_changed = cal_search_bar_query_changed;
	e_search_bar_class->menu_activated = cal_search_bar_menu_activated;
}



/* Emits the "sexp_changed" signal for the calendar search bar */
static void
notify_sexp_changed (CalSearchBar *cal_search, const char *sexp)
{
	gtk_signal_emit (GTK_OBJECT (cal_search), cal_search_bar_signals[SEXP_CHANGED],
			 sexp);
}

/* Sets the query string to be (contains? "field" "text") */
static void
notify_query_contains (CalSearchBar *cal_search, const char *field, const char *text)
{
	char *sexp;

	sexp = g_strdup_printf ("(contains? \"%s\" \"%s\")", field, text);
	notify_sexp_changed (cal_search, sexp);
	g_free (sexp);
}

/* query_changed handler for the calendar search bar */
static void
cal_search_bar_query_changed (ESearchBar *search)
{
	CalSearchBar *cal_search;
	int item;
	char *text;

	cal_search = CAL_SEARCH_BAR (search);

	item = e_search_bar_get_option_choice (search);
	text = e_search_bar_get_text (search);

	if (!text)
		return; /* This is an error in the UTF8 conversion, not an empty string! */

	switch (item) {
	case SEARCH_ANY_FIELD_CONTAINS:
		notify_query_contains (cal_search, "any", text);
		break;

	case SEARCH_SUMMARY_CONTAINS:
		notify_query_contains (cal_search, "summary", text);
		break;

	case SEARCH_DESCRIPTION_CONTAINS:
		notify_query_contains (cal_search, "description", text);
		break;

	case SEARCH_COMMENT_CONTAINS:
		notify_query_contains (cal_search, "comment", text);
		break;

	case SEARCH_HAS_CATEGORY: {
		char *sexp;

		sexp = g_strdup_printf ("(has-categories? \"%s\")", text);
		notify_sexp_changed (cal_search, sexp);
		g_free (sexp);
		break;
	}

	default:
		g_assert_not_reached ();
	}

	g_free (text);
}

/* menu_activated handler for the calendar search bar */
static void
cal_search_bar_menu_activated (ESearchBar *search, int item)
{
	CalSearchBar *cal_search;

	cal_search = CAL_SEARCH_BAR (search);

	switch (item) {
	case E_FILTERBAR_RESET_ID:
		notify_sexp_changed (cal_search, "#t"); /* match all */
		/* FIXME: should we change the rest of the search bar so that
		 * the user sees that he selected "show all" instead of some
		 * type/text search combination?
		 */
		break;

	default:
		g_assert_not_reached ();
	}
}



/**
 * cal_search_bar_construct:
 * @cal_search: A calendar search bar.
 * 
 * Constructs a calendar search bar by binding its menu and option items.
 * 
 * Return value: The same value as @cal_search.
 **/
CalSearchBar *
cal_search_bar_construct (CalSearchBar *cal_search)
{
	g_return_val_if_fail (cal_search != NULL, NULL);
	g_return_val_if_fail (IS_CAL_SEARCH_BAR (cal_search), NULL);

	e_search_bar_construct (E_SEARCH_BAR (cal_search), search_menu_items, search_option_items);
	return cal_search;
}

/**
 * cal_search_bar_new:
 * 
 * Creates a new calendar search bar.
 * 
 * Return value: A newly-created calendar search bar.  You should connect to the
 * "sexp_changed" signal to monitor changes in the generated sexps.
 **/
GtkWidget *
cal_search_bar_new (void)
{
	CalSearchBar *cal_search;

	cal_search = gtk_type_new (TYPE_CAL_SEARCH_BAR);
	return GTK_WIDGET (cal_search_bar_construct (cal_search));
}
