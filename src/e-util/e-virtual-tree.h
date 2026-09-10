/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_VIRTUAL_TREE_H
#define E_VIRTUAL_TREE_H

#include <gtk/gtk.h>
#include <e-util/e-util-enums.h>
#include <e-util/e-printable.h>
#include <e-util/e-virtual-tree-model.h>

G_BEGIN_DECLS

#define E_TYPE_VIRTUAL_TREE (e_virtual_tree_get_type ())
G_DECLARE_FINAL_TYPE (EVirtualTree, e_virtual_tree, E, VIRTUAL_TREE, GtkBox)

#define E_TYPE_CELL_RENDERER_BUTTON (e_cell_renderer_button_get_type ())
G_DECLARE_FINAL_TYPE (ECellRendererButton, e_cell_renderer_button, E, CELL_RENDERER_BUTTON, GtkCellRendererText)

GtkCellRenderer *	e_cell_renderer_button_new	(void);

#define E_TYPE_CELL_RENDERER_TOGGLE (e_cell_renderer_toggle_get_type ())
G_DECLARE_FINAL_TYPE (ECellRendererToggle, e_cell_renderer_toggle, E, CELL_RENDERER_TOGGLE, GtkCellRendererToggle)

GtkCellRenderer *	e_cell_renderer_toggle_new	(void);

typedef void (*EVirtualTreeCellDataFunc)	(EVirtualTree *self,
						 GtkCellRenderer *renderer,
						 GObject *row_object,
						 guint visible_row,
						 gpointer user_data);

typedef gchar * (*EVirtualTreeSearchFunc)	(EVirtualTree *self,
						 GObject *row_object,
						 gpointer user_data);

/* Returns TRUE and fills out_color with the row's own color, or FALSE
 * for no override. Used by e_virtual_tree_set_selected_row_color_func()
 * and e_virtual_tree_set_unselected_row_color_func(). */
typedef gboolean (*EVirtualTreeRowColorFunc)	(EVirtualTree *self,
						 GObject *row_object,
						 guint visible_row,
						 GdkRGBA *out_color,
						 gpointer user_data);

/* Returns newly-allocated markup for a whole-row tooltip, or NULL for
 * no override. Used by e_virtual_tree_set_row_tooltip_markup_func(). */
typedef gchar * (*EVirtualTreeRowTooltipMarkupFunc)	(EVirtualTree *self,
							 GObject *row_object,
							 gpointer user_data);

GtkWidget *		e_virtual_tree_new		(EVirtualTreeModel *model);
void			e_virtual_tree_set_model	(EVirtualTree *self,
							 EVirtualTreeModel *model);
EVirtualTreeModel *	e_virtual_tree_get_model	(EVirtualTree *self);
guint			e_virtual_tree_add_column	(EVirtualTree *self,
							 const gchar *column_id,
							 const gchar *title);
GtkTreeViewColumn *	e_virtual_tree_get_column	(EVirtualTree *self,
							 guint column_index);
void			e_virtual_tree_column_pack_start(EVirtualTree *self,
							 guint column_index,
							 guint line,
							 GtkCellRenderer *renderer,
							 gboolean expand,
							 EVirtualTreeCellDataFunc func,
							 gpointer user_data,
							 GDestroyNotify destroy);
void			e_virtual_tree_column_pack_end	(EVirtualTree *self,
							 guint column_index,
							 guint line,
							 GtkCellRenderer *renderer,
							 gboolean expand,
							 EVirtualTreeCellDataFunc func,
							 gpointer user_data,
							 GDestroyNotify destroy);
void			e_virtual_tree_column_pack_span	(EVirtualTree *self,
							 guint column_index,
							 GtkCellRenderer *renderer,
							 EVirtualTreeCellDataFunc func,
							 gpointer user_data,
							 GDestroyNotify destroy);
void			e_virtual_tree_column_set_accessible_name
							(EVirtualTree *self,
							 guint column_index,
							 const gchar *name);
void			e_virtual_tree_column_clear_renderers
							(EVirtualTree *self,
							 guint column_index);
void			e_virtual_tree_set_column_expand(EVirtualTree *self,
							 guint column_index,
							 gboolean expand);
void			e_virtual_tree_set_column_min_width
							(EVirtualTree *self,
							 guint column_index,
							 gint min_width);
void			e_virtual_tree_set_expander_column
							(EVirtualTree *self,
							 guint column_index,
							 guint line);
void			e_virtual_tree_set_expander_visible
							(EVirtualTree *self,
							 gboolean visible);
void			e_virtual_tree_set_group_depth	(EVirtualTree *self,
							 guint depth);
guint			e_virtual_tree_get_group_depth	(EVirtualTree *self);
void			e_virtual_tree_set_editable	(EVirtualTree *self,
							 gboolean editable);
gboolean		e_virtual_tree_get_editable	(EVirtualTree *self);
void			e_virtual_tree_set_selection_mode
							(EVirtualTree *self,
							 GtkSelectionMode mode);
GtkSelectionMode	e_virtual_tree_get_selection_mode
							(EVirtualTree *self);
GPtrArray *		e_virtual_tree_get_selected_rows(EVirtualTree *self);
void			e_virtual_tree_select_row	(EVirtualTree *self,
							 guint row);
void			e_virtual_tree_unselect_row	(EVirtualTree *self,
							 guint row);
void			e_virtual_tree_select_all	(EVirtualTree *self);
void			e_virtual_tree_unselect_all	(EVirtualTree *self);
gboolean		e_virtual_tree_row_is_selected	(EVirtualTree *self,
							 guint row);
void			e_virtual_tree_set_cursor	(EVirtualTree *self,
							 gint row);
void			e_virtual_tree_set_cursor_centered
							(EVirtualTree *self,
							 gint row);
gint			e_virtual_tree_get_cursor	(EVirtualTree *self);
gboolean		e_virtual_tree_row_is_visible	(EVirtualTree *self,
							 guint row);
void			e_virtual_tree_scroll_cursor_into_view
							(EVirtualTree *self);
void			e_virtual_tree_scroll_to_row	(EVirtualTree *self,
							 guint row);
GtkTreeView *		e_virtual_tree_get_tree_view	(EVirtualTree *self);
gboolean		e_virtual_tree_get_cell_rect	(EVirtualTree *self,
							 guint column_index,
							 guint visible_row,
							 GdkRectangle *out_rect);
void			e_virtual_tree_set_search_func	(EVirtualTree *self,
							 EVirtualTreeSearchFunc func,
							 gpointer user_data,
							 GDestroyNotify destroy);
void			e_virtual_tree_set_selected_row_color_func
							(EVirtualTree *self,
							 EVirtualTreeRowColorFunc func,
							 gpointer user_data,
							 GDestroyNotify destroy);
void			e_virtual_tree_set_unselected_row_color_func
							(EVirtualTree *self,
							 EVirtualTreeRowColorFunc func,
							 gpointer user_data,
							 GDestroyNotify destroy);
void			e_virtual_tree_set_row_tooltip_markup_func
							(EVirtualTree *self,
							 EVirtualTreeRowTooltipMarkupFunc func,
							 gpointer user_data,
							 GDestroyNotify destroy);
void			e_virtual_tree_enable_drag_source
							(EVirtualTree *self,
							 GdkModifierType start_button_mask,
							 const GtkTargetEntry *targets,
							 gint n_targets,
							 GdkDragAction actions);
void			e_virtual_tree_enable_drag_dest	(EVirtualTree *self,
							 const GtkTargetEntry *targets,
							 gint n_targets,
							 GdkDragAction actions);
void			e_virtual_tree_set_column_state_filename
							(EVirtualTree *self,
							 const gchar *filename);
const gchar *		e_virtual_tree_get_column_state_filename
							(EVirtualTree *self);
void			e_virtual_tree_save_column_state_to_key_file
							(EVirtualTree *self,
							 GKeyFile *key_file);
void			e_virtual_tree_load_column_state_from_key_file
							(EVirtualTree *self,
							 GKeyFile *key_file);
GKeyFile *		e_virtual_tree_convert_legacy_etable_state
							(const gchar *xml_data,
							 gssize length,
							 const gchar * const *legacy_column_ids,
							 guint n_legacy_column_ids);
const gchar * const *	e_virtual_tree_get_legacy_etable_column_map
							(EVirtualTree *self,
							 guint *out_n_column_ids);
gboolean		e_virtual_tree_get_applying_proportional_widths
							(EVirtualTree *self);
void			e_virtual_tree_set_header_click_sort_policy
							(EVirtualTree *self,
							 EAutomaticActionPolicy policy);
EAutomaticActionPolicy	e_virtual_tree_get_header_click_sort_policy
							(EVirtualTree *self);
void			e_virtual_tree_set_column_sort	(EVirtualTree *self,
							 guint column_index,
							 GtkSortType sort_order,
							 gint sort_priority);
void			e_virtual_tree_clear_sort	(EVirtualTree *self);
gboolean		e_virtual_tree_get_column_sort	(EVirtualTree *self,
							 guint column_index,
							 GtkSortType *out_sort_order,
							 gint *out_priority);
void			e_virtual_tree_set_column_group	(EVirtualTree *self,
							 guint column_index,
							 GtkSortType sort_order,
							 gint group_priority);
void			e_virtual_tree_clear_group	(EVirtualTree *self);
gboolean		e_virtual_tree_get_column_group	(EVirtualTree *self,
							 guint column_index,
							 GtkSortType *out_sort_order,
							 gint *out_priority);
void			e_virtual_tree_set_column_groupable
							(EVirtualTree *self,
							 guint column_index,
							 gboolean groupable);
gboolean		e_virtual_tree_get_column_groupable
							(EVirtualTree *self,
							 guint column_index);
gboolean		e_virtual_tree_get_allow_grouping
							(EVirtualTree *self);
guint			e_virtual_tree_get_n_columns	(EVirtualTree *self);
const gchar *		e_virtual_tree_get_column_id	(EVirtualTree *self,
							 guint column_index);
const gchar *		e_virtual_tree_get_column_title	(EVirtualTree *self,
							 guint column_index);
void			e_virtual_tree_freeze_state_changed
							(EVirtualTree *self);
void			e_virtual_tree_thaw_state_changed
							(EVirtualTree *self);
void			e_virtual_tree_show_customize_popover
							(EVirtualTree *self);
void			e_virtual_tree_set_empty_message(EVirtualTree *self,
							 const gchar *message);
const gchar *		e_virtual_tree_get_empty_message(EVirtualTree *self);
gconstpointer		e_virtual_tree_get_cursor_key	(EVirtualTree *self);
GObject *		e_virtual_tree_get_cursor_object(EVirtualTree *self);
guint			e_virtual_tree_selected_count	(EVirtualTree *self);
gboolean		e_virtual_tree_is_dragging	(EVirtualTree *self);
EPrintable *		e_virtual_tree_get_printable	(EVirtualTree *self);

G_END_DECLS

#endif /* E_VIRTUAL_TREE_H */
