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

	/*
	 * The ETableCol used to group this set
	 */
	ETableCol    *ecol;

	/*
	 * The canvas rectangle that contains the children
	 */
	GnomeCanvasItem *rect;

	/*
	 * Dimensions of the ETableGroup
	 */
	int width, height;

	/*
	 * State: the ETableGroup is open or closed
	 */
	guint open:1;

	/*
	 * Whether we should add indentation and open/close markers,
	 * or if we just act as containers of subtables.
	 */
	guint transparent:1;

	/*
	 * List of GnomeCanvasItems we stack
	 */
	GSList *children;
} ETableGroup;

typedef struct {
	GnomeCanvasGroupClass parent_class;
	void (*height_changed) (ETableGroup *etg);
} ETableGroupClass;

GnomeCanvasItem *e_table_group_new       (GnomeCanvasGroup *parent, ETableCol *ecol,
					  gboolean open, gboolean transparent);
void             e_table_group_construct (GnomeCanvasGroup *parent, ETableGroup *etg, 
					  ETableCol *ecol, gboolean open, gboolean transparent);

void             e_table_group_add       (ETableGroup *etg, GnomeCanvasItem *child);

GtkType          e_table_group_get_type  (void);

#endif /* _E_TABLE_TREE_H_ */
