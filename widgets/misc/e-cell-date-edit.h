/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * ECellDateEdit - a subclass of ECellPopup used to show a date with a popup
 * window to edit it.
 */

#ifndef _E_CELL_DATE_EDIT_H_
#define _E_CELL_DATE_EDIT_H_

#include <gal/e-table/e-cell-popup.h>

#define E_CELL_DATE_EDIT_TYPE        (e_cell_date_edit_get_type ())
#define E_CELL_DATE_EDIT(o)          (GTK_CHECK_CAST ((o), E_CELL_DATE_EDIT_TYPE, ECellDateEdit))
#define E_CELL_DATE_EDIT_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CELL_DATE_EDIT_TYPE, ECellDateEditClass))
#define E_IS_CELL_DATE_EDIT(o)       (GTK_CHECK_TYPE ((o), E_CELL_DATE_EDIT_TYPE))
#define E_IS_CELL_DATE_EDIT_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CELL_DATE_EDIT_TYPE))


typedef struct {
	ECellPopup parent;

	GtkWidget *popup_window;
	GtkWidget *time_list;

	/* This is the range of hours we show in the time popup. */
	gint lower_hour;
	gint upper_hour;

	gboolean use_24_hour_format;
} ECellDateEdit;

typedef struct {
	ECellPopupClass parent_class;
} ECellDateEditClass;


GtkType    e_cell_date_edit_get_type		(void);
ECell     *e_cell_date_edit_new			(void);


#endif /* _E_CELL_DATE_EDIT_H_ */
