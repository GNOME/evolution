/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_CLICK_TO_ADD_H_
#define _E_TABLE_CLICK_TO_ADD_H_

#include <libxml/tree.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-table-header.h>
#include <e-util/e-table-item.h>
#include <e-util/e-table-selection-model.h>
#include <e-util/e-table-sort-info.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_CLICK_TO_ADD \
	(e_table_click_to_add_get_type ())
#define E_TABLE_CLICK_TO_ADD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_CLICK_TO_ADD, ETableClickToAdd))
#define E_TABLE_CLICK_TO_ADD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_CLICK_TO_ADD, ETableClickToAddClass))
#define E_IS_TABLE_CLICK_TO_ADD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_CLICK_TO_ADD))
#define E_IS_TABLE_CLICK_TO_ADD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_CLICK_TO_ADD))
#define E_TABLE_CLICK_TO_ADD_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_CLICK_TO_ADD, ETableClickToAddClass))

G_BEGIN_DECLS

typedef struct _ETableClickToAdd ETableClickToAdd;
typedef struct _ETableClickToAddClass ETableClickToAddClass;

struct _ETableClickToAdd {
	GnomeCanvasGroup  parent;

	ETableModel      *one;    /* The ETableOne. */

	ETableModel      *model;  /* The backend model. */
	ETableHeader     *eth;    /* This is just to give to the ETableItem. */

	gchar             *message;

	GnomeCanvasItem  *row;    /* If row is NULL, we're sitting with
				   * no data and a "Click here" message. */
	GnomeCanvasItem  *text;   /* If text is NULL, row shouldn't be. */
	GnomeCanvasItem  *rect;   /* What the heck.  Why not. */

	gdouble           width;
	gdouble           height;

	ETableSelectionModel *selection;
};

struct _ETableClickToAddClass {
	GnomeCanvasGroupClass parent_class;

	/* Signals */
	void		(*cursor_change)	(ETableClickToAdd *etcta,
						 gint row,
						 gint col);
	void		(*style_updated)	(ETableClickToAdd *etcta);
};

GType		e_table_click_to_add_get_type	(void) G_GNUC_CONST;
void		e_table_click_to_add_commit	(ETableClickToAdd *etcta);
gboolean	e_table_click_to_add_is_editing	(ETableClickToAdd *etcta);

G_END_DECLS

#endif /* _E_TABLE_CLICK_TO_ADD_H_ */
