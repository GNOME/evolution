/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_CONFIG_H_
#define _E_TABLE_CONFIG_H_

#include <gnome.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-specification.h>

#define E_TABLE_CONFIG_TYPE        (e_table_config_get_type ())
#define E_TABLE_CONFIG(o)          (GTK_CHECK_CAST ((o), E_TABLE_CONFIG_TYPE, ETableConfig))
#define E_TABLE_CONFIG_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_CONFIG_TYPE, ETableConfigClass))
#define E_IS_TABLE_CONFIG(o)       (GTK_CHECK_TYPE ((o), E_TABLE_CONFIG_TYPE))
#define E_IS_TABLE_CONFIG_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_CONFIG_TYPE))

typedef struct {
	GnomeDialog base;

	ETableSpecification *spec;
	ETableState         *state;

	GtkWidget *sort_label;
	GtkWidget *group_label;

	GtkWidget *sort_dialog;
	GtkWidget *group_dialog;

	int sorting_changed_id;
	int grouping_changed_id;
} ETableConfig;

typedef struct {
	GnomeDialogClass parent_class;
} ETableConfigClass;

GtkType    e_table_config_get_type  (void);
GtkWidget *e_table_config_new       (ETableSpecification *spec,
				     ETableState         *state);
GtkWidget *e_table_config_construct (ETableConfig        *etco,
				     ETableSpecification *spec,
				     ETableState         *state);

#endif /* _E_TABLE_CONFIG_H */
