/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-table-sort-info.c: a Table Model
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * (C) 1999 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/tree.h>
#include "e-table-sort-info.h"

#define ETM_CLASS(e) ((ETableSortInfoClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ();
					  

static GtkObjectClass *e_table_sort_info_parent_class;

enum {
	SORT_INFO_CHANGED,
	LAST_SIGNAL
};

static guint e_table_sort_info_signals [LAST_SIGNAL] = { 0, };

enum {
	ARG_0,
	ARG_GROUPING
};

static void
etsi_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableSortInfo *etsi;

	etsi = E_TABLE_SORT_INFO (o);

	switch (arg_id){
	case ARG_GROUPING:
		etsi->grouping = GTK_VALUE_POINTER (*arg);
		break;
	}
}

static void
etsi_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	ETableSortInfo *etsi;

	etsi = E_TABLE_SORT_INFO (o);

	switch (arg_id){
	case ARG_GROUPING:
		GTK_VALUE_POINTER (*arg) = etsi->grouping;
		break;
	}
}

static void
e_table_sort_info_class_init (GtkObjectClass *object_class)
{
	e_table_sort_info_parent_class = gtk_type_class (gtk_object_get_type ());
	
	object_class->set_arg = etsi_set_arg;
	object_class->get_arg = etsi_get_arg;

	e_table_sort_info_signals [SORT_INFO_CHANGED] =
		gtk_signal_new ("sort_info_changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ETableSortInfoClass, sort_info_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_table_sort_info_signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("ETableSortInfo::grouping", GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE, ARG_GROUPING);
}


guint
e_table_sort_info_get_type (void)
{
  static guint type = 0;

  if (!type)
    {
      GtkTypeInfo info =
      {
	"ETableSortInfo",
	sizeof (ETableSortInfo),
	sizeof (ETableSortInfoClass),
	(GtkClassInitFunc) e_table_sort_info_class_init,
	NULL,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      type = gtk_type_unique (gtk_object_get_type (), &info);
    }

  return type;
}

void
e_table_sort_info_changed (ETableSortInfo *e_table_sort_info)
{
	g_return_if_fail (e_table_sort_info != NULL);
	g_return_if_fail (E_IS_TABLE_SORT_INFO (e_table_sort_info));
	
	gtk_signal_emit (GTK_OBJECT (e_table_sort_info),
			 e_table_sort_info_signals [SORT_INFO_CHANGED]);
}

ETableSortInfo *
e_table_sort_info_new (void)
{
	return gtk_type_new (e_table_sort_info_get_type ());
}
