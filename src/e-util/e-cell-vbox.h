/*
 *
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
 *		Chris Toshok <toshok@ximian.com>
 *		Chris Lahey  <clahey@ximina.com
 *
 * A majority of code taken from:
 * the ECellText renderer.
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CELL_VBOX_H_
#define _E_CELL_VBOX_H_

#include <libgnomecanvas/libgnomecanvas.h>

#include <e-util/e-cell.h>

/* Standard GObject macros */
#define E_TYPE_CELL_VBOX \
	(e_cell_vbox_get_type ())
#define E_CELL_VBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_VBOX, ECellVbox))
#define E_CELL_VBOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_VBOX, ECellVboxClass))
#define E_IS_CELL_VBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_VBOX))
#define E_IS_CELL_VBOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_VBOX))
#define E_CELL_VBOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_VBOX, ECellVboxClass))

G_BEGIN_DECLS

typedef struct _ECellVbox ECellVbox;
typedef struct _ECellVboxView ECellVboxView;
typedef struct _ECellVboxClass ECellVboxClass;

struct _ECellVbox {
	ECell parent;

	gint subcell_count;
	ECell **subcells;
	gint *model_cols;
};

struct _ECellVboxView  {
	ECellView cell_view;

	gint subcell_view_count;
	ECellView **subcell_views;
	gint *model_cols;
};

struct _ECellVboxClass {
	ECellClass parent_class;
};

GType		e_cell_vbox_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_vbox_new			(void);
void		e_cell_vbox_append		(ECellVbox *vbox,
						 ECell *subcell,
						 gint model_col);

G_END_DECLS

#endif /* _E_CELL_VBOX_H_ */
