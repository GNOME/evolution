/*
 * Evolution calendar - Search bar widget for calendar views
 *
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/e-categories.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-categories.h>
#include <filter/rule-editor.h>

#include "cal-search-bar.h"
#include "calendar-component.h"
#include "memos-component.h"
#include "tasks-component.h"

#include "e-util/e-util.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

typedef struct CALSearchBarItem {
	 ESearchBarItem search;
	 const gchar *image;
} CALSearchBarItem;

static ESearchBarItem calendar_search_items[] = {
	E_FILTERBAR_ADVANCED,
	{NULL, 0, 0},
	E_FILTERBAR_SAVE,
	E_FILTERBAR_EDIT,
	{NULL, -1, 0}
};

/* IDs and option items for the ESearchBar */
enum {
	SEARCH_SUMMARY_CONTAINS,
	SEARCH_DESCRIPTION_CONTAINS,
	SEARCH_ANY_FIELD_CONTAINS,
	SEARCH_CATEGORY_IS,
	SEARCH_COMMENT_CONTAINS,
	SEARCH_LOCATION_CONTAINS,
	SEARCH_ATTENDEE_CONTAINS

};

/* Comments are disabled because they are kind of useless right now, see bug 33247 */
static ESearchBarItem search_option_items[] = {
	{ (gchar *) N_("Summary contains"), SEARCH_SUMMARY_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ (gchar *) N_("Description contains"), SEARCH_DESCRIPTION_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ (gchar *) N_("Category is"), SEARCH_CATEGORY_IS, ESB_ITEMTYPE_RADIO },
	{ (gchar *) N_("Comment contains"), SEARCH_COMMENT_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ (gchar *) N_("Location contains"), SEARCH_LOCATION_CONTAINS, ESB_ITEMTYPE_RADIO },
	{ (gchar *) N_("Any field contains"), SEARCH_ANY_FIELD_CONTAINS, ESB_ITEMTYPE_RADIO },
};

/* IDs for the categories suboptions */

typedef enum {
	CATEGORIES_ALL,
	CATEGORIES_UNMATCHED,
	LAST_FIELD
} common_search_options;

typedef enum {
	N_DAY_TASK = LAST_FIELD,
	ACTIVE_TASK,
	OVERDUE_TASK,
	COMPLETED_TASK,
	TASK_WITH_ATTACHMENT,
	TASK_LAST_FIELD
} task_search_options;

typedef enum  {
	ACTIVE_APPONTMENT = LAST_FIELD,
	N_DAY_APPOINTMENT,
	CAL_LAST_FIELD
} cal_search_options;

/* We add 2 to the offset to include the separators used to differenciate the quick search queries. */
#define CATEGORIES_TASKS_OFFSET (TASK_LAST_FIELD + 2)
#define CATEGORIES_MEMOS_OFFSET (LAST_FIELD + 1)
#define CATEGORIES_CALENDAR_OFFSET (CAL_LAST_FIELD + 2)

/* Private part of the CalSearchBar structure */
struct CalSearchBarPrivate {
	/* Array of categories */
	GPtrArray *categories;

	RuleContext *search_context;
	FilterRule *search_rule;
	guint32 view_flag;

	time_t start;
	time_t end;
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

G_DEFINE_TYPE (CalSearchBar, cal_search_bar, E_FILTER_BAR_TYPE)

/* Class initialization function for the calendar search bar */
static void
cal_search_bar_class_init (CalSearchBarClass *klass)
{

	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	ESearchBarClass *search_bar_class = E_SEARCH_BAR_CLASS (klass);

	cal_search_bar_signals[SEXP_CHANGED] =
		g_signal_new ("sexp_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalSearchBarClass, sexp_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	cal_search_bar_signals[CATEGORY_CHANGED] =
		g_signal_new ("category_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalSearchBarClass, category_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	klass->sexp_changed = NULL;
	klass->category_changed = NULL;

	search_bar_class->search_activated = cal_search_bar_search_activated;
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

	priv->start = -1;
	priv->end = -1;
}

/* Frees an array of categories */
static void
free_categories (GPtrArray *categories)
{
	gint i;

	for (i = 0; i < categories->len; i++) {
		if (categories->pdata[i] == NULL)
			continue;
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

		if (priv->search_rule) {
			g_object_unref (priv->search_rule);
			priv->search_rule = NULL;
		}

		/* FIXME
		if (priv->search_context) {
			g_object_unref (priv->search_context);
			priv->search_context = NULL;
		}*/

		g_free (priv);
		cal_search->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (cal_search_bar_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (cal_search_bar_parent_class)->destroy) (object);
}



/* Emits the "sexp_changed" signal for the calendar search bar */
static void
notify_sexp_changed (CalSearchBar *cal_search, const gchar *sexp)
{
	g_signal_emit (GTK_OBJECT (cal_search), cal_search_bar_signals[SEXP_CHANGED], 0, sexp);
}

/* Returns the string of the currently selected category, NULL for "Unmatched" and "All
*/
static const gchar *
get_current_category (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;
	gint viewid, i = -1;

	priv = cal_search->priv;

	g_return_val_if_fail (priv->categories != NULL, NULL);

	viewid = e_search_bar_get_viewitem_id (E_SEARCH_BAR (cal_search));

	if (viewid == CATEGORIES_ALL || viewid == CATEGORIES_UNMATCHED)
		return NULL;

	if (priv->view_flag == CAL_SEARCH_TASKS_DEFAULT)
		i = viewid - CATEGORIES_TASKS_OFFSET;
	else if (priv->view_flag == CAL_SEARCH_MEMOS_DEFAULT)
		i = viewid - CATEGORIES_MEMOS_OFFSET;
	else if (priv->view_flag == CAL_SEARCH_CALENDAR_DEFAULT)
		i = viewid - CATEGORIES_CALENDAR_OFFSET;

	if (i >= 0 && i < priv->categories->len)
		return priv->categories->pdata[i];
	else
		return NULL;
}

/* Returns a sexp for the selected category in the drop-down menu.  The "All"
 * option is returned as (const gchar *) 1, and the "Unfiled" option is returned
 * as NULL.
 */
static gchar *
get_show_option_sexp (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;
	gint viewid;
	gchar *start, *end, *due, *ret = NULL;
	const gchar *category = NULL;
	time_t start_range, end_range;

	priv = cal_search->priv;
	viewid = e_search_bar_get_viewitem_id (E_SEARCH_BAR (cal_search));

	if (viewid == CATEGORIES_UNMATCHED)
		return g_strdup ("(has-categories? #f)"); /* Unfiled items */
	else if (viewid == CATEGORIES_ALL)
		return NULL; /* All items */

	switch (priv->view_flag) {
	case CAL_SEARCH_TASKS_DEFAULT:
		if (viewid == N_DAY_TASK) {
			start_range = time(NULL);
			end_range = time_add_day(start_range, 7);
			start = isodate_from_time_t (start_range);
			due = isodate_from_time_t (end_range);

			ret =  g_strdup_printf ("(due-in-time-range? (make-time \"%s\")"
					"                      (make-time \"%s\"))",
					start, due);

			g_free (start);
			g_free (due);

			return ret;
		} else if (viewid == ACTIVE_TASK) {
			/* Shows the tasks due for an year from now which are not completed yet*/
			start_range = time(NULL);
			end_range = time_add_day(start_range, 365);
			start = isodate_from_time_t (start_range);
			due = isodate_from_time_t (end_range);

			ret =  g_strdup_printf ("(and (due-in-time-range? (make-time \"%s\")"
					"                      (make-time \"%s\")) (not (is-completed?)))",
					start, due);

			g_free (start);
			g_free (due);

			return ret;
		} else if (viewid == OVERDUE_TASK) {
			/* Shows the tasks which are overdue from lower limit 1970 to the current time */
			start_range = 0;
			end_range = time (NULL);
			start = isodate_from_time_t (start_range);
			due = isodate_from_time_t (end_range);

			ret =  g_strdup_printf ("(and (due-in-time-range? (make-time \"%s\")"
					"                      (make-time \"%s\")) (not (is-completed?)))",
					start, due);

			g_free (start);
			g_free (due);

			return ret;
		} else if (viewid == COMPLETED_TASK)
			return g_strdup ("(is-completed?)");
		else if (viewid == TASK_WITH_ATTACHMENT)
			return g_strdup ("(has-attachments?)");
		break;
	case CAL_SEARCH_CALENDAR_DEFAULT:
		if (viewid == ACTIVE_APPONTMENT) {
			/* Shows next one year's Appointments */
			start_range = time (NULL);
			end_range = time_add_day (start_range, 365);
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			ret = g_strdup_printf ("(occur-in-time-range? (make-time \"%s\")"
					"                      (make-time \"%s\"))",
					start, end);

			cal_search->priv->start = start_range;
			cal_search->priv->end = end_range;

			g_free (start);
			g_free (end);

			return ret;
		} else if (viewid == N_DAY_APPOINTMENT) {
			start_range = time (NULL);
			end_range = time_add_day (start_range, 7);
			start = isodate_from_time_t (start_range);
			end = isodate_from_time_t (end_range);

			ret = g_strdup_printf ("(occur-in-time-range? (make-time \"%s\")"
					"                      (make-time \"%s\"))",
					start, end);

			cal_search->priv->start = start_range;
			cal_search->priv->end = end_range;

			g_free (start);
			g_free (end);

			return ret;
		}
		break;
	default:
		break;
	}

	category = get_current_category (cal_search);

	if (category != NULL)
		return g_strdup_printf ("(has-categories? \"%s\")", category);
	else
		return NULL;
}

/* Sets the query string to be (contains? "field" "text") */
static void
notify_e_cal_view_contains (CalSearchBar *cal_search, const gchar *field, const gchar *view)
{
	gchar *text = NULL;
	gchar *sexp;

	text = e_search_bar_get_text (E_SEARCH_BAR (cal_search));

	if (!text)
		return; /* This is an error in the UTF8 conversion, not an empty string! */

	if (text && *text)
	    sexp = g_strdup_printf ("(contains? \"%s\" \"%s\")", field, text);
	else
	    sexp = g_strdup ("(contains? \"summary\" \"\")"); /* Show all */

	g_free (text);

	/* Apply the selected view on search */
	if (view && *view) {
	    sexp = g_strconcat ("(and ",sexp, view, ")", NULL);
	}

	notify_sexp_changed (cal_search, sexp);

	g_free (sexp);
}

#if 0
/* Sets the query string to the appropriate match for categories */
static void
notify_category_is (CalSearchBar *cal_search)
{
	gchar *sexp;

	sexp = get_show_option_sexp (cal_search);

	if (!sexp)
		notify_sexp_changed (cal_search, "#t"); /* Match all */
	else
		notify_sexp_changed (cal_search, sexp);

	if (sexp)
		g_free (sexp);
}
#endif

/* Creates a new query from the values in the widgets and notifies upstream */
static void
regen_query (CalSearchBar *cal_search)
{
	gint id;
	gchar *show_option_sexp = NULL;
	gchar *sexp = NULL;
	GString *out = NULL;
	EFilterBar *efb = (EFilterBar *) cal_search;

	/* Fetch the data from the ESearchBar's entry widgets */
	id = e_search_bar_get_item_id (E_SEARCH_BAR (cal_search));

	cal_search->priv->start = -1;
	cal_search->priv->end = -1;

	/* Get the selected view */
	show_option_sexp = get_show_option_sexp (cal_search);

	/* Generate the different types of queries */
	switch (id) {
	case SEARCH_ANY_FIELD_CONTAINS:
		notify_e_cal_view_contains (cal_search, "any", show_option_sexp);
		break;

	case SEARCH_SUMMARY_CONTAINS:
		notify_e_cal_view_contains (cal_search, "summary", show_option_sexp);
		break;

	case SEARCH_DESCRIPTION_CONTAINS:
		notify_e_cal_view_contains (cal_search, "description", show_option_sexp);
		break;

	case SEARCH_COMMENT_CONTAINS:
		notify_e_cal_view_contains (cal_search, "comment", show_option_sexp);
		break;

	case SEARCH_LOCATION_CONTAINS:
		notify_e_cal_view_contains (cal_search, "location", show_option_sexp);
		break;
	case SEARCH_ATTENDEE_CONTAINS:
		notify_e_cal_view_contains (cal_search, "attendee", show_option_sexp);
		break;
	case  E_FILTERBAR_ADVANCED_ID:
		out = g_string_new ("");
		filter_rule_build_code (efb->current_query, out);

		if (show_option_sexp && *show_option_sexp)
		    sexp = g_strconcat ("(and ", out->str, show_option_sexp, ")", NULL);

		notify_sexp_changed (cal_search, sexp ? sexp : out->str);

		g_string_free (out, TRUE);
		g_free(sexp);
		break;

	default:
		g_return_if_reached ();
	}

	g_free (show_option_sexp);
}

#if 0
static void
regen_view_query (CalSearchBar *cal_search)
{
        const gchar *category;
	notify_category_is (cal_search);

	category = cal_search_bar_get_category (cal_search);
	g_signal_emit (GTK_OBJECT (cal_search), cal_search_bar_signals[CATEGORY_CHANGED], 0, category);
}
#endif

/* search_activated handler for the calendar search bar */
static void
cal_search_bar_search_activated (ESearchBar *search)
{
	CalSearchBar *cal_search;

	cal_search = CAL_SEARCH_BAR (search);
	regen_query (cal_search);
}

static GtkWidget *
generate_viewoption_menu (CALSearchBarItem *subitems)
{
	GtkWidget *menu, *menu_item;
	gint i = 0;

	menu = gtk_menu_new ();

	for (i = 0; subitems[i].search.id != -1; ++i) {
		if (subitems[i].search.text) {
			gchar *str = NULL;
			str = e_str_without_underscores (subitems[i].search.text);
			menu_item = gtk_image_menu_item_new_with_label (str);
                        if (subitems[i].image) {
                                GtkWidget *image;

                                image = gtk_image_new_from_file (
                                        subitems[i].image);
                                gtk_image_menu_item_set_image (
                                        GTK_IMAGE_MENU_ITEM (menu_item),
                                        image);
                        }
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

static void
setup_category_options (CalSearchBar *cal_search, CALSearchBarItem *subitems, gint index, gint offset)
{
	CalSearchBarPrivate *priv;
	gint i;

	priv = cal_search->priv;

	if (priv->categories->len > 0) {
		subitems[index].search.text = NULL; /* separator */
		subitems[index].search.id = 0;
		subitems[index].image = NULL;

		for (i = 0; i < priv->categories->len; i++) {
			const gchar *category;

			category = priv->categories->pdata[i] ? priv->categories->pdata [i] : "";

			/* The search.text field should not be free'd */
			subitems[i + offset].search.text = (gchar *) category;
			subitems[i + offset].search.id = i + offset;
			subitems[i + offset].image = e_categories_get_icon_file_for (category);
		}
		index = i + offset;
	}

	subitems[index].search.id = -1; /* terminator */
	subitems[index].search.text = NULL;
	subitems[index].image = NULL;
}

/* Creates the suboptions menu for the ESearchBar with the list of categories */
static void
make_suboptions (CalSearchBar *cal_search)
{
	CalSearchBarPrivate *priv;
	CALSearchBarItem *subitems = NULL;
	GtkWidget *menu;

	priv = cal_search->priv;

	g_return_if_fail (priv->categories != NULL);

	/* Categories plus "all", "unmatched", separator, terminator */

	/* All, unmatched, separator */

	if (priv->view_flag == CAL_SEARCH_TASKS_DEFAULT) {
		subitems = g_new (CALSearchBarItem, priv->categories->len + CATEGORIES_TASKS_OFFSET + 1);

		subitems[0].search.text = _("Any Category");
		subitems[0].search.id = CATEGORIES_ALL;
		subitems[0].image = NULL;

		subitems[1].search.text = _("Unmatched");
		subitems[1].search.id = CATEGORIES_UNMATCHED;
		subitems[1].image = NULL;

		subitems[2].search.text = NULL;
		subitems[2].search.id = 0;
		subitems[2].image = NULL;

		subitems[3].search.text = _("Next 7 Days' Tasks");
		subitems[3].search.id = N_DAY_TASK;
		subitems[3].image = NULL;

		subitems[4].search.text = _("Active Tasks");
		subitems[4].search.id = ACTIVE_TASK;
		subitems[4].image = NULL;

		subitems[5].search.text = _("Overdue Tasks");
		subitems[5].search.id = OVERDUE_TASK;
		subitems[5].image = NULL;

		subitems[6].search.text = _("Completed Tasks");
		subitems[6].search.id = COMPLETED_TASK;
		subitems[6].image = NULL;

		subitems[7].search.text = _("Tasks with Attachments");
		subitems[7].search.id = TASK_WITH_ATTACHMENT;
		subitems[7].image = NULL;

		/* All the other items */
		setup_category_options (cal_search, subitems, 8, CATEGORIES_TASKS_OFFSET);

		menu = generate_viewoption_menu (subitems);
		e_search_bar_set_viewoption_menu ((ESearchBar *)cal_search, menu);

	} else if (priv->view_flag == CAL_SEARCH_MEMOS_DEFAULT) {
		subitems = g_new (CALSearchBarItem, priv->categories->len + CATEGORIES_MEMOS_OFFSET + 1);

		/* All, unmatched, separator */

		subitems[0].search.text = _("Any Category");
		subitems[0].search.id = CATEGORIES_ALL;
		subitems[0].image = NULL;

		subitems[1].search.text = _("Unmatched");
		subitems[1].search.id = CATEGORIES_UNMATCHED;
		subitems[1].image = NULL;

		/* All the other items */
		setup_category_options (cal_search, subitems, 2, CATEGORIES_MEMOS_OFFSET);

		menu = generate_viewoption_menu (subitems);
		e_search_bar_set_viewoption_menu ((ESearchBar *)cal_search, menu);

	} else if (priv->view_flag == CAL_SEARCH_CALENDAR_DEFAULT) {
		subitems = g_new (CALSearchBarItem, priv->categories->len + CATEGORIES_CALENDAR_OFFSET + 1);

		/* All, unmatched, separator */

		subitems[0].search.text = _("Any Category");
		subitems[0].search.id = CATEGORIES_ALL;
		subitems[0].image = NULL;

		subitems[1].search.text = _("Unmatched");
		subitems[1].search.id = CATEGORIES_UNMATCHED;
		subitems[1].image = NULL;

		subitems[2].search.text = NULL;
		subitems[2].search.id = 0;
		subitems[2].image = NULL;

		subitems[3].search.text = _("Active Appointments");
		subitems[3].search.id = ACTIVE_APPONTMENT;
		subitems[3].image = NULL;

		subitems[4].search.text = _("Next 7 Days' Appointments");
		subitems[4].search.id = N_DAY_APPOINTMENT;
		subitems[4].image = NULL;

		/* All the other items */
		setup_category_options (cal_search, subitems, 5, CATEGORIES_CALENDAR_OFFSET);

		menu = generate_viewoption_menu (subitems);
		e_search_bar_set_viewoption_menu ((ESearchBar *)cal_search, menu);
	}

	if (subitems != NULL)
		g_free (subitems);
}

static void
search_menu_activated (ESearchBar *esb, gint id)
{
	if (id == E_FILTERBAR_ADVANCED_ID)
		e_search_bar_set_item_id (esb, id);
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
	gint i, j;
	gchar *xmlfile = NULL;
	gchar *userfile = NULL;
	FilterPart *part;
	RuleContext *search_context;
	FilterRule  *search_rule;
	const gchar *base_dir;

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
	search_context = rule_context_new ();
	cal_search->priv->view_flag = flags;

	rule_context_add_part_set (search_context, "partset", filter_part_get_type (),
			rule_context_add_part, rule_context_next_part);
	rule_context_add_rule_set (search_context, "ruleset", filter_rule_get_type (),
			rule_context_add_rule, rule_context_next_rule);

	if (flags == CAL_SEARCH_MEMOS_DEFAULT) {
		base_dir = memos_component_peek_base_directory (memos_component_peek ());
		xmlfile = g_build_filename (SEARCH_RULE_DIR, "memotypes.xml", NULL);
	} else if (flags == CAL_SEARCH_TASKS_DEFAULT) {
		base_dir = tasks_component_peek_base_directory (tasks_component_peek ());
		xmlfile = g_build_filename (SEARCH_RULE_DIR, "tasktypes.xml", NULL);
	} else {
		base_dir = calendar_component_peek_base_directory (calendar_component_peek ());
		xmlfile = g_build_filename (SEARCH_RULE_DIR, "caltypes.xml", NULL);
	}

	userfile = g_build_filename (base_dir, "searches.xml", NULL);

	g_object_set_data_full (G_OBJECT (search_context), "user", userfile, g_free);
	g_object_set_data_full (G_OBJECT (search_context), "system", xmlfile, g_free);

	rule_context_load (search_context, xmlfile, userfile);
	search_rule = filter_rule_new ();
	part = rule_context_next_part (search_context, NULL);

	if (part == NULL)
		g_warning ("Could not load calendar search; no parts.");
	else
		filter_rule_add_part (search_rule, filter_part_clone (part));

	e_filter_bar_new_construct (search_context, xmlfile, userfile, NULL, cal_search,
			(EFilterBar*) cal_search );
	e_search_bar_set_menu ((ESearchBar *) cal_search, calendar_search_items);

	g_signal_connect ((ESearchBar *) cal_search, "menu_activated", G_CALLBACK (search_menu_activated), cal_search);

	make_suboptions (cal_search);

	cal_search->priv->search_rule = search_rule;
	cal_search->priv->search_context = search_context;

	g_free (xmlfile);
	g_free (userfile);

	return cal_search;
}

/**
 * cal_search_bar_new:
 * flags: bitfield of items to appear in the search menu
 *
 * creates a new calendar search bar.
 *
 * return value: a newly-created calendar search bar.  you should connect to the
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
static gint
compare_categories_cb (gconstpointer a, gconstpointer b)
{
	const gchar **ca, **cb;

	ca = (const gchar **) a;
	cb = (const gchar **) b;

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
	gint i;

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

	g_return_if_fail (priv->categories != NULL);
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
const gchar *
cal_search_bar_get_category (CalSearchBar *cal_search)
{
	const gchar *category;

	category = get_current_category (cal_search);

	return category;
}

void
cal_search_bar_get_time_range (CalSearchBar *cal_search, time_t *start, time_t *end)
{
	CalSearchBarPrivate *priv;

	g_return_if_fail (IS_CAL_SEARCH_BAR (cal_search));

	priv = cal_search->priv;

	*start = priv->start;
	*end = priv->end;
}

