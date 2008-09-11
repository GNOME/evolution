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

#ifndef _E_TABLE_TREE_H_
#define _E_TABLE_TREE_H_

#include <table/e-table-model.h>

G_BEGIN_DECLS

typedef struct {
	char *title;

	union {
		ETableModel *table;
		GList *children;
	} u;

	guint expanded :1;
	guint is_leaf  :1;
} ETableGroup;

ETableGroup *e_table_group_new      (const char *title, ETableModel *table);
ETableGroup *e_table_group_new_leaf (const char *title);

G_END_DECLS

#endif /* _E_TABLE_TREE_H_ */
