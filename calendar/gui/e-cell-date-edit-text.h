/*
 * ECellDateEditText - a subclass of ECellText used to show and edit the text
 * representation of the date, from a ECalComponentDateTime* model value.
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
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef _E_CELL_DATE_EDIT_TEXT_H_
#define _E_CELL_DATE_EDIT_TEXT_H_

#include <libical/ical.h>
#include <table/e-cell-text.h>

G_BEGIN_DECLS

#define E_CELL_DATE_EDIT_TEXT_TYPE        (e_cell_date_edit_text_get_type ())
#define E_CELL_DATE_EDIT_TEXT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_CELL_DATE_EDIT_TEXT_TYPE, ECellDateEditText))
#define E_CELL_DATE_EDIT_TEXT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_CELL_DATE_EDIT_TEXT_TYPE, ECellDateEditTextClass))
#define E_IS_CELL_DATE_EDIT_TEXT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_CELL_DATE_EDIT_TEXT_TYPE))
#define E_IS_CELL_DATE_EDIT_TEXT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_CELL_DATE_EDIT_TEXT_TYPE))

typedef struct _ECellDateEditValue ECellDateEditValue;
struct _ECellDateEditValue {
	struct icaltimetype tt;
	icaltimezone *zone;
};

typedef struct {
	ECellText base;

	/* The timezone to display the date in. */
	icaltimezone *zone;

	/* Whether to display in 24-hour format. */
	gboolean use_24_hour_format;
} ECellDateEditText;

typedef struct {
	ECellTextClass parent_class;
} ECellDateEditTextClass;

GType      e_cell_date_edit_text_get_type (void);
ECell     *e_cell_date_edit_text_new      (const gchar *fontname,
					   GtkJustification justify);

void	   e_cell_date_edit_text_set_timezone (ECellDateEditText *ecd,
					       icaltimezone *zone);
void	   e_cell_date_edit_text_set_use_24_hour_format (ECellDateEditText *ecd,
							 gboolean use_24_hour);
G_END_DECLS

#endif /* _E_CELL_DATE_EDIT_TEXT_H_ */
