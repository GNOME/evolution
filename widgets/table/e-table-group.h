/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_GROUP_H_
#define _E_TABLE_GROUP_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-model.h"
#include "e-table-header.h"
#include "e-table-sort-info.h"
#include "e-util/e-util.h"

#define E_TABLE_GROUP_TYPE        (e_table_group_get_type ())
#define E_TABLE_GROUP(o)          (GTK_CHECK_CAST ((o), E_TABLE_GROUP_TYPE, ETableGroup))
#define E_TABLE_GROUP_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_GROUP_TYPE, ETableGroupClass))
#define E_IS_TABLE_GROUP(o)       (GTK_CHECK_TYPE ((o), E_TABLE_GROUP_TYPE))
#define E_IS_TABLE_GROUP_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_GROUP_TYPE))

typedef struct {
	GnomeCanvasGroup group;

	/*
	 * The full header.
	 */
	ETableHeader *full_header;
	ETableHeader *header;
	
	/*
	 * The model we pull data from.
	 */
	ETableModel *model;

	/*
	 * Whether we should add indentation and open/close markers,
	 * or if we just act as containers of subtables.
	 */
	guint transparent : 1;

	guint has_focus : 1;
	
	guint frozen : 1;
} ETableGroup;

typedef struct {
	GnomeCanvasGroupClass parent_class;
	void        (*row_selection)      (ETableGroup *etg, int row, gboolean selected);

	void (*add) (ETableGroup *etg, gint row);
	gboolean (*remove) (ETableGroup *etg, gint row);
	gint (*get_count) (ETableGroup *etg);
	void (*increment) (ETableGroup *etg, gint position, gint amount);
	void (*set_focus) (ETableGroup *etg, EFocus direction, gint view_col);
	gboolean (*get_focus) (ETableGroup *etg);
	gint (*get_focus_column) (ETableGroup *etg);
	ETableCol *(*get_ecol) (ETableGroup *etg);

	void (*thaw) (ETableGroup *etg);
	gdouble (*get_height) (ETableGroup *etg);
	gdouble (*get_width) (ETableGroup *etg);
	void (*set_width) (ETableGroup *etg, gdouble width);
} ETableGroupClass;

void             e_table_group_add       (ETableGroup      *etg,
					  gint              row);
gboolean         e_table_group_remove    (ETableGroup      *etg,
					  gint              row);
gint             e_table_group_get_count (ETableGroup      *etg);
void             e_table_group_increment (ETableGroup      *etg,
					  gint              position,
					  gint              amount);
void             e_table_group_set_focus (ETableGroup      *etg,
					  EFocus            direction,
					  gint              view_col);
gboolean         e_table_group_get_focus (ETableGroup      *etg);
gint             e_table_group_get_focus_column (ETableGroup      *etg);
ETableHeader    *e_table_group_get_header (ETableGroup     *etg);
ETableCol       *e_table_group_get_ecol  (ETableGroup      *etg);

ETableGroup     *e_table_group_new       (GnomeCanvasGroup *parent,
					  ETableHeader     *full_header,
					  ETableHeader     *header,
					  ETableModel      *model,
					  ETableSortInfo   *sort_info,
					  int               n);
void             e_table_group_construct (GnomeCanvasGroup *parent,
					  ETableGroup      *etg,
					  ETableHeader     *full_header,
					  ETableHeader     *header,
					  ETableModel      *model);

/* For emitting the signals */
void             e_table_group_row_selection (ETableGroup      *etg,
					      gint              row,
					      gboolean          selected);

GtkType          e_table_group_get_type  (void);

#endif /* _E_TABLE_GROUP_H_ */
