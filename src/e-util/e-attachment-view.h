/*
 * e-attachment-view.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ATTACHMENT_VIEW_H
#define E_ATTACHMENT_VIEW_H

#include <gtk/gtk.h>
#include <e-util/e-attachment-store.h>
#include <e-util/e-ui-action.h>
#include <e-util/e-ui-action-group.h>
#include <e-util/e-ui-manager.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_VIEW \
	(e_attachment_view_get_type ())
#define E_ATTACHMENT_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_VIEW, EAttachmentView))
#define E_ATTACHMENT_VIEW_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_VIEW, EAttachmentViewInterface))
#define E_IS_ATTACHMENT_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_VIEW))
#define E_IS_ATTACHMENT_VIEW_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_VIEW))
#define E_ATTACHMENT_VIEW_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_ATTACHMENT_VIEW, EAttachmentViewInterface))

G_BEGIN_DECLS

typedef struct _EAttachmentView EAttachmentView;
typedef struct _EAttachmentViewInterface EAttachmentViewInterface;
typedef struct _EAttachmentViewPrivate EAttachmentViewPrivate;

struct _EAttachmentViewInterface {
	GTypeInterface parent_interface;

	/* General Methods */
	EAttachmentViewPrivate *
			(*get_private)		(EAttachmentView *view);
	EAttachmentStore *
			(*get_store)		(EAttachmentView *view);

	/* Selection Methods */
	GtkTreePath *	(*get_path_at_pos)	(EAttachmentView *view,
						 gint x,
						 gint y);
	GList *		(*get_selected_paths)	(EAttachmentView *view);
	gboolean	(*path_is_selected)	(EAttachmentView *view,
						 GtkTreePath *path);
	void		(*select_path)		(EAttachmentView *view,
						 GtkTreePath *path);
	void		(*unselect_path)	(EAttachmentView *view,
						 GtkTreePath *path);
	void		(*select_all)		(EAttachmentView *view);
	void		(*unselect_all)		(EAttachmentView *view);

	/* Drag and Drop Methods */
	void		(*drag_source_set)	(EAttachmentView *view,
						 GdkModifierType start_button_mask,
						 const GtkTargetEntry *targets,
						 gint n_targets,
						 GdkDragAction actions);
	void		(*drag_dest_set)	(EAttachmentView *view,
						 const GtkTargetEntry *targets,
						 gint n_targets,
						 GdkDragAction actions);
	void		(*drag_source_unset)	(EAttachmentView *view);
	void		(*drag_dest_unset)	(EAttachmentView *view);

	/* Signals */
	void		(*update_actions)	(EAttachmentView *view);
	gboolean	(*before_properties_popup)
						(EAttachmentView *view,
						 GtkPopover *properties_popover, /* EAttachmentPopover */
						 gboolean is_new_attachment);
};

struct _EAttachmentViewPrivate {

	/* Drag Destination */
	GtkTargetList *target_list;
	GdkDragAction drag_actions;

	/* Popup Menu Management */
	EUIManager *ui_manager;
	GMenu *open_with_apps_menu;
	GHashTable *open_with_apps_hash; /* gint index ~> GAppInfo * */

	/* Multi-DnD State */
	GList *event_list;
	GList *selected;
	gint start_x;
	gint start_y;

	guint dragging : 1;
	guint editable : 1;
	guint allow_uri : 1;

	GtkPopover *attachment_popover;
};

GType		e_attachment_view_get_type	(void) G_GNUC_CONST;

void		e_attachment_view_init		(EAttachmentView *view);
void		e_attachment_view_dispose	(EAttachmentView *view);
void		e_attachment_view_finalize	(EAttachmentView *view);

EAttachmentViewPrivate *
		e_attachment_view_get_private	(EAttachmentView *view);
EAttachmentStore *
		e_attachment_view_get_store	(EAttachmentView *view);
gboolean	e_attachment_view_get_dragging	(EAttachmentView *view);
void		e_attachment_view_set_dragging	(EAttachmentView *view,
						 gboolean dragging);
gboolean	e_attachment_view_get_editable	(EAttachmentView *view);
void		e_attachment_view_set_editable	(EAttachmentView *view,
						 gboolean editable);
gboolean	e_attachment_view_get_allow_uri	(EAttachmentView *view);
void		e_attachment_view_set_allow_uri	(EAttachmentView *view,
						 gboolean allow_uri);
GtkTargetList *	e_attachment_view_get_target_list
						(EAttachmentView *view);
GdkDragAction	e_attachment_view_get_drag_actions
						(EAttachmentView *view);
void		e_attachment_view_add_drag_actions
						(EAttachmentView *view,
						 GdkDragAction drag_actions);
GList *		e_attachment_view_get_selected_attachments
						(EAttachmentView *view);
void		e_attachment_view_open_path	(EAttachmentView *view,
						 GtkTreePath *path,
						 GAppInfo *app_info);
void		e_attachment_view_remove_selected
						(EAttachmentView *view,
						 gboolean select_next);

/* Event Support */
gboolean	e_attachment_view_button_press_event
						(EAttachmentView *view,
						 GdkEventButton *event);
gboolean	e_attachment_view_button_release_event
						(EAttachmentView *view,
						 GdkEventButton *event);
gboolean	e_attachment_view_motion_notify_event
						(EAttachmentView *view,
						 GdkEventMotion *event);
gboolean	e_attachment_view_key_press_event
						(EAttachmentView *view,
						 GdkEventKey *event);

/* Selection Management */
GtkTreePath *	e_attachment_view_get_path_at_pos
						(EAttachmentView *view,
						 gint x,
						 gint y);
GList *		e_attachment_view_get_selected_paths
						(EAttachmentView *view);
gboolean	e_attachment_view_path_is_selected
						(EAttachmentView *view,
						 GtkTreePath *path);
void		e_attachment_view_select_path	(EAttachmentView *view,
						 GtkTreePath *path);
void		e_attachment_view_unselect_path	(EAttachmentView *view,
						 GtkTreePath *path);
void		e_attachment_view_select_all	(EAttachmentView *view);
void		e_attachment_view_unselect_all	(EAttachmentView *view);
void		e_attachment_view_sync_selection
						(EAttachmentView *view,
						 EAttachmentView *target);

/* Drag Source Support */
void		e_attachment_view_drag_source_set
						(EAttachmentView *view);
void		e_attachment_view_drag_source_unset
						(EAttachmentView *view);
void		e_attachment_view_drag_begin	(EAttachmentView *view,
						 GdkDragContext *context);
void		e_attachment_view_drag_end	(EAttachmentView *view,
						 GdkDragContext *context);
void		e_attachment_view_drag_data_get	(EAttachmentView *view,
						 GdkDragContext *context,
						 GtkSelectionData *selection,
						 guint info,
						 guint time);

/* Drag Destination Support */
void		e_attachment_view_drag_dest_set	(EAttachmentView *view);
void		e_attachment_view_drag_dest_unset
						(EAttachmentView *view);
gboolean	e_attachment_view_drag_motion	(EAttachmentView *view,
						 GdkDragContext *context,
						 gint x,
						 gint y,
						 guint time);
gboolean	e_attachment_view_drag_drop	(EAttachmentView *view,
						 GdkDragContext *context,
						 gint x,
						 gint y,
						 guint time);
void		e_attachment_view_drag_data_received
						(EAttachmentView *view,
						 GdkDragContext *context,
						 gint x,
						 gint y,
						 GtkSelectionData *selection,
						 guint info,
						 guint time);

/* Popup Menu Management */
EUIAction *	e_attachment_view_get_action	(EAttachmentView *view,
						 const gchar *action_name);
EUIActionGroup *e_attachment_view_get_action_group
						(EAttachmentView *view,
						 const gchar *group_name);
GtkWidget *	e_attachment_view_get_popup_menu
						(EAttachmentView *view);
EUIManager *	e_attachment_view_get_ui_manager
						(EAttachmentView *view);
void		e_attachment_view_update_actions
						(EAttachmentView *view);
void		e_attachment_view_position_popover
						(EAttachmentView *view,
						 GtkPopover *popover,
						 EAttachment *attachment);
void		e_attachment_view_add_possible_attachment
						(EAttachmentView *view,
						 EAttachment *attachment);
void		e_attachment_view_clear_possible_attachments
						(EAttachmentView *view);

G_END_DECLS

#endif /* E_ATTACHMENT_VIEW_H */
