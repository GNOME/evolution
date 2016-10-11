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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CELL_DATE_H
#define E_CELL_DATE_H

#include <e-util/e-cell-text.h>

/* Standard GObject macros */
#define E_TYPE_CELL_DATE \
	(e_cell_date_get_type ())
#define E_CELL_DATE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_DATE, ECellDate))
#define E_CELL_DATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_DATE, ECellDateClass))
#define E_IS_CELL_DATE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_DATE))
#define E_IS_CELL_DATE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_DATE))
#define E_CELL_DATE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_DATE, ECellDateClass))

G_BEGIN_DECLS

typedef struct _ECellDate ECellDate;
typedef struct _ECellDateClass ECellDateClass;

struct _ECellDate {
	ECellText parent;
};

struct _ECellDateClass {
	ECellTextClass parent_class;
};

GType		e_cell_date_get_type		(void) G_GNUC_CONST;
ECell *		e_cell_date_new			(const gchar *fontname,
						 GtkJustification justify);
void		e_cell_date_set_format_component
						(ECellDate *ecd,
						 const gchar *fmt_component);
gchar *		e_cell_date_value_to_text	(ECellDate *ecd,
						 gint64 value,
						 gboolean date_only);
gchar *		e_cell_date_tm_to_text		(ECellDate *ecd,
						 struct tm *tm_time,
						 gboolean date_only);
G_END_DECLS

#endif /* E_CELL_DATE_H */
