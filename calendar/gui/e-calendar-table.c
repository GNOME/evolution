/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2000, 2001, 2002, 2003 Novell, Inc.
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
 * ECalendarTable - displays the ECalComponent objects in a table (an ETable).
 * Used for calendar events and tasks.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <gnome.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/e-table/e-cell-checkbox.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-combo.h>
#include <e-util/e-dialog-utils.h>
#include <widgets/misc/e-cell-date-edit.h>
#include <widgets/misc/e-cell-percent.h>

#include "calendar-component.h"
#include "calendar-config.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/task-editor.h"
#include "e-cal-model-tasks.h"
#include "e-calendar-table.h"
#include "e-cell-date-edit-text.h"
#include "e-comp-editor-registry.h"
#include "print.h"
#include <e-util/e-icon-factory.h>
#include "e-cal-popup.h"

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

static gint e_calendar_table_on_key_press	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEventKey	*event,
						 ECalendarTable *cal_table);

static struct tm e_calendar_table_get_current_time (ECellDateEdit *ecde,
						    gpointer data);
static void mark_row_complete_cb (int model_row, gpointer data);
static ECalModelComponent *get_selected_comp (ECalendarTable *cal_table);
static void open_task (ECalendarTable *cal_table, ECalModelComponent *comp_data, gboolean assign);

/* Signal IDs */
enum {
	USER_CREATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* The icons to represent the task. */
#define E_CALENDAR_MODEL_NUM_ICONS	4
static const char* icon_names[E_CALENDAR_MODEL_NUM_ICONS] = {
	"stock_task", "stock_task-recurring", "stock_task-assigned", "stock_task-assigned-to"
};
static GdkPixbuf* icon_pixbufs[E_CALENDAR_MODEL_NUM_ICONS] = { 0 };

static GdkAtom clipboard_atom = GDK_NONE;

G_DEFINE_TYPE (ECalendarTable, e_calendar_table, GTK_TYPE_TABLE);

static void
e_calendar_table_class_init (ECalendarTableClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	/* Method override */
	object_class->destroy		= e_calendar_table_destroy;

	signals[USER_CREATED] =
		g_signal_new ("user_created",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarTableClass, user_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

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
 * Also, this assumes it is passed pointers to ECalComponents, but I think it
 * may just be passed pointers to the 2 cell values.
 */
static gint
task_compare_cb (gconstpointer a, gconstpointer b)
{
	ECalComponent *ca, *cb;
	ECalComponentDateTime due_a, due_b;
	int *prio_a, *prio_b;
	int retval;

	ca = E_CAL_COMPONENT (a);
	cb = E_CAL_COMPONENT (b);

	e_cal_component_get_due (ca, &due_a);
	e_cal_component_get_due (cb, &due_b);
	e_cal_component_get_priority (ca, &prio_a);
	e_cal_component_get_priority (cb, &prio_b);

	if (due_a.value && due_b.value) {
		int v;

		/* FIXME: TIMEZONES. But currently we have no way to get the
		   ECal, so we can't get the timezone. */
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

	e_cal_component_free_datetime (&due_a);
	e_cal_component_free_datetime (&due_b);

	if (prio_a)
		e_cal_component_free_priority (prio_a);

	if (prio_b)
		e_cal_component_free_priority (prio_b);

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

	priority1 = e_cal_util_priority_from_string ((const char*) a);
	priority2 = e_cal_util_priority_from_string ((const char*) b);

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
row_appended_cb (ECalModel *model, ECalendarTable *cal_table) 
{
	g_signal_emit (cal_table, signals[USER_CREATED], 0);
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

	cal_table->model = (ECalModel *) e_cal_model_tasks_new ();
	g_signal_connect (cal_table->model, "row_appended", G_CALLBACK (row_appended_cb), cal_table);

	/* Create the header columns */

	extras = e_table_extras_new();

	/*
	 * Normal string fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_COMPLETE,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      NULL);

	e_table_extras_add_cell (extras, "calstring", cell);


	/*
	 * Date fields.
	 */
	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_COMPLETE,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
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
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_COMPLETE,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
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
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_COMPLETE,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
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
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_COMPLETE,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
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
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_COMPLETE,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
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
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_COMPLETE,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
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
			icon_pixbufs[i] = e_icon_factory_get_icon (icon_names[i], E_ICON_SIZE_LIST);
		}

	cell = e_cell_toggle_new (0, E_CALENDAR_MODEL_NUM_ICONS, icon_pixbufs);
	e_table_extras_add_cell(extras, "icon", cell);
	e_table_extras_add_pixbuf(extras, "icon", icon_pixbufs[0]);

	pixbuf = e_icon_factory_get_icon ("stock_check-filled", E_ICON_SIZE_LIST);
	e_table_extras_add_pixbuf(extras, "complete", pixbuf);
	gdk_pixbuf_unref(pixbuf);

	/* Create the table */

	table = e_table_scrolled_new_from_spec_file (E_TABLE_MODEL (cal_table->model),
						     extras,
						     EVOLUTION_ETSPECDIR "/e-calendar-table.etspec",
						     NULL);
	/* FIXME: this causes a message from GLib about 'extras' having only a floating
	   reference */
	/* g_object_unref (extras); */

	cal_table->etable = table;
	gtk_table_attach (GTK_TABLE (cal_table), table, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (table);


	e_table = e_table_scrolled_get_table (E_TABLE_SCROLLED (table));
	g_signal_connect (e_table, "double_click", G_CALLBACK (e_calendar_table_on_double_click), cal_table);
	g_signal_connect (e_table, "right_click", G_CALLBACK (e_calendar_table_on_right_click), cal_table);
	g_signal_connect (e_table, "key_press", G_CALLBACK (e_calendar_table_on_key_press), cal_table);
	g_signal_connect (e_table, "popup_menu", G_CALLBACK (e_calendar_table_on_popup_menu), cal_table);
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
ECalModel *
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

	GTK_OBJECT_CLASS (e_calendar_table_parent_class)->destroy (object);
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

void
e_calendar_table_open_selected (ECalendarTable *cal_table)
{
	ECalModelComponent *comp_data;

	comp_data = get_selected_comp (cal_table);
	if (comp_data != NULL)
		open_task (cal_table, comp_data, FALSE);
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
static ECalModelComponent *
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

	return e_cal_model_get_component_at (cal_table->model, row);
}

struct get_selected_uids_closure {
	ECalendarTable *cal_table;
	GSList *objects;
};

/* Used from e_table_selected_row_foreach(), builds a list of the selected UIDs */
static void
add_uid_cb (int model_row, gpointer data)
{
	struct get_selected_uids_closure *closure;
	ECalModelComponent *comp_data;

	closure = data;

	comp_data = e_cal_model_get_component_at (closure->cal_table->model, model_row);

	closure->objects = g_slist_prepend (closure->objects, comp_data);
}

static GSList *
get_selected_objects (ECalendarTable *cal_table)
{
	struct get_selected_uids_closure closure;
	ETable *etable;

	closure.cal_table = cal_table;
	closure.objects = NULL;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, add_uid_cb, &closure);

	return closure.objects;
}

/* Deletes all of the selected components in the table */
static void
delete_selected_components (ECalendarTable *cal_table)
{
	GSList *objs, *l;

	objs = get_selected_objects (cal_table);

	e_calendar_table_set_status_message (cal_table, _("Deleting selected objects"));

	for (l = objs; l; l = l->next) {
		ECalModelComponent *comp_data = (ECalModelComponent *) l->data;
		GError *error = NULL;
		
		e_cal_remove_object (comp_data->client, 
				     icalcomponent_get_uid (comp_data->icalcomp), &error);
		delete_error_dialog (error, E_CAL_COMPONENT_TODO);
		g_clear_error (&error);
	}

	e_calendar_table_set_status_message (cal_table, NULL);

	g_slist_free (objs);
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
	ECalModelComponent *comp_data;
	ECalComponent *comp = NULL;

	g_return_if_fail (cal_table != NULL);
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));

	n_selected = e_table_selected_count (etable);
	if (n_selected <= 0)
		return;

	if (n_selected == 1)
		comp_data = get_selected_comp (cal_table);
	else
		comp_data = NULL;

	/* FIXME: this may be something other than a TODO component */

	if (comp_data) {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
	}
	
	if (delete_component_dialog (comp, FALSE, n_selected, E_CAL_COMPONENT_TODO,
				     GTK_WIDGET (cal_table)))
		delete_selected_components (cal_table);

	/* free memory */
	if (comp)
		g_object_unref (comp);
}

/**
 * e_calendar_table_get_selected:
 * @cal_table: 
 * 
 * Get the currently selected ECalModelComponent's on the table.
 * 
 * Return value: A GSList of the components, which should be
 * g_slist_free'd when finished with.
 **/
GSList *
e_calendar_table_get_selected (ECalendarTable *cal_table)
{
	return get_selected_objects(cal_table);
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
	ECalModelComponent *comp_data;
	gchar *comp_str;
	icalcomponent *child;

	cal_table = E_CALENDAR_TABLE (data);

	g_return_if_fail (cal_table->tmp_vcal != NULL);

	comp_data = e_cal_model_get_component_at (cal_table->model, model_row);
	if (!comp_data)
		return;

	/* add timezones to the VCALENDAR component */
	e_cal_util_add_timezones_from_component (cal_table->tmp_vcal, comp_data->icalcomp);

	/* add the new component to the VCALENDAR component */
	comp_str = icalcomponent_as_ical_string (comp_data->icalcomp);
	child = icalparser_parse_string (comp_str);
	if (child) {
		icalcomponent_add_component (cal_table->tmp_vcal,
					     icalcomponent_new_clone (child));
		icalcomponent_free (child);
	}
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

	/* create temporary VCALENDAR object */
	cal_table->tmp_vcal = e_cal_util_new_top_level ();

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, copy_row_cb, cal_table);
	comp_str = icalcomponent_as_ical_string (cal_table->tmp_vcal);
	gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (cal_table), clipboard_atom),
				(const char *) comp_str,
				g_utf8_strlen (comp_str, -1));
	
	/* free memory */
	icalcomponent_free (cal_table->tmp_vcal);
	cal_table->tmp_vcal = NULL;
}

static void
clipboard_get_text_cb (GtkClipboard *clipboard, const gchar *text, ECalendarTable *cal_table)
{
	icalcomponent *icalcomp;
	char *uid;
	ECalComponent *comp;
	ECal *client;
	icalcomponent_kind kind;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (!text || !*text)
		return;

	icalcomp = icalparser_parse_string (text);
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

	client = e_cal_model_get_default_client (cal_table->model);
	
	e_calendar_table_set_status_message (cal_table, _("Updating objects"));

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
				ECalComponent *tmp_comp;

				uid = e_cal_component_gen_uid ();
				tmp_comp = e_cal_component_new ();
				e_cal_component_set_icalcomponent (
					tmp_comp, icalcomponent_new_clone (subcomp));
				e_cal_component_set_uid (tmp_comp, uid);
				free (uid);

				/* FIXME should we convert start/due/complete times? */
				/* FIXME Error handling */
				e_cal_create_object (client, e_cal_component_get_icalcomponent (tmp_comp), NULL, NULL);

				g_object_unref (tmp_comp);
			}
			subcomp = icalcomponent_get_next_component (
				vcal_comp, ICAL_ANY_COMPONENT);
		}
	}
	else {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomp);
		uid = e_cal_component_gen_uid ();
		e_cal_component_set_uid (comp, (const char *) uid);
		free (uid);

		e_cal_create_object (client, e_cal_component_get_icalcomponent (comp), NULL, NULL);

		g_object_unref (comp);
	}

	e_calendar_table_set_status_message (cal_table, NULL);
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

	gtk_clipboard_request_text (gtk_widget_get_clipboard (GTK_WIDGET (cal_table), clipboard_atom),
				    (GtkClipboardTextReceivedFunc) clipboard_get_text_cb, cal_table);
}

/* Opens a task in the task editor */
static void
open_task (ECalendarTable *cal_table, ECalModelComponent *comp_data, gboolean assign)
{
	CompEditor *tedit;
	const char *uid;
	
	uid = icalcomponent_get_uid (comp_data->icalcomp);

	tedit = e_comp_editor_registry_find (comp_editor_registry, uid);
	if (tedit == NULL) {
		ECalComponent *comp;

		tedit = COMP_EDITOR (task_editor_new (comp_data->client));

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
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
	ECalModelComponent *comp_data;

	comp_data = e_cal_model_get_component_at (cal_table->model, row);
	open_task (cal_table, comp_data, FALSE);
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

/* popup menu callbacks */

static void
e_calendar_table_on_open_task (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;

	comp_data = get_selected_comp (cal_table);
	if (comp_data)
		open_task (cal_table, comp_data, FALSE);
}

static void
e_calendar_table_on_save_as (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;
	char *filename;
	char *ical_string;
	FILE *file;

	comp_data = get_selected_comp (cal_table);
	if (comp_data == NULL)
		return;
	
	filename = e_file_dialog_save (_("Save as..."));
	if (filename == NULL)
		return;
	
	ical_string = e_cal_get_component_as_string (comp_data->client, comp_data->icalcomp);
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
e_calendar_table_on_print_task (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;
	ECalComponent *comp;

	comp_data = get_selected_comp (cal_table);
	if (comp_data == NULL)
		return;
	
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
	print_comp (comp, comp_data->client, FALSE);

	g_object_unref (comp);
}

static void
e_calendar_table_on_cut (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_cut_clipboard (cal_table);
}

static void
e_calendar_table_on_copy (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_copy_clipboard (cal_table);
}

static void
e_calendar_table_on_paste (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_paste_clipboard (cal_table);
}

static void
e_calendar_table_on_assign (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;

	comp_data = get_selected_comp (cal_table);
	if (comp_data)
		open_task (cal_table, comp_data, TRUE);
}

static void
e_calendar_table_on_forward (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;

	comp_data = get_selected_comp (cal_table);
	if (comp_data) {
		ECalComponent *comp;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
		itip_send_comp (E_CAL_COMPONENT_METHOD_PUBLISH, comp, comp_data->client, NULL);

		g_object_unref (comp);
	}
}

/* Used from e_table_selected_row_foreach() */
static void
mark_row_complete_cb (int model_row, gpointer data)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (data);
	e_cal_model_tasks_mark_task_complete (E_CAL_MODEL_TASKS (cal_table->model), model_row);
}

/* Callback used for the "mark tasks as complete" menu item */
static void
mark_as_complete_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ETable *etable;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, mark_row_complete_cb, cal_table);
}

/* Opens the URL of the task */
static void
open_url_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;
	icalproperty *prop;

	comp_data = get_selected_comp (cal_table);
	if (!comp_data)
		return;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY);
	if (!prop)
		return;

	gnome_url_show (icalproperty_get_url (prop), NULL);
}

/* Callback for the "delete tasks" menu item */
static void
delete_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_delete_selected (cal_table);
}

static EPopupItem tasks_popup_items [] = {
	{ E_POPUP_ITEM, "00.open", N_("_Open"), e_calendar_table_on_open_task, NULL, GTK_STOCK_OPEN, E_CAL_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "05.openweb", N_("Open _Web Page"), open_url_cb, NULL, NULL, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_HASURL },
	{ E_POPUP_ITEM, "10.saveas", N_("_Save As..."), e_calendar_table_on_save_as, NULL, GTK_STOCK_SAVE_AS, E_CAL_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "20.print", N_("_Print..."), e_calendar_table_on_print_task, NULL, GTK_STOCK_PRINT, E_CAL_POPUP_SELECT_ONE },

	{ E_POPUP_BAR, "30.bar" },
	
	{ E_POPUP_ITEM, "40.cut", N_("C_ut"), e_calendar_table_on_cut, NULL, GTK_STOCK_CUT, 0, E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "50.copy", N_("_Copy"), e_calendar_table_on_copy, NULL, GTK_STOCK_COPY, 0, 0 },
	{ E_POPUP_ITEM, "60.paste", N_("_Paste"), e_calendar_table_on_paste, NULL, GTK_STOCK_PASTE, 0, E_CAL_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "70.bar" },

	{ E_POPUP_ITEM, "80.assign", N_("_Assign Task"), e_calendar_table_on_assign, NULL, NULL, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_EDITABLE|E_CAL_POPUP_SELECT_ASSIGNABLE },
	{ E_POPUP_ITEM, "90.forward", N_("_Forward as iCalendar"), e_calendar_table_on_forward, NULL, "stock_mail-forward", E_CAL_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "a0.markonecomplete", N_("_Mark as Complete"), mark_as_complete_cb, NULL, NULL, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "b0.markmanycomplete", N_("_Mark Selected Tasks as Complete"), mark_as_complete_cb, NULL, NULL, E_CAL_POPUP_SELECT_MANY, E_CAL_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "c0.bar" },

	{ E_POPUP_ITEM, "d0.delete", N_("_Delete"), delete_cb, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "e0.deletemany", N_("_Delete Selected Tasks"), delete_cb, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_MANY, E_CAL_POPUP_SELECT_EDITABLE },
};

static void
ect_popup_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static gint
e_calendar_table_show_popup_menu (ETable *table,
				  GdkEvent *gdk_event,
				  ECalendarTable *cal_table)
{
	GtkMenu *menu;
	GSList *selection, *l, *menus = NULL;
	GPtrArray *events;
	ECalPopup *ep;
	ECalPopupTargetSelect *t;
	int i;

	selection = get_selected_objects (cal_table);
	if (!selection)
		return TRUE;

	/** @HookPoint-ECalPopup: Tasks Table Context Menu
	 * @Id: org.gnome.evolution.tasks.table.popup
	 * @Class: org.gnome.evolution.calendar.popup:1.0
	 * @Target: ECalPopupTargetSelect
	 *
	 * The context menu on the tasks table.
	 */
	ep = e_cal_popup_new("org.gnome.evolution.tasks.table.popup");

	events = g_ptr_array_new();
	for (l=selection;l;l=g_slist_next(l))
		g_ptr_array_add(events, e_cal_model_copy_component_data((ECalModelComponent *)l->data));
	g_slist_free(selection);

	t = e_cal_popup_target_new_select(ep, cal_table->model, events);
	t->target.widget = (GtkWidget *)cal_table;

	for (i=0;i<sizeof(tasks_popup_items)/sizeof(tasks_popup_items[0]);i++)
		menus = g_slist_prepend(menus, &tasks_popup_items[i]);
	e_popup_add_items((EPopup *)ep, menus, ect_popup_free, cal_table);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);

	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, gdk_event?gdk_event->button.button:0,
		       gdk_event?gdk_event->button.time:gtk_get_current_event_time());

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
	g_return_val_if_fail(table, FALSE);

	return e_calendar_table_show_popup_menu (table, NULL,
						 E_CALENDAR_TABLE(data));
}

static gint
e_calendar_table_on_key_press (ETable *table,
			       gint row,
			       gint col,
			       GdkEventKey *event,
			       ECalendarTable *cal_table)
{
	if (event->keyval == GDK_Delete) {
		delete_cb (NULL, NULL, cal_table);
		return TRUE;
	} else if ((event->keyval == GDK_o)
		   &&(event->state & GDK_CONTROL_MASK)) {
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
		e_table_load_state (e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable)), filename);
	}
}


/* Saves the state of the table (headers shown etc.) to the given file. */
void
e_calendar_table_save_state (ECalendarTable	*cal_table,
			     gchar		*filename)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	e_table_save_state (e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable)),
			    filename);
}

/* Returns the current time, for the ECellDateEdit items.
   FIXME: Should probably use the timezone of the item rather than the
   current timezone, though that may be difficult to get from here. */
static struct tm
e_calendar_table_get_current_time (ECellDateEdit *ecde, gpointer data)
{
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	/* Get the current timezone. */
	zone = calendar_config_get_icaltimezone ();

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

/* Displays messages on the status bar */
#define EVOLUTION_TASKS_PROGRESS_IMAGE "stock_todo"
static GdkPixbuf *progress_icon = NULL;

void
e_calendar_table_set_activity_handler (ECalendarTable *cal_table, EActivityHandler *activity_handler)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	cal_table->activity_handler = activity_handler;
}

void
e_calendar_table_set_status_message (ECalendarTable *cal_table, const gchar *message)
{
        g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (!cal_table->activity_handler)
		return;
	
        if (!message || !*message) {
		if (cal_table->activity_id != 0) {
			e_activity_handler_operation_finished (cal_table->activity_handler, cal_table->activity_id);
			cal_table->activity_id = 0;
		}
        } else if (cal_table->activity_id == 0) {
                char *client_id = g_strdup_printf ("%p", cal_table);
		
                if (progress_icon == NULL)
                        progress_icon = e_icon_factory_get_icon (EVOLUTION_TASKS_PROGRESS_IMAGE, E_ICON_SIZE_STATUS);

                cal_table->activity_id = e_activity_handler_operation_started (cal_table->activity_handler, client_id,
									       progress_icon, message, TRUE);

                g_free (client_id);
        } else {
                e_activity_handler_operation_progressing (cal_table->activity_handler, cal_table->activity_id, message, -1.0);
	}
}
