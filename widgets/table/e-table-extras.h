/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_EXTRAS_H_
#define _E_TABLE_EXTRAS_H_

#include <gtk/gtkobject.h>
#include <gal/e-table/e-cell.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

#define E_TABLE_EXTRAS_TYPE        (e_table_extras_get_type ())
#define E_TABLE_EXTRAS(o)          (GTK_CHECK_CAST ((o), E_TABLE_EXTRAS_TYPE, ETableExtras))
#define E_TABLE_EXTRAS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_EXTRAS_TYPE, ETableExtrasClass))
#define E_IS_TABLE_EXTRAS(o)       (GTK_CHECK_TYPE ((o), E_TABLE_EXTRAS_TYPE))
#define E_IS_TABLE_EXTRAS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_EXTRAS_TYPE))

typedef struct {
	GtkObject base;

	GHashTable *cells;
	GHashTable *compares;
	GHashTable *pixbufs;
} ETableExtras;

typedef struct {
	GtkObjectClass parent_class;
} ETableExtrasClass;

GtkType       e_table_extras_get_type     (void);
ETableExtras *e_table_extras_new          (void);

void          e_table_extras_add_cell     (ETableExtras *extras,
					   char         *id,
					   ECell        *cell);
ECell        *e_table_extras_get_cell     (ETableExtras *extras,
					   char         *id);

void          e_table_extras_add_compare  (ETableExtras *extras,
					   char         *id,
					   GCompareFunc  compare);
GCompareFunc  e_table_extras_get_compare  (ETableExtras *extras,
					   char         *id);

void          e_table_extras_add_pixbuf   (ETableExtras *extras,
					   char         *id,
					   GdkPixbuf    *pixbuf);
GdkPixbuf    *e_table_extras_get_pixbuf   (ETableExtras *extras,
					   char         *id);

END_GNOME_DECLS

#endif /* _E_TABLE_EXTRAS_H_ */
