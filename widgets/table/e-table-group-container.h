/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_GROUP_CONTAINER_H_
#define _E_TABLE_GROUP_CONTAINER_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-model.h"
#include "e-table-header.h"
#include "e-table-group.h"

#define E_TABLE_GROUP_CONTAINER_TYPE        (e_table_group_container_get_type ())
#define E_TABLE_GROUP_CONTAINER(o)          (GTK_CHECK_CAST ((o), E_TABLE_GROUP_CONTAINER_TYPE, ETableGroupContainer))
#define E_TABLE_GROUP_CONTAINER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_GROUP_CONTAINER_TYPE, ETableGroupContainerClass))
#define E_IS_TABLE_GROUP_CONTAINER(o)       (GTK_CHECK_TYPE ((o), E_TABLE_GROUP_CONTAINER_TYPE))
#define E_IS_TABLE_GROUP_CONTAINER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_GROUP_CONTAINER_TYPE))

typedef struct {
	ETableGroup group;

	/*
	 * The ETableCol used to group this set
	 */
	ETableCol    *ecol;
	gint          ascending;

	/*
	 * List of ETableGroups we stack
	 */
	GList *children;

	/*
	 * The canvas rectangle that contains the children
	 */
	GnomeCanvasItem *rect;

	GdkFont *font;

	gdouble width, height, minimum_width;

	ETableSortInfo *sort_info;
	int n;
	int length_threshold;

	guint draw_grid : 1;
	guint draw_focus : 1;
	guint mode_spreadsheet : 1;

	/*
	 * State: the ETableGroup is open or closed
	 */
	guint open:1;
} ETableGroupContainer;

typedef struct {
	ETableGroupClass parent_class;
} ETableGroupContainerClass;

ETableGroup *e_table_group_container_new       (GnomeCanvasGroup *parent, ETableHeader *full_header, ETableHeader     *header,
						ETableModel *model, ETableSortInfo *sort_info, int n);
void         e_table_group_container_construct (GnomeCanvasGroup *parent, ETableGroupContainer *etgc,
						ETableHeader *full_header,
						ETableHeader     *header,
						ETableModel *model, ETableSortInfo *sort_info, int n);

GtkType      e_table_group_container_get_type  (void);

#endif /* _E_TABLE_GROUP_CONTAINER_H_ */
