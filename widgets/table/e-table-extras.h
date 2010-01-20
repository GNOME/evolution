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

#ifndef E_TABLE_EXTRAS_H
#define E_TABLE_EXTRAS_H

#include <table/e-cell.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define E_TYPE_TABLE_EXTRAS \
	(e_table_extras_get_type ())
#define E_TABLE_EXTRAS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_EXTRAS, ETableExtras))
#define E_TABLE_EXTRAS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_EXTRAS, ETableExtrasClass))
#define E_IS_TABLE_EXTRAS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_EXTRAS))
#define E_IS_TABLE_EXTRAS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_EXTRAS))
#define E_TABLE_EXTRAS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_EXTRAS, ETableExtrasClass))

G_BEGIN_DECLS

typedef struct _ETableExtras ETableExtras;
typedef struct _ETableExtrasClass ETableExtrasClass;
typedef struct _ETableExtrasPrivate ETableExtrasPrivate;

struct _ETableExtras {
	GObject parent;
	ETableExtrasPrivate *priv;
};

struct _ETableExtrasClass {
	GObjectClass parent_class;
};

GType		e_table_extras_get_type		(void);
ETableExtras *	e_table_extras_new		(void);
void		e_table_extras_add_cell		(ETableExtras *extras,
						 const gchar *id,
						 ECell *cell);
ECell *		e_table_extras_get_cell		(ETableExtras *extras,
						 const gchar *id);
void		e_table_extras_add_compare	(ETableExtras *extras,
						 const gchar *id,
						 GCompareDataFunc compare);
GCompareDataFunc e_table_extras_get_compare	(ETableExtras *extras,
						 const gchar *id);
void		e_table_extras_add_search	(ETableExtras *extras,
						 const gchar *id,
						 ETableSearchFunc search);
ETableSearchFunc
		e_table_extras_get_search	(ETableExtras *extras,
						 const gchar *id);
void		e_table_extras_add_icon_name	(ETableExtras *extras,
						 const gchar *id,
						 const gchar *icon_name);
const gchar *	e_table_extras_get_icon_name	(ETableExtras *extras,
						 const gchar *id);

G_END_DECLS

#endif /* E_TABLE_EXTRAS_H */
