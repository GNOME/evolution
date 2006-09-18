/* Evolution calendar - Search bar widget for calendar views
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <string.h>
#include <glib.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-icon-factory.h>
#include <libedataserver/e-categories.h>
#include "cal-search-bar.h"

typedef struct CALSearchBarItem {
	 ESearchBarItem search;
	 char *image;
}CALSearchBarItem;

/* IDs and option items for the ESearchBar */
enum {
	SEARCH_SUMMARY_CONTAINS,
	SEARCH_DESCRIPTION_CONTAINS,
	SEARCH_CATEGORY_IS,
	SEARCH_COMMENT_CONTAINS,
	SEARCH_LOCATION_CONTAINS,
	SEARCH_ANY_FIELD_CONTAINS
};

/* Comments are disabled because they are kind of useless right now, see bug 33247 */
static ESearchBarItem search_option_items[] = {
	{ N_("Summary contains"), SEARCH_SUMMARY_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ N_("Description contains"), SEARCH_DESCRIPTION_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ N_("Category is"), SEARCH_CATEGORY_IS, ESB_ITEMTYPE_RADIO },
	{ N_("Comment contains"), SEARCH_COMMENT_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ N_("Location contains"), SEARCH_LOCATION_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ N_("Any field contains"), SEARCH_ANY_FIELD_CONTAINS, ESB_ITEMTYPE_RADIO },
};

/* IDs for the categories suboptions */
#define CATEGORIES_ALL 0
#define CATEGORIES_UNMATCHED 1
#define CATEGORIES_OFFSET 3

/* Private part of the CalSearchBar structure */
struct CalSearchBarPrivate {
	/* Array of categories */
	GPtrArray *categories;
};

static void cal_search_bar_destroy (GtkObject *object);

static void cal_search_bar_search_activated (ESearchBar *search);

/* Signal IDs */
enum {
	SEXP_CHANGED,
	CATEGORY_CHANGED,
	LAST_SIGNAL
};

static guint cal_search_bar_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (CalSearchBar, cal_search_bar, E_SEARCH_BAR_TYPE)

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
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (CalSearchBarClass, sexp_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	cal_search_bar_signals[CATEGORY_CHANGED] =
		gtk_signal_new ("category_changed",
				GTK_RUN_FIRST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (CalSearchBarClass, category_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	class->sexp_changed = NULL;
	class->category_changed = NULL;

	e_search_bar_class->search_activated = cal_search_bar_search_activated;

	object_class->destroy = cal_search_bar_destroy;
}

/* Object initialization function for the calendar search bar */
static void
cal_search_bar_init (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;

	priv = g_new (CalSearchBarPrivate, 1);
	cal_search->priv = priv;

	priv->categories = g_ptr_array_new ();
	g_ptr_array_set_size (priv->categories, 0);
}

/* Frees an array of categories */
static void
free_categories (GPtrArray *categories)
{
	int i;

	for (i = 0; i < categories->len; i++) {
		g_assert (categories->pdata[i] != NULL);
		g_free (categories->pdata[i]);
	}

	g_ptr_array_free (categories, TRUE);
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

	if (priv) {
		if (priv->categories) {
			free_categories (priv->categories);
			priv->categories = NULL;
		}
		
		g_free (priv);
		cal_search->priv = NULL;
	}
	
	if (GTK_OBJECT_CLASS (cal_search_bar_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (cal_search_bar_parent_class)->destroy) (object);
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
	gint viewid;

	priv = cal_search->priv;

	g_assert (priv->categories != NULL);

	viewid = e_search_bar_get_viewitem_id (E_SEARCH_BAR (cal_search));

	if (viewid == CATEGORIES_ALL)
		return (const char *) 1;
	else if (viewid == CATEGORIES_UNMATCHED)
		return NULL;
	else {
		int i;

		i = viewid - CATEGORIES_OFFSET;
		g_assert (i >= 0 && i < priv->categories->len);

		return priv->categories->pdata[i];
	}
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
	else if (category == (const char *) 1) {
		return NULL; /* All items */
	}
	else
		return g_strdup_printf ("(has-categories? \"%s\")", category); /* Specific category */
}


/* Sets the query string to be (contains? "field" "text") */
static void
notify_e_cal_view_contains (CalSearchBar *cal_search, const char *field, const char *view)
{
	char *text = NULL;
	char *sexp = " ";

	text = e_search_bar_get_text (E_SEARCH_BAR (cal_search));

	if (!text)
		return; /* This is an error in the UTF8 conversion, not an empty string! */

	if (text && *text) {
	    sexp = g_strdup_printf ("(contains? \"%s\" \"%s\")", field, text);
	    g_free (text);
	} else 
	    sexp = g_strdup_printf ("(contains? \"summary\" \"\")", field, text); /* Show all */


	/* Apply the selected view on search */
	view = get_category_sexp (cal_search);
	if (view && *view)
	    sexp = g_strconcat ("(and ",sexp, view, ")", NULL);

	notify_sexp_changed (cal_search, sexp);
	g_free (sexp);
}

/* Sets the query string to the appropriate match for categories */
static void
notify_category_is (CalSearchBar *cal_search)
{
	char *sexp;

	sexp = get_category_sexp (cal_search);

	if (!sexp)
		notify_sexp_changed (cal_search, "#t"); /* Match all */
	else
		notify_sexp_changed (cal_search, sexp);

	if (sexp)
		g_free (sexp);
}

/* Creates a new query from the values in the widgets and notifies upstream */
static void
regen_query (CalSearchBar *cal_search)
{
	int id;
	const char *category_sexp, *category;

	/* Fetch the data from the ESearchBar's entry widgets */
	id = e_search_bar_get_item_id (E_SEARCH_BAR (cal_search));

	/* Get the selected view */
	category_sexp = get_category_sexp (cal_search);

	/* Generate the different types of queries */
	switch (id) {
	case SEARCH_ANY_FIELD_CONTAINS:
		notify_e_cal_view_contains (cal_search, "any", category_sexp);
		break;

	case SEARCH_SUMMARY_CONTAINS:
		notify_e_cal_view_contains (cal_search, "summary", category_sexp);
		break;

	case SEARCH_DESCRIPTION_CONTAINS:
		notify_e_cal_view_contains (cal_search, "description", category_sexp);
		break;

	case SEARCH_COMMENT_CONTAINS:
		notify_e_cal_view_contains (cal_search, "comment", category_sexp);
		break;

	case SEARCH_LOCATION_CONTAINS:
		notify_e_cal_view_contains (cal_search, "location", category_sexp);
		break;

	default:
		g_assert_not_reached ();
	}
}

static void
regen_view_query (CalSearchBar *cal_search)
{
        const char *category;
	notify_category_is (cal_search);

	category = cal_search_bar_get_category (cal_search);
	gtk_signal_emit (GTK_OBJECT (cal_search), cal_search_bar_signals[CATEGORY_CHANGED],
			 category);
}
/* search_activated handler for the calendar search bar */
static void
cal_search_bar_search_activated (ESearchBar *search)
{
	CalSearchBar *cal_search;

	cal_search = CAL_SEARCH_BAR (search);
	regen_query (cal_search);
}




static char *
string_without_underscores (const char *s)
{
	char *new_string;
	const char *sp;
	char *dp;

	new_string = g_malloc (strlen (s) + 1);

	dp = new_string;
	for (sp = s; *sp != '\0'; sp ++) {
		if (*sp != '_') {
			*dp = *sp;
			dp ++;
		} else if (sp[1] == '_') {
			/* Translate "__" in "_".  */
			*dp = '_';
			dp ++;
			sp ++;
		}
	}
	*dp = 0;

	return new_string;
}

static GtkWidget *
generate_viewoption_menu (CALSearchBarItem *subitems)
{
	GtkWidget *menu, *menu_item;
	gint i = 0;
	GSList *l;

	menu = gtk_menu_new ();

	for (i = 0; subitems[i].search.id != -1; ++i) {
		if (subitems[i].search.text) {
			char *str = NULL;
			str = string_without_underscores (subitems[i].search.text);
			menu_item = gtk_image_menu_item_new_with_label (str);
/*			if (subitems[i].image)
				gtk_image_menu_item_set_image (menu_item, e_icon_factory_get_image (subitems[i].image, E_ICON_SIZE_MENU));*/
			g_free (str);
		} else {
			menu_item = gtk_menu_item_new ();
			gtk_widget_set_sensitive (menu_item, FALSE);
		}

		g_object_set_data (G_OBJECT (menu_item), "EsbItemId",
				   GINT_TO_POINTER (subitems[i].search.id));

		gtk_widget_show (menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
	}

	return menu;
}

/* Creates the suboptions menu for the ESearchBar with the list of categories */
static void
make_suboptions (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;
	CALSearchBarItem *subitems;
	int i;
	GtkWidget *menu;
	
	priv = cal_search->priv;

	g_assert (priv->categories != NULL);

	/* Categories plus "all", "unmatched", separator, terminator */
	subitems = g_new (CALSearchBarItem, priv->categories->len + 3 + 1);

	/* All, unmatched, separator */

	subitems[0].search.text = _("Any Category");
	subitems[0].search.id = CATEGORIES_ALL;
	subitems[0].image = NULL;

	subitems[1].search.text = _("Unmatched");
	subitems[1].search.id = CATEGORIES_UNMATCHED;
	subitems[1].image = NULL;
	
	/* All the other items */

	if (priv->categories->len > 0) {
		 subitems[2].search.text = NULL; /* separator */
		 subitems[2].search.id = 0;
		 subitems[2].image = 0;
		 
		 for (i = 0; i < priv->categories->len; i++) {
			  const char *category;
			  char *str;

			  category = priv->categories->pdata[i];
			  str = g_strdup (category ? category : "");

			  subitems[i + CATEGORIES_OFFSET].search.text      = str;
			  subitems[i + CATEGORIES_OFFSET].search.id        = i + CATEGORIES_OFFSET;
			  subitems[i + CATEGORIES_OFFSET].image        = e_categories_get_icon_file_for(str);
		 }

		 subitems[i + CATEGORIES_OFFSET].search.id = -1; /* terminator */
		 subitems[2].search.text = NULL;
		 subitems[2].image = NULL;		
	} else {
		 
		 subitems[2].search.id = -1; /* terminator */
		 subitems[2].search.text = NULL;
		 subitems[2].image = NULL;
	}	

	menu = generate_viewoption_menu (subitems);
	e_search_bar_set_viewoption_menu ((ESearchBar *)cal_search, menu);
	
	/* Free the strings */
	for (i = 0; i < priv->categories->len; i++)
		g_free (subitems[i + CATEGORIES_OFFSET].search.text);

	g_free (subitems);
}

/**
 * cal_search_bar_construct:
 * @cal_search: A calendar search bar.
 * @flags: bitfield of items to appear in the search menu
 * 
 * Constructs a calendar search bar by binding its menu and option items.
 * 
 * Return value: The same value as @cal_search.
 **/
CalSearchBar *
cal_search_bar_construct (CalSearchBar *cal_search, guint32 flags)
{
	ESearchBarItem *items;
	guint32 bit = 0x1;
	int i, j;
	
	g_return_val_if_fail (IS_CAL_SEARCH_BAR (cal_search), NULL);
	
	items = g_alloca ((G_N_ELEMENTS (search_option_items) + 1) * sizeof (ESearchBarItem));
	for (i = 0, j = 0; i < G_N_ELEMENTS (search_option_items); i++, bit <<= 1) {
		if ((flags & bit) != 0) {
			items[j].text = search_option_items[i].text;
			items[j].id = search_option_items[i].id;
			items[j].type = search_option_items[i].type;
			j++;
		}
	}
	
	items[j].text = NULL;
	items[j].id = -1;
	
	e_search_bar_construct (E_SEARCH_BAR (cal_search), NULL, items);
	make_suboptions (cal_search);

	return cal_search;
}

/**
 * cal_search_bar_new:
 * flags: bitfield of items to appear in the search menu
 * 
 * Creates a new calendar search bar.
 * 
 * Return value: A newly-created calendar search bar.  You should connect to the
 * "sexp_changed" signal to monitor changes in the generated sexps.
 **/
GtkWidget *
cal_search_bar_new (guint32 flags)
{
	CalSearchBar *cal_search;

	cal_search = g_object_new (TYPE_CAL_SEARCH_BAR, NULL);
	return GTK_WIDGET (cal_search_bar_construct (cal_search, flags));
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

/* Creates a sorted array of categories based on the original one; copies the
 * string values.
 */
static GPtrArray *
sort_categories (GPtrArray *categories)
{
	GPtrArray *c;
	int i;

	c = g_ptr_array_new ();
	g_ptr_array_set_size (c, categories->len);

	for (i = 0; i < categories->len; i++)
		c->pdata[i] = g_strdup (categories->pdata[i]);

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

	g_return_if_fail (IS_CAL_SEARCH_BAR (cal_search));
	g_return_if_fail (categories != NULL);

	priv = cal_search->priv;

	g_assert (priv->categories != NULL);
	free_categories (priv->categories);

	priv->categories = sort_categories (categories);
	make_suboptions (cal_search);
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
