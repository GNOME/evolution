/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-search.h
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

#ifndef _E_TABLE_SEARCH_H_
#define _E_TABLE_SEARCH_H_

#include <gtk/gtkobject.h>

G_BEGIN_DECLS

#define E_TABLE_SEARCH_TYPE        (e_table_search_get_type ())
#define E_TABLE_SEARCH(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_SEARCH_TYPE, ETableSearch))
#define E_TABLE_SEARCH_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_SEARCH_TYPE, ETableSearchClass))
#define E_IS_TABLE_SEARCH(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_SEARCH_TYPE))
#define E_IS_TABLE_SEARCH_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_SEARCH_TYPE))
#define E_TABLE_SEARCH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_SEARCH_TYPE, ETableSearchClass))

typedef struct _ETableSearchPrivate ETableSearchPrivate;

typedef enum {
	E_TABLE_SEARCH_FLAGS_CHECK_CURSOR_FIRST = 1 << 0
} ETableSearchFlags;

typedef struct {
	GObject   base;

	ETableSearchPrivate *priv;
} ETableSearch;

typedef struct {
	GObjectClass parent_class;

	/*
	 * Signals
	 */
	gboolean (*search)    (ETableSearch *ets, char *string /* utf8 */, ETableSearchFlags flags);
	void     (*accept)    (ETableSearch *ets);
} ETableSearchClass;

GType       e_table_search_get_type         (void);
ETableSearch *e_table_search_new              (void);

/**/
void          e_table_search_input_character  (ETableSearch *e_table_search,
					       gunichar      character);
gboolean      e_table_search_backspace        (ETableSearch *e_table_search);
void          e_table_search_cancel           (ETableSearch *e_table_search);

G_END_DECLS

#endif /* _E_TABLE_SEARCH_H_ */
