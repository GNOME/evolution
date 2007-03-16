/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-scrolled.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_TABLE_SCROLLED_H_
#define _E_TABLE_SCROLLED_H_

#include <gtk/gtkscrolledwindow.h>
#include <table/e-table-model.h>
#include <table/e-table.h>

G_BEGIN_DECLS

#define E_TABLE_SCROLLED_TYPE        (e_table_scrolled_get_type ())
#define E_TABLE_SCROLLED(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SCROLLED_TYPE, ETableScrolled))
#define E_TABLE_SCROLLED_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SCROLLED_TYPE, ETableScrolledClass))
#define E_IS_TABLE_SCROLLED(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SCROLLED_TYPE))
#define E_IS_TABLE_SCROLLED_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SCROLLED_TYPE))

typedef struct {
	GtkScrolledWindow parent;

	ETable *table;
} ETableScrolled;

typedef struct {
	GtkScrolledWindowClass parent_class;
} ETableScrolledClass;

GType           e_table_scrolled_get_type                  (void);

ETableScrolled *e_table_scrolled_construct                 (ETableScrolled *ets,
							    ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec,
							    const char     *state);
GtkWidget      *e_table_scrolled_new                       (ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec,
							    const char     *state);

ETableScrolled *e_table_scrolled_construct_from_spec_file  (ETableScrolled *ets,
							    ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec_fn,
							    const char     *state_fn);
GtkWidget      *e_table_scrolled_new_from_spec_file        (ETableModel    *etm,
							    ETableExtras   *ete,
							    const char     *spec_fn,
							    const char     *state_fn);

ETable         *e_table_scrolled_get_table                 (ETableScrolled *ets);

G_END_DECLS

#endif /* _E_TABLE_SCROLLED_H_ */

