/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Vladimir Vukicevic <vladimir@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_CELL_PIXBUF_H_
#define _E_CELL_PIXBUF_H_

#include <e-util/e-table.h>

/* Standard GObject macros */
#define E_TYPE_CELL_PIXBUF \
	(e_cell_pixbuf_get_type ())
#define E_CELL_PIXBUF(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_PIXBUF, ECellPixbuf))
#define E_CELL_PIXBUF_CLASS(cls) \
	(G_TYPE_CHECK_INSTANCE_CAST_CLASS \
	((cls), E_TYPE_CELL_PIXBUF, ECellPixbufClass))
#define E_IS_CELL_PIXBUF(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_PIXBUF))
#define E_IS_CELL_PIXBUF_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_PIXBUF))
#define E_CELL_PIXBUF_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_PIXBUF, ECellPixbufClass))

G_BEGIN_DECLS

typedef struct _ECellPixbuf ECellPixbuf;
typedef struct _ECellPixbufClass ECellPixbufClass;

struct _ECellPixbuf {
	ECell parent;

	gint selected_column;
	gint focused_column;
	gint unselected_column;
};

struct _ECellPixbufClass {
	ECellClass parent_class;
};

GType		e_cell_pixbuf_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_pixbuf_new		(void);

G_END_DECLS

#endif /* _E_CELL_PIXBUF_H */
