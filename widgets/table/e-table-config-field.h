/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_CONFIG_FIELD_H_
#define _E_TABLE_CONFIG_FIELD_H_

#include <gtk/gtkvbox.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-specification.h>

#define E_TABLE_CONFIG_FIELD_TYPE        (e_table_config_field_get_type ())
#define E_TABLE_CONFIG_FIELD(o)          (GTK_CHECK_CAST ((o), E_TABLE_CONFIG_FIELD_TYPE, ETableConfigField))
#define E_TABLE_CONFIG_FIELD_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_CONFIG_FIELD_TYPE, ETableConfigFieldClass))
#define E_IS_TABLE_CONFIG_FIELD(o)       (GTK_CHECK_TYPE ((o), E_TABLE_CONFIG_FIELD_TYPE))
#define E_IS_TABLE_CONFIG_FIELD_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_CONFIG_FIELD_TYPE))

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

GtkType            e_table_config_field_get_type  (void);
ETableConfigField *e_table_config_field_new       (ETableSpecification *spec,
						   ETableSortInfo      *sort_info,
						   gboolean             grouping);
ETableConfigField *e_table_config_field_construct (ETableConfigField   *field,
						   ETableSpecification *spec,
						   ETableSortInfo      *sort_info,
						   gboolean             grouping);

#endif /* _E_TABLE_CONFIG_FIELD_H_ */
