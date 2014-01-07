/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *		Miguel de Icaza <miguel@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef _E_TABLE_CONFIG_H_
#define _E_TABLE_CONFIG_H_

#include <e-util/e-table-specification.h>

/* Standard GObject macros */
#define E_TYPE_TABLE_CONFIG \
	(e_table_config_get_type ())
#define E_TABLE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TABLE_CONFIG, ETableConfig))
#define E_TABLE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TABLE_CONFIG, ETableConfigClass))
#define E_IS_TABLE_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TABLE_CONFIG))
#define E_IS_TABLE_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TABLE_CONFIG))
#define E_TABLE_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TABLE_CONFIG, ETableConfigClass))

G_BEGIN_DECLS

typedef struct _ETableConfigSortWidgets ETableConfigSortWidgets;

typedef struct _ETableConfig ETableConfig;
typedef struct _ETableConfigClass ETableConfigClass;

struct _ETableConfigSortWidgets {
	GtkWidget    *combo;
	GtkWidget    *frames;
	GtkWidget    *radio_ascending;
	GtkWidget    *radio_descending;
	GtkWidget    *view_check; /* Only for group dialog */
	guint         changed_id, toggled_id;
	gpointer e_table_config;
};

struct _ETableConfig {
	GObject parent;

	gchar *header;

	/*
	 * Our various dialog boxes
	 */
	GtkWidget *dialog_toplevel;
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

	ETableConfigSortWidgets sort[4];
	ETableConfigSortWidgets group[4];

	gchar *domain;

	/*
	 * List of valid column names
	 */
	GSList *column_names;
};

struct _ETableConfigClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*changed)		(ETableConfig *config);
};

GType		e_table_config_get_type		(void) G_GNUC_CONST;
ETableConfig *	e_table_config_new		(const gchar *header,
						 ETableSpecification *spec,
						 ETableState *state,
						 GtkWindow *parent_window);
ETableConfig *	e_table_config_construct	(ETableConfig *etco,
						 const gchar *header,
						 ETableSpecification *spec,
						 ETableState *state,
						 GtkWindow *parent_window);
void		e_table_config_raise		(ETableConfig *config);

G_END_DECLS

#endif /* _E_TABLE_CONFIG_H */
