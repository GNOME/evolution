/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
 * SPDX-FileContributor: Chris Lahey  <clahey@ximina.com
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
