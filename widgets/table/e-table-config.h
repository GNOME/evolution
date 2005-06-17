/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-config.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Miguel de Icaza <miguel@ximian.com>
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

#ifndef _E_TABLE_CONFIG_H_
#define _E_TABLE_CONFIG_H_

#include <table/e-table-sort-info.h>
#include <table/e-table-specification.h>
#include <widgets/misc/gal-combo-text.h>
#include <table/e-table-without.h>
#include <table/e-table-subset-variable.h>
#include <table/e-table.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define E_TABLE_CONFIG_TYPE        (e_table_config_get_type ())
#define E_TABLE_CONFIG(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TABLE_CONFIG_TYPE, ETableConfig))
#define E_TABLE_CONFIG_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_TABLE_CONFIG_TYPE, ETableConfigClass))
#define E_IS_TABLE_CONFIG(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TABLE_CONFIG_TYPE))
#define E_IS_TABLE_CONFIG_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TABLE_CONFIG_TYPE))
#define E_TABLE_CONFIG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_TABLE_CONFIG_TYPE, ETableConfigClass))

typedef struct {
	GalComboText *combo;
	GtkWidget    *frames;
	GtkWidget    *radio_ascending;
	GtkWidget    *radio_descending;
	GtkWidget    *view_check; /* Only for group dialog */
	guint         changed_id, toggled_id;
	void *e_table_config;
} ETableConfigSortWidgets; 


typedef struct {
	GObject parent;

	char *header;

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
	ETableConfigSortWidgets group [4];

	ETable               *available;
	ETableWithout        *available_model;
	ETable               *shown;
	ETableSubsetVariable *shown_model;
	char *domain;

	/*
	 * List of valid column names
	 */
	GSList *column_names;
} ETableConfig;

typedef struct {
	GObjectClass parent_class;

	/* Signals */
	void        (*changed)        (ETableConfig *config);
} ETableConfigClass;

GType         e_table_config_get_type  (void);
ETableConfig *e_table_config_new       (const char          *header,
					ETableSpecification *spec,
					ETableState         *state,
					GtkWindow           *parent_window);
ETableConfig *e_table_config_construct (ETableConfig        *etco,
					const char          *header,
					ETableSpecification *spec,
					ETableState         *state,
					GtkWindow           *parent_window);
void          e_table_config_raise     (ETableConfig        *config);

G_END_DECLS

#endif /* _E_TABLE_CONFIG_H */
