#ifndef _E_TABLE_TREE_H_
#define _E_TABLE_TREE_H_

#include <libgnomeui/gnome-canvas.h>
#include "e-table-model.h"
#include "e-table-header.h"

#define E_TABLE_GROUP_TYPE        (e_table_group_get_type ())
#define E_TABLE_GROUP(o)          (GTK_CHECK_CAST ((o), E_TABLE_GROUP_TYPE, ETableGroup))
#define E_TABLE_GROUP_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_GROUP_TYPE, ETableGroupClass))
#define E_IS_TABLE_GROUP(o)       (GTK_CHECK_TYPE ((o), E_TABLE_GROUP_TYPE))
#define E_IS_TABLE_GROUP_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_GROUP_TYPE))

typedef struct {
	GnomeCanvasGroup group;

	ETableHeader *header;
	ETableCol    *ecol;
	int col;
	int open;
	GnomeCanvasItem *rect, *child;
} ETableGroup;

typedef struct {
	GnomeCanvasGroupClass parent_class;
} ETableGroupClass;

GnomeCanvasItem *e_table_group_new       (GnomeCanvasGroup *parent, ETableHeader *header,
					  int col, GnomeCanvasItem *child, int open);
void             e_table_group_construct (GnomeCanvasGroup *parent, ETableGroup *etg, 
					  ETableHeader *header, int col,
					  GnomeCanvasItem *child, int open);
GtkType          e_table_group_get_type (void);

#endif /* _E_TABLE_TREE_H_ */
