/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_TABLE_LIST_BASE_H
#define E_CAL_TABLE_LIST_BASE_H

#include <gtk/gtk.h>

#include <e-util/e-util.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-cal-model.h>
#include <calendar/gui/e-cal-table-common.h>

G_BEGIN_DECLS

#define E_TYPE_CAL_TABLE_LIST_BASE \
	(e_cal_table_list_base_get_type ())
#define E_CAL_TABLE_LIST_BASE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_TABLE_LIST_BASE, ECalTableListBase))
#define E_CAL_TABLE_LIST_BASE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_TABLE_LIST_BASE, ECalTableListBaseClass))
#define E_IS_CAL_TABLE_LIST_BASE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_TABLE_LIST_BASE))
#define E_IS_CAL_TABLE_LIST_BASE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_TABLE_LIST_BASE))
#define E_CAL_TABLE_LIST_BASE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_TABLE_LIST_BASE, ECalTableListBaseClass))

typedef struct _ECalTableListBase ECalTableListBase;
typedef struct _ECalTableListBaseClass ECalTableListBaseClass;
typedef struct _ECalTableListBasePrivate ECalTableListBasePrivate;

struct _ECalTableListBase {
	GtkBox parent;
	ECalTableListBasePrivate *priv;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ECalTableListBase, g_object_unref)

struct _ECalTableListBaseClass {
	GtkBoxClass parent_class;

	/* signals */
	void		(* open_component)	(ECalTableListBase *self,
						 ECalModelComponent *comp_data);
	void		(* popup_event)		(ECalTableListBase *self,
						 GdkEvent *event);

	/* required override */
	ECalComponentVType
			(* get_delete_confirm_vtype)
						(ECalTableListBase *self);

	/* optional override (NULL = not applicable). Returns TRUE if it fully
	 * decided *out_should_delete, in which case the base will not show
	 * the generic delete-confirmation dialog. */
	gboolean	(* maybe_retract_selection)
						(ECalTableListBase *self,
						 ECalComponent *comp,
						 ECalModelComponent *comp_data,
						 gboolean *out_should_delete);
};

typedef struct _ECalTableListBaseSetup {
	const gchar *accessible_name;
	const gchar *cut_tooltip;
	const gchar *copy_tooltip;
	const gchar *paste_tooltip;
	const gchar *delete_tooltip;
	const gchar *select_all_tooltip;
	const gchar *no_search_results_message;

	void		(* setup_columns_func)	(gpointer self);

	GCallback column_state_changed_cb;
	GCallback row_activated_cb;
	GCallback right_click_cb;
	GCallback cell_clicked_cb;
	GCallback cell_edited_cb;

	ECalTableGetLegacyColumnIdsFunc get_legacy_etable_column_ids_func;
} ECalTableListBaseSetup;

GType		e_cal_table_list_base_get_type
							(void);
GtkWidget *	e_cal_table_list_base_construct
						(ECalTableListBase *self,
						 EShellView *shell_view,
						 ECalModel *model,
						 const ECalTableListBaseSetup *setup);

EVirtualTree *	e_cal_table_list_base_get_virtual_tree
						(ECalTableListBase *self);
ECalModel *	e_cal_table_list_base_get_model
						(ECalTableListBase *self);
EShellView *	e_cal_table_list_base_get_shell_view
						(ECalTableListBase *self);
GSList *	e_cal_table_list_base_get_selected
						(ECalTableListBase *self);
ECalModelComponent *
		e_cal_table_list_base_get_selected_comp
						(ECalTableListBase *self);
GtkTargetList *	e_cal_table_list_base_get_copy_target_list
						(ECalTableListBase *self);
GtkTargetList *	e_cal_table_list_base_get_paste_target_list
						(ECalTableListBase *self);
EPrintable *	e_cal_table_list_base_get_printable
						(ECalTableListBase *self);
void		e_cal_table_list_base_set_search_active
						(ECalTableListBase *self,
						 gboolean search_active);
gboolean	e_cal_table_list_base_get_search_active
						(ECalTableListBase *self);
void		e_cal_table_list_base_emit_open_component
						(ECalTableListBase *self,
						 ECalModelComponent *comp_data);
void		e_cal_table_list_base_emit_popup_event
						(ECalTableListBase *self,
						 GdkEvent *event);

G_END_DECLS

#endif /* E_CAL_TABLE_LIST_BASE_H */
