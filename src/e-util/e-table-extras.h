/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TABLE_EXTRAS_H
#define E_TABLE_EXTRAS_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <e-util/e-cell.h>

/* Standard GObject macros */
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

GType		e_table_extras_get_type		(void) G_GNUC_CONST;
ETableExtras *	e_table_extras_new		(void);
void		e_table_extras_add_cell		(ETableExtras *extras,
						 const gchar *id,
						 ECell *cell);
ECell *		e_table_extras_get_cell		(ETableExtras *extras,
						 const gchar *id);
void		e_table_extras_add_compare	(ETableExtras *extras,
						 const gchar *id,
						 GCompareDataFunc compare);
GCompareDataFunc
		e_table_extras_get_compare	(ETableExtras *extras,
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
