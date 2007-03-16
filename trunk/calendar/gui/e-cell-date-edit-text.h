/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ECellDateEditText - a subclass of ECellText used to show and edit the text
 * representation of the date, from a ECalComponentDateTime* model value.
 */

#ifndef _E_CELL_DATE_EDIT_TEXT_H_
#define _E_CELL_DATE_EDIT_TEXT_H_

#include <libical/ical.h>
#include <table/e-cell-text.h>

G_BEGIN_DECLS

#define E_CELL_DATE_EDIT_TEXT_TYPE        (e_cell_date_edit_text_get_type ())
#define E_CELL_DATE_EDIT_TEXT(o)          (GTK_CHECK_CAST ((o), E_CELL_DATE_EDIT_TEXT_TYPE, ECellDateEditText))
#define E_CELL_DATE_EDIT_TEXT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_DATE_EDIT_TEXT_TYPE, ECellDateEditTextClass))
#define E_IS_CELL_DATE_EDIT_TEXT(o)       (GTK_CHECK_TYPE ((o), E_CELL_DATE_EDIT_TEXT_TYPE))
#define E_IS_CELL_DATE_EDIT_TEXT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_DATE_EDIT_TEXT_TYPE))

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

GtkType    e_cell_date_edit_text_get_type (void);
ECell     *e_cell_date_edit_text_new      (const char *fontname,
					   GtkJustification justify);


void	   e_cell_date_edit_text_set_timezone (ECellDateEditText *ecd,
					       icaltimezone *zone);
void	   e_cell_date_edit_text_set_use_24_hour_format (ECellDateEditText *ecd,
							 gboolean use_24_hour);
G_END_DECLS

#endif /* _E_CELL_DATE_EDIT_TEXT_H_ */
