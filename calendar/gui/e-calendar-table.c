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
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-cell-checkbox.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-text.h>
#include "e-calendar-table.h"
#include "calendar-model.h"
#include "dialogs/delete-comp.h"
#include "dialogs/task-editor.h"

/* Pixmaps. */
#include "task.xpm"
#include "task-recurring.xpm"
#include "task-assigned.xpm"
#include "task-assigned-to.xpm"

#include "check-filled.xpm"


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
static void e_calendar_table_on_mark_task_complete (GtkWidget	*menuitem,
						    gpointer	 data);
static void e_calendar_table_on_delete_task	(GtkWidget	*menuitem,
						 gpointer	 data);
static gint e_calendar_table_on_key_press	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEventKey	*event,
						 ECalendarTable *cal_table);

static void e_calendar_table_open_task		(ECalendarTable *cal_table,
						 gint		 row);

static void e_calendar_table_apply_filter	(ECalendarTable	*cal_table);
static void e_calendar_table_on_model_changed	(ETableModel	*model,
						 ECalendarTable	*cal_table);
static void e_calendar_table_on_row_inserted	(ETableModel	*model,
						 gint		 row,
						 ECalendarTable	*cal_table);
static void e_calendar_table_on_row_deleted	(ETableModel	*model,
						 gint		 row,
						 ECalendarTable	*cal_table);


/* The icons to represent the task. */
#define E_CALENDAR_MODEL_NUM_ICONS	4
static char** icon_xpm_data[E_CALENDAR_MODEL_NUM_ICONS] = {
	task_xpm, task_recurring_xpm, task_assigned_xpm, task_assigned_to_xpm
};
static GdkPixbuf* icon_pixbufs[E_CALENDAR_MODEL_NUM_ICONS] = { 0 };

static GtkTableClass *parent_class;


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
	N_("Precent complete"),
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
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"2\" _title=\"Completion Date\" "	\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"3\" _title=\"End Date\" "		\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"4\" _title=\"Start Date\" "	\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"5\" _title=\"Due Date\" "		\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"6\" _title=\"Geographical Position\" " \
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"7\" _title=\"% Complete\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"8\" _title=\"Priority\" "		\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col= \"9\" _title=\"Summary\" "		\
	"   expansion=\"3.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"  compare=\"string\"/>"			\
        "  <ETableColumn model_col=\"10\" _title=\"Transparency\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col=\"11\" _title=\"URL\" "		\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col=\"12\" _title=\"Alarms\" "		\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"calstring\"   compare=\"string\"/>"			\
        "  <ETableColumn model_col=\"13\" pixbuf=\"icon\" "		\
	"   expansion=\"1.0\" minimum_width=\"16\" resizable=\"false\" "\
	"   cell=\"icon\"     compare=\"integer\"/>"			\
        "  <ETableColumn model_col=\"14\" pixbuf=\"complete\" "		\
	"   expansion=\"1.0\" minimum_width=\"16\" resizable=\"false\" "\
	"   cell=\"checkbox\" compare=\"integer\"/>"			\
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
	ECell *cell;
	ETableExtras *extras;
	gint i;
	GdkPixbuf *pixbuf;
	GdkColormap *colormap;
	gboolean success[E_CALENDAR_TABLE_COLOR_LAST];
	gint nfailed;

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
	gtk_signal_connect (GTK_OBJECT (cal_table->model), "model_row_inserted",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_row_inserted),
			    cal_table);
	gtk_signal_connect (GTK_OBJECT (cal_table->model), "model_row_deleted",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_row_deleted),
			    cal_table);

	/* Create the header columns */

	extras = e_table_extras_new();

	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			NULL);
	e_table_extras_add_cell (extras, "calstring", cell);

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

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


void
e_calendar_table_set_cal_client (ECalendarTable *cal_table,
				 CalClient	*client)
{
	calendar_model_set_cal_client (cal_table->model, client,
				       CALOBJ_TYPE_TODO);
}


static void
e_calendar_table_on_double_click (ETable *table,
				  gint row, 
				  gint col,
				  GdkEvent *event,
				  ECalendarTable *cal_table)
{
	g_print ("In e_calendar_table_on_double_click\n");

	e_calendar_table_open_task (cal_table, row);
}


static GnomeUIInfo e_calendar_table_popup_uiinfo[] = {
	{ GNOME_APP_UI_ITEM, N_("Open..."),
	  N_("Open the task"), e_calendar_table_on_open_task,
	  NULL, NULL, 0, 0, 0, 0 },
	{ GNOME_APP_UI_ITEM, N_("Mark Complete"),
	  N_("Mark the task complete"), e_calendar_table_on_mark_task_complete,
	  NULL, NULL, 0, 0, 0, 0 },
	{ GNOME_APP_UI_ITEM, N_("Delete"),
	  N_("Delete the task"), e_calendar_table_on_delete_task,
	  NULL, NULL, 0, 0, 0, 0 },

	GNOMEUIINFO_END
};


typedef struct _ECalendarMenuData ECalendarMenuData;
struct _ECalendarMenuData {
	ECalendarTable *cal_table;
	gint row;
};

static gint
e_calendar_table_on_right_click (ETable *table,
				 gint row,
				 gint col,
				 GdkEventButton *event,
				 ECalendarTable *cal_table)
{
	ECalendarMenuData menu_data;
	GtkWidget *popup_menu;

	menu_data.cal_table = cal_table;
	menu_data.row = row;

	popup_menu = gnome_popup_menu_new (e_calendar_table_popup_uiinfo);
	gnome_popup_menu_do_popup_modal (popup_menu, NULL, NULL, event,
					 &menu_data);

	gtk_widget_destroy (popup_menu);

	return TRUE;
}


static void
e_calendar_table_on_open_task (GtkWidget *menuitem,
			       gpointer	  data)
{
	ECalendarMenuData *menu_data = (ECalendarMenuData*) data;

	e_calendar_table_open_task (menu_data->cal_table,
				    menu_data->row);
}


static void
e_calendar_table_on_mark_task_complete (GtkWidget *menuitem,
					gpointer   data)
{
	ECalendarMenuData *menu_data = (ECalendarMenuData*) data;

	calendar_model_mark_task_complete (menu_data->cal_table->model,
					   menu_data->row);
}


/* Deletes a component from the table */
static void
delete_component (CalendarModel *model, int row, GtkWidget *widget)
{
	CalComponent *comp;

	comp = calendar_model_get_component (model, row);

	if (delete_component_dialog (comp, widget)) {
		CalClient *client;
		const char *uid;

		client = calendar_model_get_cal_client (model);
		cal_component_get_uid (comp, &uid);

		/* We don't check the return value; FALSE can mean the object
		 * was not in the server anyways.
		 */
		cal_client_remove_object (client, uid);
	}
}

static void
e_calendar_table_on_delete_task (GtkWidget *menuitem,
				 gpointer   data)
{
	ECalendarMenuData *menu_data = (ECalendarMenuData*) data;

	delete_component (menu_data->cal_table->model, menu_data->row, menuitem);
}



static gint
e_calendar_table_on_key_press (ETable *table,
			       gint row,
			       gint col,
			       GdkEventKey *event,
			       ECalendarTable *cal_table)
{
	if (event->keyval == GDK_Delete) {
		delete_component (cal_table->model, row, GTK_WIDGET (table));
		return TRUE;
	}

	return FALSE;
}


static void
e_calendar_table_open_task (ECalendarTable *cal_table,
			    gint row)
{
	TaskEditor *tedit;
	CalComponent *comp;

	tedit = task_editor_new ();
	task_editor_set_cal_client (tedit, calendar_model_get_cal_client (cal_table->model));

	comp = calendar_model_get_component (cal_table->model, row);
	task_editor_set_todo_object (tedit, comp);
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
e_calendar_table_on_row_inserted	(ETableModel	*model,
					 gint		 row,
					 ECalendarTable	*cal_table)
{
	ETableSubsetVariable *etssv;
	CalComponent *comp;
	gboolean add_row = FALSE;

	etssv = E_TABLE_SUBSET_VARIABLE (cal_table->subset_model);

	if (cal_table->filter_func == NULL) {
		add_row = TRUE;
	} else {
		comp = calendar_model_get_component (cal_table->model, row);

		if ((*cal_table->filter_func) (cal_table, comp,
					       cal_table->filter_data))
			add_row = TRUE;
	}

	if (add_row) {
		e_table_subset_variable_increment (etssv, row, 1);
		e_table_subset_variable_add (etssv, row);
	}
}


static void
e_calendar_table_on_row_deleted		(ETableModel	*model,
					 gint		 row,
					 ECalendarTable	*cal_table)
{
	/* We just reapply the filter since we aren't too bothered about
	   being efficient. It doesn't happen often. */
	e_calendar_table_apply_filter (cal_table);
}
