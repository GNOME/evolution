/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
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
#include <gnome.h>
#include <e-table/e-table-scrolled.h>
#include <e-table/e-cell-checkbox.h>
#include <e-table/e-cell-toggle.h>
#include <e-table/e-cell-text.h>
#include "e-calendar-table.h"
#include "calendar-model.h"
#include "gncal-todo.h"

/* Pixmaps. */
#include "task.xpm"
#include "task-recurring.xpm"
#include "task-assigned.xpm"
#include "task-assigned-to.xpm"

#include <e-table/check-filled.xpm>


static void e_calendar_table_class_init (ECalendarTableClass *class);
static void e_calendar_table_init (ECalendarTable *cal_table);
static void e_calendar_table_destroy (GtkObject *object);

static void e_calendar_table_on_double_click (ETable *table,
					      gint row,
					      ECalendarTable *cal_table);
static gint e_calendar_table_on_right_click (ETable *table,
					     gint row,
					     gint col,
					     GdkEventButton *event,
					     ECalendarTable *cal_table);
static void e_calendar_table_on_open_task (GtkWidget *menuitem,
					   gpointer	  data);
static void e_calendar_table_on_mark_task_complete (GtkWidget *menuitem,
						    gpointer   data);
static void e_calendar_table_on_delete_task (GtkWidget *menuitem,
					     gpointer   data);
static gint e_calendar_table_on_key_press (ETable *table,
					   gint row,
					   gint col,
					   GdkEventKey *event,
					   ECalendarTable *cal_table);

static void e_calendar_table_open_task (ECalendarTable *cal_table,
					gint row);

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


#define E_CALENDAR_TABLE_SPEC				\
	"<ETableSpecification click-to-add=\"1\">"	\
	"<columns-shown>"				\
		"<column> 16 </column>"			\
		"<column> 17 </column>"			\
		"<column> 13 </column>"			\
	"</columns-shown>"				\
	"<grouping> </grouping>"			\
	"</ETableSpecification>"

/* Creates a text column for the table header */
static void
create_column (ETableHeader *header, ETableModel *model, int id, const char *text)
{
	ECell *cell;
	ETableCol *column;

	cell = e_cell_text_new (model, NULL, GTK_JUSTIFY_LEFT);
	column = e_table_col_new (id, text, 1.0, 10, cell, g_str_compare, TRUE);
	e_table_header_add_column (header, column, CAL_COMPONENT_FIELD_CATEGORIES);
}

static void
e_calendar_table_init (ECalendarTable *cal_table)
{
	GtkWidget *table;
	ETableModel *model;
	ETableHeader *header;
	ECell *cell;
	ETableCol *column;
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
	model = E_TABLE_MODEL (cal_table->model);

	header = e_table_header_new ();
	gtk_object_ref (GTK_OBJECT (header));
	gtk_object_sink (GTK_OBJECT (header));

	/* Create the header columns */

	create_column (header, model, CAL_COMPONENT_FIELD_CATEGORIES, _("Categories"));
	create_column (header, model, CAL_COMPONENT_FIELD_CLASSIFICATION, _("Classification"));
	create_column (header, model, CAL_COMPONENT_FIELD_COMPLETED, _("Completion date"));
	create_column (header, model, CAL_COMPONENT_FIELD_DTEND, _("End date"));
	create_column (header, model, CAL_COMPONENT_FIELD_DTSTART, _("Start date"));
	create_column (header, model, CAL_COMPONENT_FIELD_DUE, _("Due date"));
	create_column (header, model, CAL_COMPONENT_FIELD_GEO, _("Geographical position"));
	create_column (header, model, CAL_COMPONENT_FIELD_PERCENT, _("Percent complete"));
	create_column (header, model, CAL_COMPONENT_FIELD_PRIORITY, _("Priority"));

	cell = e_cell_text_new (model, NULL, GTK_JUSTIFY_LEFT);
	gtk_object_set (GTK_OBJECT (cell),
			"strikeout_column", CAL_COMPONENT_FIELD_COMPLETE,
			"bold_column", CAL_COMPONENT_FIELD_OVERDUE,
			"color_column", CAL_COMPONENT_FIELD_COLOR,
			NULL);
	column = e_table_col_new (CAL_COMPONENT_FIELD_SUMMARY, _("Summary"),
				  1.0, 10, cell, g_str_compare, TRUE);
	e_table_header_add_column (header, column, CAL_COMPONENT_FIELD_SUMMARY);

	create_column (header, model, CAL_COMPONENT_FIELD_SUMMARY, _("Summary"));
	create_column (header, model, CAL_COMPONENT_FIELD_TRANSPARENCY, _("Transparency"));
	create_column (header, model, CAL_COMPONENT_FIELD_URL, _("URL"));

	/* FIXME: we don't have the HAS_ALARMS field */

	/* Create pixmaps */

	if (!icon_pixbufs[0])
		for (i = 0; i < E_CALENDAR_MODEL_NUM_ICONS; i++) {
			icon_pixbufs[i] = gdk_pixbuf_new_from_xpm_data (
				(const char **) icon_xpm_data[i]);
		}

	cell = e_cell_toggle_new (0, 4, icon_pixbufs);
	column = e_table_col_new_with_pixbuf (CAL_COMPONENT_FIELD_ICON,
					      icon_pixbufs[0], 0.0, 16, cell,
					      g_int_compare, FALSE);
	e_table_header_add_column (header, column, CAL_COMPONENT_FIELD_ICON);

	cell = e_cell_checkbox_new ();
	pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) check_filled_xpm);
	column = e_table_col_new_with_pixbuf (CAL_COMPONENT_FIELD_COMPLETE,
					      pixbuf, 0.0, 16, cell,
					      g_int_compare, FALSE);
	e_table_header_add_column (header, column, CAL_COMPONENT_FIELD_COMPLETE);

	/* Create the table */

	table = e_table_scrolled_new (header, model, E_CALENDAR_TABLE_SPEC);
	gtk_object_set (GTK_OBJECT (table),
			"click_to_add_message", "Click here to add a new Task",
			NULL);
	gtk_table_attach (GTK_TABLE (cal_table), table, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (table);

	gtk_signal_connect (GTK_OBJECT (table), "double_click",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_double_click),
			    cal_table);
	gtk_signal_connect (GTK_OBJECT (table), "right_click",
			    GTK_SIGNAL_FUNC (e_calendar_table_on_right_click),
			    cal_table);
	gtk_signal_connect (GTK_OBJECT (table), "key_press",
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


static void
e_calendar_table_destroy (GtkObject *object)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (object);



	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


void
e_calendar_table_set_cal_client (ECalendarTable *cal_table,
				 CalClient	*client)
{
	g_print ("In e_calendar_table_set_cal_client\n");

	calendar_model_set_cal_client (cal_table->model, client,
				       CALOBJ_TYPE_TODO);
}


static void
e_calendar_table_on_double_click (ETable *table,
				  gint row,
				  ECalendarTable *cal_table)
{
	g_print ("In e_calendar_table_on_double_click row:%i\n", row);

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


static void
e_calendar_table_on_delete_task (GtkWidget *menuitem,
				 gpointer   data)
{
	ECalendarMenuData *menu_data = (ECalendarMenuData*) data;

	calendar_model_delete_task (menu_data->cal_table->model,
				    menu_data->row);
}



static gint
e_calendar_table_on_key_press (ETable *table,
			       gint row,
			       gint col,
			       GdkEventKey *event,
			       ECalendarTable *cal_table)
{
	g_print ("In e_calendar_table_on_key_press\n");

	if (event->keyval == GDK_Delete) {
		g_print ("  delete key!!!\n");

		calendar_model_delete_task (cal_table->model, row);
	}

	return FALSE;
}


static void
e_calendar_table_open_task (ECalendarTable *cal_table,
			    gint row)
{
	CalComponent *comp;

#if 0
	task_editor_new ();
	/* FIXME: Set iCalObject to edit. */
#endif

	comp = calendar_model_get_cal_object (cal_table->model, row);

	gncal_todo_edit (calendar_model_get_cal_client (cal_table->model), comp);
}


