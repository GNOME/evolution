/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ECalendarTable - displays the CalComponent objects in a table (an ETable).
 * Used for calendar events and tasks.
 */

#include <config.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gnome.h>
#include <gtk/gtkinvisible.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/e-table/e-cell-checkbox.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-combo.h>
#include <gal/widgets/e-popup-menu.h>
#include <e-util/e-dialog-utils.h>
#include <widgets/misc/e-cell-date-edit.h>
#include <widgets/misc/e-cell-percent.h>
#include "e-comp-editor-registry.h"
#include "e-calendar-table.h"
#include "e-cell-date-edit-text.h"
#include "calendar-config.h"
#include "calendar-model.h"
#include "print.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/task-editor.h"

/* Pixmaps. */
#include "art/task.xpm"
#include "art/task-recurring.xpm"
#include "art/task-assigned.xpm"
#include "art/task-assigned-to.xpm"

#include "art/check-filled.xpm"

extern ECompEditorRegistry *comp_editor_registry;

static void e_calendar_table_class_init		(ECalendarTableClass *class);
static void e_calendar_table_init		(ECalendarTable	*cal_table);
static void e_calendar_table_destroy		(GtkObject	*object);

static void e_calendar_table_on_double_click	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEvent	*event,
						 ECalendarTable *cal_table);
static gint e_calendar_table_show_popup_menu    (ETable *table,
						 GdkEvent *gdk_event,
						 ECalendarTable *cal_table);

static gint e_calendar_table_on_right_click	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEvent       *event,
						 ECalendarTable *cal_table);
static gboolean e_calendar_table_on_popup_menu  (GtkWidget *widget,
						 gpointer data);

static void e_calendar_table_on_open_task	(GtkWidget	*menuitem,
						 gpointer	 data);
static void e_calendar_table_on_save_as	        (GtkWidget	*menuitem,
						 gpointer	 data);
static void e_calendar_table_on_print_task      (GtkWidget	*menuitem,
						 gpointer	 data);
static void e_calendar_table_on_cut             (GtkWidget      *menuitem,
						 gpointer        data);
static void e_calendar_table_on_copy            (GtkWidget      *menuitem,
						 gpointer        data);
static void e_calendar_table_on_paste           (GtkWidget      *menuitem,
						 gpointer        data);
static void e_calendar_table_on_assign          (GtkWidget      *menuitem,
						 gpointer        data);
static void e_calendar_table_on_forward         (GtkWidget      *menuitem,
						 gpointer        data);
static gint e_calendar_table_on_key_press	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEventKey	*event,
						 ECalendarTable *cal_table);

static void selection_clear_event               (GtkWidget *invisible,
						 GdkEventSelection *event,
						 ECalendarTable *cal_table);
static void selection_received                  (GtkWidget *invisible,
						 GtkSelectionData *selection_data,
						 guint time,
						 ECalendarTable *cal_table);
static void selection_get                       (GtkWidget *invisible,
						 GtkSelectionData *selection_data,
						 guint info,
						 guint time_stamp,
						 ECalendarTable *cal_table);
static struct tm e_calendar_table_get_current_time (ECellDateEdit *ecde,
						    gpointer data);
static void mark_row_complete_cb (int model_row, gpointer data);


/* The icons to represent the task. */
#define E_CALENDAR_MODEL_NUM_ICONS	4
static char** icon_xpm_data[E_CALENDAR_MODEL_NUM_ICONS] = {
	task_xpm, task_recurring_xpm, task_assigned_xpm, task_assigned_to_xpm
};
static GdkPixbuf* icon_pixbufs[E_CALENDAR_MODEL_NUM_ICONS] = { 0 };

static GtkTableClass *parent_class;
static GdkAtom clipboard_atom = GDK_NONE;

E_MAKE_TYPE (e_calendar_table, "ECalendarTable", ECalendarTable, e_calendar_table_class_init,
	     e_calendar_table_init, GTK_TYPE_TABLE);

static void
e_calendar_table_class_init (ECalendarTableClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	/* Method override */
	object_class->destroy		= e_calendar_table_destroy;

#if 0
	widget_class->realize		= e_calendar_table_realize;
	widget_class->unrealize		= e_calendar_table_unrealize;
	widget_class->style_set		= e_calendar_table_style_set;
 	widget_class->size_allocate	= e_calendar_table_size_allocate;
	widget_class->focus_in_event	= e_calendar_table_focus_in;
	widget_class->focus_out_event	= e_calendar_table_focus_out;
	widget_class->key_press_event	= e_calendar_table_key_press;
#endif

	/* clipboard atom */
	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
}

/* Compares two priority values, which may not exist */
static int
compare_priorities (int *a, int *b)
{
	if (a && b) {
		if (*a < *b)
			return -1;
		else if (*a > *b)
			return 1;
		else
			return 0;
	} else if (a)
		return -1;
	else if (b)
		return 1;
	else
		return 0;
}

/* Comparison function for the task-sort column.  Sorts by due date and then by
 * priority.
 *
 * FIXME: Does this ever get called?? It doesn't seem to.
 * I specified that the table should be sorted by this column, but it still
 * never calls this function.
 * Also, this assumes it is passed pointers to CalComponents, but I think it
 * may just be passed pointers to the 2 cell values.
 */
static gint
task_compare_cb (gconstpointer a, gconstpointer b)
{
	CalComponent *ca, *cb;
	CalComponentDateTime due_a, due_b;
	int *prio_a, *prio_b;
	int retval;

	ca = CAL_COMPONENT (a);
	cb = CAL_COMPONENT (b);

	cal_component_get_due (ca, &due_a);
	cal_component_get_due (cb, &due_b);
	cal_component_get_priority (ca, &prio_a);
	cal_component_get_priority (cb, &prio_b);

	if (due_a.value && due_b.value) {
		int v;

		/* FIXME: TIMEZONES. But currently we have no way to get the
		   CalClient, so we can't get the timezone. */
		v = icaltime_compare (*due_a.value, *due_b.value);

		if (v == 0)
			retval = compare_priorities (prio_a, prio_b);
		else
			retval = v;
	} else if (due_a.value)
		retval = -1;
	else if (due_b.value)
		retval = 1;
	else
		retval = compare_priorities (prio_a, prio_b);

	cal_component_free_datetime (&due_a);
	cal_component_free_datetime (&due_b);

	if (prio_a)
		cal_component_free_priority (prio_a);

	if (prio_b)
		cal_component_free_priority (prio_b);

	return retval;
}

static gint
date_compare_cb (gconstpointer a, gconstpointer b)
{
	ECellDateEditValue *dv1 = (ECellDateEditValue *) a;
	ECellDateEditValue *dv2 = (ECellDateEditValue *) b;
	struct icaltimetype tt;

	/* First check if either is NULL. NULL dates sort last. */
	if (!dv1 || !dv2) {
		if (dv1 == dv2)
			return 0;
		else if (dv1)
			return -1;
		else
			return 1;
	}

	/* Copy the 2nd value and convert it to the same timezone as the
	   first. */
	tt = dv2->tt;

	icaltimezone_convert_time (&tt, dv2->zone, dv1->zone);

	/* Now we can compare them. */

	return icaltime_compare (dv1->tt, tt);
}

static gint
percent_compare_cb (gconstpointer a, gconstpointer b)
{
	int percent1 = GPOINTER_TO_INT (a);
	int percent2 = GPOINTER_TO_INT (b);
	int retval;

	if (percent1 > percent2)
		retval = 1;
	else if (percent1 < percent2)
		retval = -1;
	else
		retval = 0;

	return retval;
}

static gint
priority_compare_cb (gconstpointer a, gconstpointer b)
{
	int priority1, priority2;

	priority1 = cal_util_priority_from_string ((const char*) a);
	priority2 = cal_util_priority_from_string ((const char*) b);

	/* We change undefined priorities so they appear after 'Low'. */
	if (priority1 <= 0)
		priority1 = 10;
	if (priority2 <= 0)
		priority2 = 10;

	/* We'll just use the ordering of the priority values. */
	if (priority1 < priority2)
		return -1;
	else if (priority1 > priority2)
		return 1;
	else
		return 0;
}

static void
e_calendar_table_init (ECalendarTable *cal_table)
{
	GtkWidget *table;
	ETable *e_table;
	ECell *cell, *popup_cell;
	ETableExtras *extras;
	gint i;
	GdkPixbuf *pixbuf;
	GList *strings;

	/* Create the model */

	cal_table->model = calendar_model_new ();

	/* Create the header columns */

	extras = e_table_extras_new();

	/*
	 * Normal string fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
		      "bold_column", CAL_COMPONENT_FIELD_OVERDUE,
		      "color_column", CAL_COMPONENT_FIELD_COLOR,
		      NULL);

	e_table_extras_add_cell (extras, "calstring", cell);


	/*
	 * Date fields.
	 */
	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
		      "bold_column", CAL_COMPONENT_FIELD_OVERDUE,
		      "color_column", CAL_COMPONENT_FIELD_COLOR,
		      NULL);

	popup_cell = e_cell_date_edit_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);
	e_table_extras_add_cell (extras, "dateedit", popup_cell);
	cal_table->dates_cell = E_CELL_DATE_EDIT (popup_cell);

	e_cell_date_edit_set_get_time_callback (E_CELL_DATE_EDIT (popup_cell),
						e_calendar_table_get_current_time,
						cal_table, NULL);


	/*
	 * Combo fields.
	 */

	/* Classification field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
		      "bold_column", CAL_COMPONENT_FIELD_OVERDUE,
		      "color_column", CAL_COMPONENT_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("Public"));
	strings = g_list_append (strings, (char*) _("Private"));
	strings = g_list_append (strings, (char*) _("Confidential"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "classification", popup_cell);

	/* Priority field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
		      "bold_column", CAL_COMPONENT_FIELD_OVERDUE,
		      "color_column", CAL_COMPONENT_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("High"));
	strings = g_list_append (strings, (char*) _("Normal"));
	strings = g_list_append (strings, (char*) _("Low"));
	strings = g_list_append (strings, (char*) _("Undefined"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "priority", popup_cell);

	/* Percent field. */
	cell = e_cell_percent_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
		      "bold_column", CAL_COMPONENT_FIELD_OVERDUE,
		      "color_column", CAL_COMPONENT_FIELD_COLOR,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("0%"));
	strings = g_list_append (strings, (char*) _("10%"));
	strings = g_list_append (strings, (char*) _("20%"));
	strings = g_list_append (strings, (char*) _("30%"));
	strings = g_list_append (strings, (char*) _("40%"));
	strings = g_list_append (strings, (char*) _("50%"));
	strings = g_list_append (strings, (char*) _("60%"));
	strings = g_list_append (strings, (char*) _("70%"));
	strings = g_list_append (strings, (char*) _("80%"));
	strings = g_list_append (strings, (char*) _("90%"));
	strings = g_list_append (strings, (char*) _("100%"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "percent", popup_cell);

	/* Transparency field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
		      "bold_column", CAL_COMPONENT_FIELD_OVERDUE,
		      "color_column", CAL_COMPONENT_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("Free"));
	strings = g_list_append (strings, (char*) _("Busy"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "transparency", popup_cell);

	/* Status field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
		      "bold_column", CAL_COMPONENT_FIELD_OVERDUE,
		      "color_column", CAL_COMPONENT_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("Not Started"));
	strings = g_list_append (strings, (char*) _("In Progress"));
	strings = g_list_append (strings, (char*) _("Completed"));
	strings = g_list_append (strings, (char*) _("Cancelled"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "calstatus", popup_cell);

	/* Task sorting field */
	/* FIXME: This column should not be displayed, but ETableExtras requires
	 * its shit to be visible columns listed in the XML spec.
	 */
	e_table_extras_add_compare (extras, "task-sort", task_compare_cb);

	e_table_extras_add_compare (extras, "date-compare",
				    date_compare_cb);
	e_table_extras_add_compare (extras, "percent-compare",
				    percent_compare_cb);
	e_table_extras_add_compare (extras, "priority-compare",
				    priority_compare_cb);

	/* Create pixmaps */

	if (!icon_pixbufs[0])
		for (i = 0; i < E_CALENDAR_MODEL_NUM_ICONS; i++) {
			icon_pixbufs[i] = gdk_pixbuf_new_from_xpm_data (
				(const char **) icon_xpm_data[i]);
		}

	cell = e_cell_toggle_new (0, E_CALENDAR_MODEL_NUM_ICONS, icon_pixbufs);
	e_table_extras_add_cell(extras, "icon", cell);
	e_table_extras_add_pixbuf(extras, "icon", icon_pixbufs[0]);

	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) check_filled_xpm);
	e_table_extras_add_pixbuf(extras, "complete", pixbuf);
	gdk_pixbuf_unref(pixbuf);

	/* Create the table */

	table = e_table_scrolled_new_from_spec_file (E_TABLE_MODEL (cal_table->model),
						     extras,
						     EVOLUTION_ETSPECDIR "/e-calendar-table.etspec",
						     NULL);
	g_object_unref (extras);

	cal_table->etable = table;
	gtk_table_attach (GTK_TABLE (cal_table), table, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (table);


	e_table = e_table_scrolled_get_table (E_TABLE_SCROLLED (table));
	g_signal_connect (e_table, "double_click", G_CALLBACK (e_calendar_table_on_double_click), cal_table);
	g_signal_connect (e_table, "right_click", G_CALLBACK (e_calendar_table_on_right_click), cal_table);
	g_signal_connect (e_table, "key_press", G_CALLBACK (e_calendar_table_on_key_press), cal_table);
	g_signal_connect (e_table, "popup_menu", G_CALLBACK (e_calendar_table_on_popup_menu), cal_table);

	/* Set up the invisible widget for the clipboard selections */
	cal_table->invisible = gtk_invisible_new ();
	gtk_selection_add_target (cal_table->invisible,
				  clipboard_atom,
				  GDK_SELECTION_TYPE_STRING,
				  0);
	g_signal_connect (cal_table->invisible, "selection_get",
			  G_CALLBACK (selection_get), cal_table);
	g_signal_connect (cal_table->invisible, "selection_clear_event",
			  G_CALLBACK (selection_clear_event), cal_table);
	g_signal_connect (cal_table->invisible, "selection_received",
			  G_CALLBACK (selection_received), cal_table);

	cal_table->clipboard_selection = NULL;
}


/**
 * e_calendar_table_new:
 * @Returns: a new #ECalendarTable.
 *
 * Creates a new #ECalendarTable.
 **/
GtkWidget *
e_calendar_table_new (void)
{
	GtkWidget *cal_table;

	cal_table = GTK_WIDGET (g_object_new (e_calendar_table_get_type (), NULL));

	return cal_table;
}


/**
 * e_calendar_table_get_model:
 * @cal_table: A calendar table.
 * 
 * Queries the calendar data model that a calendar table is using.
 * 
 * Return value: A calendar model.
 **/
CalendarModel *
e_calendar_table_get_model (ECalendarTable *cal_table)
{
	g_return_val_if_fail (cal_table != NULL, NULL);
	g_return_val_if_fail (E_IS_CALENDAR_TABLE (cal_table), NULL);

	return cal_table->model;
}


static void
e_calendar_table_destroy (GtkObject *object)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (object);

	if (cal_table->model) {
		g_object_unref (cal_table->model);
		cal_table->model = NULL;
	}
	
	if (cal_table->invisible) {
		gtk_widget_destroy (cal_table->invisible);
		cal_table->invisible = NULL;
	}
	if (cal_table->clipboard_selection) {
		g_free (cal_table->clipboard_selection);
		cal_table->clipboard_selection = NULL;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

/**
 * e_calendar_table_get_table:
 * @cal_table: A calendar table.
 * 
 * Queries the #ETable widget that the calendar table is using.
 * 
 * Return value: The #ETable widget that the calendar table uses to display its
 * data.
 **/
ETable *
e_calendar_table_get_table (ECalendarTable *cal_table)
{
	g_return_val_if_fail (cal_table != NULL, NULL);
	g_return_val_if_fail (E_IS_CALENDAR_TABLE (cal_table), NULL);

	return e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
}

/**
 * e_calendar_table_complete_selected:
 * @cal_table: A calendar table
 * 
 * Marks the selected items as completed
 **/
void
e_calendar_table_complete_selected (ECalendarTable *cal_table)
{
	ETable *etable;

	g_return_if_fail (cal_table != NULL);
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, mark_row_complete_cb, cal_table);
}

/* Used from e_table_selected_row_foreach(); puts the selected row number in an
 * int pointed to by the closure data.
 */
static void
get_selected_row_cb (int model_row, gpointer data)
{
	int *row;

	row = data;
	*row = model_row;
}

/* Returns the component that is selected in the table; only works if there is
 * one and only one selected row.
 */
static CalComponent *
get_selected_comp (ECalendarTable *cal_table)
{
	ETable *etable;
	int row;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	if (e_table_selected_count (etable) != 1)
		return NULL;

	row = -1;
	e_table_selected_row_foreach (etable,
				      get_selected_row_cb,
				      &row);
	g_assert (row != -1);

	return calendar_model_get_component (cal_table->model, row);
}

struct get_selected_uids_closure {
	ECalendarTable *cal_table;
	GSList *uids;
};

/* Used from e_table_selected_row_foreach(), builds a list of the selected UIDs */
static void
add_uid_cb (int model_row, gpointer data)
{
	struct get_selected_uids_closure *closure;
	CalComponent *comp;
	const char *uid;

	closure = data;

	comp = calendar_model_get_component (closure->cal_table->model, model_row);
	cal_component_get_uid (comp, &uid);

	closure->uids = g_slist_prepend (closure->uids, (char *) uid);
}

static GSList *
get_selected_uids (ECalendarTable *cal_table)
{
	struct get_selected_uids_closure closure;
	ETable *etable;

	closure.cal_table = cal_table;
	closure.uids = NULL;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, add_uid_cb, &closure);

	return closure.uids;
}

/* Deletes all of the selected components in the table */
static void
delete_selected_components (ECalendarTable *cal_table)
{
	CalClient *client;
	GSList *uids, *l;

	uids = get_selected_uids (cal_table);

	client = calendar_model_get_cal_client (cal_table->model);

	calendar_model_set_status_message (e_calendar_table_get_model (cal_table),
					   _("Deleting selected objects"));

	for (l = uids; l; l = l->next) {
		const char *uid;
		
		uid = l->data;

		delete_error_dialog (cal_client_remove_object (client, uid), CAL_COMPONENT_TODO);
	}

	calendar_model_set_status_message (e_calendar_table_get_model (cal_table), NULL);

	g_slist_free (uids);
}

/**
 * e_calendar_table_delete_selected:
 * @cal_table: A calendar table.
 * 
 * Deletes the selected components in the table; asks the user first.
 **/
void
e_calendar_table_delete_selected (ECalendarTable *cal_table)
{
	ETable *etable;
	int n_selected;
	CalComponent *comp;

	g_return_if_fail (cal_table != NULL);
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));

	n_selected = e_table_selected_count (etable);
	if (n_selected <= 0)
		return;

	if (n_selected == 1)
		comp = get_selected_comp (cal_table);
	else
		comp = NULL;

	/* FIXME: this may be something other than a TODO component */

	if (delete_component_dialog (comp, FALSE, n_selected, CAL_COMPONENT_TODO,
				     GTK_WIDGET (cal_table)))
		delete_selected_components (cal_table);
}

/**
 * e_calendar_table_cut_clipboard:
 * @cal_table: A calendar table.
 *
 * Cuts selected tasks in the given calendar table
 */
void
e_calendar_table_cut_clipboard (ECalendarTable *cal_table)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	e_calendar_table_copy_clipboard (cal_table);
	delete_selected_components (cal_table);
}

/* callback for e_table_selected_row_foreach */
static void
copy_row_cb (int model_row, gpointer data)
{
	ECalendarTable *cal_table;
	CalComponent *comp;
	gchar *comp_str;
	icalcomponent *child;

	cal_table = E_CALENDAR_TABLE (data);

	g_return_if_fail (cal_table->tmp_vcal != NULL);

	comp = calendar_model_get_component (cal_table->model, model_row);
	if (!comp)
		return;

	/* add timezones to the VCALENDAR component */
	cal_util_add_timezones_from_component (cal_table->tmp_vcal, comp);

	/* add the new component to the VCALENDAR component */
	comp_str = cal_component_get_as_string (comp);
	child = icalparser_parse_string (comp_str);
	if (child) {
		icalcomponent_add_component (cal_table->tmp_vcal,
					     icalcomponent_new_clone (child));
		icalcomponent_free (child);
	}

	g_free (comp_str);
}

/**
 * e_calendar_table_copy_clipboard:
 * @cal_table: A calendar table.
 *
 * Copies selected tasks into the clipboard
 */
void
e_calendar_table_copy_clipboard (ECalendarTable *cal_table)
{
	ETable *etable;
	char *comp_str;
	
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (cal_table->clipboard_selection) {
		g_free (cal_table->clipboard_selection);
		cal_table->clipboard_selection = NULL;
	}

	/* create temporary VCALENDAR object */
	cal_table->tmp_vcal = cal_util_new_top_level ();

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, copy_row_cb, cal_table);

	comp_str = icalcomponent_as_ical_string (cal_table->tmp_vcal);
	cal_table->clipboard_selection = g_strdup (comp_str);
	icalcomponent_free (cal_table->tmp_vcal);
	cal_table->tmp_vcal = NULL;

	gtk_selection_owner_set (cal_table->invisible, clipboard_atom, GDK_CURRENT_TIME);
}

/**
 * e_calendar_table_paste_clipboard:
 * @cal_table: A calendar table.
 *
 * Pastes tasks currently in the clipboard into the given calendar table
 */
void
e_calendar_table_paste_clipboard (ECalendarTable *cal_table)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	gtk_selection_convert (cal_table->invisible,
			       clipboard_atom,
			       GDK_SELECTION_TYPE_STRING,
			       GDK_CURRENT_TIME);
}

/* Opens a task in the task editor */
static void
open_task (ECalendarTable *cal_table, CalComponent *comp, gboolean assign)
{
	CompEditor *tedit;
	const char *uid;
	
	cal_component_get_uid (comp, &uid);

	tedit = e_comp_editor_registry_find (comp_editor_registry, uid);
	if (tedit == NULL) {
		tedit = COMP_EDITOR (task_editor_new (calendar_model_get_cal_client (cal_table->model)));

		comp_editor_edit_comp (tedit, comp);
		if (assign)
			task_editor_show_assignment (TASK_EDITOR (tedit));
		
		e_comp_editor_registry_add (comp_editor_registry, tedit, FALSE);
	}
	
	comp_editor_focus (tedit);
}

/* Opens the task in the specified row */
static void
open_task_by_row (ECalendarTable *cal_table, int row)
{
	CalComponent *comp;

	comp = calendar_model_get_component (cal_table->model, row);
	open_task (cal_table, comp, FALSE);
}

static void
e_calendar_table_on_double_click (ETable *table,
				  gint row, 
				  gint col,
				  GdkEvent *event,
				  ECalendarTable *cal_table)
{
	open_task_by_row (cal_table, row);
}

/* Used from e_table_selected_row_foreach() */
static void
mark_row_complete_cb (int model_row, gpointer data)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (data);
	calendar_model_mark_task_complete (cal_table->model, model_row);
}

/* Callback used for the "mark tasks as complete" menu item */
static void
mark_as_complete_cb (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table;
	ETable *etable;

	cal_table = E_CALENDAR_TABLE (data);

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, mark_row_complete_cb, cal_table);
}

/* Callback for the "delete tasks" menu item */
static void
delete_cb (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (data);
	e_calendar_table_delete_selected (cal_table);
}


enum {
	MASK_SINGLE	= 1 << 0,	/* For commands that work on 1 task. */
	MASK_MULTIPLE	= 1 << 1,	/* For commands for multiple tasks. */
	MASK_EDITABLE   = 1 << 2,       /* For commands disabled in read-only folders */
	MASK_ASSIGNABLE = 1 << 3        /* For non-task assignable backends */
};


static EPopupMenu tasks_popup_menu [] = {
	E_POPUP_ITEM (N_("_Open"), GTK_SIGNAL_FUNC (e_calendar_table_on_open_task), MASK_SINGLE),
	E_POPUP_ITEM (N_("_Save as..."), GTK_SIGNAL_FUNC (e_calendar_table_on_save_as), MASK_SINGLE),
	E_POPUP_ITEM (N_("_Print..."), GTK_SIGNAL_FUNC (e_calendar_table_on_print_task), MASK_SINGLE),

	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM (N_("C_ut"), GTK_SIGNAL_FUNC (e_calendar_table_on_cut), MASK_EDITABLE),
	E_POPUP_ITEM (N_("_Copy"), GTK_SIGNAL_FUNC (e_calendar_table_on_copy), 0),
	E_POPUP_ITEM (N_("_Paste"), GTK_SIGNAL_FUNC (e_calendar_table_on_paste), MASK_EDITABLE),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Assign Task"), GTK_SIGNAL_FUNC (e_calendar_table_on_assign), MASK_SINGLE | MASK_EDITABLE | MASK_ASSIGNABLE),
	E_POPUP_ITEM (N_("_Forward as iCalendar"), GTK_SIGNAL_FUNC (e_calendar_table_on_forward), MASK_SINGLE),
	E_POPUP_ITEM (N_("_Mark as Complete"), GTK_SIGNAL_FUNC (mark_as_complete_cb), MASK_SINGLE | MASK_EDITABLE),
	E_POPUP_ITEM (N_("_Mark Selected Tasks as Complete"), GTK_SIGNAL_FUNC (mark_as_complete_cb), MASK_MULTIPLE | MASK_EDITABLE),
	
	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Delete"), GTK_SIGNAL_FUNC (delete_cb), MASK_SINGLE | MASK_EDITABLE),
	E_POPUP_ITEM (N_("_Delete Selected Tasks"), GTK_SIGNAL_FUNC (delete_cb), MASK_MULTIPLE | MASK_EDITABLE),

	E_POPUP_TERMINATOR
};

static gint
e_calendar_table_show_popup_menu (ETable *table,
				  GdkEvent *gdk_event,
				  ECalendarTable *cal_table)
{
	int n_selected;
	int hide_mask = 0;
	int disable_mask = 0;
	GtkMenu *gtk_menu;

	n_selected = e_table_selected_count (table);
	if (n_selected <= 0)
		return TRUE;

	if (n_selected == 1)
		hide_mask = MASK_MULTIPLE;
	else
		hide_mask = MASK_SINGLE;

	if (cal_client_is_read_only (calendar_model_get_cal_client (e_calendar_table_get_model (cal_table))))
		disable_mask |= MASK_EDITABLE;

	if (cal_client_get_static_capability (calendar_model_get_cal_client (e_calendar_table_get_model (cal_table)),
					      CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT))
		disable_mask |= MASK_ASSIGNABLE;

        gtk_menu = e_popup_menu_create (tasks_popup_menu, disable_mask,
					hide_mask, cal_table);
                                                                            
        e_popup_menu (gtk_menu, gdk_event);

	return TRUE;
}

static gint
e_calendar_table_on_right_click (ETable *table,
				 gint row,
				 gint col,
				 GdkEvent *event,
				 ECalendarTable *cal_table)
{
	return e_calendar_table_show_popup_menu (table, event, cal_table);
}

static gboolean
e_calendar_table_on_popup_menu (GtkWidget *widget, gpointer data)
{
	ETable *table = E_TABLE(widget);
	g_return_if_fail(table);

	return e_calendar_table_show_popup_menu (table, NULL,
						 E_CALENDAR_TABLE(data));
}

static void
e_calendar_table_on_open_task (GtkWidget *menuitem,
			       gpointer	  data)
{
	ECalendarTable *cal_table;
	CalComponent *comp;

	cal_table = E_CALENDAR_TABLE (data);

	comp = get_selected_comp (cal_table);
	if (comp)
		open_task (cal_table, comp, FALSE);
}

static void
e_calendar_table_on_save_as (GtkWidget *widget, gpointer data)
{
	ECalendarTable *cal_table;
	CalClient *client;
	CalComponent *comp;
	char *filename;
	char *ical_string;
	FILE *file;

	cal_table = E_CALENDAR_TABLE (data);

	client = calendar_model_get_cal_client (cal_table->model);
	comp = get_selected_comp (cal_table);
	if (comp == NULL)
		return;
	
	filename = e_file_dialog_save (_("Save as..."));
	if (filename == NULL)
		return;
	
	ical_string = cal_client_get_component_as_string (client, comp);
	if (ical_string == NULL) {
		g_warning ("Couldn't convert item to a string");
		return;
	}
	
	file = fopen (filename, "w");
	if (file == NULL) {
		g_warning ("Couldn't save item");
		return;
	}
	
	fprintf (file, ical_string);
	g_free (ical_string);
	fclose (file);
}

static void
e_calendar_table_on_print_task (GtkWidget *widget, gpointer data)
{
	ECalendarTable *cal_table;
	CalClient *client;
	CalComponent *comp;

	cal_table = E_CALENDAR_TABLE (data);

	client = calendar_model_get_cal_client (cal_table->model);
	comp = get_selected_comp (cal_table);
	if (comp == NULL)
		return;
	
	print_comp (comp, client, FALSE);
}

static void
e_calendar_table_on_cut (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (data);
	e_calendar_table_cut_clipboard (cal_table);
}

static void
e_calendar_table_on_copy (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (data);
	e_calendar_table_copy_clipboard (cal_table);
}

static void
e_calendar_table_on_paste (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (data);
	e_calendar_table_paste_clipboard (cal_table);
}

static void
e_calendar_table_on_assign (GtkWidget *widget, gpointer data)
{
	ECalendarTable *cal_table;
	CalComponent *comp;

	cal_table = E_CALENDAR_TABLE (data);

	comp = get_selected_comp (cal_table);
	if (comp)
		open_task (cal_table, comp, TRUE);
}

static void
e_calendar_table_on_forward (GtkWidget *widget, gpointer data)
{
	ECalendarTable *cal_table;
	CalClient *client;
	CalComponent *comp;

	cal_table = E_CALENDAR_TABLE (data);

	client = calendar_model_get_cal_client (cal_table->model);
	comp = get_selected_comp (cal_table);
	if (comp)
		itip_send_comp (CAL_COMPONENT_METHOD_PUBLISH, comp,
				client, NULL);
}

static gint
e_calendar_table_on_key_press (ETable *table,
			       gint row,
			       gint col,
			       GdkEventKey *event,
			       ECalendarTable *cal_table)
{
	if (event->keyval == GDK_Delete) {
		delete_cb (NULL, cal_table);
		return TRUE;
	} else if ((event->keyval == GDK_o)
		   &&(event->state & GDK_MOD1_MASK)) {
		open_task_by_row (cal_table, row);
		return TRUE;
	}

	return FALSE;
}

/* Loads the state of the table (headers shown etc.) from the given file. */
void
e_calendar_table_load_state	(ECalendarTable *cal_table,
				 gchar		*filename)
{
	struct stat st;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (stat (filename, &st) == 0 && st.st_size > 0
	    && S_ISREG (st.st_mode)) {
		e_table_load_state (e_table_scrolled_get_table(E_TABLE_SCROLLED (cal_table->etable)), filename);
	}
}


/* Saves the state of the table (headers shown etc.) to the given file. */
void
e_calendar_table_save_state (ECalendarTable	*cal_table,
			     gchar		*filename)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	e_table_save_state (e_table_scrolled_get_table(E_TABLE_SCROLLED (cal_table->etable)),
			    filename);
}

static void
selection_get (GtkWidget *invisible,
	       GtkSelectionData *selection_data,
	       guint info,
	       guint time_stamp,
	       ECalendarTable *cal_table)
{
	if (cal_table->clipboard_selection != NULL) {
		gtk_selection_data_set (selection_data,
					GDK_SELECTION_TYPE_STRING,
					8,
					cal_table->clipboard_selection,
					strlen (cal_table->clipboard_selection));
	}
}

static void
selection_clear_event (GtkWidget *invisible,
		       GdkEventSelection *event,
		       ECalendarTable *cal_table)
{
	if (cal_table->clipboard_selection != NULL) {
		g_free (cal_table->clipboard_selection);
		cal_table->clipboard_selection = NULL;
	}
}

static void
selection_received (GtkWidget *invisible,
		    GtkSelectionData *selection_data,
		    guint time,
		    ECalendarTable *cal_table)
{
	char *comp_str;
	icalcomponent *icalcomp;
	char *uid;
	CalComponent *comp;
	icalcomponent_kind kind;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (selection_data->length < 0 ||
	    selection_data->type != GDK_SELECTION_TYPE_STRING) {
		return;
	}

	comp_str = (char *) selection_data->data;
	icalcomp = icalparser_parse_string ((const char *) comp_str);
	if (!icalcomp)
		return;

	/* check the type of the component */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT &&
	    kind != ICAL_VEVENT_COMPONENT &&
	    kind != ICAL_VTODO_COMPONENT &&
	    kind != ICAL_VJOURNAL_COMPONENT) {
		return;
	}

	calendar_model_set_status_message (e_calendar_table_get_model (cal_table),
					   _("Updating objects"));

	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_kind child_kind;
		icalcomponent *subcomp;
		icalcomponent *vcal_comp;

		vcal_comp = icalcomp;
		subcomp = icalcomponent_get_first_component (
			vcal_comp, ICAL_ANY_COMPONENT);
		while (subcomp) {
			child_kind = icalcomponent_isa (subcomp);
			if (child_kind == ICAL_VEVENT_COMPONENT ||
			    child_kind == ICAL_VTODO_COMPONENT ||
			    child_kind == ICAL_VJOURNAL_COMPONENT) {
				CalComponent *tmp_comp;

				uid = cal_component_gen_uid ();
				tmp_comp = cal_component_new ();
				cal_component_set_icalcomponent (
					tmp_comp, icalcomponent_new_clone (subcomp));
				cal_component_set_uid (tmp_comp, uid);

				cal_client_update_object (
					calendar_model_get_cal_client (cal_table->model),
					tmp_comp);
				free (uid);
				g_object_unref (tmp_comp);
			}
			subcomp = icalcomponent_get_next_component (
				vcal_comp, ICAL_ANY_COMPONENT);
		}
	}
	else {
		comp = cal_component_new ();
		cal_component_set_icalcomponent (comp, icalcomp);
		uid = cal_component_gen_uid ();
		cal_component_set_uid (comp, (const char *) uid);
		free (uid);

		cal_client_update_object (
			calendar_model_get_cal_client (cal_table->model),
			comp);
		g_object_unref (comp);
	}

	calendar_model_set_status_message (e_calendar_table_get_model (cal_table), NULL);
}


/* Returns the current time, for the ECellDateEdit items.
   FIXME: Should probably use the timezone of the item rather than the
   current timezone, though that may be difficult to get from here. */
static struct tm
e_calendar_table_get_current_time (ECellDateEdit *ecde, gpointer data)
{
	char *location;
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	/* Get the current timezone. */
	location = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (location);

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year  = tt.year - 1900;
	tmp_tm.tm_mon   = tt.month - 1;
	tmp_tm.tm_mday  = tt.day;
	tmp_tm.tm_hour  = tt.hour;
	tmp_tm.tm_min   = tt.minute;
	tmp_tm.tm_sec   = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}


#ifdef TRANSLATORS_ONLY

static char *test[] = {
    N_("Click to add a task")
};

#endif
