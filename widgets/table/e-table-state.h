/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_STATE_H_
#define _E_TABLE_STATE_H_

#include <gtk/gtkobject.h>
#include <gnome-xml/tree.h>
#include <gal/e-table/e-table-sort-info.h>

#define E_TABLE_STATE_TYPE        (e_table_state_get_type ())
#define E_TABLE_STATE(o)          (GTK_CHECK_CAST ((o), E_TABLE_STATE_TYPE, ETableState))
#define E_TABLE_STATE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_STATE_TYPE, ETableStateClass))
#define E_IS_TABLE_STATE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_STATE_TYPE))
#define E_IS_TABLE_STATE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_STATE_TYPE))

typedef struct {
	GtkObject base;

	ETableSortInfo *sort_info;
	int             col_count;
	int            *columns;
	double         *expansions;
} ETableState;

typedef struct {
	GtkObjectClass parent_class;
} ETableStateClass;

GtkType      e_table_state_get_type          (void);
ETableState *e_table_state_new               (void);

gboolean     e_table_state_load_from_file    (ETableState   *state,
					      const char    *filename);
void         e_table_state_load_from_string  (ETableState   *state,
					      const char    *xml);
void         e_table_state_load_from_node    (ETableState   *state,
					      const xmlNode *node);

void         e_table_state_save_to_file      (ETableState   *state,
					      const char    *filename);
char        *e_table_state_save_to_string    (ETableState   *state);
xmlNode     *e_table_state_save_to_node      (ETableState   *state,
					      xmlNode       *parent);

#endif /* _E_TABLE_STATE_H_ */
