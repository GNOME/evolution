/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2000, Helix Code, Inc.
 * Copyright 2000, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <gal/e-table/e-cell-checkbox.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-combo.h>
#include <widgets/misc/e-cell-date-edit.h>
#include "e-calendar-table.h"
#include "calendar-model.h"
#include "dialogs/delete-comp.h"
#include "dialogs/task-editor.h"

/* Pixmaps. */
#include "art/task.xpm"
#include "art/task-recurring.xpm"
#include "art/task-assigned.xpm"
#include "art/task-assigned-to.xpm"

#include "art/check-filled.xpm"


static void e_calendar_table_class_init		(ECalendarTableClass *class);
static void e_calendar_table_init		(ECalendarTable	*cal_table);
static void e_calendar_table_destroy		(GtkObject	*object);

static void e_calendar_table_on_double_click	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEvent	*event,
						 ECalendarTable *cal_table);
static gint e_calendar_table_on_right_click	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEventButton *event,
						 ECalendarTable *cal_table);
static void e_calendar_table_on_open_task	(GtkWidget	*menuitem,
						 gpointer	 data);
static void e_calendar_table_on_cut             (GtkWidget      *menuitem,
						 gpointer        data);
static void e_calendar_table_on_copy            (GtkWidget      *menuitem,
						 gpointer        data);
static void e_calendar_table_on_paste           (GtkWidget      *menuitem,
						 gpointer        data);
static gint e_calendar_table_on_key_press	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEventKey	*event,
						 ECalendarTable *cal_table);

static void e_calendar_table_apply_filter	(ECalendarTable	*cal_table);
static void e_calendar_table_on_model_changed	(ETableModel	*model,
						 ECalendarTable	*cal_table);
static void e_calendar_table_on_rows_inserted	(ETableModel	*model,
						 int		 row,
						 int		 count,
						 ECalendarTable	*cal_table);
static void e_calendar_table_on_rows_deleted	(ETableModel	*model,
						 int		 row,
						 int		 count,
						 ECalendarTable	*cal_table);

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
static void invisible_destroyed                 (GtkWidget *invisible,
						 ECalendarTable *cal_table);


/* The icons to represent the task. */
#define E_CALENDAR_MODEL_NUM_ICONS	4
static char** icon_xpm_data[E_CALENDAR_MODEL_NUM_ICONS] = {
	task_xpm, task_recurring_xpm, task_assigned_xpm, task_assigned_to_xpm
};
static GdkPixbuf* icon_pixbufs[E_CALENDAR_MODEL_NUM_ICONS] = { 0 };

static GtkTableClass *parent_class;
static GdkAtom clipboard_atom = GDK_NONE;


GtkType
e_calendar_table_get_type (void)
{
	static GtkType e_calendar_table_type = 0;

	if (!e_calendar_table_type){
		GtkTypeInfo e_calendar_table_info = {
			"ECalendarTable",
			sizeof (ECalendarTable),
			sizeof (ECalendarTableClass),
			(GtkClassInitFunc) e_calendar_table_class_init,
			(GtkObjectInitFunc) e_calendar_table_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		parent_class = gtk_type_class (GTK_TYPE_TABLE);
		e_calendar_table_type = gtk_type_unique (GTK_TYPE_TABLE,
							 &e_calendar_table_info);
	}

	return e_calendar_table_type;
}


static void
e_calendar_table_class_init (ECalendarTableClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

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

#ifdef JUST_FOR_TRANSLATORS
static char *list [] = {
	N_("Categories"),
	N_("Classification"),
	N_("Completion Date"),
	N_("End Date"),
	N_("Start Date"),
	N_("Due Date"),
	N_("Geographical Position"),
	N_("Percent complete"),
	N_("Priority"),
	N_("Summary"),
	N_("Transparency"),
	N_("URL"),
	N_("Alarms"),
	N_("Click here to add a task")
};
#endif

#define E_CALENDAR_TABLE_SPEC						\
	"<ETableSpecification click-to-add=\"true\" "			\
	" _click-to-add-message=\"Click here to add a task\" "		\
	" draw-grid=\"true\">"						\
        "  <ETableColumn model_col= \"0\" _title=\"Categories\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"1\" _title=\"Classification\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"classification\"   compare=\"string\"/>"		\
        "  <ETableColumn model_col= \"2\" _title=\"Completion Date\" "	\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"dateedit\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"3\" _title=\"End Date\" "		\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"dateedit\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"4\" _title=\"Start Date\" "	\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"dateedit\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"5\" _title=\"Due Date\" "		\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"dateedit\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"6\" _title=\"Geographical Position\" " \
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"7\" _title=\"% Complete\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"percent\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"8\" _title=\"Priority\" "		\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"priority\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"9\" _title=\"Summary\" "		\
	"   expansion=\"3.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"  compare=\"string\"/>"			\
        "  <ETableColumn model_col=\"10\" _title=\"Transparency\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"transparency\"   compare=\"string\"/>"		\
        "  <ETableColumn model_col=\"11\" _title=\"URL\" "		\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col=\"12\" _title=\"Alarms\" "		\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col=\"13\" pixbuf=\"icon\" _title=\"Type\" "\
	"   expansion=\"1.0\" minimum_width=\"16\" resizable=\"false\" "\
	"   cell=\"icon\"     compare=\"integer\"/>"			\
        "  <ETableColumn model_col=\"14\" pixbuf=\"complete\" _title=\"Complete\" " \
	"   expansion=\"1.0\" minimum_width=\"16\" resizable=\"false\" "\
	"   cell=\"checkbox\" compare=\"integer\"/>"			\
        "  <ETableColumn model_col=\"18\" _title=\"Status\" "		\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstatus\"   compare=\"string\"/>"			\
	"  <ETableState>"						\
	"    <column source=\"13\"/>"					\
	"    <column source=\"14\"/>"					\
	"    <column source= \"9\"/>"					\
	"    <grouping></grouping>"					\
	"  </ETableState>"						\
	"</ETableSpecification>"

static void
e_calendar_table_init (ECalendarTable *cal_table)
{
	GtkWidget *table;
	ETable *e_table;
	ECell *cell, *popup_cell;
	ETableExtras *extras;
	gint i;
	GdkPixbuf *pixbuf;
	GdkColormap *colormap;
	gboolean success[E_CALENDAR_TABLE_COLOR_LAST];
	gint nfailed;
	GList *strings;

	/* Allocate the colors we need. */

	colormap = gtk_widget_get_colormap (GTK_WIDGET (cal_table));

	cal_table->colors[E_CALENDAR_TABLE_COLOR_OVERDUE].red   = 65535;
	cal_table->colors[E_CALENDAR_TABLE_COLOR_OVERDUE].green = 0;
	cal_table->colors[E_CALENDAR_TABLE_COLOR_OVERDUE].blue  = 0;

	nfailed = gdk_colormap_alloc_colors (colormap, cal_table->colors,
					     E_CALENDAR_TABLE_COLOR_LAST,
					     FALSE, TRUE, success);
	if (nfailed)
		g_warning ("Failed to allocate all colors");

	/* Create the model */

	cal_table->model = calendar_model_new ();
	cal_table->subset_model = e_table_subset_variable_new (E_TABLE_MODEL (cal_table->model));

	gtk_signal_connect (GTK_OBJECT (cal_table->model), "model_changed",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_model_changed),
			    cal_table);
	gtk_signal_connect (GTK_OBJECT (cal_table->model), "model_rows_inserted",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_rows_inserted),
			    cal_table);
	gtk_signal_connect (GTK_OBJECT (cal_table->model), "model_rows_deleted",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_rows_deleted),
			    cal_table);

	/* Create the header columns */

	extras = e_table_extras_new();

	/*
	 * Normal string fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			NULL);

	e_table_extras_add_cell (extras, "calstring", cell);


	/*
	 * Date fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			NULL);

	popup_cell = e_cell_date_edit_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));
	e_table_extras_add_cell (extras, "dateedit", popup_cell);
	cal_table->dates_cell = E_CELL_DATE_EDIT (popup_cell);


	/*
	 * Combo fields.
	 */

	/* Classification field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			"editable", FALSE,
			NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));

	strings = NULL;
	strings = g_list_append (strings, _("None"));
	strings = g_list_append (strings, _("Public"));
	strings = g_list_append (strings, _("Private"));
	strings = g_list_append (strings, _("Confidential"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "classification", popup_cell);

	/* Priority field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			"editable", FALSE,
			NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));

	strings = NULL;
	strings = g_list_append (strings, _("High"));
	strings = g_list_append (strings, _("Normal"));
	strings = g_list_append (strings, _("Low"));
	strings = g_list_append (strings, _("Undefined"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "priority", popup_cell);

	/* Percent field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));

	strings = NULL;
	strings = g_list_append (strings, _("0%"));
	strings = g_list_append (strings, _("10%"));
	strings = g_list_append (strings, _("20%"));
	strings = g_list_append (strings, _("30%"));
	strings = g_list_append (strings, _("40%"));
	strings = g_list_append (strings, _("50%"));
	strings = g_list_append (strings, _("60%"));
	strings = g_list_append (strings, _("70%"));
	strings = g_list_append (strings, _("80%"));
	strings = g_list_append (strings, _("90%"));
	strings = g_list_append (strings, _("100%"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "percent", popup_cell);

	/* Transparency field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			"editable", FALSE,
			NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));

	strings = NULL;
	strings = g_list_append (strings, _("None"));
	strings = g_list_append (strings, _("Opaque"));
	strings = g_list_append (strings, _("Transparent"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "transparency", popup_cell);

	/* Status field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			"editable", FALSE,
			NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));

	strings = NULL;
	strings = g_list_append (strings, _("Not Started"));
	strings = g_list_append (strings, _("In Progress"));
	strings = g_list_append (strings, _("Completed"));
	strings = g_list_append (strings, _("Cancelled"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "calstatus", popup_cell);

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

	table = e_table_scrolled_new (cal_table->subset_model, extras,
				      E_CALENDAR_TABLE_SPEC, NULL);
	gtk_object_unref (GTK_OBJECT (extras));

	cal_table->etable = table;
	gtk_table_attach (GTK_TABLE (cal_table), table, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (table);


	e_table = e_table_scrolled_get_table (E_TABLE_SCROLLED (table));
	gtk_signal_connect (GTK_OBJECT (e_table), "double_click",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_double_click),
			    cal_table);
	gtk_signal_connect (GTK_OBJECT (e_table), "right_click",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_right_click),
			    cal_table);
	gtk_signal_connect (GTK_OBJECT (e_table), "key_press",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_key_press),
			    cal_table);

	/* Set up the invisible widget for the clipboard selections */
	cal_table->invisible = gtk_invisible_new ();
	gtk_selection_add_target (cal_table->invisible,
				  clipboard_atom,
				  GDK_SELECTION_TYPE_STRING,
				  0);
	gtk_signal_connect (GTK_OBJECT (cal_table->invisible),
			    "selection_get",
			    GTK_SIGNAL_FUNC (selection_get),
			    (gpointer) cal_table);
	gtk_signal_connect (GTK_OBJECT (cal_table->invisible),
			    "selection_clear_event",
			    GTK_SIGNAL_FUNC (selection_clear_event),
			    (gpointer) cal_table);
	gtk_signal_connect (GTK_OBJECT (cal_table->invisible),
			    "selection_received",
			    GTK_SIGNAL_FUNC (selection_received),
			    (gpointer) cal_table);
	gtk_signal_connect (GTK_OBJECT (cal_table->invisible),
			    "destroy",
			    GTK_SIGNAL_FUNC (invisible_destroyed),
			    (gpointer) cal_table);
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

	cal_table = GTK_WIDGET (gtk_type_new (e_calendar_table_get_type ()));

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

	gtk_object_unref (GTK_OBJECT (cal_table->model));
	cal_table->model = NULL;

	gtk_object_unref (GTK_OBJECT (cal_table->subset_model));
	cal_table->subset_model = NULL;

	if (cal_table->invisible)
		gtk_widget_destroy (cal_table->invisible);
	if (cal_table->clipboard_selection)
		g_free (cal_table->clipboard_selection);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


void
e_calendar_table_set_cal_client (ECalendarTable *cal_table,
				 CalClient	*client)
{
	calendar_model_set_cal_client (cal_table->model, client,
				       CALOBJ_TYPE_TODO);
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
	g_assert (e_table_selected_count (etable) == 1);

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

	for (l = uids; l; l = l->next) {
		const char *uid;

		uid = l->data;

		/* We don't check the return value; FALSE can mean the object
		 * was not in the server anyways.
		 */
		cal_client_remove_object (client, uid);
	}

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
	g_assert (n_selected > 0);

	if (n_selected == 1)
		comp = get_selected_comp (cal_table);
	else
		comp = NULL;

	if (delete_component_dialog (comp, n_selected, CAL_COMPONENT_TODO, GTK_WIDGET (cal_table)))
		delete_selected_components (cal_table);
}

/* Opens a task in the task editor */
static void
open_task (ECalendarTable *cal_table, CalComponent *comp)
{
	TaskEditor *tedit;

	tedit = task_editor_new ();
	comp_editor_set_cal_client (COMP_EDITOR (tedit), calendar_model_get_cal_client (cal_table->model));
	comp_editor_edit_comp (COMP_EDITOR (tedit), comp);
	comp_editor_focus (COMP_EDITOR (tedit));
}

/* Opens the task in the specified row */
static void
open_task_by_row (ECalendarTable *cal_table, int row)
{
	CalComponent *comp;

	comp = calendar_model_get_component (cal_table->model, row);
	open_task (cal_table, comp);
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

static GnomeUIInfo tasks_popup_one[] = {
	GNOMEUIINFO_ITEM_NONE (N_("Edit this task"), NULL, e_calendar_table_on_open_task),
	GNOMEUIINFO_ITEM_NONE (N_("Cut"), NULL, e_calendar_table_on_cut),
	GNOMEUIINFO_ITEM_NONE (N_("Copy"), NULL, e_calendar_table_on_copy),
	GNOMEUIINFO_ITEM_NONE (N_("Paste"), NULL, e_calendar_table_on_paste),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Mark as complete"), NULL, mark_as_complete_cb),
	GNOMEUIINFO_ITEM_NONE (N_("Delete this task"), NULL, delete_cb),
	GNOMEUIINFO_END
};

static GnomeUIInfo tasks_popup_many[] = {
	GNOMEUIINFO_ITEM_NONE (N_("Cut"), NULL, e_calendar_table_on_cut),
	GNOMEUIINFO_ITEM_NONE (N_("Copy"), NULL, e_calendar_table_on_copy),
	GNOMEUIINFO_ITEM_NONE (N_("Paste"), NULL, e_calendar_table_on_paste),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE (N_("Mark tasks as complete"), NULL, mark_as_complete_cb),
	GNOMEUIINFO_ITEM_NONE (N_("Delete selected tasks"), NULL, delete_cb),
	GNOMEUIINFO_END
};

static gint
e_calendar_table_on_right_click (ETable *table,
				 gint row,
				 gint col,
				 GdkEventButton *event,
				 ECalendarTable *cal_table)
{
	GtkWidget *popup_menu;
	int n_selected;

	n_selected = e_table_selected_count (table);
	g_assert (n_selected > 0);

	if (n_selected == 1)
		popup_menu = gnome_popup_menu_new (tasks_popup_one);
	else
		popup_menu = gnome_popup_menu_new (tasks_popup_many);

	gnome_popup_menu_do_popup_modal (popup_menu, NULL, NULL, event, cal_table);
	gtk_widget_destroy (popup_menu);

	return TRUE;
}


static void
e_calendar_table_on_open_task (GtkWidget *menuitem,
			       gpointer	  data)
{
	ECalendarTable *cal_table;
	CalComponent *comp;

	cal_table = E_CALENDAR_TABLE (data);

	comp = get_selected_comp (cal_table);
	open_task (cal_table, comp);
}

static void
e_calendar_table_on_cut (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (data);

	e_calendar_table_on_copy (menuitem, data);
	delete_selected_components (cal_table);
}

static void
copy_row_cb (int model_row, gpointer data)
{
	ECalendarTable *cal_table;
	CalComponent *comp;
	gchar *comp_str;
	icalcomponent *new_comp;

	cal_table = E_CALENDAR_TABLE (data);

	comp = calendar_model_get_component (cal_table->model, model_row);
	if (!comp)
		return;


	if (cal_table->clipboard_selection) {
		//new_comp = icalparser_parse_string (cal_table->clipboard_selection);
		//if (!new_comp)
		//	return;

		//icalcomponent_add_component (new_comp,
		//			     cal_component_get_icalcomponent (comp));
		//g_free (cal_table->clipboard_selection);
	}
	else {
		new_comp = icalparser_parse_string (
			icalcomponent_as_ical_string (cal_component_get_icalcomponent (comp)));
		if (!new_comp)
			return;
	}

	comp_str = icalcomponent_as_ical_string (new_comp);
	cal_table->clipboard_selection = g_strdup (comp_str);

	free (comp_str);
	icalcomponent_free (new_comp);
}

static void
e_calendar_table_on_copy (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table;
	ETable *etable;

	cal_table = E_CALENDAR_TABLE (data);

	if (cal_table->clipboard_selection) {
		g_free (cal_table->clipboard_selection);
		cal_table->clipboard_selection = NULL;
	}

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, copy_row_cb, cal_table);

	gtk_selection_owner_set (cal_table->invisible, clipboard_atom, GDK_CURRENT_TIME);
}

static void
e_calendar_table_on_paste (GtkWidget *menuitem, gpointer data)
{
	ECalendarTable *cal_table = E_CALENDAR_TABLE (data);

	gtk_selection_convert (cal_table->invisible,
			       clipboard_atom,
			       GDK_SELECTION_TYPE_STRING,
			       GDK_CURRENT_TIME);
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


void
e_calendar_table_set_filter_func	(ECalendarTable *cal_table,
					 ECalendarTableFilterFunc filter_func,
					 gpointer	 filter_data,
					 GDestroyNotify  filter_data_destroy)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (cal_table->filter_func == filter_func
	    && cal_table->filter_data == filter_data
	    && cal_table->filter_data_destroy == filter_data_destroy)
		return;

	if (cal_table->filter_data_destroy)
		(*cal_table->filter_data_destroy) (cal_table->filter_data);

	cal_table->filter_func = filter_func;
	cal_table->filter_data = filter_data;
	cal_table->filter_data_destroy = filter_data_destroy;

	e_calendar_table_apply_filter (cal_table);
}


static void
e_calendar_table_apply_filter		(ECalendarTable	*cal_table)
{
	ETableSubsetVariable *etssv;
	CalComponent *comp;
	gint rows, row;

	etssv = E_TABLE_SUBSET_VARIABLE (cal_table->subset_model);

	/* Make sure that any edits get saved first. */
	e_table_model_pre_change (cal_table->subset_model);

	/* FIXME: A hack to remove all the existing rows quickly. */
	E_TABLE_SUBSET (cal_table->subset_model)->n_map = 0;

	if (cal_table->filter_func == NULL) {
		e_table_subset_variable_add_all (etssv);
	} else {
		rows = e_table_model_row_count (E_TABLE_MODEL (cal_table->model));
		for (row = 0; row < rows; row++) {
			comp = calendar_model_get_component (cal_table->model,
							     row);

			if ((*cal_table->filter_func) (cal_table, comp,
						       cal_table->filter_data))
				e_table_subset_variable_add (etssv, row);
		}
	}

	e_table_model_changed (cal_table->subset_model);
}


gboolean
e_calendar_table_filter_by_category  (ECalendarTable	*cal_table,
				      CalComponent	*comp,
				      gpointer		 filter_data)
{
	GSList *categories_list, *elem;
	gboolean retval = FALSE;

	cal_component_get_categories_list (comp, &categories_list);

	for (elem = categories_list; elem; elem = elem->next) {
		if (retval == FALSE
		    && !strcmp ((char*) elem->data, (char*) filter_data))
			retval = TRUE;
		g_free (elem->data);
	}

	g_slist_free (categories_list);

	return retval;
}


static void
e_calendar_table_on_model_changed	(ETableModel	*model,
					 ECalendarTable	*cal_table)
{
	e_calendar_table_apply_filter (cal_table);
}


static void
e_calendar_table_on_rows_inserted	(ETableModel	*model,
					 int		 row,
					 int		 count,
					 ECalendarTable	*cal_table)
{
	int i;

	for (i = 0; i < count; i++) {
		gboolean add_row;

		add_row = FALSE;

		if (cal_table->filter_func) {
			CalComponent *comp;

			comp = calendar_model_get_component (cal_table->model, row + i);
			g_assert (comp != NULL);

			add_row = (* cal_table->filter_func) (cal_table, comp,
							      cal_table->filter_data);
		} else
			add_row = TRUE;

		if (add_row) {
			ETableSubsetVariable *etssv;

			etssv = E_TABLE_SUBSET_VARIABLE (cal_table->subset_model);

			e_table_subset_variable_increment (etssv, row, 1);
			e_table_subset_variable_add (etssv, row);
		}
	}
}


static void
e_calendar_table_on_rows_deleted	(ETableModel	*model,
					 int		 row,
					 int		 count,
					 ECalendarTable	*cal_table)
{
	/* We just reapply the filter since we aren't too bothered about
	   being efficient. It doesn't happen often. */
	e_calendar_table_apply_filter (cal_table);
}

const gchar *
e_calendar_table_get_spec (void)
{
	return E_CALENDAR_TABLE_SPEC;
}

static void
invisible_destroyed (GtkWidget *invisible, ECalendarTable *cal_table)
{
	cal_table->invisible = NULL;
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

	if (selection_data->length < 0 ||
	    selection_data->type != GDK_SELECTION_TYPE_STRING) {
		return;
	}

	comp_str = (char *) selection_data->data;
	icalcomp = icalparser_parse_string ((const char *) comp_str);
	if (icalcomp) {
		icalcomponent *tmp_comp;
		char *uid;

		/* there can be various components */
		tmp_comp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
		while (tmp_comp != NULL) {
			CalComponent *comp;

			comp = cal_component_new ();
			cal_component_set_icalcomponent (comp, icalcomp);
			uid = cal_component_gen_uid ();
			cal_component_set_uid (comp, (const char *) uid);
			free (uid);

			tmp_comp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);

			cal_client_update_object (
				calendar_model_get_cal_client (cal_table->model),
				comp);
			gtk_object_unref (GTK_OBJECT (comp));
		}
	}
}
