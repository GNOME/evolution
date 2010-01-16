/*
 * e-selectable.h
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

#ifndef E_SELECTABLE_H
#define E_SELECTABLE_H

#include <gtk/gtk.h>
#include <misc/e-focus-tracker.h>

/* Standard GObject macros */
#define E_TYPE_SELECTABLE \
	(e_selectable_get_type ())
#define E_SELECTABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SELECTABLE, ESelectable))
#define E_IS_SELECTABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SELECTABLE))
#define E_SELECTABLE_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_SELECTABLE, ESelectableInterface))

G_BEGIN_DECLS

typedef struct _ESelectable ESelectable;
typedef struct _ESelectableInterface ESelectableInterface;

struct _ESelectableInterface {
	GTypeInterface parent_iface;

	/* Required Methods */
	void		(*update_actions)	(ESelectable *selectable,
						 EFocusTracker *focus_tracker,
						 GdkAtom *clipboard_targets,
						 gint n_clipboard_targets);

	/* Optional Methods */
	void		(*cut_clipboard)	(ESelectable *selectable);
	void		(*copy_clipboard)	(ESelectable *selectable);
	void		(*paste_clipboard)	(ESelectable *selectable);
	void		(*delete_selection)	(ESelectable *selectable);
	void		(*select_all)		(ESelectable *selectable);
};

GType		e_selectable_get_type		(void);
void		e_selectable_update_actions	(ESelectable *selectable,
						 EFocusTracker *focus_tracker,
						 GdkAtom *clipboard_targets,
						 gint n_clipboard_targets);
void		e_selectable_cut_clipboard	(ESelectable *selectable);
void		e_selectable_copy_clipboard	(ESelectable *selectable);
void		e_selectable_paste_clipboard	(ESelectable *selectable);
void		e_selectable_delete_selection	(ESelectable *selectable);
void		e_selectable_select_all		(ESelectable *selectable);
GtkTargetList *	e_selectable_get_copy_target_list
						(ESelectable *selectable);
GtkTargetList *	e_selectable_get_paste_target_list
						(ESelectable *selectable);

G_END_DECLS

#endif /* E_SELECTABLE_H */
