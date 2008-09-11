/*
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

#ifndef _E_TABLE_CONFIG_FIELD_H_
#define _E_TABLE_CONFIG_FIELD_H_

#include <gtk/gtkvbox.h>
#include <table/e-table-sort-info.h>
#include <table/e-table-specification.h>

G_BEGIN_DECLS

#define E_TABLE_CONFIG_FIELD_TYPE        (e_table_config_field_get_type ())
#define E_TABLE_CONFIG_FIELD(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_CONFIG_FIELD_TYPE, ETableConfigField))
#define E_TABLE_CONFIG_FIELD_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_CONFIG_FIELD_TYPE, ETableConfigFieldClass))
#define E_IS_TABLE_CONFIG_FIELD(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_CONFIG_FIELD_TYPE))
#define E_IS_TABLE_CONFIG_FIELD_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_CONFIG_FIELD_TYPE))

typedef struct {
	GtkVBox base;

	ETableSpecification *spec;
	ETableSortInfo *sort_info;
	guint grouping : 1;
	int n;

	GtkWidget *combo;
	GtkWidget *radio_ascending;
	GtkWidget *radio_descending;

	GtkWidget *child_fields;
} ETableConfigField;

typedef struct {
	GtkVBoxClass parent_class;
} ETableConfigFieldClass;

GType              e_table_config_field_get_type  (void);
ETableConfigField *e_table_config_field_new       (ETableSpecification *spec,
						   ETableSortInfo      *sort_info,
						   gboolean             grouping);
ETableConfigField *e_table_config_field_construct (ETableConfigField   *field,
						   ETableSpecification *spec,
						   ETableSortInfo      *sort_info,
						   gboolean             grouping);

G_END_DECLS

#endif /* _E_TABLE_CONFIG_FIELD_H_ */
