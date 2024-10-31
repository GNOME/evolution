/* e-source-selector.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SOURCE_SELECTOR_H
#define E_SOURCE_SELECTOR_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_SELECTOR \
	(e_source_selector_get_type ())
#define E_SOURCE_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_SELECTOR, ESourceSelector))
#define E_SOURCE_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_SELECTOR, ESourceSelectorClass))
#define E_IS_SOURCE_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_SELECTOR))
#define E_IS_SOURCE_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_SELECTOR))
#define E_SOURCE_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_SELECTOR, ESourceSelectorClass))

#define E_SOURCE_SELECTOR_GROUPS_SETUP_NAME "SourceSelector"

G_BEGIN_DECLS

typedef struct _ESourceSelector ESourceSelector;
typedef struct _ESourceSelectorClass ESourceSelectorClass;
typedef struct _ESourceSelectorPrivate ESourceSelectorPrivate;

/**
 * ESourceSelectorForeachSourceChildFunc:
 * @selector: an #ESourceSelector
 * @display_name: child's display name
 * @child_data: child's data
 * @user_data: callback user data
 *
 * Callback called for each source's child added by e_source_selector_add_source_child().
 * The callback is used by e_source_selector_foreach_source_child_remove().
 *
 * Returns: %TRUE to remove the child, or %FALSE to keep it
 *
 * Since: 3.48
 **/
typedef gboolean (* ESourceSelectorForeachSourceChildFunc)	(ESourceSelector *selector,
								 const gchar *display_name,
								 const gchar *child_data,
								 gpointer user_data);

struct _ESourceSelector {
	GtkTreeView parent;
	ESourceSelectorPrivate *priv;
};

struct _ESourceSelectorClass {
	GtkTreeViewClass parent_class;

	/* Methods */
	gboolean	(*get_source_selected)	(ESourceSelector *selector,
						 ESource *source);
	gboolean	(*set_source_selected)	(ESourceSelector *selector,
						 ESource *source,
						 gboolean selected);

	/* Signals */
	void		(*selection_changed)	(ESourceSelector *selector);
	void		(*primary_selection_changed)
						(ESourceSelector *selector);
	gboolean	(*popup_event)		(ESourceSelector *selector,
						 ESource *primary,
						 GdkEventButton *event);
	gboolean	(*data_dropped)		(ESourceSelector *selector,
						 GtkSelectionData *data,
						 ESource *destination,
						 GdkDragAction action,
						 guint target_info);
	void		(*source_selected)	(ESourceSelector *selector,
						 ESource *source);
	void		(*source_unselected)	(ESourceSelector *selector,
						 ESource *source);
	gboolean	(*filter_source)	(ESourceSelector *selector,
						 ESource *source);
	void		(*source_child_selected)(ESourceSelector *selector,
						 ESource *source,
						 const gchar *child_data);

	gpointer padding[1];
};

GType		e_source_selector_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_source_selector_new		(ESourceRegistry *registry,
						 const gchar *extension_name);
ESourceRegistry *
		e_source_selector_get_registry	(ESourceSelector *selector);
const gchar *	e_source_selector_get_extension_name
						(ESourceSelector *selector);
gboolean	e_source_selector_get_show_colors
						(ESourceSelector *selector);
void		e_source_selector_set_show_colors
						(ESourceSelector *selector,
						 gboolean show_colors);
gboolean	e_source_selector_get_show_icons
						(ESourceSelector *selector);
void		e_source_selector_set_show_icons
						(ESourceSelector *selector,
						 gboolean show_icons);
gboolean	e_source_selector_get_show_toggles
						(ESourceSelector *selector);
void		e_source_selector_set_show_toggles
						(ESourceSelector *selector,
						 gboolean show_toggles);
void		e_source_selector_select_source	(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_unselect_source
						(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_select_exclusive
						(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_select_all	(ESourceSelector *selector);
gboolean	e_source_selector_source_is_selected
						(ESourceSelector *selector,
						 ESource *source);
GList *		e_source_selector_get_selection	(ESourceSelector *selector);
guint		e_source_selector_count_total	(ESourceSelector *selector);
guint		e_source_selector_count_selected(ESourceSelector *selector);
void		e_source_selector_edit_primary_selection
						(ESourceSelector *selector);
ESource *	e_source_selector_ref_primary_selection
						(ESourceSelector *selector);
void		e_source_selector_set_primary_selection
						(ESourceSelector *selector,
						 ESource *source);
gboolean	e_source_selector_get_source_iter
						(ESourceSelector *selector,
						 ESource *source,
						 GtkTreeIter *iter,
						 GtkTreeModel **out_model);
ESource *	e_source_selector_ref_source_by_iter
						(ESourceSelector *selector,
						 GtkTreeIter *iter);
ESource *	e_source_selector_ref_source_by_path
						(ESourceSelector *selector,
						 GtkTreePath *path);
void		e_source_selector_queue_write	(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_update_row	(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_update_all_rows
						(ESourceSelector *selector);
void		e_source_selector_set_source_tooltip
						(ESourceSelector *selector,
						 ESource *source,
						 const gchar *tooltip);
gchar *		e_source_selector_dup_source_tooltip
						(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_set_source_is_busy
						(ESourceSelector *selector,
						 ESource *source,
						 gboolean is_busy);
gboolean	e_source_selector_get_source_is_busy
						(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_set_source_connection_status
						(ESourceSelector *selector,
						 ESource *source,
						 guint value);
guint		e_source_selector_get_source_connection_status
						(ESourceSelector *selector,
						 ESource *source);
gboolean	e_source_selector_manage_groups	(ESourceSelector *selector);
gboolean	e_source_selector_save_groups_setup
						(ESourceSelector *selector,
						 GKeyFile *key_file);
void		e_source_selector_load_groups_setup
						(ESourceSelector *selector,
						 GKeyFile *key_file);
void		e_source_selector_add_source_child
						(ESourceSelector *selector,
						 ESource *source,
						 const gchar *display_name,
						 const gchar *child_data);
void		e_source_selector_remove_source_children
						(ESourceSelector *selector,
						 ESource *source);
void		e_source_selector_foreach_source_child_remove
						(ESourceSelector *selector,
						 ESource *source,
						 ESourceSelectorForeachSourceChildFunc func,
						 gpointer user_data);
gchar *		e_source_selector_dup_selected_child_data
						(ESourceSelector *selector);

G_END_DECLS

#endif /* E_SOURCE_SELECTOR_H */
