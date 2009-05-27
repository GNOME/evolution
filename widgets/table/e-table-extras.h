/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TABLE_EXTRAS_H_
#define _E_TABLE_EXTRAS_H_

#include <glib-object.h>
#include <table/e-cell.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define E_TABLE_EXTRAS_TYPE        (e_table_extras_get_type ())
#define E_TABLE_EXTRAS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_EXTRAS_TYPE, ETableExtras))
#define E_TABLE_EXTRAS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_EXTRAS_TYPE, ETableExtrasClass))
#define E_IS_TABLE_EXTRAS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_EXTRAS_TYPE))
#define E_IS_TABLE_EXTRAS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_EXTRAS_TYPE))
#define E_TABLE_EXTRAS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_TABLE_EXTRAS_TYPE, ETableExtrasClass))

typedef struct {
	GObject base;

	GHashTable *cells;
	GHashTable *compares;
	GHashTable *pixbufs;
	GHashTable *searches;
} ETableExtras;

typedef struct {
	GObjectClass parent_class;
} ETableExtrasClass;

GType             e_table_extras_get_type     (void);
ETableExtras     *e_table_extras_new          (void);

void              e_table_extras_add_cell     (ETableExtras     *extras,
					       const gchar      *id,
					       ECell            *cell);
ECell            *e_table_extras_get_cell     (ETableExtras     *extras,
					       const gchar      *id);

void              e_table_extras_add_compare  (ETableExtras     *extras,
					       const gchar      *id,
					       GCompareFunc      compare);
GCompareFunc      e_table_extras_get_compare  (ETableExtras     *extras,
					       const gchar      *id);

void              e_table_extras_add_search   (ETableExtras     *extras,
					       const gchar      *id,
					       ETableSearchFunc  search);
ETableSearchFunc  e_table_extras_get_search   (ETableExtras     *extras,
					       const gchar      *id);

void              e_table_extras_add_pixbuf   (ETableExtras     *extras,
					       const gchar      *id,
					       GdkPixbuf        *pixbuf);
GdkPixbuf        *e_table_extras_get_pixbuf   (ETableExtras     *extras,
					       const gchar      *id);

G_END_DECLS

#endif /* _E_TABLE_EXTRAS_H_ */
