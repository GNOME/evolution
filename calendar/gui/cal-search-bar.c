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

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gal/widgets/e-unicode.h>
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
	SEARCH_COMMENT_CONTAINS
};

static ESearchBarItem search_option_items[] = {
	{ N_("Any field contains"), SEARCH_ANY_FIELD_CONTAINS },
	{ N_("Summary contains"), SEARCH_SUMMARY_CONTAINS },
	{ N_("Description contains"), SEARCH_DESCRIPTION_CONTAINS },
	{ N_("Comment contains"), SEARCH_COMMENT_CONTAINS },
	{ NULL, -1 }
};

/* Private part of the CalSearchBar structure */
struct CalSearchBarPrivate {
	/* Option menu for the categories drop-down */
	GtkOptionMenu *categories_omenu;
};



static void cal_search_bar_class_init (CalSearchBarClass *class);
static void cal_search_bar_init (CalSearchBar *cal_search);
static void cal_search_bar_destroy (GtkObject *object);

static void cal_search_bar_query_changed (ESearchBar *search);
static void cal_search_bar_menu_activated (ESearchBar *search, int item);

static ESearchBarClass *parent_class = NULL;

/* Signal IDs */
enum {
	SEXP_CHANGED,
	CATEGORY_CHANGED,
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
			(GtkObjectInitFunc) cal_search_bar_init,
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

	parent_class = gtk_type_class (E_SEARCH_BAR_TYPE);

	cal_search_bar_signals[SEXP_CHANGED] =
		gtk_signal_new ("sexp_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalSearchBarClass, sexp_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	cal_search_bar_signals[CATEGORY_CHANGED] =
		gtk_signal_new ("category_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalSearchBarClass, category_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, cal_search_bar_signals, LAST_SIGNAL);

	class->sexp_changed = NULL;
	class->category_changed = NULL;

	e_search_bar_class->query_changed = cal_search_bar_query_changed;
	e_search_bar_class->menu_activated = cal_search_bar_menu_activated;

	object_class->destroy = cal_search_bar_destroy;
}

/* Object initialization function for the calendar search bar */
static void
cal_search_bar_init (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;

	priv = g_new (CalSearchBarPrivate, 1);
	cal_search->priv = priv;

	priv->categories_omenu = NULL;
}

/* Destroy handler for the calendar search bar */
static void
cal_search_bar_destroy (GtkObject *object)
{
	CalSearchBar *cal_search;
	CalSearchBarPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_SEARCH_BAR (object));

	cal_search = CAL_SEARCH_BAR (object);
	priv = cal_search->priv;

	priv->categories_omenu = NULL;

	g_free (priv);
	cal_search->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Emits the "sexp_changed" signal for the calendar search bar */
static void
notify_sexp_changed (CalSearchBar *cal_search, const char *sexp)
{
	gtk_signal_emit (GTK_OBJECT (cal_search), cal_search_bar_signals[SEXP_CHANGED],
			 sexp);
}

/* Returns the string of the currently selected category, NULL for "Unmatched",
 * or (const char *) 1 for "All".
 */
static const char *
get_current_category (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;
	GtkMenu *menu;
	GtkWidget *active;
	const char *category;

	priv = cal_search->priv;

	menu = GTK_MENU (gtk_option_menu_get_menu (priv->categories_omenu));

	active = gtk_menu_get_active (menu);
	g_assert (active != NULL);

	category = gtk_object_get_user_data (GTK_OBJECT (active));
	return category;
}

/* Returns a sexp for the selected category in the drop-down menu.  The "All"
 * option is returned as (const char *) 1, and the "Unfiled" option is returned
 * as NULL.
 */
static char *
get_category_sexp (CalSearchBar *cal_search)
{
	const char *category;

	category = get_current_category (cal_search);

	if (category == NULL)
		return g_strdup ("(has-categories? #f)"); /* Unfiled items */
	else if (category == (const char *) 1)
		return NULL; /* All items */
	else
		return g_strdup_printf ("(has-categories? \"%s\")", category); /* Specific category */
}

/* Sets the query string to be (contains? "field" "text") */
static void
notify_query_contains (CalSearchBar *cal_search, const char *field, const char *text)
{
	char *sexp;
	char *category_sexp;

	category_sexp = get_category_sexp (cal_search);

	if (category_sexp)
		/* "Contains" sexp plus a sexp for a category or for unfiled items */
		sexp = g_strdup_printf ("(and (contains? \"%s\" \"%s\")"
					"     %s)",
					field, text, category_sexp);
	else
		/* "Contains" sexp; matches any category */
		sexp = g_strdup_printf ("(contains? \"%s\" \"%s\")", field, text);

	notify_sexp_changed (cal_search, sexp);

	if (category_sexp)
		g_free (category_sexp);

	g_free (sexp);
}

/* Creates a new query from the values in the widgets and notifies upstream */
static void
regen_query (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;
	int item;
	char *text;

	priv = cal_search->priv;

	/* Fetch the data from the ESearchBar's entry widgets */

	item = e_search_bar_get_option_choice (E_SEARCH_BAR (cal_search));
	text = e_search_bar_get_text (E_SEARCH_BAR (cal_search));

	if (!text)
		return; /* This is an error in the UTF8 conversion, not an empty string! */

	/* Generate the different types of queries */

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

	default:
		g_assert_not_reached ();
	}

	g_free (text);
}

/* query_changed handler for the calendar search bar */
static void
cal_search_bar_query_changed (ESearchBar *search)
{
	CalSearchBar *cal_search;

	cal_search = CAL_SEARCH_BAR (search);
	regen_query (cal_search);
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



/* Callback used when an item is selected in the categories option menu */
static void
categories_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	CalSearchBar *cal_search;
	const char *category;

	cal_search = CAL_SEARCH_BAR (data);
	regen_query (cal_search);

	category = cal_search_bar_get_category (cal_search);
	gtk_signal_emit (GTK_OBJECT (cal_search), cal_search_bar_signals[CATEGORY_CHANGED],
			 category);
}

/* Creates the option menu of categories */
static void
setup_categories_omenu (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;
	GtkWidget *label;

	priv = cal_search->priv;

	priv->categories_omenu = GTK_OPTION_MENU (gtk_option_menu_new ());
	gtk_box_pack_end (GTK_BOX (cal_search), GTK_WIDGET (priv->categories_omenu), FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET (priv->categories_omenu));

	label = gtk_label_new (_("Category:"));
	gtk_box_pack_end (GTK_BOX (cal_search), label, FALSE, FALSE, 4);
	gtk_widget_show (label);
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
	setup_categories_omenu (cal_search);

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

/* Callback used when a categories menu item is destroyed.  We free its user
 * data, which is the category string.
 */
static void
item_destroyed_cb (GtkObject *object, gpointer data)
{
	char *category;

	category = gtk_object_get_user_data (object);
	g_assert (category != NULL);

	g_free (category);
}

/* Used from qsort() */
static int
compare_categories_cb (const void *a, const void *b)
{
	const char **ca, **cb;

	ca = (const char **) a;
	cb = (const char **) b;

	/* FIXME: should use some utf8 strcoll() thingy */
	return strcmp (*ca, *cb);
}

/* Creates a sorted array of categories based on the original one; does not
 * duplicate the string values.
 */
static GPtrArray *
sort_categories (GPtrArray *categories)
{
	GPtrArray *c;
	int i;

	c = g_ptr_array_new ();
	g_ptr_array_set_size (c, categories->len);

	for (i = 0; i < categories->len; i++)
		c->pdata[i] = categories->pdata[i];

	qsort (c->pdata, c->len, sizeof (gpointer), compare_categories_cb);

	return c;
}

/**
 * cal_search_bar_set_categories:
 * @cal_search: A calendar search bar.
 * @categories: Array of pointers to strings for the category names.
 * 
 * Sets the list of categories that are to be shown in the drop-down list
 * of a calendar search bar.  The search bar will automatically add an item
 * for "unfiled" components, that is, those that have no categories assigned
 * to them.
 **/
void
cal_search_bar_set_categories (CalSearchBar *cal_search, GPtrArray *categories)
{
	CalSearchBarPrivate *priv;
	GtkMenu *menu;
	GtkWidget *item;
	GPtrArray *sorted;
	int i;

	g_return_if_fail (cal_search != NULL);
	g_return_if_fail (IS_CAL_SEARCH_BAR (cal_search));
	g_return_if_fail (categories != NULL);

	priv = cal_search->priv;

	menu = GTK_MENU (gtk_menu_new ());
	gtk_signal_connect (GTK_OBJECT (menu), "selection_done",
			    GTK_SIGNAL_FUNC (categories_selection_done_cb), cal_search);

	/* All, Unmatched, separator items */

	item = gtk_menu_item_new_with_label (_("All"));
	gtk_object_set_user_data (GTK_OBJECT (item), (char *) 1);
	gtk_menu_append (menu, item);
	gtk_widget_show (item);

	item = gtk_menu_item_new_with_label (_("Unfiled"));
	gtk_object_set_user_data (GTK_OBJECT (item), NULL);
	gtk_menu_append (menu, item);
	gtk_widget_show (item);

	if (categories->len > 0) {
		item = gtk_menu_item_new ();
		gtk_widget_set_sensitive (item, FALSE);
		gtk_menu_append (menu, item);
		gtk_widget_show (item);
	}

	/* Categories items */

	sorted = sort_categories (categories);

	for (i = 0; i < sorted->len; i++) {
		char *str;

		/* FIXME: Put the category icons here */

		str = e_utf8_to_gtk_string (GTK_WIDGET (menu), sorted->pdata[i]);
		if (!str)
			continue;

		item = gtk_menu_item_new_with_label (str);
		g_free (str);

		gtk_object_set_user_data (GTK_OBJECT (item), g_strdup (sorted->pdata[i]));
		gtk_signal_connect (GTK_OBJECT (item), "destroy",
				    GTK_SIGNAL_FUNC (item_destroyed_cb),
				    NULL);

		gtk_menu_append (menu, item);
		gtk_widget_show (item);
	}

	g_ptr_array_free (sorted, TRUE);

	/* Set the new menu; the old one will be destroyed automatically */

	gtk_option_menu_set_menu (priv->categories_omenu, GTK_WIDGET (menu));
}

/**
 * cal_search_bar_get_category:
 * @cal_search: A calendar search bar.
 * 
 * Queries the currently selected category name in a calendar search bar.
 * If "All" or "Unfiled" are selected, this function will return NULL.
 * 
 * Return value: Name of the selected category, or NULL if there is no
 * selected category.
 **/
const char *
cal_search_bar_get_category (CalSearchBar *cal_search)
{
	const char *category;

	category = get_current_category (cal_search);

	if (!category || category == (const char *) 1)
		return NULL;
	else
		return category;
}
