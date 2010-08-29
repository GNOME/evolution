/*
 * e-focus-tracker.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_FOCUS_TRACKER_H
#define E_FOCUS_TRACKER_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_FOCUS_TRACKER \
	(e_focus_tracker_get_type ())
#define E_FOCUS_TRACKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FOCUS_TRACKER, EFocusTracker))
#define E_FOCUS_TRACKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FOCUS_TRACKER, EFocusTrackerClass))
#define E_IS_FOCUS_TRACKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FOCUS_TRACKER))
#define E_IS_FOCUS_TRACKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FOCUS_TRACKER))
#define E_FOCUS_TRACKER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FOCUS_TRACKER, EFocusTrackerClass))

G_BEGIN_DECLS

typedef struct _EFocusTracker EFocusTracker;
typedef struct _EFocusTrackerClass EFocusTrackerClass;
typedef struct _EFocusTrackerPrivate EFocusTrackerPrivate;

struct _EFocusTracker {
	GObject parent;
	EFocusTrackerPrivate *priv;
};

struct _EFocusTrackerClass {
	GObjectClass parent_class;
};

GType		e_focus_tracker_get_type	(void);
EFocusTracker *	e_focus_tracker_new		(GtkWindow *window);
GtkWidget *	e_focus_tracker_get_focus	(EFocusTracker *focus_tracker);
GtkWindow *	e_focus_tracker_get_window	(EFocusTracker *focus_tracker);
GtkAction *	e_focus_tracker_get_cut_clipboard_action
						(EFocusTracker *focus_tracker);
void		e_focus_tracker_set_cut_clipboard_action
						(EFocusTracker *focus_tracker,
						 GtkAction *cut_clipboard);
GtkAction *	e_focus_tracker_get_copy_clipboard_action
						(EFocusTracker *focus_tracker);
void		e_focus_tracker_set_copy_clipboard_action
						(EFocusTracker *focus_tracker,
						 GtkAction *copy_clipboard);
GtkAction *	e_focus_tracker_get_paste_clipboard_action
						(EFocusTracker *focus_tracker);
void		e_focus_tracker_set_paste_clipboard_action
						(EFocusTracker *focus_tracker,
						 GtkAction *paste_clipboard);
GtkAction *	e_focus_tracker_get_delete_selection_action
						(EFocusTracker *focus_tracker);
void		e_focus_tracker_set_delete_selection_action
						(EFocusTracker *focus_tracker,
						 GtkAction *delete_selection);
GtkAction *	e_focus_tracker_get_select_all_action
						(EFocusTracker *focus_tracker);
void		e_focus_tracker_set_select_all_action
						(EFocusTracker *focus_tracker,
						 GtkAction *select_all);
void		e_focus_tracker_update_actions	(EFocusTracker *focus_tracker);
void		e_focus_tracker_cut_clipboard	(EFocusTracker *focus_tracker);
void		e_focus_tracker_copy_clipboard	(EFocusTracker *focus_tracker);
void		e_focus_tracker_paste_clipboard	(EFocusTracker *focus_tracker);
void		e_focus_tracker_delete_selection
						(EFocusTracker *focus_tracker);
void		e_focus_tracker_select_all	(EFocusTracker *focus_tracker);

G_END_DECLS

#endif /* E_FOCUS_TRACKER_H */
