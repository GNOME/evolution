/*
 * ECellDateEditText - a subclass of ECellText used to show and edit the text
 * representation of the date, from a ECalComponentDateTime* model value.
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
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef _E_CELL_DATE_EDIT_TEXT_H_
#define _E_CELL_DATE_EDIT_TEXT_H_

#include <libical/ical.h>
#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CELL_DATE_EDIT_TEXT \
	(e_cell_date_edit_text_get_type ())
#define E_CELL_DATE_EDIT_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_DATE_EDIT_TEXT, ECellDateEditText))
#define E_CELL_DATE_EDIT_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_DATE_EDIT_TEXT, ECellDateEditTextClass))
#define E_IS_CELL_DATE_EDIT_TEXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_DATE_EDIT_TEXT))
#define E_IS_CELL_DATE_EDIT_TEXT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_DATE_EDIT_TEXT))
#define E_CELL_DATE_EDIT_TEXT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_DATE_EDIT_TEXT, ECellDateEditTextClass))

G_BEGIN_DECLS

typedef struct _ECellDateEditText ECellDateEditText;
typedef struct _ECellDateEditTextClass ECellDateEditTextClass;
typedef struct _ECellDateEditTextPrivate ECellDateEditTextPrivate;
typedef struct _ECellDateEditValue ECellDateEditValue;

struct _ECellDateEditValue {
	struct icaltimetype tt;
	icaltimezone *zone;
};

struct _ECellDateEditText {
	ECellText parent;
	ECellDateEditTextPrivate *priv;
};

struct _ECellDateEditTextClass {
	ECellTextClass parent_class;
};

GType		e_cell_date_edit_text_get_type	(void);
ECell *		e_cell_date_edit_text_new	(const gchar *fontname,
						 GtkJustification justify);
icaltimezone *	e_cell_date_edit_text_get_timezone
						(ECellDateEditText *ecd);
void		e_cell_date_edit_text_set_timezone
						(ECellDateEditText *ecd,
						 icaltimezone *timezone);
gboolean	e_cell_date_edit_text_get_use_24_hour_format
						(ECellDateEditText *ecd);
void		e_cell_date_edit_text_set_use_24_hour_format
						(ECellDateEditText *ecd,
						 gboolean use_24_hour);
gint		e_cell_date_edit_compare_cb	(gconstpointer a,
						 gconstpointer b,
						 gpointer cmp_cache);

G_END_DECLS

#endif /* _E_CELL_DATE_EDIT_TEXT_H_ */
