/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_CONFIG_H_
#define _E_TABLE_CONFIG_H_

#include <gnome.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-specification.h>
#include <gal/widgets/gtk-combo-text.h>

#define E_TABLE_CONFIG_TYPE        (e_table_config_get_type ())
#define E_TABLE_CONFIG(o)          (GTK_CHECK_CAST ((o), E_TABLE_CONFIG_TYPE, ETableConfig))
#define E_TABLE_CONFIG_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_CONFIG_TYPE, ETableConfigClass))
#define E_IS_TABLE_CONFIG(o)       (GTK_CHECK_TYPE ((o), E_TABLE_CONFIG_TYPE))
#define E_IS_TABLE_CONFIG_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_CONFIG_TYPE))

typedef struct {
	GtkComboText *combo;
	GtkWidget    *frames;
	GtkWidget    *radio_ascending;
	GtkWidget    *radio_descending;
	guint         changed_id, toggled_id;
	void *e_table_config;
} ETableConfigSortWidgets; 

typedef struct {
	GtkObject parent;

	/*
	 * Our various dialog boxes
	 */
	GtkWidget *dialog_toplevel;
	GtkWidget *dialog_show_fields;
	GtkWidget *dialog_group_by;
	GtkWidget *dialog_sort;

	/*
	 * The state we manipulate
	 */
	ETableSpecification *source_spec;
	ETableState         *source_state, *state, *temp_state;

	GtkWidget *sort_label;
	GtkWidget *group_label;
	GtkWidget *fields_label;

	ETableConfigSortWidgets sort [4];

	/*
	 * List of valid column names
	 */
	GSList *column_names;
} ETableConfig;

typedef struct {
	GtkObjectClass parent_class;
} ETableConfigClass;

GtkType       e_table_config_get_type  (void);
ETableConfig *e_table_config_new       (const char          *header,
					ETableSpecification *spec,
					ETableState         *state);
ETableConfig *e_table_config_construct (ETableConfig        *etco,
					const char          *header,
					ETableSpecification *spec,
					ETableState         *state);
void          e_table_config_raise     (ETableConfig        *config);

#endif /* _E_TABLE_CONFIG_H */
