/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_TABLE_COMMON_H
#define E_CAL_TABLE_COMMON_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>
#include <calendar/gui/e-cal-model.h>

G_BEGIN_DECLS

typedef struct _ECalTableColumnDef {
	const gchar *id;
	const gchar *title;
	gint field; /* an ECalModelField value */
	gboolean default_visible;
} ECalTableColumnDef;

/* Stored as the EVirtualTreeCellDataFunc user_data for every column. */
typedef struct _ECalTableColumnFuncData {
	gpointer self; /* borrowed: the ECalTable{Tasks,Memos,Events}*, outlives the column */
	const ECalTableColumnDef *def; /* borrowed: points into the table's static column array */
} ECalTableColumnFuncData;

/* Returns a newly-created renderer for a field that needs special handling
 * (a toggle or combo renderer), or NULL to fall back to the generic
 * COLOR/ICON/text renderer selection. Set *out_center_align to TRUE to
 * have the column centered and non-resizable, like ICON/COLOR columns. */
typedef GtkCellRenderer *
		(* ECalTableCreateSpecialRendererFunc)
						(gint field,
						 gpointer user_data,
						 gboolean *out_center_align);

/* Called right after a column is created, so the owner can remember a
 * column index it needs later (e.g. Tasks' complete_column_index). */
typedef void	(* ECalTableColumnCreatedFunc)	(gint field,
						 guint column_index,
						 gpointer user_data);

typedef const gchar * const *
		(* ECalTableGetLegacyColumnIdsFunc)
						(guint *out_n_column_ids);

void		e_cal_table_common_setup_columns
						(EVirtualTree *vtree,
						 gpointer owner,
						 const ECalTableColumnDef *defs,
						 guint n_defs,
						 EVirtualTreeCellDataFunc data_func,
						 ECalTableCreateSpecialRendererFunc create_special_renderer,
						 gpointer create_special_renderer_user_data,
						 ECalTableColumnCreatedFunc on_column_created,
						 gpointer on_column_created_user_data,
						 gint expander_field);

void		e_cal_table_common_set_color_cell
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 GtkCellRenderer *renderer);
void		e_cal_table_common_set_icon_cell
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 GtkCellRenderer *renderer,
						 const gchar * const *icon_names,
						 guint n_icon_names);
gchar *		e_cal_table_common_date_value_to_text
						(ECalModel *model,
						 ECalModelComponent *comp_data,
						 gint field);

gchar *		e_cal_table_common_row_tooltip_markup_cb
						(EVirtualTree *vtree,
						 GObject *row_object,
						 gpointer user_data /* ECalModel * */);

GtkListStore *	e_cal_table_common_build_status_store
						(ECalModel *model);

void		e_cal_table_common_column_state_changed
						(EVirtualTree *vtree,
						 const ECalTableColumnDef *defs,
						 guint n_defs,
						 ECalModel *model);

const gchar * const *
		e_cal_table_common_get_legacy_etable_column_ids
						(const ECalTableColumnDef *defs,
						 guint n_defs,
						 const gchar **out_array,
						 guint *out_n_column_ids);
gpointer	e_cal_table_common_get_legacy_etable_column_map_cb
						(EVirtualTree *vtree,
						 guint *out_n_column_ids,
						 gpointer user_data /* an ECalTableGetLegacyColumnIdsFunc, cast to gpointer */);

G_END_DECLS

#endif /* E_CAL_TABLE_COMMON_H */
