/*
 * e-cell-pixbuf.h: An ECell that displays a GdkPixbuf
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors: Vladimir Vukicevic <vladimir@ximian.com>
 *
 */

#ifndef _E_CELL_PIXBUF_H_
#define _E_CELL_PIXBUF_H_

#include <gal/e-table/e-table.h>

#define E_CELL_PIXBUF_TYPE		(e_cell_pixbuf_get_type ())
#define E_CELL_PIXBUF(o)		(GTK_CHECK_CAST ((o), E_CELL_PIXBUF_TYPE, ECellPixbuf))
#define E_CELL_PIXBUF_CLASS(k)	(GTK_CHECK_CAST_CLASS ((k), E_CELL_PIXBUF_TYPE, ECellPixbufClass))
#define E_IS_CELL_PIXBUF(o)		(GTK_CHECK_TYPE ((o), E_CELL_PIXBUF_TYPE))
#define E_IS_CELL_PIXBUF_CLASS(k)	(GTK_CHECK_CLASS_TYPE ((k), E_CELL_PIXBUF_TYPE))

typedef struct _ECellPixbuf ECellPixbuf;
typedef struct _ECellPixbufClass ECellPixbufClass;

struct _ECellPixbuf {
    ECell parent;
};

struct _ECellPixbufClass {
    ECellClass parent_class;
};

GtkType e_cell_pixbuf_get_type (void);
ECell *e_cell_pixbuf_new (void);
void e_cell_pixbuf_construct (ECellPixbuf *ecp);

#endif
