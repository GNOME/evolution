/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table-scrolled.c: A graphical view of a Table.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * Copyright 2000, 1999, Ximian, Inc
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
	ARG_0,
	ARG_TABLE,
};

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

	gtk_widget_show(GTK_WIDGET(ets->table));
}

ETableScrolled *e_table_scrolled_construct                 (ETableScrolled    *ets,
							    ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec,
							    const char        *state)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	e_table_construct(ets->table, etm, ete, spec, state);

	e_table_scrolled_real_construct(ets);

	return ets;
}

GtkWidget      *e_table_scrolled_new                       (ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec,
							    const char        *state)
{
	ETableScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec != NULL, NULL);

	ets = E_TABLE_SCROLLED (gtk_widget_new (e_table_scrolled_get_type (),
						"hadjustment", NULL,
						"vadjustment", NULL,
						NULL));

	ets = e_table_scrolled_construct (ets, etm, ete, spec, state);

	return GTK_WIDGET (ets);
}

ETableScrolled *e_table_scrolled_construct_from_spec_file  (ETableScrolled    *ets,
							    ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec_fn,
							    const char        *state_fn)
{
	g_return_val_if_fail(ets != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_SCROLLED(ets), NULL);
	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	e_table_construct_from_spec_file(ets->table, etm, ete, spec_fn, state_fn);

	e_table_scrolled_real_construct(ets);

	return ets;
}

GtkWidget      *e_table_scrolled_new_from_spec_file        (ETableModel       *etm,
							    ETableExtras      *ete,
							    const char        *spec_fn,
							    const char        *state_fn)
{
	ETableScrolled *ets;

	g_return_val_if_fail(etm != NULL, NULL);
	g_return_val_if_fail(E_IS_TABLE_MODEL(etm), NULL);
	g_return_val_if_fail(ete == NULL || E_IS_TABLE_EXTRAS(ete), NULL);
	g_return_val_if_fail(spec_fn != NULL, NULL);

	ets = gtk_type_new (e_table_scrolled_get_type ());

	ets = e_table_scrolled_construct_from_spec_file (ets, etm, ete, spec_fn, state_fn);

	return GTK_WIDGET (ets);
}

ETable *
e_table_scrolled_get_table                 (ETableScrolled *ets)
{
	return ets->table;
}

static void
ets_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableScrolled *ets = E_TABLE_SCROLLED (o);

	switch (arg_id){
	case ARG_TABLE:
		if (ets->table)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(ets->table);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
	}
}
	
static void
e_table_scrolled_class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->get_arg = ets_get_arg;
	
	gtk_object_add_arg_type ("ETableScrolled::table", GTK_TYPE_OBJECT,
				 GTK_ARG_READABLE, ARG_TABLE);
}

E_MAKE_TYPE(e_table_scrolled, "ETableScrolled", ETableScrolled, e_table_scrolled_class_init, e_table_scrolled_init, PARENT_TYPE);

