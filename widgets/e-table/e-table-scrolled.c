/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-scrolled.c: A graphical view of a Table.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * Copyright 2000, 1999, Helix Code, Inc
 */
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include "e-table.h"
#include "e-table-scrolled.h"

#define COLUMN_HEADER_HEIGHT 16

#define PARENT_TYPE e_scroll_frame_get_type ()

static GtkObjectClass *parent_class;

enum {
	CURSOR_CHANGE,
	DOUBLE_CLICK,
	RIGHT_CLICK,
	KEY_PRESS,
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_TABLE_DRAW_GRID,
	ARG_TABLE_DRAW_FOCUS,
	ARG_CURSOR_MODE,
	ARG_LENGTH_THRESHOLD,
	ARG_CLICK_TO_ADD_MESSAGE,
};

static gint ets_signals [LAST_SIGNAL] = { 0, };

static void
cursor_change_proxy (ETable *et, int row, ETableScrolled *ets)
{
	gtk_signal_emit (GTK_OBJECT (ets),
			 ets_signals [CURSOR_CHANGE],
			 row);
}

static void
double_click_proxy (ETable *et, int row, ETableScrolled *ets)
{
	gtk_signal_emit (GTK_OBJECT (ets),
			 ets_signals [DOUBLE_CLICK],
			 row);
}

static gint
right_click_proxy (ETable *et, int row, int col, GdkEvent *event, ETableScrolled *ets)
{
	int return_val = 0;
	gtk_signal_emit (GTK_OBJECT (ets),
			 ets_signals [RIGHT_CLICK],
			 row, col, event, &return_val);
	return return_val;
}

static gint
key_press_proxy (ETable *et, int row, int col, GdkEvent *event, ETableScrolled *ets)
{
	int return_val;
	gtk_signal_emit (GTK_OBJECT (ets),
			 ets_signals [KEY_PRESS],
			 row, col, event, &return_val);
	return return_val;
}

static void
e_table_scrolled_init (GtkObject *object)
{
	ETableScrolled *ets;
	EScrollFrame *scroll_frame;

	ets          = E_TABLE_SCROLLED (object);
	scroll_frame = E_SCROLL_FRAME   (object);

	ets->table = gtk_type_new(e_table_get_type());

	e_scroll_frame_set_policy      (scroll_frame, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	e_scroll_frame_set_shadow_type (scroll_frame, GTK_SHADOW_IN);
}

static void
e_table_scrolled_real_construct (ETableScrolled *ets)
{
	gtk_object_set(GTK_OBJECT(ets),
		       "shadow_type", GTK_SHADOW_IN,
		       "hscrollbar_policy", GTK_POLICY_NEVER,
		       "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
		       NULL);

	gtk_container_add(GTK_CONTAINER(ets), GTK_WIDGET(ets->table));

	gtk_signal_connect(GTK_OBJECT(ets->table), "cursor_change",
			   GTK_SIGNAL_FUNC(cursor_change_proxy), ets);
	gtk_signal_connect(GTK_OBJECT(ets->table), "double_click",
			   GTK_SIGNAL_FUNC(double_click_proxy), ets);
	gtk_signal_connect(GTK_OBJECT(ets->table), "right_click",
			   GTK_SIGNAL_FUNC(right_click_proxy), ets);
	gtk_signal_connect(GTK_OBJECT(ets->table), "key_press",
			   GTK_SIGNAL_FUNC(key_press_proxy), ets);

	gtk_widget_show(GTK_WIDGET(ets->table));
}

ETableScrolled *
e_table_scrolled_construct (ETableScrolled *ets, ETableHeader *full_header,
			    ETableModel *etm, const char *spec)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);
	g_return_val_if_fail(full_header != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_HEADER(full_header), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(spec != NULL, NULL);
	
	e_table_construct(ets->table, full_header, etm, spec);

	e_table_scrolled_real_construct(ets);

	return ets;
}

ETableScrolled *
e_table_scrolled_construct_from_spec_file (ETableScrolled *ets, ETableHeader *full_header, ETableModel *etm,
					   const char *filename)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);
	g_return_val_if_fail(full_header != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_HEADER(full_header), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(filename != NULL, NULL);
	
	e_table_construct_from_spec_file(ets->table, full_header, etm, filename);

	e_table_scrolled_real_construct(ets);

	return ets;
}

GtkWidget *
e_table_scrolled_new (ETableHeader *full_header, ETableModel *etm, const char *spec)
{
	ETableScrolled *ets;

	g_return_val_if_fail(full_header != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_HEADER(full_header), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(spec != NULL, NULL);	

	ets = E_TABLE_SCROLLED (gtk_widget_new (e_table_scrolled_get_type (),
						"hadjustment", NULL,
						"vadjustment", NULL,
						NULL));

	ets = e_table_scrolled_construct (ets, full_header, etm, spec);
		
	return GTK_WIDGET (ets);
}

GtkWidget *
e_table_scrolled_new_from_spec_file (ETableHeader *full_header, ETableModel *etm, const char *filename)
{
	ETableScrolled *ets;

	g_return_val_if_fail(full_header != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_HEADER(full_header), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(filename != NULL, NULL);
	
	ets = gtk_type_new (e_table_scrolled_get_type ());

	ets = e_table_scrolled_construct_from_spec_file (ets, full_header, etm, filename);
		
	return GTK_WIDGET (ets);
}

gchar *
e_table_scrolled_get_specification (ETableScrolled *ets)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);

	return e_table_get_specification(ets->table);
}

void
e_table_scrolled_save_specification (ETableScrolled *ets, gchar *filename)
{
	g_return_if_fail(ets != NULL);
	g_return_if_fail(E_IS_TABLE_SCROLLED(ets));
	g_return_if_fail(filename != NULL);

	e_table_save_specification(ets->table, filename);
}

void
e_table_scrolled_set_cursor_row (ETableScrolled *ets, int row)
{
	g_return_if_fail(ets != NULL);
	g_return_if_fail(E_IS_TABLE_SCROLLED(ets));

	e_table_set_cursor_row(ets->table, row);
}

int
e_table_scrolled_get_cursor_row (ETableScrolled *ets)
{
	g_return_val_if_fail(ets != NULL, -1);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), -1);

	return e_table_get_cursor_row(ets->table);
}

void
e_table_scrolled_selected_row_foreach     (ETableScrolled *ets,
					   ETableForeachFunc callback,
					   gpointer closure)
{
	g_return_if_fail(ets != NULL);
	g_return_if_fail(E_IS_TABLE_SCROLLED(ets));

	e_table_selected_row_foreach(ets->table,
				     callback,
				     closure);
}

EPrintable *
e_table_scrolled_get_printable (ETableScrolled *ets)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);

	return e_table_get_printable(ets->table);
}

static void
ets_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableScrolled *ets = E_TABLE_SCROLLED (o);
	gboolean bool_val;
	gchar *string_val;

	switch (arg_id){
	case ARG_TABLE_DRAW_GRID:
		gtk_object_get(GTK_OBJECT(ets->table),
			       "drawgrid", &bool_val,
			       NULL);
		GTK_VALUE_BOOL (*arg) = bool_val;
		break;

	case ARG_TABLE_DRAW_FOCUS:
		gtk_object_get(GTK_OBJECT(ets->table),
			       "drawfocus", &bool_val,
			       NULL);
		GTK_VALUE_BOOL (*arg) = bool_val;
		break;

	case ARG_CLICK_TO_ADD_MESSAGE:
		gtk_object_get(GTK_OBJECT(ets->table),
			       "click_to_add_message", &string_val,
			       NULL);
		GTK_VALUE_STRING (*arg) = string_val;
		break;
	}
}

static void
ets_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableScrolled *ets = E_TABLE_SCROLLED (o);

	switch (arg_id){
	case ARG_LENGTH_THRESHOLD:
		gtk_object_set(GTK_OBJECT(ets->table),
			       "length_threshold", GTK_VALUE_INT (*arg),
			       NULL);
		break;
		
	case ARG_TABLE_DRAW_GRID:
		gtk_object_set(GTK_OBJECT(ets->table),
			       "drawgrid", GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_TABLE_DRAW_FOCUS:
		gtk_object_set(GTK_OBJECT(ets->table),
			       "drawfocus", GTK_VALUE_BOOL (*arg),
			       NULL);
		break;

	case ARG_CURSOR_MODE:
		gtk_object_set(GTK_OBJECT(ets->table),
			       "cursor_mode", GTK_VALUE_INT (*arg),
			       NULL);
		break;
	case ARG_CLICK_TO_ADD_MESSAGE:
		gtk_object_set(GTK_OBJECT(ets->table),
			       "click_to_add_message", GTK_VALUE_STRING (*arg),
			       NULL);
		break;
	}
}
	
static void
e_table_scrolled_class_init (GtkObjectClass *object_class)
{
	ETableScrolledClass *klass = E_TABLE_SCROLLED_CLASS(object_class);
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_arg = ets_set_arg;
	object_class->get_arg = ets_get_arg;
	
	klass->cursor_change = NULL;
	klass->double_click = NULL;
	klass->right_click = NULL;
	klass->key_press = NULL;

	ets_signals [CURSOR_CHANGE] =
		gtk_signal_new ("cursor_change",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableScrolledClass, cursor_change),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	ets_signals [DOUBLE_CLICK] =
		gtk_signal_new ("double_click",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableScrolledClass, double_click),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	ets_signals [RIGHT_CLICK] =
		gtk_signal_new ("right_click",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableScrolledClass, right_click),
				e_marshal_INT__INT_INT_POINTER,
				GTK_TYPE_INT, 3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_POINTER);

	ets_signals [KEY_PRESS] =
		gtk_signal_new ("key_press",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableScrolledClass, key_press),
				e_marshal_INT__INT_INT_POINTER,
				GTK_TYPE_INT, 3, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, ets_signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("ETableScrolled::drawgrid", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_TABLE_DRAW_GRID);
	gtk_object_add_arg_type ("ETableScrolled::drawfocus", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_TABLE_DRAW_FOCUS);
	gtk_object_add_arg_type ("ETableScrolled::cursor_mode", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_CURSOR_MODE);
	gtk_object_add_arg_type ("ETableScrolled::length_threshold", GTK_TYPE_INT,
				 GTK_ARG_WRITABLE, ARG_LENGTH_THRESHOLD);
	gtk_object_add_arg_type ("ETableScrolled::click_to_add_message", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_CLICK_TO_ADD_MESSAGE);
}

E_MAKE_TYPE(e_table_scrolled, "ETableScrolled", ETableScrolled, e_table_scrolled_class_init, e_table_scrolled_init, PARENT_TYPE);

